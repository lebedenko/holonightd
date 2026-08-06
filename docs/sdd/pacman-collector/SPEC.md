# Pacman Collector — EARS Specification

**Feature Slug:** `pacman-collector`  
**Component:** `holonightd::PacmanCollector`  
**Project:** holonightd (Linux daemon, C++23)  
**Date:** 2026-08-01  
**Phase:** SDD Stage 1 (Requirements)

---

## 1. Overview

The `PacmanCollector` is a dedicated C++23 subsystem in `holonightd` responsible for monitoring package management state, kernel-module consistency, and orphan configuration artifacts on Arch Linux and Arch-derived systems.

Package management issues—such as running a system on an updated kernel whose corresponding modules were deleted from `/usr/lib/modules/`, stale database locks (`db.lck`) preventing automated maintenance, accumulating `.pacnew`/`.pacsave` files, or unconfigured interrupted pacman transactions—can cause critical system failures, service degradation, or reboot instability.

`PacmanCollector` inspects pacman state files, `/proc`, and `/etc` non-invasively through standard C++ `<filesystem>` calls without spawning `pacman` binaries or executing sub-shell processes. It normalizes diagnostic observations into `ObservationEvent` payloads emitted to the daemon's central diagnostic pipeline.

---

## 2. Requirements

### Functional Requirements (REQ-F)

#### REQ-F-001: Root Path Configuration and Abstraction (`sys_root`)

**Statement:** Where a custom `sys_root` path is specified in configuration, the pacman collector shall evaluate all filesystem and `/proc` targets relative to `sys_root`.

**Acceptance criteria:**
- Default `sys_root` path is set to `"/"`.
- All internal path lookups (including `/proc`, `/var/lib/pacman`, `/usr/lib/modules`, and `/etc`) prepend `sys_root`.
- Supports full mock filesystem trees during unit testing without modifying host system files.

---

#### REQ-F-002: Kernel Version Mismatch Detection

**Statement:** When kernel version evaluation is executed, the pacman collector shall compare the running kernel release against installed kernel modules and pacman local database package records.

**Acceptance criteria:**
- Obtains the running kernel release version string from `uname -r` or `<sys_root>/proc/version`.
- Checks for matching directory presence under `<sys_root>/usr/lib/modules/<kernel_release>`.
- Inspects installed kernel package entries in `<sys_root>/var/lib/pacman/local/linux-*`.
- Flags a kernel version mismatch if the running kernel version directory is missing from installed modules or installed pacman package records.

---

#### REQ-F-003: Kernel Mismatch Event Generation

**Statement:** When a kernel version mismatch is detected, the pacman collector shall emit an ObservationEvent with signal pacman.kernel_mismatch.

**Acceptance criteria:**
- Sets event `source` to `"pacman_collector"`, `category` to `"package"`, and `subject` to `"kernel"`.
- Sets event `signal` to `"pacman.kernel_mismatch"`.
- Sets event `severity` to `Severity::Warning`.
- Populates event `attributes` JSON object with `running_kernel` version string, `installed_modules_dirs` list, and `installed_kernel_packages` list.

---

#### REQ-F-004: Database Lock State Inspection (`db.lck`)

**Statement:** When database lock state evaluation is executed, the pacman collector shall inspect `<sys_root>/var/lib/pacman/db.lck` for lock file presence and stored process ID.

**Acceptance criteria:**
- Checks for existence of lock file at `<sys_root>/var/lib/pacman/db.lck`.
- Reads file content to parse the stored integer Process ID (PID) if non-empty.
- Correctly handles empty, unreadable, or missing lock files.

---

#### REQ-F-005: Active Lock Event Generation

**Statement:** When db.lck exists and the stored PID corresponds to an active process in /proc, the pacman collector shall emit an ObservationEvent with signal pacman.active_lock.

**Acceptance criteria:**
- Verifies process existence via checking directory `<sys_root>/proc/<pid>` or `/proc/<pid>`.
- Sets event `source` to `"pacman_collector"`, `category` to `"package"`, and `subject` to `"pacman_db"`.
- Sets event `signal` to `"pacman.active_lock"`.
- Sets event `severity` to `Severity::Info`.
- Populates event `attributes` JSON object with `pid`, process command name if available, and lock file modification time.

---

#### REQ-F-006: Stale Lock Event Generation

**Statement:** When db.lck exists and the stored PID does not correspond to an active process in /proc, the pacman collector shall emit an ObservationEvent with signal pacman.stale_lock.

**Acceptance criteria:**
- Identifies dead PID (process directory missing in `/proc`) or malformed PID content in lock file.
- Sets event `source` to `"pacman_collector"`, `category` to `"package"`, and `subject` to `"pacman_db"`.
- Sets event `signal` to `"pacman.stale_lock"`.
- Sets event `severity` to `Severity::Warning`.
- Populates event `attributes` JSON object with `pid`, lock file age/modification timestamp, and reason (e.g. `"process_dead"` or `"invalid_pid"`).

---

#### REQ-F-007: Orphan Configuration File Scanning (`.pacnew` and `.pacsave`)

**Statement:** When configuration file scanning is executed, the pacman collector shall scan `<sys_root>/etc` for .pacnew and .pacsave files up to the configured maximum search depth.

**Acceptance criteria:**
- Recursively inspects `<sys_root>/etc` respecting `max_depth` configuration (default limit: 3 directory levels).
- Identifies files matching filenames ending with `.pacnew` or `.pacsave`.
- Aggregates file paths and total counts categorized by extension.

---

#### REQ-F-008: Orphan Config File Event Generation

**Statement:** When one or more .pacnew or .pacsave files are discovered in /etc, the pacman collector shall emit an ObservationEvent with signal pacman.pacnew_files.

**Acceptance criteria:**
- Sets event `source` to `"pacman_collector"`, `category` to `"package"`, and `subject` to `"config_files"`.
- Sets event `signal` to `"pacman.pacnew_files"`.
- Sets event `value` to the total count of orphan configuration files (`int64_t`).
- Sets event `severity` to `Severity::Info` if total count is less than `warning_threshold` (default: 5), or `Severity::Warning` if total count equals or exceeds `warning_threshold`.
- Populates event `attributes` JSON object with `pacnew_count`, `pacsave_count`, and array of relative or absolute file paths.

---

#### REQ-F-009: Interrupted Transaction State Detection

**Statement:** When package state collection is executed, the pacman collector shall inspect `<sys_root>/var/lib/pacman/` for leftover transaction files or unconfigured transaction states.

**Acceptance criteria:**
- Checks for existence of transaction artifacts such as `<sys_root>/var/lib/pacman/db.lck` alongside uncommitted package transactions or temporary directory markers.
- Emits an `ObservationEvent` with `signal` set to `"pacman.interrupted_transaction"` and `severity` set to `Severity::Warning` when interrupted transaction artifacts are detected.
- Includes transaction state details and detected lock files in event `attributes`.

---

### Non-Functional Requirements (REQ-NF)

#### REQ-NF-001: Direct Filesystem and Proc Inspection (No Subprocess Execution)

**Statement:** The pacman collector shall inspect filesystem targets and /proc directly without spawning pacman binaries, sub-processes, or shell commands.

**Acceptance criteria:**
- Does not call `system()`, `popen()`, `fork()`, or `execve()`.
- Reads and parses files using standard C++ `<filesystem>` and standard stream libraries (`std::ifstream`).
- Eliminates execution latency, shell injection risk, sub-process hang hazards, and requirement for elevated pacman execution rights.

---

#### REQ-NF-002: Exception Safety and Non-Throwing Collect Interface

**Statement:** The pacman collector collect method shall be declared nodiscard and shall never throw exceptions to the caller.

**Acceptance criteria:**
- Method declaration: `[[nodiscard]] std::vector<ObservationEvent> collect() noexcept` or returns `std::expected<std::vector<ObservationEvent>, PacmanCollectorError>`.
- Internal filesystem calls use non-throwing standard library overloads accepting `std::error_code`.
- Gracefully handles permission denied errors (`EACCES`), missing directories (`ENOENT`), or malformed content without throwing unhandled exceptions or crashing the daemon.

---

#### REQ-NF-003: Non-Arch Linux & Missing Path Handling

**Statement:** If pacman database directories or /etc do not exist or are inaccessible, then the pacman collector shall return an empty event list without emitting spurious error events.

**Acceptance criteria:**
- Gracefully handles execution on non-Arch Linux distributions (e.g., Debian, Ubuntu, Fedora, Alpine) or minimal container environments where `/var/lib/pacman` does not exist.
- Emits a debug log message when pacman database path is not present, without treating missing pacman paths as daemon errors.

---

#### REQ-NF-004: Performance & Bounded Directory Traversal

**Statement:** While scanning /etc for orphan configuration files, the pacman collector shall complete directory traversal within 50 milliseconds by enforcing depth limits and skipping circular symlinks.

**Acceptance criteria:**
- Directory traversal adheres to configured `max_depth` (default: 3).
- Configured with `std::filesystem::directory_options::skip_permission_denied`.
- Prevents infinite recursion or stack exhaustion on cyclic directory symlinks.

---

### Constraints (REQ-C)

#### REQ-C-001: Modern C++23 Standard Implementation

**Statement:** The pacman collector shall be implemented in C++23 as a distinct class holonightd::PacmanCollector in include/holonightd/PacmanCollector.h and src/holonightd/PacmanCollector.cpp.

**Acceptance criteria:**
- Header file located at `include/holonightd/PacmanCollector.h` using `#pragma once`.
- Source file located at `src/holonightd/PacmanCollector.cpp`.
- Targets C++23 standard (`-std=c++23`).
- Uses standard library RAII and zero raw owning pointers.

---

#### REQ-C-002: Scope Limits and Non-Goals

**Statement:** The pacman collector shall perform diagnostic observation only and shall not modify pacman databases, remove .pacnew/.pacsave files, or install/remove package binaries.

**Acceptance criteria:**
- Performs read-only inspections exclusively.
- Does not delete, modify, or rename any files in `/etc` or `/var/lib/pacman`.
- Does not invoke pacman package management actions.

---

#### REQ-C-003: Code Quality and Linting Compliance

**Statement:** The pacman collector implementation shall pass all static analysis checks in task tidy-src, code formatting in task format-check, and unit tests in task test.

**Acceptance criteria:**
- Zero warnings or errors returned by `task format-check`.
- Zero warnings or errors returned by `task tidy-src`.
- Unit tests in `tests/test_pacman_collector.cpp` pass cleanly under `task test`.
