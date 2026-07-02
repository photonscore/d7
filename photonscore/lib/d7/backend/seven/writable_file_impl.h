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

#ifndef PHOTONSCORE_LIB_D7_BACKEND_SEVEN_WRITABLE_FILE_IMPL_H
#define PHOTONSCORE_LIB_D7_BACKEND_SEVEN_WRITABLE_FILE_IMPL_H

#include "photonscore/lib/d7/backend/seven/writable_file.h"

#include <cstdio>
#include <memory>

namespace photonscore {
namespace d7 {
namespace backend {
namespace seven {

class WritableFileImpl : public WritableFile {
 public:
  static Status CreateNew(const std::string& filename, WritableFile** file);

  static Status CreateNew(const std::string& filename,
                          std::unique_ptr<WritableFile>* file);

  virtual ~WritableFileImpl();
  Status Close() override;
  Status Write(const base::Slice& data) override;
  Status Flush() override;
  int64_t file_size_in_bytes() override;

  WritableFileImpl(const WritableFileImpl&) = delete;
  WritableFileImpl& operator=(const WritableFileImpl&) = delete;

 private:
  WritableFileImpl();

  FILE* file_;
  int64_t file_size_in_bytes_;
};

}  // namespace seven
}  // namespace backend
}  // namespace d7
}  // namespace photonscore

#endif  // PHOTONSCORE_LIB_D7_BACKEND_SEVEN_WRITABLE_FILE_IMPL_H
