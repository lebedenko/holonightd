// SPDX-License-Identifier: GPL-3.0-or-later
// holonightd - Lightweight daemon for routine maintenance automation.
// Copyright (C) 2026 holonightd contributors

#pragma once

#include "holonightd/agentd/AgentActivity.h"

#include <cstdint>
#include <string>

#ifdef HOLONIGHTD_HAS_SYSTEMD
#include <systemd/sd-bus.h>
#endif

namespace holonightd::agent {

class NotificationBridge {
 public:
  NotificationBridge() = default;

  // Sends desktop notification via D-Bus org.freedesktop.Notifications
  // Returns updated notification ID (for replacing existing notifications)
  static std::uint32_t sendNotification(const AgentSession& session, const AgentEvent& event,
                                        std::uint32_t replaces_id = 0);
};

}  // namespace holonightd::agent
