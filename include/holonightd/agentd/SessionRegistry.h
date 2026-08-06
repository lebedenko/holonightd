// SPDX-License-Identifier: GPL-3.0-or-later
// holonightd - Lightweight daemon for routine maintenance automation.
// Copyright (C) 2026 holonightd contributors

#pragma once

#include "holonightd/agentd/AgentActivity.h"

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace holonightd::agent {

class SessionRegistry {
 public:
  SessionRegistry() = default;

  std::string registerSession(const std::string& provider, const std::string& session_id, std::uint32_t pid,
                              const std::string& cwd, const nlohmann::json& metadata = {});

  bool publishEvent(const AgentEvent& event);
  bool endSession(const std::string& session_id, const std::string& result = "Completed");

  [[nodiscard]] std::optional<AgentSession> getSession(const std::string& session_id) const;
  [[nodiscard]] std::vector<AgentSession> listSessions() const;

  void updateNotificationId(const std::string& session_id, std::uint32_t notif_id);
  void pruneStaleSessions(std::chrono::seconds max_age = std::chrono::hours(24));

 private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, AgentSession> sessions_;
};

}  // namespace holonightd::agent
