# SDD Tasks — pacman-collector

- [x] T-001: Define PacmanCollectorOptions, PacmanCollectorMetrics, and PacmanCollector header in `include/holonightd/PacmanCollector.h`
  - REQs: REQ-F-001, REQ-NF-002, REQ-C-001
  - Check: `include/holonightd/PacmanCollector.h` exists with `#pragma once`, targets C++23, and declares `PacmanCollectorOptions`, `PacmanCollectorMetrics`, and `PacmanCollector` with a `[[nodiscard]] std::vector<ObservationEvent> collect() const noexcept` member function signature.

- [x] T-002: Add CMake build target entry for `src/holonightd/PacmanCollector.cpp` in `CMakeLists.txt` and `tests/CMakeLists.txt`
  - REQs: REQ-C-001, REQ-C-003
  - Check: Both `CMakeLists.txt` and `tests/CMakeLists.txt` include `src/holonightd/PacmanCollector.cpp` in target sources, enabling successful configuration with `task configure-tests`.

- [x] T-003: Implement base constructor, `sys_root` path resolution helpers, and non-throwing `collect()` skeleton in `src/holonightd/PacmanCollector.cpp`
  - REQs: REQ-F-001, REQ-NF-001, REQ-NF-002, REQ-NF-003, REQ-C-001, REQ-C-002
  - Check: `PacmanCollector` base constructor and `collect()` skeleton compile cleanly, returning an empty vector when `sys_root / db_path` does not exist without throwing any exceptions.

- [x] T-004: Implement kernel mismatch evaluation (`evaluateKernelState`) and unit tests in `tests/test_pacman_collector.cpp`
  - REQs: REQ-F-002, REQ-F-003, REQ-NF-001
  - Check: `evaluateKernelState` accurately identifies when running kernel release directory is absent from `<sys_root>/usr/lib/modules/` or local package database records, verified by unit tests in `tests/test_pacman_collector.cpp`.

- [x] T-005: Implement database lock state evaluation (`evaluateLockState`) and unit tests in `tests/test_pacman_collector.cpp`
  - REQs: REQ-F-004, REQ-F-005, REQ-F-006, REQ-NF-001
  - Check: `evaluateLockState` parses PID from `<sys_root>/var/lib/pacman/db.lck`, classifies active processes via `/proc` vs dead or malformed PIDs, and unit tests pass for active and stale lock conditions.

- [x] T-006: Implement orphan config scanner (`evaluateOrphanConfigs`) for `.pacnew`/`.pacsave` files and unit tests in `tests/test_pacman_collector.cpp`
  - REQs: REQ-F-007, REQ-F-008, REQ-NF-004
  - Check: `evaluateOrphanConfigs` recursively inspects `<sys_root>/etc` up to `max_depth` (default: 3), correctly counts `.pacnew` and `.pacsave` files while skipping inaccessible directories, and unit tests verify count accuracy and depth bounding.

- [x] T-007: Implement interrupted transaction state evaluation (`evaluateTransactions`) and unit tests in `tests/test_pacman_collector.cpp`
  - REQs: REQ-F-009, REQ-NF-001
  - Check: `evaluateTransactions` detects `.tmp` package directories or transaction lock artifacts under `<sys_root>/var/lib/pacman/local/`, setting `interrupted_transaction_detected = true`, and unit tests verify artifact discovery.

- [x] T-008: Implement complete metric-to-ObservationEvent mapping in `collect()` and comprehensive unit tests covering non-Arch environments, exception safety, and custom `sys_root` mocks
  - REQs: REQ-F-001, REQ-F-003, REQ-F-005, REQ-F-006, REQ-F-008, REQ-F-009, REQ-NF-001, REQ-NF-002, REQ-NF-003, REQ-NF-004, REQ-C-001, REQ-C-002, REQ-C-003
  - Check: `collect()` maps all collected metrics to their specified `ObservationEvent` signals (`pacman.kernel_mismatch`, `pacman.active_lock`, `pacman.stale_lock`, `pacman.pacnew_files`, `pacman.interrupted_transaction`), returning expected events across mock filesystem tests and passing `task test`, `task format-check`, and `task tidy-src`.
