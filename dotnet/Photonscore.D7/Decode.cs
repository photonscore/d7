// Copyright (c) 2026 Photonscore GmbH
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

using System.Diagnostics.CodeAnalysis;

namespace Photonscore.D7;

// One page's Data message, its packed fields left in place
internal unsafe struct DataView {
  public uint Id;
  public long Seed;
  public byte* Ints, IntsEnd, Longs, LongsEnd, Floats, FloatsEnd;
  public byte* Doubles, DoublesEnd, Raw, RawEnd;

  public static DataView Parse(byte* p, int len) {
    byte* end = p + len;
    if (len < 2 || *p != 0x12) {
      throw new InvalidDataException("a page entry holds no data");
    }
    p++;
    byte* mend = Seven.Len(ref p, end);
    return Message(p, mend);
  }

  public static DataView Message(byte* p, byte* mend) {
    DataView d = default;
    while (p < mend) {
      ulong tag = Seven.Varint(ref p, mend);
      switch (tag) {
        case 0x08:
          d.Id = (uint)Seven.Varint(ref p, mend);
          break;
        case 0x18:
          d.Seed = Seven.ZigZag(Seven.Varint(ref p, mend));
          break;
        case 0x22:
          Packed(ref p, mend, ref d.Ints, ref d.IntsEnd);
          break;
        case 0x2A:
          Packed(ref p, mend, ref d.Longs, ref d.LongsEnd);
          break;
        case 0x32:
          Packed(ref p, mend, ref d.Floats, ref d.FloatsEnd);
          break;
        case 0x3A:
          Packed(ref p, mend, ref d.Doubles, ref d.DoublesEnd);
          break;
        case 0x42:
          d.RawEnd = Seven.Len(ref p, mend);
          d.Raw = p;
          p = d.RawEnd;
          break;
        default:
          if (tag >> 3 is 1 or (>= 3 and <= 8)) {
            throw new InvalidDataException(
              $"data field {tag >> 3} in wire type {tag & 7}");
          }
          Seven.Skip(ref p, mend, tag);
          break;
      }
    }
    return d;
  }

  // Protobuf would concatenate a packed field given twice; no writer does
  static void Packed(
      ref byte* p, byte* end, ref byte* start, ref byte* stop) {
    if (start != null) {
      throw new InvalidDataException("a packed data field given twice");
    }
    stop = Seven.Len(ref p, end);
    start = p;
    p = stop;
  }
}

// Pages into spans: integers as a seed and zigzag deltas, bytes raw since
// version_patch 2, floats as stored
internal static unsafe class Decode {
  // One page of ds into dst, at most capacity elements; how many it held
  public static int Page(
      Dataset ds, byte* entry, int len, byte* dst, int capacity) {
    var d = DataView.Parse(entry, len);
    if (d.Id != ds.Id) {
      throw new InvalidDataException(
        $"a page of dataset {d.Id} listed for {ds.Name}");
    }
    return ds.Type switch {
      SevenType.Int16 or SevenType.UInt16 =>
        U16(d.Ints, d.IntsEnd, d.Seed, (ushort*)dst, capacity),
      SevenType.Int32 or SevenType.UInt32 =>
        U32(d.Ints, d.IntsEnd, d.Seed, (uint*)dst, capacity),
      SevenType.Int64 or SevenType.UInt64 =>
        U64(d.Longs, d.LongsEnd, d.Seed, (ulong*)dst, capacity),
      SevenType.Int8 or SevenType.UInt8 => d.RawEnd > d.Raw
        ? Copy(d.Raw, d.RawEnd, 1, dst, capacity)
        : U8(d.Ints, d.IntsEnd, d.Seed, dst, capacity),
      SevenType.Single => Copy(d.Floats, d.FloatsEnd, 4, dst, capacity),
      _ => Copy(d.Doubles, d.DoublesEnd, 8, dst, capacity),
    };
  }

  static int Copy(byte* p, byte* end, int size, byte* dst, int capacity) {
    long bytes = end - p;
    if (bytes % size != 0) {
      throw new InvalidDataException($"{bytes} bytes of {size}-byte values");
    }
    if (bytes / size > capacity)
      TooMany();
    new ReadOnlySpan<byte>(p, (int)bytes)
      .CopyTo(new Span<byte>(dst, (int)bytes));
    return (int)(bytes / size);
  }

  // The unchecked loop stops ten bytes short of the end, the longest a
  // varint gets; the tail is checked
  public static int U16(
      byte* p, byte* end, long seed, ushort* dst, int capacity) {
    ushort* d = dst, dend = dst + capacity;
    uint cur = (uint)seed;
    *d++ = (ushort)cur;
    if (p == null)
      return 1;
    byte* fast = end - 10;
    while (p < fast) {
      if (d == dend)
        TooMany();
      uint v = *p++;
      if (v >= 0x80) {
        uint b = *p++;
        v = (v & 0x7F) | (b << 7);
        if (b >= 0x80) {
          b = *p++;
          v = (v & 0x3FFF) | (b << 14);
          if (b >= 0x80) {
            b = *p++;
            v = (v & 0x1FFFFF) | (b << 21);
            if (b >= 0x80) {
              b = *p++;
              v = (v & 0xFFFFFFF) | (b << 28);
              if (b >= 0x80)
                p = High(p);
            }
          }
        }
      }
      cur += (v >> 1) ^ (0u - (v & 1));
      *d++ = (ushort)cur;
    }
    while (p < end) {
      if (d == dend)
        TooMany();
      uint v = (uint)Seven.Varint(ref p, end);
      cur += (v >> 1) ^ (0u - (v & 1));
      *d++ = (ushort)cur;
    }
    return (int)(d - dst);
  }

  public static int U32(
      byte* p, byte* end, long seed, uint* dst, int capacity) {
    uint* d = dst, dend = dst + capacity;
    uint cur = (uint)seed;
    *d++ = cur;
    if (p == null)
      return 1;
    while (p < end) {
      if (d == dend)
        TooMany();
      uint v = (uint)Seven.Varint(ref p, end);
      cur += (v >> 1) ^ (0u - (v & 1));
      *d++ = cur;
    }
    return (int)(d - dst);
  }

  public static int U8(
      byte* p, byte* end, long seed, byte* dst, int capacity) {
    byte* d = dst, dend = dst + capacity;
    uint cur = (uint)seed;
    *d++ = (byte)cur;
    if (p == null)
      return 1;
    while (p < end) {
      if (d == dend)
        TooMany();
      uint v = (uint)Seven.Varint(ref p, end);
      cur += (v >> 1) ^ (0u - (v & 1));
      *d++ = (byte)cur;
    }
    return (int)(d - dst);
  }

  public static int U64(
      byte* p, byte* end, long seed, ulong* dst, int capacity) {
    ulong* d = dst, dend = dst + capacity;
    ulong cur = (ulong)seed;
    *d++ = cur;
    if (p == null)
      return 1;
    byte* fast = end - 10;
    while (p < fast) {
      if (d == dend)
        TooMany();
      ulong v = *p++;
      if (v >= 0x80) {
        v &= 0x7F;
        int shift = 7;
        ulong b;
        do {
          if (shift > 63)
            TooLong();
          b = *p++;
          v |= (b & 0x7F) << shift;
          shift += 7;
        } while (b >= 0x80);
      }
      cur += (v >> 1) ^ (0ul - (v & 1));
      *d++ = cur;
    }
    while (p < end) {
      if (d == dend)
        TooMany();
      ulong v = Seven.Varint(ref p, end);
      cur += (v >> 1) ^ (0ul - (v & 1));
      *d++ = cur;
    }
    return (int)(d - dst);
  }

  // Bytes six to ten of a 32-bit varint carry nothing the value keeps, as
  // protobuf's ReadVarint32 has it
  static byte* High(byte* p) {
    for (int i = 0; i < 5; i++) {
      if (*p++ < 0x80)
        return p;
    }
    TooLong();
    return p;
  }

  [DoesNotReturn]
  static void TooMany() => throw new InvalidDataException(
    "a page holds more values than a page can");

  [DoesNotReturn]
  static void TooLong() =>
    throw new InvalidDataException("a varint longer than ten bytes");
}
