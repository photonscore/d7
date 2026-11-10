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

#include <filesystem>
#include <gtest/gtest.h>

#include <random>

namespace fs = std::filesystem;

using photonscore::base::TypeId;
using photonscore::d7::DatasetInfo;
using photonscore::d7::OpenOptions;
using photonscore::d7::Vault;

TEST(SevenVaultTest, Test1) {
  Vault* v1;
  OpenOptions o1;
  o1.create_if_missing = true;
  o1.read_only = false;
  o1.backend = "seven";

  fs::remove("test1");

  auto s = photonscore::d7::Open("test1", o1, &v1);
  ASSERT_TRUE(s.ok());
  DatasetInfo di;
  di.name = "/d1";
  di.type = TypeId::kInt32;
  di.rank = 1;
  di.size[0] = 0;
  s = v1->CreateDataset(di);
  ASSERT_TRUE(s.ok());

  const int x[] = {1, 2, 3, 4, 5};
  s = v1->Append("/d1", 5, x);
  ASSERT_TRUE(s.ok());
  di = v1->dataset_info("/d1");
  ASSERT_EQ(1, di.rank);
  ASSERT_EQ(5, di.size[0]);
  delete v1;

  Vault* v2;
  OpenOptions o2;
  s = photonscore::d7::Open("test1", o2, &v2);
  ASSERT_TRUE(s.ok());
  di = v2->dataset_info("/d1");
  ASSERT_TRUE(di.valid());
  ASSERT_EQ(TypeId::kInt32, di.type);
  ASSERT_TRUE(di.name == "/d1");
  ASSERT_EQ(1, di.rank);
  ASSERT_EQ(5, di.size[0]);

  std::vector<int> xx;
  xx.resize(5);
  for (size_t i = 0; i < xx.size(); i++) {
    xx[i] = 1234500 + i;
  }

  s = v2->Read("/d1", 0, 5, xx.data());
  ASSERT_TRUE(s.ok());

  for (size_t i = 0; i < 5; i++) {
    EXPECT_EQ(x[i], xx[i]);
  }
  delete v2;
}

void AssertSizes(Vault* v, size_t n) {
  auto di = v->dataset_info("/i8");
  ASSERT_TRUE(di.valid());
  ASSERT_EQ(1, di.rank);
  ASSERT_EQ(TypeId::kInt8, di.type);
  EXPECT_EQ(n, di.size[0]);

  di = v->dataset_info("/i16");
  ASSERT_TRUE(di.valid());
  ASSERT_EQ(1, di.rank);
  ASSERT_EQ(TypeId::kInt16, di.type);
  EXPECT_EQ(n, di.size[0]);

  di = v->dataset_info("/i32");
  ASSERT_TRUE(di.valid());
  ASSERT_EQ(1, di.rank);
  ASSERT_EQ(TypeId::kInt32, di.type);
  EXPECT_EQ(n, di.size[0]);

  di = v->dataset_info("/i64");
  ASSERT_TRUE(di.valid());
  ASSERT_EQ(1, di.rank);
  ASSERT_EQ(TypeId::kInt64, di.type);
  EXPECT_EQ(n, di.size[0]);

  di = v->dataset_info("/u8");
  ASSERT_TRUE(di.valid());
  ASSERT_EQ(1, di.rank);
  ASSERT_EQ(TypeId::kUInt8, di.type);
  EXPECT_EQ(n, di.size[0]);

  di = v->dataset_info("/u16");
  ASSERT_TRUE(di.valid());
  ASSERT_EQ(1, di.rank);
  ASSERT_EQ(TypeId::kUInt16, di.type);
  EXPECT_EQ(n, di.size[0]);

  di = v->dataset_info("/u32");
  ASSERT_TRUE(di.valid());
  ASSERT_EQ(1, di.rank);
  ASSERT_EQ(TypeId::kUInt32, di.type);
  EXPECT_EQ(n, di.size[0]);

  di = v->dataset_info("/u64");
  ASSERT_TRUE(di.valid());
  ASSERT_EQ(1, di.rank);
  ASSERT_EQ(TypeId::kUInt64, di.type);
  EXPECT_EQ(n, di.size[0]);

  di = v->dataset_info("/f64");
  ASSERT_TRUE(di.valid());
  ASSERT_EQ(1, di.rank);
  ASSERT_EQ(TypeId::kDouble, di.type);
  EXPECT_EQ(n, di.size[0]);

  di = v->dataset_info("/f32");
  ASSERT_TRUE(di.valid());
  ASSERT_EQ(1, di.rank);
  ASSERT_EQ(TypeId::kSingle, di.type);
  EXPECT_EQ(n, di.size[0]);
}

TEST(SevenVaultTest, Large) {
  const size_t n = 300 * 1000;

  Vault* v1;
  OpenOptions o1;
  o1.create_if_missing = true;
  o1.read_only = false;
  o1.backend = "seven";

  fs::remove("large");
  auto s = photonscore::d7::Open("large", o1, &v1);
  ASSERT_TRUE(s.ok());

  std::vector<int8_t> i8;
  std::vector<int16_t> i16;
  std::vector<int32_t> i32;
  std::vector<int64_t> i64;
  std::vector<uint8_t> u8;
  std::vector<uint16_t> u16;
  std::vector<uint32_t> u32;
  std::vector<uint64_t> u64;
  std::vector<float> f32;
  std::vector<double> f64;

  std::mt19937_64 engine(123);
  std::uniform_real_distribution<double> rnd;
  for (size_t i = 0; i < n; i++) {
    i8.push_back(int8_t((rnd(engine) - 0.5) * 126));
    i16.push_back(int16_t((rnd(engine) - 0.5) * 12600));
    i32.push_back(int32_t((rnd(engine) - 0.5) * 1260000));
    i64.push_back(int64_t((rnd(engine) - 0.5) * 126000000));
    u8.push_back(uint8_t((rnd(engine) - 0.5) * 126));
    u16.push_back(uint16_t((rnd(engine) - 0.5) * 12600));
    u32.push_back(uint32_t((rnd(engine) - 0.5) * 1260000));
    u64.push_back(uint64_t((rnd(engine) - 0.5) * 126000000));
    f32.push_back(float(rnd(engine)));
    f64.push_back(rnd(engine));
  }

  DatasetInfo di;

  di.name = "/i8";
  di.rank = 1;
  di.type = TypeId::kInt8;
  ASSERT_TRUE(v1->CreateDataset(di).ok());

  di.name = "/i16";
  di.rank = 1;
  di.type = TypeId::kInt16;
  ASSERT_TRUE(v1->CreateDataset(di).ok());

  di.name = "/i32";
  di.rank = 1;
  di.type = TypeId::kInt32;
  ASSERT_TRUE(v1->CreateDataset(di).ok());

  di.name = "/i64";
  di.rank = 1;
  di.type = TypeId::kInt64;
  ASSERT_TRUE(v1->CreateDataset(di).ok());

  di.name = "/u8";
  di.rank = 1;
  di.type = TypeId::kUInt8;
  ASSERT_TRUE(v1->CreateDataset(di).ok());

  di.name = "/u16";
  di.rank = 1;
  di.type = TypeId::kUInt16;
  ASSERT_TRUE(v1->CreateDataset(di).ok());

  di.name = "/u32";
  di.rank = 1;
  di.type = TypeId::kUInt32;
  ASSERT_TRUE(v1->CreateDataset(di).ok());

  di.name = "/u64";
  di.rank = 1;
  di.type = TypeId::kUInt64;
  ASSERT_TRUE(v1->CreateDataset(di).ok());

  di.name = "/f32";
  di.rank = 1;
  di.type = TypeId::kSingle;
  ASSERT_TRUE(v1->CreateDataset(di).ok());

  di.name = "/f64";
  di.rank = 1;
  di.type = TypeId::kDouble;
  ASSERT_TRUE(v1->CreateDataset(di).ok());

  AssertSizes(v1, 0);

  for (size_t i = 0; i < 3; i++) {
    ASSERT_TRUE(v1->Append("/i8", i8.size(), i8.data()).ok());
    ASSERT_TRUE(v1->Append("/i16", i16.size(), i16.data()).ok());
    ASSERT_TRUE(v1->Append("/i32", i32.size(), i32.data()).ok());
    ASSERT_TRUE(v1->Append("/i64", i64.size(), i64.data()).ok());
    ASSERT_TRUE(v1->Append("/u8", u8.size(), u8.data()).ok());
    ASSERT_TRUE(v1->Append("/u16", u16.size(), u16.data()).ok());
    ASSERT_TRUE(v1->Append("/u32", u32.size(), u32.data()).ok());
    ASSERT_TRUE(v1->Append("/u64", u64.size(), u64.data()).ok());
    ASSERT_TRUE(v1->Append("/f32", f32.size(), f32.data()).ok());
    ASSERT_TRUE(v1->Append("/f64", f64.size(), f64.data()).ok());
    AssertSizes(v1, n * (i + 1));
  }

  delete v1;

  Vault* v2;
  OpenOptions o2;
  ASSERT_TRUE(photonscore::d7::Open("large", o2, &v2).ok());
  AssertSizes(v2, n * 3);

  std::vector<int8_t> ri8(3 * n);
  std::vector<int16_t> ri16(3 * n);
  std::vector<int32_t> ri32(3 * n);
  std::vector<int64_t> ri64(3 * n);
  std::vector<uint8_t> ru8(3 * n);
  std::vector<uint16_t> ru16(3 * n);
  std::vector<uint32_t> ru32(3 * n);
  std::vector<uint64_t> ru64(3 * n);
  std::vector<float> rf32(3 * n);
  std::vector<double> rf64(3 * n);

  for (size_t retry = 0; retry < 4; retry++) {
    ASSERT_TRUE(v2->Read("f64", 0, rf64.size(), rf64.data()).ok());
    ASSERT_TRUE(v2->Read("i64", 0, ri64.size(), ri64.data()).ok());
    ASSERT_TRUE(v2->Read("u64", 0, ru64.size(), ru64.data()).ok());
    ASSERT_TRUE(v2->Read("f32", 0, rf32.size(), rf32.data()).ok());
    ASSERT_TRUE(v2->Read("i32", 0, ri32.size(), ri32.data()).ok());
    ASSERT_TRUE(v2->Read("u32", 0, ru32.size(), ru32.data()).ok());
    ASSERT_TRUE(v2->Read("i16", 0, ri16.size(), ri16.data()).ok());
    ASSERT_TRUE(v2->Read("u16", 0, ru16.size(), ru16.data()).ok());
    ASSERT_TRUE(v2->Read("i8", 0, ri8.size(), ri8.data()).ok());
    ASSERT_TRUE(v2->Read("u8", 0, ru8.size(), ru8.data()).ok());

    for (size_t i = 0; i < n; i++) {
      ASSERT_EQ(i8[i], ri8[i]);
      ASSERT_EQ(i8[i], ri8[i + n]);
      ASSERT_EQ(i8[i], ri8[i + 2 * n]);
      ASSERT_EQ(i16[i], ri16[i]);
      ASSERT_EQ(i16[i], ri16[i + n]);
      ASSERT_EQ(i16[i], ri16[i + 2 * n]);
      ASSERT_EQ(i32[i], ri32[i]);
      ASSERT_EQ(i32[i], ri32[i + n]);
      ASSERT_EQ(i32[i], ri32[i + 2 * n]);
      ASSERT_EQ(i64[i], ri64[i]);
      ASSERT_EQ(i64[i], ri64[i + n]);
      ASSERT_EQ(i64[i], ri64[i + 2 * n]);
      ASSERT_EQ(u8[i], ru8[i]);
      ASSERT_EQ(u8[i], ru8[i + n]);
      ASSERT_EQ(u8[i], ru8[i + 2 * n]);
      ASSERT_EQ(u16[i], ru16[i]);
      ASSERT_EQ(u16[i], ru16[i + n]);
      ASSERT_EQ(u16[i], ru16[i + 2 * n]);
      ASSERT_EQ(u32[i], ru32[i]);
      ASSERT_EQ(u32[i], ru32[i + n]);
      ASSERT_EQ(u32[i], ru32[i + 2 * n]);
      ASSERT_EQ(f32[i], rf32[i]);
      ASSERT_EQ(f32[i], rf32[i + n]);
      ASSERT_EQ(f32[i], rf32[i + 2 * n]);
      ASSERT_EQ(u64[i], ru64[i]);
      ASSERT_EQ(u64[i], ru64[i + n]);
      ASSERT_EQ(u64[i], ru64[i + 2 * n]);
      ASSERT_EQ(f64[i], rf64[i]);
      ASSERT_EQ(f64[i], rf64[i + n]);
      ASSERT_EQ(f64[i], rf64[i + 2 * n]);
    }
  }

  delete v2;
}

void DoReadWrite(bool full) {
  const size_t chunk = 300 * 1000;
  size_t n = 3 * 1000 * 1000;
#ifdef NDEBUG
  n *= 10;
#endif

  std::unique_ptr<Vault> v1;
  OpenOptions o1;
  o1.create_if_missing = true;
  o1.read_only = false;
  o1.backend = "seven";

  fs::remove("huge");
  auto s = photonscore::d7::Open("huge", o1, &v1);
  ASSERT_TRUE(s.ok());

  std::vector<uint32_t> data(n);

  std::mt19937_64 engine(123);
  std::uniform_int_distribution<uint32_t> rnd(1, 1 << 21);
  for (size_t i = 0; i < n; i++) data[i] = rnd(engine);

  DatasetInfo di;
  di.name = "/data";
  di.rank = 1;
  di.type = TypeId::kUInt32;
  ASSERT_TRUE(v1->CreateDataset(di).ok());

  int64_t last_visible = 0;

  for (size_t offset = 0; offset < n; offset += chunk) {
    ASSERT_TRUE(v1->Append("/data", chunk, &data[offset]).ok());
    auto written = v1->dataset_info("/data").items();
    ASSERT_EQ(offset + chunk, written);

    std::unique_ptr<Vault> v2;
    OpenOptions o2;
    o2.backend_options = full ? "full" : "";
    ASSERT_TRUE(o2.read_only);
    s = photonscore::d7::Open("huge", o2, &v2);
    EXPECT_EQ(s.message(), std::string());
    ASSERT_TRUE(s.ok());
    auto d2 = v2->dataset_info("/data");
    ASSERT_EQ(di.name, d2.name);
    ASSERT_EQ(di.rank, d2.rank);
    ASSERT_EQ(di.type, d2.type);
    auto visible = d2.items();
    EXPECT_TRUE(di.valid());

    ASSERT_LE(visible, written);
    if (visible == last_visible) {
      continue;
    }

    std::cout << visible << " / " << written << " = "
              << double(visible) / written << (full ? " full" : "")
              << std::endl;

    std::vector<uint32_t> buffer(visible - last_visible);
    ASSERT_TRUE(
        v2->Read("/data", last_visible, buffer.size(), buffer.data()).ok());
    for (size_t i = 0; i < buffer.size(); i++) {
      if (data[i + last_visible] != buffer[i]) {
        std::cout << "FAIL @ " << i << std::endl;
        EXPECT_EQ(data[i], buffer[i]);
        break;
      }
    }
    last_visible = visible;
  }
}

TEST(SevenVaultTest, Huge) {
  DoReadWrite(false);
  DoReadWrite(true);
}
