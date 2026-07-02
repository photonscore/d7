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

#ifndef PHOTONSCORE_LIB_D7_BACKEND_SEVEN_SEVEN_VAULT_H
#define PHOTONSCORE_LIB_D7_BACKEND_SEVEN_SEVEN_VAULT_H

#include <string>

namespace photonscore {
namespace d7 {
namespace backend {
namespace seven {

struct SevenVault {
  enum Consts {
    // Default data chunk size in bytes
    kBytesPerPage = 16 * 1024,

    kVersionMajor = 1,
    kVersionMinor = 0,
    kVersionPatch = 2,

    kIndexStep = 512,
  };

  static const std::string kHeaderSignature;
  static const std::string kEpilogueSignature;
  static const std::string kGlobalIndexAttribute;
};

}  // namespace seven
}  // namespace backend
}  // namespace d7
}  // namespace photonscore

#endif  // PHOTONSCORE_LIB_D7_BACKEND_SEVEN_SEVEN_VAULT_H
