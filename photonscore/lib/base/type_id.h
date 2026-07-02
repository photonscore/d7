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

#ifndef PHOTONSCORE_LIB_BASE_TYPE_ID_H
#define PHOTONSCORE_LIB_BASE_TYPE_ID_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace photonscore {
namespace base {

enum TypeId {
  kUnknown = 100,
  kString = 11,

  kInt8 = 0,
  kInt16 = 1,
  kInt32 = 2,
  kInt64 = 3,

  kUInt8 = 4,
  kUInt16 = 5,
  kUInt32 = 6,
  kUInt64 = 7,

  kSingle = 8,
  kDouble = 9,

  kUUID = 14,
  kGUID = 14,
};

namespace internal {
template <typename CType>
struct TypeIdOf {
  enum { type_id = kUnknown };
};
template <>
struct TypeIdOf<int8_t> {
  enum { type_id = kInt8 };
};
template <>
struct TypeIdOf<int16_t> {
  enum { type_id = kInt16 };
};
template <>
struct TypeIdOf<int32_t> {
  enum { type_id = kInt32 };
};
template <>
struct TypeIdOf<int64_t> {
  enum { type_id = kInt64 };
};
template <>
struct TypeIdOf<uint8_t> {
  enum { type_id = kUInt8 };
};
template <>
struct TypeIdOf<uint16_t> {
  enum { type_id = kUInt16 };
};
template <>
struct TypeIdOf<uint32_t> {
  enum { type_id = kUInt32 };
};
template <>
struct TypeIdOf<uint64_t> {
  enum { type_id = kUInt64 };
};
template <>
struct TypeIdOf<float> {
  enum { type_id = kSingle };
};
template <>
struct TypeIdOf<double> {
  enum { type_id = kDouble };
};
}  // namespace internal

template <typename T>
TypeId type_id() {
  return (TypeId)internal::TypeIdOf<T>::type_id;
}

size_t sizeof_type_id(TypeId id);
TypeId TypeIdFromString(const std::string& str);
const std::string TypeIdToString(TypeId id);

}  // namespace base
}  // namespace photonscore

#endif  // PHOTONSCORE_LIB_BASE_TYPE_ID_H
