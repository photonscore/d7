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

namespace Photonscore.D7;

// One dataset of the header's table of contents, and its pages
internal sealed class Dataset {
  public readonly string Name;
  public readonly uint Id;
  public readonly SevenType Type;
  public readonly long[] Dims;
  public readonly int ElemBytes;
  public readonly long ItemElems;
  public int PerPage;
  public long[] Pages = [];
  public long Elems;

  public Dataset(string name, uint id, SevenType type, long[] dims) {
    Name = name;
    Id = id;
    Type = type;
    Dims = dims;
    ElemBytes = type switch {
      SevenType.Int8 or SevenType.UInt8 => 1,
      SevenType.Int16 or SevenType.UInt16 => 2,
      SevenType.Int32 or SevenType.UInt32 or SevenType.Single => 4,
      _ => 8,
    };
    ItemElems = 1;
    for (int i = 1; i < dims.Length; i++) {
      ItemElems *= dims[i];
      if (ItemElems > int.MaxValue) {
        throw new InvalidDataException($"items of '{name}' too large");
      }
    }
  }

  public long Items => ItemElems == 0 ? 0 : Elems / ItemElems;

  public Type ClrType => Type switch {
    SevenType.Int8 => typeof(sbyte),
    SevenType.Int16 => typeof(short),
    SevenType.Int32 => typeof(int),
    SevenType.Int64 => typeof(long),
    SevenType.UInt8 => typeof(byte),
    SevenType.UInt16 => typeof(ushort),
    SevenType.UInt32 => typeof(uint),
    SevenType.UInt64 => typeof(ulong),
    SevenType.Single => typeof(float),
    _ => typeof(double),
  };

  public D7DatasetInfo Info() =>
    new() { Name = Name, Type = ClrType, Dims = [Items, .. Dims[1..]] };
}
