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

#include "holonightd/Logger.h"

#include <cctype>
#include <chrono>
#include <iomanip>
#include <stdexcept>

#ifdef HOLONIGHTD_HAS_SYSTEMD
#include <systemd/sd-journal.h>
#endif

namespace holonightd {

LogLevel parseLogLevel(std::string_view level_str) {
  std::string lower;
  lower.reserve(level_str.size());
  for (char symbol : level_str) {
    lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(symbol))));
  }
  if (lower == "debug") {
    return LogLevel::Debug;
  }
  if (lower == "info") {
    return LogLevel::Info;
  }
  if (lower == "warn" || lower == "warning") {
    return LogLevel::Warn;
  }
  if (lower == "error") {
    return LogLevel::Error;
  }
  throw std::invalid_argument("invalid log level: " + std::string(level_str));
}

std::string_view logLevelToString(LogLevel level) {
  switch (level) {
    case LogLevel::Debug:
      return "DEBUG";
    case LogLevel::Info:
      return "INFO";
    case LogLevel::Warn:
      return "WARN";
    case LogLevel::Error:
      return "ERROR";
  }
  return "UNKNOWN";
}

Logger::Logger(LogLevel active_level, bool force_stdout, std::ostream& output)
    : active_level_{active_level}, force_stdout_{force_stdout}, output_{&output} {}

Logger::Logger(std::ostream& output) : force_stdout_{true}, output_{&output} {}

void Logger::debug(const std::string& message) { write(LogLevel::Debug, message); }

void Logger::info(const std::string& message) { write(LogLevel::Info, message); }

void Logger::warn(const std::string& message) { write(LogLevel::Warn, message); }

void Logger::error(const std::string& message) { write(LogLevel::Error, message); }

void Logger::write(LogLevel level, const std::string& message) {
  if (static_cast<int>(level) > static_cast<int>(active_level_)) {
    return;
  }

#ifdef HOLONIGHTD_HAS_SYSTEMD
  if (!force_stdout_) {
    sd_journal_send("MESSAGE=%s", message.c_str(), "PRIORITY=%d", static_cast<int>(level),
                    "SYSLOG_IDENTIFIER=holonightd", nullptr);
    return;
  }
#endif

  std::scoped_lock lock{*output_mutex_};

  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);

  *output_ << std::put_time(std::localtime(&time), "%FT%T%z") << ' ' << logLevelToString(level) << ' ' << message
           << '\n';
}

}  // namespace holonightd
