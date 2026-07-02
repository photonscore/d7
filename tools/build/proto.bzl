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

# Minimal C++ protobuf codegen for the standalone D7 repository.
#
# Compiles `.proto` files with the vendored `protoc`
# (`//third_party/protobuf:protoc`) into a `cc_library`. This is a trimmed
# extraction of zoo's `tools/build/grpc.bzl` — C++ only, no gRPC, no Java,
# no Python.

def cc_proto_library(
    name,
    srcs,
    deps = [],
    lib_deps = [],
    visibility = None):
  protoc_label = str(Label("//third_party/protobuf:protoc"))

  native.filegroup(
    name = name + "_cc_proto_files",
    srcs = srcs,
    visibility = visibility,
  )

  pb_h = [s[:-len(".proto")] + ".pb.h" for s in srcs]
  pb_cc = [s[:-len(".proto")] + ".pb.cc" for s in srcs]

  cmd_line = [
    "$(location " + protoc_label + ")",
    "  -I .",
    "  -I $(GENDIR)",
    "  -I third_party/protobuf/src",
    "  --cpp_out=$(GENDIR)",
  ]
  cmd_line += ["  $(locations " + s + ")" for s in srcs]

  native.genrule(
    name = name + "_cc_proto_codegen",
    srcs = srcs + [s + "_cc_proto_files" for s in deps] +
           ["//third_party/protobuf:well_known"],
    tools = [protoc_label],
    cmd = "\\\n".join(cmd_line),
    outs = pb_h + pb_cc,
    visibility = visibility,
  )

  native.cc_library(
    name = name,
    srcs = pb_cc,
    hdrs = pb_h,
    deps = lib_deps + deps + [str(Label("//third_party/protobuf"))],
    includes = ["."],
    linkstatic = 1,
    visibility = visibility,
  )
