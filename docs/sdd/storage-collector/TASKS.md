# SDD Tasks — storage-collector

## Execution Tasks

- [x] T-001: Create StorageCollector.h header file
  - REQs: REQ-C-001, REQ-F-001, REQ-F-006, REQ-F-010
  - Check: `include/holonightd/StorageCollector.h` defines `StorageCollectorOptions`, `MountInfo`, and `StorageCollector` in namespace `holonightd` with `#pragma once`.

- [x] T-002: Wire StorageCollector header and source in CMakeLists.txt
  - REQs: REQ-C-001, REQ-C-003
  - Check: `CMakeLists.txt` includes `include/holonightd/StorageCollector.h` and `src/holonightd/StorageCollector.cpp` in `holonightd_core` target.

- [x] T-003: Implement mount point discovery and filtering in StorageCollector.cpp
  - REQs: REQ-F-001, REQ-F-002, REQ-F-003, REQ-F-004, REQ-F-005
  - Check: `StorageCollector::discoverMounts()` parses `/proc/mounts` or `/proc/self/mountinfo`, skipping virtual (proc, sysfs, tmpfs, overlay), network (nfs, cifs, sshfs), and removable (/media/*, /run/media/*) mounts.

- [x] T-004: Implement statvfs metric collection and pressure event generation in StorageCollector.cpp
  - REQs: REQ-F-006, REQ-F-007, REQ-F-008, REQ-F-009, REQ-F-010, REQ-NF-001, REQ-NF-002, REQ-C-002
  - Check: `StorageCollector::inspectMount()` calls `statvfs`, calculates byte and inode percentages, and returns ObservationEvents for space pressure, inode pressure, read-only mounts, or stat failures without throwing exceptions.

- [x] T-005: Update Config to include storage collector threshold settings
  - REQs: REQ-F-010
  - Check: `Config` struct and TOML parsing in `Application.h`/`Application.cpp` support optional `storage_warning_threshold`, `storage_critical_threshold`, and `storage_mount_points`.

## Test Implementation Tasks

- [x] T-006: Write StorageCollector unit tests in tests/test_storage_collector.cpp
  - REQs: REQ-F-001..010, REQ-NF-001, REQ-C-001, REQ-C-003
  - Check: GTest unit tests verify mount discovery filtering, statvfs metric calculation, threshold event triggers (Warning & Critical for space and inodes), read-only mount detection, and statvfs error handling.

## Verification Tasks

- [x] T-007: Run code formatting, static analysis, and full unit test suite
  - REQs: REQ-C-003
  - Check: `task format-check`, `task tidy-src`, and `task test` all succeed with zero errors or warnings.
