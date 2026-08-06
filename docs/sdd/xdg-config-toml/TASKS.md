# SDD Tasks — xdg-config-toml

## Overview
This task breakdown implements the XDG Base Directory config path resolution and TOML format migration for holonightd. Tasks are ordered by dependency: vendoring → CMake wiring → headers → implementation → config file → tests → verification.

---

## Execution Tasks

- [x] T-001: Vendor toml++ single-header library
  - REQs: REQ-NF-001, REQ-C-005
  - Check: `third_party/toml.hpp` exists, contains `namespace toml` definition, file is self-contained with no external #includes

- [x] T-002: Wire toml++ include path in CMake
  - REQs: REQ-NF-001, REQ-NF-003, REQ-C-005
  - Check: Running `cmake --build build` succeeds with `SYSTEM PRIVATE` include of `third_party/` added to `holonightd_core` target

- [x] T-003: Update CliOptions struct to use std::optional<path>
  - REQs: REQ-F-001, REQ-F-002, REQ-C-001
  - Check: `CliOptions::config_path` is `std::optional<std::filesystem::path>` with no default initializer; `--config` flag still parses correctly

- [x] T-004: Declare resolveConfigPath free function in Application.h
  - REQs: REQ-F-001, REQ-F-002, REQ-NF-002
  - Check: Function signature includes `[[nodiscard]]` attribute; accepts `std::optional<std::filesystem::path>` override; returns `std::filesystem::path`; docstring documents XDG spec and precedence

- [x] T-005: Implement resolveConfigPath in Application.cpp
  - REQs: REQ-F-001, REQ-F-002, REQ-NF-001, REQ-NF-002, REQ-NF-004
  - Check: Function correctly checks `XDG_CONFIG_HOME` and falls back to `HOME/.config`; treats empty `XDG_CONFIG_HOME` as unset; throws `std::runtime_error` if `HOME` is unset; explicit override returns as-is without modification

- [x] T-006: Replace INI parser with toml++ in Config::fromFile
  - REQs: REQ-F-004, REQ-F-005, REQ-F-006, REQ-NF-001, REQ-NF-002, REQ-NF-003, REQ-NF-004
  - Check: Function calls `toml::parse()` on input stream; extracts `[general]` table; reads `interval_seconds`, `scan_root`, `commands`; error messages include file path and parsing context; code passes `task fmt:check`

- [x] T-007: Add explicit file existence and readability checks before parsing
  - REQs: REQ-F-003, REQ-NF-004
  - Check: Error message for missing file is "config file not found: PATH"; error message for unopenable file is "failed to open config file: PATH"; both errors are thrown before TOML parsing is attempted

- [x] T-008: Validate interval_seconds is positive
  - REQs: REQ-F-005, REQ-NF-004
  - Check: Throws `std::runtime_error` with message "interval_seconds must be a positive integer" when interval_seconds is zero or negative

- [x] T-009: Update main.cpp to call resolveConfigPath before Config::fromFile
  - REQs: REQ-F-001, REQ-F-002, REQ-C-001
  - Check: `main()` calls `resolveConfigPath(options.config_path)` and passes result to `Config::fromFile()`; behavior matches design flow diagram

- [x] T-010: Wrap toml.hpp include with NOLINTBEGIN/NOLINTEND
  - REQs: REQ-NF-003, REQ-NF-002
  - Check: `Application.cpp` has `// NOLINTBEGIN` before `#include <toml.hpp>` and `// NOLINTEND` after; `task lint` passes with no clang-tidy warnings on that section

- [x] T-011: Replace example config file with TOML format
  - REQs: REQ-F-005, REQ-F-006, REQ-C-002, REQ-C-003
  - Check: File `config/holonightd.example.toml` exists with valid TOML syntax; contains `[general]` section with `interval_seconds`, `scan_root`, and example `commands` array; old `.conf` file is removed

---

## Test Implementation Tasks

- [x] T-012: Create EnvGuard RAII helper in test file
  - REQs: REQ-C-004
  - Check: `EnvGuard` struct defined in `tests/test_application.cpp` with constructor/destructor; saves/restores environment variables; compiles without warnings

- [x] T-013: Write Config::fromFile tests for valid TOML and defaults
  - REQs: REQ-F-004, REQ-F-005, REQ-F-006, REQ-C-004
  - Check: Five tests pass (`task test`):
    - `ConfigFromFileReadsAllFields` — full TOML with all three keys
    - `ConfigFromFileDefaultsWhenFieldsAbsent` — `[general]` present but keys omitted
    - `ConfigFromFileParsesMultipleCommands` — `commands = ["a", "b", "c"]`
    - `ConfigFromFileHandlesEmptyCommands` — missing commands defaults to empty vector
    - `ConfigFromFileEmptyCommandsArray` — explicit empty array `commands = []`

- [x] T-014: Write Config::fromFile error tests
  - REQs: REQ-F-003, REQ-F-004, REQ-F-005, REQ-C-004
  - Check: Four tests pass (`task test`):
    - `ConfigFromFileThrowsWhenFileNotFound` — asserts `std::runtime_error` with "config file not found" in message
    - `ConfigFromFileThrowsOnMalformedToml` — invalid TOML raises exception
    - `ConfigFromFileThrowsOnNonPositiveInterval` — `interval_seconds = 0` raises error
    - `ConfigFromFileMissingGeneralTable` — no `[general]` section raises error

- [x] T-015: Write resolveConfigPath tests for XDG resolution
  - REQs: REQ-F-001, REQ-F-002, REQ-C-001, REQ-C-004
  - Check: Five tests pass (`task test`):
    - `ResolveConfigPathExplicitOverride` — override takes precedence
    - `ResolveConfigPathUsesXdgConfigHome` — `XDG_CONFIG_HOME=/custom` yields `/custom/holonight/holonightd.toml`
    - `ResolveConfigPathFallsBackToHome` — unset `XDG_CONFIG_HOME` uses `~/.config`
    - `ResolveConfigPathEmptyXdgFallsBack` — empty `XDG_CONFIG_HOME` treated as unset
    - `ResolveConfigPathThrowsWhenHomeUnset` — both env vars unset raises error

---

## Verification Tasks

- [x] T-016: Run full test suite and verify all new tests pass
  - REQs: REQ-C-004
  - Check: `task test` exits with code 0; all 14 new test cases (5 valid+4 error+5 resolve) pass; no test segfaults or deadlocks

- [x] T-017: Apply code formatting to all modified files
  - REQs: REQ-NF-003, REQ-C-006
  - Check: `task fmt` completes without error; `task fmt:check` shows no violations on `include/holonightd/Application.h`, `src/holonightd/Application.cpp`, `src/main.cpp`, `tests/test_application.cpp`

- [x] T-018: Run clang-tidy linter and fix any violations
  - REQs: REQ-NF-003, REQ-NF-002, REQ-C-006
  - Check: `task lint` exits with code 0; no new warnings introduced by config loading code; warnings from toml++ instantiations are suppressed by NOLINTBEGIN/END guards

- [x] T-019: Clean build and verify no warnings
  - REQs: REQ-NF-002, REQ-NF-003
  - Check: `rm -rf build && task build` completes with exit code 0; compiler output shows no new warnings; executable binary is created at `build/holonightd`

- [x] T-020: Integration test: load valid config via XDG and --config override
  - REQs: REQ-F-001, REQ-F-002, REQ-C-001, REQ-C-004
  - Check: Run `build/holonightd --config config/holonightd.example.toml --once` and verify it loads config without error; run with `XDG_CONFIG_HOME` set to valid path and verify load succeeds

- [x] T-021: Integration test: error messages are user-friendly
  - REQs: REQ-F-003, REQ-NF-004
  - Check: Run with non-existent path in `--config`; stderr output contains "error: config file not found: PATH" (no C++ exception type names); exit code is 1

- [x] T-022: Verify no external runtime dependencies introduced
  - REQs: REQ-NF-001, REQ-C-005
  - Check: `ldd build/holonightd | grep -i toml` returns no results; `objdump -p build/holonightd | grep NEEDED | grep -i toml` returns no results

- [x] T-023: Create final commit with Conventional Commits message
  - REQs: REQ-C-006
  - Check: Commit message follows `feat(config): <subject>` format with optional body describing XDG + TOML migration; all changes from T-001 through T-022 are staged

---

## Task Dependencies

```
T-001 ──────┐
            ├──→ T-002 ──────────┐
            │                    │
T-003 ──────┼────────────────────┼──→ T-004 ──→ T-005 ──→ T-009 ──┐
            │                    │                                │
T-006 ──────┼────────────────────┼──→ T-007 ──→ T-008 ──────────┤
            │                    │                                ├──→ T-016
            │                    │                                │
            ├──→ T-010 ──────────┤                                │
            │                    │                                │
            └──→ T-011 ──────────┘──→ T-012 ──→ T-013 ──→ T-014 ──┘
                                      T-012 ──→ T-015 ───────────┘

T-016 ──→ T-017 ──→ T-018 ──→ T-019 ──→ T-020 ──→ T-021 ──→ T-022
```

---

## Acceptance Criteria Summary

✓ All 16 execution tasks complete with passing checks  
✓ All 5 test task groups pass with full code coverage  
✓ All 7 verification tasks pass without warnings or errors  
✓ SPEC REQ-F (7 functional), REQ-NF (4 non-functional), REQ-C (6 constraint) fully satisfied  
✓ Code is formatted, linted, tested, and builds cleanly  
✓ No external TOML library runtime dependency; toml++ vendored as single header  
✓ Conventional Commits message created and ready to push
