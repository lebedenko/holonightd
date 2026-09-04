// SPDX-License-Identifier: GPL-3.0-or-later
// holonightd - Lightweight daemon for routine maintenance automation.
// Copyright (C) 2026 holonightd contributors

#pragma once

#include <chrono>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace holonightd::agent {

class ActivationClient;
class SessionRegistry;

inline constexpr std::size_t kMaximumActivationTitleBytes = 512;

struct ActivationDescriptor {
  std::vector<std::uint32_t> process_lineage;
  std::string title_hint;

  [[nodiscard]] bool valid() const { return !process_lineage.empty(); }
};

enum class AgentState : std::uint8_t {
  Starting,
  Working,
  WaitingForApproval,
  WaitingForInput,
  Completed,
  Failed,
  Cancelled
};

[[nodiscard]] std::string toString(AgentState state);
[[nodiscard]] std::optional<AgentState> agentStateFromString(const std::string& str);

struct AgentEvent {
  std::string provider;      // "claude", "codex", "kiro", "antigravity"
  std::string session_id;    // Unique session identifier or PID token
  std::string project_path;  // Project working directory (cwd)
  std::string terminal_id;   // Wayland/Hyprland window address or terminal ID
  AgentState state{AgentState::Working};
  std::string title;    // Display title for notification
  std::string message;  // Notification body / prompt snippet
  std::uint64_t timestamp_ms{0};
  nlohmann::json metadata;

  [[nodiscard]] nlohmann::json toJson() const;
  [[nodiscard]] static AgentEvent fromJson(const nlohmann::json& json_obj);
};

struct AgentSession {
  std::string session_id;
  std::string provider;
  std::string project_name;
  std::string project_path;
  std::uint32_t pid{0};
  std::string window_address;
  AgentState current_state{AgentState::Starting};
  std::string last_title;
  std::string last_message;
  std::chrono::system_clock::time_point start_time{std::chrono::system_clock::now()};
  std::chrono::system_clock::time_point last_update_time{std::chrono::system_clock::now()};
  std::uint32_t notification_id{0};  // Active org.freedesktop.Notifications ID
  std::optional<ActivationDescriptor> activation;
};

[[nodiscard]] bool activateSession(const SessionRegistry& registry, ActivationClient& client,
                                   const std::string& session_id);

}  // namespace holonightd::agent
