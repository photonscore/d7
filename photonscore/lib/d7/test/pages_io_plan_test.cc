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

#include "photonscore/lib/d7/internal/pages_io_plan.h"

#include <gtest/gtest.h>

#include <vector>

using photonscore::d7::internal::PagesIOPlan;

void CheckIfComplete(const PagesIOPlan& plan) {
  ASSERT_TRUE(plan.valid());

  std::vector<bool> buf(plan.io_length(), false);

  for (size_t i = 0; i < plan.size(); i++) {
    auto e = plan(i);
    EXPECT_TRUE(e.valid());
    // Fill buffer
    for (size_t it = e.begin, j = e.destination; it != e.end; it++, j++) {
      EXPECT_FALSE(buf.at(j));  // no overlap
      ASSERT_TRUE(j < buf.size());
      buf.at(j) = true;
    }
  }

  // Validate buffer
  for (auto x : buf) {
    EXPECT_TRUE(x);
  }
}

TEST(PagesIOPlanTest, FullPages) {
  PagesIOPlan plan(100, 10, 0, 100);
  CheckIfComplete(plan);

  EXPECT_EQ(10, plan.size());
  for (size_t i = 0; i < 10; i++) {
    auto e = plan(i);
    EXPECT_TRUE(e.valid());
    EXPECT_EQ(i, e.io_page_number);
    EXPECT_EQ(0, e.begin);
    EXPECT_EQ(10, e.end);
    EXPECT_EQ(i * 10, e.destination);
  }
}

TEST(PagesIOPlanTest, ToEnd) {
  PagesIOPlan plan(100, 10, 3, 97);
  CheckIfComplete(plan);

  EXPECT_EQ(10, plan.size());

  // First page
  {
    auto e = plan(0);
    EXPECT_TRUE(e.valid());
    EXPECT_EQ(0, e.io_page_number);
    EXPECT_EQ(3, e.begin);
    EXPECT_EQ(10, e.end);
    EXPECT_EQ(0, e.destination);
  }

  // Inner pages
  for (size_t i = 1; i < 9; i++) {
    auto e = plan(i);
    EXPECT_TRUE(e.valid());
    EXPECT_EQ(i, e.io_page_number);
    EXPECT_EQ(0, e.begin);
    EXPECT_EQ(10, e.end);
    EXPECT_EQ(7 + (i - 1) * 10, e.destination);
  }

  // Last page
  {
    auto e = plan(9);
    EXPECT_TRUE(e.valid());
    EXPECT_EQ(9, e.io_page_number);
    EXPECT_EQ(0, e.begin);
    EXPECT_EQ(10, e.end);
    EXPECT_EQ(87, e.destination);
  }
}

TEST(PagesIOPlanTest, FromBeginning) {
  PagesIOPlan plan(100, 10, 0, 97);
  CheckIfComplete(plan);

  // First page
  {
    auto e = plan(0);
    EXPECT_TRUE(e.valid());
    EXPECT_EQ(0, e.io_page_number);
    EXPECT_EQ(0, e.begin);
    EXPECT_EQ(10, e.end);
    EXPECT_EQ(0, e.destination);
  }

  // Inner pages
  for (size_t i = 1; i < 9; i++) {
    auto e = plan(i);
    EXPECT_TRUE(e.valid());
    EXPECT_EQ(i, e.io_page_number);
    EXPECT_EQ(0, e.begin);
    EXPECT_EQ(10, e.end);
    EXPECT_EQ(i * 10, e.destination);
  }

  // Last page
  {
    auto e = plan(9);
    EXPECT_TRUE(e.valid());
    EXPECT_EQ(9, e.io_page_number);
    EXPECT_EQ(0, e.begin);
    EXPECT_EQ(7, e.end);
    EXPECT_EQ(90, e.destination);
  }
}

TEST(PagesIOPlanTest, Middle) {
  PagesIOPlan plan(100, 10, 3, 91);
  CheckIfComplete(plan);
  EXPECT_EQ(10, plan.size());

  // First page
  {
    auto e = plan(0);
    EXPECT_TRUE(e.valid());
    EXPECT_EQ(0, e.io_page_number);
    EXPECT_EQ(3, e.begin);
    EXPECT_EQ(10, e.end);
    EXPECT_EQ(0, e.destination);
  }

  // Inner pages
  for (size_t i = 1; i < 9; i++) {
    auto e = plan(i);
    EXPECT_TRUE(e.valid());
    EXPECT_EQ(i, e.io_page_number);
    EXPECT_EQ(0, e.begin);
    EXPECT_EQ(10, e.end);
    EXPECT_EQ(7 + (i - 1) * 10, e.destination);
  }

  // Last page
  {
    auto e = plan(9);
    EXPECT_TRUE(e.valid());
    EXPECT_EQ(9, e.io_page_number);
    EXPECT_EQ(0, e.begin);
    EXPECT_EQ(4, e.end);
    EXPECT_EQ(87, e.destination);
  }
}

TEST(PagesIOPlanTest, Inner) {
  PagesIOPlan plan(100, 10, 4, 91);
  CheckIfComplete(plan);

  // First page
  {
    auto e = plan(0);
    EXPECT_TRUE(e.valid());
    EXPECT_EQ(0, e.io_page_number);
    EXPECT_EQ(4, e.begin);
    EXPECT_EQ(10, e.end);
    EXPECT_EQ(0, e.destination);
  }

  // Inner pages
  for (size_t i = 1; i < 9; i++) {
    auto e = plan(i);
    EXPECT_TRUE(e.valid());
    EXPECT_EQ(i, e.io_page_number);
    EXPECT_EQ(0, e.begin);
    EXPECT_EQ(10, e.end);
    EXPECT_EQ(6 + (i - 1) * 10, e.destination);
  }

  // Last page
  {
    auto e = plan(9);
    EXPECT_TRUE(e.valid());
    EXPECT_EQ(9, e.io_page_number);
    EXPECT_EQ(0, e.begin);
    EXPECT_EQ(5, e.end);
    EXPECT_EQ(86, e.destination);
  }
}

TEST(PagesIOPlanTest, Edges) {
  {
    PagesIOPlan plan(100, 10, 19, 1);
    CheckIfComplete(plan);
    EXPECT_EQ(1, plan.size());
    {
      auto e = plan(0);
      EXPECT_EQ(1, e.io_page_number);
      EXPECT_EQ(9, e.begin);
      EXPECT_EQ(10, e.end);
      EXPECT_EQ(0, e.destination);
    }
  }
  {
    PagesIOPlan plan(100, 10, 29, 2);
    CheckIfComplete(plan);
    EXPECT_EQ(2, plan.size());
    {
      auto e = plan(0);
      EXPECT_EQ(2, e.io_page_number);
      EXPECT_EQ(9, e.begin);
      EXPECT_EQ(10, e.end);
      EXPECT_EQ(0, e.destination);
    }
    {
      auto e = plan(1);
      EXPECT_EQ(3, e.io_page_number);
      EXPECT_EQ(0, e.begin);
      EXPECT_EQ(1, e.end);
      EXPECT_EQ(1, e.destination);
    }
  }
}

TEST(PagesIOPlanTest, InvalidPlans) {
  PagesIOPlan too_large(100, 10, 99, 2);
  EXPECT_FALSE(too_large.valid());
}
