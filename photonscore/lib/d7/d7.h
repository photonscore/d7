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

#ifndef PHOTONSCORE_LIB_D7_D7_H
#define PHOTONSCORE_LIB_D7_D7_H

#include "photonscore/lib/d7/open_options.h"
#include "photonscore/lib/d7/vault.h"

namespace photonscore {
namespace d7 {

namespace internal_ {
Status OpenImpl(const std::string& path, const OpenOptions& options,
                      Vault** vault);
}

Status Open(const std::string& path, const OpenOptions& options,
                  Vault** vault);
Status Open(const std::string& path, const OpenOptions& options,
                  std::unique_ptr<Vault>* vault);

}  // namespace d7
}  // namespace photonscore

#endif  // PHOTONSCORE_LIB_D7_D7_H
