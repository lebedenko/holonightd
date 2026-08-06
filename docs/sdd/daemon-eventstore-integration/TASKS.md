# SDD Tasks — daemon-eventstore-integration

- [x] T-001: Implement DatabaseConfig and resolveDatabasePath in Application.h/cpp
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-C-001
  - Check: `resolveDatabasePath()` returns explicit TOML path, $XDG_DATA_HOME path, or ~/.local/share/holonight/events.db fallback correctly.

- [x] T-002: Unit tests for DatabaseConfig and resolveDatabasePath in test_application.cpp
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-C-003
  - Check: `test_application.cpp` verifies TOML [database] parsing, default 30-day retention, and XDG/explicit path resolution.

- [x] T-003: Integrate EventStore into Daemon class in Daemon.h/cpp
  - REQs: REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-007, REQ-F-008, REQ-NF-001, REQ-NF-002
  - Check: `Daemon` initializes `EventStore` on startup, batch-inserts `storage_events` in `runIteration()`, prunes old events, and logs failures gracefully.

- [x] T-004: Update main.cpp to pass resolved database path to Daemon
  - REQs: REQ-F-002, REQ-F-004, REQ-F-007
  - Check: `main.cpp` resolves database path from config, creates `Daemon`, and handles initialization errors with non-zero exit code.

- [x] T-005: Write unit and integration tests for Daemon EventStore persistence in test_daemon.cpp
  - REQs: REQ-F-005, REQ-F-006, REQ-F-008, REQ-C-003
  - Check: `test_daemon.cpp` verifies events are persisted to SQLite during `runIteration()` and old events are pruned.

- [x] T-006: Run verification suite (format-check, tidy-src, test)
  - REQs: REQ-C-001, REQ-C-002, REQ-C-003
  - Check: `task format-check`, `task tidy-src`, and `task test` pass with 100% success.
