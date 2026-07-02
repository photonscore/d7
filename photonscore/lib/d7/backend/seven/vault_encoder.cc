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

#include "photonscore/lib/d7/backend/seven/seven.pb.h"
#include "photonscore/lib/d7/backend/seven/vault_encoder.h"

#include <cassert>

using photonscore::base::TypeId;

namespace photonscore {
namespace d7 {
namespace backend {
namespace seven {

namespace {
// Encoder into ProtoBuf diff form
template <typename T, typename Dst>
void CopyFloat(const void* page, size_t size, Dst* dst) {
  auto src = (const T*)page;
  size_t n = size / sizeof(*src);
  assert(size == (n * sizeof(*src)) && "CopyFloat");
  static_assert(sizeof(*src) == sizeof(dst->Get(0)), "Type mismatch");
  dst->Reserve(n);
  for (size_t i = 0; i < n; i++) dst->Add(src[i]);
}

template <typename T>
void CopyBytes(const void* page, size_t size, std::string* dst) {
  dst->resize(size);
  auto src = (const char*)page;
  std::copy(src, src + size, dst->begin());
}

template <typename T, typename Dst>
void CopyInt(const void* page, size_t size, Dst* data) {
  auto src = (const T*)page;
  size_t n = size / sizeof(*src);
  assert(size == (n * sizeof(*src)) && "CopyInt");
  data->set_seed(src[0]);
  auto dst = data->mutable_integers();
  dst->Reserve(n - 1);
  for (size_t i = 1; i < n; i++) dst->Add(src[i] - src[i - 1]);
}

template <typename T>
void CopyLong(const void* page, size_t size, Data* data) {
  auto src = (const T*)page;
  size_t n = size / sizeof(*src);
  assert(size == (n * sizeof(*src)) && "CopyLong");
  data->set_seed(src[0]);
  auto dst = data->mutable_longs();
  dst->Reserve(n - 1);
  for (size_t i = 1; i < n; i++) dst->Add(src[i] - src[i - 1]);
}

}  // namespace

VaultEncoder::VaultEncoder(size_t bytes_per_page, LogAppender* log_appender,
                           uint32_t dataset_id, TypeId type_id)
    : bytes_per_page_(bytes_per_page),
      log_appender_(log_appender),
      dataset_id_(dataset_id),
      type_id_(type_id),
      paged_appender_(this) {}

VaultEncoder::~VaultEncoder() {
  auto status = Close();
  assert(status.ok() && "Exception in destructor");
}

Status VaultEncoder::Init() { return paged_appender_.Init(); }

Status VaultEncoder::Close() { return paged_appender_.Close(); }

Status VaultEncoder::Encode(const void* data, size_t len) {
  return paged_appender_.Append(data, len);
}

std::string VaultEncoder::SerializeChunk(const char* chunk, size_t n) const {
  FileEntry entry;
  auto data = entry.mutable_data();
  data->set_dataset_id(dataset_id_);

  if (type_id_ == TypeId::kSingle) {
    CopyFloat<float>(chunk, n, data->mutable_floats());
  } else if (type_id_ == TypeId::kDouble) {
    CopyFloat<double>(chunk, n, data->mutable_doubles());
  } else if (type_id_ == TypeId::kInt8) {
    CopyBytes<int8_t>(chunk, n, data->mutable_raw_bytes());
  } else if (type_id_ == TypeId::kInt16) {
    CopyInt<int16_t>(chunk, n, data);
  } else if (type_id_ == TypeId::kInt32) {
    CopyInt<int32_t>(chunk, n, data);
  } else if (type_id_ == TypeId::kUInt8) {
    CopyBytes<uint8_t>(chunk, n, data->mutable_raw_bytes());
  } else if (type_id_ == TypeId::kUInt16) {
    CopyInt<uint16_t>(chunk, n, data);
  } else if (type_id_ == TypeId::kUInt32) {
    CopyInt<uint32_t>(chunk, n, data);
  } else if (type_id_ == TypeId::kInt64) {
    CopyLong<int64_t>(chunk, n, data);
  } else if (type_id_ == TypeId::kUInt64) {
    CopyLong<uint64_t>(chunk, n, data);
  }

  return entry.SerializeAsString();
}

Status VaultEncoder::AppendPage(const std::string& page) {
  auto serialized = SerializeChunk(page.c_str(), page.size());
  auto status = log_appender_->Append(serialized);
  if (!status.ok()) {
    return status;
  }

  return Status::OK();
}

}  // namespace seven
}  // namespace backend
}  // namespace d7
}  // namespace photonscore
