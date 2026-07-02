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

#include "photonscore/lib/d7/vault.h"

namespace photonscore {
namespace d7 {

Vault::Vault(const std::string& path, const std::string& backend,
             const OpenOptions& options)
    : closed_(false),
      path_(path),
      backend_name_(backend),
      open_options_(options) {}

Vault::~Vault() {
  if (!closed_) {
    Close();
  }
}

Status Vault::Close() {
  closed_ = true;
  return Status::OK();
}

Status Vault::Flush() {
  // By default pretend everything is just fine.
  return Status::OK();
}

Status Vault::CreateDataset(const DatasetInfo&) {
  return Status::Unimplemented("Abstract Vault::CreateDataset()");
}

Status Vault::Append(const std::string&, size_t, const void*) {
  return Status::Unimplemented("Abstract Vault::Append()");
}

Status Vault::Read(const std::string&, uint64_t, size_t, void*) {
  return Status::Unimplemented("Abstract Vault::Read()");
}

Status Vault::Read(const std::string&, uint64_t, size_t, void*) const {
  return Status::Unimplemented("Abstract Vault::Read() const");
}

Status Vault::set_attribute(const std::string&, const std::string&) {
  return Status::Unimplemented("Abstract Vault::set_attribute()");
}

Status Vault::attribute(const std::string&, std::string*) {
  return Status::Unimplemented("Abstract Vault::attribute()");
}

}  // namespace d7
}  // namespace photonscore
