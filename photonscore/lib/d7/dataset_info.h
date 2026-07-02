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

#ifndef PHOTONSCORE_LIB_D7_DATASET_INFO_H
#define PHOTONSCORE_LIB_D7_DATASET_INFO_H

#include "photonscore/lib/base/type_id.h"

#include <cstdint>
#include <string>

namespace photonscore {
namespace d7 {

struct DatasetInfo {
  enum {
    kMaxRank = 3,
  };

  DatasetInfo() : type(base::TypeId::kUnknown), rank(0) {
    size[0] = size[1] = size[2] = 0;
  }

  std::string name;
  ::photonscore::base::TypeId type;
  size_t rank;
  int64_t size[3];

  bool valid() const { return 1 <= rank && rank <= kMaxRank; }
  size_t item_bytes() const { return item_size() * type_size(); }
  size_t item_size() const {
    if (rank == 1) return 1;
    if (rank == 2) return size[1];
    if (rank == 3) return size[1] * size[2];
    return 0;
  }
  int64_t items() const { return size[0]; }
  size_t type_size() const { return sizeof_type_id(type); }
};

}  // namespace d7
}  // namespace photonscore

#endif  // PHOTONSCORE_LIB_D7_DATASET_INFO_H
