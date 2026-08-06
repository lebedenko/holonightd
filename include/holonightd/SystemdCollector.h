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

#pragma once

#include "holonightd/ObservationEvent.h"

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef HOLONIGHTD_HAS_SYSTEMD
#include <systemd/sd-bus.h>
#endif

namespace holonightd {

struct SystemdCollectorOptions {
  int flapping_threshold{3};
  int flapping_window_seconds{300};
  std::vector<std::string> ignore_units;
};

struct UnitFlappingState {
  std::vector<std::chrono::system_clock::time_point> recent_starts;
};

class SystemdCollector {
 public:
  explicit SystemdCollector(SystemdCollectorOptions options = {});

  /// Performs collection scan over systemd via system D-Bus.
  /// Emits ObservationEvents for failed units, flapping restarts, and coredumps.
  /// Safely logs a warning and returns an empty vector if D-Bus is unavailable.
  [[nodiscard]] std::vector<ObservationEvent> collect();

 private:
  [[nodiscard]] bool isUnitIgnored(const std::string& unit_name) const;
  void pruneOldTimestamps(const std::chrono::system_clock::time_point& now);

#ifdef HOLONIGHTD_HAS_SYSTEMD
  void collectUnitEvents(sd_bus* bus_ptr, const std::chrono::system_clock::time_point& now,
                         std::vector<ObservationEvent>& events);
  void collectCoredumpEvents(const std::chrono::system_clock::time_point& now, std::vector<ObservationEvent>& events);
#endif

  SystemdCollectorOptions options_;
  std::unordered_map<std::string, UnitFlappingState> flapping_state_;
};

}  // namespace holonightd
