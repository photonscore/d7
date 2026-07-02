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

#ifndef PHOTONSCORE_LIB_D7_BACKEND_SEVEN_WRITABLE_FILE_BATCH_H
#define PHOTONSCORE_LIB_D7_BACKEND_SEVEN_WRITABLE_FILE_BATCH_H

#include "photonscore/lib/d7/backend/seven/writable_file.h"

#include <string>

namespace photonscore {
namespace d7 {
namespace backend {
namespace seven {

class WritableFileBatch : public WritableFile {
 public:
  WritableFileBatch(WritableFile* target, size_t expected_batch);

  virtual ~WritableFileBatch();

  virtual Status Close() final;
  virtual Status Write(const base::Slice& data) final;
  virtual Status Flush() final;
  virtual int64_t file_size_in_bytes() final;

 public:
  WritableFile* target_;
  int64_t target_size_;
  std::string buffer_;
};

}  // namespace seven
}  // namespace backend
}  // namespace d7
}  // namespace photonscore

#endif  // PHOTONSCORE_LIB_D7_BACKEND_SEVEN_WRITABLE_FILE_BATCH_H
