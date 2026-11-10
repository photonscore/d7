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

// Entries by file offset over positional reads, for the header, the indexes
// and the recovery scan
internal sealed unsafe class EntryCursor(SafeFileHandle file, long length) {
  long _length = length;
  byte[] _io = new byte[64 << 10];
  long _base;
  int _count;
  byte[] _entry = new byte[64 << 10];

  public long Length => _length;

  // Where the entry last read begins
  public long Begin { get; private set; }

  // The entry whose begin fragment lies at or after offset, good until the
  // next call; false when the file ends first
  public bool Read(long offset, out ReadOnlySpan<byte> entry, out long next) {
    entry = default;
    next = offset;
    if (offset >= _length)
      return false;
    if (offset < _base || offset >= _base + _count)
      Fill(offset);
    while (true) {
      fixed (byte* io = _io) {
        int len = Seven.ReadEntry(io, _count, _base, offset, ref _entry,
                                  out var data, out next, out long begin);
        Begin = begin;
        if (len >= 0) {
          entry = data != null
            ? new ReadOnlySpan<byte>(_io, (int)(data - io), len)
            : new ReadOnlySpan<byte>(_entry, 0, len);
          return true;
        }
      }
      if (_base + _count >= _length)
        return false;
      if (next == _base)
        Array.Resize(ref _io, _io.Length * 2);
      offset = next;
      Fill(offset);
    }
  }

  void Fill(long offset) {
    _base = offset;
    int n = (int)Math.Min(_io.Length, _length - offset);
    _count = Seven.ReadAt(file, _io.AsSpan(0, n), offset);
    if (_count < n)
      _length = offset + _count;
  }
}
