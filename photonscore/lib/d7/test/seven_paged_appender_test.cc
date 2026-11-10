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

#include "photonscore/lib/d7/backend/seven/paged_appender.h"

#include <gtest/gtest.h>

#include <vector>

using photonscore::Status;
using photonscore::d7::backend::seven::PagedAppender;

struct IntContainer {
  IntContainer(size_t page_size) : bytes_per_page_(page_size) {}
  size_t bytes_per_page() const { return bytes_per_page_; }
  Status AppendPage(const std::string& data) {
    assert((data.size() % sizeof(data_.front())) == 0);
    auto p = (const int*)&data.front();
    for (size_t i = 0; i < data.size(); p++, i += sizeof(data_.front())) {
      data_.push_back(*p);
    }
    return Status::OK();
  }

  size_t bytes_per_page_;
  std::vector<int> data_;
};

TEST(SevenPageAppenderTest, Test) {
  Status s;
  IntContainer container(12);
  PagedAppender<IntContainer> pa(&container);

  ASSERT_TRUE(pa.Init().ok());

  int a[] = {1, 2};
  ASSERT_TRUE(pa.Append(a, sizeof(a)).ok());
  ASSERT_EQ(4 * 2, pa.total_bytes());

  int b[] = {3, 4};
  ASSERT_TRUE(pa.Append(b, sizeof(b)).ok());
  ASSERT_EQ(4 * 4, pa.total_bytes());

  int c[] = {5, 6, 7, 8, 9, 10, 11};
  ASSERT_TRUE(pa.Append(c, sizeof(c)).ok());
  ASSERT_EQ(4 * 11, pa.total_bytes());

  ASSERT_TRUE(pa.Close().ok());
  ASSERT_EQ(11, container.data_.size());

  for (size_t i = 0; i < 11; i++) {
    ASSERT_EQ(i + 1, container.data_[i]);
  }
}
