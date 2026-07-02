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

#ifndef _WIN32
#include "photonscore/lib/mmap/mmap.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

namespace photonscore {
namespace mmap {

MemoryMappedFile::~MemoryMappedFile() {
  ::munmap(data(), size());
  ::close(fd_);
}

Status MemoryMappedFile::Open(const std::string& filename, bool readonly,
                              size_t offset, size_t size,
                              MemoryMappedFile** mm) {
  int access_flags = readonly ? R_OK : (R_OK | W_OK);
  bool file_exists = ::access(filename.c_str(), access_flags) == 0;

  if (size == 0 && !file_exists) {
    return Status::FailedPrecondition(
        "cannot access file {0} and size is not given", filename);
  }

  const int open_flag = readonly ? (O_RDONLY) : (O_RDWR | O_CREAT);
  int fd = ::open(filename.c_str(), open_flag, 0644);
  if (fd == -1) {
    return Status::IoError("open {0} fails {1}", filename, errno);
  }

  if (size == 0) {
    size = ::lseek(fd, 0, SEEK_END);
    if (size == size_t(-1)) {
      ::close(fd);
      return Status::IoError("lseek {0} fails {1}", filename, errno);
    }
  } else {
    if (!file_exists) {
      // new file to be extended
      int err = ::ftruncate(fd, size);
      if (err) {
        return Status::IoError("ftruncate {0} fails {1}", filename, errno);
      }
    }
  }

  if (size <= offset) {
    ::close(fd);
    return Status::FailedPrecondition("size <= offset ({0} <= {1}", size,
                                      offset);
  }

  int mmap_prot = readonly ? (PROT_READ) : (PROT_READ | PROT_WRITE);
  int mmap_flags = MAP_FILE | MAP_SHARED;
  void* ptr = ::mmap(nullptr, size - offset, mmap_prot, mmap_flags, fd, offset);
  if (ptr == MAP_FAILED) {
    ::close(fd);
    return Status::IoError("mmap fails {0}", errno);
  }

  auto res = new MemoryMappedFile({ptr, size - offset});
  res->fd_ = fd;
  *mm = res;

  return Status::OK();
}

}  // namespace mmap
}  // namespace photonscore

#endif  //!_WIN32
