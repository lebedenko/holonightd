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

#include "holonightd/Application.h"
#include "holonightd/EventStore.h"
#include "holonightd/HealthCheckJob.h"
#include "holonightd/LlmClient.h"
#include "holonightd/Logger.h"
#include "holonightd/MemoryCollector.h"
#include "holonightd/ObservationEvent.h"
#include "holonightd/PacmanCollector.h"
#include "holonightd/RuleEngine.h"
#include "holonightd/StorageCollector.h"
#include "holonightd/SystemdCollector.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace holonightd {

enum class RunMode : std::uint8_t { Loop, Once };

class Daemon {
 public:
  Daemon(Config config, Logger logger, std::optional<std::filesystem::path> db_path = std::nullopt, bool debug = false);

  void run(const std::atomic_bool& stopRequested, RunMode mode);

  /// Executes a single diagnostic pass and outputs formatted status report.
  /// Returns exit code 1 if critical/error findings exist, 0 otherwise.
  [[nodiscard]] int runStatusCheck();

  /// Formats findings into human-readable ASCII report.
  [[nodiscard]] static std::string formatStatusReport(const std::vector<DiagnosticFinding>& findings);

 private:
  void runIteration(const std::atomic_bool* stop_requested = nullptr);
  [[nodiscard]] std::vector<ObservationEvent> collectAllEvents();

  Config config_;
  Logger logger_;
  bool debug_{false};
  LocalSummaryClient llmClient_;
  HealthCheckJob healthCheck_;

  SystemdCollector systemdCollector_;
  StorageCollector storageCollector_;
  MemoryCollector memoryCollector_;
  PacmanCollector pacmanCollector_;
  RuleEngine ruleEngine_;

  std::unique_ptr<EventStore> eventStore_;
  int retention_days_{30};
};

}  // namespace holonightd
