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
namespace Test.D7;

// net8.0 has no Convert.ToHexStringLower; the tests hash with it.
static class ConvertPolyfill {
  extension(Convert) {
    public static string ToHexStringLower(byte[] bytes) =>
      Convert.ToHexString(bytes).ToLowerInvariant();
  }
}
#endif
