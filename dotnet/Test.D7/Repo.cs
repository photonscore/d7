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

namespace Zoo;

// Stands in for the monorepo's Repo, so the tests keep their paths as is.
internal static class Repo {
  public static string Root { get; } = Find();

  static string Find() {
    for (var d = new DirectoryInfo(AppContext.BaseDirectory); d != null;
         d = d.Parent) {
      if (File.Exists(Path.Combine(d.FullName, "configure.py"))) {
        return d.FullName;
      }
    }
    throw new InvalidOperationException(
      "d7's root (configure.py) is not above " + AppContext.BaseDirectory);
  }
}
