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
#include "photonscore/lib/d7/backend/seven/seven_vault.h"
#include "photonscore/lib/d7/backend/seven/seven_vault_writer.h"
#include "photonscore/lib/d7/backend/seven/writable_file_impl.h"
#include "photonscore/lib/path/path.h"

#include <oneapi/tbb/parallel_for.h>

#include <algorithm>

using photonscore::base::TypeCode;
using photonscore::base::TypeId;
using photonscore::path::Join;

namespace photonscore {
namespace d7 {
namespace backend {
namespace seven {

SevenVaultWriter::SevenVaultWriter(const std::string& path,
                                   const OpenOptions& options)
    : Vault(path, "seven", options), header_written_(false) {}

SevenVaultWriter::~SevenVaultWriter() {
  auto status = Close();
  assert(status.ok() && "Failure in desctructor");
}

Status SevenVaultWriter::Close() {
  if (closed_) {
    return Status::OK();
  }
  Vault::Close();

  Status status;

  status = EnsureHeaderWritten();
  if (!status.ok()) {
    return status;
  }

  for (const auto& kv : encoders_) {
    auto encoder = kv.second.get();
    auto bytes_before = log_appender_->file_size_in_bytes();
    auto blocks_before = log_appender_->file_size_in_blocks();
    status = encoder->Close();
    if (!status.ok()) {
      return status;
    }
    auto bytes_after = log_appender_->file_size_in_bytes();
    auto blocks_after = log_appender_->file_size_in_blocks();
    status = FlushIndex(encoder, bytes_before, blocks_before, bytes_after,
                        blocks_after);
    if (!status.ok()) {
      return status;
    }
  }

  // Save global index
  std::string buffer;
  int64_t global_index_offset = log_appender_->file_size_in_bytes();
  // Set dummy attribute that so it serializes into non-empty string.
  auto& gattrs = *global_index_.mutable_index()->mutable_attributes();
  gattrs[SevenVault::kGlobalIndexAttribute] = SevenVault::kGlobalIndexAttribute;
  auto buffers = SerializeLargeEntry(global_index_);
  for (auto& buf : buffers) {
    status = log_appender_->Append(buf);
    if (!status.ok()) {
      return status;
    }
  }

  // Fill with zeros up to the next file block
  status = log_appender_->PadToBlock();
  if (!status.ok()) {
    return status;
  }

  // Write the epilogue into the fresh block
  FileEntry epilogue_entry;
  auto epilogue = epilogue_entry.mutable_epilogue();
  epilogue->set_global_index_offset(global_index_offset);
  epilogue->set_signatue(SevenVault::kEpilogueSignature);
  buffer = epilogue_entry.SerializeAsString();
  status = log_appender_->Append(buffer);
  if (!status.ok()) {
    return status;
  }

  log_appender_->Close();
  return Status::OK();
}

Status SevenVaultWriter::Flush() { return log_appender_->Flush(); }

Status SevenVaultWriter::Init() {
  auto h = header_.mutable_header();
  h->set_signatue(SevenVault::kHeaderSignature);
  h->set_version_major(SevenVault::kVersionMajor);
  h->set_version_minor(SevenVault::kVersionMinor);
  h->set_version_patch(SevenVault::kVersionPatch);
  h->set_index_step(SevenVault::kIndexStep);

  std::unique_ptr<WritableFile> wf;
  Status status = WritableFileImpl::CreateNew(path_, &wf);
  if (!status.ok()) {
    Vault::Close();
    return status;
  }
  log_appender_.reset(new LogAppender(std::move(wf)));
  return Status::OK();
}

Status SevenVaultWriter::CreateDataset(const DatasetInfo& dataset) {
  if (header_written_) {
    return Status::Unimplemented(
        "Cannot create new dataset after the data are appended");
  }

  if (!dataset.valid()) {
    return Status::InvalidArgument("DatasetInfo is not valid");
  }

  DatasetInfo di = dataset;
  di.size[0] = 0;

  // Normalize dataset name
  di.name = Join("/", di.name);

  if (datasets_info_.count(di.name)) {
    return Status::AlreadyExists("{0}", di.name);
  }

  // Convert to protobuf enum
  switch (di.type) {
    case TypeId::kInt8:
    case TypeId::kUInt8:
    case TypeId::kInt16:
    case TypeId::kUInt16:
    case TypeId::kInt32:
    case TypeId::kUInt32:
    case TypeId::kInt64:
    case TypeId::kUInt64:
    case TypeId::kSingle:
    case TypeId::kDouble:
      break;
    default:
      return Status::InvalidArgument(
          "Unsupported data type: {0}",
          photonscore::base::TypeIdToString(di.type));
  }

  auto hdi = header_.mutable_header()->add_toc();
  hdi->set_name(di.name);
  hdi->set_type(TypeCode(di.type));
  for (size_t i = 0; i < di.rank; i++) {
    hdi->mutable_size()->Add(i == 0 ? 0 : di.size[i]);
  }
  const uint32_t dataset_id = header_.header().toc_size() - 1;
  encoders_[di.name] = std::unique_ptr<VaultEncoder>(new VaultEncoder(
      SevenVault::kBytesPerPage, log_appender_.get(), dataset_id, di.type));

  datasets_info_[di.name] = di;

  return Status::OK();
}

Status SevenVaultWriter::Append(const std::string& dataset, size_t items,
                                const void* buffer) {
  if (closed_) {
    return Status::Internal("Append({0}) to closed file '{1}'", dataset,
                            path());
  }

  Status status;
  auto name = Join("/", dataset);
  if (datasets_info_.count(name) == 0) {
    return Status::InvalidArgument("No such dataset '{0}'", name);
  }
  auto& di = datasets_info_[name];

  status = EnsureHeaderWritten();
  if (!status.ok()) {
    return status;
  }

  auto encoder = encoders_[name].get();
  size_t bytes = items * di.item_bytes();
  auto ptr = (const char*)buffer;
  const size_t bytes_per_page = encoder->bytes_per_page();

  if (bytes > (32 * bytes_per_page)) {
    // Large batch
    status = log_appender_->BeginBatch(bytes + bytes / 10);
    assert(status.ok());

    // First, append until the page boundary
    size_t page_remaining =
        bytes_per_page - (encoder->total_bytes() % bytes_per_page);
    page_remaining = std::min(page_remaining, bytes);
    if (page_remaining > 0) {
      status = Append(encoder, ptr, page_remaining);
      if (!status.ok()) return status;
      ptr += page_remaining;
      bytes -= page_remaining;
      assert(bytes == 0 ? 1 : ((encoder->total_bytes() % bytes_per_page) == 0));
    }

    // Second, append full pages with parallel encoding
    size_t full_pages = bytes / bytes_per_page;
    status = AppendParallel(encoder, ptr, full_pages);
    if (!status.ok()) return status;

    // Third, append trailing bytes
    ptr += full_pages * bytes_per_page;
    bytes -= full_pages * bytes_per_page;
    status = Append(encoder, ptr, bytes);
    if (!status.ok()) return status;

    // Flush to the file
    status = log_appender_->CommitBatch();
    if (!status.ok()) return status;
  } else {
    // Small writes
    status = Append(encoder, ptr, bytes);
    if (!status.ok()) return status;
  }
  di.size[0] += items;
  assert(di.size[0] == int64_t(encoder->total_bytes() / di.item_bytes()));
  return Status::OK();
}

Status SevenVaultWriter::Append(VaultEncoder* encoder, const char* ptr,
                                size_t bytes) {
  Status status;
  // Do not write more than a single page of bytes because we cannot miss a
  // log_appender_->Append() call to build index properly.
  while (bytes != 0) {
    size_t chunk = std::min(encoder->bytes_per_page(), bytes);
    auto bytes_before = log_appender_->file_size_in_bytes();
    auto blocks_before = log_appender_->file_size_in_blocks();
    status = encoder->Encode(ptr, chunk);
    if (!status.ok()) {
      return status;
    }
    auto bytes_after = log_appender_->file_size_in_bytes();
    auto blocks_after = log_appender_->file_size_in_blocks();

    status = FlushIndex(encoder, bytes_before, blocks_before, bytes_after,
                        blocks_after);
    if (!status.ok()) {
      return status;
    }
    ptr += chunk;
    bytes -= chunk;
  }
  return status;
}

Status SevenVaultWriter::AppendParallel(VaultEncoder* encoder, const char* ptr,
                                        size_t pages) {
  std::vector<std::string> encoded;
  encoded.resize(pages);

  const size_t bytes_per_page = encoder->bytes_per_page();

  tbb::parallel_for(size_t(0), pages, [&](size_t page) {
    if (page >= pages) return;
    auto data = ptr + bytes_per_page * page;
    assert(data < ptr + pages * bytes_per_page);
    encoded[page] = encoder->SerializeChunk(data, bytes_per_page);
  });

  for (size_t i = 0; i < encoded.size(); i++) {
    auto bytes_before = log_appender_->file_size_in_bytes();
    auto blocks_before = log_appender_->file_size_in_blocks();

    auto status = log_appender_->Append(encoded[i]);
    if (!status.ok()) return status;
    encoder->StepPage();

    auto bytes_after = log_appender_->file_size_in_bytes();
    auto blocks_after = log_appender_->file_size_in_blocks();

    status = FlushIndex(encoder, bytes_before, blocks_before, bytes_after,
                        blocks_after);
    if (!status.ok()) return status;
  }

  return Status::OK();
}

Status SevenVaultWriter::set_attribute(const std::string& name,
                                       const std::string& value) {
  if ((name.size() + value.size()) > (LogFormat::kMaxFragment / 2)) {
    return Status::InvalidArgument(
        "Argument too size(key + vaule) = {0} (expected max {1} bytes)",
        name.size() + value.size(), LogFormat::kMaxFragment / 2);
  }

  auto& a = *running_index_.mutable_index()->mutable_attributes();
  a[name] = value;

  auto& g = *global_index_.mutable_index()->mutable_attributes();
  g[name] = value;

  if (g.count(name) == 0) {
    attribute_names_.push_back(name);
  }
  g[name] = value;

  return Status::OK();
}

Status SevenVaultWriter::attribute(const std::string& name,
                                   std::string* value) {
  auto& g = global_index_.index().attributes();
  if (g.count(name) != 0) {
    *value = g.at(name);
    return Status::OK();
  }
  return Status::InvalidArgument("No such attribute '{0}'", name);
}

Status SevenVaultWriter::EnsureHeaderWritten() {
  Status status;
  if (!header_written_) {
    auto buf = header_.SerializeAsString();
    status = log_appender_->Append(buf);
    if (!status.ok()) {
      return status;
    }

    for (auto& kv : encoders_) {
      status = kv.second->Init();
      if (!status.ok()) {
        return status;
      }
    }
  }

  header_written_ = true;
  return Status::OK();
}

Status SevenVaultWriter::FlushIndex(const VaultEncoder* encoder,
                                    int64_t bytes_before, int64_t blocks_before,
                                    int64_t bytes_after, int64_t blocks_after) {
  // Register stored block if any
  if (bytes_before != bytes_after) {
    auto ri = running_index_.mutable_index()->add_data_info();
    auto gi = global_index_.mutable_index()->add_data_info();
    ri->set_dataset_id(encoder->dataset_id());
    gi->set_dataset_id(encoder->dataset_id());
    ri->set_offset(bytes_before);
    gi->set_offset(bytes_before);

    // Optionally flush the index
    blocks_before /= header_.header().index_step();
    blocks_after /= header_.header().index_step();
    if (blocks_before != blocks_after) {
      auto serialized_ix = running_index_.SerializeAsString();
      running_index_.Clear();
      auto status = log_appender_->Append(serialized_ix);
      if (!status.ok()) {
        return status;
      }
    }
  }

  return Status::OK();
}

}  // namespace seven
}  // namespace backend
}  // namespace d7
}  // namespace photonscore
