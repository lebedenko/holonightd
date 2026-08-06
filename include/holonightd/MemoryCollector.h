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
#include <sys/types.h>
#include <vector>

namespace holonightd {

struct MemoryCollectorOptions {
  double some_warning_threshold{10.0};
  double full_critical_threshold{25.0};
  double meminfo_warning_threshold{85.0};
  std::filesystem::path proc_root{"/proc"};
};

struct MemoryMetrics {
  // PSI Metrics
  std::optional<double> psi_some_avg10;
  std::optional<double> psi_some_avg60;
  std::optional<double> psi_some_avg300;
  std::optional<double> psi_some_total;

  std::optional<double> psi_full_avg10;
  std::optional<double> psi_full_avg60;
  std::optional<double> psi_full_avg300;
  std::optional<double> psi_full_total;

  // Meminfo Metrics
  std::uint64_t total_bytes{0};
  std::uint64_t available_bytes{0};
  std::uint64_t used_bytes{0};
  std::optional<double> percent_used;

  // OOM Metrics
  std::optional<std::uint64_t> oom_kill_counter;
  std::optional<std::uint64_t> oom_kill_delta;

  // Victim extraction
  std::optional<pid_t> victim_pid;
  std::optional<std::string> victim_name;

  bool psi_success{false};
  bool meminfo_success{false};
  bool vmstat_success{false};
};

class MemoryCollector {
 public:
  explicit MemoryCollector(MemoryCollectorOptions options = {});

  /// Performs raw metrics collection across memory subsystems.
  [[nodiscard]] MemoryMetrics collectMetrics();

  /// Performs collection and converts threshold violations into observation events.
  [[nodiscard]] std::vector<ObservationEvent> collect();

 private:
  MemoryCollectorOptions options_;
  std::optional<std::uint64_t> baseline_oom_kill_;

  void parsePsi(MemoryMetrics& metrics) const;
  void parseMeminfo(MemoryMetrics& metrics) const;
  void parseVmstatOom(MemoryMetrics& metrics);
  void extractOomVictim(MemoryMetrics& metrics) const;
};

}  // namespace holonightd
