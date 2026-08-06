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

#include <algorithm>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace holonightd {

TEST(StorageCollectorTest, DiscoverMountsReturnsLocalPhysicalMounts) {
  const auto mounts = StorageCollector::discoverMounts();
  ASSERT_FALSE(mounts.empty());

  const auto root_it = std::ranges::find_if(mounts, [](const MountInfo& info) { return info.mount_path == "/"; });
  EXPECT_NE(root_it, mounts.end());

  for (const auto& mount : mounts) {
    EXPECT_NE(mount.fstype, "proc");
    EXPECT_NE(mount.fstype, "sysfs");
    EXPECT_NE(mount.fstype, "tmpfs");
    EXPECT_NE(mount.fstype, "devtmpfs");
    EXPECT_NE(mount.fstype, "nfs");
    EXPECT_FALSE(mount.mount_path.string().starts_with("/run/media/"));
    EXPECT_FALSE(mount.mount_path.string().starts_with("/media/"));
  }
}

TEST(StorageCollectorTest, CollectWithHighWarningThresholdEmitsNoPressureEvents) {
  StorageCollectorOptions options;
  options.warning_threshold = 100.0;
  options.critical_threshold = 100.0;
  options.mount_points = {"/"};

  StorageCollector collector{options};
  const auto events = collector.collect();

  for (const auto& event : events) {
    EXPECT_NE(event.signal, "stat_failure");
  }
}

TEST(StorageCollectorTest, CollectWithLowWarningThresholdTriggersSpacePressure) {
  StorageCollectorOptions options;
  options.warning_threshold = 0.0;
  options.critical_threshold = 100.0;
  options.mount_points = {"/"};

  StorageCollector collector{options};
  const auto events = collector.collect();

  const auto space_it =
      std::ranges::find_if(events, [](const ObservationEvent& event) { return event.signal == "space_pressure"; });

  ASSERT_NE(space_it, events.end());
  EXPECT_EQ(space_it->source, "storage_collector");
  EXPECT_EQ(space_it->category, "storage");
  EXPECT_EQ(space_it->subject, "/");
  EXPECT_EQ(space_it->severity, Severity::Warning);

  const auto attr = nlohmann::json::parse(space_it->attributes_json);
  EXPECT_TRUE(attr.contains("total_bytes"));
  EXPECT_TRUE(attr.contains("used_bytes"));
  EXPECT_TRUE(attr.contains("available_bytes"));
  EXPECT_TRUE(attr.contains("total_inodes"));
  EXPECT_TRUE(attr.contains("used_inodes"));
  EXPECT_TRUE(attr.contains("free_inodes"));
  EXPECT_TRUE(attr.contains("fstype"));
}

TEST(StorageCollectorTest, CollectWithLowCriticalThresholdTriggersCriticalSeverity) {
  StorageCollectorOptions options;
  options.warning_threshold = 0.0;
  options.critical_threshold = 0.0;
  options.mount_points = {"/"};

  StorageCollector collector{options};
  const auto events = collector.collect();

  const auto space_it =
      std::ranges::find_if(events, [](const ObservationEvent& event) { return event.signal == "space_pressure"; });

  ASSERT_NE(space_it, events.end());
  EXPECT_EQ(space_it->severity, Severity::Critical);
}

TEST(StorageCollectorTest, CollectHandlesNonExistentMountGracefully) {
  StorageCollectorOptions options;
  options.mount_points = {"/non_existent_mount_path_xyz_12345"};

  StorageCollector collector{options};
  const auto events = collector.collect();

  ASSERT_EQ(events.size(), 1U);
  EXPECT_EQ(events[0].source, "storage_collector");
  EXPECT_EQ(events[0].category, "storage");
  EXPECT_EQ(events[0].subject, "/non_existent_mount_path_xyz_12345");
  EXPECT_EQ(events[0].signal, "stat_failure");
  EXPECT_EQ(events[0].severity, Severity::Error);

  const auto attr = nlohmann::json::parse(events[0].attributes_json);
  EXPECT_TRUE(attr.contains("error"));
}

TEST(StorageCollectorTest, CollectMetricsReturnsDetailedMetrics) {
  StorageCollectorOptions options;
  options.mount_points = {"/"};

  StorageCollector collector{options};
  const auto metrics = collector.collectMetrics();

  ASSERT_EQ(metrics.size(), 1U);
  EXPECT_EQ(metrics[0].mount_path, "/");
  EXPECT_TRUE(metrics[0].stat_success);
  EXPECT_GT(metrics[0].total_bytes, 0U);
  EXPECT_GT(metrics[0].total_inodes, 0U);
}

}  // namespace holonightd
