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

#include "photonscore/lib/fs/fs.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace photonscore {
namespace fs {

Status ReadFile(const std::string& filename, std::string* data,
                size_t max_size) {
  auto f = OpenFile(filename, "r");
  if (f == nullptr) {
    return Status::IoError("cannot open '{0}'", filename);
  }

  if (std::fseek(f, 0, SEEK_END) != 0) {
    std::fclose(f);
    return Status::IoError("seek to end fails '{0}'", filename);
  }

  auto sz = std::ftell(f);
  if (sz < 0) {
    std::fclose(f);
    return Status::IoError("fail getting size '{0}'", filename);
  }

  if (std::fseek(f, 0, SEEK_SET) != 0) {
    std::fclose(f);
    return Status::IoError("seek to beginning fails '{0}'", filename);
  }

  sz = std::min(size_t(sz), max_size);
  data->resize(sz);

  if (sz != 0) {
    // Looks like normal file, read it
    auto rd = std::fread(&data->at(0), 1, sz, f);
    if (long(rd) != sz) {
      std::fclose(f);
      return Status::IoError("reading '{0}', got {1} expected {2}", filename,
                             rd, sz);
    }
  } else {
    // Can be special file like /proc or /dev
    while (data->size() < max_size) {
      char buf[1024];
      size_t n = fread(buf, 1, sizeof(buf), f);
      if (n == 0) {
        break;
      }
      data->append(buf, n);
    }
  }

  if (fclose(f) != 0) {
    return Status::IoError("fclose of '{0}'", filename);
  }

  return Status::OK();
}

Status WriteFile(const std::string& filename, const std::string& data) {
  return WriteFile(filename, data.size(), data.data());
}

Status WriteFile(const std::string& filename, size_t n, const void* ptr) {
  auto f = OpenFile(filename, "w+");
  if (f == nullptr) {
    return Status::IoError("cannot open '{0}'", filename);
  }

  auto wr = std::fwrite(ptr, 1, n, f);
  if (wr != n) {
    fclose(f);
    return Status::IoError("write '{0}', got {1} expected {2}", filename, wr,
                           n);
  }

  if (fclose(f) != 0) {
    return Status::IoError("fclose of '{0}'", filename);
  }

  return Status::OK();
}

bool DirExists(const std::string& path) {
  StatInfo stat_info;
  auto status = Stat(path, &stat_info);
  if (status.ok()) {
    return stat_info.is_directory;
  }
  return false;
}

bool FileExists(const std::string& path) {
  StatInfo stat_info;
  auto status = Stat(path, &stat_info);
  if (status.ok()) {
    return stat_info.is_regular_file;
  }
  return false;
}

bool Exists(const std::string& path) {
  StatInfo stat_info;
  auto status = Stat(path, &stat_info);
  return status.ok();
}

std::string GetTempDir() {
  const char* candidates[] = {
      // Non-null only during unittest/regtest
      getenv("TEST_TMPDIR"),
      // Explicitly-supplied temp dirs
      getenv("TMPDIR"),
      getenv("TMP"),
      getenv("TEMP"),
  };

  for (auto res : candidates) {
    if (res != nullptr) return res;
  }

  return "/tmp";
}

}  // namespace fs
}  // namespace photonscore
