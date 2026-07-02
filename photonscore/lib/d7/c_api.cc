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

#include "photonscore/lib/d7/c_api.h"
#include "photonscore/lib/d7/d7.h"

#include <algorithm>

using photonscore::Status;
using photonscore::base::TypeId;
using photonscore::d7::DatasetInfo;
using photonscore::d7::Open;
using photonscore::d7::OpenOptions;
using photonscore::d7::Vault;

namespace {

struct File {
  std::unique_ptr<Vault> vault_;

  int error_code_;
  std::string error_message_;

  std::vector<std::string> string_list_;
  std::vector<PscrD7StringList> c_string_list_;

  std::string di_name;
  PscrD7DatasetInfo c_di;

  std::string attr_value;

  void UpdateCStringList();
};

void File::UpdateCStringList() {
  c_string_list_.resize(string_list_.size() + 1);
  for (size_t i = 0; i < string_list_.size(); i++) {
    c_string_list_[i].next = &c_string_list_[i + 1];
    c_string_list_[i].value_length = string_list_[i].size();
    c_string_list_[i].value = string_list_[i].c_str();
  }
  c_string_list_.back().next = nullptr;
  c_string_list_.back().value_length = 0;
  c_string_list_.back().value = nullptr;
}

}  // namespace

PSCR_D7_API
int pscr_d7_open(const char* path, const PscrD7OpenOptions* co, PscrD7** file) {
  OpenOptions oo;
  if (co->backend) {
    oo.backend = co->backend;
  }
  if (co->backend_options) {
    oo.backend_options = co->backend_options;
  }

  oo.create_if_missing = co->create_if_missing;
  oo.fail_if_exists = co->fail_if_exists;
  oo.read_only = co->read_only;

  if (co->read_threads > 0) {
    oo.read_threads = co->read_threads;
  }

  auto f = new File();
  *file = (PscrD7*)f;

  auto s = Open(path, oo, &f->vault_);
  f->error_code_ = s.code();
  f->error_message_ = s.message();

  return f->error_code_;
}

PSCR_D7_API
int pscr_d7_close(PscrD7* file) {
  auto f = (File*)file;
  auto s = f->vault_->Close();
  f->error_code_ = s.code();
  f->error_message_ = s.message();
  return f->error_code_;
}

PSCR_D7_API
int pscr_d7_read(PscrD7* file, const char* dataset_name, int64_t offset,
                 int items, void* data) {
  auto f = (File*)file;
  auto s = f->vault_->Read(dataset_name, offset, items, data);
  f->error_code_ = s.code();
  f->error_message_ = s.message();
  return f->error_code_;
}

PSCR_D7_API
int pscr_d7_append(PscrD7* file, const char* dataset_name, int items,
                   const void* data) {
  auto f = (File*)file;
  auto s = f->vault_->Append(dataset_name, items, data);
  f->error_code_ = s.code();
  f->error_message_ = s.message();
  return f->error_code_;
}

PSCR_D7_API
int pscr_d7_create_dataset(PscrD7* file, const PscrD7DatasetInfo* info) {
  auto f = (File*)file;

  DatasetInfo di;
  di.name = info->name;
  di.rank = size_t(info->rank);
  di.size[0] = info->size[0];
  di.size[1] = info->size[1];
  di.size[2] = info->size[2];
  di.type = TypeId(info->type);

  auto s = f->vault_->CreateDataset(di);
  f->error_code_ = s.code();
  f->error_message_ = s.message();
  return f->error_code_;
}

PSCR_D7_API
int pscr_d7_dataset_names(PscrD7* file, PscrD7StringList** dataset_names) {
  auto f = (File*)file;
  auto dsi = f->vault_->datasets_info();

  f->string_list_.resize(0);
  for (auto& kv : dsi) {
    f->string_list_.push_back(kv.first);
  }

  f->UpdateCStringList();
  *dataset_names = f->c_string_list_.data();
  f->error_code_ = 0;
  f->error_message_.clear();
  return f->error_code_;
}

PSCR_D7_API
int pscr_d7_dataset_info(PscrD7* file, const char* dataset_name,
                         PscrD7DatasetInfo** info) {
  *info = nullptr;
  auto f = (File*)file;
  auto di = f->vault_->dataset_info(dataset_name);
  if (!di.valid()) {
    f->error_code_ = Status::kInvalidArgument;
    f->error_message_ = fmt::format("No such dataset '{0}'", dataset_name);
  } else {
    f->di_name = di.name;
    f->c_di.name = f->di_name.c_str();
    f->c_di.type = di.type;
    f->c_di.rank = di.rank;
    f->c_di.size[0] = di.size[0];
    f->c_di.size[1] = di.size[1];
    f->c_di.size[2] = di.size[2];
    *info = &f->c_di;
    f->error_code_ = 0;
    f->error_message_.clear();
  }
  return f->error_code_;
}

PSCR_D7_API
int pscr_d7_attribute_names(PscrD7* file, PscrD7StringList** attribute_names) {
  auto f = (File*)file;
  f->string_list_ = f->vault_->attribute_names();
  std::sort(f->string_list_.begin(), f->string_list_.end());
  f->UpdateCStringList();
  *attribute_names = f->c_string_list_.data();
  f->error_code_ = 0;
  f->error_message_.clear();
  return f->error_code_;
}

PSCR_D7_API
int pscr_d7_attribute(PscrD7* file, const char* attribute_name,
                      int* attribute_size, const char** attribute) {
  auto f = (File*)file;
  auto s = f->vault_->attribute(attribute_name, &f->attr_value);
  if (s.ok()) {
    *attribute_size = f->attr_value.size();
    *attribute = f->attr_value.c_str();
  }
  f->error_code_ = s.code();
  f->error_message_ = s.message();
  return f->error_code_;
}

PSCR_D7_API
int pscr_d7_set_attribute(PscrD7* file, const char* attribute_name,
                          int attribute_size, const char* attribute) {
  auto f = (File*)file;
  std::string attr_value(attribute, attribute_size);
  auto s = f->vault_->set_attribute(attribute_name, attr_value);
  f->error_code_ = s.code();
  f->error_message_ = s.message();
  return f->error_code_;
}

PSCR_D7_API
void pscr_d7_free(PscrD7* file) {
  pscr_d7_close(file);
  auto f = (File*)file;
  delete f;
}

PSCR_D7_API
const char* pscr_d7_last_error(PscrD7* file) {
  auto f = (File*)file;
  return f->error_message_.c_str();
}
