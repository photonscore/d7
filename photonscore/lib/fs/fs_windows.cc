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
#include "photonscore/lib/fs/fs.h"
#include "photonscore/lib/path/path.h"

#include <memory>

#include <direct.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <windows.h>

namespace photonscore {
namespace fs {

using ::photonscore::base::DateTime;

namespace {
std::string GetLastErrorStdStr() {
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
      std::string result((const char*)lpMsgStr, (const char*)lpMsgStr + bufLen);
      LocalFree(lpMsgBuf);
      return result;
    }
  }
  return std::string();
}

template <typename Attributes>
StatInfo ToStatInfo(const Attributes& attrs) {
  StatInfo stat_info;
  stat_info.is_directory = attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
  stat_info.is_symlink = attrs.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT;
  stat_info.is_regular_file =
      !(attrs.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) &&
      !stat_info.is_directory;

  stat_info.size = attrs.nFileSizeHigh;
  stat_info.size <<= 32;
  stat_info.size |= attrs.nFileSizeLow;

  if (stat_info.is_directory) {
    stat_info.size = 0;
  }

  stat_info.unix_mode = 0;
  if (stat_info.is_directory) {
    stat_info.unix_mode |= 0444;  // r
    stat_info.unix_mode |= 0111;  // x
  }
  if (!(attrs.dwFileAttributes & FILE_ATTRIBUTE_READONLY)) {
    stat_info.unix_mode |= 0222;  // w
  }

  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms724284.aspx
  const DateTime file_t0{1601, 1, 1};

  stat_info.created =
      file_t0.AddTicks((int64_t(attrs.ftCreationTime.dwHighDateTime) << 32) |
                       (int64_t(attrs.ftCreationTime.dwLowDateTime)));
  stat_info.modified =
      file_t0.AddTicks((int64_t(attrs.ftLastWriteTime.dwHighDateTime) << 32) |
                       (int64_t(attrs.ftLastWriteTime.dwLowDateTime)));
  return stat_info;
}

Status Utf8ToUtf16(const std::string& src, std::wstring* dst) {
  const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                         src.data(), src.size(), NULL, 0);
  if (length <= 0) {
    return Status::FailedPrecondition("Cannot convert UTF8 string to UTF18");
  }
  dst->resize(length);
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src.data(), src.size(),
                      &dst->front(), length);
  return Status::OK();
}

Status Utf16ToUtf8(const std::wstring& src, std::string* dst) {
  const int length = WideCharToMultiByte(CP_UTF8, 0, src.data(), src.size(),
                                         NULL, 0, NULL, NULL);
  if (length <= 0) {
    return Status::FailedPrecondition("Cannot convert UTF16 string to UTF8");
  }
  dst->resize(length);
  WideCharToMultiByte(CP_UTF8, 0, src.data(), src.size(), (LPSTR)&dst->front(),
                      length, NULL, NULL);
  return Status::OK();
}

}  // namespace

FILE* OpenFile(const std::string& filename, const std::string& mode) {
  std::wstring filename_w;
  if (!Utf8ToUtf16(filename, &filename_w).ok()) {
    return nullptr;
  }

  std::wstring mode_w;
  if (!Utf8ToUtf16(mode, &mode_w).ok()) {
    return nullptr;
  }

  FILE* f = ::_wfsopen(filename_w.c_str(), mode_w.c_str(), _SH_DENYNO);
  if (f != nullptr) {
    _setmode(_fileno(f), _O_BINARY);
  }
  return f;
}

Status MakeDirectory(const std::string& path) {
  return MakeDirectoryWithMode(path, 0755);
}

Status MakeDirectoryWithMode(const std::string& path, int mode) {
  (void)mode;
  std::wstring path_w;
  auto status = Utf8ToUtf16(path, &path_w);
  if (!status.ok()) {
    return status;
  }
  BOOL ok = ::CreateDirectoryW(path_w.c_str(), NULL);
  if (!ok) {
    return Status::IoError("CreateDirectory({0}): '{1}'", path,
                           GetLastErrorStdStr());
  }
  return Status::OK();
}

Status Remove(const std::string& path) {
  std::wstring path_w;
  auto status = Utf8ToUtf16(path, &path_w);
  if (!status.ok()) {
    return status;
  }
  BOOL ok = ::DeleteFileW(path_w.c_str());
  if (!ok) {
    auto delete_file_err = GetLastErrorStdStr();
    // Could be path points to the dir
    ok = ::RemoveDirectoryW(path_w.c_str());
    if (!ok) {
      return Status::IoError(
          "RemoveDirectory({0}): '{1}' and DeleteFile: '{2}'", path,
          GetLastErrorStdStr(), delete_file_err);
    }
  }
  return Status::OK();
}

Status Stat(const std::string& path, StatInfo* info) {
  typedef std::unique_ptr<std::remove_pointer<HANDLE>::type,
                          decltype(&::CloseHandle)>
      FileHandle;

  WIN32_FILE_ATTRIBUTE_DATA attrs;
  std::wstring path_w;
  auto status = Utf8ToUtf16(path, &path_w);
  if (!status.ok()) {
    return status;
  }
  if (!GetFileAttributesExW(path_w.c_str(), GetFileExInfoStandard, &attrs)) {
    return Status::IoError("GetFileAttributesEx({0}): '{1}'", path,
                           GetLastErrorStdStr());
  }
  *info = ToStatInfo(attrs);

  // Here we shall go into the link to get POSIX stat() behaivour instead of
  // lstat() implemented above.
  if (info->is_symlink) {
    FileHandle hFile(::CreateFileW(path_w.c_str(), GENERIC_READ,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL),
                     &::CloseHandle);
    if (hFile.get() == INVALID_HANDLE_VALUE) {
      return Status::IoError("Fail to open symlink target for '{0}'", path);
    }

    const size_t requiredSize = ::GetFinalPathNameByHandleW(
        hFile.get(), nullptr, 0, FILE_NAME_NORMALIZED);
    path_w.resize(requiredSize);

    ::GetFinalPathNameByHandleW(hFile.get(), &path_w[0],
                                static_cast<DWORD>(path_w.size()),
                                FILE_NAME_NORMALIZED);
    if (!GetFileAttributesExW(path_w.c_str(), GetFileExInfoStandard, &attrs)) {
      return Status::IoError("Symlink target GetFileAttributesEx({0}): '{1}'",
                             path, GetLastErrorStdStr());
    }
    *info = ToStatInfo(attrs);
  }

  if (path.size() > 4 && info->is_regular_file) {
    char dot_ext[4];
    for (size_t i = 0; i < 4; i++) {
      dot_ext[i] = path[path.size() - 4 + i];
    }
    if (dot_ext[0] == '.') {
      bool dll = (dot_ext[1] == 'd' || dot_ext[1] == 'D') &&
                 (dot_ext[2] == 'l' || dot_ext[2] == 'L') &&
                 (dot_ext[3] == 'l' || dot_ext[3] == 'L');
      bool exe = (dot_ext[1] == 'e' || dot_ext[1] == 'E') &&
                 (dot_ext[2] == 'x' || dot_ext[2] == 'X') &&
                 (dot_ext[3] == 'e' || dot_ext[3] == 'E');
      bool bat = (dot_ext[1] == 'b' || dot_ext[1] == 'B') &&
                 (dot_ext[2] == 'a' || dot_ext[2] == 'A') &&
                 (dot_ext[3] == 't' || dot_ext[3] == 'T');
      bool cmd = (dot_ext[1] == 'c' || dot_ext[1] == 'C') &&
                 (dot_ext[2] == 'm' || dot_ext[2] == 'M') &&
                 (dot_ext[3] == 'd' || dot_ext[3] == 'D');
      if (dll || exe || bat || cmd) {
        info->unix_mode |= 0111;
      }
    }
  }

  return Status::OK();
}

std::string WorkingDirectory() {
  size_t n = ::GetCurrentDirectoryA(0, NULL);
  std::string res(n - 1, 0);
  ::GetCurrentDirectoryA(n, &res[0]);
  return res;
}

Status SetWorkingDirectory(const std::string& cd) {
  int err = ::_chdir(cd.c_str());
  if (err != 0) {
    return Status::IoError("chdir {0}", ::strerror(errno));
  }
  return Status::OK();
}

class DirectoryWalkerImpl {
 public:
  DirectoryWalkerImpl();
  ~DirectoryWalkerImpl();

  std::string begin_path;

  std::string filename;
  StatInfo stat_info;

  Status Begin(const std::string& path);
  Status Next();

 private:
  void Close();
  HANDLE hff_;
  WIN32_FIND_DATAW ff_;
  Status next_status_;
};

DirectoryWalkerImpl::DirectoryWalkerImpl()
    : hff_(INVALID_HANDLE_VALUE), next_status_(Status::OK()) {}

DirectoryWalkerImpl::~DirectoryWalkerImpl() { Close(); }

void DirectoryWalkerImpl::Close() {
  if (hff_ != INVALID_HANDLE_VALUE) {
    FindClose(hff_);
  }
  hff_ = INVALID_HANDLE_VALUE;
}

Status DirectoryWalkerImpl::Begin(const std::string& path) {
  Close();
  std::wstring path_w;
  auto status = Utf8ToUtf16(path + "/*.*", &path_w);
  if (!status.ok()) {
    return status;
  }
  hff_ = ::FindFirstFileW(path_w.c_str(), &ff_);
  if (hff_ == INVALID_HANDLE_VALUE) {
    return Status::IoError("FindFirstFile({0}): '{1}'", path,
                           GetLastErrorStdStr());
  }
  begin_path = path;
  return Status::OK();
}

Status DirectoryWalkerImpl::Next() {
  if (!next_status_.ok()) {
    return next_status_;
  }
  if (hff_ == INVALID_HANDLE_VALUE) {
    return Status::FailedPrecondition("Directory is not open");
  }

  auto s = Utf16ToUtf8(ff_.cFileName, &filename);
  if (!s.ok()) {
    return s;
  }

  stat_info = ToStatInfo(ff_);
  if (!FindNextFileW(hff_, &ff_)) {
    if (GetLastError() == ERROR_NO_MORE_FILES) {
      next_status_ = Status::EndOfFile("{0}", begin_path);
    } else {
      next_status_ = Status::IoError("FindNextFile({0}): '{1}", begin_path,
                                     GetLastErrorStdStr());
    }
    Close();
  }
  return Status::OK();
}

Status Chmod(const std::string& path, int mode) {
  bool read_only = (mode & 0222) == 0;

  auto attrs = ::GetFileAttributesA(path.c_str());
  if (attrs == INVALID_FILE_ATTRIBUTES) {
    return Status::IoError("GetFileAttributes {0}", GetLastErrorStdStr());
  }

  if (read_only) {
    attrs |= FILE_ATTRIBUTE_READONLY;
  } else {
    attrs &= ~FILE_ATTRIBUTE_READONLY;
  }

  auto res = ::SetFileAttributesA(path.c_str(), attrs);
  if (res == FALSE) {
    return Status::IoError("SetFileAttributes {0}", GetLastErrorStdStr());
  }

  return Status::OK();
}

DirectoryWalker::DirectoryWalker() : impl_(new DirectoryWalkerImpl()) {}

DirectoryWalker::~DirectoryWalker() { delete impl_; }

Status DirectoryWalker::Begin(const std::string& path) {
  return impl_->Begin(path);
}

Status DirectoryWalker::Next() {
  auto s = impl_->Next();
  while (s.ok() && (impl_->filename == "." || impl_->filename == ".." ||
                    impl_->filename == impl_->begin_path)) {
    s = impl_->Next();
  }
  return s;
}

const StatInfo& DirectoryWalker::stat_info() const {  //
  return impl_->stat_info;
}

const std::string& DirectoryWalker::name() const {  //
  return impl_->filename;
}

}  // namespace fs
}  // namespace photonscore
#endif
