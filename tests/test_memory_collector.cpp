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

#include "holonightd/MemoryCollector.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace holonightd {

class MemoryCollectorTest : public ::testing::Test {
 protected:
  // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
  std::filesystem::path mock_proc_dir;

  void SetUp() override {
    mock_proc_dir =
        std::filesystem::temp_directory_path() /
        ("holonightd_mock_proc_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(mock_proc_dir);
  }

  void TearDown() override {
    if (std::filesystem::exists(mock_proc_dir)) {
      std::filesystem::remove_all(mock_proc_dir);
    }
  }
};

TEST_F(MemoryCollectorTest, ParsesPsiMetricsAndFiresEvents) {
  const auto pressure_dir = mock_proc_dir / "pressure";
  std::filesystem::create_directories(pressure_dir);
  {
    std::ofstream out{pressure_dir / "memory"};
    out << "some avg10=12.50 avg60=5.00 avg300=1.00 total=54321\n";
    out << "full avg10=28.00 avg60=10.00 avg300=2.00 total=12345\n";
  }

  MemoryCollectorOptions options;
  options.some_warning_threshold = 10.0;
  options.full_critical_threshold = 25.0;
  options.proc_root = mock_proc_dir;

  MemoryCollector collector{options};
  const auto metrics = collector.collectMetrics();

  EXPECT_TRUE(metrics.psi_success);
  ASSERT_TRUE(metrics.psi_some_avg10.has_value());
  EXPECT_DOUBLE_EQ(*metrics.psi_some_avg10, 12.50);
  ASSERT_TRUE(metrics.psi_full_avg10.has_value());
  EXPECT_DOUBLE_EQ(*metrics.psi_full_avg10, 28.00);

  const auto events = collector.collect();
  ASSERT_EQ(events.size(), 2U);

  EXPECT_EQ(events[0].signal, "memory_pressure_some");
  EXPECT_EQ(events[0].severity, Severity::Warning);
  EXPECT_DOUBLE_EQ(std::get<double>(events[0].value), 12.50);

  EXPECT_EQ(events[1].signal, "memory_pressure_full");
  EXPECT_EQ(events[1].severity, Severity::Critical);
  EXPECT_DOUBLE_EQ(std::get<double>(events[1].value), 28.00);
}

TEST_F(MemoryCollectorTest, MeminfoFallbackWhenPsiMissing) {
  {
    std::ofstream out{mock_proc_dir / "meminfo"};
    out << "MemTotal:       1000000 kB\n";
    out << "MemAvailable:    100000 kB\n";
  }

  MemoryCollectorOptions options;
  options.meminfo_warning_threshold = 85.0;
  options.proc_root = mock_proc_dir;

  MemoryCollector collector{options};
  const auto metrics = collector.collectMetrics();

  EXPECT_FALSE(metrics.psi_success);
  EXPECT_TRUE(metrics.meminfo_success);
  EXPECT_EQ(metrics.total_bytes, 1024000000U);
  EXPECT_EQ(metrics.available_bytes, 102400000U);
  EXPECT_EQ(metrics.used_bytes, 921600000U);
  ASSERT_TRUE(metrics.percent_used.has_value());
  EXPECT_DOUBLE_EQ(*metrics.percent_used, 90.0);

  const auto events = collector.collect();
  ASSERT_EQ(events.size(), 1U);
  EXPECT_EQ(events[0].signal, "memory_used_high");
  EXPECT_EQ(events[0].severity, Severity::Warning);
  EXPECT_DOUBLE_EQ(std::get<double>(events[0].value), 90.0);
}

TEST_F(MemoryCollectorTest, VmstatOomBaselineAndDeltaTracking) {
  {
    std::ofstream out{mock_proc_dir / "vmstat"};
    out << "nr_free_pages 12345\n";
    out << "oom_kill 5\n";
  }

  MemoryCollectorOptions options;
  options.proc_root = mock_proc_dir;

  MemoryCollector collector{options};

  // Iteration 1: Baseline established (no OOM event emitted)
  auto events1 = collector.collect();
  EXPECT_TRUE(events1.empty());

  // Iteration 2: OOM kill counter increased to 7 + kmsg victim details
  {
    std::ofstream out{mock_proc_dir / "vmstat"};
    out << "nr_free_pages 12000\n";
    out << "oom_kill 7\n";
  }
  {
    std::ofstream out{mock_proc_dir / "kmsg"};
    out << "Out of memory: Kill process 1234 (python3) score 500 or sacrifice child\n";
  }

  auto events2 = collector.collect();
  ASSERT_EQ(events2.size(), 1U);
  EXPECT_EQ(events2[0].signal, "oom_killer_invoked");
  EXPECT_EQ(events2[0].severity, Severity::Error);
  EXPECT_DOUBLE_EQ(std::get<double>(events2[0].value), 2.0);

  const auto attr = nlohmann::json::parse(events2[0].attributes_json);
  EXPECT_EQ(attr["delta"].get<int>(), 2);
  EXPECT_EQ(attr["total_oom_kills"].get<int>(), 7);
  EXPECT_EQ(attr["victim_pid"].get<int>(), 1234);
  EXPECT_EQ(attr["victim_name"].get<std::string>(), "python3");
}

TEST_F(MemoryCollectorTest, HandlesMissingOrInvalidProcFilesGracefully) {
  MemoryCollectorOptions options;
  options.proc_root = mock_proc_dir / "non_existent_dir";

  MemoryCollector collector{options};
  const auto metrics = collector.collectMetrics();

  EXPECT_FALSE(metrics.psi_success);
  EXPECT_FALSE(metrics.meminfo_success);
  EXPECT_FALSE(metrics.vmstat_success);

  const auto events = collector.collect();
  EXPECT_TRUE(events.empty());
}

}  // namespace holonightd
