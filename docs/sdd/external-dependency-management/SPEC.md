# External dependency management — holonightd

Work package: EDM-020
Baseline: `c6a9f9e1a353322bf7a3430cdab8392890348ccb`
Umbrella audit: `38146d8f474317f3f8d72699f60e6d16a71dd165`
Implementation baseline: `569905495a8ec0089a62c86c1b39ca68953ae01e` (clean canonical origin/main).
Planning umbrella baseline: `758347bd58c4b77b5a5830e37ae69a2c72eb7dfd`.
Accepted preparation checkpoint: `a776b40`.
Status: Verified locally; repository commit and coordinator handoff are local only.

## Requirements

- Configuration and normal build commands shall never acquire dependencies or prepare source providers.
- `task deps` shall validate installed external prerequisites. Daemon has no internal source providers.
- Internal providers shall resolve only from approved prefixes; external packages shall resolve from an explicit
  external prefix or normal system locations.
- Enabled dependency-dependent features shall require their dependencies; minimal profiles shall disable them explicitly.
- Where production components use Qt, discovery shall require >=6.11; private Qt components shall match
  the selected public version exactly.
- Where tests use GTest, consumers shall require installed GTest >=1.14 CONFIG and any GMock targets they use.
- Existing public APIs, component switches, install-tree consumers and acceptance coverage shall be preserved.

## Design

Adopt canonical `tooling/modules/holonightd.json` and `.cmake`, then synchronize bundle 2.0.0 into this repository.
The copied metadata is owned by this standalone checkout. CMake performs authoritative discovery; shared probes
provide prerequisite diagnostics and record resolved versions/paths. Incompatible caches and local provider install
trees are archived before reuse. Package acquisition occurs only in explicit environment preparation. CI switches
build/runtime/release lanes together to qualified immutable images and runs acceptance offline after preparation.

Replace vendored nlohmann/json and toml++ with installed imported targets; update TOML includes, attribution and REUSE metadata. Add BUILD_SYSTEMD default ON with required libsystemd, and verify explicit OFF. Remove the GTest download fallback; require GMock.

## Implementation and verification tasks

- [x] Confirm the exact baseline and canonical handoff before implementation.
- [x] Adopt declarations, prefix helpers, explicit preparation and compatibility signatures.
- [x] Apply the repository-specific discovery changes above.
- [x] Update Task, CI preparation/entrypoints, Dockerfiles, release lanes and contributor instructions.
- [x] Run declaration/CMake/profile consistency checks and offline missing-package regressions.
- [x] Run narrow regressions, clean acceptance, installed-package consumers and required quality checks.
- [x] Inspect complete logs; record commands, versions, features, warnings and manual checks.
- [x] Make a repository-local commit only after local verification; publish only when authorized.

## Implementation details

- Bundle 2.0.0 is copied from canonical tooling, with identical module declarations and provider snippet.
  Task/deps performs probes only; configure/build/test never call dependency preparation or acquisition.
  External prefixes and feature/toolchain/package signatures participate in compatibility decisions;
  incompatible build directories are renamed and preserved.
- Installed JSON/toml++ imported targets replace both vendored headers. The TOML include is
  `<toml++/toml.hpp>`. Removed vendored notices/REUSE overrides and the now-unused MIT license text.
  No production behavior or test assertions changed. Notification formatting helpers are compiled
  only with systemd support, resolving unused-function warnings in explicit OFF builds.
- Installed GTest >=1.14 CONFIG must provide gtest, gtest_main and gmock. No FetchContent remains.
- BUILD_SYSTEMD defaults ON and requires libsystemd; OFF bypasses its discovery, compile definition
  and all explicit links, preserving stdout, standalone and notification/activation fallbacks.
  Service-unit installation remains independent of the feature.
- C++23, four executable names, compatibility build symlink, presets and install layout are preserved.
  Removed unused Qt QML preset variables, which generated configure warnings in this Qt-free project.
- CI uses the qualified Qt-free image
  `ghcr.io/lebedenko/holonight-deps@sha256:cfd5daeb5745c71e72062aa9926b4ca1636599987f93697a1bf79e17693768ec`
  and retains REUSE
  `docker.io/fsfe/reuse@sha256:85462a75c0f8efda09ddd190b92816b70e7662577c8427429e11e1b9f25a992e`.
  Images are explicitly pulled before containers use `--pull=never --network=none`.
  Snapshot isolation, rootless Podman mapping and user permissions are preserved; an isolated Git
  index makes canonical owned-source checks work in snapshots without host Git metadata.
- Six clean lanes cover Debug/systemd ON, Debug/systemd OFF, Release/tests OFF staged install,
  coverage, full owned-source/test static checks and REUSE. Release's build gate invokes the same
  offline launcher. Obsolete project-image publication, Dockerfile and mutable-image tasks are retired.
- Coverage instruments concrete libraries, applications and the test executable; static libraries
  propagate coverage linkage to their consumers. Task coverage builds, executes tests and generates
  the HTML report; the coverage build preset builds actual binaries before report generation.

## Verification contract

Use focused launcher and real-CMake regressions before clean acceptance. Missing JSON/TOML/GTest,
old TOML/GTest and incomplete GMock packages must fail without acquisition; tests OFF and systemd OFF
must avoid disabled discovery. Exercise installed prefixes containing spaces and incompatible-cache
preservation after feature changes. Clean acceptance uses `JOBS=8 python3 scripts/ci/run.py`.
The Release stage must contain all four binaries, the example config under `usr/etc/holonight`
(the existing layout), three service units and the services target. Check ldd from the stage and run
CLI/once fixtures with temporary HOME/state/config and an unavailable private bus address.
No host installation, live activation, service restart or desktop automation is permitted.

Full acceptance results, environment identities, reviewed diagnostics and command evidence are
recorded in VERIFICATION.md and EVIDENCE.json after checks complete. Publication and umbrella
pinning remain separate authorized actions. Hyprlock/final integration remain Planned and the
initiative remains Accepted.
