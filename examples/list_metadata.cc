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

// List the attributes and datasets of a .photons file using the native C++
// API (photonscore/lib/d7). See list_metadata_c_api.cc for the same tool
// written against the C ABI.

#include "photonscore/lib/d7/d7.h"

#include <cstdio>
#include <memory>
#include <string>

namespace d7 = photonscore::d7;
using photonscore::base::TypeId;

static const char* TypeName(TypeId t) {
  switch (t) {
    case TypeId::kInt8: return "int8";
    case TypeId::kInt16: return "int16";
    case TypeId::kInt32: return "int32";
    case TypeId::kInt64: return "int64";
    case TypeId::kUInt8: return "uint8";
    case TypeId::kUInt16: return "uint16";
    case TypeId::kUInt32: return "uint32";
    case TypeId::kUInt64: return "uint64";
    case TypeId::kSingle: return "float";
    case TypeId::kDouble: return "double";
    default: return "UNKNOWN";
  }
}

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <filename.photons>\n", argv[0]);
    return 1;
  }

  std::unique_ptr<d7::Vault> vault;
  auto status = d7::Open(argv[1], d7::OpenOptions::ForReading(), &vault);
  if (!status.ok()) {
    fprintf(stderr, "open fails: %s\n", status.message().c_str());
    return 1;
  }

  printf("ATTRIBUTES:\n");
  for (const auto& name : vault->attribute_names()) {
    std::string value;
    vault->attribute(name, &value);
    printf("  %s: %s\n", name.c_str(), value.c_str());
  }
  printf("\n");

  printf("DATASETS:\n");
  for (const auto& kv : vault->datasets_info()) {
    const d7::DatasetInfo& di = kv.second;
    printf("  %s: %s[", di.name.c_str(), TypeName(di.type));
    for (size_t i = 0; i < di.rank; i++) {
      printf("%lld", (long long)di.size[i]);
      if (i + 1 < di.rank) printf(", ");
    }
    printf("]\n");
  }
  printf("\n");

  return 0;
}
