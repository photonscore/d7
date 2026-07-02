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
#include "photonscore/lib/fs/fs.h"
#include "photonscore/lib/path/path.h"

#include <dirent.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace photonscore {
namespace fs {

using ::photonscore::base::DateTime;
using ::photonscore::path::Join;

FILE* OpenFile(const std::string& filename, const std::string& mode) {
  return fopen(filename.c_str(), mode.c_str());
}

Status MakeDirectory(const std::string& path) {
  return MakeDirectoryWithMode(path, 0755);
}

Status MakeDirectoryWithMode(const std::string& path, int mode) {
  int err = ::mkdir(path.c_str(), mode);
  if (err) {
    return Status::IoError("mkdir failure: ", ::strerror(err));
  }
  return Status::OK();
}

Status Remove(const std::string& path) {
  int err = ::unlink(path.c_str());
  if (err) {
    // Could be path points to the dir
    err = ::rmdir(path.c_str());
    if (err) {
      return Status::IoError("rmdir failure {0}", ::strerror(err));
    }
  }
  return Status::OK();
}

Status Stat(const std::string& path, StatInfo* info) {
  struct stat os_info;
  int err = ::stat(path.c_str(), &os_info);
  if (err) {
    return Status::IoError("stat({0}) failure {1} {2}", path, err,
                           ::strerror(err));
  }

  info->is_directory = S_ISDIR(os_info.st_mode);
  info->is_symlink = S_ISLNK(os_info.st_mode);
  info->is_regular_file = S_ISREG(os_info.st_mode);
  info->size = os_info.st_size;
  if (info->is_directory) {
    info->size = 0;
  }
  info->unix_mode = os_info.st_mode;

#ifdef __linux__
  info->created = DateTime::FromCTime(os_info.st_ctime);
  info->modified = DateTime::FromCTime(os_info.st_mtime);
#else
  info->created = DateTime::FromCTime(os_info.st_ctimespec.tv_sec);
  info->modified = DateTime::FromCTime(os_info.st_mtimespec.tv_sec);
#endif

  return Status::OK();
}

std::string WorkingDirectory() {
  char buf[PATH_MAX + 1];
  buf[0] = 0;
  ::getcwd(buf, PATH_MAX);
  return (const char*)buf;
}

Status SetWorkingDirectory(const std::string& cd) {
  int err = ::chdir(cd.c_str());
  if (err != 0) {
    return Status::IoError("chdir {0}", ::strerror(errno));
  }
  return Status::OK();
}

Status Chmod(const std::string& path, int mode) {
  int res = ::chmod(path.c_str(), mode);
  if (res == -1) {
    return Status::IoError("chmod {0}", ::strerror(errno));
  }
  return Status::OK();
}

class DirectoryWalkerImpl {
 public:
  DirectoryWalkerImpl();
  ~DirectoryWalkerImpl();

  std::string filename;
  StatInfo stat_info;

  Status Begin(const std::string& path);
  Status Next();

 private:
  std::string path_;
  DIR* dir_p;
  void Close();
};

DirectoryWalkerImpl::DirectoryWalkerImpl() : dir_p(nullptr) {}

DirectoryWalkerImpl::~DirectoryWalkerImpl() { Close(); }

void DirectoryWalkerImpl::Close() {
  if (dir_p) {
    closedir(dir_p);
  }
  dir_p = nullptr;
}

Status DirectoryWalkerImpl::Begin(const std::string& path) {
  Close();
  dir_p = ::opendir(path.c_str());
  if (dir_p == nullptr) {
    return Status::IoError("opendir failure {0}", ::strerror(errno));
  }
  path_ = path;
  return Status::OK();
}

Status DirectoryWalkerImpl::Next() {
  if (dir_p == nullptr) {
    return Status::FailedPrecondition("Directory is not open");
  }

  auto entry = ::readdir(dir_p);
  if (entry == nullptr) {
    Close();
    return Status::EndOfFile("readdir {0}", path_);
  }

  filename = entry->d_name;
  return Stat(Join(path_, filename), &stat_info);
}

DirectoryWalker::DirectoryWalker() : impl_(new DirectoryWalkerImpl()) {}

DirectoryWalker::~DirectoryWalker() { delete impl_; }

Status DirectoryWalker::Begin(const std::string& path) {
  return impl_->Begin(path);
}

Status DirectoryWalker::Next() {
  auto s = impl_->Next();
  while (s.ok() && (impl_->filename == "." || impl_->filename == "..")) {
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
