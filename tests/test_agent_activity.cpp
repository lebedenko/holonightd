// SPDX-License-Identifier: GPL-3.0-or-later
// holonightd - Lightweight daemon for routine maintenance automation.
// Copyright (C) 2026 holonightd contributors

#include "holonightd/agentd/AgentActivity.h"
#include "holonightd/agentd/NotificationActionBindings.h"
#include "holonightd/agentd/NotificationBridge.h"
#include "holonightd/agentd/ProcessAncestryReader.h"
#include "holonightd/agentd/ProviderNormalizer.h"
#include "holonightd/agentd/SessionRegistry.h"
#include "holonightd/agentd/ShellActivationClient.h"

#include <cerrno>
#include <gtest/gtest.h>

using namespace holonightd::agent;

namespace {

class FakeActivationClient final : public ActivationClient {
 public:
  ActivationResult result{.accepted = true, .diagnostic = ActivationDiagnostic::Accepted};
  std::vector<ActivationDescriptor> calls;

  ActivationResult activate(const ActivationDescriptor& descriptor) override {
    calls.push_back(descriptor);
    return result;
  }
};

std::optional<std::string> ancestryFixture(std::uint32_t pid) {
  if (pid == 30) {
    return "30 (agent (worker)) S 20 0 0";
  }
  if (pid == 20) {
    return "20 (shell) S 1 0 0";
  }
  if (pid == 1) {
    return "1 (init) S 0 0 0";
  }
  return std::nullopt;
}

}  // namespace

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

TEST(ProcessAncestryReaderTest, ParsesNamesWithSpacesAndParentheses) {
  EXPECT_EQ(ProcessAncestryReader::parseParentPid("30 (agent (worker)) S 20 0 0"), 20U);
  EXPECT_FALSE(ProcessAncestryReader::parseParentPid("malformed").has_value());
}

TEST(ProcessAncestryReaderTest, CollectsBoundedPartialLineage) {
  ProcessAncestryReader reader(ancestryFixture);
  EXPECT_EQ(reader.read(30), (std::vector<std::uint32_t>{30, 20, 1}));
  EXPECT_TRUE(reader.read(99).empty());
  ProcessAncestryReader malformed([](std::uint32_t) { return std::optional<std::string>("malformed"); });
  EXPECT_TRUE(malformed.read(7).empty());

  ProcessAncestryReader partial([](std::uint32_t pid) -> std::optional<std::string> {
    return pid == 7 ? std::optional<std::string>("7 (cmd) S 8 0") : std::nullopt;
  });
  EXPECT_EQ(partial.read(7), (std::vector<std::uint32_t>{7}));
}

TEST(ProcessAncestryReaderTest, StopsAtCyclesAndLimit) {
  ProcessAncestryReader cycle([](std::uint32_t pid) -> std::optional<std::string> {
    return std::to_string(pid) + " (cmd) S " + std::to_string(pid == 4 ? 5 : 4) + " 0";
  });
  EXPECT_EQ(cycle.read(4), (std::vector<std::uint32_t>{4, 5}));

  ProcessAncestryReader long_chain([](std::uint32_t pid) -> std::optional<std::string> {
    return std::to_string(pid) + " (cmd) S " + std::to_string(pid + 1) + " 0";
  });
  EXPECT_EQ(long_chain.read(2).size(), 64U);
}

TEST(SessionRegistryTest, CapturesDescriptorAndBoundsTitle) {
  SessionRegistry registry{ProcessAncestryReader(ancestryFixture)};
  registry.registerSession("codex", "session", 30, "/repo", {{"terminal_title", std::string(700, 'x')}});
  const auto session = registry.getSession("session");
  ASSERT_TRUE(session.has_value());
  ASSERT_TRUE(session->activation.has_value());
  EXPECT_EQ(session->activation->process_lineage, (std::vector<std::uint32_t>{30, 20, 1}));
  EXPECT_EQ(session->activation->title_hint.size(), kMaximumActivationTitleBytes);
}

TEST(NotificationActionBindingsTest, ReplacesConsumesAndClosesCopiedDescriptors) {
  auto now = NotificationActionBindings::Clock::time_point{};
  NotificationActionBindings bindings([&now] { return now; });
  ActivationDescriptor descriptor{.process_lineage = {30, 20, 1}, .title_hint = "terminal"};
  bindings.replace(0, 10, descriptor);
  descriptor.process_lineage.clear();
  bindings.replace(10, 11, ActivationDescriptor{.process_lineage = {40, 20, 1}, .title_hint = "other"});
  EXPECT_FALSE(bindings.consume(10).has_value());
  const auto consumed = bindings.consume(11);
  ASSERT_TRUE(consumed.has_value());
  EXPECT_EQ(consumed->process_lineage.front(), 40U);
  EXPECT_FALSE(bindings.consume(11).has_value());

  bindings.replace(0, 12, ActivationDescriptor{.process_lineage = {1}, .title_hint = {}});
  bindings.close(12);
  EXPECT_TRUE(bindings.size() == 0);
}

TEST(NotificationActionBindingsTest, PrunesExpiredAndCapsCapacity) {
  auto now = NotificationActionBindings::Clock::time_point{};
  NotificationActionBindings bindings([&now] { return now; });
  for (std::uint32_t id = 1; id <= 257; ++id) {
    now += std::chrono::seconds(1);
    bindings.replace(0, id, ActivationDescriptor{.process_lineage = {id}, .title_hint = {}});
  }
  EXPECT_EQ(bindings.size(), 256U);
  EXPECT_FALSE(bindings.consume(1).has_value());
  now += std::chrono::hours(25);
  bindings.prune();
  EXPECT_EQ(bindings.size(), 0U);
}

TEST(NotificationBridgeTest, RoutesOnlyOpenAndConsumesBindingBeforeActivation) {
  FakeActivationClient client;
#ifdef HOLONIGHTD_HAS_SYSTEMD
  NotificationBridge bridge(nullptr, client);
#else
  NotificationBridge bridge(client);
#endif
  bridge.bindNotification(0, 42, ActivationDescriptor{.process_lineage = {30, 20, 1}, .title_hint = "terminal"});
  bridge.handleAction(42, "dismiss");
  EXPECT_TRUE(client.calls.empty());
  bridge.handleAction(42, "open");
  ASSERT_EQ(client.calls.size(), 1U);
  bridge.handleAction(42, "open");
  EXPECT_EQ(client.calls.size(), 1U);

  bridge.bindNotification(0, 43, ActivationDescriptor{.process_lineage = {30}, .title_hint = {}});
  bridge.handleClosed(43);
  bridge.handleAction(43, "open");
  EXPECT_EQ(client.calls.size(), 1U);
}

TEST(ActivateSessionTest, UsesStoredDescriptorAndRejectsMissingOrEndedSessions) {
  SessionRegistry registry{ProcessAncestryReader(ancestryFixture)};
  registry.registerSession("codex", "session", 30, "/repo", {{"terminal_title", "terminal"}});
  FakeActivationClient client;
  EXPECT_TRUE(activateSession(registry, client, "session"));
  ASSERT_EQ(client.calls.size(), 1U);
  EXPECT_EQ(client.calls.front().title_hint, "terminal");
  EXPECT_FALSE(activateSession(registry, client, "unknown"));
  registry.endSession("session");
  EXPECT_FALSE(activateSession(registry, client, "session"));
  EXPECT_EQ(client.calls.size(), 1U);
}

TEST(ShellActivationClientTest, DefinesExactContractAndBoundedTimeout) {
  EXPECT_EQ(ShellActivationClient::kDestination, "org.holonight.Shell");
  EXPECT_EQ(ShellActivationClient::kPath, "/org/holonight/Shell");
  EXPECT_EQ(ShellActivationClient::kInterface, "org.holonight.Shell.WindowActivation1");
  EXPECT_EQ(ShellActivationClient::kMember, "RequestWindowActivation");
  EXPECT_EQ(ShellActivationClient::kInputSignature, "aus");
  EXPECT_EQ(ShellActivationClient::kTimeout, std::chrono::milliseconds(500));
}

TEST(ShellActivationClientTest, ClassifiesAllCallOutcomes) {
  EXPECT_EQ(ShellActivationClient::classifyCallResult(1, true).diagnostic, ActivationDiagnostic::Accepted);
  EXPECT_EQ(ShellActivationClient::classifyCallResult(1, false).diagnostic, ActivationDiagnostic::Rejected);
  EXPECT_EQ(ShellActivationClient::classifyCallResult(-ECONNREFUSED, std::nullopt).diagnostic,
            ActivationDiagnostic::Unavailable);
  EXPECT_EQ(ShellActivationClient::classifyCallResult(1, std::nullopt).diagnostic,
            ActivationDiagnostic::MalformedReply);
  EXPECT_EQ(ShellActivationClient::classifyCallResult(-ETIMEDOUT, std::nullopt).diagnostic,
            ActivationDiagnostic::Timeout);
}
