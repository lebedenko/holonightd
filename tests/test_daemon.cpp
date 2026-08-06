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
#include "holonightd/Daemon.h"
#include "holonightd/EventStore.h"
#include "holonightd/Logger.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>
#include <sstream>
#include <stdexcept>

TEST(DaemonTest, InitializesEventStoreAndPersistsEventsOnIteration) {
  const auto db_path = std::filesystem::temp_directory_path() / "test_daemon_events_persist.db";
  std::filesystem::remove(db_path);

  holonightd::Config config;
  config.scan_root = std::filesystem::temp_directory_path();
  config.storage_warning_threshold = 0.1;
  std::ostringstream null_stream;
  holonightd::Logger logger{null_stream};

  {
    holonightd::Daemon daemon{config, logger, db_path, false};
    std::atomic_bool stop{true};
    daemon.run(stop, holonightd::RunMode::Once);
  }

  EXPECT_TRUE(std::filesystem::exists(db_path));

  holonightd::EventStore store_reader{db_path};
  const auto query_res = store_reader.query({});
  ASSERT_TRUE(query_res.has_value());
  EXPECT_FALSE(query_res->empty());

  std::filesystem::remove(db_path);
}

TEST(DaemonTest, PrunesOldEventsOnIteration) {
  const auto db_path = std::filesystem::temp_directory_path() / "test_daemon_events_prune.db";
  std::filesystem::remove(db_path);

  // Pre-seed an event older than 30 days
  {
    holonightd::EventStore store_seed{db_path};
    holonightd::ObservationEvent old_event;
    old_event.source = "test_source";
    old_event.category = "test_category";
    old_event.subject = "test_subject";
    old_event.signal = "test_signal";
    old_event.timestamp = std::chrono::system_clock::now() - std::chrono::days(45);
    old_event.severity = holonightd::Severity::Info;

    const auto insert_res = store_seed.insert(old_event);
    ASSERT_TRUE(insert_res.has_value());
  }

  holonightd::Config config;
  config.scan_root = std::filesystem::temp_directory_path();
  config.database.retention_days = 30;
  std::ostringstream null_stream;
  holonightd::Logger logger{null_stream};

  {
    holonightd::Daemon daemon{config, logger, db_path, false};
    std::atomic_bool stop{true};
    daemon.run(stop, holonightd::RunMode::Once);
  }

  holonightd::EventStore store_reader{db_path};
  holonightd::EventQuery query;
  query.source = "test_source";
  const auto query_res = store_reader.query(query);
  ASSERT_TRUE(query_res.has_value());
  EXPECT_TRUE(query_res->empty());

  std::filesystem::remove(db_path);
}

#include <thread>

TEST(DaemonTest, ThrowsOnInvalidDatabasePath) {
  const std::filesystem::path invalid_db_path{"/proc/nonexistent_dir_12345/events.db"};
  holonightd::Config config;
  std::ostringstream null_stream;
  holonightd::Logger logger{null_stream};

  EXPECT_THROW({ holonightd::Daemon daemon(config, logger, invalid_db_path, false); }, std::exception);
}

TEST(DaemonTest, RespondsPromptlyToStopSignal) {
  const auto db_path = std::filesystem::temp_directory_path() / "test_daemon_stop_signal.db";
  std::filesystem::remove(db_path);

  holonightd::Config config;
  config.scan_root = std::filesystem::temp_directory_path();
  config.interval = std::chrono::seconds(60);
  std::ostringstream null_stream;
  holonightd::Logger logger{null_stream};

  std::atomic_bool stop{false};
  const auto start_time = std::chrono::steady_clock::now();

  std::thread daemon_thread([&]() {
    holonightd::Daemon daemon{config, logger, db_path, false};
    daemon.run(stop, holonightd::RunMode::Loop);
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  stop.store(true);
  daemon_thread.join();

  const auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
  EXPECT_LT(elapsed, std::chrono::seconds(2));

  std::filesystem::remove(db_path);
}

TEST(DaemonTest, RunStatusCheckExecutesAndFormatsReport) {
  const auto db_path = std::filesystem::temp_directory_path() / "test_daemon_status_check.db";
  std::filesystem::remove(db_path);

  holonightd::Config config;
  config.scan_root = std::filesystem::temp_directory_path();
  std::ostringstream null_stream;
  holonightd::Logger logger{null_stream};

  holonightd::Daemon daemon{config, logger, db_path, false};
  const int exit_code = daemon.runStatusCheck();
  EXPECT_TRUE(exit_code == 0 || exit_code == 1);

  std::vector<holonightd::DiagnosticFinding> empty_findings;
  const std::string report = holonightd::Daemon::formatStatusReport(empty_findings);
  EXPECT_NE(report.find("holonightd System Status Report"), std::string::npos);

  EXPECT_NE(report.find("Overall Status: OK"), std::string::npos);

  std::filesystem::remove(db_path);
}
