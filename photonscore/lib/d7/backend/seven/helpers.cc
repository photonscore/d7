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

#include "photonscore/lib/d7/backend/seven/helpers.h"
#include "photonscore/lib/d7/backend/seven/log_format.h"

namespace photonscore {
namespace d7 {
namespace backend {
namespace seven {

namespace {
Status Seek(FILE* file, int64_t offset, int origin) {
#ifdef _WIN32
  int error = _fseeki64(file, offset, origin);
#else
  int error = fseeko(file, offset, origin);
#endif
  return error == 0 ? Status::OK() : Status::IoError("fseek fails");
}
}  // namespace

int64_t Tell(FILE* file) {
#ifdef _WIN32
  int64_t offset = _ftelli64(file);
#else
  int64_t offset = ftello(file);
#endif
  return offset;
}

Status SeekCur(FILE* file, int64_t offset) {
  return Seek(file, offset, SEEK_CUR);
}

Status SeekBeg(FILE* file, int64_t offset) {
  return Seek(file, offset, SEEK_SET);
}

Status SeekEnd(FILE* file, int64_t offset) {
  return Seek(file, offset, SEEK_END);
}

size_t EstimateEntrySize(const FileEntry& entry) {
  size_t size = 0;

  if (entry.has_data()) {
    auto& data = entry.data();
    size += data.doubles_size() * sizeof(double);
    size += data.floats_size() * sizeof(float);
    size += data.integers_size() * sizeof(int32_t);
    size += data.longs_size() * sizeof(int64_t);
  }

  if (entry.has_index()) {
    auto& index = entry.index();
    size += index.data_info_size() * (sizeof(uint32_t) + sizeof(uint64_t));
    for (auto& kv : index.attributes()) {
      size += kv.first.size() + kv.second.size();
    }
  }

  return size;
}

std::vector<std::string> SerializeLargeEntry(const FileEntry& entry) {
  std::vector<std::string> res;
  size_t max_size = LogFormat::kMaxFragment * 3 / 4;

  // If estimate is less than 75% of maximal fragment, give it as it is.
  if (EstimateEntrySize(entry) < max_size) {
    res.push_back(entry.SerializeAsString());
    return res;
  }

  // Here we split index
  if (entry.has_index()) {
    FileEntry chunk;
    size_t chunk_size = 0;

    // First loop through attributes
    for (auto& kv : entry.index().attributes()) {
      size_t kv_size = kv.first.size() + kv.second.size();
      if ((chunk_size + kv_size) >= max_size) {
        res.push_back(chunk.SerializeAsString());
        chunk.Clear();
        chunk_size = 0;
      }
      auto& attrs = *chunk.mutable_index()->mutable_attributes();
      attrs[kv.first] = kv.second;
      chunk_size += kv_size;
    }
    if (chunk_size > 0) {
      res.push_back(chunk.SerializeAsString());
    }

    // Loop through data infos.
    chunk.Clear();
    chunk_size = 0;
    size_t data_size = entry.index().data_info_size();
    for (size_t i = 0; i < data_size; i++) {
      auto new_data = chunk.mutable_index()->add_data_info();
      auto& di = entry.index().data_info(i);
      new_data->set_dataset_id(di.dataset_id());
      new_data->set_offset(di.offset());
      chunk_size += sizeof(uint32_t) + sizeof(uint64_t);
      if (chunk_size >= max_size) {
        res.push_back(chunk.SerializeAsString());
        chunk.Clear();
        chunk_size = 0;
      }
    }
    if (chunk_size > 0) {
      res.push_back(chunk.SerializeAsString());
    }

    return res;
  }

  // Others are simple
  res.push_back(entry.SerializeAsString());
  return res;
}

}  // namespace seven
}  // namespace backend
}  // namespace d7
}  // namespace photonscore
