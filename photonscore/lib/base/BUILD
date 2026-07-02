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

cc_library(
  name = "base",
  srcs = [
    "date_time.cc",
    "internal/date_time_constants.inc",
    "type_id.cc",
  ],
  hdrs = [
    "date_time.h",
    "slice.h",
    "status.h",
    "type_id.h",
  ],
  deps = [
    "//third_party/fmt",
  ],
  visibility = ["//visibility:public"],
  linkstatic = 1,
)
