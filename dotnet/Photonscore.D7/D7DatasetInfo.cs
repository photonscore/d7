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

using System.Diagnostics;
using System.Runtime.InteropServices;

namespace Photonscore.D7;

[DebuggerDisplay("{DebuggerDisplay,nq}")]
public class D7DatasetInfo {
  public string Name = "";
  public long Items => Dims[0];
  public int Rank => Dims.Length;
  public Type Type = typeof(void);
  public long[] Dims = [];

  public long ItemSize => Rank switch {
    1 => 1,
    2 => Dims[1],
    _ => Dims[1] * Dims[2],
  };

  public int ItemBytes => (int)(ItemSize * Marshal.SizeOf(Type));

  private string DebuggerDisplay => Rank switch {
    1 => $"{Name}: {Type.Name}[{Dims[0]}]",
    2 => $"{Name}: {Type.Name}[{Dims[0]}, {Dims[1]}]",
    _ => $"{Name}: {Type.Name}[{Dims[0]}, {Dims[1]}, {Dims[2]}]",
  };
}
