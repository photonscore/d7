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

#include "photonscore/lib/d7/backend/seven/writable_file_batch.h"

namespace photonscore {
namespace d7 {
namespace backend {
namespace seven {

WritableFileBatch::WritableFileBatch(WritableFile* target,
                                     size_t expected_batch)
    : target_(target), target_size_(target->file_size_in_bytes()) {
  buffer_.reserve(expected_batch);
}

WritableFileBatch::~WritableFileBatch() { Flush(); }
Status WritableFileBatch::Close() { return Flush(); }

Status WritableFileBatch::Write(const base::Slice& data) {
  auto begin = data.data();
  auto end = begin + data.size();
  size_t n = buffer_.size();
  buffer_.resize(data.size() + n);
  std::copy(begin, end, &buffer_[n]);
  return Status::OK();
}

Status WritableFileBatch::Flush() {
  auto status = target_->Write(buffer_);
  if (!status.ok()) return status;
  target_size_ = target_->file_size_in_bytes();
  buffer_.resize(0);
  return Status::OK();
}

int64_t WritableFileBatch::file_size_in_bytes() {
  return target_size_ + buffer_.size();
}

}  // namespace seven
}  // namespace backend
}  // namespace d7
}  // namespace photonscore

