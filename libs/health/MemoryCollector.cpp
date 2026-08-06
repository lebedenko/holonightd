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

#include "holonightd/Logger.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <string_view>
#include <utility>

namespace holonightd {

MemoryCollector::MemoryCollector(MemoryCollectorOptions options) : options_(std::move(options)) {}

static void parsePsiLine(std::string_view line, std::optional<double>& avg10, std::optional<double>& avg60,
                         std::optional<double>& avg300, std::optional<double>& total) {
  std::istringstream stream{std::string(line)};
  std::string token;
  stream >> token;  // Skip "some" or "full"

  while (stream >> token) {
    if (token.starts_with("avg10=")) {
      try {
        avg10 = std::stod(token.substr(6));
      } catch (...) {
      }
    } else if (token.starts_with("avg60=")) {
      try {
        avg60 = std::stod(token.substr(6));
      } catch (...) {
      }
    } else if (token.starts_with("avg300=")) {
      try {
        avg300 = std::stod(token.substr(7));
      } catch (...) {
      }
    } else if (token.starts_with("total=")) {
      try {
        total = std::stod(token.substr(6));
      } catch (...) {
      }
    }
  }
}

void MemoryCollector::parsePsi(MemoryMetrics& metrics) const {
  const auto psi_file = options_.proc_root / "pressure" / "memory";
  std::ifstream input{psi_file};
  if (!input) {
    metrics.psi_success = false;
    return;
  }

  std::string line;
  bool parsed_some = false;
  bool parsed_full = false;

  while (std::getline(input, line)) {
    if (line.starts_with("some ")) {
      parsePsiLine(line, metrics.psi_some_avg10, metrics.psi_some_avg60, metrics.psi_some_avg300,
                   metrics.psi_some_total);
      parsed_some = metrics.psi_some_avg10.has_value();
    } else if (line.starts_with("full ")) {
      parsePsiLine(line, metrics.psi_full_avg10, metrics.psi_full_avg60, metrics.psi_full_avg300,
                   metrics.psi_full_total);
      parsed_full = metrics.psi_full_avg10.has_value();
    }
  }

  metrics.psi_success = (parsed_some || parsed_full);
}

void MemoryCollector::parseMeminfo(MemoryMetrics& metrics) const {
  const auto meminfo_file = options_.proc_root / "meminfo";
  std::ifstream input{meminfo_file};
  if (!input) {
    metrics.meminfo_success = false;
    return;
  }

  std::string line;
  std::uint64_t total_kb = 0;
  std::uint64_t avail_kb = 0;
  bool found_total = false;
  bool found_avail = false;

  while (std::getline(input, line)) {
    if (line.starts_with("MemTotal:")) {
      std::istringstream stream{line.substr(9)};
      if (stream >> total_kb) {
        found_total = true;
      }
    } else if (line.starts_with("MemAvailable:")) {
      std::istringstream stream{line.substr(13)};
      if (stream >> avail_kb) {
        found_avail = true;
      }
    }
  }

  if (found_total && total_kb > 0) {
    metrics.total_bytes = total_kb * 1024U;
    metrics.available_bytes = found_avail ? (avail_kb * 1024U) : 0U;
    metrics.used_bytes =
        (metrics.total_bytes >= metrics.available_bytes) ? (metrics.total_bytes - metrics.available_bytes) : 0U;
    metrics.percent_used = (static_cast<double>(metrics.used_bytes) / static_cast<double>(metrics.total_bytes)) * 100.0;
    metrics.meminfo_success = true;
  } else {
    metrics.meminfo_success = false;
  }
}

void MemoryCollector::parseVmstatOom(MemoryMetrics& metrics) {
  const auto vmstat_file = options_.proc_root / "vmstat";
  std::ifstream input{vmstat_file};
  if (!input) {
    metrics.vmstat_success = false;
    return;
  }

  std::string key;
  std::uint64_t val = 0;
  bool found_oom = false;

  while (input >> key >> val) {
    if (key == "oom_kill") {
      found_oom = true;
      metrics.oom_kill_counter = val;
      break;
    }
  }

  if (!found_oom) {
    metrics.vmstat_success = false;
    return;
  }

  metrics.vmstat_success = true;

  if (!baseline_oom_kill_.has_value()) {
    baseline_oom_kill_ = val;
    metrics.oom_kill_delta = 0;
  } else {
    if (val >= *baseline_oom_kill_) {
      const std::uint64_t delta = val - *baseline_oom_kill_;
      metrics.oom_kill_delta = delta;
      baseline_oom_kill_ = val;
      if (delta > 0) {
        extractOomVictim(metrics);
      }
    }
  }
}

void MemoryCollector::extractOomVictim(MemoryMetrics& metrics) const {
  const auto kmsg_file = options_.proc_root / "kmsg";
  std::ifstream input{kmsg_file};
  if (!input) {
    metrics.victim_pid = 0;
    metrics.victim_name = "unknown";
    return;
  }

  std::string line;
  const std::regex oom_regex(R"((?:Out of memory|Killed process)[:\s]+(?:Kill process\s+)?(\d+)\s+\(([^)]+)\))",
                             std::regex::icase);

  pid_t last_pid = 0;
  std::string last_name = "unknown";
  bool found = false;

  while (std::getline(input, line)) {
    std::smatch match;
    if (std::regex_search(line, match, oom_regex)) {
      try {
        last_pid = static_cast<pid_t>(std::stoi(match[1].str()));
        last_name = match[2].str();
        found = true;
      } catch (...) {
      }
    }
  }

  if (found) {
    metrics.victim_pid = last_pid;
    metrics.victim_name = last_name;
  } else {
    metrics.victim_pid = 0;
    metrics.victim_name = "unknown";
  }
}

MemoryMetrics MemoryCollector::collectMetrics() {
  MemoryMetrics metrics;
  parsePsi(metrics);
  if (!metrics.psi_success) {
    parseMeminfo(metrics);
  }
  parseVmstatOom(metrics);
  return metrics;
}

std::vector<ObservationEvent> MemoryCollector::collect() {
  const MemoryMetrics metrics = collectMetrics();
  std::vector<ObservationEvent> events;

  if (metrics.psi_success) {
    if (metrics.psi_some_avg10.has_value() && *metrics.psi_some_avg10 >= options_.some_warning_threshold) {
      ObservationEvent event;
      event.source = "memory_collector";
      event.category = "memory";
      event.subject = "psi";
      event.signal = "memory_pressure_some";
      event.severity = Severity::Warning;
      event.value = *metrics.psi_some_avg10;

      nlohmann::json attr = {
          {"some_avg10", *metrics.psi_some_avg10},
          {"some_avg60", metrics.psi_some_avg60.value_or(0.0)},
          {"some_avg300", metrics.psi_some_avg300.value_or(0.0)},
          {"some_total", metrics.psi_some_total.value_or(0.0)},
      };
      event.attributes_json = attr.dump();
      events.push_back(std::move(event));
    }

    if (metrics.psi_full_avg10.has_value() && *metrics.psi_full_avg10 >= options_.full_critical_threshold) {
      ObservationEvent event;
      event.source = "memory_collector";
      event.category = "memory";
      event.subject = "psi";
      event.signal = "memory_pressure_full";
      event.severity = Severity::Critical;
      event.value = *metrics.psi_full_avg10;

      nlohmann::json attr = {
          {"full_avg10", *metrics.psi_full_avg10},
          {"full_avg60", metrics.psi_full_avg60.value_or(0.0)},
          {"full_avg300", metrics.psi_full_avg300.value_or(0.0)},
          {"full_total", metrics.psi_full_total.value_or(0.0)},
      };
      event.attributes_json = attr.dump();
      events.push_back(std::move(event));
    }
  } else if (metrics.meminfo_success && metrics.percent_used.has_value()) {
    if (*metrics.percent_used >= options_.meminfo_warning_threshold) {
      ObservationEvent event;
      event.source = "memory_collector";
      event.category = "memory";
      event.subject = "meminfo";
      event.signal = "memory_used_high";
      event.severity = Severity::Warning;
      event.value = *metrics.percent_used;

      nlohmann::json attr = {
          {"total_bytes", metrics.total_bytes},
          {"available_bytes", metrics.available_bytes},
          {"used_bytes", metrics.used_bytes},
          {"percent_used", *metrics.percent_used},
      };
      event.attributes_json = attr.dump();
      events.push_back(std::move(event));
    }
  }

  if (metrics.oom_kill_delta.has_value() && *metrics.oom_kill_delta > 0) {
    ObservationEvent event;
    event.source = "memory_collector";
    event.category = "memory";
    event.subject = "oom_killer";
    event.signal = "oom_killer_invoked";
    event.severity = Severity::Error;
    event.value = static_cast<double>(*metrics.oom_kill_delta);

    nlohmann::json attr = {
        {"delta", *metrics.oom_kill_delta},
        {"total_oom_kills", metrics.oom_kill_counter.value_or(0)},
        {"victim_pid", metrics.victim_pid.value_or(0)},
        {"victim_name", metrics.victim_name.value_or("unknown")},
    };
    event.attributes_json = attr.dump();
    events.push_back(std::move(event));
  }

  return events;
}

}  // namespace holonightd
