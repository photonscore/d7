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

#include "photonscore/lib/d7/backend/seven/log_appender.h"
#include "photonscore/lib/d7/backend/seven/log_format.h"
#include "photonscore/lib/d7/backend/seven/log_reader.h"
#include "photonscore/lib/d7/backend/seven/writable_file_impl.h"

#include <filesystem>
#include <gtest/gtest.h>

#include <random>

namespace fs = std::filesystem;

using photonscore::d7::backend::seven::LogAppender;
using photonscore::d7::backend::seven::LogFormat;
using photonscore::d7::backend::seven::LogReader;
using photonscore::d7::backend::seven::WritableFile;
using photonscore::d7::backend::seven::WritableFileImpl;

TEST(LogRdWrTest, Simple) {
  const std::string filename = "test_rdwr.log";

  std::mt19937_64 rng(12345);
  std::uniform_int_distribution<int> rnd(10, 100);
  std::vector<std::string> data;

  // This string leaves two bytes available in the block
  std::string two_left;
  two_left.resize(LogFormat::kBlockSize - LogFormat::kHeaderSize - 2);
  for (size_t i = 0; i < two_left.size(); i++) two_left[i] = '2';
  data.push_back(two_left);

  // This leaves one byte
  std::string one_left;
  one_left.resize(LogFormat::kBlockSize - LogFormat::kHeaderSize - 1);
  for (size_t i = 0; i < two_left.size(); i++) one_left[i] = '1';
  data.push_back(one_left);

  // Generate random data
  for (int i = 0; i < 300; i++) {
    int n = rnd(rng) * rnd(rng);
    std::string chunk;
    chunk.reserve(n);
    for (int j = 0; j < n; j++) {
      chunk.push_back(rnd(rng));
    }
    data.push_back(std::move(chunk));
  }

  std::unique_ptr<WritableFile> wf;
  fs::remove_all(filename);
  auto status = WritableFileImpl::CreateNew(filename, &wf);
  ASSERT_TRUE(status.ok());
  LogAppender appender(std::move(wf));
  for (auto& chunk : data) {
    ASSERT_TRUE(appender.Append(chunk).ok());
  }
  ASSERT_TRUE(appender.Close().ok());
  const size_t expected_file_size = appender.file_size_in_bytes();

  // Manualy check two first blocks
  uint8_t buf[LogFormat::kBlockSize * 2];
  FILE* f = fopen(filename.c_str(), "rb");
  ASSERT_NE(nullptr, f);
  ASSERT_EQ(sizeof(buf), fread(buf, 1, sizeof(buf), f));
  fclose(f);

  uint16_t b1_hdr = (uint16_t(buf[1]) << 8) | buf[0];
  EXPECT_EQ(
      LogFormat::kFragmentBegin | LogFormat::kFragmentEnd | two_left.size(),
      b1_hdr);
  uint16_t b1_hdr2 = (uint16_t(buf[LogFormat::kBlockSize - 1]) << 8) |
                     buf[LogFormat::kBlockSize - 2];
  EXPECT_EQ(LogFormat::kFragmentBegin, b1_hdr2);
  EXPECT_EQ(0, buf[LogFormat::kBlockSize * 2 - 1]);  // padding zero

  LogReader reader;
  ASSERT_TRUE(reader.Open(filename).ok());
  ASSERT_EQ(expected_file_size, reader.file_size_in_bytes());
  int64_t offset = 0;
  for (size_t i = 0; i < data.size(); i++) {
    std::string rd;
    ASSERT_TRUE(reader.Read(offset, &rd, &offset).ok());
    ASSERT_EQ(data[i].size(), rd.size());
    EXPECT_EQ(data[i], rd);
  }

  std::string rd_must_fail;
  ASSERT_FALSE(reader.Read(offset, &rd_must_fail, &offset).ok());

  ASSERT_TRUE(reader.Close().ok());
}
