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

#ifndef PHOTONSCORE_LIB_D7_BACKEND_SEVEN_VAULT_ENCODER_H
#define PHOTONSCORE_LIB_D7_BACKEND_SEVEN_VAULT_ENCODER_H

#include "photonscore/lib/base/type_id.h"
#include "photonscore/lib/d7/backend/seven/log_appender.h"
#include "photonscore/lib/d7/backend/seven/paged_appender.h"

#include <cstdint>

namespace photonscore {
namespace d7 {
namespace backend {
namespace seven {

class VaultEncoder {
 public:
  VaultEncoder(size_t bytes_per_page, LogAppender* log_appender,
               uint32_t dataset_id, photonscore::base::TypeId type_id);
  ~VaultEncoder();

  Status Init();
  Status Close();
  Status Encode(const void* data, size_t len);
  Status AppendPage(const std::string& page);
  std::string SerializeChunk(const char* page, size_t n) const;

  size_t bytes_per_page() const { return bytes_per_page_; }
  uint32_t dataset_id() const { return dataset_id_; }
  int64_t total_bytes() const { return paged_appender_.total_bytes(); }

  void StepPage() { paged_appender_.StepPage(); }

 private:
  size_t bytes_per_page_;
  LogAppender* log_appender_;
  uint32_t dataset_id_;
  photonscore::base::TypeId type_id_;
  PagedAppender<VaultEncoder> paged_appender_;
};

}  // namespace seven
}  // namespace backend
}  // namespace d7
}  // namespace photonscore

#endif  // PHOTONSCORE_LIB_D7_BACKEND_SEVEN_VAULT_ENCODER_H
