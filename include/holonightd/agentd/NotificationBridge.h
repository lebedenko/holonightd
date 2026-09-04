// SPDX-License-Identifier: GPL-3.0-or-later
// holonightd - Lightweight daemon for routine maintenance automation.
// Copyright (C) 2026 holonightd contributors

#pragma once

#include "holonightd/agentd/AgentActivity.h"
#include "holonightd/agentd/NotificationActionBindings.h"
#include "holonightd/agentd/ShellActivationClient.h"

#include <cstdint>
#include <string>

#ifdef HOLONIGHTD_HAS_SYSTEMD
#include <systemd/sd-bus.h>
#endif

namespace holonightd::agent {

class NotificationBridge {
 public:
  NotificationBridge(
#ifdef HOLONIGHTD_HAS_SYSTEMD
      sd_bus* bus,
#endif
      ActivationClient& activation_client);
  ~NotificationBridge();
  NotificationBridge(const NotificationBridge&) = delete;
  NotificationBridge& operator=(const NotificationBridge&) = delete;
  NotificationBridge(NotificationBridge&&) = delete;
  NotificationBridge& operator=(NotificationBridge&&) = delete;

  std::uint32_t sendNotification(const AgentSession& session, const AgentEvent& event, std::uint32_t replaces_id = 0);

  void handleAction(std::uint32_t notification_id, std::string_view action_key);
  void handleClosed(std::uint32_t notification_id);
  void bindNotification(std::uint32_t replaces_id, std::uint32_t notification_id,
                        const ActivationDescriptor& descriptor);

 private:
#ifdef HOLONIGHTD_HAS_SYSTEMD
  static int onActionInvoked(sd_bus_message* message, void* userdata, sd_bus_error* error);
  static int onNotificationClosed(sd_bus_message* message, void* userdata, sd_bus_error* error);
  sd_bus* bus_;
  sd_bus_slot* action_slot_{nullptr};
  sd_bus_slot* closed_slot_{nullptr};
#endif
  ActivationClient& activation_client_;
  NotificationActionBindings bindings_;
};

}  // namespace holonightd::agent
