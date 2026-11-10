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

using Microsoft.Win32.SafeHandles;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;
using System.Text;

namespace Photonscore.D7;

internal enum SevenType : byte {
  Int8, Int16, Int32, Int64, UInt8, UInt16, UInt32, UInt64, Single, Double,
}

// The seven format's framing and wire primitives, after lib/d7's LogReader
// and seven.proto
internal static unsafe class Seven {
  public const int Block = 0x4000, BlockMask = 0x3FFF;
  public const int FragmentBegin = 0x8000, FragmentEnd = 0x4000;
  public const int MaxEntry = 16 << 20, DefaultBytesPerPage = 16 << 10;
  public const string HeaderSignature = "D7 Photons Data";
  public const string EpilogueSignature = "End of D7 Photons Data File";
  public const string GlobalIndexAttribute = "tnVQlJA4HJWz62GElZ20ZVgf6ac";

  // FileEntry's oneof, by field number
  public enum Kind { None, Header, Data, Index, Epilogue }

  // LogReader::Read over src, the file's bytes [srcBase, srcBase + srcLen):
  // the first entry whose begin fragment lies at or after offset, at data
  // or, reassembled from fragments, at buf's start (data null), its begin
  // fragment at `begin`. -1 when src ends first, `next` then being where to
  // read again.
  public static int ReadEntry(
      byte* src, long srcLen, long srcBase, long offset, ref byte[] buf,
      out byte* data, out long next, out long begin) {
    long p = offset - srcBase;
    int inBlock = (int)(offset & BlockMask);
    begin = -1;
    int n = 0;
    data = null;
    while (n < MaxEntry) {
      long at = p;
      if (Block - inBlock == 1) { p++; inBlock = 0; }
      if (srcLen - p < 2) {
        next = begin >= 0 ? begin : srcBase + at;
        return -1;
      }
      int h = src[p] | (src[p + 1] << 8);
      p += 2;
      inBlock = (inBlock + 2) & BlockMask;
      bool first = (h & FragmentBegin) != 0, last = (h & FragmentEnd) != 0;
      int len = h & BlockMask;
      if (begin >= 0 && first) {
        throw new InvalidDataException(
          $"a fragment begins inside the entry at {begin}");
      }
      if (inBlock + len > Block) {
        throw new InvalidDataException(
          $"a fragment crosses the block at {srcBase + p - 2}");
      }
      inBlock = (inBlock + len) & BlockMask;
      if (begin < 0 && !first) { p += Math.Min(srcLen - p, len); continue; }
      if (begin < 0)
        begin = srcBase + p - 2;
      if (srcLen - p < len) { next = begin; return -1; }
      if (first && last) {
        data = src + p;
        next = srcBase + p + len;
        return len;
      }
      if (n + len > buf.Length) {
        Array.Resize(ref buf, Math.Max(buf.Length * 2, n + len));
      }
      new ReadOnlySpan<byte>(src + p, len).CopyTo(buf.AsSpan(n));
      n += len;
      p += len;
      if (last) { next = srcBase + p; return n; }
    }
    throw new InvalidDataException($"the entry at {begin} runs to 16 MB");
  }

  // A FileEntry's payload: which of its oneof it holds, and the message
  public static Kind Payload(
      byte* p, int len, out byte* message, out byte* end) {
    byte* e = p + len;
    var kind = Kind.None;
    message = end = null;
    while (p < e) {
      ulong tag = Varint(ref p, e);
      if ((tag & 7) == 2 && tag >> 3 is >= 1 and <= 4) {
        byte* m = Len(ref p, e);
        kind = (Kind)(tag >> 3);
        message = p;
        end = m;
        p = m;
      } else {
        Skip(ref p, e, tag);
      }
    }
    return kind;
  }

  [MethodImpl(MethodImplOptions.AggressiveInlining)]
  public static ulong Varint(ref byte* p, byte* end) {
    ulong v = 0;
    for (int shift = 0; shift < 64; shift += 7) {
      if (p >= end)
        TooShort();
      byte b = *p++;
      v |= (ulong)(b & 0x7F) << shift;
      if (b < 0x80)
        return v;
    }
    throw new InvalidDataException("a varint longer than ten bytes");
  }

  // A length-delimited field's end, p left at its start: `p + Varint(ref p)`
  // would read p before the call moves it
  public static byte* Len(ref byte* p, byte* end) {
    ulong l = Varint(ref p, end);
    if (l > (ulong)(end - p))
      TooShort();
    return p + (long)l;
  }

  public static long ZigZag(ulong v) => (long)(v >> 1) ^ -(long)(v & 1);

  public static void Skip(ref byte* p, byte* end, ulong tag) {
    switch ((int)(tag & 7)) {
      case 0:
        Varint(ref p, end);
        break;
      case 1:
        if (end - p < 8)
          TooShort();
        p += 8;
        break;
      case 2:
        p = Len(ref p, end);
        break;
      case 5:
        if (end - p < 4)
          TooShort();
        p += 4;
        break;
      default:
        throw new InvalidDataException($"wire type {tag & 7}");
    }
  }

  public static string Str(ref byte* p, byte* end) {
    byte* e = Len(ref p, end);
    var s = Encoding.UTF8.GetString(p, (int)(e - p));
    p = e;
    return s;
  }

  // RandomAccess.Read until the buffer is full or the file ends
  public static int ReadAt(
      SafeFileHandle file, Span<byte> buffer, long offset) {
    int total = 0;
    while (total < buffer.Length) {
      int n = RandomAccess.Read(file, buffer[total..], offset + total);
      if (n == 0)
        break;
      total += n;
    }
    return total;
  }

  [DoesNotReturn]
  static void TooShort() =>
    throw new InvalidDataException("a field runs past its message");
}
