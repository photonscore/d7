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

namespace Photonscore.D7;

// What opening a seven file learns: the table of contents, each dataset's
// pages and count, the attributes, and whether the index was recovered
internal sealed unsafe class SevenIndex {
  public Dataset[] ById = [];
  public readonly Dictionary<string, string> Attributes =
    new(StringComparer.Ordinal);
  public int BytesPerPage = Seven.DefaultBytesPerPage;
  public int IndexStep;
  public uint VersionPatch;
  public bool Recovered;
  long[] _starts = []; // every page's offset, sorted, then where data ends

  public static SevenIndex Open(SafeFileHandle file, long length) {
    var ix = new SevenIndex();
    var cursor = new EntryCursor(file, length);
    ix.ReadHeader(cursor);
    if (!ix.ReadGlobalIndex(cursor)) {
      ix.Recovered = true;
      ix.Recover(cursor);
    }
    ix.Count(cursor);
    return ix;
  }

  // The entry at a page's offset ends by the next page's offset at the latest
  public long NextStart(long offset) =>
    _starts[Array.BinarySearch(_starts, offset) + 1];

  void ReadHeader(EntryCursor cursor) {
    if (!cursor.Read(0, out var entry, out _)) {
      throw new InvalidDataException("not a seven file: no header");
    }
    fixed (byte* e = entry) {
      var kind = Seven.Payload(e, entry.Length, out var p, out var end);
      if (kind != Seven.Kind.Header) {
        throw new InvalidDataException("not a seven file: no header");
      }
      ParseHeader(p, end);
    }
  }

  void ParseHeader(byte* p, byte* end) {
    var toc = new List<Dataset>();
    bool signed = false;
    while (p < end) {
      ulong tag = Seven.Varint(ref p, end);
      switch (tag) {
        case 0x0A:
          signed = Seven.Str(ref p, end) == Seven.HeaderSignature;
          break;
        case 0x10:
          if ((uint)Seven.Varint(ref p, end) > 1) {
            throw new InvalidDataException("seven of a version after 1");
          }
          break;
        case 0x20:
          VersionPatch = (uint)Seven.Varint(ref p, end);
          break;
        case 0x30:
          IndexStep = (int)Math.Min((uint)Seven.Varint(ref p, end), 1 << 30);
          break;
        case 0x40:
          uint bytes = (uint)Seven.Varint(ref p, end);
          if (bytes > Seven.MaxEntry) {
            throw new InvalidDataException($"pages of {bytes} bytes");
          }
          if (bytes != 0)
            BytesPerPage = (int)bytes;
          break;
        case 0x3A:
          toc.Add(ParseDataset(ref p, end, (uint)toc.Count));
          break;
        default:
          Seven.Skip(ref p, end, tag);
          break;
      }
    }
    if (!signed)
      throw new InvalidDataException("not a seven file");
    var names = new HashSet<string>(StringComparer.Ordinal);
    foreach (var ds in toc) {
      if (!names.Add(ds.Name)) {
        throw new InvalidDataException($"two datasets named '{ds.Name}'");
      }
      if (BytesPerPage % ds.ElemBytes != 0) {
        throw new InvalidDataException(
          $"pages of {BytesPerPage} bytes split values of '{ds.Name}'");
      }
      ds.PerPage = BytesPerPage / ds.ElemBytes;
    }
    ById = [.. toc];
  }

  static Dataset ParseDataset(ref byte* p, byte* end, uint id) {
    byte* dend = Seven.Len(ref p, end);
    string name = "";
    ulong type = 0;
    var size = new List<ulong>();
    while (p < dend) {
      ulong t = Seven.Varint(ref p, dend);
      if (t == 0x0A) {
        name = Seven.Str(ref p, dend);
      } else if (t == 0x10) {
        type = Seven.Varint(ref p, dend);
      } else if (t == 0x18) {
        size.Add(Seven.Varint(ref p, dend));
      } else if (t == 0x1A) {
        byte* send = Seven.Len(ref p, dend);
        while (p < send)
          size.Add(Seven.Varint(ref p, send));
      } else {
        Seven.Skip(ref p, dend, t);
      }
    }
    if (type > (ulong)SevenType.Double) {
      throw new InvalidDataException($"'{name}' of the unknown type {type}");
    }
    if (size.Count is < 1 or > 3) {
      throw new InvalidDataException($"'{name}' of rank {size.Count}");
    }
    var dims = new long[size.Count];
    for (int i = 1; i < dims.Length; i++) {
      if (size[i] > int.MaxValue) {
        throw new InvalidDataException($"'{name}' of dimension {size[i]}");
      }
      dims[i] = (long)size[i];
    }
    return new Dataset(name, id, (SevenType)type, dims);
  }

  // The epilogue and the global index it points at; false where native falls
  // back to the running indexes
  bool ReadGlobalIndex(EntryCursor cursor) {
    long length = cursor.Length;
    try {
      long epilogue = length / Seven.Block * Seven.Block, at;
      if (!cursor.Read(epilogue, out var entry, out _))
        return false;
      fixed (byte* e = entry) {
        var kind = Seven.Payload(e, entry.Length, out var p, out var end);
        if (kind != Seven.Kind.Epilogue)
          return false;
        at = ParseEpilogue(p, end);
      }
      if (at <= 0 || at >= length)
        return false;
      var found = new List<(uint Id, long Offset)>();
      var attributes = new Dictionary<string, string>(StringComparer.Ordinal);
      bool marker = false;
      for (long pos = at; ;) {
        if (!cursor.Read(pos, out entry, out long next))
          return false;
        fixed (byte* e = entry) {
          var kind = Seven.Payload(e, entry.Length, out var p, out var end);
          if (kind == Seven.Kind.Epilogue)
            break;
          if (kind != Seven.Kind.Index)
            return false;
          marker |= ParseIndex(p, end, found, attributes);
        }
        pos = next;
      }
      if (!marker)
        return false;
      Check(found, length);
      var pages = Group(found);
      if (!LastPagesBegin(cursor, pages))
        return false;
      Commit(pages, attributes, at);
      return true;
    } catch (InvalidDataException) {
      return false;
    }
  }

  // Each dataset's last page begins where the index says, one byte later
  // after a pad: a recorder closing amid an append lists one inside another
  // entry (README.md, *Decisions in force*)
  static bool LastPagesBegin(EntryCursor cursor, List<long>[] pages) {
    for (int id = 0; id < pages.Length; id++) {
      if (pages[id].Count == 0)
        continue;
      long offset = pages[id][^1];
      if (!TryEntry(cursor, offset, out var kind, out var message, out _) ||
          cursor.Begin > offset + 1 || kind != Seven.Kind.Data ||
          !TryDataId(message, out uint got) || got != id) {
        return false;
      }
    }
    return true;
  }

  static long ParseEpilogue(byte* p, byte* end) {
    long at = 0;
    bool signed = false;
    while (p < end) {
      ulong t = Seven.Varint(ref p, end);
      if (t == 0x08) {
        at = (long)Math.Min(Seven.Varint(ref p, end), long.MaxValue);
      } else if (t == 0x12) {
        signed = Seven.Str(ref p, end) == Seven.EpilogueSignature;
      } else {
        Seven.Skip(ref p, end, t);
      }
    }
    return signed ? at : -1;
  }

  // lib/d7's `recover full`: the running index every IndexStep blocks, then
  // each entry after the last. The index scan stops where native fails the
  // open or lists pages twice: README.md, *Decisions in force*.
  void Recover(EntryCursor cursor) {
    long length = cursor.Length;
    var pages = new List<long>[ById.Length];
    for (int i = 0; i < pages.Length; i++)
      pages[i] = [];
    var last = new long[ById.Length];
    Array.Fill(last, -1);
    var attributes = new Dictionary<string, string>(StringComparer.Ordinal);
    var found = new List<(uint Id, long Offset)>();
    var foundAttributes = new Dictionary<string, string>();
    long scan = 0;
    long blocks = IndexStep > 0 ? length / Seven.Block : 0;
    for (long block = IndexStep; block < blocks; block += IndexStep) {
      if (!TryEntry(cursor, block * Seven.Block,
                    out var kind, out var index, out long next))
        break;
      if (kind != Seven.Kind.Index &&
          !TryEntry(cursor, (block + 1) * Seven.Block,
                    out kind, out index, out next))
        break;
      if (kind != Seven.Kind.Index)
        break;
      found.Clear();
      foundAttributes.Clear();
      if (!TryParseIndex(index, found, foundAttributes))
        break;
      Check(found, length);
      if (!Running(found, last))
        break;
      foreach (var (id, offset) in found) {
        pages[id].Add(offset);
        last[id] = offset;
      }
      foreach (var (key, value) in foundAttributes)
        attributes[key] = value;
      scan = next;
    }
    long pos = scan, end = scan;
    while (TryEntry(cursor, pos, out var kind, out var message, out var next)) {
      if (kind == Seven.Kind.Data) {
        if (!TryDataId(message, out uint id) || id >= ById.Length)
          break;
        pages[id].Add(pos);
        end = next;
      } else if (kind == Seven.Kind.Index) {
        found.Clear();
        if (!TryParseIndex(message, found, foundAttributes))
          break;
      }
      pos = next;
    }
    Commit(pages, attributes, end);
  }

  // A running index lists pages written since the one before: each above
  // its dataset's last
  static bool Running(List<(uint Id, long Offset)> found, long[] last) {
    if (found.Count == 0)
      return false;
    var seen = (long[])last.Clone();
    foreach (var (id, offset) in found) {
      if (offset <= seen[id])
        return false;
      seen[id] = offset;
    }
    return true;
  }

  // The entry at or after offset and its payload; false where native's scan
  // stops: the file ends first, or the entry does not parse
  static bool TryEntry(EntryCursor cursor, long offset, out Seven.Kind kind,
                       out ReadOnlySpan<byte> message, out long next) {
    kind = Seven.Kind.None;
    message = default;
    next = offset;
    try {
      if (!cursor.Read(offset, out var entry, out next))
        return false;
      if (entry.Length == 0)
        return false;
      fixed (byte* e = entry) {
        kind = Seven.Payload(e, entry.Length, out var m, out var end);
        if (kind != Seven.Kind.None) {
          message = entry.Slice((int)(m - e), (int)(end - m));
        }
      }
      return true;
    } catch (InvalidDataException) {
      return false;
    }
  }

  static bool TryDataId(ReadOnlySpan<byte> message, out uint id) {
    id = 0;
    try {
      fixed (byte* m = message)
        id = DataView.Message(m, m + message.Length).Id;
      return true;
    } catch (InvalidDataException) {
      return false;
    }
  }

  static bool TryParseIndex(ReadOnlySpan<byte> message,
                            List<(uint Id, long Offset)> found,
                            Dictionary<string, string> attributes) {
    try {
      fixed (byte* m = message) {
        ParseIndex(m, m + message.Length, found, attributes);
      }
      return true;
    } catch (InvalidDataException) {
      return false;
    }
  }

  // One Index message: its pages in order and its attributes; true when it
  // carries the global index's marker
  internal static bool ParseIndex(
      byte* p, byte* end, List<(uint Id, long Offset)> found,
      Dictionary<string, string> attributes) {
    bool marker = false;
    while (p < end) {
      ulong t = Seven.Varint(ref p, end);
      if (t == 0x0A) {
        byte* dend = Seven.Len(ref p, end);
        uint id = 0;
        ulong offset = 0;
        while (p < dend) {
          ulong u = Seven.Varint(ref p, dend);
          if (u == 0x08)
            id = (uint)Seven.Varint(ref p, dend);
          else if (u == 0x10)
            offset = Seven.Varint(ref p, dend);
          else
            Seven.Skip(ref p, dend, u);
        }
        found.Add((id, (long)Math.Min(offset, long.MaxValue)));
      } else if (t == 0x12) {
        byte* mend = Seven.Len(ref p, end);
        string key = "", value = "";
        while (p < mend) {
          ulong u = Seven.Varint(ref p, mend);
          if (u == 0x0A)
            key = Seven.Str(ref p, mend);
          else if (u == 0x12)
            value = Seven.Str(ref p, mend);
          else
            Seven.Skip(ref p, mend, u);
        }
        if (key == Seven.GlobalIndexAttribute)
          marker = true;
        else
          attributes[key] = value;
      } else {
        Seven.Skip(ref p, end, t);
      }
    }
    return marker;
  }

  // What native's ProcessIndexEntry refuses, failing the open
  void Check(List<(uint Id, long Offset)> found, long length) {
    foreach (var (id, offset) in found) {
      if (id >= ById.Length) {
        throw new InvalidDataException($"a page of dataset {id}, unlisted");
      }
      if (offset >= length) {
        throw new InvalidDataException($"a page at {offset}, past the end");
      }
    }
  }

  List<long>[] Group(List<(uint Id, long Offset)> found) {
    var pages = new List<long>[ById.Length];
    for (int i = 0; i < pages.Length; i++)
      pages[i] = [];
    foreach (var (id, offset) in found)
      pages[id].Add(offset);
    return pages;
  }

  void Commit(List<long>[] pages, Dictionary<string, string> attributes,
              long end) {
    var starts = new List<long>();
    foreach (var list in pages)
      starts.AddRange(list);
    starts.Sort();
    for (int i = 1; i < starts.Count; i++) {
      if (starts[i] == starts[i - 1]) {
        throw new InvalidDataException($"two pages at {starts[i]}");
      }
    }
    if (starts.Count > 0 && starts[^1] >= end) {
      throw new InvalidDataException($"a page at {starts[^1]}, past the data");
    }
    starts.Add(end);
    for (int i = 0; i < ById.Length; i++)
      ById[i].Pages = [.. pages[i]];
    foreach (var (key, value) in attributes)
      Attributes[key] = value;
    _starts = [.. starts];
  }

  // Every page but the last is full, so the last one gives the count
  void Count(EntryCursor cursor) {
    var page = new byte[BytesPerPage];
    foreach (var ds in ById) {
      if (ds.Pages.Length == 0)
        continue;
      if (!cursor.Read(ds.Pages[^1], out var entry, out _)) {
        throw new InvalidDataException($"the last page of {ds.Name} is cut");
      }
      fixed (byte* e = entry)
      fixed (byte* d = page) {
        int n = Decode.Page(ds, e, entry.Length, d, ds.PerPage);
        ds.Elems = (long)(ds.Pages.Length - 1) * ds.PerPage + n;
      }
    }
  }
}
