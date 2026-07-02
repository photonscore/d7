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

#include "photonscore/lib/base/type_id.h"

namespace photonscore {
namespace base {

TypeId TypeIdFromString(const std::string& str) {
  TypeId type = TypeId::kUnknown;

  switch (str.size()) {
    case 3:
      // bin
      if ((str[0] == 'b' || str[0] == 'B') &&
          (str[1] == 'i' || str[1] == 'I') &&
          (str[2] == 'n' || str[2] == 'N')) {
        type = TypeId::kString;
      }
      // txt
      else if ((str[0] == 't' || str[0] == 'T') &&
               (str[1] == 'x' || str[1] == 'X') &&
               (str[2] == 't' || str[2] == 'T')) {
        type = TypeId::kString;
      }
      break;

    case 4:
      // int8
      if ((str[0] == 'i' || str[0] == 'I') &&
          (str[1] == 'n' || str[1] == 'N') &&
          (str[2] == 't' || str[2] == 'T') && (str[3] == '8')) {
        type = TypeId::kInt8;
      }
      // uuid | guid
      else if ((str[0] == 'u' || str[0] == 'U' || str[0] == 'g' ||
                str[0] == 'G') &&
               (str[1] == 'u' || str[1] == 'U') &&
               (str[2] == 'i' || str[2] == 'I') &&
               (str[3] == 'd' || str[3] == 'D')) {
        type = TypeId::kUUID;
      }
      break;

    case 5:
      // int16
      // int32
      // int64
      if ((str[0] == 'i' || str[0] == 'I') &&
          (str[1] == 'n' || str[1] == 'N') &&
          (str[2] == 't' || str[2] == 'T')) {
        if (str[3] == '1' && str[4] == '6') {
          type = TypeId::kInt16;
        } else if (str[3] == '3' && str[4] == '2') {
          type = TypeId::kInt32;
        } else if (str[3] == '6' && str[4] == '4') {
          type = TypeId::kInt64;
        }
      }
      // uint8
      else if ((str[0] == 'u' || str[0] == 'U') &&
               (str[1] == 'i' || str[1] == 'I') &&
               (str[2] == 'n' || str[2] == 'N') &&
               (str[3] == 't' || str[3] == 'T') && (str[4] == '8')) {
        type = TypeId::kUInt8;
      }
      // float
      else if ((str[0] == 'f' || str[0] == 'F') &&
               (str[1] == 'l' || str[1] == 'L') &&
               (str[2] == 'o' || str[2] == 'O') &&
               (str[3] == 'a' || str[3] == 'A') &&
               (str[4] == 't' || str[4] == 'T')) {
        type = TypeId::kSingle;
      }
      break;

    case 6:
      // uint16
      // uint32
      // uint64
      if ((str[0] == 'u' || str[0] == 'U') &&
          (str[1] == 'i' || str[1] == 'I') &&
          (str[2] == 'n' || str[2] == 'N') &&
          (str[3] == 't' || str[3] == 'T')) {
        if (str[4] == '1' && str[5] == '6') {
          type = TypeId::kUInt16;
        } else if (str[4] == '3' && str[5] == '2') {
          type = TypeId::kUInt32;
        } else if (str[4] == '6' && str[5] == '4') {
          type = TypeId::kUInt64;
        }
      }
      // single
      else if ((str[0] == 's' || str[0] == 'S') &&
               (str[1] == 'i' || str[1] == 'I') &&
               (str[2] == 'n' || str[2] == 'N') &&
               (str[3] == 'g' || str[3] == 'G') &&
               (str[4] == 'l' || str[4] == 'L') &&
               (str[5] == 'e' || str[5] == 'E')) {
        type = TypeId::kSingle;
      }
      // double
      else if ((str[0] == 'd' || str[0] == 'D') &&
               (str[1] == 'o' || str[1] == 'O') &&
               (str[2] == 'u' || str[2] == 'U') &&
               (str[3] == 'b' || str[3] == 'B') &&
               (str[4] == 'l' || str[4] == 'L') &&
               (str[5] == 'e' || str[5] == 'E')) {
        type = TypeId::kDouble;
      }
      break;

    case 7:
      // float32
      // float64
      if ((str[0] == 'f' || str[0] == 'F') &&
          (str[1] == 'l' || str[1] == 'L') &&
          (str[2] == 'o' || str[2] == 'O') &&
          (str[3] == 'a' || str[3] == 'A') &&
          (str[4] == 't' || str[4] == 'T')) {
        if (str[5] == '3' && str[6] == '2') {
          type = TypeId::kSingle;
        } else if (str[5] == '6' && str[6] == '4') {
          type = TypeId::kDouble;
        }
      }
      break;

    default:
      break;
  }

  return type;
}

size_t sizeof_type_id(TypeId id) {
  switch (id) {
    case TypeId::kInt8:
    case TypeId::kUInt8:
      return 1;
    case TypeId::kInt16:
    case TypeId::kUInt16:
      return 2;
    case TypeId::kInt32:
    case TypeId::kUInt32:
    case TypeId::kSingle:
      return 4;
    case TypeId::kInt64:
    case TypeId::kUInt64:
    case TypeId::kDouble:
      return 8;
    case TypeId::kUUID:
      return 16;
    default:
      return 0;
  }
  return 0;
}

const std::string TypeIdToString(TypeId id) {
  switch (id) {
    case kString:
      return "txt";
    case kInt8:
      return "int8";
    case kInt16:
      return "int16";
    case kInt32:
      return "int32";
    case kInt64:
      return "int64";
    case kUInt8:
      return "uint8";
    case kUInt16:
      return "uint16";
    case kUInt32:
      return "uint32";
    case kUInt64:
      return "uint64";
    case kSingle:
      return "single";
    case kDouble:
      return "double";
    case kUUID:
      return "uuid";
    default:
      break;
  }
  return "";
}

}  // namespace base
}  // namespace photonscore
