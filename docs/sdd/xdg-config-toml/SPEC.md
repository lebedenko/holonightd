# XDG Base Directory Config & TOML Migration — EARS Specification

**Feature ID:** `xdg-config-toml`  
**Project:** holonightd (Linux daemon, C++23)  
**Date:** 2026-06-12

---

## Overview

This specification defines the migration of `holonightd` to use the XDG Base Directory Specification for default config location and switch from INI-like `key=value` format to TOML. The feature introduces three key changes:

1. **Default config path resolution** following the XDG Base Directory spec (`$XDG_CONFIG_HOME/holonight/holonightd.toml`, fallback to `~/.config/holonight/holonightd.toml`)
2. **Explicit CLI override** via `--config PATH` that fully replaces the resolved default (no merging)
3. **Hard error with clear messaging** when the resolved config file does not exist
4. **TOML format migration** with a `[general]` section schema
5. **Vendored toml++** single-header library (no external runtime dependency)

---

## Requirements

### Functional Requirements (REQ-F)

#### REQ-F-001: Resolve default config path using XDG Base Directory spec

**Statement:** When `holonightd` starts without `--config` argument, the system shall resolve the config file path as follows:
1. If `$XDG_CONFIG_HOME` environment variable is set and non-empty, use `$XDG_CONFIG_HOME/holonight/holonightd.toml`
2. If `$XDG_CONFIG_HOME` is unset or empty, fall back to `$HOME/.config/holonight/holonightd.toml`

**Acceptance criteria:**
- When `XDG_CONFIG_HOME=/custom/config` and config file exists at that path, the daemon loads it
- When `XDG_CONFIG_HOME` is unset, the daemon resolves to `$HOME/.config/holonight/holonightd.toml`
- When `XDG_CONFIG_HOME` is set to an empty string, the daemon treats it as unset and uses `$HOME/.config`
- Path resolution occurs before any file I/O attempt and is logged (if logging is enabled)

---

#### REQ-F-002: Allow explicit config path override via --config

**Statement:** When the user provides `--config PATH` argument, the system shall use `PATH` as the config file location, replacing the default resolution entirely (no merging with default location).

**Acceptance criteria:**
- `--config /path/to/custom.toml` loads config from the specified path, regardless of `XDG_CONFIG_HOME` or `$HOME`
- Multiple `--config` arguments: the last one takes precedence (or reject with error — implementer's choice, documented)
- `--config` with a relative path resolves relative to the current working directory at startup time
- `--config` with an empty string or no argument raises an error with message "–config requires a path"

---

#### REQ-F-003: Hard error when resolved config file does not exist

**Statement:** If the resolved config file path (default or explicit via `--config`) does not exist or is not readable, then the system shall exit immediately with a non-zero exit code and emit a human-readable error message to stderr.

**Acceptance criteria:**
- Missing file error message includes the full resolved path
- Error message distinguishes between "file not found" and "permission denied" cases
- The error is raised before any daemon state is initialized
- Error format: `error: config file not found: {resolved_path}` (or `permission denied`, `is a directory`, etc.)
- Exit code is non-zero (e.g., 1)

---

#### REQ-F-004: Parse config file in TOML format

**Statement:** The system shall parse the config file as TOML, rejecting any invalid TOML syntax with a clear error message pointing to the line/column where parsing failed.

**Acceptance criteria:**
- Valid TOML files are parsed successfully (verified via toml++ library parsing without exceptions)
- Invalid TOML (e.g., unclosed brackets, invalid escape sequences) produces an error message including line:column
- The error message is human-readable and does not expose internal library details
- Parsing errors include the invalid line/fragment for context

---

#### REQ-F-005: Load [general] section with interval_seconds, scan_root, and commands

**Statement:** The system shall load configuration values from the `[general]` section of the TOML file:
- `interval_seconds` (integer): polling interval in seconds
- `scan_root` (string): root directory path for scans
- `commands` (array of strings): shell commands to execute

**Acceptance criteria:**
- All three keys are correctly deserialized from the TOML `[general]` table
- `interval_seconds` is parsed as a positive integer; zero or negative values raise an error
- `scan_root` is parsed as a string and converted to `std::filesystem::path`
- `commands` is parsed as an array of strings; empty array is allowed
- Missing `interval_seconds` or `scan_root` raises a clear error (e.g., "required key missing: interval_seconds")
- Missing `commands` array defaults to an empty vector (allows running with no commands)

---

#### REQ-F-006: Default values for optional config keys

**Statement:** Where `commands` array is missing from `[general]`, the system shall default to an empty vector of commands (no commands executed).

**Acceptance criteria:**
- Config file with only `interval_seconds` and `scan_root` (no `commands` key) loads successfully
- The resulting `Config::commands` vector is empty
- An empty `commands` array in TOML also results in an empty vector (not an error)

---

#### REQ-F-007: Reject config with unexpected top-level TOML sections

**Statement:** If the TOML file contains top-level sections other than `[general]`, then the system shall log a warning or error and either skip the unknown section or fail with a clear message (implementer decision, must be documented).

**Acceptance criteria:**
- A TOML file with `[general]` and an unrecognized `[unknown_section]` is handled consistently
- Behavior is documented in code comments or error messages
- If failure is chosen, error message is clear: "unexpected config section: [unknown_section]"
- If ignored, a debug log (if available) notes the skipped section

---

### Non-Functional Requirements (REQ-NF)

#### REQ-NF-001: No external runtime dependencies for TOML parsing

**Statement:** The system shall parse TOML without introducing external runtime package dependencies; toml++ is vendored as a single header file under `third_party/` and included directly in the build.

**Acceptance criteria:**
- `third_party/toml.hpp` (or similar) is a complete single-header implementation
- CMakeLists.txt includes `third_party/` in the include path but does not link to external TOML library
- The final binary does not depend on system TOML libraries (e.g., libtoml, libtoml0)
- Build succeeds on a minimal Linux system with no TOML packages installed

---

#### REQ-NF-002: C++23 compliance with zero unsafe constructs

**Statement:** The system shall use C++23 features (ranges, `std::expected`, structured bindings, etc.) and shall not introduce raw owning pointers, manual memory management, or RAII violations.

**Acceptance criteria:**
- No raw pointers for memory ownership (e.g., `new`/`delete`) in config loading code
- All TOML parsing errors are handled via exceptions or `std::expected<T, Error>` (implementer choice)
- Code compiles with `-std=c++23` flag and passes clang-tidy checks (HOLONIGHTD_ENABLE_CLANG_TIDY=ON)
- No undefined behavior or memory safety issues detected by ASan or Valgrind

---

#### REQ-NF-003: Google-style code formatting and 120-character line limit

**Statement:** The config loading implementation shall adhere to Google C++ style guide conventions enforced by `.clang-format` and `.clang-tidy`, with a 120-character line length limit.

**Acceptance criteria:**
- Running `task fmt:check` shows no formatting violations in new config files
- Running `task fmt` auto-corrects all formatting to Google style
- Running `task lint` (clang-tidy) reports no new violations
- All lines are <= 120 characters

---

#### REQ-NF-004: Clear, user-friendly error messages

**Statement:** All error conditions (missing file, invalid TOML, invalid key value, missing required key) shall include a human-readable message suitable for end-user consumption, not internal debug details.

**Acceptance criteria:**
- Error messages include the config file path being attempted
- Error messages avoid exception type names or stack traces in user output
- Example message for missing file: `error: config file not found: /home/user/.config/holonight/holonightd.toml`
- Example message for invalid TOML: `error: invalid TOML syntax at line 3, column 5: unclosed bracket`
- Example message for invalid key value: `error: config key 'interval_seconds' must be a positive integer, got: 0`

---

### Constraint Requirements (REQ-C)

#### REQ-C-001: Maintain backward compatibility with --config explicit path

**Statement:** The `--config PATH` mechanism shall continue to work exactly as it does today: accepting an arbitrary file path and loading it. If `--config` is omitted, the new XDG resolution logic applies; if provided, XDG resolution is bypassed entirely.

**Acceptance criteria:**
- Existing scripts and systemd units using `holonightd --config /etc/holonightd.conf` continue to work unchanged
- `--config` takes precedence over environment variables and default locations
- No automatic fallback to XDG paths when `--config` points to a non-existent file (fails immediately with clear error)

---

#### REQ-C-002: TOML schema uses [general] section only

**Statement:** The config TOML schema shall use a single `[general]` section containing all configuration keys. No nested tables or multiple sections are supported in this version.

**Acceptance criteria:**
- Schema documents that only `[general]` is recognized
- Config keys (`interval_seconds`, `scan_root`, `commands`) are only valid inside `[general]`
- Placing these keys at the TOML root level (outside any section) is an error with a clear message
- Future versions may add additional sections; this constraint allows clean extension

---

#### REQ-C-003: No merging of config sources

**Statement:** The system shall not merge config from multiple sources (e.g., environment variables, default files, CLI arguments). A single config file is loaded entirely; there is no partial override mechanism.

**Acceptance criteria:**
- Config is loaded from exactly one file: either the XDG default or the `--config` path
- Environment variables do not override or augment config keys (e.g., no `HOLONIGHTD_INTERVAL_SECONDS` support)
- The resolved/explicit config file is the sole source of truth
- If more granular config override is needed, it is a separate feature

---

#### REQ-C-004: Use Google Test for all config parsing tests

**Statement:** All new test cases for config loading, XDG path resolution, and TOML parsing shall be written using Google Test (GTest) framework with `TEST(Suite, Name)` macro and `EXPECT_*` / `ASSERT_*` assertions.

**Acceptance criteria:**
- Tests are located in `tests/test_*.cpp` files
- Tests build with `task test` and pass (exit code 0)
- Test coverage includes: XDG path resolution, missing file error, valid TOML parsing, invalid TOML syntax, missing required keys, type errors
- Tests write temp files to `/tmp` and clean up after themselves
- No external test dependencies beyond GTest

---

#### REQ-C-005: Vendor toml++ as a single-header library

**Statement:** The toml++ library shall be vendored as a complete single-header file under `third_party/toml.hpp` (or equivalent). No submodules, no git dependencies, no package manager integration.

**Acceptance criteria:**
- `third_party/toml.hpp` exists and is self-contained (no #include of external files relative to system paths)
- CMakeLists.txt adds `third_party/` to `include_directories()`
- Build succeeds without fetching or cloning anything (offline build works)
- The header is the official toml++ single-header release (e.g., from https://github.com/marzer/tomlplusplus)

---

#### REQ-C-006: Follow Conventional Commits for this feature

**Statement:** All commits implementing this feature shall follow the Conventional Commits specification: `feat:`, `fix:`, `chore:`, `docs:`, etc.

**Acceptance criteria:**
- Commit messages use format: `<type>(<scope>): <subject>`
- Example: `feat(config): add XDG Base Directory support`, `feat(config): migrate to TOML format`, `test(config): add XDG path resolution tests`
- Scope is optional but recommended (e.g., `(config)`)
- First line is <= 50 characters (when scope fits)

---

## Example Config File

**Path:** `$XDG_CONFIG_HOME/holonight/holonightd.toml` (or `~/.config/holonight/holonightd.toml`)

```toml
[general]
interval_seconds = 300
scan_root = "."
commands = [
    "cmake --build build",
    "ctest --test-dir build --output-on-failure"
]
```

---

## Error Message Examples

**Missing config file:**
```
error: config file not found: /home/user/.config/holonight/holonightd.toml
```

**Invalid TOML:**
```
error: TOML parse error at line 2, column 1: expected '=' after key name
```

**Missing required key:**
```
error: config key 'interval_seconds' is required in [general] section
```

**Invalid interval value:**
```
error: config key 'interval_seconds' must be a positive integer, got: 0
```

**Permission denied:**
```
error: config file not readable: /etc/holonightd/holonightd.toml (permission denied)
```

---

## Out of Scope (Future Features)

- Environment variable overrides (e.g., `HOLONIGHTD_INTERVAL_SECONDS`)
- Config merging or partial updates
- JSON or YAML support (TOML only in this version)
- Hot-reload of config without daemon restart
- Configuration validation schema (JSON Schema, etc.)
- Additional top-level TOML sections beyond `[general]`

---

## Implementation Guidance

### Key Files to Create/Modify

1. **`third_party/toml.hpp`** — Vendor toml++ single-header library
2. **`include/holonightd/Application.h`** — Extend `Config` struct to support TOML deserialization
3. **`src/holonightd/Application.cpp`** — Implement:
   - `resolveConfigPath()` function for XDG resolution
   - `Config::fromFile()` update to parse TOML instead of INI
   - Error handling with clear messages
4. **`src/main.cpp`** — Update to use `resolveConfigPath()` when `--config` is not provided
5. **`tests/test_config.cpp`** — New test suite covering:
   - XDG path resolution (with and without `XDG_CONFIG_HOME`)
   - Valid TOML parsing
   - Invalid TOML error messages
   - Missing file error messages
   - Missing required keys
   - Type mismatches
   - Edge cases (empty commands, negative intervals)
6. **`config/holonightd.example.toml`** — Updated example config in TOML format

### Error Handling Strategy

Use exceptions for config errors (consistency with current approach) or `std::expected<Config, std::string>` (C++23 approach). Whichever is chosen, ensure the error message flows cleanly to stderr and exit code is non-zero.

### Testing Strategy

- **Unit tests** for path resolution, TOML parsing, validation
- **Integration tests** with real config files in `/tmp`
- **Error tests** for all error paths with `EXPECT_*` assertions on error message content

---

## Acceptance Checklist

- [ ] All REQ-F (functional) requirements are implemented and tested
- [ ] All REQ-NF (non-functional) requirements are verified:
  - No external TOML runtime dependency
  - C++23 code, no unsafe constructs
  - Google style formatting, 120-char limit
  - Clear error messages
- [ ] All REQ-C (constraints) are satisfied:
  - Backward compatibility with `--config` preserved
  - TOML `[general]` schema enforced
  - No config merging
  - Google Test coverage
  - toml++ vendored
  - Conventional Commits used
- [ ] Code passes:
  - `task fmt:check` (no formatting violations)
  - `task lint` (no clang-tidy violations)
  - `task test` (all tests pass)
  - `task build` (clean build, no warnings)
- [ ] Example config file updated to TOML format
- [ ] All error paths tested with real config files
