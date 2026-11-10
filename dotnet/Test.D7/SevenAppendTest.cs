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

// An append is all or nothing, a failed write final (README.md, *Writing*)
public sealed class SevenAppendTest : IDisposable {
  readonly string _dir =
    Directory.CreateTempSubdirectory("d7-append-").FullName;

  public void Dispose() => TempDir.Delete(_dir);

  // Cancelled before anything, below the threshold and amid a parallel
  // encode: the file is native's without those appends, its hash pinned
  [Fact]
  public void ACancelledAppendLeavesNativesFileWithoutIt() {
    var rng = new Random(13);
    var x = new ushort[2_000_000];
    var t = new ulong[2_000_000];
    for (int i = 0; i < x.Length; i++) {
      x[i] = (ushort)rng.Next(4096);
      t[i] = (ulong)i * 10_000 + (ulong)rng.Next(20);
    }
    const int Half = 600_000;
    var path = Path.Combine(_dir, "cancelled.photons");
    using (var w = new D7FileWriter(path)) {
      using var cancelled = new CancellationTokenSource();
      cancelled.Cancel();
      var dead = new ParallelOptions { CancellationToken = cancelled.Token };
      w.CreateDataset<ushort>("/x");
      Assert.Throws<OperationCanceledException>(
        () => w.Append(x.AsSpan(0, Half), "/x", dead));
      w.CreateDataset<ulong>("/t");
      w.Append(x.AsSpan(0, Half), "/x");
      w.Append(t.AsSpan(0, Half), "/t");
      Assert.Throws<OperationCanceledException>(
        () => w.Append(x.AsSpan(Half, 100), "/x", dead));

      using var midway = new CancellationTokenSource();
      int pages = 0;
      w.ParallelFrom = 1;
      w.PageEncoded = () => {
        Interlocked.Increment(ref pages);
        midway.Cancel();
      };
      var four = new ParallelOptions {
        MaxDegreeOfParallelism = 4, CancellationToken = midway.Token,
      };
      Assert.Throws<OperationCanceledException>(
        () => w.Append(x.AsSpan(Half), "/x", four));
      Assert.True(pages >= 1, "cancelled amid the encode");
      w.PageEncoded = null;
      Assert.Equal(Half, w.Datasets["/x"].Items);

      w.Append(x.AsSpan(Half), "/x",
               new ParallelOptions { MaxDegreeOfParallelism = 4 });
      w.Append(t.AsSpan(Half), "/t");
    }
    Assert.Equal(
      "0f7ef54e44cb5ef68b28e90331246fdfcafa818e317c09fcbb3cfcaf26d89271",
      Steps.Sha(path));
  }

  // The third 4 MB buffer's write fails: an append then throws, every call
  // after it too, and the file holds the first two, which recovery reads
  [Fact]
  public void AFailedWriteIsFinal() {
    var path = Path.Combine(_dir, "failed.photons");
    var x = new ushort[20_000_000];
    var rng = new Random(17);
    for (int i = 0; i < x.Length; i++)
      x[i] = (ushort)rng.Next(4096);
    var w = new D7FileWriter(path);
    w.BeforeWrite = at => {
      if (at >= 8 << 20)
        throw new IOException("planted");
    };
    w.CreateDataset<ushort>("/x");
    IOException? failed = null;
    for (int at = 0; at < x.Length && failed == null; at += 500_000) {
      try {
        w.Append(x.AsSpan(at, 500_000), "/x");
      } catch (IOException e) {
        failed = e;
      }
    }
    Assert.Equal("planted", failed?.Message);
    Assert.Equal("planted", Assert.Throws<IOException>(
      () => w.Append(x.AsSpan(0, 10), "/x")).Message);
    Assert.Throws<IOException>(() => w["Comment"] = "late");
    Assert.Throws<IOException>(() => w.CreateDataset<int>("/y"));
    w.Dispose();
    w.Dispose();
    Assert.Equal(8L << 20, new FileInfo(path).Length);
    using (var r = new D7FileReader(path)) {
      Assert.True(r.Recovered);
      long items = r.Datasets["/x"].Items;
      Assert.True(items > 3_000_000, $"{items} items");
      Assert.Equal(SevenRead.Bytes(x)[..(int)(2 * items)],
                   SevenRead.Managed(r, "/x", 0, (int)items));
    }
    File.Delete(path);
  }
}
