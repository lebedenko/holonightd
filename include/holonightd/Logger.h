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

#pragma once

#include <cstdint>
#include <iosfwd>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace holonightd {

enum class LogLevel : std::uint8_t {
  Debug = 7,  // Syslog LOG_DEBUG
  Info = 6,   // Syslog LOG_INFO
  Warn = 4,   // Syslog LOG_WARNING
  Error = 3   // Syslog LOG_ERR
};

[[nodiscard]] LogLevel parseLogLevel(std::string_view level_str);
[[nodiscard]] std::string_view logLevelToString(LogLevel level);

class Logger {
 public:
  explicit Logger(LogLevel active_level = LogLevel::Info, bool force_stdout = false, std::ostream& output = std::cout);
  explicit Logger(std::ostream& output);

  void debug(const std::string& message);
  void info(const std::string& message);
  void warn(const std::string& message);
  void error(const std::string& message);

  [[nodiscard]] LogLevel activeLevel() const noexcept { return active_level_; }
  [[nodiscard]] bool isStdoutForced() const noexcept { return force_stdout_; }

 private:
  void write(LogLevel level, const std::string& message);

  LogLevel active_level_{LogLevel::Info};
  bool force_stdout_{false};
  std::ostream* output_{&std::cout};
  std::shared_ptr<std::mutex> output_mutex_{std::make_shared<std::mutex>()};
};

}  // namespace holonightd
