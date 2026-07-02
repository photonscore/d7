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

#include "photonscore/lib/d7/backend/seven.h"
#include "photonscore/lib/d7/backend/seven/seven_vault_reader.h"
#include "photonscore/lib/d7/backend/seven/seven_vault_writer.h"
#include "photonscore/lib/fs/fs.h"

using photonscore::d7::backend::seven::SevenVaultReader;
using photonscore::d7::backend::seven::SevenVaultWriter;

namespace photonscore {
namespace d7 {
namespace backend {

Status OpenSeven(const std::string& path, Vault** vault_p,
                 const OpenOptions& options) {
  bool exists = fs::Exists(path);

  Vault* vault = nullptr;
  Status status;

  if (options.create_if_missing && !exists) {
    auto writer = new SevenVaultWriter(path, options);
    status = writer->Init();
    vault = writer;
  }

  if (options.read_only && exists) {
    auto reader = new SevenVaultReader(path, options);
    status = reader->Init();
    vault = reader;
  }

  if (vault != nullptr) {
    if (!status.ok()) {
      delete vault;
    } else {
      *vault_p = vault;
    }
    return status;
  }

  return Status::InvalidArgument("Cannot open");
}

}  // namespace backend
}  // namespace d7
}  // namespace photonscore
