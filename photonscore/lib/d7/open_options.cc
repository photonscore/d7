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

#include "photonscore/lib/d7/open_options.h"

namespace photonscore {
namespace d7 {

OpenOptions::OpenOptions()
    : fail_if_exists(false),
      create_if_missing(false),
      read_only(true),
      read_threads(4) {}

OpenOptions OpenOptions::ForReading() {
  OpenOptions oo;
  oo.fail_if_exists = false;
  oo.create_if_missing = false;
  oo.read_only = true;
  return oo;
}

OpenOptions OpenOptions::ForWriting() {
  OpenOptions oo;
  oo.fail_if_exists = true;
  oo.create_if_missing = true;
  oo.read_only = false;
  return oo;
}

}  // namespace d7
}  // namespace photonscore
