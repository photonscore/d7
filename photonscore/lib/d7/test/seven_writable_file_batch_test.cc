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

#include "photonscore/lib/d7/backend/seven/writable_file_batch.h"
#include "photonscore/lib/d7/backend/seven/writable_file_impl.h"
#include "photonscore/lib/fs/fs.h"

#include <gtest/gtest.h>

using photonscore::d7::backend::seven::WritableFile;
using photonscore::d7::backend::seven::WritableFileBatch;
using photonscore::d7::backend::seven::WritableFileImpl;
using photonscore::fs::ReadFile;
using photonscore::fs::Remove;

TEST(WritableFileBatchTest, Test1) {
  WritableFile* wf;
  Remove("test1");

  auto status = WritableFileImpl::CreateNew("test1", &wf);
  ASSERT_TRUE(status.ok());
  wf->Write({"abc", 3});
  ASSERT_EQ(3, wf->file_size_in_bytes());

  {
    auto* wfb = new WritableFileBatch(wf, 100);
    ASSERT_EQ(3, wfb->file_size_in_bytes());

    wfb->Write({"def", 3});
    ASSERT_EQ(3, wf->file_size_in_bytes());
    ASSERT_EQ(6, wfb->file_size_in_bytes());

    // Close batch
    delete wfb;
  }

  // Should flush into backend
  ASSERT_EQ(6, wf->file_size_in_bytes());

  // Close backend
  delete wf;

  std::string data;
  ASSERT_TRUE(ReadFile("test1", &data).ok());
  ASSERT_EQ("abcdef", data);
}

