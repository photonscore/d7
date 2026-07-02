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

#ifndef PHOTONSCORE_LIB_D7_BACKEND_SEVEN_PAGED_APPENDER_H
#define PHOTONSCORE_LIB_D7_BACKEND_SEVEN_PAGED_APPENDER_H

#include "photonscore/lib/base/status.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <string>

namespace photonscore {
namespace d7 {
namespace backend {
namespace seven {

template <typename Container>
class PagedAppender {
 public:
  PagedAppender(Container* container)
      : container_(container), total_bytes_(0), bytes_per_page_(-1) {}

  ~PagedAppender() {
    assert(container_ == nullptr && "PagedAppender must be closed before");
  }

  Status Init() {
    bytes_per_page_ = container_->bytes_per_page();
    page_.reserve(bytes_per_page_);
    return Status::OK();
  }

  Status Append(const void* data, size_t length) {
    assert(container_ != nullptr && "PagedAppender was closed");
    assert(bytes_per_page_ != size_t(-1) &&
           "PagedAppender was not initialized");
    auto p = (const char*)data;
    assert(page_.size() < bytes_per_page_);
    while (length != 0) {
      const size_t old_size = page_.size();
      const size_t avail = bytes_per_page_ - old_size;
      const size_t to_copy = std::min(avail, length);
      assert((old_size + to_copy) <= bytes_per_page_);
      page_.resize(old_size + to_copy);
      std::copy(p, p + to_copy, page_.begin() + old_size);
      assert(page_.size() <= bytes_per_page_);
      if (page_.size() == bytes_per_page_) {
        auto status = container_->AppendPage(page_);
        if (!status.ok()) {
          return status;
        }
        page_.resize(0);
        assert(page_.capacity() >= bytes_per_page_);
      }
      length -= to_copy;
      p += to_copy;
      total_bytes_ += to_copy;
      assert(page_.size() == 0 ? ((total_bytes_ % bytes_per_page_) == 0) : 1);
    }
    return Status::OK();
  }

  Status Close() {
    Status status = Status::OK();
    if (container_ != nullptr) {
      if (page_.size() > 0) {
        status = container_->AppendPage(page_);
      }
    }
    container_ = nullptr;
    return status;
  }

  void StepPage() {
    // Use when the data is going into without Append(...)
    assert(page_.size() == 0);
    total_bytes_ += bytes_per_page_;
  }

  int64_t total_bytes() const { return total_bytes_; }

 private:
  Container* container_;
  int64_t total_bytes_;
  size_t bytes_per_page_;
  std::string page_;
};

}  // namespace seven
}  // namespace backend
}  // namespace d7
}  // namespace photonscore

#endif  // PHOTONSCORE_LIB_D7_BACKEND_SEVEN_PAGED_APPENDER_H
