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

// Read the first N events of a .photons file using the native C++ API
// (photonscore/lib/d7). Expects the LINcam-style datasets /photons/{x,y,dt}.
// See read_n_events_c_api.cc for the same tool written against the C ABI.

#include "photonscore/lib/d7/d7.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

namespace d7 = photonscore::d7;

int main(int argc, char** argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s <filename.photons> <n>\n", argv[0]);
    return 1;
  }

  std::unique_ptr<d7::Vault> vault;
  auto status = d7::Open(argv[1], d7::OpenOptions::ForReading(), &vault);
  if (!status.ok()) {
    fprintf(stderr, "open fails: %s\n", status.message().c_str());
    return 1;
  }

  size_t n = std::atoi(argv[2]);
  std::vector<uint16_t> x(n), y(n), dt(n);

  status = vault->Read("/photons/x", 0, n, x.data());
  if (!status.ok()) {
    fprintf(stderr, "read x: %s\n", status.message().c_str());
    return 1;
  }
  status = vault->Read("/photons/y", 0, n, y.data());
  if (!status.ok()) {
    fprintf(stderr, "read y: %s\n", status.message().c_str());
    return 1;
  }
  status = vault->Read("/photons/dt", 0, n, dt.data());
  if (!status.ok()) {
    fprintf(stderr, "read dt: %s\n", status.message().c_str());
    return 1;
  }

  printf("X\tY\tDT\n");
  for (size_t i = 0; i < n; i++) {
    printf("%u\t%u\t%u\n", x[i], y[i], dt[i]);
  }

  return 0;
}
