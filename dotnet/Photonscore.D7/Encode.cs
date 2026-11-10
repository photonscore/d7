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

using System.Runtime.CompilerServices;

namespace Photonscore.D7;

// Pages into entries, native's SerializeChunk byte for byte: integers as a
// seed and zigzag deltas, 16-bit ones unwrapped, bytes and floats raw
internal static unsafe class Encode {
  // Before the payload: the entry's and Data's tags and lengths, the
  // dataset id and the seed, written once the payload's length is known
  const int Room = 32;

  // An entry's buffer: three bytes a 16-bit delta at most, and slack for
  // Put32's four-byte stores
  public static int Capacity(int bytesPerPage) =>
    Room + bytesPerPage / 2 * 3 + 16;

  // One page of ds, `bytes` long at src, into buf; the entry is
  // buf[From, From + Len)
  public static (int From, int Len) Page(
      Dataset ds, byte* src, int bytes, byte* buf) {
    byte* q = buf + Room;
    long seed = 0;
    int field;
    switch (ds.Type) {
      case SevenType.UInt16: {
          var s = (ushort*)src;
          int n = bytes / 2;
          seed = s[0];
          for (int i = 1; i < n; i++)
            q = Put32(q, s[i] - s[i - 1]);
          field = 4;
          break;
        }
      case SevenType.Int16: {
          var s = (short*)src;
          int n = bytes / 2;
          seed = s[0];
          for (int i = 1; i < n; i++)
            q = Put32(q, s[i] - s[i - 1]);
          field = 4;
          break;
        }
      case SevenType.UInt32 or SevenType.Int32: {
          var s = (uint*)src;
          int n = bytes / 4;
          seed = ds.Type == SevenType.Int32 ? (int)s[0] : s[0];
          for (int i = 1; i < n; i++)
            q = Put32(q, (int)(s[i] - s[i - 1]));
          field = 4;
          break;
        }
      case SevenType.UInt64 or SevenType.Int64: {
          var s = (ulong*)src;
          int n = bytes / 8;
          seed = (long)s[0];
          for (int i = 1; i < n; i++) {
            long d = (long)(s[i] - s[i - 1]);
            q = Put(q, (ulong)((d << 1) ^ (d >> 63)));
          }
          field = 5;
          break;
        }
      default:
        new ReadOnlySpan<byte>(src, bytes).CopyTo(new Span<byte>(q, bytes));
        q += bytes;
        field = ds.Type switch {
          SevenType.Single => 6,
          SevenType.Double => 7,
          _ => 8,
        };
        break;
    }
    int payload = (int)(q - (buf + Room));
    ulong zseed = (ulong)((seed << 1) ^ (seed >> 63));
    // proto3 leaves a zero id, a zero seed and an empty field out
    int data = (ds.Id != 0 ? 1 + Length(ds.Id) : 0) +
               (zseed != 0 ? 1 + Length(zseed) : 0) +
               (payload > 0 ? 1 + Length((ulong)payload) + payload : 0);
    int prefix = 1 + Length((ulong)data) + data - payload;
    byte* w = buf + Room - prefix;
    *w++ = 0x12;
    w = Put(w, (ulong)data);
    if (ds.Id != 0) {
      *w++ = 0x08;
      w = Put(w, ds.Id);
    }
    if (zseed != 0) {
      *w++ = 0x18;
      w = Put(w, zseed);
    }
    if (payload > 0) {
      *w++ = (byte)(field << 3 | 2);
      Put(w, (ulong)payload);
    }
    return (Room - prefix, prefix + payload);
  }

  // A zigzag sint32 of at most three bytes as one four-byte store
  [MethodImpl(MethodImplOptions.AggressiveInlining)]
  static byte* Put32(byte* q, int d) {
    uint z = (uint)((d << 1) ^ (d >> 31));
    if (z >= 1u << 21)
      return Put(q, z);
    uint c1 = z >= 0x80 ? 1u : 0u, c2 = z >= 0x4000 ? 1u : 0u;
    *(uint*)q = (z & 0x7F) | (c1 << 7) | (((z >> 7) & 0x7F) << 8) |
                (c2 << 15) | ((z >> 14) << 16);
    return q + 1 + c1 + c2;
  }

  [MethodImpl(MethodImplOptions.AggressiveInlining)]
  public static byte* Put(byte* q, ulong v) {
    while (v >= 0x80) {
      *q++ = (byte)(v | 0x80);
      v >>= 7;
    }
    *q++ = (byte)v;
    return q;
  }

  public static int Length(ulong v) {
    int n = 1;
    while (v >= 0x80) {
      v >>= 7;
      n++;
    }
    return n;
  }
}
