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

package(default_visibility = ["//visibility:public"])

# Platform config_settings consumed by select()s in //third_party and the
# core libraries. Keyed on --cpu (auto-detected from the host by default).

config_setting(
  name = "mac",
  values = {"cpu": "darwin_arm64"},
)

config_setting(
  name = "linux",
  values = {"cpu": "k8"},
)

config_setting(
  name = "linux_arm64",
  values = {"cpu": "aarch64"},
)

config_setting(
  name = "windows",
  values = {"cpu": "x64_windows"},
)

# vim:ft=bzl
