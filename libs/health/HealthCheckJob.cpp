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

#include <sstream>

namespace holonightd {

HealthCheckJob::HealthCheckJob(std::vector<std::string> commands, const LlmClient& llmClient, Logger& logger)
    : commands_{std::move(commands)}, llmClient_{llmClient}, logger_{logger} {}

void HealthCheckJob::run(const std::atomic_bool* stop_requested) const {
  if (commands_.empty()) {
    logger_.get().info("health check skipped: no commands configured");
    return;
  }

  std::vector<CommandResult> results;
  results.reserve(commands_.size());

  for (const auto& command : commands_) {
    if (stop_requested != nullptr && stop_requested->load(std::memory_order_relaxed)) {
      logger_.get().info("health check cancelled: daemon stop requested");
      break;
    }
    logger_.get().info("running: " + command);
    auto result = CommandRunner::run(command, {.stop_requested = stop_requested});
    logger_.get().info("finished: " + command + " exit=" + std::to_string(result.exit_code) +
                       (result.timed_out ? " timed_out=true" : "") + (result.cancelled ? " cancelled=true" : "") +
                       (result.output_truncated ? " output_truncated=true" : ""));
    results.push_back(std::move(result));
  }

  logger_.get().info(llmClient_.get().summarize(resultsToText(results)));
}

std::string HealthCheckJob::resultsToText(const std::vector<CommandResult>& results) {
  std::ostringstream output;
  for (const auto& result : results) {
    output << "$ " << result.command << '\n';
    output << "exit=" << result.exit_code << '\n';
    output << result.output << '\n';
  }
  return output.str();
}

}  // namespace holonightd
