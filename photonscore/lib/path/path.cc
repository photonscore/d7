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

#include "photonscore/lib/path/path.h"

#include <cassert>
#include <vector>

namespace {

inline bool IsSeparator(char c) { return c == '/' || c == '\\'; }

inline bool IsLetter(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

// Internal decomposition of a path used by Join(). Not part of the public API.
struct PathComponents {
  std::string root;
  std::vector<std::string> components;
  std::string name_without_ext;
  std::string extension;

  std::string ToString() const;
};

std::string PathComponents::ToString() const {
  std::string res;

  // Process root
  if (root.size() == 2 && IsLetter(root[0]) && root[1] == ':') {
    // Windows C:
    res = root;
    if (components.size() > 0) res += '/' + components[0];
  } else if (root.size() == 2 && root[0] == '\\' && root[1] == '\\') {
    // Windows smb share
    res = "\\\\";
    if (components.size() > 0) res += components[0];
  } else if (root.size() == 4 && root[0] == '\\' && root[1] == '\\' &&
             root[2] == '.' && root[3] == '\\') {
    // Windows named pipe
    if (components.size() > 0) res += root + components[0];
  } else if (root.size() > 2) {
    // URL
    res = root + '/';
    if (components.size() > 0) res += '/' + components[0];
  } else if (root.size() == 1 && root[0] == '/') {
    // UNIX root
    res = '/';
    if (components.size() > 0) res += components[0];
  } else {
    assert(root.size() == 0);
    if (components.size() > 0) res = components[0];
  }

  for (size_t i = 1; i < components.size(); i++) {
    res += '/' + components[i];
  }

  if (name_without_ext.size() > 0 || extension.size() > 0) {
    if (res.size() > 0 && res.back() != '/') res += '/';
    res += name_without_ext + extension;
  }

  return res;
}

std::string GetExtension(const std::string& path) {
  size_t i;

  if (!path.empty()) {
    // Skip tailing slashes
    for (i = path.size() - 1; i != 0; --i)
      if (!IsSeparator(path[i])) break;

    const size_t tail = i;

    for (; i != 0; --i) {
      const char c = path[i];
      if (c == '.') {
        // Check if path is a unix hidden name "a/b/.name"
        if (i == 0) break;
        if (IsSeparator(path[i - 1])) break;

        return path.substr(i, tail - i + 1);
      }

      // No extension found "a/b/name"
      if (IsSeparator(c)) break;
    }
  }

  return std::string();
}

std::string GetRoot(const std::string& path) {
  if (!path.empty()) {
    const size_t n = path.size();

    // Windows named pipe: \\.\pipe
    if ((n >= 4) && (path[0] == '\\' && path[1] == '\\' && path[2] == '.' &&
                     path[3] == '\\'))
      return "\\\\.\\";

    // Windows network: \\server\file
    if ((n >= 2) && (path[0] == '\\' && path[1] == '\\')) return "\\\\";

    // Windows path: x:/
    if ((n >= 3) &&
        (IsLetter(path[0]) && path[1] == ':' && IsSeparator(path[2])))
      return path.substr(0, 2);

    // UNIX root
    if (path[0] == '/') return path.substr(0, 1);

    // URL schema://
    if (path.size() > 3) {
      bool all_letters = IsLetter(path[0]);
      for (size_t i = 1; i < path.size() - 2; i++) {
        if (all_letters && path[i] == ':' && IsSeparator(path[i + 1]) &&
            IsSeparator(path[i + 1])) {
          return path.substr(0, i + 1);
        }
        all_letters &= IsLetter(path[i]);
        if (!all_letters) break;
      }
    }
  }

  return std::string();
}

PathComponents Parse(const std::string& path) {
  PathComponents res;

  res.root = GetRoot(path);
  res.extension = GetExtension(path);

  if (path.size() == 0) return res;

  size_t begin = res.root.size();

  // Skip tailing slashes
  size_t end = path.size() - 1;
  for (; end != 0; --end)
    if (!IsSeparator(path[end])) break;
  end -= res.extension.size() - 1;

  if (end > begin) {
    for (size_t i = begin; i < end; ++i) {
      const char c = path[i];
      if (begin == i && IsSeparator(c)) {
        begin = i + 1;
        continue;
      }

      const bool eol = ((i + 1) == end);
      if (IsSeparator(c) || eol) {
        if (eol)
          res.name_without_ext = path.substr(begin, i - begin + 1);
        else
          res.components.push_back(path.substr(begin, i - begin));
        begin = i + 1;
      }
    }
  }

  return res;
}

}  // namespace

namespace photonscore {
namespace path {

std::string Join(const std::string& path_a, const std::string& path_b) {
  auto a = Parse(path_a);
  auto b = Parse(path_b);
  auto& cc = a.components;

  if (a.name_without_ext.size() > 0 || a.extension.size() > 0) {
    cc.push_back(a.name_without_ext + a.extension);
  }

  if (b.root.size() > 1 && b.root[0] != '/') {
    cc.push_back(b.root);
  }

  for (const auto& component : b.components) {
    if (component.size() > 0) {
      cc.push_back(component);
    }
  }

  a.name_without_ext = b.name_without_ext;
  a.extension = b.extension;

  return a.ToString();
}

}  // namespace path
}  // namespace photonscore
