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

#if !NET9_0_OR_GREATER
namespace System.Threading;

// net8.0 has no Lock: the compiler's lock statement on it, over Monitor.
internal sealed class Lock {
  readonly object _monitor = new();

  public Scope EnterScope() {
    Monitor.Enter(_monitor);
    return new Scope(_monitor);
  }

  public ref struct Scope(object monitor) {
    public void Dispose() => Monitor.Exit(monitor);
  }
}
#endif
