# SDD Tasks — eventstore-retention-export

- [x] T-001: Update headers for database configuration and EventStore interface
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-005, REQ-C-001
  - Check: `DatabaseConfig` struct in `Application.h` contains `max_bytes` and `max_events` fields with defaults, and `EventStore.h` declares `pruneEventsByAge`, `pruneEventsByCapacity`, `checkpointWal`, and `exportToJsonl`.

- [x] T-002: Implement TOML configuration parsing for retention limits in Application.cpp
  - REQs: REQ-F-001, REQ-C-001
  - Check: `Config::fromFile` populates `database.max_bytes` and `database.max_events` from the `[database]` section of TOML configs, defaulting to 50MB and 100,000 events when unspecified.

- [x] T-003: Add composite index for severity and timestamp in EventStore.cpp
  - REQs: REQ-F-003, REQ-C-002
  - Check: `EventStore::Impl` schema initialization includes `CREATE INDEX IF NOT EXISTS idx_severity_timestamp ON observation_events (severity ASC, timestamp ASC)`.

- [x] T-004: Implement age-based and capacity-based event pruning logic in EventStore.cpp
  - REQs: REQ-F-002, REQ-F-003, REQ-NF-002, REQ-C-001, REQ-C-002
  - Check: `pruneEventsByAge` deletes rows prior to cutoff timestamp, while `pruneEventsByCapacity` checks database size/count and prunes down to 90% of max limits with severity-first ordering (`Debug`/`Info` before `Critical`).

- [x] T-005: Implement SQLite WAL log checkpointing in EventStore.cpp
  - REQs: REQ-F-004, REQ-C-002
  - Check: `checkpointWal()` executes `PRAGMA wal_checkpoint(TRUNCATE)` against the SQLite handle and returns `std::expected<void, std::string>`.

- [x] T-006: Implement streaming atomic JSONL export in EventStore.cpp
  - REQs: REQ-F-005, REQ-NF-001, REQ-C-001, REQ-C-002
  - Check: `exportToJsonl` streams matching events to `dest_path.tmp` line-by-line as JSON, cleans up `.tmp` on error, and atomically renames `.tmp` to `dest_path` upon completion.

- [x] T-007: Add unit tests for retention parsing, pruning, checkpointing, and export
  - REQs: REQ-NF-003, REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-005, REQ-NF-001
  - Check: Unit tests in `test_application.cpp` and `test_event_store.cpp` cover config parsing, age pruning, capacity hysteresis pruning, severity preservation, WAL checkpointing, and atomic export error resilience.

- [x] T-008: Perform build verification, format checks, and static analysis
  - REQs: REQ-C-003, REQ-NF-003
  - Check: Running `task format-check`, `task tidy-src`, and `task test` passes with zero compiler warnings, zero lint errors, and 100% test suite success.
