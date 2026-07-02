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

#ifdef _WIN32
#include "photonscore/lib/mmap/mmap.h"

namespace photonscore {
namespace mmap {
namespace {
std::string GetLastErrorStr() {
  DWORD error = GetLastError();
  if (error) {
    LPVOID lpMsgBuf;
    DWORD bufLen = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf, 0, NULL);
    if (bufLen) {
      LPCSTR lpMsgStr = (LPCSTR)lpMsgBuf;
      std::string result(lpMsgStr, lpMsgStr + bufLen);

      LocalFree(lpMsgBuf);

      return result;
    }
  }
  return std::string();
}
}  // namespace

MemoryMappedFile::~MemoryMappedFile() {
  ::UnmapViewOfFile(data());
  ::CloseHandle(map_);
  ::CloseHandle(fd_);
}

Status MemoryMappedFile::Open(const std::string& filename, bool readonly,
                              size_t offset, size_t size,
                              MemoryMappedFile** mm) {
  DWORD cf_access = readonly ? (GENERIC_READ) : (GENERIC_READ | GENERIC_WRITE);
  DWORD cf_share = FILE_SHARE_READ | FILE_SHARE_WRITE;
  HANDLE fd = ::CreateFileA(filename.c_str(), cf_access, cf_share, NULL,
                            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (fd == INVALID_HANDLE_VALUE) {
    return Status::IoError("CreateFileA '{0}'", GetLastErrorStr());
  }

  if (size == 0) {
    DWORD sh;
    DWORD sl = ::GetFileSize(fd, &sh);
    if (sh == INVALID_FILE_SIZE) {
      ::CloseHandle(fd);
      return Status::IoError("GetFileSize '{0}'", GetLastErrorStr());
    }
    size = sh;
    size <<= 32;
    size |= sl;
  }

  if (size <= offset) {
    ::CloseHandle(fd);
    return Status::FailedPrecondition("size <= offset ({0} <= {1}", size,
                                      offset);
  }

  DWORD cfm_protect = readonly ? PAGE_READONLY : PAGE_READWRITE;
  DWORD cfm_size_hi = (size >> 32);
  DWORD cfm_size_lo = (size & 0x0FFFFFFFFUL);
  HANDLE map = ::CreateFileMappingA(fd, NULL, cfm_protect, cfm_size_hi,
                                    cfm_size_lo, NULL);
  if (map == NULL) {
    ::CloseHandle(fd);
    return Status::IoError("CreateFileMappingA '{0}'", GetLastErrorStr());
  }

  DWORD mvof_access =
      readonly ? FILE_MAP_READ : (FILE_MAP_READ | FILE_MAP_WRITE);
  DWORD mvof_offset_hi = (offset >> 32);
  DWORD mvof_offset_lo = (offset & 0x0FFFFFFFFUL);
  void* view = ::MapViewOfFile(map, mvof_access, mvof_offset_hi, mvof_offset_lo,
                               size - offset);
  if (view == NULL) {
    ::CloseHandle(map);
    ::CloseHandle(fd);
    return Status::IoError("MapViewOfFile '{0}'", GetLastErrorStr());
  }

  auto res = new MemoryMappedFile({view, size - offset});
  res->fd_ = fd;
  res->map_ = map;
  *mm = res;

  return Status::OK();
}

}  // namespace mmap
}  // namespace photonscore

#endif  //_WIN32
