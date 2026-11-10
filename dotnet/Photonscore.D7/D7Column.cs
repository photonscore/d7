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

namespace Photonscore.D7;

// A dataset and the buffer its items go to, for reading several in one pass
public abstract class D7Column {
  private protected D7Column(string dataset) => Dataset = dataset;

  public string Dataset { get; }

  internal abstract Type ElementType { get; }
  internal abstract long Length { get; }
  internal abstract MemoryHandle Pin();

  public static D7Column Of<T>(string dataset, T[] buffer)
      where T : unmanaged => new Column<T>(dataset, buffer);

  public static D7Column Of<T>(string dataset, Memory<T> buffer)
      where T : unmanaged => new Column<T>(dataset, buffer);

  private sealed class Column<T>(string dataset, Memory<T> buffer)
      : D7Column(dataset) where T : unmanaged {
    internal override Type ElementType => typeof(T);
    internal override long Length => buffer.Length;
    internal override MemoryHandle Pin() => buffer.Pin();
  }
}
