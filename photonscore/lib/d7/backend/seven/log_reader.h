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

#ifndef PHOTONSCORE_LIB_D7_BACKEND_SEVEN_LOG_READER_H
#define PHOTONSCORE_LIB_D7_BACKEND_SEVEN_LOG_READER_H

#include "photonscore/lib/mmap/mmap.h"

#include <cstdint>
#include <cstdio>
#include <memory>

namespace photonscore {
namespace d7 {
namespace backend {
namespace seven {

class LogReader {
 public:
  LogReader();
  virtual ~LogReader();

  Status Open(const std::string& filename);
  Status Close();
  Status Read(int64_t byte_offset, std::string* buffer,
              int64_t* new_offset = nullptr) const;
  Status ReadBlock(int64_t block_offset, std::string* buffer,
                   int64_t* new_offset = nullptr) const;

  int64_t file_size_in_bytes() const { return mm_->size(); }
  size_t file_size_in_blocks() const;

 private:
  LogReader(const LogReader&) = delete;
  LogReader& operator=(const LogReader&) = delete;

  std::unique_ptr<mmap::MemoryMappedFile> mm_;
};

}  // namespace seven
}  // namespace backend
}  // namespace d7
}  // namespace photonscore

#endif  // PHOTONSCORE_LIB_D7_BACKEND_SEVEN_LOG_READER_H
