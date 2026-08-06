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

#include "holonightd/ObservationEvent.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace holonightd {

/// Configuration options for PacmanCollector.
struct PacmanCollectorOptions {
  std::filesystem::path sys_root{"/"};
  std::filesystem::path db_path{"var/lib/pacman"};  // Relative to sys_root
  std::filesystem::path etc_path{"etc"};            // Relative to sys_root

  unsigned int max_depth{3};
  unsigned int warning_threshold{5};

  bool check_kernel{true};
  bool check_locks{true};
  bool check_orphans{true};
  bool check_transactions{true};
};

/// Snapshot of raw metrics collected by PacmanCollector before mapping to events.
struct PacmanCollectorMetrics {
  // Kernel metrics
  std::optional<std::string> running_kernel;
  bool kernel_mismatch_detected{false};
  std::vector<std::string> installed_modules_dirs;
  std::vector<std::string> installed_kernel_packages;

  // Lock metrics
  struct LockState {
    bool exists{false};
    std::optional<int32_t> pid;
    bool is_active{false};
    std::optional<std::string> process_name;
    std::optional<std::uint64_t> lock_age_seconds;
    std::string invalid_reason;  // e.g. "invalid_pid", "process_dead"
  } lock;

  // Orphan config metrics
  std::int64_t pacnew_count{0};
  std::int64_t pacsave_count{0};
  std::vector<std::string> orphan_files;

  // Transaction metrics
  bool interrupted_transaction_detected{false};
  std::vector<std::string> transaction_artifacts;
};

/// Pacman / package subsystem diagnostic collector.
/// Inspects pacman DB, /proc, /usr/lib/modules, and /etc non-invasively using std::filesystem.
class PacmanCollector {
 public:
  explicit PacmanCollector(PacmanCollectorOptions options = {});

  /// Collects raw telemetry metrics across configured pacman state targets.
  [[nodiscard]] PacmanCollectorMetrics collectMetrics() const;

  /// Performs pacman state diagnosis and returns normalized ObservationEvents.
  /// Guaranteed not to throw exceptions to the caller.
  [[nodiscard]] std::vector<ObservationEvent> collect() const noexcept;

 private:
  [[nodiscard]] std::filesystem::path resolvePath(const std::filesystem::path& relative_or_abs) const;
  void evaluateKernelState(PacmanCollectorMetrics& metrics) const;
  void evaluateLockState(PacmanCollectorMetrics& metrics) const;
  void evaluateOrphanConfigs(PacmanCollectorMetrics& metrics) const;
  void evaluateTransactions(PacmanCollectorMetrics& metrics) const;

  PacmanCollectorOptions options_;
};

}  // namespace holonightd
