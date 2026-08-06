# Usage Guide: `holonight-agentd` (AI Agent Activity Service)

`holonight-agentd` is a lightweight Linux user daemon that normalizes lifecycle events and notifications across AI coding CLI agents (`Claude Code`, `Codex`, `Kiro CLI`, `Antigravity CLI`), manages dynamic terminal window titles, and integrates with desktop notifications and shell widgets.

---

## 1. Component Overview

| Component | Type | Description |
| :--- | :--- | :--- |
| **`holonight-agentd`** | Daemon | Background service listening on D-Bus (`org.holonight.AgentActivity1`), maintaining session registries, and dispatching desktop notifications (`org.freedesktop.Notifications`). |
| **`holonight-agent-event`** | Ingest CLI | Lightweight event broker invoked by provider hooks (`Claude`, `Codex`, `Kiro`, `Antigravity`). Normalizes JSON hook payloads and publishes events via D-Bus. |
| **`hn-agent-run`** | Launcher CLI | Subprocess wrapper. Dynamically sets terminal title (`Provider · Project`), registers session PID/cwd with `holonight-agentd`, and restores terminal title upon exit. |

---

## 2. Building and Installation

### Local Build & Test
```bash
# Build Debug binary and run unit tests
task configure-tests
task test
```

### Local User Installation (`~/.local`)
```bash
# Build Release binaries and install to ~/.local/bin/ and ~/.local/share/systemd/user/
task install:local
```

### Enabling systemd User Service
```bash
# Reload user systemd daemon and enable holonight-agentd
systemctl --user daemon-reload
systemctl --user enable --now holonight-agentd.service

# Check service status
systemctl --user status holonight-agentd.service
```

---

## 3. Dynamic Terminal Titles & Launcher (`hn-agent-run`)

Instead of invoking CLI agents directly, wrap them with `hn-agent-run`.

### Basic Usage
```bash
hn-agent-run claude
hn-agent-run codex
hn-agent-run kiro
hn-agent-run agy
```

### Recommended Shell Aliases
Add the following aliases to `~/.zshrc` or `~/.bashrc`:

```bash
alias claude="hn-agent-run claude"
alias codex="hn-agent-run codex"
alias kiro="hn-agent-run kiro"
alias agy="hn-agent-run agy"
```

---

## 4. Agent Lifecycle Hook Configuration

### A. Claude Code Configuration (`~/.claude.json`)
Add `Notification` and `Stop` hooks in `~/.claude.json` to automatically forward turn status and approval prompts to `holonight-agentd`:

```json
{
  "hooks": {
    "Notification": [
      {
        "matcher": "",
        "hooks": [
          {
            "type": "command",
            "command": "holonight-agent-event --provider claude"
          }
        ]
      }
    ],
    "Stop": [
      {
        "matcher": "",
        "hooks": [
          {
            "type": "command",
            "command": "holonight-agent-event --provider claude --event completed"
          }
        ]
      }
    ]
  }
}
```

### B. Kiro CLI Configuration (`~/.kiro/hooks/holonight.json`)
Create a global hook definition in `~/.kiro/hooks/holonight.json`:

```json
{
  "version": "v1",
  "hooks": [
    {
      "name": "holonight-turn-complete",
      "trigger": "AgentTurnComplete",
      "action": {
        "type": "command",
        "command": "holonight-agent-event --provider kiro --event completed"
      },
      "enabled": true
    }
  ]
}
```

### C. Codex Lifecycle Hooks
Register a Codex command hook to notify `holonight-agent-event` on turn completion:

```bash
codex hook --command "holonight-agent-event --provider codex"
```

### D. Antigravity CLI / Custom Pipe Ingestion
Piping custom or machine-readable event JSON to `holonight-agent-event`:

```bash
echo '{"session_id":"sess-101","cwd":"/home/user/project","title":"Approval Required","message":"Execute command: cmake --build build","notification_type":"permission_prompt"}' | \
  holonight-agent-event --provider antigravity
```

---

## 5. D-Bus Interface Reference

- **Service Bus**: User Session Bus (`sd_bus_open_user`)
- **Bus Name**: `org.holonight.AgentActivity1`
- **Object Path**: `/org/holonight/AgentActivity1`
- **Interface**: `org.holonight.AgentActivity1`

### Methods

| Method | Signature | Description |
| :--- | :--- | :--- |
| `RegisterSession` | `(ssuss) -> s` | `(provider, session_id, pid, cwd, metadata_json)` → Returns session ID. |
| `PublishEvent` | `(sssssss) -> b` | `(session_id, provider, cwd, event_type, state_str, title, message)` → Publishes event. |
| `EndSession` | `(ss) -> b` | `(session_id, result_status)` → Terminates active session. |

### Signals Emitted

| Signal | Signature | Description |
| :--- | :--- | :--- |
| `SessionAdded` | `(sss)` | `(session_id, provider, project_path)` |
| `SessionChanged` | `(ssss)` | `(session_id, state_str, title, message)` |
| `SessionRemoved` | `(ss)` | `(session_id, result_status)` |
| `AttentionRequested` | `(ssss)` | `(session_id, provider, title, message)` |

### Real-Time D-Bus Signal Inspection
Monitor D-Bus events using standard CLI tools:

```bash
dbus-monitor --session "type='signal',interface='org.holonight.AgentActivity1'"
```
