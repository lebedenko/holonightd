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

#include <atomic>
#include <chrono>
#include <cstddef>
#include <string>

namespace holonightd {

struct CommandResult {
  std::string command;
  int exit_code{};
  std::string output;
  bool timed_out{false};
  bool cancelled{false};
  bool output_truncated{false};
};

struct CommandOptions {
  std::chrono::seconds timeout{60};
  std::size_t max_output_bytes{1024 * 1024};
  const std::atomic_bool* stop_requested{nullptr};
};

class CommandRunner {
 public:
  [[nodiscard]] static CommandResult run(const std::string& command, CommandOptions options = {});
};

}  // namespace holonightd
