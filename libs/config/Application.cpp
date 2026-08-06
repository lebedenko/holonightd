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

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string_view>

// NOLINTBEGIN
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-literal-operator"
#include <toml.hpp>
#pragma GCC diagnostic pop
// NOLINTEND

namespace holonightd {

namespace {

void validatePercentage(double value, std::string_view name) {
  if (!std::isfinite(value) || value < 0.0 || value > 100.0) {
    throw std::runtime_error{std::string{name} + " must be between 0 and 100"};
  }
}

void parseGeneralSection(const toml::table& tbl, Config& config) {
  const toml::node* general_node = tbl.get("general");
  if (general_node == nullptr) {
    throw std::runtime_error{"config is missing required [general] table"};
  }
  const toml::table* general = general_node->as_table();
  if (general == nullptr) {
    throw std::runtime_error{"config is missing required [general] table"};
  }

  const auto* interval_node = general->get_as<int64_t>("interval_seconds");
  if (interval_node == nullptr) {
    throw std::runtime_error{"config key 'interval_seconds' is required in [general] section"};
  }
  if (interval_node->get() <= 0) {
    throw std::runtime_error{"interval_seconds must be a positive integer"};
  }
  config.interval = std::chrono::seconds{interval_node->get()};

  const auto* scan_root_node = general->get_as<std::string>("scan_root");
  if (scan_root_node == nullptr) {
    throw std::runtime_error{"config key 'scan_root' is required in [general] section"};
  }
  config.scan_root = scan_root_node->get();

  if (const auto* log_level_node = general->get_as<std::string>("log_level")) {
    const std::string level_str = log_level_node->get();
    static_cast<void>(parseLogLevel(level_str));
    config.log_level = level_str;
  }

  const toml::node* commands_node = general->get("commands");

  if (commands_node == nullptr) {
    return;
  }
  const toml::array* commands_arr = commands_node->as_array();
  if (commands_arr == nullptr) {
    return;
  }
  for (const auto& elem : *commands_arr) {
    if (const auto* str = elem.as_string()) {
      config.commands.push_back(str->get());
    }
  }
}

void parseStorageSection(const toml::table& tbl, Config& config) {
  const toml::node* storage_node = tbl.get("storage");
  if (storage_node == nullptr) {
    return;
  }
  const toml::table* storage = storage_node->as_table();
  if (storage == nullptr) {
    return;
  }

  if (const auto* warn = storage->get_as<double>("warning_threshold")) {
    config.storage_warning_threshold = warn->get();
  } else if (const auto* warn_int = storage->get_as<int64_t>("warning_threshold")) {
    config.storage_warning_threshold = static_cast<double>(warn_int->get());
  }

  if (const auto* crit = storage->get_as<double>("critical_threshold")) {
    config.storage_critical_threshold = crit->get();
  } else if (const auto* crit_int = storage->get_as<int64_t>("critical_threshold")) {
    config.storage_critical_threshold = static_cast<double>(crit_int->get());
  }

  validatePercentage(config.storage_warning_threshold, "storage.warning_threshold");
  validatePercentage(config.storage_critical_threshold, "storage.critical_threshold");
  if (config.storage_warning_threshold > config.storage_critical_threshold) {
    throw std::runtime_error{"storage.warning_threshold must not exceed storage.critical_threshold"};
  }

  const toml::node* mounts_node = storage->get("mount_points");
  if (mounts_node == nullptr) {
    return;
  }
  const toml::array* mounts_arr = mounts_node->as_array();
  if (mounts_arr == nullptr) {
    return;
  }
  for (const auto& elem : *mounts_arr) {
    if (const auto* str = elem.as_string()) {
      config.storage_mount_points.emplace_back(str->get());
    }
  }
}

void parseDatabaseSection(const toml::table& tbl, Config& config) {
  const toml::node* db_node = tbl.get("database");
  if (db_node == nullptr) {
    return;
  }
  const toml::table* db_table = db_node->as_table();
  if (db_table == nullptr) {
    return;
  }

  if (const auto* path_node = db_table->get_as<std::string>("path")) {
    config.database.path = std::filesystem::path{path_node->get()};
  }

  if (const auto* ret_node = db_table->get_as<int64_t>("retention_days")) {
    if (ret_node->get() <= 0) {
      throw std::runtime_error{"retention_days must be a positive integer"};
    }
    config.database.retention_days = static_cast<int>(ret_node->get());
  }

  if (const auto* max_b = db_table->get_as<int64_t>("max_bytes")) {
    if (max_b->get() <= 0) {
      throw std::runtime_error{"max_bytes must be a positive integer"};
    }
    config.database.max_bytes = static_cast<size_t>(max_b->get());
  }

  if (const auto* max_e = db_table->get_as<int64_t>("max_events")) {
    if (max_e->get() <= 0) {
      throw std::runtime_error{"max_events must be a positive integer"};
    }
    config.database.max_events = static_cast<size_t>(max_e->get());
  }
}

void parseSystemdSection(const toml::table& tbl, Config& config) {
  const toml::node* systemd_node = tbl.get("systemd");
  if (systemd_node == nullptr) {
    return;
  }
  const toml::table* systemd_table = systemd_node->as_table();
  if (systemd_table == nullptr) {
    return;
  }

  if (const auto* threshold_node = systemd_table->get_as<int64_t>("flapping_threshold")) {
    if (threshold_node->get() <= 0) {
      std::cerr << "warning: flapping_threshold must be positive; falling back to default "
                << config.systemd.flapping_threshold << "\n";
    } else {
      config.systemd.flapping_threshold = static_cast<int>(threshold_node->get());
    }
  }

  if (const auto* window_node = systemd_table->get_as<int64_t>("flapping_window_seconds")) {
    if (window_node->get() <= 0) {
      std::cerr << "warning: flapping_window_seconds must be positive; falling back to default "
                << config.systemd.flapping_window_seconds << "\n";
    } else {
      config.systemd.flapping_window_seconds = static_cast<int>(window_node->get());
    }
  }

  const toml::node* ignore_node = systemd_table->get("ignore_units");
  if (ignore_node != nullptr) {
    if (const toml::array* ignore_arr = ignore_node->as_array()) {
      for (const auto& elem : *ignore_arr) {
        if (const auto* str = elem.as_string()) {
          config.systemd.ignore_units.push_back(str->get());
        }
      }
    }
  }
}

void parseMemorySection(const toml::table& tbl, Config& config) {
  const toml::node* mem_node = tbl.get("memory");
  if (mem_node == nullptr) {
    return;
  }
  const toml::table* mem = mem_node->as_table();
  if (mem == nullptr) {
    return;
  }

  if (const auto* some_warn = mem->get_as<double>("some_warning_threshold")) {
    config.memory.some_warning_threshold = some_warn->get();
  } else if (const auto* some_int = mem->get_as<int64_t>("some_warning_threshold")) {
    config.memory.some_warning_threshold = static_cast<double>(some_int->get());
  }

  if (const auto* full_crit = mem->get_as<double>("full_critical_threshold")) {
    config.memory.full_critical_threshold = full_crit->get();
  } else if (const auto* full_int = mem->get_as<int64_t>("full_critical_threshold")) {
    config.memory.full_critical_threshold = static_cast<double>(full_int->get());
  }

  if (const auto* meminfo_warn = mem->get_as<double>("meminfo_warning_threshold")) {
    config.memory.meminfo_warning_threshold = meminfo_warn->get();
  } else if (const auto* meminfo_int = mem->get_as<int64_t>("meminfo_warning_threshold")) {
    config.memory.meminfo_warning_threshold = static_cast<double>(meminfo_int->get());
  }

  validatePercentage(config.memory.some_warning_threshold, "memory.some_warning_threshold");
  validatePercentage(config.memory.full_critical_threshold, "memory.full_critical_threshold");
  validatePercentage(config.memory.meminfo_warning_threshold, "memory.meminfo_warning_threshold");
}

void parsePacmanSection(const toml::table& tbl, Config& config) {
  const toml::node* pacman_node = tbl.get("pacman");
  if (pacman_node == nullptr) {
    return;
  }
  const toml::table* pacman_table = pacman_node->as_table();
  if (pacman_table == nullptr) {
    return;
  }

  if (const auto* val = pacman_table->get_as<std::string>("sys_root")) {
    config.pacman.sys_root = val->get();
  }
  if (const auto* val = pacman_table->get_as<std::string>("db_path")) {
    config.pacman.db_path = val->get();
  }
  if (const auto* val = pacman_table->get_as<std::string>("etc_path")) {
    config.pacman.etc_path = val->get();
  }
  if (const auto* val = pacman_table->get_as<int64_t>("max_depth")) {
    if (val->get() >= 0) {
      config.pacman.max_depth = static_cast<unsigned int>(val->get());
    }
  }
  if (const auto* val = pacman_table->get_as<int64_t>("warning_threshold")) {
    if (val->get() >= 0) {
      config.pacman.warning_threshold = static_cast<unsigned int>(val->get());
    }
  }
  if (const auto* val = pacman_table->get_as<bool>("check_kernel")) {
    config.pacman.check_kernel = val->get();
  }
  if (const auto* val = pacman_table->get_as<bool>("check_locks")) {
    config.pacman.check_locks = val->get();
  }
  if (const auto* val = pacman_table->get_as<bool>("check_orphans")) {
    config.pacman.check_orphans = val->get();
  }
  if (const auto* val = pacman_table->get_as<bool>("check_transactions")) {
    config.pacman.check_transactions = val->get();
  }
}

void parseRulesSection(const toml::table& tbl, Config& config) {
  const toml::node* rules_node = tbl.get("rules");
  if (rules_node == nullptr) {
    return;
  }
  const toml::table* rules_table = rules_node->as_table();
  if (rules_table == nullptr) {
    return;
  }

  if (const auto* val = rules_table->get_as<std::string>("rules_dir")) {
    config.rules_dir = std::filesystem::path{val->get()};
  }
}

}  // namespace

std::filesystem::path resolveConfigPath(std::optional<std::filesystem::path> explicit_override) {
  if (explicit_override.has_value()) {
    return *explicit_override;
  }

  const char* const xdg = std::getenv("XDG_CONFIG_HOME");
  if (xdg != nullptr && !std::string_view{xdg}.empty()) {
    return std::filesystem::path{xdg} / "holonight" / "holonightd.toml";
  }

  const char* const home = std::getenv("HOME");
  if (home == nullptr || std::string_view{home}.empty()) {
    throw std::runtime_error{"$HOME is unset; cannot resolve config path"};
  }

  return std::filesystem::path{home} / ".config" / "holonight" / "holonightd.toml";
}

std::filesystem::path resolveDatabasePath(std::optional<std::filesystem::path> explicit_path) {
  if (explicit_path.has_value() && !explicit_path->empty()) {
    return *explicit_path;
  }

  const char* const xdg = std::getenv("XDG_DATA_HOME");
  if (xdg != nullptr && !std::string_view{xdg}.empty()) {
    return std::filesystem::path{xdg} / "holonight" / "events.db";
  }

  const char* const home = std::getenv("HOME");
  if (home == nullptr || std::string_view{home}.empty()) {
    throw std::runtime_error{"$HOME is unset; cannot resolve database path"};
  }

  return std::filesystem::path{home} / ".local" / "share" / "holonight" / "events.db";
}

Config Config::fromFile(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error{"config file not found: " + path.string()};
  }

  std::ifstream input{path};
  if (!input) {
    throw std::runtime_error{"failed to open config file: " + path.string()};
  }

  toml::table tbl = toml::parse(input, path.string());

  Config config;
  parseGeneralSection(tbl, config);
  parseStorageSection(tbl, config);
  parseDatabaseSection(tbl, config);
  parseSystemdSection(tbl, config);
  parseMemorySection(tbl, config);
  parsePacmanSection(tbl, config);
  parseRulesSection(tbl, config);

  return config;
}

LogLevel resolveLogLevel(const CliOptions& cli_options, const Config& config) {
  if (cli_options.debug) {
    return LogLevel::Debug;
  }

  const char* const env_val = std::getenv("HOLONIGHTD_LOG_LEVEL");
  if (env_val != nullptr && !std::string_view{env_val}.empty()) {
    return parseLogLevel(env_val);
  }

  if (config.log_level.has_value() && !config.log_level->empty()) {
    return parseLogLevel(*config.log_level);
  }

#ifdef NDEBUG
  return LogLevel::Info;
#else
  return LogLevel::Debug;
#endif
}

}  // namespace holonightd
