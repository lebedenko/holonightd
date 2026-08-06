// SPDX-License-Identifier: GPL-3.0-or-later
// holonightd - Lightweight daemon for routine maintenance automation.
// Copyright (C) 2026 holonightd contributors

#include "holonightd/Logger.h"

#include <gtest/gtest.h>
#include <sstream>
#include <thread>
#include <vector>

namespace holonightd {

TEST(LogLevelTest, ParseValidLogLevelsCaseInsensitive) {
  EXPECT_EQ(parseLogLevel("debug"), LogLevel::Debug);
  EXPECT_EQ(parseLogLevel("DEBUG"), LogLevel::Debug);
  EXPECT_EQ(parseLogLevel("DeBuG"), LogLevel::Debug);

  EXPECT_EQ(parseLogLevel("info"), LogLevel::Info);
  EXPECT_EQ(parseLogLevel("INFO"), LogLevel::Info);

  EXPECT_EQ(parseLogLevel("warn"), LogLevel::Warn);
  EXPECT_EQ(parseLogLevel("WARN"), LogLevel::Warn);
  EXPECT_EQ(parseLogLevel("warning"), LogLevel::Warn);
  EXPECT_EQ(parseLogLevel("WARNING"), LogLevel::Warn);

  EXPECT_EQ(parseLogLevel("error"), LogLevel::Error);
  EXPECT_EQ(parseLogLevel("ERROR"), LogLevel::Error);
}

TEST(LogLevelTest, ParseInvalidLogLevelThrows) {
  EXPECT_THROW(static_cast<void>(parseLogLevel("trace")), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(parseLogLevel("verbose")), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(parseLogLevel("123")), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(parseLogLevel("")), std::invalid_argument);
}

TEST(LogLevelTest, LogLevelToString) {
  EXPECT_EQ(logLevelToString(LogLevel::Debug), "DEBUG");
  EXPECT_EQ(logLevelToString(LogLevel::Info), "INFO");
  EXPECT_EQ(logLevelToString(LogLevel::Warn), "WARN");
  EXPECT_EQ(logLevelToString(LogLevel::Error), "ERROR");
}

TEST(LoggerTest, OutputFilteringByActiveLevel) {
  std::ostringstream stream;
  Logger logger{LogLevel::Warn, true, stream};

  logger.debug("Debug msg");
  logger.info("Info msg");
  logger.warn("Warn msg");
  logger.error("Error msg");

  const std::string output = stream.str();
  EXPECT_EQ(output.find("Debug msg"), std::string::npos);
  EXPECT_EQ(output.find("Info msg"), std::string::npos);
  EXPECT_NE(output.find("Warn msg"), std::string::npos);
  EXPECT_NE(output.find("Error msg"), std::string::npos);
}

TEST(LoggerTest, ThreadSafetyOnStdoutSink) {
  std::ostringstream stream;
  Logger logger{LogLevel::Debug, true, stream};

  std::vector<std::thread> threads;
  threads.reserve(10);
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&logger, i] { logger.info("Thread message " + std::to_string(i)); });
  }

  for (auto& worker_thread : threads) {
    worker_thread.join();
  }

  const std::string output = stream.str();
  for (int i = 0; i < 10; ++i) {
    EXPECT_NE(output.find("Thread message " + std::to_string(i)), std::string::npos);
  }
}

}  // namespace holonightd
