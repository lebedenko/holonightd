# Local CI rehearsal

Baseline: `281e824bbe107dfe1a1825bd34bf1d78a98f09b8`.
Umbrella work package: CI-001.

## Requirements and design

`task ci` validates current tracked edits and non-ignored untracked inputs, including
executable bits and symlinks. Deleted inputs are absent. Python creates one isolated
snapshot; each required lane receives it read-only and copies it into a fresh tmpfs.
No development artifact enters a build. Host-owned logs live under ignored `build/ci`.
Docker is preferred; Podman is used when Docker is absent. Required failures fail the
aggregate while allowing other lanes to report results.

The repository owns launcher, lane commands and immutable image references. GitHub
uses the same launcher, preserving separate build/test, static and licensing jobs
and their triggers. Build/test uses Debug, Ninja, compile commands and BUILD_TESTS=ON;
static checks configure independently and run format-check and full tidy. Licensing
uses the immutable REUSE 6.2.0 image. Registry publication and tagged releases retain
their existing remote workflows. No provider dependency is involved in this module.

## Files

Taskfile.yml and README.md expose the command. scripts/ci owns run.py, lane.sh,
images.json and test_launcher.py. Push validation workflows invoke the launcher.
.gitignore excludes Python bytecode produced by test discovery.

## Verification

2026-10-02: all three container lanes passed from a fresh snapshot; build/test ran
111 tests. Complete logs were reviewed, with no actionable warnings. Evidence:
`build/ci/20261002T184359Z-91mjr_oq/`. Launcher regression checks cover edited,
deleted, new, ignored and space-containing paths, symlinks and executable mode;
missing runtime and all-lane failure recording/continuation also pass.

Injected actual formatter, tidy naming, GTest assertion and missing licensing failures
in disposable fixture repositories: every lane returned nonzero, and logs identify
its corresponding check. Evidence: `build/ci/faults/`.

Repository commands: `task format-check` passed and `task test` passed 111 tests.
`task tidy-src` failed on pre-existing code with host clang-tidy 23.1.1 (systemd compound
literal and readability-trailing-comma diagnostics). Full tidy passed with clang
22.1.8 in the immutable CI image; host verification failure is retained explicitly,
with no unrelated product edits or diagnostic suppression.

Final acceptance: `task ci` passed all three lanes, including three launcher
regressions and 111 GTests. Full logs reviewed at
`build/ci/20261002T185021Z-qtfux1sn/`. Hashes, modes and modification times for
245 tracked source/development-build files were unchanged across the invocation.
Platform is fixed to linux/amd64 locally and remotely. Failure logs print to the
console as well as remaining in the run directory. Final metadata includes an
explicit dirty flag. These reporting-only adjustments are covered by launcher
regressions; they do not invalidate the build or static acceptance results.

## Host diagnostic corrections

The user requested fixing host diagnostics before rollout acceptance. Clang-tidy
23.1.1 findings were collected across all 25 source compilation entries before
editing. The fixes use standard C++ value initialization for sd_bus_error (all
fields remain null/zero), trailing commas in multiline enums/initializers,
using-declarations for nlohmann::json and merged short-circuit conditions. Public
APIs and runtime behavior remain unchanged; existing collector/config/rule tests
cover the affected conditions. No checks were disabled or suppressed.

Files: apps/agent-event/main.cpp, apps/agent-run/main.cpp, apps/agentd/main.cpp;
include/holonightd/{Logger.h,RuleEngine.h,agentd/AgentActivity.h};
libs/agent/{AgentActivity.cpp,NotificationBridge.cpp,ShellActivationClient.cpp};
libs/config/Application.cpp; libs/health/{MemoryCollector.cpp,PacmanCollector.cpp,
RuleEngine.cpp,StorageCollector.cpp,SystemdCollector.cpp};
libs/storage/ObservationEvent.cpp. Final verification, 2026-10-02: `task format-check`, `task tidy-src` and `task test`
all pass with host clang-tidy 23.1.1; all 111 tests pass. Logs:
`build/ci/host-format.log`, `build/ci/host-tidy-src.log`, `build/ci/host-test.log`.
Clean container build/test and full static lanes pass after these corrections:
`build/ci/20261002T185635Z-xe9lj91c/`. REUSE 6.2.0 passed the corrected files in
`build/ci/20261002T185437Z-t9qmgy34/licensing.log`; final documentation also passes
`reuse --no-multiprocessing lint`. Complete logs were reviewed. Suppressed host
third-party diagnostics contain no remaining actionable user-code findings.

No publication or submodule updates are authorized.


Rootless Podman uses `--userns=keep-id` so preserved private file modes remain
readable under the requested UID/GID. A fake-runtime launcher regression verifies
user mapping and the read-only input mount. Podman is not installed on this host;
its real-runtime integration is unverified. The verified Docker path is unchanged.
