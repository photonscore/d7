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

#ifndef PHOTONSCORE_LIB_FS_FS_H
#define PHOTONSCORE_LIB_FS_FS_H

#include "photonscore/lib/base/date_time.h"
#include "photonscore/lib/base/status.h"

#include <cstdint>
#include <limits>
#include <string>

namespace photonscore {
namespace fs {

FILE* OpenFile(const std::string& filename, const std::string& mode);

Status ReadFile(const std::string& filename, std::string* data,
                size_t max_size = std::numeric_limits<size_t>::max());
Status WriteFile(const std::string& filename, size_t n, const void* ptr);
Status WriteFile(const std::string& filename, const std::string& data);

Status MakeDirectory(const std::string& path);
Status MakeDirectoryWithMode(const std::string& path, int mode);

Status Remove(const std::string& path);

bool DirExists(const std::string& path);
bool FileExists(const std::string& path);
bool Exists(const std::string& path);

struct StatInfo {
  bool is_directory;
  bool is_symlink;
  bool is_regular_file;
  uint64_t size;
  base::DateTime created;
  base::DateTime modified;
  int unix_mode;
};

Status Stat(const std::string& path, StatInfo* info);

std::string GetTempDir();
std::string WorkingDirectory();
Status SetWorkingDirectory(const std::string& cd);

Status Chmod(const std::string& path, int mode);

class DirectoryWalkerImpl;

class DirectoryWalker {
  friend class DirectoryWalkerImpl;

 public:
  DirectoryWalker();
  ~DirectoryWalker();

  Status Begin(const std::string& path);
  Status Next();

  const StatInfo& stat_info() const;
  const std::string& name() const;

 private:
  DirectoryWalkerImpl* impl_;
};

}  // namespace fs
}  // namespace photonscore

#endif  // PHOTONSCORE_LIB_FS_FS_H
