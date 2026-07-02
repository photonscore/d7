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
#include "photonscore/lib/d7/d7.h"
#include "photonscore/lib/fs/fs.h"

namespace photonscore {
namespace d7 {

namespace internal_ {
Status OpenImpl(const std::string& path, const OpenOptions& options,
                Vault** vault) {
  Status status;

  if (options.read_only && options.create_if_missing)
    return Status::InvalidArgument("cannot create in read-only mode");

  bool file_exists = fs::Exists(path);

  if (file_exists && options.fail_if_exists) {
    return Status::AlreadyExists("Already exists '{0}'", path);
  }

  if (!file_exists && options.read_only) {
    return Status::NotFound("No such file '{0}'", path);
  }

  if (options.create_if_missing && file_exists) {
    status = fs::Remove(path);
    if (!status.ok()) return status;
  }

  bool should_create = options.create_if_missing && !file_exists;

  if (options.backend.size() > 0) {
    if (options.backend == "seven") {
      return backend::OpenSeven(path, vault, options);
    }
  } else {
    status = backend::OpenSeven(path, vault, options);
    if (status.ok() || (should_create && !status.ok())) return status;
  }

  return Status::Unimplemented("Unsupported file format");
}
}  // namespace internal_

Status Open(const std::string& path, const OpenOptions& options,
            Vault** vault) {
  Vault* v = nullptr;
  Status status = internal_::OpenImpl(path, options, &v);
  if (!status.ok()) return status;
  *vault = v;

  return Status::OK();
}

Status Open(const std::string& path, const OpenOptions& options,
            std::unique_ptr<Vault>* vault) {
  Vault* ptr;
  auto s = Open(path, options, &ptr);
  if (s.ok()) vault->reset(ptr);
  return s;
}

}  // namespace d7
}  // namespace photonscore
