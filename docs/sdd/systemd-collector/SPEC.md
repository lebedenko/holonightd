# Systemd Collector — EARS Specification

**Feature ID:** `systemd-collector`  
**Project:** holonightd (Linux daemon, C++23)  
**Date:** 2026-08-01  
**Phase:** 1.2 Collector Modules  

---

## Overview

The `SystemdCollector` module for `holonightd` monitors systemd service health, unit flapping (rapid restart cycles), and coredump events on Linux systems via system D-Bus (`sd-bus` / `org.freedesktop.systemd1`).

As part of the `holonightd` diagnostic daemon architecture, `SystemdCollector` emits normalized `ObservationEvent` objects to document system degradation and service failures without requiring external dependencies or GUI components. It operates resiliently in diverse Linux environments, including containers, chroots, and non-systemd init systems, by safely falling back to an operational state without throwing uncaught exceptions or terminating the daemon process.

---

## Requirements

### Functional Requirements (REQ-F)

#### REQ-F-001: Configuration Parsing for Systemd Collector

**Statement:** Where systemd collector configuration is specified in `holonightd.toml`, the system shall parse the `[systemd]` configuration table into a structured configuration.

**Acceptance criteria:**
- `flapping_threshold` is parsed as a positive integer (default: `3`).
- `flapping_window_seconds` is parsed as a positive integer (default: `300`).
- `ignore_units` is parsed as a string vector (default: `[]`).
- Unspecified keys revert to their defined default values without error.

---

#### REQ-F-002: Failed Unit Detection via System D-Bus

**Statement:** When a systemd collection scan is executed, the system shall query system D-Bus (`org.freedesktop.systemd1`, `ListUnits`) for all units in a `failed` active state.

**Acceptance criteria:**
- Queries `org.freedesktop.systemd1.Manager` via `ListUnits` method over system D-Bus.
- Correctly identifies systemd units whose `ActiveState` is `"failed"`.
- Filters out non-failed units from the failed unit list.

---

#### REQ-F-003: Failed Unit Event Generation

**Statement:** When a systemd unit is detected in a `failed` active state, the system shall generate an `ObservationEvent` with `source = "systemd"`, `category = "systemd.unit"`, `subject = unit_name`, `severity = Severity::Error`, and `signal = "unit_failed"`.

**Acceptance criteria:**
- `ObservationEvent` attributes contain unit load state, substate, and failure details.
- Event `timestamp` records current UTC time.
- Subject matches the full systemd unit name (e.g., `"nginx.service"`).

---

#### REQ-F-004: Rapid Restart and Flapping Unit Monitoring

**Statement:** When a systemd unit restarts at least `flapping_threshold` times within `flapping_window_seconds`, the system shall generate an `ObservationEvent` with `source = "systemd"`, `category = "systemd.unit"`, `subject = unit_name`, `severity = Severity::Warning`, and `signal = "unit_flapping"`.

**Acceptance criteria:**
- Tracks unit state transitions and restart timestamps within the moving time window `flapping_window_seconds`.
- Triggers event generation when restart count equals or exceeds `flapping_threshold`.
- Event attributes include total restart count within the sliding window.

---

#### REQ-F-005: Coredump Event Detection

**Statement:** When a coredump event or `systemd-coredump` journal entry is detected, the system shall generate an `ObservationEvent` with `source = "systemd"`, `category = "systemd.coredump"`, `subject = executable_or_unit_name`, `severity = Severity::Error`, and `signal = "coredump"`.

**Acceptance criteria:**
- Detects coredump logs produced by `systemd-coredump`.
- Sets `subject` to the crashing binary name or unit name (e.g., `"my_app"` or `"my_app.service"`).
- Attributes contain crash PID, signal number, and executable path if available.

---

#### REQ-F-006: Ignored Units Filtering

**Statement:** Where a unit name matches any entry in `ignore_units`, the system shall skip observation event generation for that unit.

**Acceptance criteria:**
- Exact string matching ignores specified units in `ignore_units` (e.g., `["test.service"]`).
- Ignored units do not produce `unit_failed`, `unit_flapping`, or `coredump` events.
- Non-matching units continue to be processed and reported normally.

---

#### REQ-F-007: Fallback for System D-Bus or Systemd Absence

**Statement:** If system D-Bus connection fails or systemd is absent, then the system shall log a warning message and return an empty list of observation events without throwing uncaught exceptions.

**Acceptance criteria:**
- Environments without D-Bus socket or systemd PID 1 (e.g., Docker containers, chroot) log a warning explaining systemd unavailability.
- Collection returns an empty `std::vector<ObservationEvent>`.
- The daemon does not terminate, crash, or enter an illegal state.

---

#### REQ-F-008: Invalid Configuration Recovery

**Statement:** If `flapping_threshold` or `flapping_window_seconds` in configuration is zero or negative, then the system shall fall back to default configuration values and log a warning message.

**Acceptance criteria:**
- Setting `flapping_threshold <= 0` logs a warning and uses default value `3`.
- Setting `flapping_window_seconds <= 0` logs a warning and uses default value `300`.
- Application startup or configuration parsing succeeds despite invalid inputs.

---

### Non-Functional Requirements (REQ-NF)

#### REQ-NF-001: Collection Scan Latency

**Statement:** While executing a systemd collection scan, the system shall complete processing within 500 milliseconds under standard operating conditions.

**Acceptance criteria:**
- Total scan time (D-Bus query + flapping analysis + event assembly) stays under 500ms for up to 1,000 active systemd units.
- Non-blocking or minimal D-Bus call timeouts are enforced to prevent daemon event loop stalls.

---

#### REQ-NF-002: Memory and Resource Management

**Statement:** The system shall manage all D-Bus connections and message structures using standard C++ RAII wrappers without leaking memory or file descriptors.

**Acceptance criteria:**
- D-Bus connection handles and iterator contexts are released upon scope exit.
- `Valgrind` or `AddressSanitizer` (ASan) reports zero memory leaks during repeated scans.

---

### Constraints (REQ-C)

#### REQ-C-001: C++23 Language Standard

**Statement:** The system shall strictly conform to standard C++23 (`-std=c++23`) using standard library features and zero raw owning pointers.

**Acceptance criteria:**
- Code compiles cleanly with `-std=c++23` enabled in CMake.
- Smart pointers (`std::unique_ptr`, `std::shared_ptr`) or value semantics manage all heap memory allocations.

---

#### REQ-C-002: System D-Bus Interface Standard

**Statement:** The system shall query systemd via system D-Bus (`sd-bus` or `org.freedesktop.systemd1`).

**Acceptance criteria:**
- Uses D-Bus system bus path `/org/freedesktop/systemd1` and interface `org.freedesktop.systemd1.Manager`.
- No shell calls to `systemctl` or external binaries are required for unit querying.

---

#### REQ-C-003: Code Quality and Static Analysis Compliance

**Statement:** All newly introduced code shall pass `task format-check`, `task tidy-src`, and all unit tests in `task test`.

**Acceptance criteria:**
- `task format-check` passes without formatting violations.
- `task tidy-src` passes with zero warnings or errors.
- `task test` passes 100% of unit tests.
