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

public abstract class D7File : IDisposable {
  private protected D7File() { }

  private protected readonly Dictionary<string, string> _attributes = [];
  public IReadOnlyDictionary<string, string> Attributes => _attributes;

  private protected readonly Dictionary<string, D7DatasetInfo> _datasets = [];
  public IReadOnlyDictionary<string, D7DatasetInfo> Datasets => _datasets;

  public void Close() => Dispose();

  public void Dispose() {
    Dispose(disposing: true);
    GC.SuppressFinalize(this);
  }

  protected virtual void Dispose(bool disposing) { }
}
