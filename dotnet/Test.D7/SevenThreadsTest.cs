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
using System.Collections.Concurrent;
using Xunit;

namespace Test.D7;

// One writer and one reader shared by threads (README.md, *Threads*)
public sealed class SevenThreadsTest : IDisposable {
  const int Threads = 4;
  static readonly ParallelOptions Four = new() { MaxDegreeOfParallelism = 4 };

  readonly string _dir =
    Directory.CreateTempSubdirectory("d7-threads-").FullName;

  public void Dispose() => TempDir.Delete(_dir);

  // Each thread creates datasets of its own, of every type and rank between
  // them, and an attribute, then appends at random and sets it again; the
  // odd ones encode on four workers
  [Fact]
  public void AWriterAppendedToFromSeveralThreadsReadsBackAsAppended() {
    var path = Path.Combine(_dir, "shared.photons");
    (string Name, Array Data, int[] Dims)[] sets =
      WrittenFile.Generate(new Random(41), 600_000);
    var steps = new List<Step>[Threads];
    for (int t = 0; t < Threads; t++) {
      var own = sets.Where((_, s) => s % Threads == t).ToArray();
      steps[t] = Steps.Interleaved(own, new Random(43 + t));
    }
    using (var w = new D7FileWriter(path)) {
      OnThreads(Threads, t => {
        foreach (var c in steps[t].OfType<CreateStep>()) {
          Steps.Replay(w, c, null);
        }
        w[$"thread {t}"] = "created";
      });
      OnThreads(Threads, t => {
        var options = t % 2 == 1 ? Four : null;
        foreach (var a in steps[t].OfType<AppendStep>()) {
          Steps.Replay(w, a, options);
        }
        w[$"thread {t}"] = $"{steps[t].Count} steps";
      });
    }
    using var r = new D7FileReader(path);
    Assert.False(r.Recovered);
    Assert.Equal(sets.Select(s => s.Name).Order(StringComparer.Ordinal),
                 r.Datasets.Keys.Order(StringComparer.Ordinal));
    foreach (var (name, data, dims) in sets) {
      int items = data.Length / dims.Aggregate(1, (a, b) => a * b);
      Assert.Equal(items, r.Datasets[name].Items);
      Assert.Equal(SevenRead.Bytes(data),
                   SevenRead.Managed(r, name, 0, items, Four));
    }
    Assert.Equal(Threads, r.Attributes.Count);
    for (int t = 0; t < Threads; t++) {
      Assert.Equal($"{steps[t].Count} steps", r[$"thread {t}"]);
    }
  }

  // Every dataset whole and in ranges, read on one thread, then by eight
  // threads at once in orders of their own, the odd ones on four workers
  [Fact]
  public void OneReaderReadFromSeveralThreadsReadsWhatOneThreadReads() {
    var path = Path.Combine(_dir, "read.photons");
    var sets = WrittenFile.Generate(new Random(29), 300_000);
    Steps.Managed(path, Steps.Interleaved(sets, new Random(31)));
    var reads = new List<(string Name, long Offset, int Items, byte[] Want)>();
    var rng = new Random(37);
    foreach (var (name, data, dims) in sets) {
      int items = data.Length / dims.Aggregate(1, (a, b) => a * b);
      var bytes = SevenRead.Bytes(data);
      int size = bytes.Length / items;
      reads.Add((name, 0, items, bytes));
      for (int i = 0; i < 6; i++) {
        int offset = rng.Next(items);
        int count = 1 + rng.Next(Math.Min(100_000, items - offset));
        reads.Add((name, offset, count,
                   bytes[(offset * size)..((offset + count) * size)]));
      }
    }
    using var r = new D7FileReader(path);
    foreach (var (name, offset, items, want) in reads) {
      Assert.Equal(want, SevenRead.Managed(r, name, offset, items));
    }
    var wrong = new ConcurrentQueue<string>();
    OnThreads(8, t => {
      var options = t % 2 == 1 ? Four : null;
      var order = new Random(t);
      foreach (var (name, offset, items, want) in
               reads.OrderBy(_ => order.Next())) {
        var got = SevenRead.Managed(r, name, offset, items, options);
        if (!got.AsSpan().SequenceEqual(want)) {
          wrong.Enqueue($"thread {t}: {name}, {items} from {offset}");
        }
      }
    });
    Assert.Empty(wrong);
  }

  // Work on threads of its own, started together
  static void OnThreads(int n, Action<int> work) {
    using var start = new Barrier(n);
    var failed = new ConcurrentQueue<Exception>();
    var threads = Enumerable.Range(0, n).Select(t => new Thread(() => {
      start.SignalAndWait();
      try {
        work(t);
      } catch (Exception e) {
        failed.Enqueue(e);
      }
    })).ToArray();
    foreach (var thread in threads)
      thread.Start();
    foreach (var thread in threads)
      thread.Join();
    if (!failed.IsEmpty)
      throw new AggregateException(failed);
  }
}
