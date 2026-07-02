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

// Create a small LINcam-style .photons file using the native C++ API
// (photonscore/lib/d7): three uint16 datasets /photons/{x,y,dt} plus an
// attribute. The file it writes can be inspected with list_metadata and
// read back with read_n_events.

#include "photonscore/lib/d7/d7.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

namespace d7 = photonscore::d7;
using photonscore::Status;
using photonscore::base::TypeId;

static Status AddDataset(d7::Vault* vault, const std::string& name) {
  d7::DatasetInfo di;
  di.name = name;
  di.type = TypeId::kUInt16;
  di.rank = 1;  // 1-D stream; size[0] grows as events are appended
  return vault->CreateDataset(di);
}

int main(int argc, char** argv) {
  const char* path = (argc == 2) ? argv[1] : "sample.photons";

  std::unique_ptr<d7::Vault> vault;
  auto opts = d7::OpenOptions::ForWriting();  // create, read-write, "seven"
  opts.fail_if_exists = false;                // overwrite if it already exists
  auto status = d7::Open(path, opts, &vault);
  if (!status.ok()) {
    fprintf(stderr, "open fails: %s\n", status.message().c_str());
    return 1;
  }

  for (const char* name : {"/photons/x", "/photons/y", "/photons/dt"}) {
    status = AddDataset(vault.get(), name);
    if (!status.ok()) {
      fprintf(stderr, "create %s: %s\n", name, status.message().c_str());
      return 1;
    }
  }

  // Synthesize a handful of events.
  const size_t n = 8;
  std::vector<uint16_t> x(n), y(n), dt(n);
  for (size_t i = 0; i < n; i++) {
    x[i] = static_cast<uint16_t>(100 + i);
    y[i] = static_cast<uint16_t>(200 + 2 * i);
    dt[i] = static_cast<uint16_t>(i * i);
  }

  if (!(status = vault->Append("/photons/x", n, x.data())).ok() ||
      !(status = vault->Append("/photons/y", n, y.data())).ok() ||
      !(status = vault->Append("/photons/dt", n, dt.data())).ok()) {
    fprintf(stderr, "append: %s\n", status.message().c_str());
    return 1;
  }

  vault->set_attribute("instrument", "LINcam");

  status = vault->Close();  // flushes the global index + epilogue
  if (!status.ok()) {
    fprintf(stderr, "close: %s\n", status.message().c_str());
    return 1;
  }

  printf("wrote %s (%zu events)\n", path, n);
  return 0;
}
