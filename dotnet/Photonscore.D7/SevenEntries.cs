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

using System.Text;

namespace Photonscore.D7;

// The header, the indexes and the epilogue as native's protobuf writes them:
// fields in number order, proto3's defaults left out, a map entry's key and
// value always written
internal static unsafe class SevenEntries {
  public const int IndexStep = 512, VersionPatch = 2;

  // SerializeLargeEntry's bound: 3/4 of the largest fragment, estimated at
  // 12 bytes a page plus every attribute's key and value
  public const long SplitAt = (16L << 20) * 3 / 4;

  public static byte[] Header(IReadOnlyList<Dataset> toc) {
    var h = new Proto();
    h.String(1, Seven.HeaderSignature);
    h.Varint(2, 1);
    h.Varint(4, VersionPatch);
    h.Varint(6, IndexStep);
    foreach (var ds in toc) {
      var d = new Proto();
      d.String(1, ds.Name);
      d.Varint(2, (ulong)ds.Type);
      var size = new Proto();
      size.Raw(0);
      for (int i = 1; i < ds.Dims.Length; i++)
        size.Raw((ulong)ds.Dims[i]);
      d.Bytes(3, size.Span);
      h.Bytes(7, d.Span);
    }
    var e = new Proto();
    e.Bytes(1, h.Span);
    return e.ToArray();
  }

  public static byte[] Epilogue(long globalIndexOffset) {
    var p = new Proto();
    p.Varint(1, (ulong)globalIndexOffset);
    p.String(2, Seven.EpilogueSignature);
    var e = new Proto();
    e.Bytes(4, p.Span);
    return e.ToArray();
  }

  // The global index as SerializeLargeEntry cuts it: whole below the bound,
  // else the attributes in chunks, then chunks of 2^20 pages
  public static List<byte[]> GlobalIndex(
      List<(uint Id, long Offset)> pages,
      IReadOnlyList<(byte[] Key, byte[] Value)> attributes,
      long splitAt = SplitAt) {
    long estimate = 12L * pages.Count;
    foreach (var (key, value) in attributes) {
      estimate += key.Length + value.Length;
    }
    if (estimate < splitAt)
      return [Index(pages, 0, pages.Count, attributes)];
    var entries = new List<byte[]>();
    var chunk = new List<(byte[] Key, byte[] Value)>();
    long size = 0;
    foreach (var kv in attributes) {
      long n = kv.Key.Length + kv.Value.Length;
      if (size + n >= splitAt && chunk.Count > 0) {
        entries.Add(Index(pages, 0, 0, chunk));
        chunk.Clear();
        size = 0;
      }
      chunk.Add(kv);
      size += n;
    }
    if (size > 0)
      entries.Add(Index(pages, 0, 0, chunk));
    int from = 0;
    size = 0;
    for (int i = 0; i < pages.Count; i++) {
      size += 12;
      if (size < splitAt)
        continue;
      entries.Add(Index(pages, from, i + 1 - from, []));
      from = i + 1;
      size = 0;
    }
    if (size > 0)
      entries.Add(Index(pages, from, pages.Count - from, []));
    return entries;
  }

  // One Index entry: pages[from, from + count), then the attributes
  public static byte[] Index(
      List<(uint Id, long Offset)> pages, int from, int count,
      IReadOnlyList<(byte[] Key, byte[] Value)> attributes) {
    long body = 0;
    for (int i = from; i < from + count; i++) {
      int info = Info(pages[i].Id, pages[i].Offset);
      body += 1 + Encode.Length((ulong)info) + info;
    }
    foreach (var (key, value) in attributes) {
      int kv = Pair(key, value);
      body += 1 + Encode.Length((ulong)kv) + kv;
    }
    var entry = new byte[1 + Encode.Length((ulong)body) + body];
    fixed (byte* e = entry) {
      byte* q = e;
      *q++ = 0x1A;
      q = Encode.Put(q, (ulong)body);
      for (int i = from; i < from + count; i++) {
        var (id, offset) = pages[i];
        *q++ = 0x0A;
        q = Encode.Put(q, (ulong)Info(id, offset));
        if (id != 0) {
          *q++ = 0x08;
          q = Encode.Put(q, id);
        }
        if (offset != 0) {
          *q++ = 0x10;
          q = Encode.Put(q, (ulong)offset);
        }
      }
      foreach (var (key, value) in attributes) {
        *q++ = 0x12;
        q = Encode.Put(q, (ulong)Pair(key, value));
        q = Field(q, 0x0A, key);
        q = Field(q, 0x12, value);
      }
    }
    return entry;
  }

  static int Info(uint id, long offset) =>
    (id != 0 ? 1 + Encode.Length(id) : 0) +
    (offset != 0 ? 1 + Encode.Length((ulong)offset) : 0);

  static int Pair(byte[] key, byte[] value) =>
    2 + Encode.Length((ulong)key.Length) + key.Length +
    Encode.Length((ulong)value.Length) + value.Length;

  static byte* Field(byte* q, byte tag, byte[] bytes) {
    *q++ = tag;
    q = Encode.Put(q, (ulong)bytes.Length);
    bytes.CopyTo(new Span<byte>(q, bytes.Length));
    return q + bytes.Length;
  }

  // Just enough protobuf for the header and the epilogue
  sealed class Proto {
    byte[] _b = new byte[256];
    int _n;

    public ReadOnlySpan<byte> Span => _b.AsSpan(0, _n);

    public byte[] ToArray() => _b[.._n];

    public void Raw(ulong v) {
      Ensure(10);
      fixed (byte* b = _b)
        _n = (int)(Encode.Put(b + _n, v) - b);
    }

    public void Varint(int field, ulong v) {
      if (v == 0)
        return;
      Raw((ulong)(field << 3));
      Raw(v);
    }

    public void String(int field, string s) {
      if (s.Length != 0)
        Bytes(field, Encoding.UTF8.GetBytes(s));
    }

    public void Bytes(int field, ReadOnlySpan<byte> bytes) {
      Raw((ulong)(field << 3 | 2));
      Raw((ulong)bytes.Length);
      Ensure(bytes.Length);
      bytes.CopyTo(_b.AsSpan(_n));
      _n += bytes.Length;
    }

    void Ensure(int more) {
      if (_n + more > _b.Length) {
        Array.Resize(ref _b, Math.Max(_b.Length * 2, _n + more));
      }
    }
  }
}
