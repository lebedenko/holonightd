# SDD Tasks — daemon-status-cli

- [x] T-001: Add PacmanConfig, rules_dir, and status flag to `Application.h`
  - REQs: REQ-F-001, REQ-F-003
  - Check: `include/holonightd/Application.h` declares `PacmanConfig`, updates `Config` with `pacman` and `rules_dir`, and updates `CliOptions` with `status`.

- [x] T-002: Implement TOML parsing for `[pacman]` and `[rules]` sections in `Application.cpp`
  - REQs: REQ-F-002
  - Check: `Config::fromFile` populates `PacmanConfig` from `[pacman]` section and `rules_dir` from `[rules]` section when present in the TOML file.

- [x] T-003: Update argument parsing in `main.cpp` for `--status` flag
  - REQs: REQ-F-003
  - Check: `parseArgs` in `src/main.cpp` recognizes `--status` and sets `CliOptions::status` to true, and `--help` displays `--status` usage info.

- [x] T-004: Update `Daemon.h` header with collector & rule engine members and `runStatusCheck()` interface
  - REQs: REQ-F-004
  - Check: `include/holonightd/Daemon.h` declares instance members for `SystemdCollector`, `StorageCollector`, `MemoryCollector`, `PacmanCollector`, `RuleEngine`, and public `runStatusCheck()` declaration.

- [x] T-005: Implement `collectAllEvents()` helper in `Daemon.cpp` with exception isolation across collectors
  - REQs: REQ-F-005, REQ-F-008, REQ-NF-002
  - Check: `collectAllEvents()` calls all four collectors sequentially, catches exceptions per collector without aborting remaining collectors, and returns the aggregated vector of events.

- [x] T-006: Implement `formatStatusReport()` rendering structured ASCII diagnostic output
  - REQs: REQ-F-007, REQ-NF-003
  - Check: `formatStatusReport()` produces a structured ASCII string formatted with timestamp, overall status (UNHEALTHY/OK), active findings with severity, matched events, candidate causes, suggested actions, and summary.

- [x] T-007: Wire `runIteration()` loop in `Daemon.cpp` to run collectors, store events, and log findings
  - REQs: REQ-F-004, REQ-F-005, REQ-F-006, REQ-NF-002
  - Check: `runIteration()` calls `collectAllEvents()`, persists events batch to `EventStore`, evaluates events via `RuleEngine`, and logs findings.

- [x] T-008: Implement `runStatusCheck()` and `--status` execution path in `main.cpp`
  - REQs: REQ-F-007, REQ-F-008, REQ-NF-001
  - Check: Executing `holonightd --status` performs a single diagnostic check, outputs ASCII status report, and exits with 1 if error/critical findings exist or 0 otherwise within 2.0 seconds.

- [x] T-009: Add unit tests in `tests/test_application.cpp` for pacman config parsing and `--status` flag parsing
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-C-002
  - Check: `test_application.cpp` verifies default `PacmanConfig`, parsing of `[pacman]` and `[rules]` sections from TOML, and `--status` argument parsing.

- [x] T-010: Add unit tests in `tests/test_daemon.cpp` (or update existing tests) and verify code quality (`task test`, `task format-check`, `task tidy-src`)
  - REQs: REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-007, REQ-F-008, REQ-C-001, REQ-C-002
  - Check: `task test`, `task format-check`, and `task tidy-src` pass with 100% test success and zero static analysis warnings.
