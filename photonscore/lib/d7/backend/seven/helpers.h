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

#ifndef PHOTONSCORE_LIB_D7_BACKEND_SEVEN_HELPERS_H
#define PHOTONSCORE_LIB_D7_BACKEND_SEVEN_HELPERS_H

#include "photonscore/lib/base/status.h"
#include "photonscore/lib/d7/backend/seven/seven.pb.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace photonscore {
namespace d7 {
namespace backend {
namespace seven {

int64_t Tell(FILE* file);
Status SeekCur(FILE* file, int64_t offset);
Status SeekBeg(FILE* file, int64_t offset);
Status SeekEnd(FILE* file, int64_t offset);

size_t EstimateEntrySize(const FileEntry& entry);
std::vector<std::string> SerializeLargeEntry(const FileEntry& entry);

}  // namespace seven
}  // namespace backend
}  // namespace d7
}  // namespace photonscore

#endif  // PHOTONSCORE_LIB_D7_BACKEND_SEVEN_HELPERS_H
