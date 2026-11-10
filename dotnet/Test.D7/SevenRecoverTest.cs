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

// Truncated copies, an unclosed file's every state, against what native's
// `recover full` read of them, pinned; then the one-byte pad before a
// running index (README.md, *Decisions in force*)
public sealed unsafe class SevenRecoverTest(WrittenFile written)
    : IClassFixture<WrittenFile>, IDisposable {
  const int Block = 16384, Step = 512;
  static readonly ParallelOptions Four = new() { MaxDegreeOfParallelism = 4 };

  // Each cut as Cuts lists it and what recover full read of it (README.md,
  // *Tests*): a digest of every dataset and attribute, or its refusal
  static readonly string[] D7PhotonsCuts = [
    "0 refused",
    "364 refused",
    "365 9ef7e424424a7ebcad7783fc2dd6a493a64833c5e641a97821d72d8aaf2b7138",
    "5365 9ef7e424424a7ebcad7783fc2dd6a493a64833c5e641a97821d72d8aaf2b7138",
    "655360 002b0e79813563712bc1973eaf3751b6f91eedd657e5922f8313ec14038c86d4",
    "655361 002b0e79813563712bc1973eaf3751b6f91eedd657e5922f8313ec14038c86d4",
    "3137650 9b66aee28097cc84c0cfd750bbc3cd3bc76d48a58949774a0d72499d5502dfd4",
    "3137651 983b67f1395a30f801cc776da7b908c428c7169c31e3c7eac9d022d1d480cad7",
    "3141708 983b67f1395a30f801cc776da7b908c428c7169c31e3c7eac9d022d1d480cad7",
    "3145765 983b67f1395a30f801cc776da7b908c428c7169c31e3c7eac9d022d1d480cad7",
    "757763 89fa9939716baa852afe8b29e9294d36f2a4d0d15da2955841ca7d7b5c3c4060",
    "1462920 00c4a87a02856af5523e150ea30afc8482e6ac887f9f95cb7d7b6a6fc5f08e22",
  ];
  static readonly string[] WrittenCuts = [
    "0 refused",
    "241 refused",
    "242 d42f57166f4168b421b76e0ee671b88c21c81227360e928f558dbbd9845620db",
    "5242 d42f57166f4168b421b76e0ee671b88c21c81227360e928f558dbbd9845620db",
    "655360 46db602e363169b12be29a0fa109bd43d527d2175e11a4b9de39ce0190c8b3e0",
    "655361 46db602e363169b12be29a0fa109bd43d527d2175e11a4b9de39ce0190c8b3e0",
    "32543600 98322f918bafdd94c6db98595dfc85931fd0d78ff6536f487e9cd3f2811674ae",
    "32543601 8aa737dbc9c0e801277912fa494b4e45b31f888e5d0bd00ce672a29c3a9207a3",
    "32565707 8aa737dbc9c0e801277912fa494b4e45b31f888e5d0bd00ce672a29c3a9207a3",
    "32587813 8aa737dbc9c0e801277912fa494b4e45b31f888e5d0bd00ce672a29c3a9207a3",
    "8388608 b9826ee288fe969f7e619115b985ea44a805a266e44f284f0d568f18f958946f",
    "8406485 55e6b436ff1d4499fbdb996640ca74812c434ef809ca41c173b9661a091c2651",
    "8406486 a00edb6b590b156ce6af7283b62f7eabf602ae143d77dda4ba7e4694a5b999d6",
    "8413486 a00edb6b590b156ce6af7283b62f7eabf602ae143d77dda4ba7e4694a5b999d6",
    "16777216 927e55ca69a1624bf878a7d3145584cb918796dca821027f1f2894d8a18ab6c1",
    "16786656 8d46e0e570a96fed6d56c0647e8b8a79e0e8f370847c6e577cd70f2134c1c775",
    "16786657 8d46e0e570a96fed6d56c0647e8b8a79e0e8f370847c6e577cd70f2134c1c775",
    "16793657 8d46e0e570a96fed6d56c0647e8b8a79e0e8f370847c6e577cd70f2134c1c775",
    "25165824 20260f578bc1d6b70b922f3f70ebb0d559eb0a71a726ee68182f6b5b2fb64f2d",
    "25182723 747def6402e5b25d0704c0be3d6ac809bdd03ff4bc0116d755deefe2d28174b0",
    "25182724 747def6402e5b25d0704c0be3d6ac809bdd03ff4bc0116d755deefe2d28174b0",
    "25189724 747def6402e5b25d0704c0be3d6ac809bdd03ff4bc0116d755deefe2d28174b0",
    "29024392 6d984a0ecc6efd05b055f8fdd91bce179b1a266e8df8f3ea5eee151e9853e881",
    "6059427 a0a19c7435bf8de8bb3ebad75171130cc1b53c3d8d4840ba0e3ac9d2aaa8182c",
  ];

  readonly string _dir =
    Directory.CreateTempSubdirectory("d7-recover-").FullName;

  public void Dispose() => TempDir.Delete(_dir);

  [Fact]
  public void TheWrittenFileHasThreeRunningIndexes() {
    using var file = File.OpenHandle(written.Path);
    long length = RandomAccess.GetLength(file);
    Assert.True(length > 3L * Step * Block, $"{length} bytes");
    var cursor = new EntryCursor(file, length);
    for (long k = 1; k <= 3; k++) {
      Assert.Equal(Seven.Kind.Index, KindAt(cursor, k * Step * Block));
    }
    Assert.Equal(
      "28f172e0ad3c7dfb6ee2af5239b70000b5e114fbefa72f6282bc982a838f96ce",
      Steps.Sha(written.Path));
  }

  [Fact]
  public void TruncatedCopiesReadAsRecoverFullReadThem() {
    SevenReadTest.Fetched(SevenReadTest.D7Photons);
    (string, string[])[] files = [
      (SevenReadTest.D7Photons, D7PhotonsCuts), (written.Path, WrittenCuts),
    ];
    foreach (var (path, pins) in files) {
      var (header, cuts) = Cuts(path);
      Assert.True(cuts.Count >= 12, $"{cuts.Count} cuts of {path}");
      var read = new List<string>();
      foreach (long cut in cuts) {
        var copy = Path.Combine(_dir, "cut.photons");
        CopyPrefix(path, copy, cut);
        if (cut < header) {
          Assert.Throws<InvalidDataException>(() => new D7FileReader(copy));
          read.Add($"{cut} refused");
          continue;
        }
        using var m = new D7FileReader(copy);
        Assert.True(m.Recovered, $"{path} cut at {cut}");
        read.Add($"{cut} {SevenRead.Digest(m)}");
      }
      Assert.Equal(pins, read);
    }
  }

  // Where native fails the open, "Cannot find index at block 512" (README.md,
  // *Decisions in force*)
  [Fact]
  public void APadBeforeAWholePageRecoversEveryPage() {
    var (data, cut) = Padded(raw: false);
    using var m = new D7FileReader(cut);
    Assert.True(m.Recovered);
    Holds(m, data);
  }

  // As native's `recover full` did
  [Fact]
  public void APadBeforeASpilledPageRecoversEveryPage() {
    var (data, cut) = Padded(raw: true);
    using var m = new D7FileReader(cut);
    Assert.True(m.Recovered);
    Holds(m, data);
  }

  // LnTCapture's close amid an append: a last page listed two bytes into a
  // block that another page crosses, where no entry begins (README.md,
  // *Decisions in force*)
  [Fact]
  public void ALastPageListedInsideAnotherRecoversEveryPage() {
    var copy = Path.Combine(_dir, "misplaced.photons");
    File.Copy(written.Path, copy);
    using (var file = File.OpenHandle(
             copy, FileMode.Open, FileAccess.ReadWrite)) {
      long length = RandomAccess.GetLength(file);
      var cursor = new EntryCursor(file, length);
      long index = GlobalIndexOffset(cursor, length);
      long last = SevenIndex.Open(file, length).ById
        .Single(d => d.Name == "/t/u16").Pages[^1];
      long block = index / Block * Block;
      while (!cursor.Read(block, out _, out _) || cursor.Begin <= block + 1) {
        block -= Block;
      }
      var tail = new byte[length - index];
      RandomAccess.Read(file, tail, index);
      byte[] from = [0x10, .. Varint(last)], to = [0x10, .. Varint(block + 2)];
      int at = tail.AsSpan().IndexOf(from);
      Assert.True(at >= 0 && from.Length == to.Length, $"{last} {block}");
      Assert.Equal(-1, tail.AsSpan(at + 1).IndexOf(from));
      RandomAccess.Write(file, to, index + at);
    }
    using var m = new D7FileReader(copy);
    Assert.True(m.Recovered);
    for (int s = 0; s < written.Sets.Length; s++) {
      var (name, data, _) = written.Sets[s];
      Assert.Equal(SevenRead.Bytes(data),
                   SevenRead.Managed(m, name, 0, written.Items(s), Four));
    }
    Assert.Equal(new[] { "Comment", "Empty" }, m.Attributes.Keys.Order());
  }

  static byte[] Varint(long v) {
    var bytes = new List<byte>();
    for (; v >= 0x80; v >>= 7)
      bytes.Add((byte)(v | 0x80));
    bytes.Add((byte)v);
    return [.. bytes];
  }

  // Every branch of the scan: in and after the header, at a block's edge,
  // at and in the running indexes, at the data's end, in the global index
  // and the epilogue, and at random
  static (long Header, List<long> Cuts) Cuts(string path) {
    using var file = File.OpenHandle(path);
    long length = RandomAccess.GetLength(file);
    var cursor = new EntryCursor(file, length);
    Assert.True(cursor.Read(0, out _, out long header));
    long data = GlobalIndexOffset(cursor, length);
    var cuts = new List<long> {
      0, header - 1, header, header + 5000, 40L * Block, 40L * Block + 1,
      data - 1, data, (data + length) / 2, length - 1,
    };
    for (long k = Step; k * Block < data; k += Step) {
      Assert.True(cursor.Read(k * Block, out _, out long index));
      cuts.AddRange([k * Block, index - 1, index, index + 7000]);
    }
    var rng = new Random(3);
    cuts.Add(header + rng.NextInt64(data - header));
    cuts.Add(header + rng.NextInt64(data - header));
    return (header, [.. cuts.Where(c => c >= 0 && c < length).Distinct()]);
  }

  // A file whose entry before block 512 leaves one byte in its block: the
  // pad moves the next page to the block's start and the index after it,
  // inside that block for a whole page, in the next for a spilled one
  (PaddedData Data, string Cut) Padded(bool raw) {
    var sizes = Path.Combine(_dir, "sizes.photons");
    Write(sizes, Pages(raw, 0, 0, 1));
    var (header, shim, page) = Lengths(sizes);
    var (m, k) = (from pages in Enumerable.Range(0, raw ? 1 : 32)
                  from shims in Enumerable.Range(0, 8000)
                  where EndsAtPad(header, shim + shims, page + pages)
                  select (pages, shims)).FirstOrDefault((-1, -1));
    Assert.True(k >= 0, "no shim puts a pad before block 512");
    var full = Path.Combine(_dir, raw ? "spilled.photons" : "whole.photons");
    var data = Pages(raw, m, k, raw ? 560 : 1100);
    Write(full, data);

    using var file = File.OpenHandle(full);
    long length = RandomAccess.GetLength(file);
    var head = new byte[3];
    RandomAccess.Read(file, head, (long)Step * Block - 1);
    Assert.Equal(0, head[0]);
    Assert.NotEqual(0, head[2] & 0x80);
    var cursor = new EntryCursor(file, length);
    Assert.Equal(Seven.Kind.Data, KindAt(cursor, (long)Step * Block));
    Assert.Equal(raw ? Seven.Kind.Index : Seven.Kind.Data,
                 KindAt(cursor, (Step + 1L) * Block));
    var cut = Path.ChangeExtension(full, ".cut.photons");
    CopyPrefix(full, cut, GlobalIndexOffset(cursor, length));
    return (data, cut);
  }

  // /a a stream of pages, u16 whole in a block or raw bytes that spill
  // over; /shim one u16 page
  record PaddedData(Array A, ushort[] Shim);

  // m and k two-byte varints tune the pages' sizes
  static PaddedData Pages(bool raw, int m, int k, int pages) {
    if (raw) {
      var bytes = new byte[pages * Block];
      var rng = new Random(11);
      for (int i = 0; i < pages; i++) {
        rng.NextBytes(bytes.AsSpan(i * Block, Block));
      }
      return new(bytes, Tuned(k));
    }
    var page = Tuned(m);
    var a = new ushort[pages * page.Length];
    for (int i = 0; i < pages; i++)
      page.CopyTo(a, i * page.Length);
    return new(a, Tuned(k));
  }

  static void Write(string path, PaddedData data) {
    using var w = new D7FileWriter(path);
    w.CreateDataset("/a", data.A.GetType().GetElementType()!);
    w.CreateDataset<ushort>("/shim");
    w.Append(data.Shim, "/shim");
    if (data.A is byte[] bytes)
      w.Append(bytes, "/a");
    else
      w.Append((ushort[])data.A, "/a");
  }

  // A u16 page whose first n deltas are ±100, two bytes each, the rest 0
  static ushort[] Tuned(int n) {
    var page = new ushort[8192];
    for (int i = 1; i < page.Length; i++) {
      page[i] = i <= n ? (ushort)(i % 2 * 100) : page[i - 1];
    }
    return page;
  }

  static (int Header, int Shim, int Page) Lengths(string path) {
    using var file = File.OpenHandle(path);
    var cursor = new EntryCursor(file, RandomAccess.GetLength(file));
    var lengths = new int[3];
    long at = 0;
    for (int i = 0; i < 3; i++) {
      Assert.True(cursor.Read(at, out var entry, out at));
      lengths[i] = entry.Length;
    }
    return (lengths[0], lengths[1], lengths[2]);
  }

  // LogAppender's layout: whether an entry before block 512 ends one byte
  // short of it
  static bool EndsAtPad(int header, int shim, int page) {
    long target = (long)Step * Block - 1, size = 0;
    for (int i = 0; size < target; i++) {
      long left = i == 0 ? header : i == 1 ? shim : page;
      while (left > 0) {
        if (Block - size % Block < 2)
          size++;
        long fragment = Math.Min(left, Block - size % Block - 2);
        size += 2 + fragment;
        left -= fragment;
      }
      if (size == target)
        return true;
    }
    return false;
  }

  // Every page written: a cut at the global index loses none
  static void Holds(D7FileReader m, PaddedData data) {
    Assert.Equal(new[] { "/a", "/shim" }, SevenReadTest.Names(m));
    Assert.Equal(SevenRead.Bytes(data.Shim), SevenRead.Managed(
      m, "/shim", 0, data.Shim.Length, Four));
    Assert.Equal(SevenRead.Bytes(data.A), SevenRead.Managed(
      m, "/a", 0, data.A.Length, Four));
  }

  static Seven.Kind KindAt(EntryCursor cursor, long offset) {
    Assert.True(cursor.Read(offset, out var entry, out _));
    fixed (byte* e = entry)
      return Seven.Payload(e, entry.Length, out _, out _);
  }

  static long GlobalIndexOffset(EntryCursor cursor, long length) {
    Assert.True(cursor.Read(length / Block * Block, out var entry, out _));
    fixed (byte* e = entry) {
      var kind = Seven.Payload(e, entry.Length, out var p, out var end);
      Assert.Equal(Seven.Kind.Epilogue, kind);
      Assert.Equal(0x08ul, Seven.Varint(ref p, end));
      return (long)Seven.Varint(ref p, end);
    }
  }

  static void CopyPrefix(string from, string to, long bytes) {
    using var source = File.OpenRead(from);
    using var copy = File.Create(to);
    var buffer = new byte[1 << 20];
    for (long left = bytes; left > 0;) {
      int n = source.Read(buffer, 0, (int)Math.Min(buffer.Length, left));
      copy.Write(buffer, 0, n);
      left -= n;
    }
  }
}
