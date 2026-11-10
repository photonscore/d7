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

// The same calls write native's bytes, on the caller's thread and on four:
// their hashes pinned while the wrapper judged (README.md, *Tests*)
public sealed class SevenWriteTest : IDisposable {
  static readonly ParallelOptions Four = new() { MaxDegreeOfParallelism = 4 };

  readonly string _dir =
    Directory.CreateTempSubdirectory("d7-write-").FullName;

  public void Dispose() => TempDir.Delete(_dir);

  [Fact]
  public void EveryTypeAndRankWritesNativesBytes() {
    var sets = WrittenFile.Generate(new Random(5), 900_000);
    List<Step> steps = [
      new CreateStep("/t/empty", typeof(uint), []),
      .. Steps.Interleaved(sets, new Random(7)),
    ];
    long bytes = SameAsNative(steps,
      "46fe290cce75e102c22eeafe7c4f2c2205c2f5131b4675c73918fe284976fae1");
    Assert.True(bytes > 3L * (8 << 20), "three running indexes");
  }

  [Theory]
  [InlineData(1, 30_000,
    "6d19f5b2a723046d163dd9b81d634be23b7598c599243d7221b1ea04658a435c")]
  [InlineData(7, 200_000,
    "7b1ed8c9ad9ef015243c189ec980daa8508eae9176f3f4b3cf54050be0d2b172")]
  [InlineData(1000, 1_000_000,
    "0dd5045d9496399fc4d37c838a5be2da6eb63f08af3e7d6367fca5e98212dc58")]
  [InlineData(100_000, 3_000_000,
    "5cdf2213edfa50ac278470b94461b6ab7e0bc75bef28cac7adc3334a13303fbc")]
  [InlineData(1_000_000, 3_500_000,
    "37f0ecdeba7cc563c7f81c6aa639c275c0459baa8ab5b4799102de5d8fb5097d")]
  public void PhotonsAndTagsWriteNativesBytes(int chunk, int items,
                                              string sha256) {
    var rng = new Random(chunk);
    var x = new ushort[items];
    var y = new ushort[items];
    var dt = new ushort[items];
    var tags = new ulong[items];
    for (int i = 0; i < items; i++) {
      x[i] = (ushort)rng.Next(4096);
      y[i] = (ushort)rng.Next(4096);
      dt[i] = (ushort)(rng.Next(3) == 0 ? rng.Next(64) : rng.Next(4096));
      tags[i] = (ulong)i * 10_000 + (ulong)rng.Next(20);
    }
    List<Step> steps = [
      new CreateStep("/photons/x", typeof(ushort), []),
      new CreateStep("/photons/y", typeof(ushort), []),
      new CreateStep("/photons/dt", typeof(ushort), []),
      new CreateStep("/tags", typeof(ulong), []),
    ];
    for (int at = 0; at < items; at += chunk) {
      int n = Math.Min(chunk, items - at);
      steps.Add(new AppendStep("/photons/x", x, at, n));
      steps.Add(new AppendStep("/photons/y", y, at, n));
      steps.Add(new AppendStep("/photons/dt", dt, at, n));
      steps.Add(new AppendStep("/tags", tags, at, n));
    }
    SameAsNative(steps, sha256);
  }

  [Fact]
  public void EmptyFilesWriteNativesBytes() {
    SameAsNative([],
      "9231379db829af53d76e80174d72f20394c87bab80fd21e279f4a72ceca8b21e");
    SameAsNative([
      new CreateStep("/a", typeof(ushort), []),
      new CreateStep("/b", typeof(double), [3]),
    ], "b8fe2b7c2d1bc3aea8d9ef18871680252e48831f3d93137ef6e08b2da1d843f0");
    SameAsNative([
      new CreateStep("/a", typeof(ushort), []),
      new AppendStep("/a", new ushort[1], 0, 0),
    ], "8e1ea62c0e26c8d7159e3d41b56cc6c5f95c8ac640942cdb1d415fcd9fff5b66");
  }

  // Native's lib/path normalizes a dataset's name; either spelling appends
  [Fact]
  public void NamesNormalizeAsNativesDo() {
    var data = Enumerable.Range(0, 50_000).Select(i => (ushort)i).ToArray();
    SameAsNative([
      new CreateStep("photons/x", typeof(ushort), []),
      new CreateStep(@"\t\y", typeof(ushort), [2]),
      new CreateStep("//a//b.c/", typeof(ushort), []),
      new AppendStep("photons/x", data, 0, 30_000),
      new AppendStep(@"\t\y", data, 0, 40_000),
      new AppendStep("//a//b.c/", data, 0, 20_000),
    ], "6edee6f143fb906bda9c924cc0eb7b5cca9a91c7d0f4b4e1ffd263918b2cf2d9");
    using var w = new D7FileWriter(Path.Combine(_dir, "names.photons"));
    w.CreateDataset<ushort>("photons/x");
    w.Append(data, "photons/x");
    w.Append(data, "/photons/x");
    w.Append(data, @"\photons\x\");
    Assert.Equal(new[] { "/photons/x" }, w.Datasets.Keys);
    Assert.Equal(150_000, w.Datasets["/photons/x"].Items);
  }

  // Native writes attributes in protobuf's map order, the managed writer
  // in ordinal order: the dictionaries native read, not its bytes
  // (question 5)
  [Fact]
  public void AttributesReadBackAsNativesDo() {
    var x = new ushort[5_000_000];
    var rng = new Random(3);
    for (int i = 0; i < x.Length; i++)
      x[i] = (ushort)rng.Next(4096);
    List<Step> steps = [
      new CreateStep("/x", typeof(ushort), []),
      new AttributeStep("Comment", "grün, 光子, 12 µm"),
      new AttributeStep("Empty", ""),
      new AppendStep("/x", x, 0, 2_500_000),
      new AttributeStep("Long", new string('x', 5000)),
      new AttributeStep("Comment", "second"),
      new AppendStep("/x", x, 2_500_000, 2_500_000),
      new AttributeStep("Created", "2026-09-26 12:00:00.000"),
    ];
    var path = Path.Combine(_dir, "attributes.photons");
    Steps.Managed(path, steps);
    Assert.Equal(
      "832e7d33286da1a6ca362c4bee99ef78bf9617213a0d972bc024790c8b15c57c",
      Steps.Sha(path));
    var want = new Dictionary<string, string> {
      ["Comment"] = "second", ["Empty"] = "", ["Long"] = new string('x', 5000),
      ["Created"] = "2026-09-26 12:00:00.000",
    };
    using (var m = new D7FileReader(path)) {
      Assert.Equal(Sorted(want), Sorted(m.Attributes));
      Assert.Equal(SevenRead.Bytes(x),
                   SevenRead.Managed(m, "/x", 0, 5_000_000));
    }
    // Cut past the first running index, recovery finds the attributes set
    // before it
    var cut = path + ".cut";
    File.WriteAllBytes(cut, File.ReadAllBytes(path)[..(9 << 20)]);
    using var r = new D7FileReader(cut);
    Assert.True(r.Recovered);
    want.Remove("Created");
    Assert.Equal(Sorted(want), Sorted(r.Attributes));
  }

  // Shared for reading as native's file is: a reader recovers what is
  // written while the writer is open
  [Fact]
  public void AFileBeingWrittenOpensForReading() {
    var path = Path.Combine(_dir, "recording.photons");
    var x = new ushort[6_000_000];
    var rng = new Random(9);
    for (int i = 0; i < x.Length; i++)
      x[i] = (ushort)rng.Next(4096);
    using var w = new D7FileWriter(path);
    w.CreateDataset<ushort>("/x");
    w.Append(x, "/x");
    using var r = new D7FileReader(path);
    Assert.True(r.Recovered);
    long items = r.Datasets["/x"].Items;
    Assert.True(items > 1_000_000, $"{items} items");
    Assert.Equal(SevenRead.Bytes(x)[..(int)(2 * items)],
                 SevenRead.Managed(r, "/x", 0, (int)items));
  }

  [Fact]
  public void ItRefusesWhatItCannotWrite() {
    var path = Path.Combine(_dir, "refused.photons");
    File.WriteAllBytes(path, [1]);
    Assert.Throws<IOException>(() => new D7FileWriter(path));
    File.Delete(path);
    var w = new D7FileWriter(path);
    Assert.Throws<ArgumentException>(() => w.CreateDataset("/a", typeof(char)));
    Assert.Throws<ArgumentException>(
      () => w.CreateDataset<ushort>("/a", 2, 3, 4));
    Assert.Throws<ArgumentException>(() => w.CreateDataset<ushort>("/a", 0));
    Assert.Throws<ArgumentException>(() => w.CreateDataset<ushort>("//"));
    Assert.Throws<ArgumentException>(() => w.CreateDataset<ushort>(@"\\srv\a"));
    w.CreateDataset<ushort>("/a", 3);
    Assert.Throws<ArgumentException>(() => w.CreateDataset<short>("a"));
    Assert.Throws<ArgumentException>(() => w.Append(new short[3], "/a"));
    Assert.Throws<ArgumentException>(() => w.Append(new ushort[4], "/a"));
    Assert.Throws<ArgumentException>(() => w.Append(new ushort[3], "/b"));
    Assert.Throws<ArgumentException>(
      () => w[Seven.GlobalIndexAttribute] = "x");
    Assert.Throws<ArgumentException>(() => w["big"] = new string('x', 8 << 20));
    w.Append(new ushort[3], "/a");
    Assert.Throws<InvalidOperationException>(
      () => w.CreateDataset<ushort>("/b"));
    w.Dispose();
    Assert.Throws<ObjectDisposedException>(() => w.Append(new ushort[3], "/a"));
    Assert.Throws<ObjectDisposedException>(() => w["k"] = "v");
    Assert.Throws<ObjectDisposedException>(() => w.CreateDataset<int>("/c"));
    w.Dispose();
    using var r = new D7FileReader(path);
    Assert.Equal(1, r.Datasets["/a"].Items);
  }

  // The file on the caller's thread and on four, from the default threshold,
  // from any page and from 2^19 values, against native's pinned hash; its
  // length
  long SameAsNative(List<Step> steps, string sha256) {
    var path = Path.Combine(_dir, "managed.photons");
    (string, ParallelOptions?, long?)[] ways = [
      ("the caller's thread", null, null), ("four", Four, null),
      ("four, always parallel", Four, 1),
      ("four from 2^19 values", Four, 1 << 19),
    ];
    foreach (var (way, options, from) in ways) {
      File.Delete(path);
      Steps.Managed(path, steps, options, w => {
        if (from is long values)
          w.ParallelFrom = values;
      });
      string sha = Steps.Sha(path);
      Assert.True(sha256 == sha, $"{way}: {sha}");
    }
    return new FileInfo(path).Length;
  }

  static List<KeyValuePair<string, string>> Sorted(
      IEnumerable<KeyValuePair<string, string>> d) =>
    [.. d.OrderBy(kv => kv.Key, StringComparer.Ordinal)];
}
