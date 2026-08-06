# Observation Event Schema & Persistence Layer — EARS Specification

**Feature ID:** `01-observation-event-schema`  
**Project:** holonightd (Linux daemon, C++23)  
**Date:** 2026-07-30  
**Phase:** 1.1 Core Foundation  

---

## Overview

This specification defines the standardized Observation Event data structure and SQLite persistence layer for `holonightd`. 

As outlined in the HoloNight design principles, `holonightd` is a deterministic diagnostic system. All system monitoring collectors emit normalized, JSON-compatible `ObservationEvent` objects. These events are saved to an embedded SQLite database (`EventStore`), enabling time-window correlation, historical query analysis, and dataset generation for diagnostic rules and future ML models.

---

## Requirements

### Functional Requirements (REQ-F)

#### REQ-F-001: Normalized Observation Event Structure

**Statement:** The `holonightd` system shall represent system observation events using a standardized C++23 `ObservationEvent` structure containing timestamp, source, category, subject, signal, value, severity, and attributes.

**Acceptance criteria:**
- `timestamp` represents UTC time with microsecond or millisecond precision (stored in ISO-8601 format e.g., `2026-07-30T22:31:14.123Z` or Unix epoch microseconds).
- `source` identifies the system collector or subsystem (e.g., `"systemd"`, `"journal"`, `"smart"`, `"kernel"`, `"pacman"`, `"statvfs"`).
- `category` identifies the functional domain (e.g., `"service"`, `"storage"`, `"memory"`, `"package"`, `"graphics"`, `"audio"`).
- `subject` identifies the entity being observed (e.g., `"bluetooth.service"`, `"/dev/nvme0n1"`, `"system"`).
- `signal` identifies the specific event type or metric name (e.g., `"unit_failed"`, `"oom_kill"`, `"media_errors"`, `"space_pressure"`).
- `value` represents optional quantitative or state payload (`std::variant<std::monostate, bool, int64_t, double, std::string>`).
- `severity` is an enum with values `Debug`, `Info`, `Warning`, `Error`, `Critical`.
- `attributes` contains key-value metadata pairs stored as a JSON object (or string map).

---

#### REQ-F-002: JSON Serialization & Deserialization

**Statement:** When an `ObservationEvent` is converted to or from JSON format, the system shall produce and parse a schema-compliant JSON payload matching the HoloNight observation event schema.

**Acceptance criteria:**
- Serialized JSON matches the canonical schema keys: `timestamp`, `source`, `category`, `subject`, `signal`, `value`, `severity`, and `attributes`.
- Deserialization parses valid JSON strings back into an identical `ObservationEvent` C++ struct.
- Enum severity values serialize to lowercase string representations (`"debug"`, `"info"`, `"warning"`, `"error"`, `"critical"`).

---

#### REQ-F-003: SQLite Event Persistence Layer

**Statement:** When an `ObservationEvent` is recorded, the system shall insert the event into an embedded SQLite database table (`observation_events`).

**Acceptance criteria:**
- SQLite database initializes automatically with appropriate schema and indices if the database file or tables do not exist.
- Primary key `id` is auto-incremented.
- Indexed columns include `timestamp`, `source`, `category`, and `severity` for efficient time-series and filtered queries.
- Transactions are used for batch inserts to maximize write performance.

---

#### REQ-F-004: Event Query & Filtering Interface

**Statement:** The system shall provide an `EventQuery` filtering interface in `EventStore` allowing queries by time range, source, category, severity threshold, subject, and result limit.

**Acceptance criteria:**
- Queries filtering by `start_time` and `end_time` return only events within the specified time window.
- Queries filtering by minimum `severity` return events of that severity or higher.
- Queries filtering by `source`, `category`, or `subject` match exactly when specified.
- Queries support a `limit` parameter returning events in reverse chronological order (newest first).

---

#### REQ-F-005: Event Pruning and Retention Policy

**Statement:** When requested or during scheduled maintenance, the system shall prune observation events older than a configurable retention threshold from the database.

**Acceptance criteria:**
- `pruneEvents(older_than_timestamp)` deletes all rows in `observation_events` where `timestamp < older_than_timestamp`.
- Function returns the count of deleted rows.
- Executing `pruneEvents` does not corrupt database integrity or lock active queries indefinitely.

---

### Non-Functional Requirements (REQ-NF)

#### REQ-NF-001: Low Overhead & Performance

**Statement:** The SQLite persistence layer shall execute single event insertions in under 5 milliseconds and batch inserts of 100 events in under 20 milliseconds on standard Linux storage.

**Acceptance criteria:**
- WAL (Write-Ahead Logging) mode is enabled on the SQLite connection.
- Prepared statements are reused for repeated insertions to avoid SQL parsing overhead.

---

#### REQ-NF-002: Safety & Exception Handling

**Statement:** While interacting with SQLite or JSON parsing, the system shall return structured `std::expected<T, std::string>` or throw explicit `std::runtime_error` exceptions with diagnostic context on database errors.

**Acceptance criteria:**
- Corrupted database files or disk full conditions emit clear error messages containing the database path and SQLite error code/string.
- Database connections are closed cleanly via RAII on object destruction.

---

### Constraints (REQ-C)

#### REQ-C-001: Modern C++23 Standards

**Statement:** The system shall strictly conform to C++23 (`-std=c++23`) using standard library RAII and zero raw owning pointers.

---

#### REQ-C-002: SQLite3 Dependency

**Statement:** The persistence layer shall use system `libsqlite3` via C CMake target linking (`sqlite3`).

---

#### REQ-C-003: Code Quality and Linting Compliance

**Statement:** All newly introduced code shall pass `task format-check`, `task tidy-src`, and all unit tests in `task test`.
