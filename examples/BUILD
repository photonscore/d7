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

# C++ examples — link the native library (//photonscore/lib/d7) directly.

cc_binary(
  name = "write_demo",
  srcs = ["write_demo.cc"],
  deps = ["//photonscore/lib/d7"],
)

cc_binary(
  name = "list_metadata",
  srcs = ["list_metadata.cc"],
  deps = ["//photonscore/lib/d7"],
)

cc_binary(
  name = "read_n_events",
  srcs = ["read_n_events.cc"],
  deps = ["//photonscore/lib/d7"],
)

# C ABI examples — link the shared library through its cc_import.

cc_binary(
  name = "list_metadata_c_api",
  srcs = ["list_metadata_c_api.cc"],
  copts = ["-DPSCR_D7_IMPORT"],
  deps = ["//photonscore/lib/d7:photonscore_d7_import"],
  dynamic_deps = ["//photonscore/lib/d7:photonscore_d7"],
)

cc_binary(
  name = "read_n_events_c_api",
  srcs = ["read_n_events_c_api.cc"],
  copts = ["-DPSCR_D7_IMPORT"],
  deps = ["//photonscore/lib/d7:photonscore_d7_import"],
  dynamic_deps = ["//photonscore/lib/d7:photonscore_d7"],
)
