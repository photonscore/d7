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
#include "photonscore/lib/d7/backend/seven/vault_decoder.h"
#include "photonscore/lib/d7/internal/pages_io_plan.h"

#include <oneapi/tbb/parallel_pipeline.h>
#include <oneapi/tbb/task_arena.h>

using photonscore::base::TypeId;
using photonscore::d7::internal::PagesIOPlan;

namespace photonscore {
namespace d7 {
namespace backend {
namespace seven {

namespace {

// Decoder into ProtoBuf diff form
template <typename T>
void CopyFloat(const T* src, size_t n, std::string* cache_line) {
  cache_line->resize(n * sizeof(T));
  std::memcpy(&cache_line->front(), src, cache_line->size());
}

void CopyBytes(const std::string& src, std::string* cache_line) {
  *cache_line = src;
}

template <typename T, typename SrcInt>
void CopyInt(const SrcInt* src, size_t n, int64_t seed,
             std::string* cache_line) {
  cache_line->resize((n + 1) * sizeof(T));
  auto dst = (T*)&cache_line->front();
  *dst = T(seed);
  dst++;
  for (size_t i = 0; i < n; i++, dst++) {
    *dst = src[i] + *(dst - 1);
  }
  assert(((char*)dst) == (cache_line->data() + cache_line->size()));
}

struct PipeData {
  std::string raw_entry;
  PagesIOPlan::Entry plan_entry;
  Status status;
};

}  // namespace

VaultDecoder::VaultDecoder(size_t bytes_per_page, LogReader* log_reader,
                           uint32_t dataset_id, TypeId type_id)
    : bytes_per_page_(bytes_per_page),
      log_reader_(log_reader),
      dataset_id_(dataset_id),
      type_id_(type_id) {}

VaultDecoder::~VaultDecoder() {}

Status VaultDecoder::Init() {
  Status status;
  total_bytes_ = 0;
  if (pages_offsets_.size() == 0) {
    return Status::OK();
  }

  auto n = pages_offsets_.size() - 1;
  std::string raw_entry;
  status = ReadRawEntry(n, &raw_entry);
  if (!status.ok()) {
    return status;
  }

  std::string page;
  status = DecodePage(n, raw_entry, &page);
  if (!status.ok()) {
    return Status::IoError("VaultDecoder.Init() DecodePage {0}:{1}",
                           status.code_string(), status.message());
  }

  total_bytes_ = n * bytes_per_page_ + page.size();
  return Status::OK();
}

Status VaultDecoder::Decode(int64_t offset, size_t length, void* buffer) const {
  PagesIOPlan io_plan(total_bytes_, bytes_per_page_, offset, length);
  std::atomic_size_t plan_step = 0;
  typedef std::unique_ptr<PipeData> PipeDataP;

  std::mutex faults_mutex;
  std::vector<Status> faults;

  auto read_filter = tbb::v1::make_filter<void, PipeDataP>(
      tbb::filter_mode::parallel, [&](tbb::flow_control& fc) {
        PipeDataP pd;
        size_t plan_step_i = plan_step++;
        if (plan_step_i >= io_plan.size()) {
          fc.stop();
          return pd;
        }
        pd = std::make_unique<PipeData>();
        pd->plan_entry = io_plan(plan_step_i);
        auto s = ReadRawEntry(pd->plan_entry.io_page_number, &pd->raw_entry);
        if (!s.ok()) {
          faults_mutex.lock();
          faults.push_back(s);
          faults_mutex.unlock();
          pd.reset();
          fc.stop();
        }
        return pd;
      });
  auto decode_filter = tbb::make_filter<PipeDataP, PipeDataP>(
      tbb::filter_mode::parallel, [&](PipeDataP pd) {
        std::string page;
        pd->status =
            DecodePage(pd->plan_entry.io_page_number, pd->raw_entry, &page);
        if (pd->status.ok()) {
          std::memcpy(((char*)buffer) + pd->plan_entry.destination,
                      &page.front() + pd->plan_entry.begin,
                      (pd->plan_entry.end - pd->plan_entry.begin));
        }
        return pd;
      });

  auto status_filter = tbb::make_filter<PipeDataP, void>(
      tbb::filter_mode::serial_out_of_order, [&](PipeDataP pd) {
        if (!pd->status.ok()) {
          faults_mutex.lock();
          faults.push_back(pd->status);
          faults_mutex.unlock();
        }
        pd.reset();
      });

  tbb::parallel_pipeline(tbb::this_task_arena::max_concurrency(),
                         read_filter & decode_filter & status_filter);

  if (faults.size() == 0) {
    return Status::OK();
  }

  std::string cummulative_error;
  cummulative_error.reserve(faults.size() * 100);
  for (size_t i = 0; i < faults.size(); i++) {
    cummulative_error +=
        fmt::format("  {0}) {1}: {2}\n", i + 1, faults[i].code_string(),
                    faults[i].message());
  }
  return Status::IoError("VaultDecoder::Decode offset={0:X} len={1}:\n{2}",
                         offset, length, cummulative_error);
}

Status VaultDecoder::ReadRawEntry(size_t entry_number,
                                  std::string* raw_entry) const {
  return log_reader_->Read(pages_offsets_[entry_number], raw_entry);
}

Status VaultDecoder::DecodePage(int64_t entry_number,
                                const std::string& raw_entry,
                                std::string* page) const {
  auto file_origin = pages_offsets_[entry_number];
  FileEntry entry;
  if (!entry.ParseFromString(raw_entry)) {
    return Status::InvalidFormat(
        "Error parsing FileEntry at 0x{0:X} of size 0x{1:X}", file_origin,
        raw_entry.size());
  }

  if (!entry.has_data()) {
    return Status::InvalidFormat("FileEntry has no data at {0}", file_origin);
  }

  if (entry.data().dataset_id() != dataset_id_) {
    return Status::InvalidFormat(
        "FileEntry at offset {0} has invalid dataset_id {1} "
        "(expected {2})",
        file_origin, entry.data().dataset_id(), dataset_id_);
  }

  if (type_id_ == TypeId::kSingle) {
    CopyFloat(entry.data().floats().data(), entry.data().floats().size(), page);
  } else if (type_id_ == TypeId::kDouble) {
    CopyFloat(entry.data().doubles().data(), entry.data().doubles().size(),
              page);
  } else if (type_id_ == TypeId::kInt8) {
    if (entry.data().raw_bytes().size() > 0) {
      CopyBytes(entry.data().raw_bytes(), page);
    } else {
      CopyInt<int8_t>(entry.data().integers().data(),  // src
                      entry.data().integers().size(),  // n
                      entry.data().seed(),             // src[0]
                      page);
    }
  } else if (type_id_ == TypeId::kInt16) {
    CopyInt<int16_t>(entry.data().integers().data(),  // src
                     entry.data().integers().size(),  // n
                     entry.data().seed(),             // src[0]
                     page);
  } else if (type_id_ == TypeId::kInt32) {
    CopyInt<int32_t>(entry.data().integers().data(),  // src
                     entry.data().integers().size(),  // n
                     entry.data().seed(),             // src[0]
                     page);
  } else if (type_id_ == TypeId::kInt64) {
    CopyInt<int64_t>(entry.data().longs().data(),  // src
                     entry.data().longs().size(),  // n
                     entry.data().seed(),          // src[0]
                     page);
  } else if (type_id_ == TypeId::kUInt8) {
    if (entry.data().raw_bytes().size() > 0) {
      CopyBytes(entry.data().raw_bytes(), page);
    } else {
      // Fallback to older format
      CopyInt<uint8_t>(entry.data().integers().data(),  // src
                       entry.data().integers().size(),  // n
                       entry.data().seed(),             // src[0]
                       page);
    }
  } else if (type_id_ == TypeId::kUInt16) {
    CopyInt<uint16_t>(entry.data().integers().data(),  // src
                      entry.data().integers().size(),  // n
                      entry.data().seed(),             // src[0]
                      page);
  } else if (type_id_ == TypeId::kUInt32) {
    CopyInt<uint32_t>(entry.data().integers().data(),  // src
                      entry.data().integers().size(),  // n
                      entry.data().seed(),             // src[0]
                      page);
  } else if (type_id_ == TypeId::kUInt64) {
    CopyInt<uint64_t>(entry.data().longs().data(),  // src
                      entry.data().longs().size(),  // n
                      entry.data().seed(),          // src[0]
                      page);
  } else {
    return Status::InvalidArgument("Unsupported datatype {0}",
                                   photonscore::base::TypeIdToString(type_id_));
  }

  return Status::OK();
}

}  // namespace seven
}  // namespace backend
}  // namespace d7
}  // namespace photonscore
