// SPDX-License-Identifier: GPL-3.0-or-later
// holonightd - Lightweight daemon for routine maintenance automation.
// Copyright (C) 2026 holonightd contributors

#include "holonightd/agentd/AgentActivity.h"

#include <chrono>
#include <stdexcept>

namespace holonightd::agent {

std::string toString(AgentState state) {
  switch (state) {
    case AgentState::Starting:
      return "Starting";
    case AgentState::Working:
      return "Working";
    case AgentState::WaitingForApproval:
      return "WaitingForApproval";
    case AgentState::WaitingForInput:
      return "WaitingForInput";
    case AgentState::Completed:
      return "Completed";
    case AgentState::Failed:
      return "Failed";
    case AgentState::Cancelled:
      return "Cancelled";
  }
  return "Working";
}

std::optional<AgentState> agentStateFromString(const std::string& str) {
  if (str == "Starting" || str == "starting") {
    return AgentState::Starting;
  }
  if (str == "Working" || str == "working") {
    return AgentState::Working;
  }
  if (str == "WaitingForApproval" || str == "waiting_for_approval" || str == "permission_prompt") {
    return AgentState::WaitingForApproval;
  }
  if (str == "WaitingForInput" || str == "waiting_for_input" || str == "idle_prompt") {
    return AgentState::WaitingForInput;
  }
  if (str == "Completed" || str == "completed" || str == "stop") {
    return AgentState::Completed;
  }
  if (str == "Failed" || str == "failed" || str == "error") {
    return AgentState::Failed;
  }
  if (str == "Cancelled" || str == "cancelled") {
    return AgentState::Cancelled;
  }
  return std::nullopt;
}

nlohmann::json AgentEvent::toJson() const {
  return nlohmann::json{{"provider", provider},       {"session_id", session_id},     {"project_path", project_path},
                        {"terminal_id", terminal_id}, {"state", toString(state)},     {"title", title},
                        {"message", message},         {"timestamp_ms", timestamp_ms}, {"metadata", metadata}};
}

AgentEvent AgentEvent::fromJson(const nlohmann::json& json_obj) {
  AgentEvent event;
  if (json_obj.contains("provider") && json_obj["provider"].is_string()) {
    event.provider = json_obj["provider"].get<std::string>();
  }
  if (json_obj.contains("session_id") && json_obj["session_id"].is_string()) {
    event.session_id = json_obj["session_id"].get<std::string>();
  }
  if (json_obj.contains("project_path") && json_obj["project_path"].is_string()) {
    event.project_path = json_obj["project_path"].get<std::string>();
  }
  if (json_obj.contains("terminal_id") && json_obj["terminal_id"].is_string()) {
    event.terminal_id = json_obj["terminal_id"].get<std::string>();
  }
  if (json_obj.contains("state") && json_obj["state"].is_string()) {
    auto parsed_state = agentStateFromString(json_obj["state"].get<std::string>());
    if (parsed_state.has_value()) {
      event.state = *parsed_state;
    }
  }
  if (json_obj.contains("title") && json_obj["title"].is_string()) {
    event.title = json_obj["title"].get<std::string>();
  }
  if (json_obj.contains("message") && json_obj["message"].is_string()) {
    event.message = json_obj["message"].get<std::string>();
  }
  if (json_obj.contains("timestamp_ms") && json_obj["timestamp_ms"].is_number_unsigned()) {
    event.timestamp_ms = json_obj["timestamp_ms"].get<std::uint64_t>();
  }
  if (json_obj.contains("metadata") && json_obj["metadata"].is_object()) {
    event.metadata = json_obj["metadata"];
  }
  return event;
}

}  // namespace holonightd::agent
