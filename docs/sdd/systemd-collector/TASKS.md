# SDD Tasks — systemd-collector

- [x] T-001: Extend configuration structures for `[systemd]` table parsing and defaults
  - REQs: REQ-F-001, REQ-F-008, REQ-C-001
  - Check: `SystemdConfig` struct is added to `Application.h`, and `Config::fromFile` in `Application.cpp` parses `flapping_threshold`, `flapping_window_seconds`, and `ignore_units` with default values (3, 300, []) while falling back to defaults with a warning when threshold or window are <= 0.

- [x] T-002: Declare `SystemdCollector` class and option structures in header
  - REQs: REQ-F-001, REQ-F-004, REQ-NF-002, REQ-C-001
  - Check: `include/holonightd/SystemdCollector.h` defines `SystemdCollectorOptions`, `UnitFlappingState`, and `SystemdCollector` class with RAII primitives and `collect()` method returning `std::vector<ObservationEvent>`.

- [x] T-003: Implement D-Bus connection management and non-systemd fallback logic
  - REQs: REQ-F-007, REQ-NF-001, REQ-NF-002, REQ-C-001, REQ-C-002
  - Check: `SystemdCollector::collect()` in `src/holonightd/SystemdCollector.cpp` safely initializes `sd-bus` system connection using RAII wrappers, handles missing D-Bus or non-systemd init systems by logging a warning, and returns an empty event vector without crashing or throwing uncaught exceptions.

- [x] T-004: Implement `ListUnits` D-Bus call, failed unit detection, and ignored units filtering
  - REQs: REQ-F-002, REQ-F-003, REQ-F-006, REQ-NF-001, REQ-C-002
  - Check: Collector executes `org.freedesktop.systemd1.Manager.ListUnits` with strict timeout, filters out units matching `ignore_units`, identifies units in `"failed"` active state, and generates `ObservationEvent` instances with signal `"unit_failed"` and `Severity::Error`.

- [x] T-005: Implement unit flapping sliding window monitoring
  - REQs: REQ-F-004, REQ-F-006, REQ-NF-001
  - Check: Tracks restart timestamps per unit in `UnitFlappingState`, prunes timestamps older than `flapping_window_seconds`, and generates `ObservationEvent` with signal `"unit_flapping"` and `Severity::Warning` when restart count reaches `flapping_threshold` for non-ignored units.

- [x] T-006: Implement coredump event detection
  - REQs: REQ-F-005, REQ-F-006
  - Check: Detects coredump entries via `systemd-coredump` journal/log inspection, extracts target process/unit and crash metadata, and emits `ObservationEvent` with signal `"coredump"` and `Severity::Error` while honoring `ignore_units`.

- [x] T-007: Update CMake build target for `SystemdCollector` implementation
  - REQs: REQ-C-001, REQ-C-002
  - Check: `src/holonightd/SystemdCollector.cpp` is added to `holonightd_core` library target in `src/CMakeLists.txt` and compiles cleanly under `-std=c++23`.

- [x] T-008: Add unit tests for configuration parsing and fallback handling
  - REQs: REQ-F-001, REQ-F-007, REQ-F-008, REQ-C-003
  - Check: `tests/test_systemd_collector.cpp` contains test cases for TOML `[systemd]` section parsing, zero/negative parameter fallback recovery, and non-systemd environment graceful fallback.

- [x] T-009: Add unit tests for failed units, flapping sliding window, coredumps, and filtering
  - REQs: REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-005, REQ-F-006, REQ-C-003
  - Check: Test suite in `tests/test_systemd_collector.cpp` verifies `unit_failed`, `unit_flapping`, and `coredump` event generation, sliding window timestamp pruning, and `ignore_units` filtering. Target is registered in `tests/CMakeLists.txt`.

- [x] T-010: Perform static analysis, formatting, and test suite verification
  - REQs: REQ-C-001, REQ-C-002, REQ-C-003, REQ-NF-001, REQ-NF-002
  - Check: `task format-check`, `task tidy-src`, and `task test` all succeed with zero errors or warnings.
