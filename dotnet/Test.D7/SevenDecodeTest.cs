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

using Photonscore.D7;
using Xunit;

namespace Test.D7;

// Pages built by hand: the encodings no file of today's writer holds, and
// the damage a page must not get past
public unsafe class SevenDecodeTest {
  // Version_patch 1 stored u8 and i8 as a seed and zigzag deltas, added in
  // int and cut to the byte, as native's CopyInt<uint8_t> does
  [Fact]
  public void BytesStoredAsIntegersDecodeAsNativeDecodesThem() {
    byte[] data = [
      0x18, 0x90, 0x03, // seed 200
      0x22, 0x05, 0x14, 0xD7, 0x04, 0x6E, 0x00, // +10, -300, +55, 0
    ];
    Assert.Equal(new byte[] { 200, 210, 166, 221, 221 },
                 Decode(SevenType.UInt8, data, 16));
    Assert.Equal(new byte[] { 200, 210, 166, 221, 221 },
                 Decode(SevenType.Int8, data, 16));
    byte[] empty = [0x42, 0x00, .. data];
    Assert.Equal(new byte[] { 200, 210, 166, 221, 221 },
                 Decode(SevenType.UInt8, empty, 16));
    Assert.Equal(new byte[] { 7, 8, 9 },
                 Decode(SevenType.UInt8, [0x42, 0x03, 7, 8, 9], 16));
  }

  [Fact]
  public void APageWithMoreValuesThanItCanHoldIsRefused() {
    byte[] eleven = [0x22, 0x0A, .. new byte[10]];
    Assert.Equal(22, Decode(SevenType.UInt16, eleven, 11).Length);
    Assert.Throws<InvalidDataException>(
      () => Decode(SevenType.UInt16, eleven, 10));
    Assert.Throws<InvalidDataException>(
      () => Decode(SevenType.UInt64, [0x2A, 0x0A, .. new byte[10]], 10));
    Assert.Throws<InvalidDataException>(
      () => Decode(SevenType.UInt8, [0x42, 0x03, 7, 8, 9], 2));
    Assert.Throws<InvalidDataException>(
      () => Decode(SevenType.Single, [0x32, 0x08, .. new byte[8]], 1));
  }

  [Fact]
  public void UnknownFieldsPassAndDamagedOnesDoNot() {
    byte[] ints = [0x22, 0x02, 0x02, 0x04];
    Assert.Equal(new byte[] { 0, 0, 1, 0, 3, 0 },
                 Decode(SevenType.UInt16, [0x48, 0x01, .. ints], 8));
    Assert.Throws<InvalidDataException>(
      () => Decode(SevenType.UInt16, [.. ints, .. ints], 8));
    Assert.Throws<InvalidDataException>(
      () => Decode(SevenType.UInt16, [0x20, 0x02], 8));
    Assert.Throws<InvalidDataException>(
      () => Decode(SevenType.UInt16, [0x22, 0x05, 0x02], 8));
    Assert.Throws<InvalidDataException>(
      () => Decode(SevenType.UInt16, [0x08, 0x01, .. ints], 8));
    Assert.Throws<InvalidDataException>(
      () => Decode(SevenType.Double, [0x3A, 0x05, .. new byte[5]], 8));
  }

  // Protobuf reads a 32-bit varint of up to ten bytes and keeps the low 32
  // bits; the unchecked loop sees it only ten bytes from the end
  [Fact]
  public void LongVarintsKeepWhatProtobufKeeps() {
    byte[] six = [0x82, 0x80, 0x80, 0x80, 0x80, 0x00];
    byte[] packed = [.. six, .. new byte[10]];
    byte[] data = [0x22, (byte)packed.Length, .. packed];
    var values = Decode(SevenType.UInt16, data, 16);
    Assert.Equal(new ushort[] { 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
                 Ushorts(values));
    byte[] eleven = [.. Enumerable.Repeat((byte)0xFF, 10), 0x01];
    byte[] tooLong = [.. eleven, .. new byte[10]];
    Assert.Throws<InvalidDataException>(() => Decode(
      SevenType.UInt64, [0x2A, (byte)tooLong.Length, .. tooLong], 32));
  }

  // One Data message of dataset 0 through the decoder, as the bytes it
  // wrote
  static byte[] Decode(SevenType type, byte[] data, int capacity) {
    var ds = new Dataset("/d", 0, type, [0]);
    byte[] entry = [0x12, (byte)data.Length, .. data];
    var dst = new byte[capacity * ds.ElemBytes];
    int n;
    fixed (byte* e = entry)
    fixed (byte* d = dst) {
      n = Photonscore.D7.Decode.Page(ds, e, entry.Length, d, capacity);
    }
    return dst[..(n * ds.ElemBytes)];
  }

  static ushort[] Ushorts(byte[] bytes) {
    var values = new ushort[bytes.Length / 2];
    Buffer.BlockCopy(bytes, 0, values, 0, bytes.Length);
    return values;
  }
}
