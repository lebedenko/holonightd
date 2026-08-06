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

#include "holonightd/HealthCheckJob.h"

#include <gtest/gtest.h>

TEST(HealthCheckJobTest, ResultsToTextIncludesCommandExitAndOutput) {
  const std::vector<holonightd::CommandResult> results{
      holonightd::CommandResult{.command = "echo hello", .exit_code = 0, .output = "hello\n"}};

  const auto text = holonightd::HealthCheckJob::resultsToText(results);
  EXPECT_TRUE(text.contains("$ echo hello"));
  EXPECT_TRUE(text.contains("exit=0"));
  EXPECT_TRUE(text.contains("hello"));
}
