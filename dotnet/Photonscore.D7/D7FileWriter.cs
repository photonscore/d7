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

using System.Buffers;
using System.Text;

namespace Photonscore.D7;

// A new seven file, byte for byte what native writes: datasets first, then
// appends and attributes, then Close; every call takes the writer's lock.
public sealed unsafe class D7FileWriter : D7File {
  const int BytesPerPage = Seven.DefaultBytesPerPage;
  const long IndexBytes = (long)Seven.Block * SevenEntries.IndexStep;

  readonly Lock _lock = new();
  readonly string _path;
  readonly SevenLog _log;
  readonly List<Column> _toc = [];
  readonly Dictionary<string, Column> _byName = new(StringComparer.Ordinal);
  readonly List<(uint Id, long Offset)> _running = [], _global = [];
  readonly SortedDictionary<string, string> _runningAttributes =
    new(StringComparer.Ordinal);
  readonly byte[] _entry = new byte[Encode.Capacity(BytesPerPage)];
  bool _headerWritten, _closed;

  // Values in full pages an append needs before a Parallel.For pays for its
  // wake-ups, four u16 pages on hpph: README.md, *Decisions in force*
  internal long ParallelFrom = 32_768;

  // Tests: called as each page of a parallel encode is done; a global index
  // split early
  internal Action? PageEncoded;
  internal long SplitAt = SevenEntries.SplitAt;

  internal Action<long>? BeforeWrite { set => _log.BeforeWrite = value; }

  public D7FileWriter(string filename) {
    _path = filename;
    _log = new SevenLog(File.OpenHandle(
      filename, FileMode.CreateNew, FileAccess.Write, FileShare.Read));
  }

  // Written with the next running index and the global one, as native does
  public string this[string attribute] {
    get {
      lock (_lock)
        return _attributes[attribute];
    }
    set {
      lock (_lock) {
        Check();
        ArgumentNullException.ThrowIfNull(attribute);
        ArgumentNullException.ThrowIfNull(value);
        if (attribute == Seven.GlobalIndexAttribute) {
          throw new ArgumentException(
            "The global index's marker is no attribute.", nameof(attribute));
        }
        long bytes = Encoding.UTF8.GetByteCount(attribute) +
                     Encoding.UTF8.GetByteCount(value);
        if (bytes > Seven.MaxEntry / 2) {
          throw new ArgumentException(
            $"An attribute of {bytes} bytes; at most {Seven.MaxEntry / 2}.",
            nameof(value));
        }
        _attributes[attribute] = value;
        _runningAttributes[attribute] = value;
      }
    }
  }

  public void CreateDataset<T>(string name, params int[] dims)
      where T : unmanaged =>
    CreateDataset(name, typeof(T), dims);

  public void CreateDataset(string name, Type type, params int[] dims) {
    lock (_lock) {
      Check();
      if (_headerWritten) {
        throw new InvalidOperationException(
          "Datasets are created before the first append.");
      }
      var code = Code(type) ?? throw new ArgumentException(
        $"Unsupported dataset type: {type}", nameof(type));
      if (dims.Length > 2) {
        throw new ArgumentException(
          $"A dataset of rank {dims.Length + 1}; at most 3.", nameof(dims));
      }
      long elems = 1;
      foreach (int d in dims) {
        if (d < 1) {
          throw new ArgumentException($"A dimension of {d}.", nameof(dims));
        }
        elems *= d;
      }
      if (elems > int.MaxValue) {
        throw new ArgumentException($"Items of {elems} values.", nameof(dims));
      }
      string normal = Normalize(name);
      if (_datasets.ContainsKey(normal)) {
        throw new ArgumentException(
          $"Dataset '{normal}' exists in {_path}.", nameof(name));
      }
      long[] shape = [0, .. dims.Select(d => (long)d)];
      var column = new Column(
        new Dataset(normal, (uint)_toc.Count, code, shape),
        new D7DatasetInfo { Name = normal, Type = type, Dims = [.. shape] });
      _toc.Add(column);
      _byName[normal] = column;
      _datasets[normal] = column.Info;
    }
  }

  public void Append<T>(T[] buffer, string dataset,
                        ParallelOptions? options = null)
      where T : unmanaged =>
    Append((ReadOnlySpan<T>)buffer, dataset, options);

  public void Append<T>(ReadOnlyMemory<T> buffer, string dataset,
                        ParallelOptions? options = null)
      where T : unmanaged =>
    Append(buffer.Span, dataset, options);

  // Null options encode on the caller's thread. An append is all or
  // nothing: a cancel stops it before any page is framed.
  public void Append<T>(ReadOnlySpan<T> buffer, string dataset,
                        ParallelOptions? options = null)
      where T : unmanaged {
    lock (_lock) {
      Check();
      var column = Find(dataset);
      if (typeof(T) != column.Ds.ClrType) {
        throw new ArgumentException(
          $"Dataset '{column.Ds.Name}' is of type '{column.Ds.ClrType}', " +
          $"but the provided buffer is of type '{typeof(T)}'.");
      }
      if (buffer.Length % column.Ds.ItemElems != 0) {
        throw new ArgumentException(
          $"{buffer.Length} values are no whole items of " +
          $"{column.Ds.ItemElems}.", nameof(buffer));
      }
      fixed (T* p = buffer) {
        AppendBytes(
          column, (byte*)p, (long)buffer.Length * sizeof(T), options);
      }
      column.Info.Dims[0] += buffer.Length / column.Ds.ItemElems;
    }
  }

  protected override void Dispose(bool disposing) {
    if (!disposing)
      return;
    lock (_lock) {
      if (_closed)
        return;
      _closed = true;
      try {
        if (!_log.Failed)
          Finish();
      } finally {
        _log.Dispose();
      }
    }
  }

  // A dataset being written: its partial page and what Datasets shows
  sealed class Column(Dataset ds, D7DatasetInfo info) {
    public readonly Dataset Ds = ds;
    public readonly D7DatasetInfo Info = info;
    public readonly byte[] Utf8Name = Encoding.UTF8.GetBytes(ds.Name);
    public readonly byte[] Page = new byte[BytesPerPage];
    public int PageLen;
  }

  // The partial page completed, the full pages, the rest kept: the pages
  // native's PagedAppender makes of the same bytes
  void AppendBytes(Column c, byte* p, long bytes, ParallelOptions? options) {
    int head = c.PageLen > 0
      ? (int)Math.Min(BytesPerPage - c.PageLen, bytes) : 0;
    long full = (bytes - head) / BytesPerPage;
    (byte[] Buf, int From, int Len)[]? encoded = null;
    if (options != null) {
      options.CancellationToken.ThrowIfCancellationRequested();
      if (full * (BytesPerPage / c.Ds.ElemBytes) >= ParallelFrom) {
        encoded = EncodeAll(c, p + head, (int)full, options);
      }
    }
    try {
      EnsureHeader();
      if (head > 0) {
        new ReadOnlySpan<byte>(p, head).CopyTo(c.Page.AsSpan(c.PageLen));
        c.PageLen += head;
        if (c.PageLen == BytesPerPage) {
          fixed (byte* page = c.Page)
            Emit(c, page, BytesPerPage);
          c.PageLen = 0;
        }
      }
      byte* at = p + head;
      for (long i = 0; i < full; i++) {
        if (encoded == null) {
          Emit(c, at + i * BytesPerPage, BytesPerPage);
          continue;
        }
        var (buf, from, len) = encoded[i];
        Frame(c, buf.AsSpan(from, len));
        ArrayPool<byte>.Shared.Return(buf);
        encoded[i] = default;
      }
      int tail = (int)(bytes - head - full * BytesPerPage);
      new ReadOnlySpan<byte>(at + full * BytesPerPage, tail)
        .CopyTo(c.Page.AsSpan(c.PageLen));
      c.PageLen += tail;
    } finally {
      if (encoded != null)
        Return(encoded);
    }
  }

  (byte[] Buf, int From, int Len)[] EncodeAll(
      Column c, byte* first, int pages, ParallelOptions options) {
    var encoded = new (byte[] Buf, int From, int Len)[pages];
    int capacity = Encode.Capacity(BytesPerPage);
    try {
      Parallel.For(0, pages, options, i => {
        var buf = ArrayPool<byte>.Shared.Rent(capacity);
        fixed (byte* b = buf) {
          var (from, len) = Encode.Page(
            c.Ds, first + (long)i * BytesPerPage, BytesPerPage, b);
          encoded[i] = (buf, from, len);
        }
        PageEncoded?.Invoke();
      });
    } catch {
      Return(encoded);
      throw;
    }
    return encoded;
  }

  static void Return((byte[] Buf, int From, int Len)[] encoded) {
    foreach (var (buf, _, _) in encoded) {
      if (buf != null)
        ArrayPool<byte>.Shared.Return(buf);
    }
  }

  void Emit(Column c, byte* page, int bytes) {
    fixed (byte* b = _entry) {
      var (from, len) = Encode.Page(c.Ds, page, bytes, b);
      Frame(c, _entry.AsSpan(from, len));
    }
  }

  // FlushIndex: a page entry listed, and the running index written after
  // the entry that crosses each 512-block boundary
  void Frame(Column c, ReadOnlySpan<byte> entry) {
    long before = _log.Size;
    _log.Append(entry);
    _running.Add((c.Ds.Id, before));
    _global.Add((c.Ds.Id, before));
    if (before / IndexBytes == _log.Size / IndexBytes)
      return;
    _log.Append(SevenEntries.Index(
      _running, 0, _running.Count, Pairs(_runningAttributes)));
    _running.Clear();
    _runningAttributes.Clear();
  }

  void EnsureHeader() {
    if (_headerWritten)
      return;
    _headerWritten = true;
    _log.Append(SevenEntries.Header([.. _toc.Select(c => c.Ds)]));
  }

  // Native's Close: the partial pages in name order, the global index, the
  // padding, the epilogue in a fresh block
  void Finish() {
    EnsureHeader();
    var byName = Comparer<byte[]>.Create(
      (a, b) => a.AsSpan().SequenceCompareTo(b));
    foreach (var c in _toc.OrderBy(c => c.Utf8Name, byName)) {
      if (c.PageLen == 0)
        continue;
      fixed (byte* page = c.Page)
        Emit(c, page, c.PageLen);
      c.PageLen = 0;
    }
    long at = _log.Size;
    var attributes = new SortedDictionary<string, string>(
      _attributes, StringComparer.Ordinal) {
      [Seven.GlobalIndexAttribute] = Seven.GlobalIndexAttribute,
    };
    var pairs = Pairs(attributes);
    foreach (var e in SevenEntries.GlobalIndex(_global, pairs, SplitAt)) {
      _log.Append(e);
    }
    _log.PadToBlock();
    _log.Append(SevenEntries.Epilogue(at));
    _log.Finish();
  }

  static List<(byte[] Key, byte[] Value)> Pairs(
      SortedDictionary<string, string> attributes) =>
    [.. attributes.Select(a => (Encoding.UTF8.GetBytes(a.Key),
                                Encoding.UTF8.GetBytes(a.Value)))];

  void Check() {
    ObjectDisposedException.ThrowIf(_closed, this);
    _log.ThrowIfFailed();
  }

  Column Find(string dataset) {
    ArgumentNullException.ThrowIfNull(dataset);
    if (_byName.TryGetValue(dataset, out var column))
      return column;
    if (Normal(dataset) is not { } normal ||
        !_byName.TryGetValue(normal, out column)) {
      throw new ArgumentException(
        $"No dataset '{dataset}' in {_path}.", nameof(dataset));
    }
    _byName[dataset] = column;
    return column;
  }

  static string Normalize(string name) {
    ArgumentNullException.ThrowIfNull(name);
    return Normal(name) ?? throw new ArgumentException(
      $"'{name}' names no dataset under the root.", nameof(name));
  }

  // lib/path's Join("/", name): the same for every name but one opening
  // with two backslashes, which native reads as a share and this refuses
  static string? Normal(string name) {
    var parts = name.Split(['/', '\\'], StringSplitOptions.RemoveEmptyEntries);
    return parts.Length == 0 || name.StartsWith(@"\\", StringComparison.Ordinal)
      ? null : "/" + string.Join('/', parts);
  }

  static SevenType? Code(Type type) =>
    type == typeof(sbyte) ? SevenType.Int8 :
    type == typeof(short) ? SevenType.Int16 :
    type == typeof(int) ? SevenType.Int32 :
    type == typeof(long) ? SevenType.Int64 :
    type == typeof(byte) ? SevenType.UInt8 :
    type == typeof(ushort) ? SevenType.UInt16 :
    type == typeof(uint) ? SevenType.UInt32 :
    type == typeof(ulong) ? SevenType.UInt64 :
    type == typeof(float) ? SevenType.Single :
    type == typeof(double) ? SevenType.Double : null;
}
