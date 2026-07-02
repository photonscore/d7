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

#ifndef PHOTONSCORE_LIB_MMAP_MMAP_H
#define PHOTONSCORE_LIB_MMAP_MMAP_H

#include "photonscore/lib/base/slice.h"
#include "photonscore/lib/base/status.h"

#include <memory>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace photonscore {
namespace mmap {

class MemoryMappedFile {
 public:
  ~MemoryMappedFile();

  static Status Open(const std::string& filename, bool readonly, size_t offset,
                     size_t size, MemoryMappedFile** mm);

  static Status OpenRead(const std::string& filename, size_t offset,
                         size_t size, MemoryMappedFile** mm) {
    return Open(filename, true, offset, size, mm);
  }
  static Status OpenRead(const std::string& filename, MemoryMappedFile** mm) {
    return OpenRead(filename, 0, 0, mm);
  }

  static Status OpenWrite(const std::string& filename, size_t offset,
                          size_t size, MemoryMappedFile** mm) {
    return Open(filename, false, offset, size, mm);
  }
  static Status OpenWrite(const std::string& filename, MemoryMappedFile** mm) {
    return OpenWrite(filename, 0, 0, mm);
  }

  template <typename SmartPtr>
  static Status OpenRead(const std::string& filename, size_t offset,
                         size_t size, SmartPtr* mm) {
    MemoryMappedFile* res = nullptr;
    auto status = Open(filename, true, offset, size, &res);
    mm->reset(res);
    return status;
  }
  template <typename SmartPtr>
  static Status OpenRead(const std::string& filename, SmartPtr* mm) {
    return OpenRead<SmartPtr>(filename, 0, 0, mm);
  }

  template <typename SmartPtr>
  static Status OpenWrite(const std::string& filename, size_t offset,
                          size_t size, SmartPtr* mm) {
    MemoryMappedFile* res = nullptr;
    auto status = Open(filename, false, offset, size, &res);
    mm->reset(res);
    return status;
  }
  template <typename SmartPtr>
  static Status OpenWrite(const std::string& filename, SmartPtr* mm) {
    return OpenWrite<SmartPtr>(filename, 0, 0, mm);
  }

  void* data() { return (void*)data_.data(); }
  size_t size() const { return data_.size(); }

 private:
  MemoryMappedFile() = delete;
  MemoryMappedFile(MemoryMappedFile&) = delete;
  MemoryMappedFile(base::Slice mapping) : data_(mapping) {}

  base::Slice data_;

#ifdef _WIN32
  HANDLE fd_;
  HANDLE map_;
#else
  int fd_;
#endif
};

}  // namespace mmap
}  // namespace photonscore

#endif  // PHOTONSCORE_LIB_MMAP_MMAP_H
