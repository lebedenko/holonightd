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
#include "holonightd/Logger.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <optional>
#include <string>

// RAII helper for environment variable isolation.
// setenv/unsetenv are not thread-safe — do not run with --gtest_parallel.
class EnvGuard {
 public:
  EnvGuard(std::string name, std::optional<std::string> value) : name_(std::move(name)) {
    const char* existing = std::getenv(name_.c_str());
    saved_ = (existing != nullptr) ? std::optional<std::string>{existing} : std::nullopt;
    if (value.has_value()) {
      ::setenv(name_.c_str(), value->c_str(), 1);
    } else {
      ::unsetenv(name_.c_str());
    }
  }

  ~EnvGuard() {
    if (saved_.has_value()) {
      ::setenv(name_.c_str(), saved_->c_str(), 1);
    } else {
      ::unsetenv(name_.c_str());
    }
  }

  EnvGuard(const EnvGuard&) = delete;
  EnvGuard& operator=(const EnvGuard&) = delete;
  EnvGuard(EnvGuard&&) = delete;
  EnvGuard& operator=(EnvGuard&&) = delete;

 private:
  std::string name_;
  std::optional<std::string> saved_;
};

// ─── Config::fromFile tests ───────────────────────────────────────────────────

TEST(ConfigFromFile, ReadsAllFields) {
  const auto path = std::filesystem::temp_directory_path() / "holonightd-test-all.toml";
  {
    std::ofstream out{path};
    out << "[general]\n";
    out << "interval_seconds = 42\n";
    out << "scan_root = \"/tmp\"\n";
    out << "commands = [\"echo hello\", \"echo goodbye\"]\n";
  }

  const auto config = holonightd::Config::fromFile(path);
  EXPECT_EQ(config.interval, std::chrono::seconds{42});
  EXPECT_EQ(config.scan_root, std::filesystem::path{"/tmp"});
  ASSERT_EQ(config.commands.size(), 2U);
  EXPECT_EQ(config.commands[0], "echo hello");
  EXPECT_EQ(config.commands[1], "echo goodbye");
  std::filesystem::remove(path);
}

TEST(ConfigFromFile, DefaultsWhenCommandsAbsent) {
  const auto path = std::filesystem::temp_directory_path() / "holonightd-test-no-cmds.toml";
  {
    std::ofstream out{path};
    out << "[general]\n";
    out << "interval_seconds = 10\n";
    out << "scan_root = \".\"\n";
  }

  const auto config = holonightd::Config::fromFile(path);
  EXPECT_EQ(config.interval, std::chrono::seconds{10});
  EXPECT_TRUE(config.commands.empty());
  std::filesystem::remove(path);
}

TEST(ConfigFromFile, ParsesMultipleCommands) {
  const auto path = std::filesystem::temp_directory_path() / "holonightd-test-cmds.toml";
  {
    std::ofstream out{path};
    out << "[general]\n";
    out << "interval_seconds = 1\n";
    out << "scan_root = \".\"\n";
    out << "commands = [\"a\", \"b\", \"c\"]\n";
  }

  const auto config = holonightd::Config::fromFile(path);
  ASSERT_EQ(config.commands.size(), 3U);
  EXPECT_EQ(config.commands[0], "a");
  EXPECT_EQ(config.commands[1], "b");
  EXPECT_EQ(config.commands[2], "c");
  std::filesystem::remove(path);
}

TEST(ConfigFromFile, EmptyCommandsArray) {
  const auto path = std::filesystem::temp_directory_path() / "holonightd-test-empty-cmds.toml";
  {
    std::ofstream out{path};
    out << "[general]\n";
    out << "interval_seconds = 5\n";
    out << "scan_root = \".\"\n";
    out << "commands = []\n";
  }

  const auto config = holonightd::Config::fromFile(path);
  EXPECT_TRUE(config.commands.empty());
  std::filesystem::remove(path);
}

TEST(ConfigFromFile, ThrowsWhenFileNotFound) {
  const auto path = std::filesystem::temp_directory_path() / "holonightd-does-not-exist.toml";
  std::filesystem::remove(path);

  EXPECT_THROW({ static_cast<void>(holonightd::Config::fromFile(path)); }, std::runtime_error);
}

TEST(ConfigFromFile, ThrowsOnMalformedToml) {
  const auto path = std::filesystem::temp_directory_path() / "holonightd-test-bad.toml";
  {
    std::ofstream out{path};
    out << "[general\n";
    out << "interval_seconds = !!!\n";
  }

  EXPECT_THROW({ static_cast<void>(holonightd::Config::fromFile(path)); }, std::exception);
  std::filesystem::remove(path);
}

TEST(ConfigFromFile, ThrowsOnNonPositiveInterval) {
  const auto path = std::filesystem::temp_directory_path() / "holonightd-test-zero.toml";
  {
    std::ofstream out{path};
    out << "[general]\n";
    out << "interval_seconds = 0\n";
    out << "scan_root = \".\"\n";
  }

  EXPECT_THROW({ static_cast<void>(holonightd::Config::fromFile(path)); }, std::runtime_error);
  std::filesystem::remove(path);
}

TEST(ConfigFromFile, ThrowsOnMissingGeneralTable) {
  const auto path = std::filesystem::temp_directory_path() / "holonightd-test-no-general.toml";
  {
    std::ofstream out{path};
    out << "interval_seconds = 300\n";
  }

  EXPECT_THROW({ static_cast<void>(holonightd::Config::fromFile(path)); }, std::runtime_error);
  std::filesystem::remove(path);
}

TEST(ConfigFromFile, ReadsStorageSection) {
  const auto path = std::filesystem::temp_directory_path() / "holonightd-test-storage.toml";
  {
    std::ofstream out{path};
    out << "[general]\n";
    out << "interval_seconds = 60\n";
    out << "scan_root = \".\"\n";
    out << "[storage]\n";
    out << "warning_threshold = 80.0\n";
    out << "critical_threshold = 90.0\n";
    out << "mount_points = [\"/\", \"/var\"]\n";
  }

  const auto config = holonightd::Config::fromFile(path);
  EXPECT_DOUBLE_EQ(config.storage_warning_threshold, 80.0);
  EXPECT_DOUBLE_EQ(config.storage_critical_threshold, 90.0);
  ASSERT_EQ(config.storage_mount_points.size(), 2U);
  EXPECT_EQ(config.storage_mount_points[0], "/");
  EXPECT_EQ(config.storage_mount_points[1], "/var");
  std::filesystem::remove(path);
}

// ─── resolveConfigPath tests ──────────────────────────────────────────────────

TEST(ResolveConfigPath, ExplicitOverrideTakesPrecedence) {
  const EnvGuard xdg_guard{"XDG_CONFIG_HOME", "/some/xdg"};
  const std::filesystem::path override{"/explicit/path.toml"};

  EXPECT_EQ(holonightd::resolveConfigPath(override), override);
}

TEST(ResolveConfigPath, UsesXdgConfigHome) {
  const EnvGuard xdg_guard{"XDG_CONFIG_HOME", "/custom"};

  const auto result = holonightd::resolveConfigPath();
  EXPECT_EQ(result, std::filesystem::path{"/custom/holonight/holonightd.toml"});
}

TEST(ResolveConfigPath, FallsBackToHome) {
  const EnvGuard xdg_guard{"XDG_CONFIG_HOME", std::nullopt};
  const EnvGuard home_guard{"HOME", "/home/testuser"};

  const auto result = holonightd::resolveConfigPath();
  EXPECT_EQ(result, std::filesystem::path{"/home/testuser/.config/holonight/holonightd.toml"});
}

TEST(ResolveConfigPath, EmptyXdgFallsBackToHome) {
  const EnvGuard xdg_guard{"XDG_CONFIG_HOME", ""};
  const EnvGuard home_guard{"HOME", "/home/testuser"};

  const auto result = holonightd::resolveConfigPath();
  EXPECT_EQ(result, std::filesystem::path{"/home/testuser/.config/holonight/holonightd.toml"});
}

TEST(ResolveConfigPath, ThrowsWhenHomeUnset) {
  const EnvGuard xdg_guard{"XDG_CONFIG_HOME", std::nullopt};
  const EnvGuard home_guard{"HOME", std::nullopt};

  EXPECT_THROW({ static_cast<void>(holonightd::resolveConfigPath()); }, std::runtime_error);
}

// ─── resolveLogLevel & log_level TOML tests ───────────────────────────────────

TEST(ConfigFromFile, ReadsLogLevelSetting) {
  const auto path = std::filesystem::temp_directory_path() / "holonightd-test-loglevel.toml";
  {
    std::ofstream out{path};
    out << "[general]\n";
    out << "interval_seconds = 60\n";
    out << "scan_root = \".\"\n";
    out << "log_level = \"warn\"\n";
  }

  const auto config = holonightd::Config::fromFile(path);
  ASSERT_TRUE(config.log_level.has_value());
  EXPECT_EQ(*config.log_level, "warn");
  std::filesystem::remove(path);
}

TEST(ConfigFromFile, ThrowsOnInvalidLogLevelSetting) {
  const auto path = std::filesystem::temp_directory_path() / "holonightd-test-bad-loglevel.toml";
  {
    std::ofstream out{path};
    out << "[general]\n";
    out << "interval_seconds = 60\n";
    out << "scan_root = \".\"\n";
    out << "log_level = \"invalid_level\"\n";
  }

  EXPECT_THROW({ static_cast<void>(holonightd::Config::fromFile(path)); }, std::invalid_argument);
  std::filesystem::remove(path);
}

TEST(ResolveLogLevel, CliDebugFlagTakesTopPriority) {
  const EnvGuard env_guard{"HOLONIGHTD_LOG_LEVEL", "error"};
  holonightd::CliOptions cli;
  cli.debug = true;
  holonightd::Config config;
  config.log_level = "info";

  EXPECT_EQ(holonightd::resolveLogLevel(cli, config), holonightd::LogLevel::Debug);
}

TEST(ResolveLogLevel, EnvVarOverridesTomlConfig) {
  const EnvGuard env_guard{"HOLONIGHTD_LOG_LEVEL", "warn"};
  holonightd::CliOptions cli;
  cli.debug = false;
  holonightd::Config config;
  config.log_level = "info";

  EXPECT_EQ(holonightd::resolveLogLevel(cli, config), holonightd::LogLevel::Warn);
}

TEST(ResolveLogLevel, TomlConfigUsedWhenEnvUnset) {
  const EnvGuard env_guard{"HOLONIGHTD_LOG_LEVEL", std::nullopt};
  holonightd::CliOptions cli;
  cli.debug = false;
  holonightd::Config config;
  config.log_level = "error";

  EXPECT_EQ(holonightd::resolveLogLevel(cli, config), holonightd::LogLevel::Error);
}

TEST(ResolveLogLevel, BuildDefaultUsedWhenEnvAndTomlUnset) {
  const EnvGuard env_guard{"HOLONIGHTD_LOG_LEVEL", std::nullopt};
  holonightd::CliOptions cli;
  cli.debug = false;
  holonightd::Config config;
  config.log_level = std::nullopt;

  const auto level = holonightd::resolveLogLevel(cli, config);
#ifdef NDEBUG
  EXPECT_EQ(level, holonightd::LogLevel::Info);
#else
  EXPECT_EQ(level, holonightd::LogLevel::Debug);
#endif
}

// ─── DatabaseConfig & resolveDatabasePath tests ──────────────────────────────

TEST(ConfigFromFile, RejectsInvalidStorageThresholdOrdering) {
  const auto path = std::filesystem::temp_directory_path() / "holonightd-test-storage-order.toml";
  {
    std::ofstream out{path};
    out << "[general]\ninterval_seconds = 60\nscan_root = \".\"\n";
    out << "[storage]\nwarning_threshold = 96\ncritical_threshold = 95\n";
  }

  EXPECT_THROW(static_cast<void>(holonightd::Config::fromFile(path)), std::runtime_error);
  std::filesystem::remove(path);
}

TEST(ConfigFromFile, RejectsMemoryThresholdOutsidePercentageRange) {
  const auto path = std::filesystem::temp_directory_path() / "holonightd-test-memory-range.toml";
  {
    std::ofstream out{path};
    out << "[general]\ninterval_seconds = 60\nscan_root = \".\"\n";
    out << "[memory]\nsome_warning_threshold = 101\n";
  }

  EXPECT_THROW(static_cast<void>(holonightd::Config::fromFile(path)), std::runtime_error);
  std::filesystem::remove(path);
}

TEST(ConfigFromFile, ReadsDatabaseSection) {
  const auto path = std::filesystem::temp_directory_path() / "holonightd-test-database.toml";
  {
    std::ofstream out{path};
    out << "[general]\n";
    out << "interval_seconds = 60\n";
    out << "scan_root = \".\"\n";
    out << "[database]\n";
    out << "path = \"/var/db/events.sqlite\"\n";
    out << "retention_days = 14\n";
    out << "max_bytes = 10485760\n";
    out << "max_events = 50000\n";
  }

  const auto config = holonightd::Config::fromFile(path);
  ASSERT_TRUE(config.database.path.has_value());
  EXPECT_EQ(*config.database.path, std::filesystem::path{"/var/db/events.sqlite"});
  ASSERT_TRUE(config.database.retention_days.has_value());
  EXPECT_EQ(*config.database.retention_days, 14);
  ASSERT_TRUE(config.database.max_bytes.has_value());
  EXPECT_EQ(*config.database.max_bytes, 10485760U);
  ASSERT_TRUE(config.database.max_events.has_value());
  EXPECT_EQ(*config.database.max_events, 50000U);
  std::filesystem::remove(path);
}

TEST(ConfigFromFile, DatabaseSectionDefaultsWhenOmitted) {
  const auto path = std::filesystem::temp_directory_path() / "holonightd-test-no-db.toml";
  {
    std::ofstream out{path};
    out << "[general]\n";
    out << "interval_seconds = 60\n";
    out << "scan_root = \".\"\n";
  }

  const auto config = holonightd::Config::fromFile(path);
  EXPECT_FALSE(config.database.path.has_value());
  ASSERT_TRUE(config.database.retention_days.has_value());
  EXPECT_EQ(*config.database.retention_days, 30);
  ASSERT_TRUE(config.database.max_bytes.has_value());
  EXPECT_EQ(*config.database.max_bytes, 52428800U);
  ASSERT_TRUE(config.database.max_events.has_value());
  EXPECT_EQ(*config.database.max_events, 100000U);
  std::filesystem::remove(path);
}

TEST(ConfigFromFile, ThrowsOnNonPositiveRetentionDays) {
  const auto path = std::filesystem::temp_directory_path() / "holonightd-test-bad-retention.toml";
  {
    std::ofstream out{path};
    out << "[general]\n";
    out << "interval_seconds = 60\n";
    out << "scan_root = \".\"\n";
    out << "[database]\n";
    out << "retention_days = 0\n";
  }

  EXPECT_THROW({ static_cast<void>(holonightd::Config::fromFile(path)); }, std::runtime_error);
  std::filesystem::remove(path);
}

TEST(ConfigFromFile, ThrowsOnNonPositiveMaxBytes) {
  const auto path = std::filesystem::temp_directory_path() / "holonightd-test-bad-maxbytes.toml";
  {
    std::ofstream out{path};
    out << "[general]\n";
    out << "interval_seconds = 60\n";
    out << "scan_root = \".\"\n";
    out << "[database]\n";
    out << "max_bytes = 0\n";
  }

  EXPECT_THROW({ static_cast<void>(holonightd::Config::fromFile(path)); }, std::runtime_error);
  std::filesystem::remove(path);
}

TEST(ConfigFromFile, ThrowsOnNonPositiveMaxEvents) {
  const auto path = std::filesystem::temp_directory_path() / "holonightd-test-bad-maxevents.toml";
  {
    std::ofstream out{path};
    out << "[general]\n";
    out << "interval_seconds = 60\n";
    out << "scan_root = \".\"\n";
    out << "[database]\n";
    out << "max_events = 0\n";
  }

  EXPECT_THROW({ static_cast<void>(holonightd::Config::fromFile(path)); }, std::runtime_error);
  std::filesystem::remove(path);
}

TEST(ConfigFromFile, ReadsMemorySection) {
  const auto path = std::filesystem::temp_directory_path() / "holonightd-test-memory.toml";
  {
    std::ofstream out{path};
    out << "[general]\n";
    out << "interval_seconds = 60\n";
    out << "scan_root = \".\"\n";
    out << "[memory]\n";
    out << "some_warning_threshold = 15.5\n";
    out << "full_critical_threshold = 30.0\n";
    out << "meminfo_warning_threshold = 90.0\n";
  }

  const auto config = holonightd::Config::fromFile(path);
  EXPECT_DOUBLE_EQ(config.memory.some_warning_threshold, 15.5);
  EXPECT_DOUBLE_EQ(config.memory.full_critical_threshold, 30.0);
  EXPECT_DOUBLE_EQ(config.memory.meminfo_warning_threshold, 90.0);
  std::filesystem::remove(path);
}

TEST(ConfigFromFile, MemorySectionDefaultsWhenOmitted) {
  const auto path = std::filesystem::temp_directory_path() / "holonightd-test-no-memory.toml";
  {
    std::ofstream out{path};
    out << "[general]\n";
    out << "interval_seconds = 60\n";
    out << "scan_root = \".\"\n";
  }

  const auto config = holonightd::Config::fromFile(path);
  EXPECT_DOUBLE_EQ(config.memory.some_warning_threshold, 10.0);
  EXPECT_DOUBLE_EQ(config.memory.full_critical_threshold, 25.0);
  EXPECT_DOUBLE_EQ(config.memory.meminfo_warning_threshold, 85.0);
  std::filesystem::remove(path);
}

TEST(ResolveDatabasePath, ExplicitOverrideTakesPrecedence) {
  const EnvGuard xdg_guard{"XDG_DATA_HOME", "/some/xdg"};
  const std::filesystem::path override{"/explicit/events.db"};

  EXPECT_EQ(holonightd::resolveDatabasePath(override), override);
}

TEST(ResolveDatabasePath, UsesXdgDataHome) {
  const EnvGuard xdg_guard{"XDG_DATA_HOME", "/custom/data"};

  const auto result = holonightd::resolveDatabasePath();
  EXPECT_EQ(result, std::filesystem::path{"/custom/data/holonight/events.db"});
}

TEST(ResolveDatabasePath, FallsBackToHome) {
  const EnvGuard xdg_guard{"XDG_DATA_HOME", std::nullopt};
  const EnvGuard home_guard{"HOME", "/home/testuser"};

  const auto result = holonightd::resolveDatabasePath();
  EXPECT_EQ(result, std::filesystem::path{"/home/testuser/.local/share/holonight/events.db"});
}

TEST(ResolveDatabasePath, EmptyXdgFallsBackToHome) {
  const EnvGuard xdg_guard{"XDG_DATA_HOME", ""};
  const EnvGuard home_guard{"HOME", "/home/testuser"};

  const auto result = holonightd::resolveDatabasePath();
  EXPECT_EQ(result, std::filesystem::path{"/home/testuser/.local/share/holonight/events.db"});
}

TEST(ResolveDatabasePath, ThrowsWhenHomeUnset) {
  const EnvGuard xdg_guard{"XDG_DATA_HOME", std::nullopt};
  const EnvGuard home_guard{"HOME", std::nullopt};

  EXPECT_THROW({ static_cast<void>(holonightd::resolveDatabasePath()); }, std::runtime_error);
}

TEST(ConfigFromFile, ReadsPacmanAndRulesSection) {
  const auto path = std::filesystem::temp_directory_path() / "holonightd-test-pacman.toml";
  {
    std::ofstream out{path};
    out << "[general]\n";
    out << "interval_seconds = 60\n";
    out << "scan_root = \".\"\n";
    out << "[pacman]\n";
    out << "sys_root = \"/tmp/sys\"\n";
    out << "max_depth = 8\n";
    out << "warning_threshold = 10\n";
    out << "check_kernel = false\n";
    out << "[rules]\n";
    out << "rules_dir = \"/etc/custom_rules\"\n";
  }

  const auto config = holonightd::Config::fromFile(path);
  EXPECT_EQ(config.pacman.sys_root, std::filesystem::path{"/tmp/sys"});
  EXPECT_EQ(config.pacman.max_depth, 8U);
  EXPECT_EQ(config.pacman.warning_threshold, 10U);
  EXPECT_FALSE(config.pacman.check_kernel);
  ASSERT_TRUE(config.rules_dir.has_value());
  EXPECT_EQ(*config.rules_dir, std::filesystem::path{"/etc/custom_rules"});
  std::filesystem::remove(path);
}

TEST(ConfigFromFile, PacmanAndRulesDefaultsWhenOmitted) {
  const auto path = std::filesystem::temp_directory_path() / "holonightd-test-default-pacman.toml";
  {
    std::ofstream out{path};
    out << "[general]\n";
    out << "interval_seconds = 60\n";
    out << "scan_root = \".\"\n";
  }

  const auto config = holonightd::Config::fromFile(path);
  EXPECT_EQ(config.pacman.sys_root, std::filesystem::path{"/"});
  EXPECT_EQ(config.pacman.max_depth, 3U);
  EXPECT_EQ(config.pacman.warning_threshold, 5U);
  EXPECT_TRUE(config.pacman.check_kernel);
  EXPECT_FALSE(config.rules_dir.has_value());
  std::filesystem::remove(path);
}
