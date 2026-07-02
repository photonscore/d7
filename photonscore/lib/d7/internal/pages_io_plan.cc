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

#include "photonscore/lib/d7/internal/pages_io_plan.h"

#include <algorithm>

namespace photonscore {
namespace d7 {
namespace internal {

PagesIOPlan::PagesIOPlan(uint64_t total_size, size_t page_size,
                         uint64_t io_offset, size_t io_length)
    : total_size_(total_size),
      page_size_(page_size),
      io_offset_(io_offset),
      io_length_(io_length),
      valid_((io_offset + io_length) <= total_size && page_size_ > 0) {
  if (valid()) {
    first_entry_.io_page_number = -1;
    inner_entries_ = 0;
    last_entry_.io_page_number = -1;

    size_t remains = io_length;

    if (remains != 0) {
      first_entry_.io_page_number = io_offset / page_size;
      first_entry_.begin = size_t(io_offset % page_size);
      size_t chunk = std::min(page_size_ - first_entry_.begin, remains);
      first_entry_.end = first_entry_.begin + chunk;
      first_entry_.destination = 0;
      remains -= chunk;
    }

    if (remains != 0) {
      inner_entries_ = remains / page_size_;
      remains -= inner_entries_ * page_size_;
    }

    if (remains != 0) {
      last_entry_.io_page_number =
          first_entry_.io_page_number + inner_entries_ + 1;
      last_entry_.begin = 0;
      last_entry_.end = remains;
      last_entry_.destination = io_length - remains;
    }
  }
}

PagesIOPlan::Entry PagesIOPlan::operator()(size_t i) const {
  if (i == 0) return first_entry_;
  if (i == (size() - 1) && last_entry_.valid()) return last_entry_;

  PagesIOPlan::Entry e;
  if (i < size()) {
    e.io_page_number = first_entry_.io_page_number + i;
    e.begin = 0;
    e.end = page_size_;
    e.destination = (first_entry_.end - first_entry_.begin) +  // head
                    (i - 1) * page_size_;
  }

  return e;
}

}  // namespace internal
}  // namespace d7
}  // namespace photonscore
