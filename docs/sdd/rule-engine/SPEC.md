# RuleEngine (Declarative Knowledge Base & Rule Engine) — EARS Specification

**Feature ID:** `rule-engine`  
**Slug:** `rule-engine`  
**Project:** holonightd (Linux daemon, C++23)  
**Header / Implementation:** `include/holonightd/RuleEngine.h` / `src/holonightd/RuleEngine.cpp`  
**Namespace:** `holonightd`  
**Date:** 2026-08-01  
**Phase:** 1.2 Diagnostic Engine & Rule Evaluation  

---

## 1. Overview & Architecture

The `RuleEngine` component provides a declarative, JSON-driven knowledge base and evaluation engine for `holonightd`. It processes normalized `ObservationEvent` objects emitted by collectors (systemd, pacman, storage, memory/kernel, journal) and evaluates them against declarative diagnostic rules to produce structured `DiagnosticFinding` output.

### Core Responsibilities
1. **Rule Representation & Schema:** Define C++ data structures (`RuleCondition`, `DiagnosticRule`, `DiagnosticFinding`) and a JSON schema for diagnostic rules.
2. **Rule Ingestion:** Support loading rules dynamically from JSON files (`loadRuleFromJson`) or scanning directory trees (`loadRulesFromDirectory`).
3. **Built-in Knowledge Base:** Pre-load default rules for all Phase 1 collectors (kernel version mismatch, stale pacman locks, storage space/inode pressure, memory pressure/OOM, systemd unit failures).
4. **Deterministic Evaluation:** Pattern-match `ObservationEvent` vectors against active rules, supporting logical `ALL` / `ANY` condition operators and scalar comparison predicates.
5. **Exception-Safe Interface:** Provide a non-throwing `evaluate() const noexcept` method for deterministic runtime execution inside the daemon loop.

### Non-Goals
- **Action Execution:** The `RuleEngine` does **NOT** execute system repair commands, shell scripts, or mitigation actions directly. It only produces `DiagnosticFinding` objects containing recommended action identifiers (`suggested_actions`).
- **Persistence:** The `RuleEngine` does not directly query or write to SQLite (`EventStore`). It acts as a pure in-memory evaluation function over provided `ObservationEvent` collections.

---

## 2. Data Structures & Declarative JSON Schema

### 2.1 C++ Structs

```cpp
namespace holonightd {

enum class MatchOperator {
    Equals,
    NotEquals,
    GreaterThan,
    GreaterThanOrEqual,
    LessThan,
    LessThanOrEqual,
    Contains,
    Regex
};

enum class LogicOperator {
    All, // All conditions must match
    Any  // At least one condition must match
};

struct RuleCondition {
    std::optional<std::string> source;             // e.g. "pacman", "storage", "kernel"
    std::optional<std::string> category;           // e.g. "package", "storage", "memory"
    std::optional<std::string> subject;            // Exact string match or pattern e.g. "/var/lib/pacman/db.lck"
    std::optional<std::string> signal;             // e.g. "stale_lock", "space_pressure", "unit_failed"
    std::optional<Severity> min_severity;          // Minimum event severity threshold
    std::optional<std::string> target_value;       // Expected value for comparison
    MatchOperator value_operator{MatchOperator::Equals};
};

struct DiagnosticRule {
    std::string rule_id;                           // Unique identifier, e.g. "RULE-PACMAN-001"
    std::string title;                             // Human-readable summary
    std::string category;                          // Functional category
    Severity severity{Severity::Warning};           // Default finding severity
    LogicOperator condition_logic{LogicOperator::All};
    std::vector<RuleCondition> conditions;
    std::vector<std::string> candidate_causes;     // Hypothesized root causes
    std::vector<std::string> suggested_actions;    // Remediation action IDs
};

struct DiagnosticFinding {
    std::string finding_id;                        // Unique generated finding ID
    std::string rule_id;                           // Associated rule ID
    std::string title;                             // Rule title
    std::string category;                          // Domain category
    Severity severity;                             // Finding severity level
    std::vector<ObservationEvent> matched_events;  // Events triggering this finding
    std::vector<std::string> candidate_causes;     // Root causes from rule
    std::vector<std::string> suggested_actions;    // Action IDs for resolution
    std::string timestamp;                         // Finding generation timestamp (ISO-8601)
};

} // namespace holonightd
```

### 2.2 Declarative Rule JSON Schema

Diagnostic rules are specified in JSON format matching the following schema:

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "title": "DiagnosticRule",
  "type": "object",
  "required": [
    "rule_id",
    "title",
    "category",
    "severity",
    "conditions",
    "candidate_causes",
    "suggested_actions"
  ],
  "properties": {
    "rule_id": { "type": "string", "pattern": "^RULE-[A-Z0-9_-]+$" },
    "title": { "type": "string", "minLength": 1 },
    "category": { "type": "string", "minLength": 1 },
    "severity": { "type": "string", "enum": ["debug", "info", "warning", "error", "critical"] },
    "condition_logic": { "type": "string", "enum": ["all", "any"], "default": "all" },
    "conditions": {
      "type": "array",
      "minItems": 1,
      "items": {
        "type": "object",
        "properties": {
          "source": { "type": "string" },
          "category": { "type": "string" },
          "subject": { "type": "string" },
          "signal": { "type": "string" },
          "min_severity": { "type": "string", "enum": ["debug", "info", "warning", "error", "critical"] },
          "target_value": { "type": "string" },
          "value_operator": { 
            "type": "string", 
            "enum": ["equals", "not_equals", "greater_than", "greater_than_or_equal", "less_than", "less_than_or_equal", "contains", "regex"],
            "default": "equals"
          }
        }
      }
    },
    "candidate_causes": {
      "type": "array",
      "items": { "type": "string" }
    },
    "suggested_actions": {
      "type": "array",
      "items": { "type": "string" }
    }
  }
}
```

---

## 3. Built-in Phase 1 Diagnostic Rules

The `RuleEngine` comes with pre-populated built-in rules for all Phase 1 collectors:

### Rule 1: Kernel Version Mismatch (`RULE-KERNEL-001`)
- **Category:** `kernel`
- **Severity:** `warning`
- **Condition:** `source` = `"kernel"`, `signal` = `"version_mismatch"`
- **Candidate Causes:** Kernel package was upgraded via pacman/system update but system has not been rebooted.
- **Suggested Actions:** `["action.system.reboot_recommended", "action.pacman.check_pending_updates"]`

### Rule 2: Stale Pacman Database Lock (`RULE-PACMAN-001`)
- **Category:** `package`
- **Severity:** `error`
- **Condition:** `source` = `"pacman"`, `signal` = `"stale_lock"`, `subject` = `"/var/lib/pacman/db.lck"`
- **Candidate Causes:** Interrupted pacman transaction or crashed package manager process leaving orphan database lock.
- **Suggested Actions:** `["action.pacman.remove_stale_lock", "action.pacman.verify_db_integrity"]`

### Rule 3: Storage Space / Inode Pressure (`RULE-STORAGE-001`)
- **Category:** `storage`
- **Severity:** `critical`
- **Condition:** `source` = `"storage"`, `signal` = `"space_pressure"` OR `signal` = `"inode_pressure"`, `value_operator` = `"greater_than_or_equal"`, `target_value` = `"90"`
- **Candidate Causes:** Disk usage or inode consumption on mount point exceeded 90% threshold due to system logs, cache growth, or orphan files.
- **Suggested Actions:** `["action.storage.clean_journal_logs", "action.storage.clean_pacman_cache"]`

### Rule 4: Memory Pressure / OOM Event (`RULE-MEMORY-001`)
- **Category:** `memory`
- **Severity:** `critical`
- **Condition:** `source` = `"memory"`, `signal` = `"oom_kill"` OR `signal` = `"swap_exhaustion"`
- **Candidate Causes:** Out of memory condition triggered kernel OOM killer or swap partition hit maximum capacity.
- **Suggested Actions:** `["action.memory.identify_top_consumers", "action.memory.restart_leaking_services"]`

### Rule 5: Systemd Unit Failure (`RULE-SYSTEMD-001`)
- **Category:** `service`
- **Severity:** `error`
- **Condition:** `source` = `"systemd"`, `signal` = `"unit_failed"`
- **Candidate Causes:** System service or user unit entered failed state due to crash, exit code failure, or unmet dependency.
- **Suggested Actions:** `["action.systemd.restart_unit", "action.systemd.inspect_unit_logs"]`

---

## 4. Requirements & Acceptance Criteria

### 4.1 Functional Requirements (REQ-F)

#### REQ-F-001: Rule Engine Data Model & Structures
**Statement:** The `holonightd` `RuleEngine` shall represent diagnostic rules, rule evaluation conditions, and diagnostic findings using C++23 structures `DiagnosticRule`, `RuleCondition`, and `DiagnosticFinding`.

**Acceptance criteria:**
- `RuleCondition` supports filtering fields (`source`, `category`, `subject`, `signal`, `min_severity`, `target_value`, `value_operator`).
- `DiagnosticRule` stores unique `rule_id`, `title`, `category`, `severity`, `condition_logic`, a vector of `RuleCondition`s, `candidate_causes`, and `suggested_actions`.
- `DiagnosticFinding` contains `finding_id`, `rule_id`, `title`, `category`, `severity`, a vector of matching `ObservationEvent` objects, `candidate_causes`, `suggested_actions`, and ISO-8601 `timestamp`.
- Structures are defined in `include/holonightd/RuleEngine.h` under namespace `holonightd`.

---

#### REQ-F-002: Declarative JSON Rule Parsing & Validation
**Statement:** When a rule JSON string or payload is provided to the system, the `holonightd` `RuleEngine` shall parse and validate the payload against the required diagnostic rule schema.

**Acceptance criteria:**
- Valid rule JSON containing all required fields (`rule_id`, `title`, `category`, `severity`, `conditions`, `candidate_causes`, `suggested_actions`) deserializes cleanly into a `DiagnosticRule`.
- Enum values for `severity` (`"debug"`, `"info"`, `"warning"`, `"error"`, `"critical"`) and `value_operator` (`"equals"`, `"not_equals"`, `"greater_than"`, etc.) map correctly to C++ enum representations.
- Parsing returns `std::expected<DiagnosticRule, std::string>` containing error details on invalid JSON or missing fields.

---

#### REQ-F-003: Graceful Handling of Malformed JSON Rules
**Statement:** If a rule JSON payload is missing required fields or contains invalid syntax, then the `holonightd` `RuleEngine` shall reject the rule and return a descriptive error without modifying active engine state.

**Acceptance criteria:**
- Attempting to load malformed JSON returns an `std::unexpected` string identifying the specific missing field or JSON syntax error.
- Existing registered rules in `RuleEngine` remain untouched when rule loading fails.
- Invalid severity strings or unknown condition operators are rejected as validation errors.

---

#### REQ-F-004: Loading Custom Rules from Directory
**Statement:** When `loadRulesFromDirectory` is invoked, the `holonightd` `RuleEngine` shall recursively scan the target directory path and register all valid `.json` rule files.

**Acceptance criteria:**
- `loadRulesFromDirectory(dir_path)` traverses subdirectories to discover `.json` files.
- Valid rule files are parsed and appended to the active rule repository.
- Non-JSON files or unparseable files are skipped with error log entries, allowing remaining valid rules to load successfully.
- Returns the total count of successfully loaded rules.

---

#### REQ-F-005: Observation Event Evaluation and Finding Generation
**Statement:** When `evaluate` is invoked with a vector of `ObservationEvent` objects, the `holonightd` `RuleEngine` shall evaluate all registered diagnostic rules against the events and return matching `DiagnosticFinding` objects.

**Acceptance criteria:**
- `evaluate(events)` matches each registered `DiagnosticRule` against the input `ObservationEvent` slice.
- Each matching rule produces exactly one `DiagnosticFinding` containing the subset of `ObservationEvent`s that satisfied the rule conditions.
- Findings populate `candidate_causes` and `suggested_actions` directly from the triggering `DiagnosticRule`.
- If no rules match the input events, `evaluate` returns an empty vector.

---

#### REQ-F-006: Rule Condition Logic Operators (ALL vs ANY)
**Statement:** Where a `DiagnosticRule` specifies a condition match operator, the `holonightd` `RuleEngine` shall evaluate `ALL` or `ANY` condition criteria as declared by the rule.

**Acceptance criteria:**
- When `condition_logic` is `LogicOperator::All`, a rule triggers if and only if every `RuleCondition` in `conditions` matches at least one corresponding event.
- When `condition_logic` is `LogicOperator::Any`, a rule triggers if at least one `RuleCondition` in `conditions` matches an event.
- Empty event collections evaluate to no findings regardless of logic operator.

---

#### REQ-F-007: Built-in Default Phase 1 Diagnostic Knowledge Base
**Statement:** The `holonightd` `RuleEngine` shall include built-in default diagnostic rules for kernel version mismatch, stale pacman database locks, storage space and inode pressure, memory pressure and OOM events, and systemd unit failures.

**Acceptance criteria:**
- Default constructor `RuleEngine()` populates the engine with built-in rules `RULE-KERNEL-001`, `RULE-PACMAN-001`, `RULE-STORAGE-001`, `RULE-MEMORY-001`, and `RULE-SYSTEMD-001`.
- Built-in rules can be retrieved via `getRules()`.
- Built-in rules match test events synthesized for Phase 1 collectors.

---

#### REQ-F-008: Non-Execution Action Scope Restriction
**Statement:** The `holonightd` `RuleEngine` shall restrict finding output to diagnostic metadata and suggested action identifiers without executing external commands or modifying system state.

**Acceptance criteria:**
- `DiagnosticFinding` structures contain string action identifiers (e.g. `"action.pacman.remove_stale_lock"`).
- `RuleEngine` contains no subprocess execution logic (`fork`, `exec`, `system`) or file mutation calls.

---

### 4.2 Non-Functional Requirements (REQ-NF)

#### REQ-NF-001: Exception-Safe Non-Throwing Evaluation Interface
**Statement:** When processing rule evaluation, the `holonightd` `RuleEngine` shall provide a non-throwing `evaluate() const noexcept` interface that guarantees exception safety.

**Acceptance criteria:**
- Method signature is `std::vector<DiagnosticFinding> evaluate(std::span<const ObservationEvent> events) const noexcept;`.
- Internal logic handles edge cases (empty spans, unexpected null attributes) without raising or leaking C++ exceptions.

---

#### REQ-NF-002: In-Memory Evaluation Performance
**Statement:** When evaluating 1,000 `ObservationEvent` objects against 100 registered `DiagnosticRule` definitions, the `holonightd` `RuleEngine` shall complete execution within 10 milliseconds.

**Acceptance criteria:**
- Evaluated via microbenchmarks / unit tests using high-resolution timers.
- Rule matching avoids duplicate allocations or expensive regex compilation during evaluation loop.

---

#### REQ-NF-003: Memory Management & Safety
**Statement:** The `holonightd` `RuleEngine` shall store and manage rule collections using standard library RAII containers with zero raw owning pointers.

**Acceptance criteria:**
- Rule repository uses `std::vector<DiagnosticRule>`.
- Code compiles without compiler warnings on `-Wall -Wextra -Wpedantic`.

---

### 4.3 Constraints (REQ-C)

#### REQ-C-001: Modern C++23 Specification Conformance
**Statement:** The `holonightd` `RuleEngine` shall target standard C++23 within namespace `holonightd` declared in `include/holonightd/RuleEngine.h` and implemented in `src/holonightd/RuleEngine.cpp`.

**Acceptance criteria:**
- Uses C++23 features (`std::expected`, `std::optional`, `std::span`, ranges).
- Zero external dependencies outside nlohmann/json and existing holonightd headers.

---

#### REQ-C-002: Code Quality and Linting Compliance
**Statement:** The `holonightd` `RuleEngine` code and unit tests shall pass `task format-check`, `task tidy-src`, and `task test` without warnings or errors.

---

## 5. Header Declaration Sketch (`include/holonightd/RuleEngine.h`)

```cpp
#pragma once

#include "holonightd/ObservationEvent.h"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <span>
#include <expected>
#include <string>
#include <vector>

namespace holonightd {

enum class MatchOperator {
    Equals,
    NotEquals,
    GreaterThan,
    GreaterThanOrEqual,
    LessThan,
    LessThanOrEqual,
    Contains,
    Regex
};

enum class LogicOperator {
    All,
    Any
};

struct RuleCondition {
    std::optional<std::string> source;
    std::optional<std::string> category;
    std::optional<std::string> subject;
    std::optional<std::string> signal;
    std::optional<Severity> min_severity;
    std::optional<std::string> target_value;
    MatchOperator value_operator{MatchOperator::Equals};
};

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

struct DiagnosticFinding {
    std::string finding_id;
    std::string rule_id;
    std::string title;
    std::string category;
    Severity severity{Severity::Warning};
    std::vector<ObservationEvent> matched_events;
    std::vector<std::string> candidate_causes;
    std::vector<std::string> suggested_actions;
    std::string timestamp;
};

class RuleEngine {
public:
    RuleEngine(); // Initializes with Phase 1 built-in rules
    
    // Register custom rule
    void addRule(DiagnosticRule rule);
    
    // Ingestion methods
    static std::expected<DiagnosticRule, std::string> loadRuleFromJson(const std::string& json_str);
    static std::expected<DiagnosticRule, std::string> loadRuleFromFile(const std::filesystem::path& file_path);
    size_t loadRulesFromDirectory(const std::filesystem::path& dir_path);

    // Accessors
    [[nodiscard]] const std::vector<DiagnosticRule>& getRules() const noexcept { return rules_; }
    void clearRules() noexcept { rules_.clear(); }

    // Pure evaluation engine (non-throwing)
    [[nodiscard]] std::vector<DiagnosticFinding> evaluate(std::span<const ObservationEvent> events) const noexcept;

private:
    std::vector<DiagnosticRule> rules_;
    void registerBuiltInRules();
};

} // namespace holonightd
```
