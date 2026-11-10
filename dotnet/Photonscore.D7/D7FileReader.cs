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
using System.Buffers;
using System.Collections.Concurrent;
using System.Diagnostics.CodeAnalysis;

namespace Photonscore.D7;

// A seven file's datasets, read into spans on the caller's threads
public sealed unsafe class D7FileReader : D7File {
  // About 1 MB a read: the cold knee on the M1, flat on the Xeon (README.md)
  const int RunBytes = 1 << 20;

  readonly string _path;
  readonly SafeFileHandle _file;
  readonly SevenIndex _index;
  readonly Dictionary<string, Dataset> _byName = new(StringComparer.Ordinal);
  readonly ConcurrentBag<SafeFileHandle> _handles = [];
  volatile bool _disposed;

  // Tests: called on the worker as each run is decoded
  internal Action? RunDecoded;

  public D7FileReader(string filename) {
    _path = filename;
    _file = Open(filename);
    try {
      _index = SevenIndex.Open(_file, RandomAccess.GetLength(_file));
    } catch {
      _file.Dispose();
      throw;
    }
    var byName = _index.ById.OrderBy(d => d.Name, StringComparer.Ordinal);
    foreach (var ds in byName) {
      _byName[ds.Name] = ds;
      _datasets[ds.Name] = ds.Info();
    }
    foreach (var (key, value) in _index.Attributes)
      _attributes[key] = value;
  }

  // Opened from the running indexes and a scan: the file was never closed
  public bool Recovered => _index.Recovered;

  public string this[string attribute] => _attributes[attribute];

  public void Read<T>(T[] buffer, string dataset, long offset, int items,
                      ParallelOptions? options = null) where T : unmanaged =>
    Read(buffer.AsSpan(), dataset, offset, items, options);

  public void Read<T>(Memory<T> buffer, string dataset, long offset,
                      int items, ParallelOptions? options = null)
      where T : unmanaged =>
    Read(buffer.Span, dataset, offset, items, options);

  // Null options read on the caller's thread; their token cancels between
  // reads of about 1 MB
  public void Read<T>(Span<T> buffer, string dataset, long offset, int items,
                      ParallelOptions? options = null) where T : unmanaged {
    var target = Aim(dataset, typeof(T), buffer.Length, offset, items);
    fixed (T* p = buffer) {
      ReadTargets([target with { Dst = (nint)p }], options);
    }
  }

  // The same items of several datasets in one pass through the file
  public void Read(ReadOnlySpan<D7Column> columns, long offset, int items,
                   ParallelOptions? options = null) {
    var targets = new Target[columns.Length];
    for (int i = 0; i < columns.Length; i++) {
      var c = columns[i];
      targets[i] = Aim(c.Dataset, c.ElementType, c.Length, offset, items);
    }
    var pins = new MemoryHandle[columns.Length];
    try {
      for (int i = 0; i < columns.Length; i++) {
        pins[i] = columns[i].Pin();
        targets[i] = targets[i] with { Dst = (nint)pins[i].Pointer };
      }
      ReadTargets(targets, options);
    } finally {
      foreach (var pin in pins)
        pin.Dispose();
    }
  }

  protected override void Dispose(bool disposing) {
    if (!disposing || _disposed)
      return;
    _disposed = true;
    while (_handles.TryTake(out var h))
      h.Dispose();
    _file.Dispose();
  }

  // Elements [First, First + Count) of a dataset, to Dst
  readonly record struct Target(Dataset Ds, nint Dst, long First, long Count);

  readonly record struct Work(long Offset, int Target, long Page);

  readonly record struct Run(int From, int To, long Start, long End);

  Target Aim(string dataset, Type type, long length, long offset, int items) {
    ObjectDisposedException.ThrowIf(_disposed, this);
    if (!_byName.TryGetValue(dataset, out var ds)) {
      throw new ArgumentException(
        $"No dataset '{dataset}' in {_path}.", nameof(dataset));
    }
    if (type != ds.ClrType) {
      throw new ArgumentException(
        $"Dataset '{dataset}' is of type '{ds.ClrType}', " +
        $"but the provided buffer is of type '{type}'.");
    }
    ArgumentOutOfRangeException.ThrowIfNegative(offset);
    ArgumentOutOfRangeException.ThrowIfNegative(items);
    if (offset > ds.Items - items) {
      throw new ArgumentOutOfRangeException(nameof(items),
        $"Items [{offset}, {offset + items}) of '{dataset}', " +
        $"which has {ds.Items}.");
    }
    long count = items * ds.ItemElems;
    if (length < count) {
      throw new ArgumentException(
        $"A buffer of {length} for {count} values.", "buffer");
    }
    return new(ds, 0, offset * ds.ItemElems, count);
  }

  void ReadTargets(Target[] targets, ParallelOptions? options) {
    var (work, runs) = Plan(targets);
    if (runs.Count == 0)
      return;
    if (options is null) {
      var w = Rent();
      try {
        foreach (var run in runs)
          ReadRun(w, run, work, targets);
      } finally {
        Return(w);
      }
      return;
    }
    Parallel.For(0, runs.Count, options, Rent, (r, _, w) => {
      ReadRun(w, runs[r], work, targets);
      return w;
    }, Return);
  }

  // The wanted pages of every target in file order, cut into runs of about
  // RunBytes, a run one read
  (Work[] Work, List<Run> Runs) Plan(Target[] targets) {
    var work = new List<Work>();
    for (int t = 0; t < targets.Length; t++) {
      var (ds, _, first, count) = targets[t];
      if (count == 0)
        continue;
      long p0 = first / ds.PerPage, p1 = (first + count - 1) / ds.PerPage;
      for (long pg = p0; pg <= p1; pg++)
        work.Add(new(ds.Pages[pg], t, pg));
    }
    work.Sort((a, b) => a.Offset.CompareTo(b.Offset));
    var runs = new List<Run>();
    for (int i = 0; i < work.Count;) {
      int j = i;
      long start = work[i].Offset, end = _index.NextStart(start);
      while (j + 1 < work.Count) {
        long next = _index.NextStart(work[j + 1].Offset);
        if (next - start > RunBytes)
          break;
        end = next;
        j++;
      }
      runs.Add(new(i, j, start, end));
      i = j + 1;
    }
    return ([.. work], runs);
  }

  void ReadRun(Worker w, Run run, Work[] work, Target[] targets) {
    int n = (int)(run.End - run.Start);
    if (w.Io.Length < n) {
      ArrayPool<byte>.Shared.Return(w.Io);
      w.Io = ArrayPool<byte>.Shared.Rent(n);
    }
    int got = Seven.ReadAt(w.Handle, w.Io.AsSpan(0, n), run.Start);
    fixed (byte* io = w.Io) {
      for (int k = run.From; k <= run.To; k++) {
        var (offset, t, page) = work[k];
        int len = Seven.ReadEntry(
          io, got, run.Start, offset, ref w.Entry, out var data, out _, out _);
        if (len < 0)
          Cut(targets[t].Ds, page);
        fixed (byte* entry = w.Entry) {
          PageInto(targets[t], page, data != null ? data : entry, len, w.Page);
        }
      }
    }
    RunDecoded?.Invoke();
  }

  // One page into its target: straight in when the target wants all of it,
  // else through the worker's page buffer
  static void PageInto(in Target t, long page, byte* data, int len,
                       byte[] buffer) {
    var ds = t.Ds;
    int perPage = ds.PerPage;
    long first = page * perPage;
    long lo = Math.Max(t.First, first);
    long hi = Math.Min(t.First + t.Count, first + perPage);
    byte* at = (byte*)t.Dst + (lo - t.First) * ds.ElemBytes;
    if (lo == first && hi == first + perPage) {
      int whole = Decode.Page(ds, data, len, at, perPage);
      if (whole != perPage)
        Short(ds, page, whole);
      return;
    }
    fixed (byte* p = buffer) {
      int n = Decode.Page(ds, data, len, p, perPage);
      bool last = page == ds.Pages.Length - 1;
      if (n < perPage && !last || first + n < hi)
        Short(ds, page, n);
      int bytes = (int)((hi - lo) * ds.ElemBytes);
      new ReadOnlySpan<byte>(p + (lo - first) * ds.ElemBytes, bytes)
        .CopyTo(new Span<byte>(at, bytes));
    }
  }

  // A worker's buffers and file handle for the length of one read
  sealed class Worker(SafeFileHandle handle, int bytesPerPage) {
    public readonly SafeFileHandle Handle = handle;
    public byte[] Io = ArrayPool<byte>.Shared.Rent(RunBytes);
    public byte[] Entry = new byte[64 << 10];
    public readonly byte[] Page = new byte[bytesPerPage];
  }

  Worker Rent() => new(TakeHandle(), _index.BytesPerPage);

  void Return(Worker w) {
    ArrayPool<byte>.Shared.Return(w.Io);
    GiveHandle(w.Handle);
  }

  // Windows serializes the reads on a synchronous handle through its
  // file-object lock, so a worker there reads through a handle of its own
  SafeFileHandle TakeHandle() {
    if (!OperatingSystem.IsWindows())
      return _file;
    return _handles.TryTake(out var h) ? h : Open(_path);
  }

  // Shared for writing as native's map is, so a file still being recorded
  // opens; not for deleting, so the path names it for every worker's handle
  static SafeFileHandle Open(string path) => File.OpenHandle(
    path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);

  void GiveHandle(SafeFileHandle h) {
    if (h == _file)
      return;
    if (_disposed)
      h.Dispose();
    else
      _handles.Add(h);
  }

  [DoesNotReturn]
  static void Cut(Dataset ds, long page) => throw new InvalidDataException(
    $"page {page} of {ds.Name} runs past the data");

  [DoesNotReturn]
  static void Short(Dataset ds, long page, int n) =>
    throw new InvalidDataException(
      $"page {page} of {ds.Name} holds {n} values");
}
