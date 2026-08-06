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

#include "holonightd/CommandRunner.h"
#include "holonightd/LlmClient.h"
#include "holonightd/Logger.h"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace holonightd {

class HealthCheckJob {
 public:
  HealthCheckJob(std::vector<std::string> commands, const LlmClient& llmClient, Logger& logger);

  void run(const std::atomic_bool* stop_requested = nullptr) const;

  [[nodiscard]] static std::string resultsToText(const std::vector<CommandResult>& results);

 private:
  std::vector<std::string> commands_;
  std::reference_wrapper<const LlmClient> llmClient_;
  std::reference_wrapper<Logger> logger_;
};

}  // namespace holonightd
