# EDM-020 local verification — 2026-10-15

The audit baseline remains `c6a9f9e1a353322bf7a3430cdab8392890348ccb`.
Implementation starts from clean canonical `origin/main`
`569905495a8ec0089a62c86c1b39ca68953ae01e`. Planning umbrella baseline is
`758347bd58c4b77b5a5830e37ae69a2c72eb7dfd`; accepted preparation is `a776b40`.
There are no internal source providers. Bundle 2.0.0 and the canonical Daemon
JSON/CMake declarations match exactly; generated install profiles are current.

## Environment and execution

Build, static, runtime and Release checks use
`ghcr.io/lebedenko/holonight-deps@sha256:cfd5daeb5745c71e72062aa9926b4ca1636599987f93697a1bf79e17693768ec`,
image ID `sha256:17f90982a6c56a84d706706f75ebd91b0e123df2fa84d6f0d444f6eedcecd93d`.
Licensing retains
`docker.io/fsfe/reuse@sha256:85462a75c0f8efda09ddd190b92816b70e7662577c8427429e11e1b9f25a992e`,
image ID `sha256:2575b4ad3a736a14323c7705b708e0027109930a1511cce4e65cd09b9dfc1cc4`.
Docker 29.9.0, linux/amd64; both pinned images were explicitly pulled before
containers ran with `--pull=never --network=none`. No image rebuild occurred.
Source snapshots are read-only inputs, copied into fresh disposable filesystems;
existing host builds, services and user data are untouched. Snapshot Git indexes
support canonical owned-source enumeration; rootless Podman mapping has fixture
coverage, but actual execution used Docker.

Qualified image identities: GCC 16.2.1, CMake 4.4.3, Ninja 1.13.2, Python 3.14.7,
Task 3.53.1, clang-format/clang-tidy 22.1.8, SQLite 3.53.4, JSON 3.12.0,
toml++ 3.4.0, systemd 262, GTest 1.18.0, gcovr package 8.6-1 (reported development
version `8.6.post1.dev0+ge01ad7358.d20260115`). REUSE reports 6.2.0.
Complete package/toolchain identities, package-config signatures and feature/cache
options are retained in each run's provenance, CMakeCache and dependency-state.
Commands run with `JOBS=8`; no configure/build/test command acquires dependencies.

## Accepted checks

| Check | Command | Result |
|---|---|---|
| Launcher | `python3 scripts/ci/test_launcher.py` | 5 pass: isolation, deletions, permissions, rootless mapping, offline run flags, preparation/lane failures |
| Real CMake/cache | `python3 scripts/ci/test_dependencies.py` in pinned offline image | 11 pass; final repeat passes after removing an unused Python import |
| Canonical tooling | `python3 tooling/test_external.py`; `python3 tooling/test_tooling.py` from umbrella | 21 + 11 pass |
| Installer fixtures | `python3 -m unittest discover -s tests -p 'test_install_*.py'` from umbrella | 68 pass, no skips |
| Declarations/profiles | `python3 tooling/sync.py --check --module holonightd`; `python3 tooling/generate-install-profile.py --check`; byte comparison of canonical module JSON/CMake | Pass |
| Debug/systemd ON | `task deps PRESET=test`; `task test PRESET=test` | Clean build, 111/111 CTest, no skips |
| Debug/systemd OFF | Same commands with `HOLONIGHT_BUILD_FEATURES=BUILD_SYSTEMD=OFF` | Final clean build, 111/111 CTest, no skips; no systemd definitions or linkage |
| Release/tests OFF | `task deps PRESET=release`; `task build PRESET=release`; `DESTDIR=/work/install cmake --install build/release`; `python3 scripts/ci/check_install.py /work/install/usr` | Pass |
| Coverage | `task deps PRESET=coverage`; `task coverage` | All 40 translation units contain `--coverage`; 111/111 tests; HTML report generated |
| Owned static | `task build PRESET=test`; `task format-check`; `task tidy-src`; `task tidy-tests` | Final pass, 25 production and 15 test translation units, warnings as errors |
| Licensing | `reuse --version`; `reuse lint` in pinned REUSE image | Pass; final documentation also checked locally |
| Final diff | `git diff --check`, staged review | Pass |

Dependency fixtures cover missing JSON/TOML/GTest, toml++ <3.4, GTest <1.14,
missing GMock target, tests OFF, required systemd ON, systemd OFF without pkg-config,
explicit external prefixes containing spaces, and incompatible-cache preservation
across feature changes. No `_deps` acquisition tree appears. Canonical fixtures
also cover no implicit preparation, separate internal/external prefixes, compiler,
package-export and source identity invalidation, and installed-provider signatures.
The cache fixture uses real preserved directories and mocked prerequisite/build
calls; package discovery fixtures invoke the actual daemon CMake project.

Release staging contains the four existing executables, example config under
`usr/etc/holonight`, three unit files and the services target. `ldd` resolves every
binary without missing or source/build-path dependencies. From an unrelated temporary
directory, healthd/event CLI help and a healthd once-run with temporary configuration
pass. Agentd and hn-agent-run failure paths run with an unavailable private bus address
and temporary HOME/config/state, avoiding live notification or activation.
The original install layout and executable names are preserved.

## Runs, corrections and diagnostic review

Logs use UTC 2026-10-14 22:xx (local verification date 2026-10-15).
The initial run `build/ci/20261014T220211Z-qt_zrm7k` is diagnostic evidence, not
final acceptance: its build-test snapshot preceded the new dependency fixture,
its staging fixture expected `/etc` instead of the existing `/usr/etc` layout,
and REUSE detected the unused MIT license after vendor removal. All were corrected.
It also exposed unused Qt QML preset variables; those were removed.

Clean full acceptance command `JOBS=8 python3 scripts/ci/run.py` produced
`build/ci/20261014T220752Z-6lvu4b2v`: all six lanes passed. Review of complete logs
identified three compiler warnings in systemd OFF: notification formatting helpers
were unused. Those helpers are now guarded by `HOLONIGHTD_HAS_SYSTEMD`, preserving
both enabled behavior and disabled fallback behavior.

Final affected command:
`JOBS=8 python3 scripts/ci/run.py --lane systemd-off --lane coverage --lane static-checks --lane licensing`
produced `build/ci/20261014T220954Z-4k28z1_b`. This supersedes the original OFF,
coverage, static and licensing evidence. The enabled helper implementation is unchanged;
final coverage recompiles and tests systemd ON. Prior Release installation and Debug ON
acceptance remain applicable; their behavior is unaffected by the conditional helper guard.
The only subsequent non-documentation edit removed an unused Python test import;
all 11 dependency fixtures were rerun in the pinned offline image.

Final accepted builds have no compiler or CMake warnings. Invalid-configuration tests
intentionally print flapping-threshold/window fallback warnings and storage/health
fixture messages. The isolated Release smoke prints an informational absent-system-bus
message. Strict clang-tidy reports suppressed diagnostics in third-party/system headers
and existing NOLINT sites; no actionable owned-source diagnostics remain. No assertions
were removed, no suppressions were added, and no tests were skipped.

Source, image, log and artifact hashes are in [EVIDENCE.json](EVIDENCE.json).
Ignored complete logs and coverage artifacts stay in `build/ci`; their hashes make the
accepted snapshots identifiable without committing generated outputs.

## Handoff constraints

This is local verification. Publication, canonical-remote availability confirmation,
one exact-revision CI query, and umbrella pinning remain separate authorized steps.
No hosted CI result is claimed. No host deployment, service restart, live activation,
Debian qualification, or pointer/focus automation occurred. Hyprlock EDM-021 and final
integration EDM-022 remain Planned; the initiative remains Accepted.
