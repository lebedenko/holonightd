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
    "ctest --test-dir build --output-on-failure",
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
