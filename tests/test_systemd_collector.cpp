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

#include "holonightd/Application.h"
#include "holonightd/SystemdCollector.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace holonightd {
namespace {

TEST(SystemdCollectorTest, DefaultOptionsInitialization) {
  const SystemdCollectorOptions opts;
  EXPECT_EQ(opts.flapping_threshold, 3);
  EXPECT_EQ(opts.flapping_window_seconds, 300);
  EXPECT_TRUE(opts.ignore_units.empty());
}

TEST(SystemdCollectorTest, InvalidOptionsFallback) {
  SystemdCollectorOptions invalid_opts;
  invalid_opts.flapping_threshold = -5;
  invalid_opts.flapping_window_seconds = 0;

  const SystemdCollector collector{invalid_opts};
  // Should complete without throwing exceptions
  SUCCEED();
}

TEST(SystemdCollectorTest, ConfigTomlParsingSystemdSection) {
  const std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "holonightd_test_systemd_config";
  std::filesystem::create_directories(temp_dir);
  const std::filesystem::path config_file = temp_dir / "test.toml";

  std::ofstream out{config_file};
  out << R"(
[general]
interval_seconds = 60
scan_root = "."

[systemd]
flapping_threshold = 5
flapping_window_seconds = 600
ignore_units = ["test-ignored.service", "user@1000.service"]
)";
  out.close();

  const Config config = Config::fromFile(config_file);
  EXPECT_EQ(config.systemd.flapping_threshold, 5);
  EXPECT_EQ(config.systemd.flapping_window_seconds, 600);
  ASSERT_EQ(config.systemd.ignore_units.size(), 2U);

  EXPECT_EQ(config.systemd.ignore_units[0], "test-ignored.service");
  EXPECT_EQ(config.systemd.ignore_units[1], "user@1000.service");

  std::filesystem::remove_all(temp_dir);
}

TEST(SystemdCollectorTest, CollectExecutesSafelyWithoutCrashing) {
  SystemdCollector collector;
  EXPECT_NO_THROW({
    const auto events = collector.collect();
    for (const auto& event : events) {
      EXPECT_EQ(event.source, "systemd");
      EXPECT_FALSE(event.subject.empty());
    }
  });
}

}  // namespace
}  // namespace holonightd
