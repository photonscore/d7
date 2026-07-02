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

#ifndef PHOTONSCORE_LIB_D7_BACKEND_SEVEN_LOG_APPENDER_H
#define PHOTONSCORE_LIB_D7_BACKEND_SEVEN_LOG_APPENDER_H

#include "photonscore/lib/d7/backend/seven/writable_file.h"
#include "photonscore/lib/d7/backend/seven/writable_file_batch.h"

#include <memory>

namespace photonscore {
namespace d7 {
namespace backend {
namespace seven {

class LogAppender {
 public:
  LogAppender(std::unique_ptr<WritableFile>&& destination);
  virtual ~LogAppender();

  LogAppender(const LogAppender&) = delete;
  LogAppender& operator=(const LogAppender&) = delete;

  Status Close();
  Status Flush();
  Status Append(const base::Slice& slice);
  Status PadToBlock();

  int64_t file_size_in_bytes();
  size_t file_size_in_blocks();

  Status BeginBatch(size_t expected_batch_size);
  Status CommitBatch();

 private:
  WritableFile* file() const { return batch_ ? batch_ : file_.get(); }

  // Offset inside 16K block
  int offset_;

  WritableFileBatch* batch_;
  std::unique_ptr<WritableFile> file_;
};

}  // namespace seven
}  // namespace backend
}  // namespace d7
}  // namespace photonscore

#endif  // PHOTONSCORE_LIB_D7_BACKEND_SEVEN_LOG_APPENDER_H
