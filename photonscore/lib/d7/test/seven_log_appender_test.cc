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
#include "photonscore/lib/d7/backend/seven/writable_file_impl.h"

#include <gtest/gtest.h>
#include <filesystem>
#include <random>

namespace fs = std::filesystem;

using photonscore::base::Slice;
using photonscore::d7::backend::seven::LogAppender;
using photonscore::d7::backend::seven::WritableFile;
using photonscore::d7::backend::seven::WritableFileImpl;

size_t ReadWholeFile(const std::string filename, void *buf, size_t n) {
  FILE *f = fopen(filename.c_str(), "rb");
  if (f == nullptr) return -1;
  size_t read = fread(buf, 1, n, f);
  fclose(f);
  return read;
}

TEST(LogAppenderTest, Simple) {
  std::string filename = "test.log-1";
  std::unique_ptr<WritableFile> wf;
  fs::remove_all(filename);
  auto status = WritableFileImpl::CreateNew(filename, &wf);
  ASSERT_TRUE(status.ok());
  LogAppender appender(std::move(wf));
  std::string abc = "abc";
  ASSERT_TRUE(appender.Append(Slice(&abc[0], 3)).ok());
  ASSERT_TRUE(appender.Close().ok());

  uint8_t buf[10];
  ASSERT_EQ(5, ReadWholeFile(filename, buf, 10));

  uint16_t header = (uint16_t(buf[1]) << 8) | buf[0];
  EXPECT_EQ(0xC003, header);  // Full fragment of 3 bytes
  EXPECT_EQ('a', buf[2]);
  EXPECT_EQ('b', buf[3]);
  EXPECT_EQ('c', buf[4]);
}

TEST(LogAppenderTest, TwoBlocks) {
  std::mt19937_64 rng(12345);
  std::uniform_int_distribution<uint16_t> rnd(0, 1000);
  std::vector<uint16_t> data;
  for (size_t i = 0; i < 10000; i++) data.push_back(rnd(rng));

  std::string filename = "test.log-2";
  std::unique_ptr<WritableFile> wf;
  fs::remove_all(filename);
  auto status = WritableFileImpl::CreateNew(filename, &wf);
  ASSERT_TRUE(status.ok());
  LogAppender appender(std::move(wf));
  ASSERT_TRUE(
      appender.Append(Slice((char *)&data[0], data.size() * sizeof(data[0])))
          .ok());
  ASSERT_TRUE(appender.Close().ok());

  std::vector<uint16_t> buf;
  buf.resize(data.size() + 2);
  ASSERT_EQ(buf.size() * sizeof(buf[0]),
            ReadWholeFile(filename, &buf[0], buf.size() * sizeof(buf[0])));
  EXPECT_EQ(0x8000 | (0x4000 - 2), buf[0]);

  size_t i, j;
  j = 0;
  for (i = 1; i < 0x2000; i++, j++) EXPECT_EQ(data[j], buf[i]);
  EXPECT_EQ(0x4000 | ((data.size() - 0x2000) * 2 + 2), buf[i]);
  i++;
  for (size_t k = 0; k < 10000 - 0x2000; k++, i++, j++)
    EXPECT_EQ(data[j], buf[i]);
}
