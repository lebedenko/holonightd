# Daemon EventStore Integration — EARS Specification

**Feature ID:** `daemon-eventstore-integration`  
**Project:** holonightd (Linux daemon, C++23)  
**Date:** 2026-07-31  
**Phase:** 1.2 Core Daemon Integration  

---

## Overview

This specification defines the integration of the embedded `EventStore` persistence layer into the `holonightd` core daemon lifecycle. 

`holonightd` runs periodic diagnostic iterations collecting `ObservationEvent` instances (such as storage state from `StorageCollector`). To support historical query analysis, diagnostic rules, and automated maintenance, the daemon must initialize an SQLite `EventStore` based on TOML configuration or XDG environment standards, batch insert observation events on every iteration in `Daemon::runIteration()`, prune expired events according to a retention schedule, and handle runtime database errors robustly without interrupting system monitoring.

---

## Requirements

### Functional Requirements (REQ-F)

#### REQ-F-001: Database TOML Configuration Parsing

**Statement:** Where a `[database]` section is present in `holonightd.toml`, the system shall parse optional `path` (filesystem path string) and optional `retention_days` (integer) settings into the application configuration structure.

**Acceptance criteria:**
- Configuration parser reads `[database]` section from TOML configuration files.
- Field `[database] path` maps to `DatabaseConfig::path` (`std::optional<std::string>`).
- Field `[database] retention_days` maps to `DatabaseConfig::retention_days` (`std::optional<int>`).
- Parsing succeeds when `[database]` section or individual fields are omitted.

---

#### REQ-F-002: Default Database Path Resolution

**Statement:** The `holonightd` system shall resolve the target SQLite database path according to configuration hierarchy and XDG Base Directory specification standards.

**Acceptance criteria:**
- Where `[database] path` is explicitly configured in TOML, the system uses the specified filesystem path.
- Where `[database] path` is unconfigured and environment variable `$XDG_DATA_HOME` is set and non-empty, the system resolves the database path to `$XDG_DATA_HOME/holonight/events.db`.
- Where `[database] path` is unconfigured and `$XDG_DATA_HOME` is unset or empty, the system resolves the database path to `~/.local/share/holonight/events.db` (expanding `~` to the user's home directory).

---

#### REQ-F-003: Retention Period Resolution

**Statement:** The system shall determine the event retention threshold in days based on TOML configuration with a fallback default.

**Acceptance criteria:**
- Where `[database] retention_days` is explicitly configured in TOML, the system uses the configured integer value.
- Where `[database] retention_days` is omitted or unconfigured, the system defaults the retention period to 30 days.
- Retention days value is validated to be a positive integer (> 0).

---

#### REQ-F-004: Daemon EventStore Startup Initialization

**Statement:** When the daemon initializes during startup, the `holonightd` daemon shall resolve the database path, ensure parent directories exist, and open the `EventStore`.

**Acceptance criteria:**
- Parent directories for the resolved database path (e.g. `~/.local/share/holonight/`) are created automatically if they do not exist.
- `EventStore` instance is successfully created and opened using the resolved path.
- SQLite tables (`observation_events`) and indices are created or verified on startup.

---

#### REQ-F-005: Batch Event Persistence on Daemon Iteration

**Statement:** When executing a daemon iteration in `Daemon::runIteration()`, the `holonightd` daemon shall batch insert all collected `ObservationEvent` items into `EventStore`.

**Acceptance criteria:**
- Events collected during the current iteration (e.g., from `StorageCollector` or other collectors) are gathered into a vector.
- The daemon invokes `eventStore.insertBatch(events)` within `Daemon::runIteration()`.
- Successfully inserted events are readable from `EventStore` queries.
- Empty event batches execute safely without throwing or opening unneeded database transactions.

---

#### REQ-F-006: Automated Event Pruning on Daemon Iteration

**Statement:** When executing a daemon iteration in `Daemon::runIteration()`, the `holonightd` daemon shall delete observation events older than the configured retention threshold.

**Acceptance criteria:**
- The cutoff timestamp is computed as `std::chrono::system_clock::now() - std::chrono::days(retention_days)`.
- The daemon invokes `eventStore.pruneEvents(cutoff)` on each iteration.
- All rows in `observation_events` with timestamps strictly prior to `cutoff` are deleted.

---

#### REQ-F-007: Fatal Initialization Error Handling

**Statement:** If `EventStore` fails to open or initialize during daemon startup, then the system shall log a fatal error message and terminate daemon initialization.

**Acceptance criteria:**
- Startup failure (e.g., invalid path permissions, directory creation error, corrupt database file) logs a descriptive error message via `Logger::error()`.
- Daemon initialization halts and process exits with a non-zero status code or throws a fatal runtime exception.
- Daemon does not enter the main execution loop if database initialization fails.

---

#### REQ-F-008: Iteration Error Fault Tolerance

**Statement:** If `insertBatch()` or `pruneEvents()` fails during a routine daemon iteration, then the system shall log the error using `Logger::error()` and continue daemon execution gracefully.

**Acceptance criteria:**
- A database error during `insertBatch()` or `pruneEvents()` in `Daemon::runIteration()` is caught and logged via `Logger::error()`.
- The daemon process does not terminate or abort the iteration loop.
- Subsequent daemon iterations and collector executions continue operating normally.

---

### Non-Functional Requirements (REQ-NF)

#### REQ-NF-001: Execution Loop Overhead & Performance

**Statement:** The batch insertion and pruning operations executed during `Daemon::runIteration()` shall complete within 50 milliseconds under standard operating conditions to prevent delaying scheduled daemon tasks.

**Acceptance criteria:**
- `EventStore::insertBatch()` uses a single SQLite transaction for all events in an iteration.
- Pruning query leverages indexed timestamp column `idx_events_timestamp`.

---

#### REQ-NF-002: Observability & Diagnostic Logging

**Statement:** The daemon shall log database lifecycle events including initialization target path, count of inserted events, count of pruned events, and detailed error messages on failure.

**Acceptance criteria:**
- Target database path is logged at daemon startup.
- Non-zero pruned event counts are logged during iterations.
- Database operation failures include SQLite error strings and diagnostic context in log output.

---

### Constraints (REQ-C)

#### REQ-C-001: Modern C++23 Specification

**Statement:** All daemon integration code shall strictly target standard C++23 (`-std=c++23`) using standard library components (`std::filesystem`, `std::chrono::days`, `std::expected` / explicit exceptions, RAII).

---

#### REQ-C-002: Zero GUI / Qt Dependencies

**Statement:** The database integration and daemon iteration lifecycle shall remain completely headless with zero GUI or Qt framework dependencies.

---

#### REQ-C-003: Code Quality and Static Analysis Compliance

**Statement:** All newly introduced implementation and test code shall pass `task format-check`, `task tidy-src`, and all unit tests in `task test`.
