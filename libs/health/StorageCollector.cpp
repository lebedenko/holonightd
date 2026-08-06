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

#include "holonightd/StorageCollector.h"

// NOLINTBEGIN
#include <nlohmann/json.hpp>
// NOLINTEND

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <ranges>
#include <sstream>
#include <string_view>
#include <sys/statvfs.h>
#include <unordered_set>

namespace holonightd {

namespace {

using json = nlohmann::json;

std::string unescapeMountPath(std::string_view str) {
  std::string result;
  result.reserve(str.size());
  for (std::size_t i = 0; i < str.size(); ++i) {
    if (str[i] == '\\' && i + 3 < str.size() && std::isdigit(static_cast<unsigned char>(str[i + 1])) != 0 &&
        std::isdigit(static_cast<unsigned char>(str[i + 2])) != 0 &&
        std::isdigit(static_cast<unsigned char>(str[i + 3])) != 0) {
      const int octal = ((str[i + 1] - '0') * 64) + ((str[i + 2] - '0') * 8) + (str[i + 3] - '0');
      result.push_back(static_cast<char>(octal));
      i += 3;
    } else {
      result.push_back(str[i]);
    }
  }
  return result;
}

bool isVirtualFstype(std::string_view fstype) {
  static const std::unordered_set<std::string_view> kVirtualFstypes = {
      "proc",     "sysfs",      "tmpfs", "devtmpfs",   "cgroup",   "cgroup2", "overlay",     "squashfs",
      "devpts",   "securityfs", "bpf",   "pstore",     "tracefs",  "autofs",  "mqueue",      "hugetlbfs",
      "configfs", "debugfs",    "ramfs", "rpc_pipefs", "efivarfs", "fusectl", "binfmt_misc", "fuse"};
  return kVirtualFstypes.contains(fstype) || fstype.starts_with("fuse.");
}

bool isNetworkFstype(std::string_view fstype) {
  static const std::unordered_set<std::string_view> kNetworkFstypes = {"nfs",   "nfs4",  "cifs",
                                                                       "smbfs", "sshfs", "fuse.sshfs"};
  return kNetworkFstypes.contains(fstype);
}

bool isRemovablePath(const std::filesystem::path& path) {
  const std::string path_str = path.string();
  return path_str.starts_with("/run/media/") || path_str.starts_with("/media/");
}

}  // namespace

StorageCollector::StorageCollector(StorageCollectorOptions options) : options_{std::move(options)} {}

std::vector<MountInfo> StorageCollector::discoverMounts() {
  std::vector<MountInfo> mounts;
  std::ifstream mounts_file{"/proc/mounts"};
  if (!mounts_file.is_open()) {
    return mounts;
  }

  std::string line;
  while (std::getline(mounts_file, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }

    std::istringstream iss{line};
    std::string device_node;
    std::string raw_mount_path;
    std::string fstype;
    std::string options;

    if (!(iss >> device_node >> raw_mount_path >> fstype >> options)) {
      continue;
    }

    if (isVirtualFstype(fstype) || isNetworkFstype(fstype)) {
      continue;
    }

    const std::filesystem::path mount_path{unescapeMountPath(raw_mount_path)};
    if (isRemovablePath(mount_path)) {
      continue;
    }

    mounts.push_back(MountInfo{
        .mount_path = mount_path,
        .device_node = device_node,
        .fstype = fstype,
        .options = options,
    });
  }

  return mounts;
}

std::vector<StorageMetrics> StorageCollector::collectMetrics() const {
  std::vector<MountInfo> targets;
  if (!options_.mount_points.empty()) {
    const auto discovered = discoverMounts();
    for (const auto& path : options_.mount_points) {
      auto found_it =
          std::ranges::find_if(discovered, [&path](const MountInfo& info) { return info.mount_path == path; });
      if (found_it != discovered.end()) {
        targets.push_back(*found_it);
      } else {
        targets.push_back(MountInfo{
            .mount_path = path,
            .device_node = "unknown",
            .fstype = "unknown",
            .options = "rw",
        });
      }
    }
  } else if (options_.auto_discover) {
    targets = discoverMounts();
  }

  std::vector<StorageMetrics> metrics_list;
  metrics_list.reserve(targets.size());
  for (const auto& mount : targets) {
    metrics_list.push_back(inspectMountMetrics(mount));
  }

  return metrics_list;
}

std::vector<ObservationEvent> StorageCollector::collect() const {
  const auto metrics_list = collectMetrics();
  std::vector<ObservationEvent> events;
  for (const auto& metrics : metrics_list) {
    auto mount_events = eventsFromMetrics(metrics);
    events.insert(events.end(), std::make_move_iterator(mount_events.begin()),
                  std::make_move_iterator(mount_events.end()));
  }

  return events;
}

StorageMetrics StorageCollector::inspectMountMetrics(const MountInfo& info) {
  StorageMetrics metrics;
  metrics.mount_path = info.mount_path;
  metrics.device_node = info.device_node;
  metrics.fstype = info.fstype;
  metrics.options = info.options;

  struct statvfs stat_buf{};
  if (statvfs(info.mount_path.c_str(), &stat_buf) != 0) {
    metrics.stat_success = false;
    metrics.error = strerror(errno);
    return metrics;
  }

  metrics.stat_success = true;
  metrics.total_bytes = static_cast<std::uint64_t>(stat_buf.f_blocks) * static_cast<std::uint64_t>(stat_buf.f_frsize);
  metrics.available_bytes =
      static_cast<std::uint64_t>(stat_buf.f_bavail) * static_cast<std::uint64_t>(stat_buf.f_frsize);
  const auto free_bytes = static_cast<std::uint64_t>(stat_buf.f_bfree) * static_cast<std::uint64_t>(stat_buf.f_frsize);
  metrics.used_bytes = metrics.total_bytes > free_bytes ? metrics.total_bytes - free_bytes : 0;

  metrics.percent_used = metrics.total_bytes > 0 ? (static_cast<double>(metrics.total_bytes - metrics.available_bytes) /
                                                    static_cast<double>(metrics.total_bytes)) *
                                                       100.0
                                                 : 0.0;

  metrics.total_inodes = static_cast<std::uint64_t>(stat_buf.f_files);
  metrics.free_inodes = static_cast<std::uint64_t>(stat_buf.f_ffree);
  metrics.used_inodes = metrics.total_inodes > metrics.free_inodes ? metrics.total_inodes - metrics.free_inodes : 0;

  metrics.inode_percent_used =
      metrics.total_inodes > 0
          ? (static_cast<double>(metrics.used_inodes) / static_cast<double>(metrics.total_inodes)) * 100.0
          : 0.0;

  metrics.is_read_only = (stat_buf.f_flag & ST_RDONLY) != 0;
  return metrics;
}

std::vector<ObservationEvent> StorageCollector::eventsFromMetrics(const StorageMetrics& metrics) const {
  std::vector<ObservationEvent> events;

  if (!metrics.stat_success) {
    ObservationEvent err_event;
    err_event.source = "storage_collector";
    err_event.category = "storage";
    err_event.subject = metrics.mount_path.string();
    err_event.signal = "stat_failure";
    err_event.value = std::monostate{};
    err_event.severity = Severity::Error;

    const json attr = {
        {"fstype", metrics.fstype},
        {"device_node", metrics.device_node},
        {"error", metrics.error},
    };
    err_event.attributes_json = attr.dump();
    events.push_back(err_event);
    return events;
  }

  const json attr = {
      {"total_bytes", metrics.total_bytes},
      {"used_bytes", metrics.used_bytes},
      {"available_bytes", metrics.available_bytes},
      {"total_inodes", metrics.total_inodes},
      {"used_inodes", metrics.used_inodes},
      {"free_inodes", metrics.free_inodes},
      {"fstype", metrics.fstype},
      {"mount_flags", metrics.options},
  };

  if (metrics.is_read_only) {
    ObservationEvent ro_event;
    ro_event.source = "storage_collector";
    ro_event.category = "storage";
    ro_event.subject = metrics.mount_path.string();
    ro_event.signal = "read_only_filesystem";
    ro_event.value = std::monostate{};
    ro_event.severity = Severity::Error;
    ro_event.attributes_json = attr.dump();
    events.push_back(ro_event);
  }

  if (metrics.percent_used >= options_.warning_threshold) {
    ObservationEvent space_event;
    space_event.source = "storage_collector";
    space_event.category = "storage";
    space_event.subject = metrics.mount_path.string();
    space_event.signal = "space_pressure";
    space_event.value = metrics.percent_used;
    space_event.severity = metrics.percent_used >= options_.critical_threshold ? Severity::Critical : Severity::Warning;
    space_event.attributes_json = attr.dump();
    events.push_back(space_event);
  }

  if (metrics.inode_percent_used >= options_.warning_threshold) {
    ObservationEvent inode_event;
    inode_event.source = "storage_collector";
    inode_event.category = "storage";
    inode_event.subject = metrics.mount_path.string();
    inode_event.signal = "inode_pressure";
    inode_event.value = metrics.inode_percent_used;
    inode_event.severity =
        metrics.inode_percent_used >= options_.critical_threshold ? Severity::Critical : Severity::Warning;
    inode_event.attributes_json = attr.dump();
    events.push_back(inode_event);
  }

  return events;
}

}  // namespace holonightd
