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

#include "photonscore/lib/d7/backend/seven/writable_file_impl.h"

#include <gtest/gtest.h>
#include <filesystem>

namespace fs = std::filesystem;

using photonscore::d7::backend::seven::WritableFile;
using photonscore::d7::backend::seven::WritableFileImpl;

TEST(SevenWritableFileTest, Test1) {
  WritableFile *wf;
  fs::remove_all("test1");
  auto status = WritableFileImpl::CreateNew("test1", &wf);
  ASSERT_TRUE(status.ok());
  ASSERT_TRUE(wf->Close().ok());
  delete wf;

  // Fail overwritting
  status = WritableFileImpl::CreateNew("test1", &wf);
  ASSERT_FALSE(status.ok());
}

TEST(SevenWritableFileTest, Test2) {
  WritableFile *wf;
  fs::remove_all("test2");
  auto status = WritableFileImpl::CreateNew("test2", &wf);
  ASSERT_TRUE(status.ok());

  std::string buf;

  buf = "abc";
  EXPECT_TRUE(wf->Write(buf).ok());
  EXPECT_EQ(3, wf->file_size_in_bytes());

  buf = "4567890";
  EXPECT_TRUE(wf->Write(buf).ok());
  EXPECT_EQ(10, wf->file_size_in_bytes());

  ASSERT_TRUE(wf->Close().ok());
  delete wf;

  FILE *f = fopen("test2", "rb");
  ASSERT_NE(nullptr, f);
  buf.resize(22);
  int rd = fread(&buf[0], 1, buf.size(), f);
  fclose(f);
  ASSERT_EQ(10, rd);
  buf.resize(rd);
  EXPECT_EQ("abc4567890", buf);
}
