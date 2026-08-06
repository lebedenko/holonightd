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

#include "holonightd/RuleEngine.h"

// NOLINTBEGIN
#include <nlohmann/json.hpp>
// NOLINTEND

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <system_error>
#include <utility>

namespace holonightd {

namespace {
using json = nlohmann::json;

std::uint64_t nextFindingSequence() {
  static std::atomic<std::uint64_t> counter{1};
  return counter.fetch_add(1, std::memory_order_relaxed);
}

std::string generateFindingId() {
  const auto time_ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
  const auto seq = nextFindingSequence();
  std::ostringstream stream;
  stream << "finding-" << std::hex << time_ns << "-" << seq;
  return stream.str();
}

std::string formatIsoTimestamp() {
  const auto now = std::chrono::system_clock::now();
  const auto time_t_now = std::chrono::system_clock::to_time_t(now);
  std::tm utc_tm{};
  gmtime_r(&time_t_now, &utc_tm);

  std::ostringstream stream;
  stream << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
  return stream.str();
}

bool evaluateNumericComparison(double event_num, double target_num, MatchOperator operator_val) noexcept {
  switch (operator_val) {
    case MatchOperator::Equals:
      return event_num == target_num;
    case MatchOperator::NotEquals:
      return event_num != target_num;
    case MatchOperator::GreaterThan:
      return event_num > target_num;
    case MatchOperator::GreaterThanOrEqual:
      return event_num >= target_num;
    case MatchOperator::LessThan:
      return event_num < target_num;
    case MatchOperator::LessThanOrEqual:
      return event_num <= target_num;
    default:
      return false;
  }
}

bool matchValueOperator(const EventValue& value, const std::string& target_str, MatchOperator operator_val) noexcept {
  std::string event_str;
  std::optional<double> event_num;

  std::visit(
      [&event_str, &event_num](const auto& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>) {
          event_str = arg ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
          event_str = std::to_string(arg);
          event_num = static_cast<double>(arg);
        } else if constexpr (std::is_same_v<T, double>) {
          event_str = std::to_string(arg);
          event_num = arg;
        } else if constexpr (std::is_same_v<T, std::string>) {
          event_str = arg;
          try {
            event_num = std::stod(arg);
          } catch (...) {
            // Not a numeric string
          }
        }
      },
      value);

  if (event_num.has_value() &&
      (operator_val == MatchOperator::GreaterThan || operator_val == MatchOperator::GreaterThanOrEqual ||
       operator_val == MatchOperator::LessThan || operator_val == MatchOperator::LessThanOrEqual)) {
    try {
      const double target_num = std::stod(target_str);
      return evaluateNumericComparison(*event_num, target_num, operator_val);
    } catch (...) {
      return false;
    }
  }

  switch (operator_val) {
    case MatchOperator::Equals:
      return event_str == target_str;
    case MatchOperator::NotEquals:
      return event_str != target_str;
    case MatchOperator::Contains:
      return event_str.contains(target_str);
    case MatchOperator::Regex: {
      try {
        const std::regex regex_pattern(target_str);
        return std::regex_search(event_str, regex_pattern);
      } catch (...) {
        return false;
      }
    }
    default:
      return false;
  }
}

bool matchEventToCondition(const ObservationEvent& event, const RuleCondition& condition) noexcept {
  if (condition.source.has_value() && event.source != *condition.source) {
    return false;
  }
  if (condition.category.has_value() && event.category != *condition.category) {
    return false;
  }
  if (condition.subject.has_value() && event.subject != *condition.subject) {
    return false;
  }
  if (condition.signal.has_value() && event.signal != *condition.signal) {
    return false;
  }
  if (condition.min_severity.has_value()) {
    if (static_cast<std::uint8_t>(event.severity) < static_cast<std::uint8_t>(*condition.min_severity)) {
      return false;
    }
  }
  if (condition.target_value.has_value()) {
    if (!matchValueOperator(event.value, *condition.target_value, condition.value_operator)) {
      return false;
    }
  }
  return true;
}

bool evaluateAllLogic(const DiagnosticRule& rule, std::span<const ObservationEvent> events,
                      std::vector<ObservationEvent>& matched_events) noexcept {
  for (const auto& condition : rule.conditions) {
    bool condition_matched = false;
    for (const auto& event : events) {
      if (matchEventToCondition(event, condition)) {
        condition_matched = true;
        matched_events.push_back(event);
      }
    }
    if (!condition_matched) {
      return false;
    }
  }
  return true;
}

bool evaluateAnyLogic(const DiagnosticRule& rule, std::span<const ObservationEvent> events,
                      std::vector<ObservationEvent>& matched_events) noexcept {
  bool rule_satisfied = false;
  for (const auto& condition : rule.conditions) {
    for (const auto& event : events) {
      if (matchEventToCondition(event, condition)) {
        rule_satisfied = true;
        matched_events.push_back(event);
      }
    }
  }
  return rule_satisfied;
}

void parseConditionItem(const json& item, RuleCondition& cond) {
  if (item.contains("source") && item["source"].is_string()) {
    cond.source = item["source"].get<std::string>();
  }
  if (item.contains("category") && item["category"].is_string()) {
    cond.category = item["category"].get<std::string>();
  }
  if (item.contains("subject") && item["subject"].is_string()) {
    cond.subject = item["subject"].get<std::string>();
  }
  if (item.contains("signal") && item["signal"].is_string()) {
    cond.signal = item["signal"].get<std::string>();
  }
  if (item.contains("min_severity") && item["min_severity"].is_string()) {
    const auto sev_res = severityFromString(item["min_severity"].get<std::string>());
    if (sev_res.has_value()) {
      cond.min_severity = *sev_res;
    }
  }
  if (item.contains("target_value") && item["target_value"].is_string()) {
    cond.target_value = item["target_value"].get<std::string>();
  }
  if (item.contains("value_operator") && item["value_operator"].is_string()) {
    const auto op_res = matchOperatorFromString(item["value_operator"].get<std::string>());
    if (op_res.has_value()) {
      cond.value_operator = *op_res;
    }
  }
}

std::vector<std::string> parseStringArray(const json& root, const char* key) {
  std::vector<std::string> result;
  if (root.contains(key) && root[key].is_array()) {
    for (const auto& item : root[key]) {
      if (item.is_string()) {
        result.push_back(item.get<std::string>());
      }
    }
  }
  return result;
}

}  // namespace

std::string_view matchOperatorToString(MatchOperator operator_val) noexcept {
  switch (operator_val) {
    case MatchOperator::Equals:
      return "equals";
    case MatchOperator::NotEquals:
      return "not_equals";
    case MatchOperator::GreaterThan:
      return "greater_than";
    case MatchOperator::GreaterThanOrEqual:
      return "greater_than_or_equal";
    case MatchOperator::LessThan:
      return "less_than";
    case MatchOperator::LessThanOrEqual:
      return "less_than_or_equal";
    case MatchOperator::Contains:
      return "contains";
    case MatchOperator::Regex:
      return "regex";
  }
  return "equals";
}

std::expected<MatchOperator, std::string> matchOperatorFromString(std::string_view str) noexcept {
  if (str == "equals") {
    return MatchOperator::Equals;
  }
  if (str == "not_equals") {
    return MatchOperator::NotEquals;
  }
  if (str == "greater_than") {
    return MatchOperator::GreaterThan;
  }
  if (str == "greater_than_or_equal") {
    return MatchOperator::GreaterThanOrEqual;
  }
  if (str == "less_than") {
    return MatchOperator::LessThan;
  }
  if (str == "less_than_or_equal") {
    return MatchOperator::LessThanOrEqual;
  }
  if (str == "contains") {
    return MatchOperator::Contains;
  }
  if (str == "regex") {
    return MatchOperator::Regex;
  }
  return std::unexpected("Invalid MatchOperator: " + std::string(str));
}

std::string_view logicOperatorToString(LogicOperator operator_val) noexcept {
  switch (operator_val) {
    case LogicOperator::All:
      return "all";
    case LogicOperator::Any:
      return "any";
  }
  return "all";
}

std::expected<LogicOperator, std::string> logicOperatorFromString(std::string_view str) noexcept {
  if (str == "all") {
    return LogicOperator::All;
  }
  if (str == "any") {
    return LogicOperator::Any;
  }
  return std::unexpected("Invalid LogicOperator: " + std::string(str));
}

RuleEngine::RuleEngine() { registerBuiltInRules(); }

void RuleEngine::addRule(DiagnosticRule rule) { rules_.push_back(std::move(rule)); }

void RuleEngine::registerBuiltInRules() {
  // 1. Kernel Version Mismatch
  DiagnosticRule kernel_rule;
  kernel_rule.rule_id = "RULE-KERNEL-001";
  kernel_rule.title = "Kernel Version Mismatch";
  kernel_rule.category = "kernel";
  kernel_rule.severity = Severity::Warning;
  kernel_rule.condition_logic = LogicOperator::All;
  kernel_rule.conditions = {RuleCondition{
      .source = std::nullopt,
      .category = "package",
      .subject = "kernel",
      .signal = "pacman.kernel_mismatch",
      .min_severity = Severity::Warning,
      .target_value = std::nullopt,
      .value_operator = MatchOperator::Equals,
  }};
  kernel_rule.candidate_causes = {
      "Kernel package was upgraded via pacman/system update but system has not been rebooted."};
  kernel_rule.suggested_actions = {"action.system.reboot_recommended", "action.pacman.check_pending_updates"};
  addRule(std::move(kernel_rule));

  // 2. Stale Pacman Database Lock
  DiagnosticRule pacman_lock_rule;
  pacman_lock_rule.rule_id = "RULE-PACMAN-001";
  pacman_lock_rule.title = "Stale Pacman Database Lock";
  pacman_lock_rule.category = "package";
  pacman_lock_rule.severity = Severity::Warning;
  pacman_lock_rule.condition_logic = LogicOperator::All;
  pacman_lock_rule.conditions = {RuleCondition{
      .source = std::nullopt,
      .category = "package",
      .subject = "pacman_db",
      .signal = "pacman.stale_lock",
      .min_severity = Severity::Warning,
      .target_value = std::nullopt,
      .value_operator = MatchOperator::Equals,
  }};
  pacman_lock_rule.candidate_causes = {"Interrupted pacman transaction or dead process left orphan db.lck file."};
  pacman_lock_rule.suggested_actions = {"action.pacman.remove_stale_lock", "action.pacman.verify_db_integrity"};
  addRule(std::move(pacman_lock_rule));

  // 3. Storage Space / Inode Pressure
  DiagnosticRule storage_rule;
  storage_rule.rule_id = "RULE-STORAGE-001";
  storage_rule.title = "Storage Pressure Warning";
  storage_rule.category = "storage";
  storage_rule.severity = Severity::Warning;
  storage_rule.condition_logic = LogicOperator::Any;
  storage_rule.conditions = {
      RuleCondition{
          .source = std::nullopt,
          .category = "storage",
          .subject = std::nullopt,
          .signal = "space_pressure",
          .min_severity = Severity::Warning,
          .target_value = std::nullopt,
          .value_operator = MatchOperator::Equals,
      },
      RuleCondition{
          .source = std::nullopt,
          .category = "storage",
          .subject = std::nullopt,
          .signal = "inode_pressure",
          .min_severity = Severity::Warning,
          .target_value = std::nullopt,
          .value_operator = MatchOperator::Equals,
      },
  };
  storage_rule.candidate_causes = {"Filesystem volume or inode pool is nearing full capacity."};
  storage_rule.suggested_actions = {"action.storage.clean_journal_logs", "action.storage.clean_pacman_cache"};
  addRule(std::move(storage_rule));

  // 4. Memory Pressure / OOM Event
  DiagnosticRule memory_rule;
  memory_rule.rule_id = "RULE-MEMORY-001";
  memory_rule.title = "Memory Pressure or OOM Event";
  memory_rule.category = "memory";
  memory_rule.severity = Severity::Error;
  memory_rule.condition_logic = LogicOperator::Any;
  memory_rule.conditions = {
      RuleCondition{
          .source = std::nullopt,
          .category = "memory",
          .subject = std::nullopt,
          .signal = "oom_kill",
          .min_severity = Severity::Warning,
          .target_value = std::nullopt,
          .value_operator = MatchOperator::Equals,
      },
      RuleCondition{
          .source = std::nullopt,
          .category = "memory",
          .subject = std::nullopt,
          .signal = "memory_pressure",
          .min_severity = Severity::Warning,
          .target_value = std::nullopt,
          .value_operator = MatchOperator::Equals,
      },
  };
  memory_rule.candidate_causes = {"High memory allocation triggered kernel OOM killer or system memory pressure."};
  memory_rule.suggested_actions = {"action.memory.identify_top_consumers", "action.memory.restart_leaking_services"};
  addRule(std::move(memory_rule));

  // 5. Systemd Unit Failure
  DiagnosticRule systemd_rule;
  systemd_rule.rule_id = "RULE-SYSTEMD-001";
  systemd_rule.title = "Systemd Unit Failure";
  systemd_rule.category = "service";
  systemd_rule.severity = Severity::Error;
  systemd_rule.condition_logic = LogicOperator::All;
  systemd_rule.conditions = {RuleCondition{
      .source = std::nullopt,
      .category = "systemd",
      .subject = std::nullopt,
      .signal = "unit_failed",
      .min_severity = Severity::Warning,
      .target_value = std::nullopt,
      .value_operator = MatchOperator::Equals,
  }};
  systemd_rule.candidate_causes = {"System service entered failed state."};
  systemd_rule.suggested_actions = {"action.systemd.restart_unit", "action.systemd.inspect_unit_logs"};
  addRule(std::move(systemd_rule));
}

std::expected<DiagnosticRule, std::string> RuleEngine::loadRuleFromJson(const std::string& json_str) {
  try {
    const auto parsed_json = json::parse(json_str);

    DiagnosticRule rule;
    if (!parsed_json.contains("rule_id") || !parsed_json["rule_id"].is_string()) {
      return std::unexpected("Missing or invalid 'rule_id' string");
    }
    rule.rule_id = parsed_json["rule_id"].get<std::string>();

    if (!parsed_json.contains("title") || !parsed_json["title"].is_string()) {
      return std::unexpected("Missing or invalid 'title' string");
    }
    rule.title = parsed_json["title"].get<std::string>();

    if (!parsed_json.contains("category") || !parsed_json["category"].is_string()) {
      return std::unexpected("Missing or invalid 'category' string");
    }
    rule.category = parsed_json["category"].get<std::string>();

    if (parsed_json.contains("severity") && parsed_json["severity"].is_string()) {
      const auto sev_res = severityFromString(parsed_json["severity"].get<std::string>());
      if (!sev_res.has_value()) {
        return std::unexpected(sev_res.error());
      }
      rule.severity = *sev_res;
    }

    if (parsed_json.contains("condition_logic") && parsed_json["condition_logic"].is_string()) {
      const auto logic_res = logicOperatorFromString(parsed_json["condition_logic"].get<std::string>());
      if (!logic_res.has_value()) {
        return std::unexpected(logic_res.error());
      }
      rule.condition_logic = *logic_res;
    }

    if (parsed_json.contains("conditions") && parsed_json["conditions"].is_array()) {
      for (const auto& item : parsed_json["conditions"]) {
        RuleCondition cond;
        parseConditionItem(item, cond);
        rule.conditions.push_back(std::move(cond));
      }
    }

    rule.candidate_causes = parseStringArray(parsed_json, "candidate_causes");
    rule.suggested_actions = parseStringArray(parsed_json, "suggested_actions");

    return rule;
  } catch (const std::exception& ex) {
    return std::unexpected(std::string("JSON parse error: ") + ex.what());
  }
}

std::expected<DiagnosticRule, std::string> RuleEngine::loadRuleFromFile(const std::filesystem::path& file_path) {
  std::error_code fs_error;
  if (!std::filesystem::exists(file_path, fs_error)) {
    return std::unexpected("File does not exist: " + file_path.string());
  }

  std::ifstream stream(file_path);
  if (!stream.is_open()) {
    return std::unexpected("Could not open file: " + file_path.string());
  }

  std::stringstream stream_buffer;
  stream_buffer << stream.rdbuf();
  return loadRuleFromJson(stream_buffer.str());
}

std::size_t RuleEngine::loadRulesFromDirectory(const std::filesystem::path& dir_path) {
  std::error_code fs_error;
  if (!std::filesystem::exists(dir_path, fs_error) || !std::filesystem::is_directory(dir_path, fs_error)) {
    return 0;
  }

  std::size_t loaded_count = 0;
  const auto opts = std::filesystem::directory_options::skip_permission_denied;
  std::filesystem::recursive_directory_iterator dir_iter(dir_path, opts, fs_error);
  const std::filesystem::recursive_directory_iterator end;

  while (dir_iter != end && !fs_error) {
    const auto& entry = *dir_iter;
    if (entry.is_regular_file(fs_error) && entry.path().extension() == ".json") {
      auto rule_res = loadRuleFromFile(entry.path());
      if (rule_res.has_value()) {
        addRule(std::move(*rule_res));
        loaded_count++;
      }
    }
    dir_iter.increment(fs_error);
  }

  return loaded_count;
}

std::vector<DiagnosticFinding> RuleEngine::evaluate(std::span<const ObservationEvent> events) const noexcept {
  std::vector<DiagnosticFinding> findings;

  try {
    for (const auto& rule : rules_) {
      if (rule.conditions.empty()) {
        continue;
      }

      std::vector<ObservationEvent> matched_events;
      const bool rule_satisfied = (rule.condition_logic == LogicOperator::All)
                                      ? evaluateAllLogic(rule, events, matched_events)
                                      : evaluateAnyLogic(rule, events, matched_events);

      if (rule_satisfied && !matched_events.empty()) {
        DiagnosticFinding finding;
        finding.finding_id = generateFindingId();
        finding.rule_id = rule.rule_id;
        finding.title = rule.title;
        finding.category = rule.category;
        finding.severity = rule.severity;
        finding.matched_events = std::move(matched_events);
        finding.candidate_causes = rule.candidate_causes;
        finding.suggested_actions = rule.suggested_actions;
        finding.timestamp = formatIsoTimestamp();

        findings.push_back(std::move(finding));
      }
    }
  } catch (...) {
    // Enforce noexcept contract
  }

  return findings;
}

}  // namespace holonightd
