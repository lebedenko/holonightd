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

#include "holonightd/PacmanCollector.h"

// NOLINTBEGIN
#include <nlohmann/json.hpp>
// NOLINTEND

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

namespace holonightd {

namespace {
using json = nlohmann::json;

std::optional<std::string> readOsReleaseKernel(const std::filesystem::path& osrelease_path) {
  std::error_code fs_error;
  if (std::filesystem::exists(osrelease_path, fs_error)) {
    std::ifstream stream(osrelease_path);
    std::string release_str;
    if (stream >> release_str && !release_str.empty()) {
      return release_str;
    }
  }
  return std::nullopt;
}

std::optional<std::string> readProcVersionKernel(const std::filesystem::path& proc_version_path) {
  std::error_code fs_error;
  if (std::filesystem::exists(proc_version_path, fs_error)) {
    std::ifstream stream(proc_version_path);
    std::string token1;
    std::string token2;
    std::string version_str;
    if (stream >> token1 >> token2 >> version_str && !version_str.empty()) {
      return version_str;
    }
  }
  return std::nullopt;
}

}  // namespace

PacmanCollector::PacmanCollector(PacmanCollectorOptions options) : options_(std::move(options)) {}

std::filesystem::path PacmanCollector::resolvePath(const std::filesystem::path& relative_or_abs) const {
  if (relative_or_abs.is_absolute()) {
    return options_.sys_root / relative_or_abs.relative_path();
  }
  return options_.sys_root / relative_or_abs;
}

PacmanCollectorMetrics PacmanCollector::collectMetrics() const {
  PacmanCollectorMetrics metrics;

  std::error_code fs_error;
  const auto full_db_path = resolvePath(options_.db_path);

  if (!std::filesystem::exists(full_db_path, fs_error)) {
    return metrics;
  }

  if (options_.check_kernel) {
    evaluateKernelState(metrics);
  }
  if (options_.check_locks) {
    evaluateLockState(metrics);
  }
  if (options_.check_orphans) {
    evaluateOrphanConfigs(metrics);
  }
  if (options_.check_transactions) {
    evaluateTransactions(metrics);
  }

  return metrics;
}

std::vector<ObservationEvent> PacmanCollector::collect() const noexcept {
  std::vector<ObservationEvent> events;

  try {
    const auto metrics = collectMetrics();

    if (metrics.kernel_mismatch_detected) {
      ObservationEvent event;
      event.source = "pacman_collector";
      event.category = "package";
      event.subject = "kernel";
      event.signal = "pacman.kernel_mismatch";
      event.severity = Severity::Warning;
      event.value = true;

      const json attr = {
          {"running_kernel", metrics.running_kernel.value_or("unknown")},
          {"installed_modules_dirs", metrics.installed_modules_dirs},
          {"installed_kernel_packages", metrics.installed_kernel_packages},
      };
      event.attributes_json = attr.dump();
      events.push_back(std::move(event));
    }

    if (metrics.lock.exists) {
      ObservationEvent event;
      event.source = "pacman_collector";
      event.category = "package";
      event.subject = "pacman_db";

      if (metrics.lock.is_active) {
        event.signal = "pacman.active_lock";
        event.severity = Severity::Info;
        event.value = static_cast<std::int64_t>(metrics.lock.pid.value_or(0));

        json attr = {
            {"pid", metrics.lock.pid.value_or(0)},
            {"process_name", metrics.lock.process_name.value_or("unknown")},
        };
        if (metrics.lock.lock_age_seconds.has_value()) {
          attr["lock_age_seconds"] = *metrics.lock.lock_age_seconds;
        }
        event.attributes_json = attr.dump();
      } else {
        event.signal = "pacman.stale_lock";
        event.severity = Severity::Warning;
        event.value = static_cast<std::int64_t>(metrics.lock.pid.value_or(0));

        json attr = {
            {"pid", metrics.lock.pid.value_or(0)},
            {"reason", metrics.lock.invalid_reason},
        };
        if (metrics.lock.lock_age_seconds.has_value()) {
          attr["lock_age_seconds"] = *metrics.lock.lock_age_seconds;
        }
        event.attributes_json = attr.dump();
      }
      events.push_back(std::move(event));
    }

    const auto total_orphans = metrics.pacnew_count + metrics.pacsave_count;
    if (total_orphans > 0) {
      ObservationEvent event;
      event.source = "pacman_collector";
      event.category = "package";
      event.subject = "config_files";
      event.signal = "pacman.pacnew_files";
      event.value = total_orphans;
      const bool is_warning = std::cmp_greater_equal(total_orphans, options_.warning_threshold);
      event.severity = is_warning ? Severity::Warning : Severity::Info;

      const json attr = {
          {"pacnew_count", metrics.pacnew_count},
          {"pacsave_count", metrics.pacsave_count},
          {"orphan_files", metrics.orphan_files},
      };
      event.attributes_json = attr.dump();
      events.push_back(std::move(event));
    }

    if (metrics.interrupted_transaction_detected) {
      ObservationEvent event;
      event.source = "pacman_collector";
      event.category = "package";
      event.subject = "pacman_db";
      event.signal = "pacman.interrupted_transaction";
      event.severity = Severity::Warning;
      event.value = true;

      const json attr = {
          {"reason", "temporary_transaction_artifacts_found"},
          {"transaction_artifacts", metrics.transaction_artifacts},
      };
      event.attributes_json = attr.dump();
      events.push_back(std::move(event));
    }
  } catch (...) {
    // Catch-all to enforce noexcept contract
  }

  return events;
}

void PacmanCollector::evaluateKernelState(PacmanCollectorMetrics& metrics) const {
  std::error_code fs_error;

  // 1. Determine running kernel release string
  metrics.running_kernel = readOsReleaseKernel(resolvePath("proc/sys/kernel/osrelease"));
  if (!metrics.running_kernel.has_value()) {
    metrics.running_kernel = readProcVersionKernel(resolvePath("proc/version"));
  }

  // 2. Discover installed kernel module directories
  const auto modules_path = resolvePath("usr/lib/modules");
  if (std::filesystem::exists(modules_path, fs_error) && std::filesystem::is_directory(modules_path, fs_error)) {
    for (const auto& entry : std::filesystem::directory_iterator(modules_path, fs_error)) {
      if (entry.is_directory(fs_error)) {
        metrics.installed_modules_dirs.push_back(entry.path().filename().string());
      }
    }
  }

  // 3. Discover installed kernel packages
  const auto local_db_path = resolvePath(options_.db_path) / "local";
  if (std::filesystem::exists(local_db_path, fs_error) && std::filesystem::is_directory(local_db_path, fs_error)) {
    for (const auto& entry : std::filesystem::directory_iterator(local_db_path, fs_error)) {
      const auto name = entry.path().filename().string();
      if (entry.is_directory(fs_error) && name.starts_with("linux-")) {
        metrics.installed_kernel_packages.push_back(name);
      }
    }
  }

  // 4. Mismatch check
  if (metrics.running_kernel.has_value()) {
    const auto running_release_str = *metrics.running_kernel;
    const auto target_module_dir = modules_path / running_release_str;

    if (std::filesystem::exists(modules_path, fs_error) && !std::filesystem::exists(target_module_dir, fs_error)) {
      metrics.kernel_mismatch_detected = true;
    }
  }
}

void PacmanCollector::evaluateLockState(PacmanCollectorMetrics& metrics) const {
  std::error_code fs_error;
  const auto lock_path = resolvePath(options_.db_path) / "db.lck";

  if (!std::filesystem::exists(lock_path, fs_error)) {
    metrics.lock.exists = false;
    return;
  }

  metrics.lock.exists = true;

  // Measure file age
  const auto ftime = std::filesystem::last_write_time(lock_path, fs_error);
  if (!fs_error) {
    const auto s_ftime = std::chrono::time_point_cast<std::chrono::seconds>(ftime);
    const auto s_now =
        std::chrono::time_point_cast<std::chrono::seconds>(std::filesystem::file_time_type::clock::now());
    if (s_now >= s_ftime) {
      metrics.lock.lock_age_seconds = static_cast<std::uint64_t>((s_now - s_ftime).count());
    }
  }

  // Parse PID
  std::ifstream lock_stream(lock_path);
  std::string pid_str;
  if (!(lock_stream >> pid_str) || pid_str.empty()) {
    metrics.lock.invalid_reason = "empty_or_unreadable_lock";
    metrics.lock.is_active = false;
    return;
  }

  int32_t parsed_pid = 0;
  try {
    parsed_pid = std::stoi(pid_str);
    metrics.lock.pid = parsed_pid;
  } catch (...) {
    metrics.lock.invalid_reason = "invalid_pid";
    metrics.lock.is_active = false;
    return;
  }

  if (parsed_pid <= 0) {
    metrics.lock.invalid_reason = "invalid_pid";
    metrics.lock.is_active = false;
    return;
  }

  // Check PID in proc
  const auto proc_dir = resolvePath("proc") / std::to_string(parsed_pid);
  if (std::filesystem::exists(proc_dir, fs_error) && std::filesystem::is_directory(proc_dir, fs_error)) {
    metrics.lock.is_active = true;
    const auto comm_path = proc_dir / "comm";
    if (std::filesystem::exists(comm_path, fs_error)) {
      std::ifstream comm_stream(comm_path);
      std::string proc_name;
      if (comm_stream >> proc_name && !proc_name.empty()) {
        metrics.lock.process_name = proc_name;
      }
    }
  } else {
    metrics.lock.is_active = false;
    metrics.lock.invalid_reason = "process_dead";
  }
}

void PacmanCollector::evaluateOrphanConfigs(PacmanCollectorMetrics& metrics) const {
  std::error_code fs_error;
  const auto etc_dir = resolvePath(options_.etc_path);

  if (!std::filesystem::exists(etc_dir, fs_error) || !std::filesystem::is_directory(etc_dir, fs_error)) {
    return;
  }

  const auto opts = std::filesystem::directory_options::skip_permission_denied;
  std::filesystem::recursive_directory_iterator dir_iter(etc_dir, opts, fs_error);
  const std::filesystem::recursive_directory_iterator end;

  while (dir_iter != end && !fs_error) {
    if (dir_iter.depth() >= static_cast<int>(options_.max_depth)) {
      dir_iter.disable_recursion_pending();
    }

    const auto& entry = *dir_iter;
    if (entry.is_regular_file(fs_error)) {
      const auto filename = entry.path().filename().string();
      if (filename.ends_with(".pacnew")) {
        metrics.pacnew_count++;
        metrics.orphan_files.push_back(entry.path().string());
      } else if (filename.ends_with(".pacsave")) {
        metrics.pacsave_count++;
        metrics.orphan_files.push_back(entry.path().string());
      }
    }

    dir_iter.increment(fs_error);
  }
}

void PacmanCollector::evaluateTransactions(PacmanCollectorMetrics& metrics) const {
  std::error_code fs_error;
  const auto local_db = resolvePath(options_.db_path) / "local";

  if (!std::filesystem::exists(local_db, fs_error) || !std::filesystem::is_directory(local_db, fs_error)) {
    return;
  }

  for (const auto& entry : std::filesystem::directory_iterator(local_db, fs_error)) {
    const auto filename = entry.path().filename().string();
    if (filename.ends_with(".tmp") || filename.starts_with("ALPM_DB_")) {
      metrics.interrupted_transaction_detected = true;
      metrics.transaction_artifacts.push_back(entry.path().string());
    }
  }
}

}  // namespace holonightd
