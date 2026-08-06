// SPDX-License-Identifier: GPL-3.0-or-later
// holonightd - Lightweight daemon for routine maintenance automation.
// Copyright (C) 2026 holonightd contributors

#include "holonightd/agentd/AgentActivity.h"
#include "holonightd/agentd/ProviderNormalizer.h"
#include "holonightd/agentd/SessionRegistry.h"

#include <gtest/gtest.h>

using namespace holonightd::agent;

TEST(AgentActivityTest, EnumStateConversions) {
  EXPECT_EQ(toString(AgentState::Starting), "Starting");
  EXPECT_EQ(toString(AgentState::WaitingForApproval), "WaitingForApproval");
  EXPECT_EQ(toString(AgentState::Completed), "Completed");

  EXPECT_EQ(agentStateFromString("Starting"), AgentState::Starting);
  EXPECT_EQ(agentStateFromString("permission_prompt"), AgentState::WaitingForApproval);
  EXPECT_EQ(agentStateFromString("idle_prompt"), AgentState::WaitingForInput);
  EXPECT_EQ(agentStateFromString("stop"), AgentState::Completed);
  EXPECT_FALSE(agentStateFromString("invalid_state_string").has_value());
}

TEST(AgentActivityTest, JsonRoundTrip) {
  AgentEvent event;
  event.provider = "claude";
  event.session_id = "sess-123";
  event.project_path = "/home/user/project";
  event.state = AgentState::WaitingForApproval;
  event.title = "Permission needed";
  event.message = "Execute bash tool?";

  nlohmann::json json_out = event.toJson();
  EXPECT_EQ(json_out["provider"], "claude");
  EXPECT_EQ(json_out["state"], "WaitingForApproval");

  AgentEvent restored = AgentEvent::fromJson(json_out);
  EXPECT_EQ(restored.provider, "claude");
  EXPECT_EQ(restored.session_id, "sess-123");
  EXPECT_EQ(restored.state, AgentState::WaitingForApproval);
  EXPECT_EQ(restored.title, "Permission needed");
}

TEST(ProviderNormalizerTest, ParsesClaudeNotification) {
  std::string raw_json = R"({
    "session_id": "claude-xyz",
    "cwd": "/home/user/work",
    "title": "Permission needed",
    "message": "Claude wants to run git status",
    "notification_type": "permission_prompt"
  })";

  AgentEvent agent_event = NormalizerFactory::normalize("claude", raw_json);
  EXPECT_EQ(agent_event.provider, "claude");
  EXPECT_EQ(agent_event.session_id, "claude-xyz");
  EXPECT_EQ(agent_event.project_path, "/home/user/work");
  EXPECT_EQ(agent_event.state, AgentState::WaitingForApproval);
  EXPECT_EQ(agent_event.title, "Permission needed");
  EXPECT_EQ(agent_event.message, "Claude wants to run git status");
}

TEST(ProviderNormalizerTest, ParsesCodexStopEvent) {
  std::string raw_json = R"({
    "session_id": "codex-99",
    "cwd": "/repo",
    "event": "stop",
    "message": "Finished turn successfully"
  })";

  AgentEvent agent_event = NormalizerFactory::normalize("codex", raw_json);
  EXPECT_EQ(agent_event.provider, "codex");
  EXPECT_EQ(agent_event.session_id, "codex-99");
  EXPECT_EQ(agent_event.state, AgentState::Completed);
  EXPECT_EQ(agent_event.title, "Task completed");
}

TEST(ProviderNormalizerTest, ParsesKiroTurnComplete) {
  std::string raw_json = R"({
    "session_id": "kiro-session-1",
    "cwd": "/kiro/proj",
    "trigger": "AgentTurnComplete"
  })";

  AgentEvent agent_event = NormalizerFactory::normalize("kiro", raw_json);
  EXPECT_EQ(agent_event.provider, "kiro");
  EXPECT_EQ(agent_event.session_id, "kiro-session-1");
  EXPECT_EQ(agent_event.state, AgentState::Completed);
  EXPECT_EQ(agent_event.title, "Task completed");
}

TEST(ProviderNormalizerTest, HandlesInvalidJsonGracefully) {
  std::string raw_text = "Plain text notification message";
  AgentEvent agent_event = NormalizerFactory::normalize("claude", raw_text, "default-sess");
  EXPECT_EQ(agent_event.provider, "claude");
  EXPECT_EQ(agent_event.session_id, "default-sess");
  EXPECT_EQ(agent_event.message, "Plain text notification message");
  EXPECT_EQ(agent_event.state, AgentState::Working);
}

TEST(SessionRegistryTest, PrunesSessionsOlderThanMaximumAge) {
  SessionRegistry registry;
  registry.registerSession("codex", "stale-session", 123, "/repo");

  registry.pruneStaleSessions(std::chrono::seconds{-1});

  EXPECT_FALSE(registry.getSession("stale-session").has_value());
}
