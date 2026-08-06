// SPDX-License-Identifier: GPL-3.0-or-later
// holonightd - Lightweight daemon for routine maintenance automation.
// Copyright (C) 2026 holonightd contributors

#include "holonightd/agentd/ProviderNormalizer.h"

#include <chrono>

namespace holonightd::agent {

namespace {

std::uint64_t currentTimestampMs() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
          .count());
}

void extractJsonStringField(const nlohmann::json& json_obj, const char* key, std::string& target) {
  if (json_obj.contains(key) && json_obj[key].is_string()) {
    target = json_obj[key].get<std::string>();
  }
}

}  // namespace

AgentEvent ClaudeNormalizer::normalize(std::string_view raw_json_or_text, std::string_view default_session_id) const {
  AgentEvent event;
  event.provider = "claude";
  event.session_id = std::string(default_session_id);
  event.timestamp_ms = currentTimestampMs();

  try {
    auto parsed_json = nlohmann::json::parse(raw_json_or_text);

    extractJsonStringField(parsed_json, "session_id", event.session_id);
    extractJsonStringField(parsed_json, "cwd", event.project_path);
    extractJsonStringField(parsed_json, "title", event.title);
    extractJsonStringField(parsed_json, "message", event.message);
    if (event.message.empty()) {
      extractJsonStringField(parsed_json, "assistant_response", event.message);
    }

    std::string ntype;
    extractJsonStringField(parsed_json, "hook_event_name", ntype);
    if (ntype.empty()) {
      extractJsonStringField(parsed_json, "notification_type", ntype);
    }
    if (ntype.empty()) {
      extractJsonStringField(parsed_json, "event", ntype);
    }

    if (ntype == "permission_prompt" || ntype == "PermissionPrompt") {
      event.state = AgentState::WaitingForApproval;
      if (event.title.empty()) {
        event.title = "Permission needed";
      }
    } else if (ntype == "idle_prompt" || ntype == "IdlePrompt") {
      event.state = AgentState::WaitingForInput;
      if (event.title.empty()) {
        event.title = "Waiting for input";
      }
    } else if (ntype == "completed" || ntype == "stop" || ntype == "AgentTurnComplete") {
      event.state = AgentState::Completed;
      if (event.title.empty()) {
        event.title = "Task completed";
      }
    }
    event.metadata = parsed_json;

  } catch (const nlohmann::json::exception&) {
    event.title = "Claude Code Notification";
    event.message = std::string(raw_json_or_text);
    event.state = AgentState::Working;
  }

  return event;
}

AgentEvent CodexNormalizer::normalize(std::string_view raw_json_or_text, std::string_view default_session_id) const {
  AgentEvent event;
  event.provider = "codex";
  event.session_id = std::string(default_session_id);
  event.timestamp_ms = currentTimestampMs();

  try {
    auto parsed_json = nlohmann::json::parse(raw_json_or_text);

    extractJsonStringField(parsed_json, "session_id", event.session_id);
    extractJsonStringField(parsed_json, "cwd", event.project_path);

    if (parsed_json.contains("event") && parsed_json["event"].is_string()) {
      std::string event_name = parsed_json["event"].get<std::string>();
      if (event_name == "stop" || event_name == "completed") {
        event.state = AgentState::Completed;
        event.title = "Task completed";
      } else if (event_name == "approval" || event_name == "pre_tool_use") {
        event.state = AgentState::WaitingForApproval;
        event.title = "Approval required";
      } else if (event_name == "start") {
        event.state = AgentState::Working;
        event.title = "Working...";
      }
    }
    extractJsonStringField(parsed_json, "message", event.message);
    event.metadata = parsed_json;
  } catch (const nlohmann::json::exception&) {
    event.title = "Codex Notification";
    event.message = std::string(raw_json_or_text);
    event.state = AgentState::Working;
  }

  return event;
}

AgentEvent KiroNormalizer::normalize(std::string_view raw_json_or_text, std::string_view default_session_id) const {
  AgentEvent event;
  event.provider = "kiro";
  event.session_id = std::string(default_session_id);
  event.timestamp_ms = currentTimestampMs();

  try {
    auto parsed_json = nlohmann::json::parse(raw_json_or_text);

    extractJsonStringField(parsed_json, "session_id", event.session_id);
    extractJsonStringField(parsed_json, "cwd", event.project_path);

    std::string trig;
    extractJsonStringField(parsed_json, "hook_event_name", trig);
    if (trig.empty()) {
      extractJsonStringField(parsed_json, "trigger", trig);
    }

    if (trig == "AgentTurnComplete" || trig == "stop" || trig == "completed") {
      event.state = AgentState::Completed;
      event.title = "Task completed";
    } else if (trig == "ApprovalRequired" || trig == "permission_prompt") {
      event.state = AgentState::WaitingForApproval;
      event.title = "Approval required";
    }

    extractJsonStringField(parsed_json, "message", event.message);
    if (event.message.empty()) {
      extractJsonStringField(parsed_json, "assistant_response", event.message);
    }
    event.metadata = parsed_json;

  } catch (const nlohmann::json::exception&) {
    event.title = "Kiro CLI Notification";
    event.message = std::string(raw_json_or_text);
    event.state = AgentState::Working;
  }

  return event;
}

AgentEvent AntigravityNormalizer::normalize(std::string_view raw_json_or_text,
                                            std::string_view default_session_id) const {
  AgentEvent event;
  event.provider = "antigravity";
  event.session_id = std::string(default_session_id);
  event.timestamp_ms = currentTimestampMs();

  try {
    auto parsed_json = nlohmann::json::parse(raw_json_or_text);

    extractJsonStringField(parsed_json, "session_id", event.session_id);
    extractJsonStringField(parsed_json, "cwd", event.project_path);

    if (parsed_json.contains("state") && parsed_json["state"].is_string()) {
      auto parsed = agentStateFromString(parsed_json["state"].get<std::string>());
      if (parsed.has_value()) {
        event.state = *parsed;
      }
    }
    extractJsonStringField(parsed_json, "title", event.title);
    extractJsonStringField(parsed_json, "message", event.message);
    event.metadata = parsed_json;
  } catch (const nlohmann::json::exception&) {
    event.title = "Antigravity Notification";
    event.message = std::string(raw_json_or_text);
    event.state = AgentState::Working;
  }

  return event;
}

AgentEvent NormalizerFactory::normalize(std::string_view provider, std::string_view raw_json_or_text,
                                        std::string_view default_session_id) {
  if (provider == "claude") {
    return ClaudeNormalizer{}.normalize(raw_json_or_text, default_session_id);
  }
  if (provider == "codex") {
    return CodexNormalizer{}.normalize(raw_json_or_text, default_session_id);
  }
  if (provider == "kiro") {
    return KiroNormalizer{}.normalize(raw_json_or_text, default_session_id);
  }
  if (provider == "antigravity") {
    return AntigravityNormalizer{}.normalize(raw_json_or_text, default_session_id);
  }
  // Generic fallback normalizer
  return AntigravityNormalizer{}.normalize(raw_json_or_text, default_session_id);
}

}  // namespace holonightd::agent
