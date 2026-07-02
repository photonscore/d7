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

#ifndef PHOTONSCORE_LIB_D7_BACKEND_SEVEN_VAULT_DECODER_H
#define PHOTONSCORE_LIB_D7_BACKEND_SEVEN_VAULT_DECODER_H

#include "photonscore/lib/base/type_id.h"
#include "photonscore/lib/d7/backend/seven/log_reader.h"

#include <vector>

namespace photonscore {
namespace d7 {
namespace backend {
namespace seven {

class VaultDecoder {
 public:
  VaultDecoder(size_t bytes_per_page, LogReader* log_reader,
               uint32_t dataset_id, photonscore::base::TypeId type_id);
  ~VaultDecoder();

  Status Init();
  Status Decode(int64_t offset, size_t bytes, void* buffer) const;

  void AddPageOffset(int64_t page_offset) {
    // Append the page_offset to the list of pages that belongs to this dataset.
    // NOTE: This must NOT be called after Init().
    pages_offsets_.push_back(page_offset);
  }

  uint32_t dataset_id() const { return dataset_id_; }
  int64_t total_bytes() const { return total_bytes_; }

 private:
  uint64_t total_bytes_;
  size_t bytes_per_page_;
  LogReader* log_reader_;
  uint32_t dataset_id_;
  photonscore::base::TypeId type_id_;
  std::vector<int64_t> pages_offsets_;

  Status ReadRawEntry(size_t entry_number, std::string* raw_entry) const;
  Status DecodePage(int64_t entry_number, const std::string& raw_entry,
                    std::string* page) const;
};

}  // namespace seven
}  // namespace backend
}  // namespace d7
}  // namespace photonscore

#endif  // PHOTONSCORE_LIB_D7_BACKEND_SEVEN_VAULT_DECODER_H
