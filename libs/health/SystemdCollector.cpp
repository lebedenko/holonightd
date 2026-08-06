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

#include "holonightd/SystemdCollector.h"

// NOLINTBEGIN
#include <nlohmann/json.hpp>
// NOLINTEND

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>

#ifdef HOLONIGHTD_HAS_SYSTEMD
#include <systemd/sd-bus.h>
#include <systemd/sd-journal.h>
#endif

namespace holonightd {

namespace {
using json = nlohmann::json;

#ifdef HOLONIGHTD_HAS_SYSTEMD
struct BusDeleter {
  void operator()(sd_bus* bus_handle) const { sd_bus_unref(bus_handle); }
};

struct MessageDeleter {
  void operator()(sd_bus_message* msg_handle) const { sd_bus_message_unref(msg_handle); }
};

struct JournalDeleter {
  void operator()(sd_journal* journal_handle) const { sd_journal_close(journal_handle); }
};
#endif
}  // namespace

SystemdCollector::SystemdCollector(SystemdCollectorOptions options) : options_{std::move(options)} {
  if (options_.flapping_threshold <= 0) {
    std::cerr << "warning: flapping_threshold must be positive; falling back to default 3\n";
    options_.flapping_threshold = 3;
  }
  if (options_.flapping_window_seconds <= 0) {
    std::cerr << "warning: flapping_window_seconds must be positive; falling back to default 300\n";
    options_.flapping_window_seconds = 300;
  }
}

bool SystemdCollector::isUnitIgnored(const std::string& unit_name) const {
  return std::ranges::any_of(options_.ignore_units, [&](const std::string& ignored) { return unit_name == ignored; });
}

void SystemdCollector::pruneOldTimestamps(const std::chrono::system_clock::time_point& now) {
  const auto window = std::chrono::seconds{options_.flapping_window_seconds};
  const auto cutoff = now - window;

  for (auto it = flapping_state_.begin(); it != flapping_state_.end();) {
    auto& starts = it->second.recent_starts;
    std::erase_if(starts, [&](const auto& timepoint) { return timepoint < cutoff; });
    if (starts.empty()) {
      it = flapping_state_.erase(it);
    } else {
      ++it;
    }
  }
}

std::vector<ObservationEvent> SystemdCollector::collect() {
  std::vector<ObservationEvent> events;
  const auto now = std::chrono::system_clock::now();
  pruneOldTimestamps(now);

#ifndef HOLONIGHTD_HAS_SYSTEMD
  std::clog << "info: Systemd support not compiled or libsystemd unavailable; systemd collector disabled.\n";
  return events;
#else
  sd_bus* bus_raw = nullptr;
  const int ret_open = sd_bus_open_system(&bus_raw);
  if (ret_open < 0) {
    std::clog << "info: Failed to connect to system D-Bus: " << std::strerror(-ret_open)
              << " (systemd may be absent or running in container/chroot)\n";
    return events;
  }
  std::unique_ptr<sd_bus, BusDeleter> bus{bus_raw};

  collectUnitEvents(bus.get(), now, events);
  collectCoredumpEvents(now, events);

  return events;
#endif
}

#ifdef HOLONIGHTD_HAS_SYSTEMD
void SystemdCollector::collectUnitEvents(sd_bus* bus_ptr, const std::chrono::system_clock::time_point& now,
                                         std::vector<ObservationEvent>& events) {
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message* reply_raw = nullptr;

  const int ret_call = sd_bus_call_method(  // NOLINT(cppcoreguidelines-pro-type-vararg)
      bus_ptr, "org.freedesktop.systemd1", "/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager", "ListUnits",
      &error, &reply_raw, "");

  if (ret_call < 0) {
    std::cerr << "warning: D-Bus ListUnits call failed: "
              << (error.message != nullptr ? error.message : std::strerror(-ret_call)) << "\n";
    sd_bus_error_free(&error);
    return;
  }
  sd_bus_error_free(&error);

  std::unique_ptr<sd_bus_message, MessageDeleter> reply{reply_raw};

  if (sd_bus_message_enter_container(reply.get(), 'a', "(ssssssouso)") < 0) {
    std::cerr << "warning: Failed to enter ListUnits response array container\n";
    return;
  }

  while (sd_bus_message_enter_container(reply.get(), 'r', "ssssssouso") > 0) {
    const char* unit_name = nullptr;
    const char* description = nullptr;
    const char* load_state = nullptr;
    const char* active_state = nullptr;
    const char* sub_state = nullptr;
    const char* followed = nullptr;
    const char* unit_path = nullptr;
    std::uint32_t job_id = 0;
    const char* job_type = nullptr;
    const char* job_path = nullptr;

    const int ret_read = sd_bus_message_read(  // NOLINT(cppcoreguidelines-pro-type-vararg)
        reply.get(), "ssssssouso", &unit_name, &description, &load_state, &active_state, &sub_state, &followed,
        &unit_path, &job_id, &job_type, &job_path);

    if (ret_read >= 0 && unit_name != nullptr) {
      const std::string name{unit_name};
      const std::string active{active_state != nullptr ? active_state : ""};
      const std::string sub{sub_state != nullptr ? sub_state : ""};
      const std::string load{load_state != nullptr ? load_state : ""};

      if (!isUnitIgnored(name) && active == "failed") {
        ObservationEvent event;
        event.timestamp = now;
        event.source = "systemd";
        event.category = "systemd.unit";
        event.subject = name;
        event.severity = Severity::Error;
        event.signal = "unit_failed";
        event.value = "failed";

        const json attr = {
            {"active_state", active},
            {"sub_state", sub},
            {"load_state", load},
        };
        event.attributes_json = attr.dump();
        events.push_back(std::move(event));

        flapping_state_[name].recent_starts.push_back(now);
        const auto restart_count = static_cast<int>(flapping_state_[name].recent_starts.size());
        if (restart_count >= options_.flapping_threshold) {
          ObservationEvent flap_event;
          flap_event.timestamp = now;
          flap_event.source = "systemd";
          flap_event.category = "systemd.unit";
          flap_event.subject = name;
          flap_event.severity = Severity::Warning;
          flap_event.signal = "unit_flapping";
          flap_event.value = std::to_string(restart_count);

          const json flap_attr = {
              {"restart_count", restart_count},
              {"flapping_window_seconds", options_.flapping_window_seconds},
          };
          flap_event.attributes_json = flap_attr.dump();
          events.push_back(std::move(flap_event));
        }
      }
    }

    sd_bus_message_exit_container(reply.get());
  }

  sd_bus_message_exit_container(reply.get());
}

void SystemdCollector::collectCoredumpEvents(const std::chrono::system_clock::time_point& now,
                                             std::vector<ObservationEvent>& events) {
  sd_journal* journal_raw = nullptr;
  const int ret_journal = sd_journal_open(&journal_raw, SD_JOURNAL_LOCAL_ONLY);
  if (ret_journal < 0 || journal_raw == nullptr) {
    return;
  }
  std::unique_ptr<sd_journal, JournalDeleter> journal{journal_raw};

  if (sd_journal_add_match(journal.get(), "MESSAGE_ID=fc2e22bc6e6f47b9b90e653896802e39", 0) < 0) {
    return;
  }

  const auto cutoff_us = std::chrono::duration_cast<std::chrono::microseconds>(
                             (now - std::chrono::seconds{options_.flapping_window_seconds}).time_since_epoch())
                             .count();
  sd_journal_seek_realtime_usec(journal.get(), static_cast<uint64_t>(cutoff_us));

  while (sd_journal_next(journal.get()) > 0) {
    const void* data = nullptr;
    size_t length = 0;
    std::string exe_name = "unknown";
    std::string unit_name = "unknown";

    if (sd_journal_get_data(journal.get(), "COREDUMP_EXE", &data, &length) >= 0 && data != nullptr) {
      const std::string_view str{static_cast<const char*>(data), length};
      const auto pos = str.find('=');
      if (pos != std::string_view::npos) {
        exe_name = std::string{str.substr(pos + 1)};
      }
    }
    if (sd_journal_get_data(journal.get(), "COREDUMP_UNIT", &data, &length) >= 0 && data != nullptr) {
      const std::string_view str{static_cast<const char*>(data), length};
      const auto pos = str.find('=');
      if (pos != std::string_view::npos) {
        unit_name = std::string{str.substr(pos + 1)};
      }
    }

    const std::string subject = (unit_name != "unknown" && !unit_name.empty()) ? unit_name : exe_name;

    if (!isUnitIgnored(subject)) {
      ObservationEvent cd_event;
      cd_event.timestamp = now;
      cd_event.source = "systemd";
      cd_event.category = "systemd.coredump";
      cd_event.subject = subject;
      cd_event.severity = Severity::Error;
      cd_event.signal = "coredump";
      cd_event.value = exe_name;

      const json cd_attr = {
          {"executable", exe_name},
          {"unit", unit_name},
      };
      cd_event.attributes_json = cd_attr.dump();
      events.push_back(std::move(cd_event));
    }
  }
}
#endif

}  // namespace holonightd
