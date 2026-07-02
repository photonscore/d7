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

#include "photonscore/lib/base/slice.h"
#include "photonscore/lib/d7/backend/seven/log_format.h"
#include "photonscore/lib/d7/backend/seven/log_reader.h"

using ::photonscore::base::Slice;
using ::photonscore::mmap::MemoryMappedFile;

namespace photonscore {
namespace d7 {
namespace backend {
namespace seven {

LogReader::LogReader() {}
LogReader::~LogReader() { Close(); }

Status LogReader::Open(const std::string& filename) {
  return MemoryMappedFile::OpenRead(filename, &mm_);
}

Status LogReader::Close() {
  mm_.reset();
  return Status::OK();
}

Status LogReader::Read(int64_t offset, std::string* buffer,
                       int64_t* new_offset) const {
  Slice slice = {((const char*)mm_->data()) + offset,
                 mm_->size() - size_t(offset)};

  bool found = false;
  buffer->resize(0);

  size_t in_block = offset & LogFormat::kBlockMask;
  while (buffer->size() < LogFormat::kMaxFragment) {
    if ((LogFormat::kBlockSize - in_block) == 1) {
      if (slice.size() < 1) {
        return Status::InvalidFormat("require 1 byte, got {0}", slice.size());
      }
      slice += 1;
      in_block = 0;
    }

    uint16_t header;
    if (slice.size() < sizeof(header)) {
      return Status::InvalidFormat("require 2 bytes, got {0}", slice.size());
    }
    slice = slice.Read(&header);
    static_assert(sizeof(header) == LogFormat::kHeaderSize,
                  "Wrong header type");
    in_block += 2;
    in_block &= LogFormat::kBlockMask;

    bool header_begin = header & LogFormat::kFragmentBegin;
    bool header_end = header & LogFormat::kFragmentEnd;
    size_t len = header & LogFormat::kBlockMask;

    if (found && header_begin) {
      return Status::InvalidFormat("Invalid header");
    }

    if (!found) {
      if (!header_begin) {
        if ((in_block + len) > LogFormat::kBlockSize) {
          return Status::InvalidFormat("Data crosses block boundary");
        }
        in_block += len;
        in_block &= LogFormat::kBlockMask;
        slice += std::min(slice.size(), len);
        continue;
      }
    }

    found = true;
    if ((in_block + len) > LogFormat::kBlockSize) {
      return Status::InvalidFormat("Data crosses block boundary");
    }
    in_block += len;
    in_block &= LogFormat::kBlockMask;

    if (slice.size() < len) {
      return Status::InvalidFormat("require {1} bytes, got {0}", slice.size(),
                                   len);
    }

    auto n = buffer->size();
    buffer->resize(n + len);
    assert(slice.size() >= len);
    if (len > 0) {
      std::copy(slice.data(), slice.data() + len, &buffer->at(n));
    }

    slice += len;

    if (header_end) {
      break;
    }
  }

  if (buffer->size() >= LogFormat::kMaxFragment) {
    return Status::InvalidFormat("Too large fragment {0} >= {1}",
                                 buffer->size(), int(LogFormat::kMaxFragment));
  }

  uintptr_t p0 = uintptr_t(mm_->data()) + offset;
  uintptr_t p1 = uintptr_t(slice.data());
  if (new_offset) {
    *new_offset = offset + (p1 - p0);
  }

  return Status::OK();
}

Status LogReader::ReadBlock(int64_t block_offset, std::string* buffer,
                            int64_t* new_offset) const {
  return Read(block_offset * LogFormat::kBlockSize, buffer, new_offset);
}

size_t LogReader::file_size_in_blocks() const {
  return size_t(file_size_in_bytes() / LogFormat::kBlockSize);
}

}  // namespace seven
}  // namespace backend
}  // namespace d7
}  // namespace photonscore

