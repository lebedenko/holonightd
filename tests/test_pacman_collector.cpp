// SPDX-License-Identifier: GPL-3.0-or-later
// holonightd - Lightweight daemon for routine maintenance automation.
// Copyright (C) 2026 holonightd contributors

#include "holonightd/PacmanCollector.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

using namespace holonightd;

class PacmanCollectorTestFixture : public ::testing::Test {
 protected:
  void SetUp() override {
    test_root_ = std::filesystem::temp_directory_path() / ("holonightd_pacman_test_" + std::to_string(rand()));
    std::filesystem::create_directories(test_root_ / "var/lib/pacman/local");
    std::filesystem::create_directories(test_root_ / "usr/lib/modules");
    std::filesystem::create_directories(test_root_ / "proc/sys/kernel");
    std::filesystem::create_directories(test_root_ / "etc");
  }

  void TearDown() override {
    std::error_code err_code;
    std::filesystem::remove_all(test_root_, err_code);
  }

  void writeOsRelease(const std::string& version) {
    std::ofstream out_stream(test_root_ / "proc/sys/kernel/osrelease");
    out_stream << version << "\n";
  }

  // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes,readability-identifier-naming)
  std::filesystem::path test_root_;
};

TEST(PacmanCollectorTest, DefaultOptionsInitialization) {
  PacmanCollectorOptions options;
  EXPECT_EQ(options.sys_root, std::filesystem::path{"/"});
  EXPECT_EQ(options.db_path, std::filesystem::path{"var/lib/pacman"});
  EXPECT_EQ(options.etc_path, std::filesystem::path{"etc"});
  EXPECT_EQ(options.max_depth, 3U);
  EXPECT_EQ(options.warning_threshold, 5U);
  EXPECT_TRUE(options.check_kernel);
  EXPECT_TRUE(options.check_locks);
  EXPECT_TRUE(options.check_orphans);
  EXPECT_TRUE(options.check_transactions);
}

TEST(PacmanCollectorTest, NonExistentPathReturnsEmptyEventsWithoutThrowing) {
  PacmanCollectorOptions options;
  options.sys_root = "/nonexistent_path_test_12345";
  PacmanCollector collector(options);

  std::vector<ObservationEvent> events;
  EXPECT_NO_THROW({ events = collector.collect(); });
  EXPECT_TRUE(events.empty());
}

TEST_F(PacmanCollectorTestFixture, DetectsKernelMismatchWhenModulesMissing) {
  writeOsRelease("6.10.2-arch1-1");
  // Create an updated kernel modules directory, but NOT for running kernel
  std::filesystem::create_directories(test_root_ / "usr/lib/modules/6.10.3-arch1-1");
  std::filesystem::create_directories(test_root_ / "var/lib/pacman/local/linux-6.10.3.arch1-1");

  PacmanCollectorOptions options;
  options.sys_root = test_root_;
  PacmanCollector collector(options);

  const auto metrics = collector.collectMetrics();
  EXPECT_EQ(metrics.running_kernel.value_or(""), "6.10.2-arch1-1");
  EXPECT_TRUE(metrics.kernel_mismatch_detected);

  const auto events = collector.collect();
  ASSERT_EQ(events.size(), 1U);
  EXPECT_EQ(events[0].signal, "pacman.kernel_mismatch");
  EXPECT_EQ(events[0].severity, Severity::Warning);
}

TEST_F(PacmanCollectorTestFixture, DetectsStaleDatabaseLock) {
  writeOsRelease("6.10.2-arch1-1");
  std::filesystem::create_directories(test_root_ / "usr/lib/modules/6.10.2-arch1-1");

  // Create a lock file with PID 999999 (dead process)
  {
    std::ofstream lock(test_root_ / "var/lib/pacman/db.lck");
    lock << "999999\n";
  }

  PacmanCollectorOptions options;
  options.sys_root = test_root_;
  PacmanCollector collector(options);

  const auto metrics = collector.collectMetrics();
  EXPECT_TRUE(metrics.lock.exists);
  EXPECT_FALSE(metrics.lock.is_active);
  EXPECT_EQ(metrics.lock.pid.value_or(0), 999999);
  EXPECT_EQ(metrics.lock.invalid_reason, "process_dead");

  const auto events = collector.collect();
  ASSERT_EQ(events.size(), 1U);
  EXPECT_EQ(events[0].signal, "pacman.stale_lock");
  EXPECT_EQ(events[0].severity, Severity::Warning);
}

TEST_F(PacmanCollectorTestFixture, DetectsActiveDatabaseLock) {
  writeOsRelease("6.10.2-arch1-1");
  std::filesystem::create_directories(test_root_ / "usr/lib/modules/6.10.2-arch1-1");

  // Create mock proc directory for PID 1234
  const auto mock_proc = test_root_ / "proc/1234";
  std::filesystem::create_directories(mock_proc);
  {
    std::ofstream comm(mock_proc / "comm");
    comm << "pacman\n";
  }

  // Create lock file pointing to 1234
  {
    std::ofstream lock(test_root_ / "var/lib/pacman/db.lck");
    lock << "1234\n";
  }

  PacmanCollectorOptions options;
  options.sys_root = test_root_;
  PacmanCollector collector(options);

  const auto metrics = collector.collectMetrics();
  EXPECT_TRUE(metrics.lock.exists);
  EXPECT_TRUE(metrics.lock.is_active);
  EXPECT_EQ(metrics.lock.pid.value_or(0), 1234);
  const auto events = collector.collect();
  ASSERT_EQ(events.size(), 1U);
  EXPECT_EQ(events[0].signal, "pacman.active_lock");
  EXPECT_EQ(events[0].severity, Severity::Info);
}

TEST_F(PacmanCollectorTestFixture, DetectsOrphanPacnewAndPacsaveFiles) {
  writeOsRelease("6.10.2-arch1-1");
  std::filesystem::create_directories(test_root_ / "usr/lib/modules/6.10.2-arch1-1");

  // Create orphan config files under /etc
  std::filesystem::create_directories(test_root_ / "etc/subfolder");
  {
    std::ofstream file_stream(test_root_ / "etc/pacman.conf.pacnew");
  }
  {
    std::ofstream file_stream(test_root_ / "etc/subfolder/mirrorlist.pacnew");
  }
  {
    std::ofstream file_stream(test_root_ / "etc/subfolder/custom.conf.pacsave");
  }

  PacmanCollectorOptions options;
  options.sys_root = test_root_;
  options.warning_threshold = 2;  // 3 files total >= 2 threshold -> Warning
  PacmanCollector collector(options);

  const auto metrics = collector.collectMetrics();
  EXPECT_EQ(metrics.pacnew_count, 2);
  EXPECT_EQ(metrics.pacsave_count, 1);
  EXPECT_EQ(metrics.orphan_files.size(), 3U);

  const auto events = collector.collect();
  ASSERT_EQ(events.size(), 1U);
  EXPECT_EQ(events[0].signal, "pacman.pacnew_files");
  EXPECT_EQ(events[0].severity, Severity::Warning);
  EXPECT_EQ(std::get<std::int64_t>(events[0].value), 3);
}

TEST_F(PacmanCollectorTestFixture, DetectsInterruptedTransactions) {
  writeOsRelease("6.10.2-arch1-1");
  std::filesystem::create_directories(test_root_ / "usr/lib/modules/6.10.2-arch1-1");

  // Create temporary interrupted transaction artifacts under var/lib/pacman/local
  std::filesystem::create_directories(test_root_ / "var/lib/pacman/local/linux-6.10.3.tmp");

  PacmanCollectorOptions options;
  options.sys_root = test_root_;
  PacmanCollector collector(options);

  const auto metrics = collector.collectMetrics();
  EXPECT_TRUE(metrics.interrupted_transaction_detected);
  EXPECT_EQ(metrics.transaction_artifacts.size(), 1U);

  const auto events = collector.collect();
  ASSERT_EQ(events.size(), 1U);

  EXPECT_EQ(events[0].signal, "pacman.interrupted_transaction");
  EXPECT_EQ(events[0].severity, Severity::Warning);
}
