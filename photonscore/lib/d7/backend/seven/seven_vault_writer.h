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

#ifndef PHOTONSCORE_LIB_D7_BACKEND_SEVEN_SEVEN_VAULT_WRITER_H
#define PHOTONSCORE_LIB_D7_BACKEND_SEVEN_SEVEN_VAULT_WRITER_H

#include "photonscore/lib/d7/backend/seven/log_appender.h"
#include "photonscore/lib/d7/backend/seven/seven.pb.h"
#include "photonscore/lib/d7/backend/seven/vault_encoder.h"
#include "photonscore/lib/d7/d7.h"

#include <map>
#include <memory>
#include <string>

namespace photonscore {
namespace d7 {
namespace backend {
namespace seven {

class SevenVaultWriter : public Vault {
 public:
  SevenVaultWriter(const std::string& path, const OpenOptions& options);
  virtual ~SevenVaultWriter();

  SevenVaultWriter(const SevenVaultWriter&) = delete;
  SevenVaultWriter& operator=(const SevenVaultWriter&) = delete;

  Status Init();

  Status Close() override;
  Status Flush() override;

  virtual Status CreateDataset(const DatasetInfo& dataset) override;
  virtual Status Append(const std::string& dataset, size_t items,
                        const void* buffer) override;
  virtual Status set_attribute(const std::string& name,
                               const std::string& value) override;
  virtual Status attribute(const std::string& name,
                           std::string* value) override;

 private:
  FileEntry header_;
  bool header_written_;

  FileEntry running_index_;
  std::unique_ptr<LogAppender> log_appender_;
  std::map<std::string, std::unique_ptr<VaultEncoder>> encoders_;

  FileEntry global_index_;

  Status EnsureHeaderWritten();
  Status FlushIndex(const VaultEncoder* encoder, int64_t bytes_before,
                    int64_t blocks_before, int64_t bytes_after,
                    int64_t blocks_after);
  Status Append(VaultEncoder* encoder, const char* ptr, size_t bytes);
  Status AppendParallel(VaultEncoder* encoder, const char* ptr, size_t pages);
};

}  // namespace seven
}  // namespace backend
}  // namespace d7
}  // namespace photonscore

#endif  // PHOTONSCORE_LIB_D7_BACKEND_SEVEN_SEVEN_VAULT_WRITER_H
