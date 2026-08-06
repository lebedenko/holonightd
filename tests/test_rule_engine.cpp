// SPDX-License-Identifier: GPL-3.0-or-later
// holonightd - Lightweight daemon for routine maintenance automation.
// Copyright (C) 2026 holonightd contributors

#include "holonightd/RuleEngine.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace holonightd;

TEST(RuleEngineTest, EnumConversions) {
  EXPECT_EQ(matchOperatorToString(MatchOperator::Equals), "equals");
  EXPECT_EQ(matchOperatorToString(MatchOperator::GreaterThan), "greater_than");

  auto op_res = matchOperatorFromString("greater_than_or_equal");
  ASSERT_TRUE(op_res.has_value());
  EXPECT_EQ(*op_res, MatchOperator::GreaterThanOrEqual);

  EXPECT_FALSE(matchOperatorFromString("invalid_op").has_value());

  EXPECT_EQ(logicOperatorToString(LogicOperator::All), "all");
  EXPECT_EQ(logicOperatorToString(LogicOperator::Any), "any");

  auto logic_res = logicOperatorFromString("any");
  ASSERT_TRUE(logic_res.has_value());
  EXPECT_EQ(*logic_res, LogicOperator::Any);
}

TEST(RuleEngineTest, InitializesWithBuiltInRules) {
  RuleEngine engine;
  const auto& rules = engine.getRules();
  EXPECT_GE(rules.size(), 5U);

  bool found_kernel_rule = false;
  bool found_pacman_rule = false;

  for (const auto& rule : rules) {
    if (rule.rule_id == "RULE-KERNEL-001") {
      found_kernel_rule = true;
    }
    if (rule.rule_id == "RULE-PACMAN-001") {
      found_pacman_rule = true;
    }
  }

  EXPECT_TRUE(found_kernel_rule);
  EXPECT_TRUE(found_pacman_rule);
}

TEST(RuleEngineTest, ParsesValidJsonRule) {
  const std::string json_str = R"({
    "rule_id": "RULE-TEST-001",
    "title": "Test Custom Rule",
    "category": "custom",
    "severity": "warning",
    "condition_logic": "all",
    "conditions": [
      {
        "category": "package",
        "signal": "pacman.kernel_mismatch",
        "min_severity": "warning"
      }
    ],
    "candidate_causes": ["Test cause"],
    "suggested_actions": ["action.test"]
  })";

  auto rule_res = RuleEngine::loadRuleFromJson(json_str);
  ASSERT_TRUE(rule_res.has_value());
  EXPECT_EQ(rule_res->rule_id, "RULE-TEST-001");
  EXPECT_EQ(rule_res->title, "Test Custom Rule");
  EXPECT_EQ(rule_res->severity, Severity::Warning);
  EXPECT_EQ(rule_res->condition_logic, LogicOperator::All);
  ASSERT_EQ(rule_res->conditions.size(), 1U);
  EXPECT_EQ(rule_res->conditions[0].signal.value_or(""), "pacman.kernel_mismatch");
}

TEST(RuleEngineTest, RejectsMalformedJsonRule) {
  const std::string malformed_json = R"({ "rule_id": "RULE-BAD" })";  // missing required fields
  auto rule_res = RuleEngine::loadRuleFromJson(malformed_json);
  EXPECT_FALSE(rule_res.has_value());
}

TEST(RuleEngineTest, EvaluatesKernelMismatchEvent) {
  RuleEngine engine;

  ObservationEvent event;
  event.source = "pacman_collector";
  event.category = "package";
  event.subject = "kernel";
  event.signal = "pacman.kernel_mismatch";
  event.severity = Severity::Warning;
  event.value = true;

  std::vector<ObservationEvent> events = {event};
  const auto findings = engine.evaluate(events);

  ASSERT_GE(findings.size(), 1U);
  bool found_mismatch = false;
  for (const auto& finding : findings) {
    if (finding.rule_id == "RULE-KERNEL-001") {
      found_mismatch = true;
      EXPECT_EQ(finding.severity, Severity::Warning);
      EXPECT_FALSE(finding.suggested_actions.empty());
      EXPECT_FALSE(finding.timestamp.empty());
    }
  }
  EXPECT_TRUE(found_mismatch);
}

TEST(RuleEngineTest, EvaluatesStalePacmanLockEvent) {
  RuleEngine engine;

  ObservationEvent event;
  event.source = "pacman_collector";
  event.category = "package";
  event.subject = "pacman_db";
  event.signal = "pacman.stale_lock";
  event.severity = Severity::Warning;
  event.value = static_cast<std::int64_t>(12345);

  std::vector<ObservationEvent> events = {event};
  const auto findings = engine.evaluate(events);

  ASSERT_GE(findings.size(), 1U);
  bool found_lock_finding = false;
  for (const auto& finding : findings) {
    if (finding.rule_id == "RULE-PACMAN-001") {
      found_lock_finding = true;

      EXPECT_EQ(finding.category, "package");
    }
  }
  EXPECT_TRUE(found_lock_finding);
}

TEST(RuleEngineTest, PerformanceBenchmarkUnder10ms) {
  RuleEngine engine;

  // Synthesize 1,000 observation events
  std::vector<ObservationEvent> events;
  events.reserve(1000);
  for (int i = 0; i < 1000; ++i) {
    ObservationEvent event;
    event.source = "system_test";
    event.category = (i % 2 == 0) ? "storage" : "package";
    event.subject = "test_target_" + std::to_string(i);
    event.signal = (i % 5 == 0) ? "space_pressure" : "heartbeat";
    event.severity = (i % 10 == 0) ? Severity::Warning : Severity::Info;
    event.value = static_cast<double>(i % 100);
    events.push_back(event);
  }

  const auto start = std::chrono::high_resolution_clock::now();
  const auto findings = engine.evaluate(events);
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();

  EXPECT_LT(elapsed, 10);
  (void)findings;
}
