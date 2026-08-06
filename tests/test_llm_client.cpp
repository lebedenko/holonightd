// SPDX-License-Identifier: GPL-3.0-or-later
// holonightd - Lightweight daemon for routine maintenance automation.
// Copyright (C) 2026 holonightd contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "holonightd/LlmClient.h"

#include <gtest/gtest.h>

TEST(LlmClientTest, LocalSummaryReportsSuccess) {
  const holonightd::LocalSummaryClient client;
  const auto summary = client.summarize("$ true\nexit=0\n");
  EXPECT_TRUE(summary.contains("successfully"));
}

TEST(LlmClientTest, LocalSummaryReportsNonZeroExit) {
  const holonightd::LocalSummaryClient client;
  const auto summary = client.summarize("$ false\nexit=1\n");
  EXPECT_TRUE(summary.contains("non-zero"));
}
