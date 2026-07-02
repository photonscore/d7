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

#include "photonscore/lib/d7/backend/seven/log_appender.h"
#include "photonscore/lib/d7/backend/seven/log_format.h"

#include <algorithm>
#include <cstdio>

using photonscore::base::Slice;

namespace photonscore {
namespace d7 {
namespace backend {
namespace seven {

LogAppender::LogAppender(std::unique_ptr<WritableFile>&& destination)
    : offset_(0), batch_(nullptr), file_(std::move(destination)) {}

LogAppender::~LogAppender() {
  Status status = Close();
  assert(status.ok() && "Failure in destructor");
}

Status LogAppender::Close() {
  if (batch_) batch_->Close();
  return file_->Close();
}

Status LogAppender::Flush() { return file()->Flush(); }

Status LogAppender::Append(const base::Slice& slice) {
  assert(slice.size() != 0 && "LogAppender zero length chunk");
  assert(slice.size() < LogFormat::kMaxFragment && "Too large chunk");

  if (slice.size() == 0) {
    return Status::InvalidArgument("Zero size fragment");
  }

  if (slice.size() >= LogFormat::kMaxFragment) {
    return Status::InvalidArgument("Too large fragment {0} (expected max {1})",
                                   slice.size(), int(LogFormat::kMaxFragment));
  }

  Status status;
  Slice d = slice;
  bool begin = true;
  while (d.size() != 0) {
    const size_t leftover = LogFormat::kBlockSize - offset_;
    if (leftover < LogFormat::kHeaderSize) {
      // Pad page with zeros
      char zeros[LogFormat::kHeaderSize] = {0, 0};
      status = file()->Write(Slice(zeros, leftover));
      if (!status.ok()) {
        return status;
      }
      // Start new block
      offset_ = 0;
    }

    const size_t avail =
        LogFormat::kBlockSize - offset_ - LogFormat::kHeaderSize;
    const size_t fragment_len = std::min<size_t>(d.size(), avail);
    const bool end = (fragment_len == d.size());

    assert((fragment_len & LogFormat::kBlockMask) == fragment_len);

    uint16_t header = fragment_len;
    header |= begin ? LogFormat::kFragmentBegin : 0;
    header |= end ? LogFormat::kFragmentEnd : 0;

    status = file()->Write(Slice((char*)&header, sizeof(header)));
    if (!status.ok()) {
      return status;
    }
    offset_ += sizeof(header);

    status = file()->Write(Slice(d.data(), fragment_len));
    if (!status.ok()) {
      return status;
    }
    d += fragment_len;
    offset_ = (offset_ + fragment_len) & LogFormat::kBlockMask;
    begin = false;
  }

  return Status::OK();
}

Status LogAppender::PadToBlock() {
  while (offset_ != 0) {
    char zero = 0x00;
    auto status = file()->Write(Slice(&zero, 1));
    if (!status.ok()) {
      return status;
    }
    offset_++;
    offset_ &= LogFormat::kBlockMask;
  }
  return Status::OK();
}

int64_t LogAppender::file_size_in_bytes() {
  return file()->file_size_in_bytes();
}

size_t LogAppender::file_size_in_blocks() {
  return size_t(file_size_in_bytes() / LogFormat::kBlockSize);
}

Status LogAppender::BeginBatch(size_t expected_batch_size) {
  assert(batch_ == nullptr && "Batch already open");
  batch_ = new WritableFileBatch(file_.get(), expected_batch_size);
  return Status::OK();
}

Status LogAppender::CommitBatch() {
  assert(batch_ != nullptr && "No batch");
  auto status = batch_->Flush();
  delete batch_;
  batch_ = nullptr;
  return status;
}

}  // namespace seven
}  // namespace backend
}  // namespace d7
}  // namespace photonscore
