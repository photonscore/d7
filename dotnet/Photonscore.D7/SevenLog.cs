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
using System.Runtime.ExceptionServices;

namespace Photonscore.D7;

// lib/d7's LogAppender over 4 MB buffers, each full one written by a pool
// thread while the next fills. A failed write is final (README.md, *Writing*).
internal sealed class SevenLog : IDisposable {
  const int BufferBytes = 4 << 20;

  readonly SafeFileHandle _file;
  byte[] _fill = ArrayPool<byte>.Shared.Rent(BufferBytes);
  byte[]? _spare;
  int _filled;
  long _handed; // bytes handed to the file
  long _size; // those and the ones filled: where the next entry begins
  int _inBlock;
  Task _pending = Task.CompletedTask;
  ExceptionDispatchInfo? _failure;

  // Tests: called with the offset before each write reaches the file
  internal Action<long>? BeforeWrite;

  public SevenLog(SafeFileHandle file) => _file = file;

  public long Size => _size;

  public bool Failed => _failure != null || _pending.IsFaulted;

  // An entry in fragments, none crossing a block; a block with one byte
  // left is padded, one with two gets an empty begin fragment
  public void Append(ReadOnlySpan<byte> entry) {
    Span<byte> header = stackalloc byte[2];
    bool begin = true;
    do {
      int left = Seven.Block - _inBlock;
      if (left < 2) {
        header.Clear();
        Put(header[..left]);
        _inBlock = 0;
      }
      int fragment = Math.Min(entry.Length, Seven.Block - _inBlock - 2);
      bool end = fragment == entry.Length;
      int h = fragment | (begin ? Seven.FragmentBegin : 0) |
              (end ? Seven.FragmentEnd : 0);
      header[0] = (byte)h;
      header[1] = (byte)(h >> 8);
      Put(header);
      Put(entry[..fragment]);
      entry = entry[fragment..];
      _inBlock = (_inBlock + 2 + fragment) & Seven.BlockMask;
      begin = false;
    } while (!entry.IsEmpty);
  }

  public void PadToBlock() {
    Span<byte> zeros = stackalloc byte[256];
    zeros.Clear();
    while (_inBlock != 0) {
      int n = Math.Min(zeros.Length, Seven.Block - _inBlock);
      Put(zeros[..n]);
      _inBlock = (_inBlock + n) & Seven.BlockMask;
    }
  }

  // What is filled to the file, and every write finished
  public void Finish() {
    Wait();
    try {
      if (_filled > 0)
        Write(_fill, _filled, _handed);
    } catch (Exception e) {
      _failure = ExceptionDispatchInfo.Capture(e);
      throw;
    }
    _handed += _filled;
    _filled = 0;
  }

  public void ThrowIfFailed() {
    if (_failure == null && _pending.IsFaulted)
      Wait();
    _failure?.Throw();
  }

  public void Dispose() {
    try {
      _pending.Wait();
    } catch {
      // Final already, and seen by whoever made the next call
    }
    ArrayPool<byte>.Shared.Return(_fill);
    if (_spare != null)
      ArrayPool<byte>.Shared.Return(_spare);
    _file.Dispose();
  }

  void Put(ReadOnlySpan<byte> bytes) {
    while (!bytes.IsEmpty) {
      int n = Math.Min(bytes.Length, _fill.Length - _filled);
      bytes[..n].CopyTo(_fill.AsSpan(_filled));
      _filled += n;
      _size += n;
      bytes = bytes[n..];
      if (_filled == _fill.Length)
        HandOff();
    }
  }

  void HandOff() {
    Wait();
    var full = _fill;
    int length = _filled;
    long at = _handed;
    _handed += length;
    _fill = _spare ?? ArrayPool<byte>.Shared.Rent(BufferBytes);
    _spare = full;
    _filled = 0;
    _pending = Task.Run(() => Write(full, length, at));
  }

  void Write(byte[] buffer, int length, long at) {
    BeforeWrite?.Invoke(at);
    RandomAccess.Write(_file, buffer.AsSpan(0, length), at);
  }

  void Wait() {
    _failure?.Throw();
    try {
      _pending.GetAwaiter().GetResult();
    } catch (Exception e) {
      _failure = ExceptionDispatchInfo.Capture(e);
      throw;
    }
  }
}
