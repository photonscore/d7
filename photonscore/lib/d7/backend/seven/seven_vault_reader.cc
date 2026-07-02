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

#include "photonscore/lib/d7/backend/seven/seven_vault.h"
#include "photonscore/lib/d7/backend/seven/seven_vault_reader.h"
#include "photonscore/lib/path/path.h"

using photonscore::base::TypeCode;
using photonscore::base::TypeId;
using photonscore::path::Join;

namespace photonscore {
namespace d7 {
namespace backend {
namespace seven {

SevenVaultReader::SevenVaultReader(const std::string& path,
                                   const OpenOptions& options)
    : Vault(path, "seven", options) {}

SevenVaultReader::~SevenVaultReader() {}

Status SevenVaultReader::Init() {
  log_reader_.reset(new LogReader());
  auto status = log_reader_->Open(path_);
  if (!status.ok()) {
    return status;
  }

  size_t index_step;
  status = InitDatasets(&index_step);
  if (!status.ok()) {
    return status;
  }

  status = InitIndex(index_step);
  if (!status.ok()) {
    return status;
  }

  // Init dataset decoders
  for (auto& d : decoders_) {
    status = d.second->Init();
    if (!status.ok()) {
      return Status::IoError(
          "SevenVaultReader::Init for dataset {0}\n  {1}:{2}", d.first,
          status.code_string(), status.message());
    }
  }

  // Update dataset size[0]
  for (auto& kv : datasets_info_) {
    auto& name = kv.first;
    auto& di = kv.second;
    di.size[0] = decoders_[name]->total_bytes() / di.item_bytes();
  }

  return Status::OK();
}

Status SevenVaultReader::InitDatasets(size_t* index_step) {
  Status status;
  std::string buf;

  FileEntry entry;
  status = ReadEntryAtBlock(0, &entry);
  if (!status.ok()) {
    return status;
  }

  // Read and check header
  if (!entry.has_header()) {
    return Status::InvalidFormat("Expecting header as the first entry");
  }

  auto& header = entry.header();
  if (header.signatue() != SevenVault::kHeaderSignature) {
    return Status::InvalidFormat("Bad signature '{0}', expected '{1}'",
                                 header.signatue(),
                                 SevenVault::kHeaderSignature);
  }

  if (header.version_major() > SevenVault::kVersionMajor) {
    return Status::InvalidFormat("Future version detected '{0}' expected '{1}'",
                                 header.version_major(),
                                 int(SevenVault::kVersionMajor));
  }

  const uint32_t bytes_per_page = (header.bytes_per_page() != 0)
                                      ? header.bytes_per_page()
                                      : SevenVault::kBytesPerPage;

  // Load datasets info
  for (int i = 0; i < header.toc_size(); i++) {
    DatasetInfo di;
    auto& dsi = header.toc(i);
    di.name = dsi.name();
    di.rank = dsi.size_size();
    for (int j = 0; j < dsi.size_size(); j++) {
      di.size[j] = dsi.size(j);
    }

    switch (dsi.type()) {
      case TypeCode::INT8:
      case TypeCode::UINT8:
      case TypeCode::INT16:
      case TypeCode::UINT16:
      case TypeCode::INT32:
      case TypeCode::UINT32:
      case TypeCode::INT64:
      case TypeCode::UINT64:
      case TypeCode::SINGLE:
      case TypeCode::DOUBLE:
        di.type = (TypeId)dsi.type();
        break;
      default:
        return Status::InvalidFormat("Unsupported datatype of field '{0}'",
                                     di.name);
    }
    datasets_info_[di.name] = di;
    decoders_[di.name] = std::unique_ptr<VaultDecoder>(new VaultDecoder(
        bytes_per_page, log_reader_.get(), /* dataset_id */ i, di.type));
  }

  *index_step = header.index_step();
  return Status::OK();
}

Status SevenVaultReader::InitIndex(size_t index_step) {
  Status s;

  if (open_options_.backend_options.find("recover") != std::string::npos) {
    return InitIndexScatter(index_step);
  } else {
    s = InitIndexGlobal();
    if (!s.ok()) {
      return InitIndexScatter(index_step);
    }
  }
  return Status::OK();
}

Status SevenVaultReader::InitIndexGlobal() {
  // Load global index
  Status status;
  FileEntry entry;
  status = ReadEntryAtBlock(log_reader_->file_size_in_blocks(), &entry);
  if (!status.ok()) {
    return status;
  }

  if (!entry.has_epilogue()) {
    return Status::InvalidFormat("Epilogue not found H{0}D{1}I{2}E{3}",
                                 entry.has_header() ? "+" : "-",     // H
                                 entry.has_data() ? "+" : "-",       // D
                                 entry.has_index() ? "+" : "-",      // I
                                 entry.has_epilogue() ? "+" : "-");  // E
  }

  auto& epilogue = entry.epilogue();
  if (epilogue.signatue() != SevenVault::kEpilogueSignature) {
    return Status::IoError("Corupt epilogue");
  }

  bool found_global_index = false;

  int64_t offset = epilogue.global_index_offset();
  for (;;) {
    status = ReadEntryAtByte(offset, &entry, &offset);
    if (!status.ok()) {
      return status;
    }

    if (entry.has_data() || entry.has_header()) {
      return Status::InvalidFormat("Got data or header instead of index");
    }

    if (entry.has_epilogue()) {
      break;
    }

    assert(entry.has_index());

    auto decoders = DecodersById();
    status = ProcessIndexEntry(entry, &decoders);
    if (!status.ok()) {
      return status;
    }

    found_global_index |= entry.index().attributes().count(
                              SevenVault::kGlobalIndexAttribute) == 1;
  }

  if (!found_global_index) {
    return Status::Status::InvalidFormat("Global index marker not found");
  }

  return Status::OK();
}

Status SevenVaultReader::InitIndexScatter(size_t index_step) {
  Status status;
  int64_t total_blocks = log_reader_->file_size_in_blocks();
  auto decoders = DecodersById();

  int64_t offset = 0;
  // Read all index entries we can find.
  for (int64_t block = index_step; block < total_blocks; block += index_step) {
    FileEntry entry;
    status = ReadEntryAtBlock(block, &entry, &offset);
    if (!status.ok()) {
      break;
    }

    if (!entry.has_index()) {
      status = ReadEntryAtBlock(block + 1, &entry);
      if (!status.ok()) {
        break;
      }
    }

    if (!entry.has_index()) {
      return Status::InvalidFormat("Cannot find index at block {0}", block);
    }

    status = ProcessIndexEntry(entry, &decoders);
    if (!status.ok()) {
      return status;
    }
  }

  // Proceed reading everything until the first failure.
  if (open_options_.backend_options.find("full") != std::string::npos) {
    while (true) {
      FileEntry entry;
      int64_t new_offset;
      status = ReadEntryAtByte(offset, &entry, &new_offset);
      if (!status.ok()) {
        break;
      }

      if (!entry.has_data()) {
        offset = new_offset;
        continue;
      }

      auto id = entry.data().dataset_id();
      if (id >= decoders.size()) {
        break;
      }

      decoders[id]->AddPageOffset(offset);
      offset = new_offset;
    }
  }

  return Status::OK();
}

Status SevenVaultReader::Read(const std::string& dataset, uint64_t offset,
                              size_t items, void* buffer) {
  const Vault* vault = this;
  return vault->Read(dataset, offset, items, buffer);
}

Status SevenVaultReader::Read(const std::string& dataset, uint64_t offset,
                              size_t items, void* buffer) const {
  auto name = Join("/", dataset);
  auto& di = dataset_info(name);
  if (!di.valid()) {
    return Status::InvalidArgument("No such dataset '{0}'", name);
  }

  // Limit offset and items to 512Tb
  if (offset > 0x2000000000000UL) {
    return Status::InvalidArgument("Offet {0}", offset);
  }
  if (items > 0x2000000000000UL) {
    return Status::InvalidArgument("Items {0}", items);
  }

  if (di.size[0] < int64_t(offset + items)) {
    return Status::InvalidArgument("Read too large");
  }

  auto ib = di.item_bytes();

  const auto* decoder = decoders_.find(di.name)->second.get();
  return decoder->Decode(offset * ib, items * ib, buffer);
}

Status SevenVaultReader::ReadEntry(int64_t offset, bool is_block,
                                   FileEntry* entry,
                                   int64_t* new_offset) const {
  Status status;
  std::string buffer;
  if (is_block) {
    status = log_reader_->ReadBlock(offset, &buffer, new_offset);
  } else {
    status = log_reader_->Read(offset, &buffer, new_offset);
  }
  if (!status.ok()) {
    return status;
  }

  if (buffer.size() == 0) {
    return Status::InvalidFormat("Empty block");
  }

  entry->Clear();
  if (!entry->ParseFromString(buffer)) {
    return Status::InvalidFormat("Cannot parse entry");
  }

  return Status::OK();
}

Status SevenVaultReader::attribute(const std::string& name,
                                   std::string* value) {
  if (attributes_.count(name) == 0) {
    return Status::InvalidArgument("No such attribute '{0}'", name);
  }
  *value = attributes_[name];
  return Status::OK();
}

std::vector<VaultDecoder*> SevenVaultReader::DecodersById() {
  std::vector<VaultDecoder*> decoders;
  decoders.resize(decoders_.size());
  for (size_t i = 0; i < decoders.size(); i++) decoders[i] = nullptr;
  for (auto& kv : decoders_) {
    auto& decoder = kv.second;
    size_t i = decoder->dataset_id();
    assert(decoders[i] == nullptr && "Duplicate dataset_id");
    decoders[i] = decoder.get();
  }
  return decoders;
}

Status SevenVaultReader::ProcessIndexEntry(
    const FileEntry& file_entry, std::vector<VaultDecoder*>* decoders_p) {
  if (!file_entry.has_index()) {
    return Status::InvalidFormat(
        "Cannot find index in FileEntry H{0}D{1}I{2}E{3}",
        file_entry.has_header() ? "+" : "-",     // H
        file_entry.has_data() ? "+" : "-",       // D
        file_entry.has_index() ? "+" : "-",      // I
        file_entry.has_epilogue() ? "+" : "-");  // E
  }

  auto& decoders = *decoders_p;
  uint64_t file_size_in_bytes = log_reader_->file_size_in_bytes();

  // Feed decoders with pages
  auto& data_index = file_entry.index().data_info();
  int n = data_index.size();
  for (int i = 0; i < n; i++) {
    auto& ie = data_index.Get(i);
    if (ie.dataset_id() >= decoders.size()) {
      return Status::InvalidFormat("Unknown dataset_id {0}", ie.dataset_id());
    }
    if (file_size_in_bytes <= ie.offset()) {
      return Status::InvalidFormat("Invalid offset");
    }
    decoders[ie.dataset_id()]->AddPageOffset(ie.offset());
  }

  // Load attributes
  auto& attrs = file_entry.index().attributes();
  for (auto& kv : attrs) {
    // Skip dummy attribute
    if (kv.first == SevenVault::kGlobalIndexAttribute) {
      continue;
    }
    attributes_[kv.first] = kv.second;
    attribute_names_.push_back(kv.first);
  }

  return Status::OK();
}

}  // namespace seven
}  // namespace backend
}  // namespace d7
}  // namespace photonscore
