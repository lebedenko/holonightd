# AGENTS.md — AI Agent Guidelines for holonightd

Use Conventional Commits for every new commit: `type(scope): imperative summary`, or `type: imperative summary` when a scope adds no clarity.

This document provides guidelines, architectural details, and operational rules for AI coding agents operating on the `holonightd` codebase.

---

## 1. Project Overview

`holonightd` is a lightweight Linux daemon written in **C++23** designed for routine maintenance automation:
- **Filesystem Scans**: Scanning directories and counting/monitoring targets.
- **Job Orchestration**: Running configurable shell commands on fixed intervals.
- **LLM Integration**: Summarizing and classifying system results using LLM clients.

### Architectural Philosophy
- **Zero GUI/Qt Dependencies**: Fully self-contained C++23 daemon with minimal runtime overhead.
- **Modern C++ Idioms**: Standard library usage (`std::filesystem`, `std::optional`, `std::expected`, ranges, RAII).
- **Safety & Robustness**: Proper signal handling (`SIGINT`/`SIGTERM`), structured configuration parsing, zero raw owning pointers.

---

## 2. Codebase Layout

```
.
├── include/holonightd/    # Public C++ header files (#pragma once)
│   ├── Application.h      # Configuration structures and path resolution
│   ├── CommandRunner.h    # Shell command execution wrapper
│   ├── Daemon.h           # Core daemon event loop & signal control
│   ├── FilesystemScanner.h# Utility for directory scanning
│   ├── HealthCheckJob.h   # Periodic check job execution & summary builder
│   ├── LlmClient.h        # LLM client interface & local stub
│   └── Logger.h           # Stream logger
├── src/
│   ├── main.cpp           # CLI entrypoint, argument parsing, & signal handlers
│   └── holonightd/        # Implementation files (.cpp)
├── tests/                 # Unit tests using Google Test (GTest)
│   ├── test_application.cpp
│   ├── test_filesystem_scanner.cpp
│   ├── test_health_check_job.cpp
│   └── test_llm_client.cpp
├── config/                # Example configuration files (holonightd.example.toml)
├── docs/                  # Project specifications, SDD, and ROADMAP.md
├── scripts/               # Maintenance and helper scripts
├── Taskfile.yml           # Task runner configuration (task)
├── CMakeLists.txt         # Root CMake build configuration
├── .clang-format          # C++ formatting rules
└── .clang-tidy            # Static analysis checks
```

---

## 3. Build & Task Reference

All standard operations are managed via [`go-task`](https://taskfile.dev) (`task`).

### Configuration & Build
```sh
task configure        # Debug build (no tests)
task configure-tests  # Debug build with GTest enabled
task build            # Compile Debug build
task build:release    # Configure & compile Release build
task clean            # Wipe build directory
```

### Execution
```sh
task run              # Run holonightd once with config/holonightd.example.toml
```

### Testing & Coverage
```sh
task test             # Configure-tests, compile, and run CTest suite
task coverage         # Generate gcovr HTML coverage report
```

### Formatting & Static Analysis
```sh
task format           # Format C++ sources in-place with clang-format
task format-check     # Validate formatting without mutating files
task tidy             # Run clang-tidy across all sources and tests
task tidy-src         # Run clang-tidy on src/ only
task tidy-tests       # Run clang-tidy on tests/ only
task tidy-container   # Run format & tidy checks inside CI container (Docker/Podman)
```

---

## 4. Coding Standards & Guidelines

### C++ Specification & Language Rules
- **Standard**: Strictly target C++23 (`-std=c++23`).
- **Memory & Resource Management**: Use RAII for all resource lifecycles. Do **not** use raw owning pointers (`new`/`delete`). Prefer `std::unique_ptr`, `std::shared_ptr`, or stack allocation.
- **Modern Features**: Prefer standard C++ features such as `std::optional`, `std::expected`, ranges, structured bindings, and `<filesystem>`.
- **No Magic Numbers**: Define constants or enumerations for configuration limits, timeout values, and magic constants.

### Naming Conventions
- **Classes / Structs / Interfaces**: `CamelCase` (e.g., `HealthCheckJob`, `FilesystemScanner`)
- **Functions / Methods**: `camelBack` (e.g., `resolveConfigPath`, `stopSignal`)
- **Variables / Members**: `lower_case` or `snake_case` (e.g., `interval_seconds`, `config_path`)
- **Constants**: `CamelCase` or `kCamelCase`
- **Namespaces**: `holonightd`

### Header & Source Organization
- Use `#pragma once` in all headers under `include/holonightd/`.
- Include headers using relative paths from the include root (e.g., `#include "holonightd/Daemon.h"`).
- Maintain 120-character maximum line length as configured in `.clang-format`.

### Error Handling
- Return structured error types (`std::expected` or custom error objects) or throw explicit standard exceptions (`std::runtime_error`, `std::invalid_argument`).
- Avoid swallowing exceptions or returning dummy fallback data without logging or surfacing errors.

---

## 5. Testing & Quality Assurance

- **Framework**: Google Test (`GTest`).
- **Test Placement**: Put new unit tests in `tests/test_<component>.cpp`.
- **Test Naming**: Use descriptive suite and test case names (`TEST(ConfigFromFile, ReadsAllFields)`).
- **Assertions**: Use appropriate assertion macros (`EXPECT_EQ`, `EXPECT_TRUE`, `ASSERT_THROW`, etc.).
- **Coverage**: Ensure new features are covered by unit tests. Check coverage using `task coverage`.

---

## 6. Commit Conventions

Follow [Conventional Commits](https://www.conventionalcommits.org/):

- `feat: <description>` — New user-facing feature or core capability
- `fix: <description>` — Bug fix
- `chore: <description>` — Build system, CI, dependencies, or formatting updates
- `docs: <description>` — Documentation changes
- `test: <description>` — Adding or updating test cases
- `refactor: <description>` — Code restructuring without functional changes

---

## 7. Agent Operational Protocol

When working on this repository, AI agents must adhere to the following workflow rules:

1. **Verify Before Completion**:
   - Always run `task format` (or `task format-check`), `task tidy-src`, and `task test` before declaring any task complete.
2. **Root Cause Analysis**:
   - Do not mask failing tests, suppress exceptions blindly, or delete test assertions. Inspect compiler output and test failure tracebacks to address the underlying issue.
3. **No Speculative Implementation**:
   - Always inspect existing header declarations and types in `include/holonightd/` before introducing or modifying API calls.
4. **Preserve Documentation**:
   - Maintain existing docstrings and header comments. Update documentation whenever modifying signatures or behaviors.

Developer tooling uses `build/debug`, `build/test`, `build/release` and module-owned `build/deps`.
See tooling/README.md; run task tooling:refresh explicitly after configuring/building for editor metadata.
