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

#include "holonightd/Logger.h"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace holonightd {

struct DatabaseConfig {
  std::optional<std::filesystem::path> path;
  std::optional<int> retention_days{30};
  std::optional<size_t> max_bytes{52428800};
  std::optional<size_t> max_events{100000};
};

struct SystemdConfig {
  int flapping_threshold{3};
  int flapping_window_seconds{300};
  std::vector<std::string> ignore_units;
};

struct MemoryConfig {
  double some_warning_threshold{10.0};
  double full_critical_threshold{25.0};
  double meminfo_warning_threshold{85.0};
};

struct PacmanConfig {
  std::filesystem::path sys_root{"/"};
  std::filesystem::path db_path{"var/lib/pacman"};
  std::filesystem::path etc_path{"etc"};
  unsigned int max_depth{3};
  unsigned int warning_threshold{5};
  bool check_kernel{true};
  bool check_locks{true};
  bool check_orphans{true};
  bool check_transactions{true};
};

struct Config {
  std::chrono::seconds interval{300};
  std::filesystem::path scan_root{"."};
  std::vector<std::string> commands;

  double storage_warning_threshold{85.0};
  double storage_critical_threshold{95.0};
  std::vector<std::filesystem::path> storage_mount_points;

  std::optional<std::string> log_level;

  DatabaseConfig database;
  SystemdConfig systemd;
  MemoryConfig memory;
  PacmanConfig pacman;
  std::optional<std::filesystem::path> rules_dir;

  [[nodiscard]] static Config fromFile(const std::filesystem::path& path);
};

struct CliOptions {
  bool run_once{false};
  bool debug{false};
  bool status{false};
  std::optional<std::filesystem::path> config_path;
};

/// Returns the resolved config file path.
/// Precedence:
///   1. explicit_override — returned as-is when set (caller passed --config PATH)
///   2. $XDG_CONFIG_HOME/holonight/holonightd.toml
///   3. $HOME/.config/holonight/holonightd.toml
///
/// Does NOT verify that the path exists.
[[nodiscard]] std::filesystem::path resolveConfigPath(
    std::optional<std::filesystem::path> explicit_override = std::nullopt);

/// Resolves the final SQLite database file path using the XDG base directory specification.
/// Precedence:
///   1. explicit_path — returned as-is when set (caller passed custom path or [database].path)
///   2. $XDG_DATA_HOME/holonight/events.db
///   3. $HOME/.local/share/holonight/events.db
[[nodiscard]] std::filesystem::path resolveDatabasePath(
    std::optional<std::filesystem::path> explicit_path = std::nullopt);

[[nodiscard]] LogLevel resolveLogLevel(const CliOptions& cli_options, const Config& config);

}  // namespace holonightd
