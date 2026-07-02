/*
    Copyright (c) 2026 Photonscore GmbH

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#ifndef PHOTONSCORE_LIB_D7_OPEN_OPTIONS_H
#define PHOTONSCORE_LIB_D7_OPEN_OPTIONS_H

#include <string>

namespace photonscore {
namespace d7 {

struct OpenOptions {
  bool fail_if_exists;          // false
  bool create_if_missing;       // false
  bool read_only;               // true
  size_t read_threads;          // 4
  std::string backend;          // Empty by default.
  std::string backend_options;  // Backend specific settings

  OpenOptions();

  static OpenOptions ForWriting();
  static OpenOptions ForReading();
};

}  // namespace d7
}  // namespace photonscore

#endif  // PHOTONSCORE_LIB_D7_OPEN_OPTIONS_H
