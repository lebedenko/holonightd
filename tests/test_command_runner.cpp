// SPDX-License-Identifier: GPL-3.0-or-later

#include "holonightd/CommandRunner.h"

#include <chrono>
#include <gtest/gtest.h>

TEST(CommandRunnerTest, CapturesOutputAndExitCode) {
  const auto result = holonightd::CommandRunner::run("printf hello; exit 7");

  EXPECT_EQ(result.exit_code, 7);
  EXPECT_EQ(result.output, "hello");
  EXPECT_FALSE(result.timed_out);
  EXPECT_FALSE(result.output_truncated);
}

TEST(CommandRunnerTest, TimesOutLongRunningCommand) {
  const auto started = std::chrono::steady_clock::now();
  const auto result = holonightd::CommandRunner::run("sleep 5", {.timeout = std::chrono::seconds{0}});

  EXPECT_EQ(result.exit_code, 124);
  EXPECT_TRUE(result.timed_out);
  EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds{2});
}

TEST(CommandRunnerTest, TruncatesOutputAtConfiguredLimit) {
  const auto result = holonightd::CommandRunner::run("printf 1234567890", {.max_output_bytes = 4});

  EXPECT_EQ(result.output, "1234");
  EXPECT_TRUE(result.output_truncated);
}

TEST(CommandRunnerTest, StopsWhenCancellationIsRequested) {
  std::atomic_bool stop_requested{true};
  const auto result = holonightd::CommandRunner::run(
      "sleep 5", {.timeout = std::chrono::seconds{60}, .stop_requested = &stop_requested});

  EXPECT_EQ(result.exit_code, 125);
  EXPECT_TRUE(result.cancelled);
}
