# SDD Tasks — memory-kernel-collector

- [x] T-001: Define MemoryConfig and MemoryCollector headers
  - REQs: REQ-F-001, REQ-C-001
  - Check: `include/holonightd/Application.h` defines `MemoryConfig` struct with `some_warning_threshold`, `full_critical_threshold`, and `meminfo_warning_threshold` fields integrated into `Config`, and `include/holonightd/MemoryCollector.h` defines `MemoryCollectorOptions`, `MemoryMetrics`, and `MemoryCollector` class in namespace `holonightd` with `#pragma once`.

- [x] T-002: Implement TOML configuration parsing for memory settings in Application.cpp
  - REQs: REQ-F-001, REQ-C-001
  - Check: `Application.cpp` parses `[memory]` section keys `some_warning_threshold`, `full_critical_threshold`, and `meminfo_warning_threshold` from TOML files into `Config::memory`, using defaults (10.0%, 25.0%, 85.0%) when omitted.

- [x] T-003: Implement PSI parsing and Meminfo fallback in MemoryCollector.cpp
  - REQs: REQ-F-002, REQ-F-005, REQ-NF-001, REQ-NF-002, REQ-C-001
  - Check: `MemoryCollector::parsePsi()` reads `<proc_root>/pressure/memory` and populates `avg10`/`avg60`/`avg300`/`total` for `some` and `full`; `MemoryCollector::parseMeminfo()` reads `<proc_root>/meminfo` as fallback to compute `total_bytes`, `available_bytes`, `used_bytes`, and `percent_used`.

- [x] T-004: Implement VMStat OOM tracking and victim context extraction in MemoryCollector.cpp
  - REQs: REQ-F-007, REQ-F-008, REQ-F-009, REQ-NF-001, REQ-NF-002, REQ-C-001, REQ-C-002
  - Check: `MemoryCollector::parseVmstatOom()` initializes baseline `oom_kill` counter from `<proc_root>/vmstat` without emitting an event on initial run and calculates deltas on subsequent runs; `MemoryCollector::extractOomVictim()` parses kernel log snapshot for victim PID and process name, defaulting to `0` and `"unknown"` on missing/unreadable logs.

- [x] T-005: Implement metric collection and ObservationEvent compilation in MemoryCollector.cpp
  - REQs: REQ-F-003, REQ-F-004, REQ-F-006, REQ-F-008, REQ-NF-001, REQ-NF-002, REQ-C-001
  - Check: `MemoryCollector::collectMetrics()` aggregates subsystem metrics, and `MemoryCollector::collect()` converts threshold breaches into `ObservationEvent` instances for PSI `memory_pressure_some` (Warning), PSI `memory_pressure_full` (Critical), meminfo fallback `memory_used_high` (Warning), and `oom_killer_invoked` (Error).

- [x] T-006: Wire MemoryCollector implementation in src/CMakeLists.txt
  - REQs: REQ-C-001, REQ-C-003
  - Check: `src/CMakeLists.txt` adds `holonightd/MemoryCollector.cpp` to the `holonightd_core` target source files.

- [x] T-007: Integrate MemoryCollector into Daemon core event loop
  - REQs: REQ-F-001, REQ-NF-001, REQ-C-001
  - Check: `Daemon` in `include/holonightd/Daemon.h` and `src/holonightd/Daemon.cpp` instantiates `MemoryCollector` using `Config::memory` settings, calling `collect()` during tick loops and passing resulting events to the `EventStore`.

- [x] T-008: Add MemoryConfig parsing unit tests in test_application.cpp
  - REQs: REQ-F-001, REQ-NF-003, REQ-C-003
  - Check: `tests/test_application.cpp` includes test cases for parsing custom `[memory]` TOML sections and verifying default values when the section is absent.

- [x] T-009: Implement MemoryCollector unit tests and register test target in CMake
  - REQs: REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-005, REQ-F-006, REQ-F-007, REQ-F-008, REQ-F-009, REQ-NF-001, REQ-NF-002, REQ-NF-003, REQ-C-001, REQ-C-003
  - Check: `tests/test_memory_collector.cpp` tests PSI parsing, threshold events (`some` & `full`), meminfo fallback, vmstat baseline/delta tracking, victim log parsing, and error conditions using mock proc paths; `tests/CMakeLists.txt` includes the new test executable.

- [x] T-010: Run code formatting, static analysis, and full test suite with coverage
  - REQs: REQ-NF-003, REQ-C-003
  - Check: `task format-check`, `task tidy-src`, `task test`, and `task coverage` all complete successfully with zero errors/warnings and 100% test coverage on `MemoryCollector.cpp`.
