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

#include "holonightd/Daemon.h"

#include "holonightd/FilesystemScanner.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace holonightd {

namespace {

struct SeverityCounts {
  std::size_t critical{0};
  std::size_t error{0};
  std::size_t warning{0};
};

SeverityCounts countFindingSeverities(const std::vector<DiagnosticFinding>& findings) {
  SeverityCounts counts;
  for (const auto& finding : findings) {
    if (finding.severity == Severity::Critical) {
      counts.critical++;
    } else if (finding.severity == Severity::Error) {
      counts.error++;
    } else if (finding.severity == Severity::Warning) {
      counts.warning++;
    }
  }
  return counts;
}

void formatSingleFinding(std::ostringstream& stream, const DiagnosticFinding& finding) {
  stream << "[" << severityToString(finding.severity) << "] " << finding.rule_id << ": " << finding.title << "\n";
  stream << "  Category: " << finding.category << "\n";
  stream << "  Matched Events (" << finding.matched_events.size() << "):\n";
  for (const auto& event : finding.matched_events) {
    stream << "    - [" << event.source << "] " << event.signal << " on " << event.subject << "\n";
  }
  if (!finding.candidate_causes.empty()) {
    stream << "  Candidate Root Causes:\n";
    for (const auto& cause : finding.candidate_causes) {
      stream << "    * " << cause << "\n";
    }
  }
  if (!finding.suggested_actions.empty()) {
    stream << "  Suggested Remediation Actions:\n";
    for (const auto& action : finding.suggested_actions) {
      stream << "    * " << action << "\n";
    }
  }
  stream << "\n";
}

}  // namespace

Daemon::Daemon(Config config, Logger logger, std::optional<std::filesystem::path> db_path, bool debug)
    : config_{std::move(config)},
      logger_{std::move(logger)},
      debug_{debug},
      healthCheck_{config_.commands, llmClient_, logger_},
      systemdCollector_{SystemdCollectorOptions{
          .flapping_threshold = config_.systemd.flapping_threshold,
          .flapping_window_seconds = config_.systemd.flapping_window_seconds,
          .ignore_units = config_.systemd.ignore_units,
      }},
      storageCollector_{StorageCollectorOptions{
          .warning_threshold = config_.storage_warning_threshold,
          .critical_threshold = config_.storage_critical_threshold,
          .auto_discover = config_.storage_mount_points.empty(),
          .mount_points = config_.storage_mount_points,
      }},
      memoryCollector_{MemoryCollectorOptions{
          .some_warning_threshold = config_.memory.some_warning_threshold,
          .full_critical_threshold = config_.memory.full_critical_threshold,
          .meminfo_warning_threshold = config_.memory.meminfo_warning_threshold,
      }},
      pacmanCollector_{PacmanCollectorOptions{
          .sys_root = config_.pacman.sys_root,
          .db_path = config_.pacman.db_path,
          .etc_path = config_.pacman.etc_path,
          .max_depth = config_.pacman.max_depth,
          .warning_threshold = config_.pacman.warning_threshold,
          .check_kernel = config_.pacman.check_kernel,
          .check_locks = config_.pacman.check_locks,
          .check_orphans = config_.pacman.check_orphans,
          .check_transactions = config_.pacman.check_transactions,
      }},
      retention_days_{config_.database.retention_days.value_or(30)} {
  if (config_.rules_dir.has_value() && std::filesystem::exists(*config_.rules_dir)) {
    const auto loaded = ruleEngine_.loadRulesFromDirectory(*config_.rules_dir);
    logger_.info("Loaded " + std::to_string(loaded) + " custom rules from " + config_.rules_dir->string());
  }

  const auto resolved_db_path = db_path.has_value() ? *db_path : resolveDatabasePath(config_.database.path);
  try {
    eventStore_ = std::make_unique<EventStore>(resolved_db_path);
    logger_.info("EventStore initialized at path=" + resolved_db_path.string());
  } catch (const std::exception& err) {
    logger_.error("Failed to initialize EventStore at path=" + resolved_db_path.string() + ": " + err.what());
    throw;
  }
}

std::vector<ObservationEvent> Daemon::collectAllEvents() {
  std::vector<ObservationEvent> events;

  try {
    const auto systemd_events = systemdCollector_.collect();
    events.insert(events.end(), systemd_events.begin(), systemd_events.end());
  } catch (const std::exception& err) {
    logger_.error("SystemdCollector failed: " + std::string(err.what()));
  }

  try {
    const auto storage_events = storageCollector_.collect();
    events.insert(events.end(), storage_events.begin(), storage_events.end());
  } catch (const std::exception& err) {
    logger_.error("StorageCollector failed: " + std::string(err.what()));
  }

  try {
    const auto memory_events = memoryCollector_.collect();
    events.insert(events.end(), memory_events.begin(), memory_events.end());
  } catch (const std::exception& err) {
    logger_.error("MemoryCollector failed: " + std::string(err.what()));
  }

  try {
    const auto pacman_events = pacmanCollector_.collect();
    events.insert(events.end(), pacman_events.begin(), pacman_events.end());
  } catch (const std::exception& err) {
    logger_.error("PacmanCollector failed: " + std::string(err.what()));
  }

  return events;
}

std::string Daemon::formatStatusReport(const std::vector<DiagnosticFinding>& findings) {
  std::ostringstream stream;

  const auto now = std::chrono::system_clock::now();
  const auto time_t_now = std::chrono::system_clock::to_time_t(now);
  std::tm utc_tm{};
  gmtime_r(&time_t_now, &utc_tm);

  const auto counts = countFindingSeverities(findings);
  const bool is_unhealthy = (counts.critical > 0 || counts.error > 0);

  stream << "================================================================================\n";
  stream << "                       holonightd System Status Report\n";
  stream << "================================================================================\n";
  stream << "Timestamp: " << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ") << "\n";
  stream << "Overall Status: " << (is_unhealthy ? "UNHEALTHY" : "OK");

  if (is_unhealthy) {
    stream << " (" << counts.critical << " Critical, " << counts.error << " Error)";
  } else if (counts.warning > 0) {
    stream << " (" << counts.warning << " Warning)";
  }
  stream << "\n\n";

  stream << "--------------------------------------------------------------------------------\n";
  stream << "ACTIVE DIAGNOSTIC FINDINGS (" << findings.size() << ")\n";
  stream << "--------------------------------------------------------------------------------\n\n";

  if (findings.empty()) {
    stream << "No active diagnostic issues detected across system collectors.\n\n";
  } else {
    for (const auto& finding : findings) {
      formatSingleFinding(stream, finding);
    }
  }

  stream << "================================================================================\n";
  stream << "Summary: " << findings.size() << " findings detected across 4 system collectors.\n";
  stream << "================================================================================\n";

  return stream.str();
}

int Daemon::runStatusCheck() {
  const auto events = collectAllEvents();
  const auto findings = ruleEngine_.evaluate(events);
  const auto report = formatStatusReport(findings);

  std::cout << report << '\n';

  for (const auto& finding : findings) {
    if (finding.severity == Severity::Error || finding.severity == Severity::Critical) {
      return 1;
    }
  }
  return 0;
}

void Daemon::run(const std::atomic_bool& stopRequested, RunMode mode) {
  logger_.info("daemon started");

  while (true) {
    runIteration(&stopRequested);
    if (mode == RunMode::Once || stopRequested.load()) {
      break;
    }

    constexpr auto kStep = std::chrono::milliseconds(100);
    auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(config_.interval);
    while (remaining > std::chrono::milliseconds::zero() && !stopRequested.load()) {
      const auto sleep_dur = std::min(remaining, kStep);
      std::this_thread::sleep_for(sleep_dur);
      remaining -= sleep_dur;
    }

    if (stopRequested.load()) {
      break;
    }
  }

  logger_.info("daemon stopped");
}

void Daemon::runIteration(const std::atomic_bool* stop_requested) {
  const auto scan = FilesystemScanner::scan(config_.scan_root);
  logger_.info("scan root=" + config_.scan_root.string() + " files=" + std::to_string(scan.files) +
               " directories=" + std::to_string(scan.directories));

  const auto events = collectAllEvents();
  if (debug_) {
    logger_.debug("[telemetry] collected " + std::to_string(events.size()) + " events across collectors");
  }

  if (eventStore_ != nullptr) {
    auto insert_res = eventStore_->insertBatch(events);
    if (!insert_res.has_value()) {
      logger_.error("failed to insert event batch into EventStore: " + insert_res.error());
    } else if (debug_) {
      logger_.debug("[metrics] inserted " + std::to_string(*insert_res) + " events into EventStore");
    }

    const auto cutoff = std::chrono::system_clock::now() - std::chrono::days(retention_days_);
    auto prune_res = eventStore_->pruneEvents(cutoff);
    if (!prune_res.has_value()) {
      logger_.error("failed to prune expired events from EventStore: " + prune_res.error());
    } else if (*prune_res > 0) {
      logger_.info("pruned " + std::to_string(*prune_res) + " expired events from EventStore");
    }

    const size_t max_bytes = config_.database.max_bytes.value_or(52428800);
    const size_t max_events = config_.database.max_events.value_or(100000);
    auto capacity_prune_res = eventStore_->pruneEventsByCapacity(max_bytes, max_events);
    if (!capacity_prune_res.has_value()) {
      logger_.error("failed to enforce EventStore capacity limits: " + capacity_prune_res.error());
    } else if (*capacity_prune_res > 0) {
      logger_.info("pruned " + std::to_string(*capacity_prune_res) + " events to enforce capacity limits");
    }
  }

  const auto findings = ruleEngine_.evaluate(events);
  for (const auto& finding : findings) {
    logger_.info("[diagnostic] rule=" + finding.rule_id + " title=\"" + finding.title +
                 "\" severity=" + severityToString(finding.severity));
  }

  healthCheck_.run(stop_requested);
}

}  // namespace holonightd
