# Storage Collector — EARS Specification

**Feature Slug:** `storage-collector`  
**Component:** `holonightd::StorageCollector`  
**Project:** holonightd (Linux daemon, C++23)  
**Date:** 2026-07-30  
**Phase:** SDD Stage 1 (Requirements)

---

## 1. Overview

The `StorageCollector` is a dedicated C++23 subsystem in `holonightd` responsible for lightweight, periodic monitoring of local filesystem storage health and capacity pressure.

It discovers active local physical mount points, collects byte space and inode usage metrics via `statvfs`, evaluates pressure thresholds, and emits normalized `ObservationEvent` payloads into the daemon's diagnostic pipeline.

---

## 2. Requirements

### Functional Requirements (REQ-F)

#### REQ-F-001: Automatic Local Mount Point Discovery

**Statement:** When storage collection is executed, the storage collector shall discover all active local physical mount points by reading system mount information from `/proc/mounts` or `/proc/self/mountinfo`.

**Acceptance criteria:**
- Reads and parses mount target paths, filesystem types (`fstype`), and mount options from `/proc/mounts` or `/proc/self/mountinfo`.
- Correctly identifies root `/` and standard local disk filesystems (e.g., `ext4`, `xfs`, `btrfs`, `zfs`).
- Returns a structured list of discovered mount point descriptors containing mount path, device node, and filesystem type.

---

#### REQ-F-002: Pseudo and Virtual Filesystem Filtering

**Statement:** When discovering mount points, the storage collector shall skip pseudo and virtual filesystems.

**Acceptance criteria:**
- Skips filesystems matching virtual types: `proc`, `sysfs`, `tmpfs`, `devtmpfs`, `cgroup`, `cgroup2`, `overlay`, `squashfs`, `devpts`, `securityfs`, `bpf`, `pstore`, `tracefs`.
- Does not include pseudo or virtual mount points in collected storage metrics.

---

#### REQ-F-003: Network Filesystem Filtering

**Statement:** When discovering mount points, the storage collector shall skip network filesystems.

**Acceptance criteria:**
- Skips filesystems matching network types: `nfs`, `nfs4`, `cifs`, `smbfs`, `sshfs`, `fuse.sshfs`.
- Prevents potential I/O blocking or hang issues associated with un-responding network shares during metric collection.

---

#### REQ-F-004: Removable Media Path Filtering

**Statement:** When discovering mount points, the storage collector shall skip dynamic removable media mounted under `/run/media/*` or `/media/*`.

**Acceptance criteria:**
- Mount point paths beginning with `/run/media/` or `/media/` are excluded from discovery results.
- Dynamic USB or optical drive mounts do not trigger unexpected storage alerts.

---

#### REQ-F-005: Explicit Mount Point Configuration

**Statement:** Where explicit mount points are specified in the configuration, the storage collector shall collect metrics only for the configured mount points.

**Acceptance criteria:**
- When `holonightd.toml` includes a custom list of mount paths (e.g., `mount_points = ["/", "/home"]`), discovery restricts evaluation to those paths.
- When no explicit list is configured, automatic mount point discovery logic is used.

---

#### REQ-F-006: Byte Space and Inode Metrics Collection via statvfs

**Statement:** When evaluating a target mount point, the storage collector shall measure byte space and inode metrics using the statvfs system call.

**Acceptance criteria:**
- Calculates `total_bytes` (`f_blocks * f_frsize`), `available_bytes` (`f_bavail * f_frsize`), and `used_bytes` (`(f_blocks - f_ffree) * f_frsize`).
- Calculates `percent_used` space as a double value (`(used_bytes / total_bytes) * 100.0`).
- Calculates `total_inodes` (`f_files`), `free_inodes` (`f_ffree`), `used_inodes` (`f_files - f_ffree`), and `inode_percent_used` (`(used_inodes / total_inodes) * 100.0`).

---

#### REQ-F-007: Space Pressure Event Generation

**Statement:** When a mount point's byte space usage equals or exceeds the configured warning threshold, the storage collector shall emit an ObservationEvent with signal space_pressure.

**Acceptance criteria:**
- Sets event `source` to `"storage_collector"`, `category` to `"storage"`, and `subject` to the mount point path (e.g., `"/"`).
- Sets event `signal` to `"space_pressure"`.
- Sets event `value` to the calculated byte `percent_used` as a `double` (e.g., `88.5`).
- Sets event `severity` to `Severity::Warning` if byte usage is `>= warning_threshold` (default 85.0%) and `< critical_threshold` (default 95.0%).
- Sets event `severity` to `Severity::Critical` if byte usage is `>= critical_threshold` (default 95.0%).
- Populates event `attributes` JSON object with `total_bytes`, `used_bytes`, `available_bytes`, `total_inodes`, `used_inodes`, `free_inodes`, and `fstype`.

---

#### REQ-F-008: Inode Pressure Event Generation

**Statement:** When a mount point's inode usage equals or exceeds the configured warning threshold, the storage collector shall emit an ObservationEvent with signal inode_pressure.

**Acceptance criteria:**
- Sets event `source` to `"storage_collector"`, `category` to `"storage"`, and `subject` to the mount point path.
- Sets event `signal` to `"inode_pressure"`.
- Sets event `value` to the calculated `inode_percent_used` as a `double`.
- Sets event `severity` to `Severity::Warning` if inode usage is `>= warning_threshold` and `< critical_threshold`.
- Sets event `severity` to `Severity::Critical` if inode usage is `>= critical_threshold`.
- Populates event `attributes` JSON object with complete storage and filesystem metadata.

---

#### REQ-F-009: Unexpected Read-Only Filesystem Event Generation

**Statement:** When a writeable mount point is detected to be mounted read-only, the storage collector shall emit an ObservationEvent with signal read_only_filesystem.

**Acceptance criteria:**
- Sets event `source` to `"storage_collector"`, `category` to `"storage"`, and `subject` to the mount point path.
- Sets event `signal` to `"read_only_filesystem"`.
- Sets event `value` to `std::monostate`.
- Sets event `severity` to `Severity::Error`.
- Includes mount flags and filesystem attributes in event `attributes`.

---

#### REQ-F-010: Configurable Pressure Thresholds

**Statement:** The storage collector shall read warning_threshold and critical_threshold configuration values from holonightd.toml.

**Acceptance criteria:**
- Defaults `warning_threshold` to `85.0%` if not specified in configuration.
- Defaults `critical_threshold` to `95.0%` if not specified in configuration.
- Overrides defaults with configured values when present in `holonightd.toml`.
- Thresholds apply uniformly to both byte space and inode usage evaluation.

---

### Non-Functional Requirements (REQ-NF)

#### REQ-NF-001: Graceful Error Handling and Failure Isolation

**Statement:** If statvfs fails or /proc/mounts cannot be read for a mount point, then the storage collector shall emit a stat_failure ObservationEvent without throwing unhandled exceptions or terminating the daemon.

**Acceptance criteria:**
- Catches I/O or `statvfs` system errors gracefully.
- Emits an `ObservationEvent` with `signal` set to `"stat_failure"` and `severity` set to `Severity::Error`.
- Returns `std::expected<std::vector<ObservationEvent>, StorageError>` or empty/error result structure without propagating exceptions.
- Daemon process remains stable and continues executing without crash or restart.

---

#### REQ-NF-002: Low Overhead Execution

**Statement:** While performing storage metrics collection, the storage collector shall execute statvfs calls synchronously across all discovered mount points in under 10 milliseconds total.

**Acceptance criteria:**
- Does not perform recursive directory traversals or file tree scans.
- Discovery relies purely on `/proc/mounts` or `/proc/self/mountinfo` parsing and single `statvfs` calls per mount point.

---

### Constraints (REQ-C)

#### REQ-C-001: Modern C++23 Implementation

**Statement:** The storage collector shall be implemented in C++23 as a distinct class holonightd::StorageCollector without refactoring or modifying the existing FilesystemScanner class.

**Acceptance criteria:**
- Header located at `include/holonightd/StorageCollector.h` and implementation at `src/holonightd/StorageCollector.cpp`.
- Existing `FilesystemScanner` class remains untouched.
- Uses standard C++23 features (`std::filesystem`, `std::expected`, `std::optional`, RAII).

---

#### REQ-C-002: Scope Limits and Non-Goals

**Statement:** The storage collector shall perform metric observation only and shall not execute file deletion, automated cleanup, or SMART hardware metric collection.

**Acceptance criteria:**
- No file modification, deletion, or disk cleanup routines are included.
- No SMART IOCTL or nvme-cli/smartctl calls are performed by `StorageCollector`.

---

#### REQ-C-003: Code Quality and Linting Compliance

**Statement:** The storage collector implementation shall pass all static analysis checks in task tidy-src, code formatting in task format-check, and unit tests in task test.

**Acceptance criteria:**
- Zero warnings or errors returned by `task format-check`.
- Zero warnings or errors returned by `task tidy-src`.
- Unit tests in `tests/test_storage_collector.cpp` pass cleanly under `task test`.
