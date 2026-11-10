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
using System.Text;
using Xunit;

namespace Test.D7;

// The global index cut as native's SerializeLargeEntry cuts it: whole below
// the bound, else the attributes in chunks, then the pages in chunks
public sealed unsafe class SevenSplitTest : IDisposable {
  static readonly (byte[] Key, byte[] Value) Marker = (
    Encoding.UTF8.GetBytes(Seven.GlobalIndexAttribute),
    Encoding.UTF8.GetBytes(Seven.GlobalIndexAttribute));

  readonly string _dir =
    Directory.CreateTempSubdirectory("d7-split-").FullName;

  public void Dispose() => TempDir.Delete(_dir);

  // 12 bytes a page and the marker's 54 against 12,582,912: 2^20 pages a
  // chunk once split
  [Fact]
  public void AtNativesBoundTheIndexSplits() {
    Assert.Equal(new[] { "1048571 pages, marker" },
                 Chunks(1_048_571, [Marker]));
    Assert.Equal(new[] { "0 pages, marker", "1048572 pages" },
                 Chunks(1_048_572, [Marker]));
    Assert.Equal(new[] { "0 pages, marker", "1048576 pages", "5 pages" },
                 Chunks((1 << 20) + 5, [Marker]));
  }

  // At 120 bytes: attributes while they stay under it, ten pages a chunk
  [Fact]
  public void ASmallBoundCutsBothLoops() {
    (byte[], byte[]) Pair(string key, int value) =>
      (Encoding.UTF8.GetBytes(key), new byte[value]);
    Assert.Equal(
      new[] {
        "0 pages, a b", "0 pages, c, marker", "10 pages", "10 pages",
        "5 pages",
      },
      Chunks(25, [Pair("a", 20), Pair("b", 60), Pair("c", 50), Marker], 120));
  }

  // A file whose global index is split early, its hash pinned while the
  // wrapper read it back
  [Fact]
  public void ASplitIndexReadsBack() {
    var path = Path.Combine(_dir, "split.photons");
    var rng = new Random(19);
    var data = new ushort[3][];
    using (var w = new D7FileWriter(path)) {
      w.SplitAt = 12 * 16;
      w["Comment"] = "split";
      string[] names = ["/x", "/y", "/z"];
      foreach (var name in names)
        w.CreateDataset<ushort>(name);
      for (int d = 0; d < 3; d++) {
        data[d] = new ushort[40 * 8192 + 100 * d];
        for (int i = 0; i < data[d].Length; i++) {
          data[d][i] = (ushort)rng.Next(4096);
        }
      }
      for (int at = 0; at < data[2].Length; at += 10_000) {
        for (int d = 0; d < 3; d++) {
          int count = Math.Min(10_000, data[d].Length - at);
          if (count > 0)
            w.Append(data[d].AsSpan(at, count), names[d]);
        }
      }
    }
    Assert.Equal(9, GlobalIndexEntries(path));
    Assert.Equal(
      "00a48866ad6de8c57e98ece6c5a462c0cfe0bde45d4c158f8ba9d10ef25b267a",
      Steps.Sha(path));
    using var m = new D7FileReader(path);
    Assert.False(m.Recovered);
    Assert.Equal(
      new[] { "/x UInt16 327680", "/y UInt16 327780", "/z UInt16 327880" },
      SevenReadTest.Layout(m));
    Assert.Equal(new[] { "Comment=split" }, SevenReadTest.Attributes(m));
    for (int d = 0; d < 3; d++) {
      string name = "/" + (char)('x' + d);
      Assert.Equal(SevenRead.Bytes(data[d]),
                   SevenRead.Managed(m, name, 0, data[d].Length));
    }
  }

  // Each entry of the cut index as its pages and attributes, the pages
  // checked to come back in order
  static List<string> Chunks(
      int count, (byte[] Key, byte[] Value)[] attributes,
      long splitAt = SevenEntries.SplitAt) {
    var pages = new List<(uint Id, long Offset)>(count);
    for (int i = 0; i < count; i++)
      pages.Add(((uint)(i % 3), 16384L * i + 9));
    var back = new List<(uint Id, long Offset)>();
    var chunks = new List<string>();
    var entries = SevenEntries.GlobalIndex(pages, attributes, splitAt);
    foreach (var entry in entries) {
      var found = new List<(uint Id, long Offset)>();
      var attrs = new Dictionary<string, string>();
      bool marker;
      fixed (byte* e = entry) {
        var kind = Seven.Payload(e, entry.Length, out var p, out var end);
        Assert.Equal(Seven.Kind.Index, kind);
        marker = SevenIndex.ParseIndex(p, end, found, attrs);
      }
      back.AddRange(found);
      var keys = attrs.Keys.Order(StringComparer.Ordinal);
      chunks.Add($"{found.Count} pages" +
                 (attrs.Count > 0 ? ", " + string.Join(' ', keys) : "") +
                 (marker ? ", marker" : ""));
    }
    Assert.Equal(pages, back);
    return chunks;
  }

  // The index entries between the epilogue's offset and the epilogue
  static int GlobalIndexEntries(string path) {
    using var file = File.OpenHandle(path);
    long length = RandomAccess.GetLength(file);
    var cursor = new EntryCursor(file, length);
    Assert.True(cursor.Read(length / 16384 * 16384, out var epilogue, out _));
    long at;
    fixed (byte* e = epilogue) {
      var kind = Seven.Payload(e, epilogue.Length, out var p, out var end);
      Assert.Equal(Seven.Kind.Epilogue, kind);
      Assert.Equal(0x08ul, Seven.Varint(ref p, end));
      at = (long)Seven.Varint(ref p, end);
    }
    int entries = 0;
    while (true) {
      Assert.True(cursor.Read(at, out var entry, out at));
      fixed (byte* e = entry) {
        var kind = Seven.Payload(e, entry.Length, out _, out _);
        if (kind == Seven.Kind.Epilogue)
          return entries;
        Assert.Equal(Seven.Kind.Index, kind);
      }
      entries++;
    }
  }
}
