# Journal Logging System — EARS Specification

**Feature ID:** `journal-logging`  
**Project:** holonightd (Linux daemon, C++23)  
**Date:** 2026-07-31  

---

## Overview

This specification defines the systemd journal logging system for `holonightd`. The feature equips the daemon with native Linux systemd journal integration, configurable log levels, CLI debug overrides, and a compile-time fallback sink for non-systemd environments.

The key capabilities introduced by this specification include:

1. **Systemd Journal Integration**: Direct logging to systemd journal via `libsystemd` (`sd_journal_send`) using structured fields (`MESSAGE`, `PRIORITY`, `SYSLOG_IDENTIFIER=holonightd`).
2. **CLI Debug Override**: Directing log output to `stdout` (`std::cout`) with ISO 8601 timestamps when invoked with `--debug` or `-d`.
3. **Four-Tier Precedence Hierarchy**: Log level configuration evaluated in strict order (CLI Flag > Environment Variable > TOML Config > Build Type Default).
4. **Strict Log Level Validation**: Case-insensitive parsing with immediate startup abort upon encountering invalid log level strings.
5. **CMake Dependency Detection**: Automated build-time detection of `libsystemd` via `pkg-config` with safe standard output fallback.

---

## Requirements

### Functional Requirements (REQ-F)

#### REQ-F-001: Structured systemd journal logging sink

**Statement:** When the daemon emits a log message during normal execution without the `--debug` or `-d` CLI flag, the system shall send structured log records to the systemd journal using `sd_journal_send` with `MESSAGE`, `PRIORITY`, and `SYSLOG_IDENTIFIER=holonightd`.

**Acceptance criteria:**
- Normal daemon runs (without `--debug` / `-d`) emit logs via `sd_journal_send`.
- Every journal entry includes `MESSAGE={log_message_text}`.
- Every journal entry includes `PRIORITY={syslog_priority_code}` mapped correctly (`DEBUG=7`, `INFO=6`, `WARN=4`, `ERROR=3`).
- Every journal entry includes `SYSLOG_IDENTIFIER=holonightd`.
- Output log messages are queryable using `journalctl SYSLOG_IDENTIFIER=holonightd`.

---

#### REQ-F-002: CLI debug flag override (--debug / -d)

**Statement:** When the user provides the `--debug` or `-d` CLI argument, the system shall set the active log level to `debug` and redirect all log messages to `stdout` (`std::cout`) formatted as `YYYY-MM-DDTHH:MM:SS%z LEVEL message`.

**Acceptance criteria:**
- Invoking `holonightd --debug` or `holonightd -d` sets the runtime log level to `debug`, overriding environment variables, TOML configuration, and build defaults.
- Running with `--debug` or `-d` redirects log output to `stdout` (`std::cout`) instead of invoking `sd_journal_send`.
- `stdout` log records follow the format `YYYY-MM-DDTHH:MM:SS%z LEVEL message` (e.g., `2026-07-31T02:51:31+0300 [DEBUG] Daemon event loop started`).
- No log entries are sent to the systemd journal while CLI debug mode is active.

---

#### REQ-F-003: Log level precedence resolution

**Statement:** When resolving the effective log level at startup, the system shall evaluate log level sources in order of precedence: CLI `--debug`/`-d` flag, `HOLONIGHTD_LOG_LEVEL` environment variable, TOML `log_level` parameter, and build type default.

**Acceptance criteria:**
- **Tier 1 (CLI Flag):** `--debug` or `-d` forces log level to `debug` and sink to `stdout`.
- **Tier 2 (Environment Variable):** In the absence of `--debug`/`-d`, a valid `HOLONIGHTD_LOG_LEVEL` environment variable sets the log level, overriding TOML configuration and build defaults.
- **Tier 3 (TOML Config):** In the absence of `--debug`/`-d` and `HOLONIGHTD_LOG_LEVEL`, the `log_level` key under `[general]` in `holonightd.toml` sets the log level.
- **Tier 4 (Build Type Default):** If no CLI flag, environment variable, or TOML key is present, `Debug` and `RelWithDebInfo` builds default to `debug`, while `Release` and `MinSizeRel` builds default to `info`.

---

#### REQ-F-004: Supported log levels and case-insensitive parsing

**Statement:** When parsing a log level string from `HOLONIGHTD_LOG_LEVEL` or the TOML `log_level` setting, the system shall accept `debug`, `info`, `warn` (or `warning`), and `error` in a case-insensitive manner.

**Acceptance criteria:**
- Inputs `"debug"`, `"DEBUG"`, `"Debug"` parse to log level `DEBUG`.
- Inputs `"info"`, `"INFO"`, `"Info"` parse to log level `INFO`.
- Inputs `"warn"`, `"WARN"`, `"warning"`, `"WARNING"` parse to log level `WARN`.
- Inputs `"error"`, `"ERROR"`, `"Error"` parse to log level `ERROR`.
- Messages with severity below the resolved active log level are filtered out and not emitted to the sink.

---

#### REQ-F-005: Strict validation of log level configuration

**Statement:** If an invalid log level string is specified in the `HOLONIGHTD_LOG_LEVEL` environment variable or TOML `log_level` configuration parameter, then the system shall throw a descriptive exception, write an error message to `stderr`, and abort startup immediately with a non-zero exit code.

**Acceptance criteria:**
- Setting `HOLONIGHTD_LOG_LEVEL=trace` causes startup to fail immediately with an error message containing the invalid value `"trace"`.
- Setting `log_level = "verbose"` in `holonightd.toml` causes startup to fail immediately with a descriptive parsing exception.
- Startup abort occurs prior to daemon initialization or event loop dispatch.
- Process exit code is non-zero (e.g., `1`).

---

#### REQ-F-006: Dynamic log message filtering by level

**Statement:** When a log event is dispatched, the system shall compare the event log level against the active threshold and drop events with lower severity than the active threshold.

**Acceptance criteria:**
- At `info` level, `debug` messages are suppressed while `info`, `warn`, and `error` messages are logged.
- At `warn` level, `debug` and `info` messages are suppressed while `warn` and `error` messages are logged.
- At `error` level, `debug`, `info`, and `warn` messages are suppressed while `error` messages are logged.
- At `debug` level, all log messages (`debug`, `info`, `warn`, `error`) are logged.

---

### Non-Functional Requirements (REQ-NF)

#### REQ-NF-001: Low-latency journal dispatch

**Statement:** The system shall execute systemd journal log dispatch without blocking the main daemon loop for more than 1 millisecond per log call under normal operating conditions.

**Acceptance criteria:**
- `sd_journal_send` calls utilize stack-allocated string buffers without heap allocations in the logging hot path.
- Microbenchmarks confirm single-message log dispatch latency remains below 1.0 millisecond.

---

#### REQ-NF-002: ISO 8601 timestamp precision for stdout fallback

**Statement:** When writing log messages to `stdout`, the system shall format timestamps in ISO 8601 format with local time zone offset (`YYYY-MM-DDTHH:MM:SS%z`).

**Acceptance criteria:**
- Sample timestamp output: `2026-07-31T02:51:31+0300`.
- Timestamp resolution uses system clock time formatted via C++23 `<chrono>` or `std::format`.

---

#### REQ-NF-003: Thread-safe log dispatch

**Statement:** While multiple threads emit log messages concurrently, the system shall ensure thread-safe log dispatch without corruption of log messages or intermingled output lines.

**Acceptance criteria:**
- Concurrent calls to logger from separate worker threads produce coherent, complete log records.
- No interleaved characters or corrupted string buffers occur during multi-threaded stress tests.

---

### Constraint Requirements (REQ-C)

#### REQ-C-001: CMake build system pkg-config detection and fallback

**Statement:** Where `libsystemd` is detected via `pkg-config` during CMake configuration, the system shall compile with systemd journal support and link `libsystemd`; otherwise, the system shall compile with a standard output (`stdout`) logging sink fallback.

**Acceptance criteria:**
- `CMakeLists.txt` invokes `pkg_check_modules(LIBSYSTEMD libsystemd)`.
- If `libsystemd` is present, CMake defines `HOLONIGHTD_HAS_SYSTEMD` and links `PkgConfig::LIBSYSTEMD`.
- If `libsystemd` is absent, CMake configures the build without errors and defaults the logging sink to `stdout` (`std::cout`).
- Compiling on systems without systemd header files succeeds without build errors.

---

#### REQ-C-002: Zero GUI/Qt runtime dependencies

**Statement:** The system shall implement journal logging using C++23 standard library features and standard C `libsystemd` functions without introducing any Qt or GUI framework dependencies.

**Acceptance criteria:**
- `ldd` inspection of compiled `holonightd` binary reveals zero references to Qt, GLib, or X11/Wayland GUI libraries.
- Header includes under `include/holonightd/` rely exclusively on standard C++ headers and `<systemd/sd-journal.h>`.

---

## Log Level Precedence Matrix

| Precedence Tier | Source | Condition | Resulting Log Level | Resulting Sink |
| :--- | :--- | :--- | :--- | :--- |
| **1 (Highest)** | CLI Argument | `--debug` or `-d` flag present | `DEBUG` | `stdout` (`std::cout`) |
| **2** | Environment Var | `HOLONIGHTD_LOG_LEVEL` set | Value of Env Var | `journal` (or `stdout` fallback) |
| **3** | TOML Config | `log_level` in `[general]` | Value of TOML key | `journal` (or `stdout` fallback) |
| **4 (Lowest)** | Build Type Default | Debug / RelWithDebInfo | `DEBUG` | `journal` (or `stdout` fallback) |
| **4 (Lowest)** | Build Type Default | Release / MinSizeRel | `INFO` | `journal` (or `stdout` fallback) |

---

## Structured Metadata & Priority Mapping

| Log Severity | Syslog Priority Code (`PRIORITY`) | Journal Field Value | Stdout Display Tag |
| :--- | :--- | :--- | :--- |
| `DEBUG` | `7` (`LOG_DEBUG`) | `PRIORITY=7` | `[DEBUG]` |
| `INFO` | `6` (`LOG_INFO`) | `PRIORITY=6` | `[INFO]` |
| `WARN` / `WARNING` | `4` (`LOG_WARNING`) | `PRIORITY=4` | `[WARN]` |
| `ERROR` | `3` (`LOG_ERR`) | `PRIORITY=3` | `[ERROR]` |

---

## Acceptance & Falsifiability Summary

| Requirement ID | Verification Method | Falsifiable Failure Condition |
| :--- | :--- | :--- |
| **REQ-F-001** | `journalctl` query after daemon run | Journal lacks `SYSLOG_IDENTIFIER=holonightd` or messages missing |
| **REQ-F-002** | CLI execution with `--debug` | Logs appear in journal OR stdout timestamp format does not match ISO 8601 |
| **REQ-F-003** | Unit test matrix of Env/TOML/CLI combinations | Env var overrides `--debug` or TOML config overrides Env var |
| **REQ-F-004** | Unit test with mixed-case strings (`DeBuG`, `WARN`) | Parser throws exception for valid mixed-case string |
| **REQ-F-005** | Unit test with `HOLONIGHTD_LOG_LEVEL=invalid` | Daemon starts normally instead of throwing exception and exiting non-zero |
| **REQ-F-006** | Unit test asserting filtered messages | Debug message emitted when active level is set to `info` |
| **REQ-NF-001** | Microbenchmark timing `sd_journal_send` | Average dispatch latency exceeds 1.0 ms |
| **REQ-NF-002** | Regex match on stdout output | Timestamp format violates `YYYY-MM-DDTHH:MM:SS%z` |
| **REQ-NF-003** | Multi-threaded test with 10 concurrent threads | Interleaved log characters or race condition detected by ThreadSanitizer |
| **REQ-C-001** | CMake build without `libsystemd-dev` installed | CMake fails configuration or build fails compilation |
| **REQ-C-002** | Binary dependency audit (`ldd`) | Binary links against Qt or GUI libraries |
