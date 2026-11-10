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

# D7 for Linux and macOS: run `python3 configure.py` once, then `make`.
# GNU make 3.81 or later (macOS ships 3.81).

CXX ?= c++
CXXFLAGS ?= -O2
BUILD := build

UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
  SHARED_EXT := dylib
  SHARED_FLAGS := -dynamiclib -install_name @rpath/libphotonscore_d7.dylib
  RPATH := -Wl,-rpath,@loader_path
  LIBS :=
else
  SHARED_EXT := so
  SHARED_FLAGS := -shared -Wl,-soname,libphotonscore_d7.so
  RPATH := -Wl,-rpath,'$$ORIGIN'
  LIBS := -lpthread
endif

INCLUDES := -I. -Igen -isystem third_party/fmt \
  -isystem third_party/oneTBB/include -isystem third_party/protobuf/src
DEFINES := -D__TBB_NO_IMPLICIT_LINKAGE
ALL_FLAGS := -std=c++17 -fPIC -fvisibility=hidden -MMD -MP $(CXXFLAGS) \
  $(INCLUDES) $(DEFINES)

D7_DIRS := base fs mmap path d7 d7/backend d7/backend/seven d7/internal
D7_SRCS := $(filter-out photonscore/lib/d7/c_api.cc, \
  $(foreach d,$(D7_DIRS),$(wildcard photonscore/lib/$(d)/*.cc)))
GEN_SRCS := gen/photonscore/proto/base.pb.cc \
  gen/photonscore/lib/d7/backend/seven/seven.pb.cc
PB_DIRS := . io stubs util util/internal compiler
PB_SRCS := $(foreach d,$(PB_DIRS), \
  $(wildcard third_party/protobuf/src/google/protobuf/$(d)/*.cc))
TBB_SRCS := $(wildcard third_party/oneTBB/src/tbb/*.cpp)
FMT_SRCS := $(wildcard third_party/fmt/fmt/*.cc)

obj = $(patsubst %,$(BUILD)/obj/%.o,$(basename $(1)))
LIB_OBJS := $(call obj,$(D7_SRCS) $(GEN_SRCS) $(PB_SRCS) $(TBB_SRCS) \
  $(FMT_SRCS))
CAPI_OBJ := $(call obj,photonscore/lib/d7/c_api.cc)

STATIC := $(BUILD)/libd7.a
SHARED := $(BUILD)/libphotonscore_d7.$(SHARED_EXT)
EXAMPLES := write_demo list_metadata read_n_events
C_API_EXAMPLES := list_metadata_c_api read_n_events_c_api
BINS := $(addprefix $(BUILD)/,$(EXAMPLES) $(C_API_EXAMPLES))

.PHONY: all clean
all: $(STATIC) $(SHARED) $(BINS)

ifeq ($(wildcard $(GEN_SRCS))$(filter clean,$(MAKECMDGOALS)),)
  $(error gen/ is missing: run `python3 configure.py` first)
endif

$(STATIC): $(LIB_OBJS)
	@rm -f $@
	$(AR) rcs $@ $^

$(SHARED): $(CAPI_OBJ) $(STATIC)
	$(CXX) $(SHARED_FLAGS) -o $@ $^ $(LIBS)

$(addprefix $(BUILD)/,$(EXAMPLES)): $(BUILD)/%: $(BUILD)/obj/examples/%.o \
    $(STATIC)
	$(CXX) -o $@ $^ $(LIBS)

$(addprefix $(BUILD)/,$(C_API_EXAMPLES)): $(BUILD)/%: \
    $(BUILD)/obj/examples/%.o $(SHARED)
	$(CXX) -o $@ $< -L$(BUILD) -lphotonscore_d7 $(RPATH)

$(BUILD)/obj/examples/%_c_api.o: examples/%_c_api.cc
	@mkdir -p $(dir $@)
	$(CXX) $(ALL_FLAGS) -DPSCR_D7_IMPORT -c $< -o $@

$(CAPI_OBJ): photonscore/lib/d7/c_api.cc
	@mkdir -p $(dir $@)
	$(CXX) $(ALL_FLAGS) -DPSCR_D7_EXPORT -c $< -o $@

$(BUILD)/obj/third_party/protobuf/%.o: third_party/protobuf/%.cc
	@mkdir -p $(dir $@)
	$(CXX) $(ALL_FLAGS) -DHAVE_PTHREAD -w -c $< -o $@

$(BUILD)/obj/third_party/oneTBB/%.o: third_party/oneTBB/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(ALL_FLAGS) -D_XOPEN_SOURCE -D__TBB_BUILD \
	  -D__TBB_DYNAMIC_LOAD_ENABLED=0 -D__TBB_SOURCE_DIRECTLY_INCLUDED=1 \
	  -w -c $< -o $@

$(BUILD)/obj/third_party/fmt/%.o: third_party/fmt/%.cc
	@mkdir -p $(dir $@)
	$(CXX) $(ALL_FLAGS) -w -c $< -o $@

$(BUILD)/obj/%.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) $(ALL_FLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

-include $(LIB_OBJS:.o=.d) $(CAPI_OBJ:.o=.d)
