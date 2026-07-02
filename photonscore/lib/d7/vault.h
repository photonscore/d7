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

#ifndef PHOTONSCORE_LIB_D7_VAULT_H
#define PHOTONSCORE_LIB_D7_VAULT_H

#include "photonscore/lib/base/status.h"
#include "photonscore/lib/d7/dataset_info.h"
#include "photonscore/lib/d7/open_options.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace photonscore {
namespace d7 {

class Vault {
 public:
  virtual ~Vault();

  virtual Status Close();
  virtual Status Flush();

  // Data access.
  virtual Status CreateDataset(const DatasetInfo& dataset);
  virtual Status Append(const std::string& dataset, size_t items,
                        const void* buffer);
  virtual Status Read(const std::string& dataset, uint64_t offset, size_t items,
                      void* buffer);
  virtual Status Read(const std::string& dataset, uint64_t offset, size_t items,
                      void* buffer) const;

  // Attributes.
  virtual Status set_attribute(const std::string& name,
                               const std::string& value);
  virtual Status attribute(const std::string& name, std::string* value);

  // Common vault properties.
  const std::vector<std::string>& attribute_names() const {
    return attribute_names_;
  }

  const std::string& backend_name() const { return backend_name_; }

  const std::map<std::string, DatasetInfo>& datasets_info() const {
    return datasets_info_;
  }

  const DatasetInfo dataset_info(const std::string& name) const {
    if (datasets_info_.count(name) == 0) return DatasetInfo();
    return datasets_info_.at(name);
  }

  const std::string& path() const { return path_; }

  bool read_only() const { return open_options_.read_only; }
  const OpenOptions& open_options() { return open_options_; }

 protected:
  Vault(const std::string& path, const std::string& backend,
        const OpenOptions& options);

  bool closed_;
  std::string path_;
  std::string backend_name_;
  OpenOptions open_options_;
  std::map<std::string, DatasetInfo> datasets_info_;
  std::vector<std::string> attribute_names_;

 private:
  Vault() = delete;
  Vault(const Vault&) = delete;
  void operator=(const Vault&) = delete;
};

}  // namespace d7
}  // namespace photonscore

#endif  // PHOTONSCORE_LIB_D7_VAULT_H
