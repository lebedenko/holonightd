# SDD Tasks — Journal Logging System

**Feature ID:** `journal-logging`  
**Project:** holonightd (Linux daemon, C++23)  
**Date:** 2026-07-31  

---

## Overview

This document specifies the ordered implementation task breakdown for the `journal-logging` feature in `holonightd`. It covers CMake build system updates, log level parsing and precedence resolution, logger sink dispatching (systemd journal and stdout fallback), main/daemon integration, and test coverage.

---

## Implementation Tasks

- [x] T-001: CMake build system updates for libsystemd detection and HOLONIGHTD_HAS_SYSTEMD compile definition
  - REQs: REQ-C-001
  - Check: CMake configuration detects `libsystemd` via `pkg-config` and defines `HOLONIGHTD_HAS_SYSTEMD` when available, or cleanly falls back to stdout logging without configuration or build errors when absent.

- [x] T-002: LogLevel enum class, case-insensitive parseLogLevel, and warn() log level addition in include/holonightd/Logger.h & src/holonightd/Logger.cpp
  - REQs: REQ-F-004, REQ-F-005
  - Check: `parseLogLevel` correctly maps case-insensitive string inputs (`"debug"`, `"INFO"`, `"warn"`, `"warning"`, `"ERROR"`) to `LogLevel` enum values and throws `std::invalid_argument` for invalid values like `"trace"`.

- [x] T-003: Config TOML parsing update for optional log_level setting and startup parsing validation
  - REQs: REQ-F-003, REQ-F-005
  - Check: `Config::fromFile` parses optional `log_level` from `[general]` table into `std::optional<std::string>` and validates it via `parseLogLevel`, throwing an exception on invalid strings.

- [x] T-004: resolveLogLevel function implementing 4-tier precedence hierarchy (CLI --debug/-d > HOLONIGHTD_LOG_LEVEL > TOML log_level > NDEBUG default)
  - REQs: REQ-F-003
  - Check: `resolveLogLevel` returns `DEBUG` for `--debug`/`-d`, otherwise checks `HOLONIGHTD_LOG_LEVEL`, otherwise TOML `log_level`, defaulting to `DEBUG` when `NDEBUG` is undefined or `INFO` when `NDEBUG` is defined.

- [x] T-005: Logger systemd journal sink (sd_journal_send) and ISO 8601 thread-safe stdout fallback sink implementation
  - REQs: REQ-F-001, REQ-F-002, REQ-F-006, REQ-NF-001, REQ-NF-002, REQ-NF-003, REQ-C-002
  - Check: `Logger` emits structured records via `sd_journal_send` when `HOLONIGHTD_HAS_SYSTEMD` is enabled and `force_stdout` is false, or writes thread-safe ISO 8601 formatted records (`YYYY-MM-DDTHH:MM:SS%z`) to `stdout` under mutex protection when forced or systemd is absent.

- [x] T-006: Integration in src/main.cpp and Daemon constructor to initialize logger with resolved level and sink mode
  - REQs: REQ-F-002, REQ-F-003
  - Check: `main.cpp` resolves the active log level and sink mode using CLI options, environment, and config, passing them to `Daemon` to initialize the active `Logger` before entering the event loop.

- [x] T-007: Unit tests in tests/test_logger.cpp and tests/test_application.cpp covering level parsing, validation errors, precedence rules, message filtering, and thread safety
  - REQs: REQ-F-003, REQ-F-004, REQ-F-005, REQ-F-006, REQ-NF-003
  - Check: Running `task test` passes all unit test assertions for log level parsing, precedence hierarchy resolution, invalid level startup aborts, severity message filtering, and multi-threaded logging safety.

---

## Task Dependency Graph

```
T-001 ──┐
        ├──→ T-005 ──┐
T-002 ──┼──→ T-004 ──┼──→ T-006 ──→ T-007
        │            │
T-003 ──┴────────────┘
```

---

## Acceptance Summary

| Task | Primary Target Files | Covered Requirements |
| :--- | :--- | :--- |
| **T-001** | `CMakeLists.txt` | REQ-C-001 |
| **T-002** | `include/holonightd/Logger.h`, `src/holonightd/Logger.cpp` | REQ-F-004, REQ-F-005 |
| **T-003** | `include/holonightd/Application.h`, `src/holonightd/Application.cpp` | REQ-F-003, REQ-F-005 |
| **T-004** | `include/holonightd/Application.h`, `src/holonightd/Application.cpp` | REQ-F-003 |
| **T-005** | `include/holonightd/Logger.h`, `src/holonightd/Logger.cpp` | REQ-F-001, REQ-F-002, REQ-F-006, REQ-NF-001, REQ-NF-002, REQ-NF-003, REQ-C-002 |
| **T-006** | `src/main.cpp`, `include/holonightd/Daemon.h`, `src/holonightd/Daemon.cpp` | REQ-F-002, REQ-F-003 |
| **T-007** | `tests/test_logger.cpp`, `tests/test_application.cpp` | REQ-F-003, REQ-F-004, REQ-F-005, REQ-F-006, REQ-NF-003 |
