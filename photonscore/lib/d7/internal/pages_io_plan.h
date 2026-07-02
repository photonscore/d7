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

#ifndef PHOTONSCORE_LIB_D7_INTERNAL_PAGES_IO_PLAN_H
#define PHOTONSCORE_LIB_D7_INTERNAL_PAGES_IO_PLAN_H

#include <cstddef>
#include <cstdint>

namespace photonscore {
namespace d7 {
namespace internal {

class PagesIOPlan {
 public:
  struct Entry {
    Entry() : io_page_number(-1) {}

    int64_t io_page_number;  // Page to be read or written
    size_t begin;            // First item to copy
    size_t end;              // Last item to copy
    size_t destination;      // Destination buffer offset

    bool valid() const { return io_page_number != -1; }
  };

  PagesIOPlan(uint64_t total_size, size_t page_size, uint64_t io_offset,
              size_t io_length);

  size_t size() const {
    return inner_entries_ +                  // Full pages
           (first_entry_.valid() ? 1 : 0) +  // Head
           (last_entry_.valid() ? 1 : 0);    // Tail
  }

  Entry operator()(size_t i) const;

  int64_t total_size() const { return total_size_; }
  size_t page_size() const { return page_size_; }
  int64_t io_offset() const { return io_offset_; }
  size_t io_length() const { return io_length_; }

  bool valid() const { return valid_; }

 private:
  uint64_t total_size_;
  size_t page_size_;
  uint64_t io_offset_;
  size_t io_length_;

  bool valid_;

  Entry first_entry_;
  size_t inner_entries_;
  Entry last_entry_;
};

}  // namespace internal
}  // namespace d7
}  // namespace photonscore

#endif  // PHOTONSCORE_LIB_D7_INTERNAL_PAGES_IO_PLAN_H
