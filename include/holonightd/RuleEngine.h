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

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace holonightd {

/// Value match operator for rule condition evaluation.
enum class MatchOperator : std::uint8_t {
  Equals,
  NotEquals,
  GreaterThan,
  GreaterThanOrEqual,
  LessThan,
  LessThanOrEqual,
  Contains,
  Regex
};

/// Converts MatchOperator to string representation.
[[nodiscard]] std::string_view matchOperatorToString(MatchOperator operator_val) noexcept;

/// Parses string representation into MatchOperator.
[[nodiscard]] std::expected<MatchOperator, std::string> matchOperatorFromString(std::string_view str) noexcept;

/// Logical combination operator for rule conditions.
enum class LogicOperator : std::uint8_t { All, Any };

/// Converts LogicOperator to string representation.
[[nodiscard]] std::string_view logicOperatorToString(LogicOperator operator_val) noexcept;

/// Parses string representation into LogicOperator.
[[nodiscard]] std::expected<LogicOperator, std::string> logicOperatorFromString(std::string_view str) noexcept;

/// Condition criteria evaluated against observation events.
struct RuleCondition {
  std::optional<std::string> source;
  std::optional<std::string> category;
  std::optional<std::string> subject;
  std::optional<std::string> signal;
  std::optional<Severity> min_severity;
  std::optional<std::string> target_value;
  MatchOperator value_operator{MatchOperator::Equals};
};

/// Declarative diagnostic rule definition.
struct DiagnosticRule {
  std::string rule_id;
  std::string title;
  std::string category;
  Severity severity{Severity::Warning};
  LogicOperator condition_logic{LogicOperator::All};
  std::vector<RuleCondition> conditions;
  std::vector<std::string> candidate_causes;
  std::vector<std::string> suggested_actions;
};

/// Diagnostic finding output generated when a rule triggers on observation events.
struct DiagnosticFinding {
  std::string finding_id;
  std::string rule_id;
  std::string title;
  std::string category;
  Severity severity{Severity::Warning};
  std::vector<ObservationEvent> matched_events;
  std::vector<std::string> candidate_causes;
  std::vector<std::string> suggested_actions;
  std::string timestamp;  // ISO-8601 string
};

/// Deterministic diagnostic rule engine for evaluating observation events.
class RuleEngine {
 public:
  /// Initializes the RuleEngine with built-in default diagnostic rules.
  RuleEngine();

  /// Registers a custom diagnostic rule.
  void addRule(DiagnosticRule rule);

  /// Deserializes and validates a diagnostic rule from a JSON string.
  [[nodiscard]] static std::expected<DiagnosticRule, std::string> loadRuleFromJson(const std::string& json_str);

  /// Reads and parses a diagnostic rule from a JSON file.
  [[nodiscard]] static std::expected<DiagnosticRule, std::string> loadRuleFromFile(
      const std::filesystem::path& file_path);

  /// Scans a directory recursively and loads all valid JSON rule files.
  std::size_t loadRulesFromDirectory(const std::filesystem::path& dir_path);

  /// Returns active registered diagnostic rules.
  [[nodiscard]] const std::vector<DiagnosticRule>& getRules() const noexcept { return rules_; }

  /// Clears all active rules.
  void clearRules() noexcept { rules_.clear(); }

  /// Evaluates observation events against registered rules and produces diagnostic findings.
  /// Guaranteed not to throw exceptions.
  [[nodiscard]] std::vector<DiagnosticFinding> evaluate(std::span<const ObservationEvent> events) const noexcept;

 private:
  void registerBuiltInRules();

  std::vector<DiagnosticRule> rules_;
};

}  // namespace holonightd
