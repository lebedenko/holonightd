# EventStore Storage Management, Retention Policies & Diagnostic JSONL Export — EARS Specification

**Feature ID:** `eventstore-retention-export`  
**Project:** holonightd (Linux daemon, C++23)  
**Date:** 2026-08-01  
**Phase:** Storage Management & Diagnostic Utilities  

---

## Overview

This specification details storage capacity management, age/capacity-based event retention policies, WAL checkpointing, and atomic JSONL diagnostic export capabilities for `EventStore` in `holonightd`.

As `holonightd` runs continuously in background system maintenance roles, its SQLite database can grow indefinitely without automated storage management. This feature equips `EventStore` with retention pruning routines (by age and by capacity), configurable storage caps, WAL log truncation checkpointing, and a high-performance streaming export utility for diagnostic JSONL generation.

---

## Non-Goals

- **In-daemon archive compression**: Direct generation of compressed archives (`.gz`, `.tar.gz`) within the daemon process is excluded.
- **Remote DB synchronization**: Network synchronization or streaming of events to external endpoints is out of scope.

---

## Requirements

### Functional Requirements (REQ-F)

#### REQ-F-001: Database Configuration Expansion for Retention Limits

**Statement:** Where database configuration is parsed from configuration sources, the `DatabaseConfig` structure in `Application.h` shall include optional `max_bytes` and `max_events` retention parameters with default values of 52,428,800 bytes (50 MB) and 100,000 events respectively.

**Acceptance criteria:**
- `DatabaseConfig` struct contains `std::optional<size_t> max_bytes` initialized to default `52428800` (50 MB).
- `DatabaseConfig` struct contains `std::optional<size_t> max_events` initialized to default `100000`.
- TOML configuration parser populates `max_bytes` and `max_events` when specified under `[database]` block.
- Omitting `max_bytes` or `max_events` in TOML config retains default values.

---

#### REQ-F-002: Age-Based Event Pruning

**Statement:** When `pruneEventsByAge(older_than)` is invoked on `EventStore`, the system shall delete all stored observation events whose timestamp is strictly earlier than the specified cutoff `time_point`.

**Acceptance criteria:**
- `pruneEventsByAge` accepts a `std::chrono::system_clock::time_point older_than` parameter.
- All database rows in `observation_events` with `timestamp < older_than` are deleted.
- Returns `std::expected<size_t, std::string>` containing the number of deleted event rows on success.
- If database query fails, returns a `std::unexpected` string error describing the failure.

---

#### REQ-F-003: Capacity-Based Event Pruning with High Watermark & Severity Preservation

**Statement:** When `pruneEventsByCapacity(max_bytes, max_events)` is invoked on `EventStore`, the system shall check database size and row count against the specified limits and, if exceeded, prune events down to approximately 90% of max limits while preserving higher severity events prior to lower severity events.

**Acceptance criteria:**
- If current event count exceeds `max_events`, events are pruned until count is $\le \lfloor 0.90 \times \text{max\_events} \rfloor$.
- If current database byte size (or storage footprint) exceeds `max_bytes`, events are pruned until size is $\le \lfloor 0.90 \times \text{max\_bytes} \rfloor$.
- Pruning ordering prioritizes deletion by severity rank: `Debug` (0) and `Info` (1) events are deleted before `Warning` (2), `Error` (3), or `Critical` (4) events.
- Within the same severity tier, older events (earlier timestamp) are deleted before newer events.
- If neither `max_bytes` nor `max_events` threshold is exceeded, zero rows are deleted and method succeeds immediately.
- Returns `std::expected<size_t, std::string>` with total deleted count across all iterations.

---

#### REQ-F-004: SQLite WAL Log Checkpointing

**Statement:** When `checkpointWal()` is called on `EventStore`, the system shall execute `PRAGMA wal_checkpoint(TRUNCATE)` on the active SQLite handle to flush write-ahead log pages back to the database file and truncate the WAL file.

**Acceptance criteria:**
- `checkpointWal()` executes `PRAGMA wal_checkpoint(TRUNCATE)` against the SQLite database connection.
- Returns `std::expected<void, std::string>` representing success or SQLite execution error message.
- After successful execution, the WAL file size on disk is truncated to 0 bytes (or minimal header size).

---

#### REQ-F-005: Atomic JSONL Diagnostic Export

**Statement:** When `exportToJsonl(dest_path, filter)` is called on `EventStore`, the system shall query events matching the filter and export them line-by-line in JSON Lines format to a temporary file (`dest_path.tmp`) before atomically renaming it to `dest_path`.

**Acceptance criteria:**
- Matches events using the provided `EventQuery` filter criteria (time range, source, category, subject, severity).
- Formats each event using `ObservationEvent::toJson()` printed as a single line separated by `\n`.
- Creates and streams output to `dest_path.string() + ".tmp"` during write operation.
- Replaces existing `dest_path` atomically using `std::filesystem::rename` upon successful stream flushing and closure.
- Returns `std::expected<size_t, std::string>` containing the total count of exported events on success.

---

### Non-Functional Requirements (REQ-NF)

#### REQ-NF-001: Atomic Export Failure Resilience

**Statement:** If an error occurs during file writing or database iteration during `exportToJsonl`, then the system shall close and remove the incomplete `.tmp` file and preserve any pre-existing file at `dest_path`.

**Acceptance criteria:**
- Disk write errors or SQL query failures clean up `dest_path.tmp`.
- If `dest_path` already exists prior to failed export, original `dest_path` file remains untouched.
- Returns `std::unexpected` containing details of the failure.

---

#### REQ-NF-002: Pruning Hysteresis Loop Prevention

**Statement:** The capacity pruning mechanism shall target 90% of specified limits to prevent triggering expensive database deletion queries on every single subsequent event insertion.

**Acceptance criteria:**
- Target limit after pruning is calculated as `target = limit * 9 / 10`.
- Subsequent insertions following a capacity prune do not re-trigger pruning until limits (100%) are breached again.

---

#### REQ-NF-003: High Test Coverage & Determinism

**Statement:** All newly added methods (`pruneEventsByAge`, `pruneEventsByCapacity`, `checkpointWal`, `exportToJsonl`, and `DatabaseConfig` parsing) shall have unit test coverage verifying functional paths, edge cases, and error conditions.

**Acceptance criteria:**
- 100% line coverage for new methods in `EventStore` and `Application`.
- GTest test suite includes test cases verifying age pruning, severity-ordered capacity pruning, WAL checkpointing, and atomic JSONL export output.

---

### Constraints (REQ-C)

#### REQ-C-001: Modern C++23 Standards

**Statement:** The system shall strictly conform to C++23 (`-std=c++23`) using standard library features (`std::filesystem`, `std::expected`, `std::optional`, `std::chrono`) and zero raw owning pointers.

---

#### REQ-C-002: Direct SQLite & nlohmann/json Dependencies

**Statement:** The system shall utilize existing `sqlite3` and `nlohmann/json` libraries already integrated into `holonightd`.

---

#### REQ-C-003: Code Quality, Linting & Build Verification

**Statement:** All modified and new C++ headers and implementation files shall pass `task format-check`, `task tidy-src`, and all test cases in `task test`.
