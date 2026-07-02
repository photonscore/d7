# Copyright (c) 2026 Photonscore GmbH
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("//tools/build:proto.bzl", "cc_proto_library")

cc_proto_library(
  name = "seven_proto",
  srcs = ["backend/seven/seven.proto"],
  deps = ["//photonscore/proto:base"],
)

# The native .photons (D7 "seven") reader/writer. This is the only backend
# in this repository; the monorepo's legacy HDF5 / SQLite / bundle / QAdata
# backends are not included.
cc_library(
  name = "d7",
  srcs = [
    # D7
    "backend/seven.cc",
    "backend/seven.h",
    "backend/seven/helpers.cc",
    "backend/seven/helpers.h",
    "backend/seven/log_appender.cc",
    "backend/seven/log_appender.h",
    "backend/seven/log_format.h",
    "backend/seven/log_reader.cc",
    "backend/seven/log_reader.h",
    "backend/seven/paged_appender.h",
    "backend/seven/seven_vault.cc",
    "backend/seven/seven_vault.h",
    "backend/seven/seven_vault_reader.cc",
    "backend/seven/seven_vault_reader.h",
    "backend/seven/seven_vault_writer.cc",
    "backend/seven/seven_vault_writer.h",
    "backend/seven/vault_decoder.cc",
    "backend/seven/vault_decoder.h",
    "backend/seven/vault_encoder.cc",
    "backend/seven/vault_encoder.h",
    "backend/seven/writable_file.h",
    "backend/seven/writable_file_batch.cc",
    "backend/seven/writable_file_batch.h",
    "backend/seven/writable_file_impl.cc",
    "backend/seven/writable_file_impl.h",
    # Helpers
    "d7.cc",
    "open_options.cc",
    "vault.cc",
    # Internals
    "internal/pages_io_plan.cc",
    "internal/pages_io_plan.h",
  ],
  hdrs = [
    "d7.h",
    "dataset_info.h",
    "open_options.h",
    "vault.h",
  ],
  deps = [
    "//photonscore/lib/base",
    "//photonscore/lib/fs",
    "//photonscore/lib/mmap",
    "//photonscore/lib/path",
    "//third_party/oneTBB",
    ":seven_proto",
  ],
  linkstatic = 1,
  visibility = ["//visibility:public"],
)

# C ABI over the D7 library, plus a shared library and an import target for
# consuming it from other languages / dynamically.
cc_library(
  name = "c_api",
  srcs = ["c_api.cc"],
  hdrs = ["c_api.h"],
  copts = ["-DPSCR_D7_EXPORT"],
  deps = [":d7"],
  visibility = ["//visibility:public"],
  linkstatic = 1,
)

cc_shared_library(
  name = "photonscore_d7",
  deps = [":c_api"],
  visibility = ["//visibility:public"],
)

cc_import(
  name = "photonscore_d7_import",
  hdrs = ["c_api.h"],
  shared_library = ":photonscore_d7",
  visibility = ["//visibility:public"],
)
