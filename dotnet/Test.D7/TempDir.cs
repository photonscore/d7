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

namespace Test;

// A test's temp folder goes when the test does, if it can: on Windows a
// sidecar or a spawned child can hold a file there a moment longer, and a
// leftover folder in %TEMP% is no reason to fail a test.
public static class TempDir {
  // A folder for a Wood runtime folder: short, since macOS caps a Unix
  // socket path at 104 bytes and its temp path takes 49 of them
  public static string Short(string prefix) => Path.Combine(
    Path.GetTempPath(), $"{prefix}-{Guid.NewGuid():N}"[..(prefix.Length + 9)]);

  public static void Delete(DirectoryInfo dir) => Delete(dir.FullName);

  public static void Delete(string dir) {
    for (var attempt = 0; attempt < 3; attempt++) {
      try {
        Directory.Delete(dir, recursive: true);
        return;
      } catch (DirectoryNotFoundException) {
        return;
      } catch (Exception e) when (e is IOException or UnauthorizedAccessException) {
        Thread.Sleep(100);
      }
    }
  }
}
