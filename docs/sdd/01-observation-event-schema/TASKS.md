# SDD Tasks — 01-observation-event-schema

## Overview
This task breakdown implements the standardized Observation Event Schema and SQLite Event Persistence Layer for `holonightd` (Phase 1.1). Tasks are ordered by dependency: dependencies → header definitions → C++ implementation → CMake integration → unit tests → code format/tidy verification.

---

## Execution Tasks

- [x] T-001: Vendor nlohmann/json single-header library into third_party/nlohmann/json.hpp
  - REQs: REQ-F-002, REQ-C-001
  - Check: `third_party/nlohmann/json.hpp` exists, compiles under C++23

- [x] T-002: Wire SQLite3 dependency and include paths in CMake
  - REQs: REQ-C-002
  - Check: `CMakeLists.txt` links `SQLite3::SQLite3` to `holonightd_core`, `task configure` succeeds

- [x] T-003: Create ObservationEvent.h header
  - REQs: REQ-F-001, REQ-F-002, REQ-C-001
  - Check: `include/holonightd/ObservationEvent.h` contains `Severity` enum, `EventValue` variant, `ObservationEvent` struct, and `EventQuery` struct

- [x] T-004: Implement ObservationEvent serialization in ObservationEvent.cpp
  - REQs: REQ-F-001, REQ-F-002, REQ-NF-002
  - Check: `src/holonightd/ObservationEvent.cpp` implements `toJson()`, `fromJson()`, `severityToString()`, and `severityFromString()` using `nlohmann::json`

- [x] T-005: Create EventStore.h header
  - REQs: REQ-F-003, REQ-F-004, REQ-F-005, REQ-C-001
  - Check: `include/holonightd/EventStore.h` declares `EventStore` class with Pimpl pattern, RAII lifetime, `insert`, `insertBatch`, `query`, and `pruneEvents` methods returning `std::expected`

- [x] T-006: Implement EventStore SQLite persistence in EventStore.cpp
  - REQs: REQ-F-003, REQ-F-004, REQ-F-005, REQ-NF-001, REQ-NF-002
  - Check: `src/holonightd/EventStore.cpp` initializes SQLite database, sets WAL mode, creates tables/indexes, reuses prepared statements for single & batch inserts, queries events using `EventQuery`, and prunes old events

---

## Test Implementation Tasks

- [x] T-007: Write ObservationEvent unit tests in tests/test_observation_event.cpp
  - REQs: REQ-F-001, REQ-F-002, REQ-C-003
  - Check: Tests verify `ObservationEvent` creation, JSON serialization, JSON deserialization, invalid JSON error handling, and `Severity` string conversions

- [x] T-008: Write EventStore unit tests in tests/test_event_store.cpp
  - REQs: REQ-F-003, REQ-F-004, REQ-F-005, REQ-NF-001, REQ-NF-002, REQ-C-003
  - Check: Tests verify SQLite database creation in temporary directory, single event insert, batch insert, query filtering by time range / severity / source / limit, and event pruning

---

## Verification Tasks

- [x] T-009: Run full test suite, format check, and static analysis
  - REQs: REQ-C-003
  - Check: `task test` passes 100%, `task format-check` succeeds with zero diffs, and `task tidy-src` runs without warnings
