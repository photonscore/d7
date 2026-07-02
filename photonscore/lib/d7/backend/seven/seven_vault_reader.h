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

#ifndef PHOTONSCORE_LIB_D7_BACKEND_SEVEN_SEVEN_VAULT_READER_H
#define PHOTONSCORE_LIB_D7_BACKEND_SEVEN_SEVEN_VAULT_READER_H

#include "photonscore/lib/d7/backend/seven/log_reader.h"
#include "photonscore/lib/d7/backend/seven/seven.pb.h"
#include "photonscore/lib/d7/backend/seven/vault_decoder.h"
#include "photonscore/lib/d7/d7.h"

#include <map>
#include <memory>
#include <string>

namespace photonscore {
namespace d7 {
namespace backend {
namespace seven {

class SevenVaultReader : public Vault {
 public:
  SevenVaultReader(const std::string& path, const OpenOptions& options);
  virtual ~SevenVaultReader();

  SevenVaultReader(const SevenVaultReader&) = delete;
  SevenVaultReader& operator=(const SevenVaultReader&) = delete;

  Status Init();

  virtual Status Read(const std::string& dataset, uint64_t offset, size_t items,
                      void* buffer) override;
  virtual Status Read(const std::string& dataset, uint64_t offset, size_t items,
                      void* buffer) const override;
  virtual Status attribute(const std::string& name,
                           std::string* value) override;

 private:
  std::unique_ptr<LogReader> log_reader_;
  std::map<std::string, std::unique_ptr<VaultDecoder>> decoders_;
  std::map<std::string, std::string> attributes_;

  Status ReadEntry(int64_t offset, bool is_block, FileEntry* entry,
                   int64_t* new_offset) const;

  Status ReadEntryAtBlock(int64_t block, FileEntry* entry,
                          int64_t* new_offset = nullptr) const {
    return ReadEntry(block, true, entry, new_offset);
  }

  Status ReadEntryAtByte(int64_t byte, FileEntry* entry,
                         int64_t* new_offset = nullptr) const {
    return ReadEntry(byte, false, entry, new_offset);
  }

  Status InitDatasets(size_t* index_step);
  Status InitIndex(size_t index_step);
  Status InitIndexGlobal();
  Status InitIndexScatter(size_t index_step);

  std::vector<VaultDecoder*> DecodersById();
  Status ProcessIndexEntry(const FileEntry& file_entry,
                           std::vector<VaultDecoder*>* decoders);
};

}  // namespace seven
}  // namespace backend
}  // namespace d7
}  // namespace photonscore

#endif  // PHOTONSCORE_LIB_D7_BACKEND_SEVEN_SEVEN_VAULT_READER_H
