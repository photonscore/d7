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

#ifndef PHOTONSCORE_LIB_BASE_SLICE_H
#define PHOTONSCORE_LIB_BASE_SLICE_H

#include <cassert>
#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>

namespace photonscore {
namespace base {

class Slice {
 public:
  Slice() : data_(nullptr), size_(0) {}
  Slice(const void* data, size_t data_size)
      : data_((const char*)data), size_(data_size) {}
  Slice(const std::string& str) : data_(str.data()), size_(str.size()) {}

  template <typename T>
  Slice(const std::vector<T>& vector)
      : data_((const char*)vector.data()), size_(sizeof(T) * vector.size()) {
    static_assert(std::is_pod<T>::value, "T must be a POD type.");
  }

  template <typename Iterator>
  Slice(Iterator first, Iterator last)
      : data_((const char*)&(*first)),
        size_(sizeof(*first) * std::distance(first, last)) {}

  const char* data() const { return data_; }

  size_t size() const { return size_; }

  const char& operator[](size_t i) const {
    assert(i < size_);
    return data_[i];
  }

  template <typename T>
  Slice Read(T* t) const {
    static_assert(std::is_pod<T>::value, "T must be a POD type.");
    assert(size_ >= sizeof(T) && "Slice is to small");
    *t = *((T*)data_);
    return Slice(data_ + sizeof(T), size_ - sizeof(T));
  }

  template <typename T>
  Slice ReadBigEndian(T* t) {
    Slice res = Read<T>(t);
    auto p = (char*)t;
    const size_t n = sizeof(T);
    for (size_t i = 0; i < n / 2; i++) {
      char tmp = p[i];
      p[i] = p[n - 1 - i];
      p[n - 1 - i] = tmp;
    }
    return res;
  }

  void operator+=(size_t n) {
    assert(n <= size_);
    data_ += n;
    size_ -= n;
  }

 private:
  const char* data_;
  size_t size_;
};

}  // namespace base
}  // namespace photonscore

#endif  // PHOTONSCORE_LIB_BASE_SLICE_H
