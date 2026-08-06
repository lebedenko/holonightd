# SDD Tasks — rule-engine

- [x] T-001: Define MatchOperator, LogicOperator, RuleCondition, DiagnosticRule, DiagnosticFinding, and RuleEngine class in `include/holonightd/RuleEngine.h`.
  - REQs: REQ-F-001, REQ-C-001, REQ-NF-003
  - Check: Header `include/holonightd/RuleEngine.h` contains standard C++23 declarations for `MatchOperator`, `LogicOperator`, `RuleCondition`, `DiagnosticRule`, `DiagnosticFinding`, and `RuleEngine` under namespace `holonightd`.

- [x] T-002: Add CMake build target entry for `src/holonightd/RuleEngine.cpp` in `CMakeLists.txt` and `tests/CMakeLists.txt`.
  - REQs: REQ-C-001, REQ-C-003
  - Check: `src/holonightd/RuleEngine.cpp` is added to `CMakeLists.txt` and `tests/CMakeLists.txt`, allowing clean compilation via `task configure` and `task build`.

- [x] T-003: Implement rule serialization, deserialization (`loadRuleFromJson`), and validation in `src/holonightd/RuleEngine.cpp`.
  - REQs: REQ-F-002, REQ-F-003
  - Check: `loadRuleFromJson` successfully deserializes valid JSON rule strings into `DiagnosticRule` objects and returns `std::unexpected` with error details for malformed JSON or invalid schema properties.

- [x] T-004: Implement `loadRuleFromFile` and `loadRulesFromDirectory` ingestion methods with error resilience.
  - REQs: REQ-F-004
  - Check: `loadRulesFromDirectory` recursively parses all `.json` rule files in a directory tree, registering valid rules while logging and skipping unparseable files without aborting execution.

- [x] T-005: Implement built-in Phase 1 rules initialization (`registerBuiltInRules`).
  - REQs: REQ-F-007
  - Check: Constructing `RuleEngine()` automatically populates the engine with built-in rules `RULE-KERNEL-001`, `RULE-PACMAN-001`, `RULE-STORAGE-001`, `RULE-MEMORY-001`, and `RULE-SYSTEMD-001`.

- [x] T-006: Implement condition matching and operator evaluation (`matchCondition`, `MatchOperator` logic).
  - REQs: REQ-F-001, REQ-F-005, REQ-NF-002
  - Check: Individual condition matching logic correctly filters events by `source`, `category`, `subject`, `signal`, and `min_severity`, and evaluates string/numeric predicates for all `MatchOperator` variants.

- [x] T-007: Implement `evaluate()` engine loop supporting `ALL` vs `ANY` logic and finding generation.
  - REQs: REQ-F-005, REQ-F-006, REQ-F-008, REQ-NF-001
  - Check: `evaluate()` processes event spans in a non-throwing loop, correctly applying `ALL` and `ANY` logic operators to yield `DiagnosticFinding` objects with ISO-8601 timestamps and unique UUIDs without executing external processes.

- [x] T-008: Implement comprehensive unit tests in `tests/test_rule_engine.cpp` and verify code quality (`task test`, `task format-check`, `task tidy-src`).
  - REQs: REQ-C-002, REQ-NF-002
  - Check: Unit tests in `tests/test_rule_engine.cpp` cover rule parsing, built-in rules, condition logic, benchmark evaluation under 10ms, and `task test`, `task format-check`, and `task tidy-src` pass cleanly.
