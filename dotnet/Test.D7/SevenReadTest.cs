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
using Zoo;

namespace Test.D7;

// The reader against what the wrapper read, pinned, and against what was
// written: every dataset whole and in ranges over page edges, every path
public class SevenReadTest(WrittenFile written)
    : IClassFixture<WrittenFile> {
  internal static readonly string D7Photons = Path.Combine(
    Repo.Root, "photonscore/lib/d7/test/data/d7.photons");

  static readonly ParallelOptions Four = new() { MaxDegreeOfParallelism = 4 };

  // d7.photons as the wrapper read it: its datasets, the SHA-256 of those
  // holding data in the same order, its attributes (README.md, *Tests*)
  static readonly string[] D7PhotonsLayout = [
    "/auxiliary/time UInt64 0",
    "/auxiliary/value UInt16 0",
    "/photons/a UInt16 0",
    "/photons/aat UInt64 0",
    "/photons/dt UInt16 625300",
    "/photons/ms UInt64 141324",
    "/photons/q UInt16 0",
    "/photons/x UInt16 625300",
    "/photons/y UInt16 625300",
    "/status/auxiliary UInt16 0",
    "/status/daq_counts UInt64 0",
    "/status/mcp_counts UInt64 0",
    "/status/t UInt64 0",
    "/status/temperature Double 0",
    "/status/voltage Double 0",
  ];
  static readonly string[] D7PhotonsHashes = [
    "317c4d779c8863c1eb4e0b6d6ee691dd74f3ce73ad73aca2094dcd5162ba01da",
    "8f79d01e63709a0ec4ab8c6fea15547f5def997ebb85b466defcc749c9b9a3af",
    "af4709a0e927a25b1060cc4fe0d9920cd35e36e000bff5ce521cd1e00ada4c9d",
    "dea4eb8ef1e7ec9d24ef6d481a19ad6a216ef00db191342fec3b3772cf89d864",
  ];
  static readonly string[] D7PhotonsAttributes = [
    "/photons/DetectorGuid={961a443c-697d-426b-b991-2abe8485695c}",
    "/photons/IsNewFile=0",
    "/photons/PositionBits=12",
    "/photons/TacBias=4095",
    "/photons/TacBits=12",
    "/photons/TacChannel=11.65",
    "/photons/TimerFrequency=100000000",
    "Comment=<comment>",
    "Created=2019-02-27 14:01:33.112",
    "FileCreator=LnTCapture Version 2017.1.0.0",
    "FileGuid={fcd6107a-b0e5-4a95-ad15-764b2c731730}",
    "FileRecorderVersion=2017.0.1 (78a4205d)",
  ];

  internal static void Fetched(string path) {
    if (!File.Exists(path)) {
      Assert.Fail($"fixture not fetched: {path} — run `bobr gfb`");
    }
  }

  [Fact]
  public void D7PhotonsReadsAsTheWrapperReadIt() {
    Fetched(D7Photons);
    using var m = new D7FileReader(D7Photons);
    Assert.False(m.Recovered);
    Assert.Equal(D7PhotonsLayout, Layout(m));
    Assert.Equal(D7PhotonsAttributes, Attributes(m));
    string[] full = [.. Names(m).Where(name => m.Datasets[name].Items > 0)];
    byte[][] wholes = [
      .. full.Select(name => SevenRead.Managed(
        m, name, 0, (int)m.Datasets[name].Items)),
    ];
    Assert.Equal(D7PhotonsHashes, wholes.Select(SevenRead.Sha));
    for (int i = 0; i < full.Length; i++) {
      SameEverywhere(m, full[i], wholes[i]);
    }
  }

  [Fact]
  public void TheWrittenFileReadsAsWritten() {
    using var m = new D7FileReader(written.Path);
    Assert.False(m.Recovered);
    Assert.Equal(
      written.Sets.Select(s => s.Name).Append("/t/empty")
        .Order(StringComparer.Ordinal),
      Names(m));
    for (int s = 0; s < written.Sets.Length; s++) {
      var (name, data, dims) = written.Sets[s];
      var di = m.Datasets[name];
      Assert.Equal(data.GetType().GetElementType(), di.Type);
      long[] shape = [written.Items(s), .. dims.Select(d => (long)d)];
      Assert.Equal(shape, di.Dims);
      SameEverywhere(m, name, SevenRead.Bytes(data));
    }
    Assert.Equal(typeof(uint), m.Datasets["/t/empty"].Type);
    Assert.Equal(0, m.Datasets["/t/empty"].Items);
    Assert.Equal(
      written.Attributes.OrderBy(a => a.Key, StringComparer.Ordinal),
      m.Attributes.OrderBy(a => a.Key, StringComparer.Ordinal));
  }

  [Fact]
  public void SeveralColumnsInOnePassReadAsOneAtATime() {
    Fetched(D7Photons);
    using var m = new D7FileReader(D7Photons);
    const long Offset = 12_345;
    int items = (int)m.Datasets["/photons/x"].Items - 20_000;
    var x = new ushort[items];
    var y = new ushort[items];
    var dt = new ushort[items];
    m.Read([D7Column.Of("/photons/x", x), D7Column.Of("/photons/y", y),
            D7Column.Of<ushort>("/photons/dt", dt.AsMemory())],
           Offset, items, Four);
    Assert.Equal(SevenRead.Managed(m, "/photons/x", Offset, items), Bytes(x));
    Assert.Equal(SevenRead.Managed(m, "/photons/y", Offset, items), Bytes(y));
    Assert.Equal(SevenRead.Managed(m, "/photons/dt", Offset, items),
                 Bytes(dt));

    using var w = new D7FileReader(written.Path);
    const int Items = 10_000;
    var raw = new byte[Items * 32];
    var f64 = new double[Items];
    var i16 = new short[Items];
    w.Read([D7Column.Of("/t/raw", raw), D7Column.Of("/t/f64", f64),
            D7Column.Of("/t/i16", i16)], 777, Items);
    Assert.Equal(SevenRead.Managed(w, "/t/raw", 777, Items), raw);
    Assert.Equal(SevenRead.Managed(w, "/t/f64", 777, Items), Bytes(f64));
    Assert.Equal(SevenRead.Managed(w, "/t/i16", 777, Items), Bytes(i16));
  }

  [Fact]
  public void SpansAndMemoryReadAsArraysDo() {
    using var m = new D7FileReader(written.Path);
    var array = new ushort[50_000];
    var span = new ushort[50_001];
    var memory = new ushort[50_000];
    m.Read(array, "/t/u16", 1000, 50_000);
    m.Read(span.AsSpan(1), "/t/u16", 1000, 50_000, Four);
    m.Read(memory.AsMemory(), "/t/u16", 1000, 50_000);
    Assert.Equal(array, span[1..]);
    Assert.Equal(array, memory);
    Assert.Equal(0, span[0]);
  }

  [Fact]
  public void NullOptionsReadOnTheCallersThread() {
    using var m = new D7FileReader(written.Path);
    int caller = Environment.CurrentManagedThreadId;
    var threads = new HashSet<int>();
    m.RunDecoded = () => threads.Add(Environment.CurrentManagedThreadId);
    int items = written.Items(7);
    m.Read(new ulong[items], "/t/u64", 0, items);
    Assert.Equal(caller, Assert.Single(threads));
  }

  [Fact]
  public void ACancelledReadStopsAfterTheRunsInHand() {
    using var m = new D7FileReader(written.Path);
    int items = written.Items(7);
    var buffer = new ulong[items];
    int all = 0;
    m.RunDecoded = () => Interlocked.Increment(ref all);
    m.Read(buffer, "/t/u64", 0, items, Four);
    Assert.True(all > 8, $"{all} runs: too few for the test to mean much");

    // Counted once Cancel returns: until then the other workers take new
    // runs (README.md, *Traps*)
    foreach (int workers in new[] { 1, 4 }) {
      using var cts = new CancellationTokenSource();
      int runs = 0, atCancel = 0;
      m.RunDecoded = () => {
        if (Interlocked.Increment(ref runs) > 1)
          return;
        cts.Cancel();
        atCancel = Volatile.Read(ref runs);
      };
      var options = new ParallelOptions {
        MaxDegreeOfParallelism = workers, CancellationToken = cts.Token,
      };
      Assert.Throws<OperationCanceledException>(
        () => m.Read(buffer, "/t/u64", 0, items, options));
      Assert.InRange(runs - atCancel, 0, workers - 1);
    }

    using var cancelled = new CancellationTokenSource();
    cancelled.Cancel();
    int none = 0;
    m.RunDecoded = () => Interlocked.Increment(ref none);
    Assert.Throws<OperationCanceledException>(() => m.Read(
      buffer, "/t/u64", 0, items,
      new ParallelOptions { CancellationToken = cancelled.Token }));
    Assert.Equal(0, none);
  }

  [Fact]
  public void ItRefusesWhatItCannotRead() {
    using var m = new D7FileReader(written.Path);
    var u16 = new ushort[100];
    Assert.Throws<ArgumentException>(
      () => m.Read(new short[100], "/t/u16", 0, 100));
    Assert.Throws<ArgumentException>(() => m.Read(u16, "/t/u16", 0, 101));
    Assert.Throws<ArgumentException>(() => m.Read(u16, "/no/such", 0, 1));
    Assert.Throws<ArgumentException>(
      () => m.Read(new ushort[3 * 10 - 1], "/t/xyz", 0, 10));
    long all = m.Datasets["/t/u16"].Items;
    Assert.Throws<ArgumentOutOfRangeException>(
      () => m.Read(u16, "/t/u16", all - 99, 100));
    Assert.Throws<ArgumentOutOfRangeException>(
      () => m.Read(u16, "/t/u16", -1, 1));
    m.Read(u16, "/t/u16", all, 0);
    m.Dispose();
    Assert.Throws<ObjectDisposedException>(
      () => m.Read(u16, "/t/u16", 0, 1));
  }

  // As native's map does; on Windows a reader not sharing for writing would
  // meet a sharing violation here, and so would each worker's own handle
  [Fact]
  public void AFileStillOpenForWritingOpens() {
    var copy = Path.Combine(written.Dir, "recording.photons");
    File.Copy(written.Path, copy, overwrite: true);
    try {
      using var recorder = new FileStream(
        copy, FileMode.Open, FileAccess.ReadWrite, FileShare.Read);
      using var m = new D7FileReader(copy);
      Assert.Equal(SevenRead.Bytes(written.Sets[3].Data),
                   SevenRead.Managed(m, "/t/u16", 0, written.Items(3), Four));
    } finally {
      File.Delete(copy);
    }
  }

  [Fact]
  public void ItOpensSevenFilesOnly() {
    Assert.Throws<FileNotFoundException>(
      () => new D7FileReader(Path.Combine(written.Dir, "none.photons")));
  }

  // Whole, on one thread and on four, then ranges over page edges and at
  // random, against the whole
  static void SameEverywhere(D7FileReader m, string name, byte[] whole) {
    var di = m.Datasets[name];
    long items = di.Items;
    Assert.Equal(whole, SevenRead.Managed(m, name, 0, (int)items));
    Assert.Equal(whole, SevenRead.Managed(m, name, 0, (int)items, Four));
    long perPage = 16384 / (di.ItemBytes / di.ItemSize) / di.ItemSize;
    var rng = new Random(name.Aggregate(17, (h, c) => h * 31 + c));
    var ranges = new List<(long, long)> {
      (0, 1), (items - 1, 1), (perPage - 1, 2), (perPage, perPage),
      (perPage - 1, perPage + 2), (items - perPage - 3, perPage + 3),
      (3 * perPage + 1, 5 * perPage - 2),
    };
    for (int i = 0; i < 8; i++) {
      long offset = rng.NextInt64(items);
      long most = Math.Min(300_000, items - offset);
      ranges.Add((offset, 1 + rng.NextInt64(most)));
    }
    int bytesPerItem = di.ItemBytes;
    foreach (var (offset, count) in ranges) {
      if (offset < 0 || count <= 0 || offset + count > items)
        continue;
      var want = whole.AsSpan((int)(offset * bytesPerItem),
                              (int)(count * bytesPerItem)).ToArray();
      Assert.Equal(want, SevenRead.Managed(m, name, offset, (int)count));
      Assert.Equal(want,
                   SevenRead.Managed(m, name, offset, (int)count, Four));
    }
  }

  internal static IEnumerable<string> Names(D7File f) =>
    f.Datasets.Keys.Order(StringComparer.Ordinal);

  // Each dataset as its name, type and dimensions, in name order
  internal static string[] Layout(D7File f) => [
    .. Names(f).Select(name => f.Datasets[name]).Select(d =>
      $"{d.Name} {d.Type.Name} {string.Join(' ', d.Dims)}"),
  ];

  internal static string[] Attributes(D7File f) => [
    .. f.Attributes.OrderBy(a => a.Key, StringComparer.Ordinal)
      .Select(a => $"{a.Key}={a.Value}"),
  ];

  static byte[] Bytes<T>(T[] a) where T : unmanaged => SevenRead.Bytes(a);
}
