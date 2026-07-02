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

#include "photonscore/lib/d7/backend/seven/writable_file_impl.h"
#include "photonscore/lib/fs/fs.h"

#include <cassert>

using photonscore::base::Slice;

namespace photonscore {
namespace d7 {
namespace backend {
namespace seven {

Status WritableFileImpl::CreateNew(const std::string& filename,
                                   WritableFile** file) {
  if (fs::Exists(filename)) {
    return Status::AlreadyExists(
        "WritableFile cannot change existing file '{0}'", filename);
  }

  FILE* f = fs::OpenFile(filename.c_str(), "wb");
  if (f == nullptr) {
    return Status::IoError("Cannot create file '{0}'", filename);
  }

  auto wf = new WritableFileImpl();
  wf->file_ = f;
  wf->file_size_in_bytes_ = 0;
  *file = wf;
  return Status::OK();
}

Status WritableFileImpl::CreateNew(const std::string& filename,
                                   std::unique_ptr<WritableFile>* file) {
  WritableFile* wf = nullptr;
  auto status = CreateNew(filename, &wf);
  file->reset(wf);
  return status;
}

WritableFileImpl::WritableFileImpl()
    : file_(nullptr), file_size_in_bytes_(-1) {}

WritableFileImpl::~WritableFileImpl() {
  auto status = Close();
  assert(status.ok() && "Close() fails in destructor");
}

Status WritableFileImpl::Close() {
  if (file_ != nullptr) {
    auto err = std::fclose(file_);
    file_ = nullptr;
    if (err) {
      return Status::IoError("fclose fails");
    }
  }
  return Status::OK();
}

Status WritableFileImpl::Write(const Slice& data) {
  auto written = fwrite(data.data(), 1, data.size(), file_);
  if (written != data.size()) {
    return Status::IoError("fwrite fails");
  }
  file_size_in_bytes_ += written;
  return Status::OK();
}

Status WritableFileImpl::Flush() {
  int error = ::fflush(file_);
  if (error) {
    return Status::IoError("fflush fails {0}", error);
  }
  return Status::OK();
}

int64_t WritableFileImpl::file_size_in_bytes() { return file_size_in_bytes_; }

}  // namespace seven
}  // namespace backend
}  // namespace d7
}  // namespace photonscore
