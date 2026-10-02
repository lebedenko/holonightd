# holonightd

`holonightd` is a C++23 Linux daemon workspace with two focused services:

- `holonight-healthd` collects system health observations, persists them in SQLite, evaluates diagnostic rules, and runs configured maintenance checks.
- `holonight-agentd` tracks AI coding-agent sessions over the user D-Bus and forwards desktop notifications.

There is no GUI stack and no Qt/QML dependency.

## Build

```sh
task build
```

## Run

```sh
task run
```

Without `--once`, the daemon loops until it receives `SIGINT` or `SIGTERM`.

## Test, Format, Lint

```sh
task test
task format-check
task tidy-src
```

Apply formatting in-place:

```sh
task format
```

## Current Structure

- `apps/healthd`: health daemon CLI and signal handling.
- `apps/agentd`: agent-activity D-Bus service.
- `apps/agent-event`: provider-hook event ingestion CLI.
- `apps/agent-run`: supervised agent launcher and terminal-title wrapper.
- `include/holonightd`: public component APIs.
- `libs/config`, `libs/logging`, `libs/process`: shared infrastructure.
- `libs/storage`: normalized observation schema and thread-safe SQLite event store.
- `libs/health`: collectors, diagnostic rules, daemon orchestration, and local summaries.
- `libs/agent`: agent-event normalization, session registry, and desktop notifications.
- `tests/`: unit tests using Google Test (GTest).

## Configuration

By default the daemon reads its config from:

```
$XDG_CONFIG_HOME/holonight/holonightd.toml
```

If `XDG_CONFIG_HOME` is unset or empty, it falls back to `~/.config/holonight/holonightd.toml`.

Pass `--config PATH` to override the resolved path entirely.

Config is TOML with a `[general]` section and optional `[storage]`, `[memory]`, and `[database]` sections:

```toml
[general]
interval_seconds = 300
scan_root = "."
log_level = "info"  # "debug", "info", "warn", "error"
commands = [
    "cmake --build build",
    "ctest --test-dir build/test --output-on-failure",
]

[storage]
warning_threshold = 85.0
critical_threshold = 95.0
# mount_points = ["/", "/home"]

[memory]
some_warning_threshold = 10.0
full_critical_threshold = 25.0
meminfo_warning_threshold = 85.0

[database]
# path = "/custom/path/events.db"  # Defaults to $XDG_DATA_HOME/holonight/events.db or ~/.local/share/holonight/events.db
retention_days = 30                # Default: 30 days
max_bytes = 52428800               # Default: 50 MB (52,428,800 bytes)
max_events = 100000                # Default: 100,000 events
```

`commands` is optional and defaults to an empty list. Each command is limited to 60 seconds and 1 MiB of combined output; shutdown requests cancel a running command. `interval_seconds` and `scan_root` are required. Storage and memory percentages must be between 0 and 100, and the storage warning threshold cannot exceed its critical threshold. `[database]` configures the SQLite `EventStore` path, age retention period, and capacity limits (`max_bytes`, `max_events`). A missing or malformed config file is a hard error.

See [docs/holonight-agentd-usage.md](docs/holonight-agentd-usage.md) for agent-service installation, hook configuration, and D-Bus usage.

## Logging & Level Precedence

`holonightd` logs natively to **systemd journal** (`sd_journal_send`) with structured syslog metadata (`MESSAGE`, `PRIORITY`, `SYSLOG_IDENTIFIER=holonightd`).

Log levels are resolved in strict order of precedence:
1. `--debug` / `-d` CLI flag (forces `DEBUG` level and redirects output to `stdout`)
2. `HOLONIGHTD_LOG_LEVEL` environment variable (`debug`, `info`, `warn`, `error`)
3. `log_level` setting in `holonightd.toml`
4. Build-type default (`DEBUG` for Debug builds, `INFO` for Release builds)

Passing `--debug` or `-d` forces log level to `debug` and redirects log output from systemd journal to standard output (`stdout`) formatted as `YYYY-MM-DDTHH:MM:SS%z LEVEL message`.

## Standalone developer tooling

See [tooling/README.md](tooling/README.md) for presets, local dependency overrides, editor refresh,
`task tooling:doctor`, and the independent Serena project.

## Rehearse push CI locally

Run `task ci` with Python 3, Git, Task, and Docker (or Podman when Docker is absent).
An accessible container daemon and an amd64 host (or configured amd64 emulation)
are required, and registry access is needed on the first run. All
three required lanes run: a fresh Debug/Ninja build and all CTest tests, independent
format/full tidy checks, and REUSE 6.2.0 licensing. Local and GitHub jobs use the
same launcher, lane script, and immutable image references in `scripts/ci/images.json`.

The snapshot includes current tracked files and non-ignored new files, omits deleted
files and ignored outputs, and reports new inputs to add before pushing. Host inputs
are mounted read-only; each lane copies them into a disposable writable container
filesystem. Existing development builds are untouched. Application build artifacts
are never reused; downloaded container layers may be cached. The image publication
and release workflows remain remote operations.

Each run prints its ignored `build/ci/<run>/` directory with full lane logs and
`results.json` recording revision, dirty status, untracked inputs, runtime version,
image digests and exit codes. Tool versions appear in the lane logs. Any required
lane failure or missing runtime returns nonzero. Failed lane logs are also printed
to the console, including in GitHub jobs. Use
`python3 scripts/ci/run.py --lane static-checks` to investigate one lane; only the
unfiltered `task ci` command rehearses all required push validation.

Run launcher regressions with `python3 scripts/ci/test_launcher.py`.
