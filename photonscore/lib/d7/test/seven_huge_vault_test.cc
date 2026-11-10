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

#include "photonscore/lib/d7/d7.h"

#include <fmt/format.h>
#include <gtest/gtest.h>
#include <filesystem>

#include <map>
#include <random>

namespace fs = std::filesystem;

using photonscore::base::TypeId;
using photonscore::d7::DatasetInfo;
using photonscore::d7::Open;
using photonscore::d7::OpenOptions;
using photonscore::d7::Vault;

TEST(SevenHugeVaultTest, Index) {
  std::string filename = "huge_file_t1.d7";
  fs::remove_all(filename);
  std::unique_ptr<Vault> vault;

  OpenOptions oo;
  oo.backend = "seven";
  oo.read_only = false;
  oo.create_if_missing = true;
  oo.fail_if_exists = true;

  auto s = Open(filename, oo, &vault);
  ASSERT_TRUE(s.ok());

  std::mt19937_64 mtx;
  mtx.seed(12345);
  std::uniform_int_distribution<uint64_t> rnd;

  std::map<std::string, std::string> copy;

  // Fill vault with tons of useless attributes to make global index huge.
  for (size_t i = 0; i < 200 * 1000; i++) {
    std::string key = fmt::format("{0:016X}{1:016x}", rnd(mtx), rnd(mtx));
    std::string value =
        fmt::format("{0:016x}{1:016x}{2:016X}{3:016x}{3:016X}", rnd(mtx),
                    rnd(mtx), rnd(mtx), rnd(mtx), rnd(mtx), rnd(mtx));
    s = vault->set_attribute(key, value);
    copy[key] = value;
    ASSERT_TRUE(s.ok());
  }

  s = vault->Close();
  ASSERT_TRUE(s.ok());
  delete vault.release();

  // Read them back
  oo.read_only = true;
  oo.create_if_missing = false;
  oo.fail_if_exists = false;
  s = Open(filename, oo, &vault);

  ASSERT_EQ(copy.size(), vault->attribute_names().size());

  ASSERT_TRUE(s.ok());
  for (auto& kv : copy) {
    std::string value;
    s = vault->attribute(kv.first, &value);
    ASSERT_TRUE(s.ok());
    EXPECT_EQ(kv.second, value);
  }
}

TEST(SevenHugeVaultTest, Dataset) {
  std::string filename = "huge_file_t2.d7";
  fs::remove_all(filename);
  std::unique_ptr<Vault> vault;

  OpenOptions oo;
  oo.backend = "seven";
  oo.read_only = false;
  oo.create_if_missing = true;
  oo.fail_if_exists = true;

  auto s = Open(filename, oo, &vault);
  ASSERT_TRUE(s.ok());

  DatasetInfo di;
  di.name = "/x";
  di.type = TypeId::kInt64;
  di.rank = 3;
  di.size[1] = 10 * 1000;
  di.size[2] = 10 * 1000;
  s = vault->CreateDataset(di);
  ASSERT_TRUE(s.ok());

  ASSERT_EQ(10000 * 10000, di.item_size());
  ASSERT_EQ(8 * 10000 * 10000, di.item_bytes());

  std::mt19937_64 mtx;
  mtx.seed(12345);
  std::uniform_int_distribution<uint64_t> rnd;

  std::vector<uint64_t> item;
  item.resize(di.item_size());

  for (size_t i = 0; i < item.size(); i++) {
    item[i] = rnd(mtx);
  }

  // Append super huge data item
  s = vault->Append("/x", 1, item.data());
  ASSERT_TRUE(s.ok());
}

