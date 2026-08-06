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

#include <filesystem>
#include <string>
#include <vector>

namespace holonightd {

struct StorageCollectorOptions {
  double warning_threshold{85.0};
  double critical_threshold{95.0};
  bool auto_discover{true};
  std::vector<std::filesystem::path> mount_points;
};

struct MountInfo {
  std::filesystem::path mount_path;
  std::string device_node;
  std::string fstype;
  std::string options;
};

struct StorageMetrics {
  std::filesystem::path mount_path;
  std::string device_node;
  std::string fstype;
  std::string options;
  std::uint64_t total_bytes{0};
  std::uint64_t used_bytes{0};
  std::uint64_t available_bytes{0};
  double percent_used{0.0};
  std::uint64_t total_inodes{0};
  std::uint64_t used_inodes{0};
  std::uint64_t free_inodes{0};
  double inode_percent_used{0.0};
  bool is_read_only{false};
  bool stat_success{true};
  std::string error;
};

class StorageCollector {
 public:
  explicit StorageCollector(StorageCollectorOptions options = {});

  /// Performs raw metrics collection across target mount points.
  [[nodiscard]] std::vector<StorageMetrics> collectMetrics() const;

  /// Performs storage metrics collection across target mount points.
  /// Does not throw exceptions on I/O or filesystem errors.
  [[nodiscard]] std::vector<ObservationEvent> collect() const;

  /// Discovers active local physical mount points by reading system mount information.
  [[nodiscard]] static std::vector<MountInfo> discoverMounts();

 private:
  [[nodiscard]] static StorageMetrics inspectMountMetrics(const MountInfo& info);
  [[nodiscard]] std::vector<ObservationEvent> eventsFromMetrics(const StorageMetrics& metrics) const;

  StorageCollectorOptions options_;
};

}  // namespace holonightd
