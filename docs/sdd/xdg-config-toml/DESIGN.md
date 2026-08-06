# Design: XDG Config Path + toml++ Migration

**Feature branch target:** `feat/xdg-config-toml`
**Date:** 2026-06-12
**Status:** Draft

---

## 1. Component Overview

### What changes

| Component | Change |
|---|---|
| `include/holonightd/Application.h` | `Config` struct: rename `commands` field source (array, not repeated key); `CliOptions::config_path` default removed (becomes `std::optional`); add free function `resolveConfigPath` declaration |
| `src/holonightd/Application.cpp` | Replace hand-rolled INI parser with toml++ calls; add `resolveConfigPath` implementation |
| `src/main.cpp` | Call `resolveConfigPath` to obtain the final path before calling `Config::fromFile` |
| `src/CMakeLists.txt` | Add `third_party` include directory to `holonightd_core` |
| `CMakeLists.txt` | Add `third_party/toml.hpp` to the `format` / `format-check` exclusion list (it is vendored, not owned source) |

### What is new

| Artifact | Purpose |
|---|---|
| `third_party/toml.hpp` | Single-header vendored copy of toml++ |
| `docs/sdd/xdg-config-toml/DESIGN.md` | This document |

### What is untouched

`Daemon`, `CommandRunner`, `FilesystemScanner`, `HealthCheckJob`, `LlmClient`, `Logger` — none of these depend on how the config is loaded. `main.cpp` changes are limited to the two lines between `parseArgs` and `Config::fromFile`.

---

## 2. XDG Path Resolution

### Where it lives

A free function in `Application.h` / `Application.cpp`, inside `namespace holonightd`. It is a pure utility — no class state needed.

```cpp
// include/holonightd/Application.h

/// Returns the resolved config file path according to XDG Base Directory spec.
/// Precedence:
///   1. explicit_override — if set, returned as-is (caller passed --config PATH)
///   2. $XDG_CONFIG_HOME/holonight/holonightd.toml
///   3. ~/.config/holonight/holonightd.toml
///
/// Does NOT verify that the path exists — that is Config::fromFile's job.
[[nodiscard]] std::filesystem::path resolveConfigPath(
    std::optional<std::filesystem::path> explicit_override = std::nullopt);
```

### Logic

```
resolveConfigPath(override):
  if override has value:
    return *override

  xdg_home = getenv("XDG_CONFIG_HOME")
  if xdg_home is non-empty:
    return path(xdg_home) / "holonight" / "holonightd.toml"

  home = getenv("HOME")
  if home is empty:
    throw std::runtime_error{"$HOME is unset; cannot resolve config path"}

  return path(home) / ".config" / "holonight" / "holonightd.toml"
```

`std::getenv` returns `nullptr` for unset variables. Both `nullptr` and `""` (empty string) are treated as "unset" for `XDG_CONFIG_HOME` — the XDG spec (§2) explicitly requires this.

`HOME` being unset is a hard error: there is no sensible fallback on a standard Linux system, and silently using a wrong path would be worse than failing fast.

### `CliOptions` change

```cpp
struct CliOptions {
    bool run_once{false};
    std::optional<std::filesystem::path> config_path;  // nullopt = use XDG default
};
```

The previous default `"config/holonightd.example.conf"` was a development convenience that is no longer appropriate once XDG is the contract. Callers that need the example config during development should pass `--config config/holonightd.example.conf` explicitly.

---

## 3. Config Loading Flow

```
main()
  │
  ├─ parseArgs(argc, argv)
  │     → CliOptions { run_once, config_path: optional<path> }
  │
  ├─ resolveConfigPath(options.config_path)
  │     → std::filesystem::path  (XDG or explicit override)
  │
  ├─ Config::fromFile(resolved_path)
  │     ├─ check exists → throw if missing
  │     ├─ open file (ifstream)
  │     ├─ toml::parse(file_stream)
  │     │     → toml::table
  │     ├─ extract [general] table
  │     └─ populate Config struct
  │           → Config { interval, scan_root, commands }
  │
  └─ Daemon{ config, logger }.run(stopSignal(), mode)
```

`resolveConfigPath` and `Config::fromFile` both throw `std::runtime_error` on failure. `main()` already catches `std::exception` and exits with code 1, so no additional plumbing is needed.

---

## 4. toml++ Integration

### Header location

```
third_party/toml.hpp
```

The single-header amalgam from https://github.com/marzer/tomlplusplus (tag `v3.x.x`, file `toml.hpp`). This file is checked into the repository. No FetchContent, no submodule.

Rationale: the project's constraint is no external *runtime* dependencies. toml++ is a header-only library. Vendoring the single header satisfies the spirit of that constraint — the binary ships no dynamic dependencies — while keeping the build hermetic and offline-capable.

### CMake wiring

In `src/CMakeLists.txt`, add the `third_party` directory as a system include on `holonightd_core` so that compiler warnings from the vendored header are suppressed:

```cmake
target_include_directories(holonightd_core
    PUBLIC  "${PROJECT_SOURCE_DIR}/include"
    SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party"
)
```

`SYSTEM PRIVATE` means:
- `SYSTEM`: suppress warnings from headers inside this directory (clang-tidy and `-Wall` will not flag toml++ internals).
- `PRIVATE`: downstream targets (the test binary, the executable) do not inherit this include path. Only `holonightd_core` and anything it `PUBLIC`-exposes uses it. Since toml++ is an implementation detail of `Application.cpp` — not part of the public API — this is correct.

### clang-format exclusion

`CMakeLists.txt` glob patterns for `format` / `format-check` use `${PROJECT_SOURCE_DIR}/include` and `${PROJECT_SOURCE_DIR}/src`. `third_party/` is not covered, so `toml.hpp` will not be reformatted. No change required.

### clang-tidy exclusion

The `tidy` custom target in `CMakeLists.txt` lists source files explicitly. `third_party/toml.hpp` is not listed. No change required. The `HOLONIGHTD_ENABLE_CLANG_TIDY` path uses `CMAKE_CXX_CLANG_TIDY` which runs on compiled translation units; since `toml.hpp` is only included by `Application.cpp`, clang-tidy will analyse the instantiated template code through that TU. To prevent false positives from toml++ internals, add a `.clang-tidy` suppression at the project root or use a `// NOLINTBEGIN` guard around the include in `Application.cpp`:

```cpp
// NOLINTBEGIN
#include <toml.hpp>
// NOLINTEND
```

### toml++ API used

```cpp
#include <toml.hpp>

// parse — throws toml::parse_error (derives from std::exception) on malformed input
auto tbl = toml::parse(file_stream, path.string());

// access with type-checked value_or for optional fields
auto& general = *tbl["general"].as_table();

int interval_sec = general["interval_seconds"].value_or(300);
std::string scan_root_str = general["scan_root"].value_or(".");

// iterate array
if (auto* arr = general["commands"].as_array()) {
    for (auto& elem : *arr) {
        if (auto* str = elem.as_string()) {
            config.commands.push_back(str->get());
        }
    }
}
```

`toml::parse_error` is a subclass of `std::exception`, so the existing `catch (const std::exception&)` in `main()` handles it transparently.

---

## 5. `Config` Struct Changes

The struct itself in `Application.h` does not need new fields — the existing three fields map directly to the new TOML schema:

| TOML key | C++ field | Type | Default |
|---|---|---|---|
| `[general].interval_seconds` | `interval` | `std::chrono::seconds` | 300s |
| `[general].scan_root` | `scan_root` | `std::filesystem::path` | `"."` |
| `[general].commands` | `commands` | `std::vector<std::string>` | `{}` |

The only structural change is to `CliOptions` (see Section 2). The `Config` struct itself is unchanged.

---

## 6. Error Handling Strategy

All errors are reported as `std::runtime_error` (or a subclass such as `toml::parse_error`). The existing `main()` catch block writes `error.what()` to the logger and returns 1. No new exception types are introduced.

### Message formats

| Condition | Message |
|---|---|
| `$HOME` unset | `$HOME is unset; cannot resolve config path` |
| Config file does not exist | `config file not found: /path/to/holonightd.toml` |
| Config file cannot be opened | `failed to open config file: /path/to/holonightd.toml` |
| TOML parse error | toml++ provides a detailed message including line/column, e.g. `expected value, found 'x' at line 3, column 7 of '/path/to/file'` |
| Missing `[general]` table | `config is missing required [general] table` |
| `interval_seconds` not positive | `interval_seconds must be a positive integer` (same text as today) |

The "file not found" check is performed explicitly in `Config::fromFile` before attempting to open, so the message distinguishes "does not exist" from "exists but unreadable":

```cpp
if (!std::filesystem::exists(path)) {
    throw std::runtime_error{"config file not found: " + path.string()};
}
std::ifstream input{path};
if (!input) {
    throw std::runtime_error{"failed to open config file: " + path.string()};
}
```

---

## 7. Test Strategy

### Existing test

`tests/test_application.cpp` — `ApplicationTest.ConfigParserReadsKnownKeys` — writes a temp `.conf` file and calls `Config::fromFile`. This test must be **replaced** (not just updated), because:

- It tests the INI format which is being removed.
- The new test must write a `.toml` file.

### New tests

All in `tests/test_application.cpp`.

#### `Config::fromFile` tests

| Test name | What it checks |
|---|---|
| `ConfigFromFileReadsAllFields` | Full valid TOML with all three keys; asserts interval, scan_root, commands count |
| `ConfigFromFileDefaultsWhenFieldsAbsent` | `[general]` present but fields omitted; asserts defaults |
| `ConfigFromFileThrowsWhenMissing` | Non-existent path; asserts `std::runtime_error` is thrown |
| `ConfigFromFileThrowsOnMalformedToml` | Syntactically invalid TOML; asserts exception is thrown |
| `ConfigFromFileThrowsOnNonPositiveInterval` | `interval_seconds = 0`; asserts `std::runtime_error` |
| `ConfigFromFileParsesMultipleCommands` | `commands = ["a", "b", "c"]`; asserts `commands.size() == 3` |

#### `resolveConfigPath` tests

Environment variable isolation is done with a small RAII helper written inline or in a shared test utility:

```cpp
struct EnvGuard {
    std::string name_;
    std::optional<std::string> saved_;

    EnvGuard(std::string name, std::optional<std::string> value)
        : name_(std::move(name)) {
        const char* existing = std::getenv(name_.c_str());
        saved_ = existing ? std::optional{std::string{existing}} : std::nullopt;
        if (value) {
            ::setenv(name_.c_str(), value->c_str(), 1);
        } else {
            ::unsetenv(name_.c_str());
        }
    }

    ~EnvGuard() {
        if (saved_) {
            ::setenv(name_.c_str(), saved_->c_str(), 1);
        } else {
            ::unsetenv(name_.c_str());
        }
    }
};
```

`setenv` / `unsetenv` are POSIX, available on Linux. Tests use `EnvGuard` to set/unset `XDG_CONFIG_HOME` and `HOME` for the duration of each test case.

| Test name | Setup | Assertion |
|---|---|---|
| `ResolveConfigPathExplicitOverride` | Pass explicit path; `XDG_CONFIG_HOME` set to something else | Returns the explicit path unchanged |
| `ResolveConfigPathUsesXdgConfigHome` | `XDG_CONFIG_HOME=/custom`; no override | Returns `/custom/holonight/holonightd.toml` |
| `ResolveConfigPathFallsBackToHome` | Unset `XDG_CONFIG_HOME`; `HOME=/home/user` | Returns `/home/user/.config/holonight/holonightd.toml` |
| `ResolveConfigPathEmptyXdgFallsBack` | `XDG_CONFIG_HOME=""`; `HOME=/home/user` | Returns `/home/user/.config/holonight/holonightd.toml` |
| `ResolveConfigPathThrowsWhenHomeUnset` | Unset both `XDG_CONFIG_HOME` and `HOME` | Throws `std::runtime_error` |

---

## 8. Key Decisions with Rationale

### Single-header vendoring over FetchContent

FetchContent for toml++ would require network access at configure time and would pull in the full repository including tests and examples. The single header is ~2 000 lines of self-contained C++17-compatible code. Vendoring it keeps the build offline-capable and deterministic, consistent with the project's no-external-runtime-dependencies stance.

### `std::optional<std::filesystem::path>` for `CliOptions::config_path`

The previous field held a default of `"config/holonightd.example.conf"`. That default was a development convenience, not a contract. Making it `std::nullopt` by default forces the explicit `resolveConfigPath` call and prevents `main()` from silently loading a wrong file in production.

### XDG-only default, no fallback to CWD

Following the XDG Base Directory Specification means the daemon behaves predictably regardless of the working directory it is launched from (systemd units, cron jobs, manual invocations). CWD-relative config paths are a source of subtle bugs in long-running daemons.

### Hard error on missing config

A daemon that starts with no configuration is likely to behave incorrectly (empty `commands`, wrong `scan_root`). Failing loudly with a clear message is safer than silently running with defaults.

### `[general]` table wrapper

Grouping all fields under `[general]` makes the TOML file extensible: future sections (`[llm]`, `[notifications]`) can be added without changing the structure of the existing section or breaking the parser.

### Reuse existing exception hierarchy

Introducing a custom `ConfigError` exception type would add complexity with no benefit: the existing `std::runtime_error` path already prints `what()` and exits. toml++'s own `toml::parse_error` is already a subclass of `std::exception`. Custom types are reserved for cases where callers need to catch-and-recover from a specific error type, which is not true here.

---

## 9. Alternatives Considered

### Alternative: `nlohmann/json` with JSON config

JSON is widely supported but is not human-friendly as a config format (no comments, verbose syntax for scalars). TOML is purpose-designed for config files and is a better fit. Rejected.

### Alternative: `libconfig` or `libini`

Both are dynamic library dependencies, violating the no-external-runtime-dependencies constraint. Rejected.

### Alternative: keep the INI parser and extend it for arrays

The INI parser would need repeated keys for `commands` (as it does today with `command=`), which is non-standard and surprising. TOML's native array syntax is cleaner. The INI parser is ~50 lines of custom code that becomes dead weight once toml++ is available. Rejected.

### Alternative: environment variable `HOLONIGHTD_CONFIG` instead of `--config`

Environment variables are appropriate for containerised deployments but less discoverable than CLI flags. `--config` is already implemented; keeping it is simpler. Future work could honour `HOLONIGHTD_CONFIG` as a second-priority override between `--config` and XDG default if needed.

### Alternative: FetchContent for toml++

See Section 8. Rejected in favour of vendoring for offline-capable, hermetic builds.

### Alternative: merge `resolveConfigPath` into `main.cpp`

Keeping path resolution in `Application.cpp` (part of `holonightd_core`) makes it testable without linking against the executable. `main.cpp` is not compiled into `holonightd_core` and is therefore not covered by the existing test binary. This is the primary reason the function lives in the library.

---

## 10. Known Risks

### toml++ version drift

The vendored header is a point-in-time snapshot. If upstream fixes a security issue or a parsing bug, the project will not receive it automatically. Mitigation: record the exact commit hash and version tag in a comment at the top of `third_party/toml.hpp`, and add a note to the release checklist to check for toml++ updates.

### `setenv`/`unsetenv` in tests are not thread-safe

GTest can run test cases in the same process, potentially in parallel. Mutating process-global environment variables in tests creates a race condition. Mitigation: GTest's default is single-threaded test execution. Document this in the test file with a comment and do not use `--gtest_shuffle` + `--gtest_parallel` until the concern is addressed. Longer-term, abstract `getenv` behind an injectable function object in `resolveConfigPath` so tests can pass a fake environment without touching `environ`.

### `HOME` unset on unusual systems

Container images or restricted shells may not export `HOME`. The code raises `std::runtime_error`, which is caught in `main()` and printed to stderr. This is acceptable — the user must either set `HOME` or pass `--config` explicitly.

### clang-tidy false positives from toml++ instantiations

toml++ uses aggressive template metaprogramming. clang-tidy checks (`readability-*`, `modernize-*`) may fire on instantiated code from `Application.cpp`. The `NOLINTBEGIN/END` guard around the include (Section 4) suppresses these. If per-check suppression is needed, add the relevant check names to `.clang-tidy`'s `Checks` field as negations.
