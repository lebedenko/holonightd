# Memory & Kernel Collector — EARS Specification

**Feature Slug:** `memory-kernel-collector`  
**Component:** `holonightd::MemoryCollector`  
**Project:** holonightd (Linux daemon, C++23)  
**Date:** 2026-08-01  
**Phase:** SDD Stage 1 (Requirements)

---

## 1. Overview

The `MemoryCollector` is a dedicated C++23 subsystem in `holonightd` responsible for lightweight, periodic monitoring of Linux memory pressure, memory usage, and kernel Out-Of-Memory (OOM) killer events.

It reads Pressure Stall Information (PSI) metrics from `/proc/pressure/memory`, falls back gracefully to `/proc/meminfo` when PSI is unsupported or unreadable, tracks OOM killer invocations via `/proc/vmstat`, and extracts victim process metadata from kernel logs. The results are normalized into `ObservationEvent` payloads emitted into the daemon's diagnostic pipeline.

---

## 2. Requirements

### Functional Requirements (REQ-F)

#### REQ-F-001: Memory Collector Configuration Parsing

**Statement:** The holonightd configuration loader shall parse memory collection settings from the `[memory]` section of the TOML configuration file into a `MemoryConfig` structure.

**Acceptance criteria:**
- Extends `Config` in `Application.h` / `Application.cpp` with `MemoryConfig` containing `some_warning_threshold` (double, default 10.0 %), `full_critical_threshold` (double, default 25.0 %), and `meminfo_warning_threshold` (double, default 85.0 %).
- Correctly parses `[memory]` section keys `some_warning_threshold`, `full_critical_threshold`, and `meminfo_warning_threshold` from TOML files.
- Assigns default values (10.0%, 25.0%, 85.0%) when the `[memory]` section or individual keys are omitted.

---

#### REQ-F-002: PSI Memory Pressure Metrics Collection

**Statement:** When memory pressure collection is executed, the memory collector shall read pressure statistics from `/proc/pressure/memory`.

**Acceptance criteria:**
- Parses `some` line (`avg10`, `avg60`, `avg300`, `total`) and `full` line (`avg10`, `avg60`, `avg300`, `total`) from `/proc/pressure/memory`.
- Extracts `avg10` floating-point percentage values for both `some` and `full` pressure metrics.

---

#### REQ-F-003: Some Memory Pressure Warning Event Generation

**Statement:** When the `some avg10` memory pressure value equals or exceeds the configured `some_warning_threshold`, the memory collector shall emit an `ObservationEvent` with signal `memory_pressure_some`.

**Acceptance criteria:**
- Sets event `source` to `"memory_collector"`, `category` to `"memory"`, and `subject` to `"psi"`.
- Sets event `signal` to `"memory_pressure_some"`.
- Sets event `value` to the calculated `some avg10` percentage as a `double`.
- Sets event `severity` to `Severity::Warning`.
- Populates event `attributes` JSON object with `some_avg10`, `some_avg60`, `some_avg300`, and `some_total`.

---

#### REQ-F-004: Full Memory Pressure Critical Event Generation

**Statement:** When the `full avg10` memory pressure value equals or exceeds the configured `full_critical_threshold`, the memory collector shall emit an `ObservationEvent` with signal `memory_pressure_full`.

**Acceptance criteria:**
- Sets event `source` to `"memory_collector"`, `category` to `"memory"`, and `subject` to `"psi"`.
- Sets event `signal` to `"memory_pressure_full"`.
- Sets event `value` to the calculated `full avg10` percentage as a `double`.
- Sets event `severity` to `Severity::Critical`.
- Populates event `attributes` JSON object with `full_avg10`, `full_avg60`, `full_avg300`, and `full_total`.

---

#### REQ-F-005: Meminfo Fallback Metric Calculation

**Statement:** If `/proc/pressure/memory` is missing or unreadable, then the memory collector shall parse `/proc/meminfo` to calculate system memory usage.

**Acceptance criteria:**
- Reads `MemTotal` and `MemAvailable` fields from `/proc/meminfo`.
- Calculates byte metrics `total_bytes` (`MemTotal * 1024`), `available_bytes` (`MemAvailable * 1024`), and `used_bytes` (`(MemTotal - MemAvailable) * 1024`).
- Calculates `percent_used` space as a `double` (`((MemTotal - MemAvailable) / MemTotal) * 100.0`).

---

#### REQ-F-006: High Memory Usage Fallback Event Generation

**Statement:** While operating in meminfo fallback mode, when calculated memory usage percentage equals or exceeds `meminfo_warning_threshold`, the memory collector shall emit an `ObservationEvent` with signal `memory_used_high`.

**Acceptance criteria:**
- Sets event `source` to `"memory_collector"`, `category` to `"memory"`, and `subject` to `"meminfo"`.
- Sets event `signal` to `"memory_used_high"`.
- Sets event `value` to the calculated memory `percent_used` as a `double`.
- Sets event `severity` to `Severity::Warning`.
- Populates event `attributes` JSON object with `total_bytes`, `available_bytes`, `used_bytes`, and `percent_used`.

---

#### REQ-F-007: OOM Killer Counter Baseline Initialization

**Statement:** When the memory collector runs its initial collection iteration, the memory collector shall record the baseline `oom_kill` counter value from `/proc/vmstat`.

**Acceptance criteria:**
- Reads line matching `oom_kill` from `/proc/vmstat`.
- Stores the integer value as baseline without emitting an OOM event on the first run.

---

#### REQ-F-008: OOM Killer Detection and Event Generation

**Statement:** When the `oom_kill` counter in `/proc/vmstat` increases between iterations, the memory collector shall emit an `ObservationEvent` with signal `oom_killer_invoked`.

**Acceptance criteria:**
- Detects counter increment (`current_oom_kill > baseline_oom_kill`).
- Updates stored baseline counter to `current_oom_kill`.
- Sets event `source` to `"memory_collector"`, `category` to `"memory"`, and `subject` to `"oom_killer"`.
- Sets event `signal` to `"oom_killer_invoked"`.
- Sets event `value` to the count increment (delta) as a `double` or integer equivalent.
- Sets event `severity` to `Severity::Error`.

---

#### REQ-F-009: OOM Victim Context Extraction from Kernel Log

**Statement:** Where kernel log access is available, when an `oom_killer_invoked` event is triggered, the memory collector shall extract victim process name and PID metadata from recent kernel log buffer output.

**Acceptance criteria:**
- Scans recent kernel ring buffer or kernel log slice (e.g. `dmesg` or `/dev/kmsg` non-blocking snapshot) for recent "Out of memory: Kill process" pattern.
- If victim PID and process name are matched, populates `victim_pid` and `victim_name` in the event `attributes`.
- If kernel log parsing yields no match or is unreadable, populates `victim_pid` as `0` / `null` and `victim_name` as `"unknown"` without blocking or failing event emission.

---

### Non-Functional Requirements (REQ-NF)

#### REQ-NF-001: Graceful Error Handling and Failure Isolation

**Statement:** If reading `/proc/pressure/memory`, `/proc/meminfo`, `/proc/vmstat`, or kernel logs fails, then the memory collector shall log a diagnostic warning and return available partial metrics without terminating the daemon.

**Acceptance criteria:**
- Catches I/O and parsing errors gracefully.
- Emits diagnostic warnings using `Logger`.
- Daemon process remains fully running and stable without unhandled exceptions or crashes.

---

#### REQ-NF-002: Low Overhead Non-Blocking Execution

**Statement:** While executing metric collection, the memory collector shall complete file reads and parsing synchronously in under 10 milliseconds without establishing continuous blocking stream loops on `/dev/kmsg`.

**Acceptance criteria:**
- Performs non-blocking, snapshot-style file reads for `/proc` pseudo-files and kernel log buffers.
- Synchronous collection cycle finishes in under 10ms.

---

#### REQ-NF-003: High Test Coverage

**Statement:** The memory collector subsystem shall maintain 100% statement and branch test coverage across all new methods.

**Acceptance criteria:**
- Unit tests cover PSI parsing, meminfo fallback, OOM counter tracking, victim context extraction, and threshold configuration.
- `task coverage` demonstrates 100% statement/branch coverage for `MemoryCollector.cpp`.

---

### Constraints (REQ-C)

#### REQ-C-001: Modern C++23 Implementation

**Statement:** The memory collector shall be implemented strictly in C++23 as `holonightd::MemoryCollector` with zero external non-header dependencies.

**Acceptance criteria:**
- Header located at `include/holonightd/MemoryCollector.h` and implementation at `src/holonightd/MemoryCollector.cpp`.
- Configuration extended in `include/holonightd/Application.h` and `src/holonightd/Application.cpp`.
- Targets C++23 standard (`-std=c++23`) using standard library idioms (`std::filesystem`, `std::optional`, `std::expected`, RAII).

---

#### REQ-C-002: Scope Limits and Non-Goals

**Statement:** The memory collector shall perform diagnostic observation only and shall not execute process killing, cgroup modification, or continuous blocking stream loops on `/dev/kmsg`.

**Acceptance criteria:**
- No process killing (`kill()`), cgroup parameter mutation, or background streaming threads are implemented.

---

#### REQ-C-003: Code Quality and Linting Compliance

**Statement:** The memory collector implementation shall pass all static analysis checks in task tidy-src, code formatting in task format-check, and unit tests in task test.

**Acceptance criteria:**
- `task format-check` reports no formatting violations.
- `task tidy-src` completes with zero errors or warnings.
- `task test` passes all unit tests in `tests/test_memory_collector.cpp`.
