## Q1

I'm working with Codex, Claude Code, Kiro CLI, Antigravity CLI.
The problem:

1. Codex and Claude Code send notifications when they finished turn or waiting for user approval, but Antigravity and Kiro do not.
2. Claude Code says in title that the message from Claude Code, but Codex does not, there's a terminal program name in the title.

Is there any solution that could be implemented in HoloNight? Maybe some integration through ACP to get notifications from all 4 and unify those notifications to make all agents act the same way?

## A1

Yes. This makes sense as a **HoloNight Agent Activity service**, but I would not build the first version primarily around ACP.

ACP is excellent when HoloNight itself acts as the agent’s client. It does not automatically observe an ordinary interactive CLI already running inside Ghostty, Kitty, or another terminal.

## Recommended architecture

```text
Codex hooks ──────────┐
Claude Code hooks ────┤
Kiro hooks ───────────┤
Antigravity hooks ────┤──▶ holonight-agentd ─▶ D-Bus ─▶ HoloNight Shell
ACP sessions ─────────┤
PTY fallback adapter ─┘
```

`holonight-agentd` would normalize provider-specific events into one internal model:

```cpp
enum class AgentState {
    Starting,
    Working,
    WaitingForApproval,
    WaitingForInput,
    Completed,
    Failed,
    Cancelled
};

struct AgentEvent {
    QString provider;      // codex, claude, kiro, antigravity
    QString sessionId;
    QString projectPath;
    QString terminalId;
    AgentState state;
    QString title;
    QString message;
    QDateTime timestamp;
};
```

The shell would then provide identical behavior for every agent:

* consistent desktop notification titles;
* provider icon and project name;
* “Open terminal” action;
* persistent attention indicator in the top bar;
* optional session list popup;
* notification replacement instead of generating a new notification for every state transition.

For example:

```text
Kiro CLI · holonight-shell
Approval required

Kiro wants to execute:
  cmake --build build

[Open terminal] [Dismiss]
```

## What each CLI can provide

### Codex

Current Codex supports lifecycle hooks, including events such as `Stop`, `SessionStart`, `SessionEnd`, `PreToolUse`, and `PostToolUse`. A command hook receives structured context, so it can call a HoloNight event utility rather than letting Codex generate its own notification. ([OpenAI Developers][1])

Codex also exposes structured telemetry for conversations, API requests, tools, and approval decisions. That is useful for analytics, but hooks are simpler and more appropriate for interactive notifications. ([OpenAI Developers][2])

A global hook could conceptually run:

```bash
holonight-agent-event --provider codex --event stop
```

with the hook JSON passed through standard input.

For approval notifications, a pre-tool hook may not necessarily mean approval is required: it also runs for automatically permitted tools. Therefore, use a dedicated approval-related event when available in the hook payload, or let a wrapper correlate the hook with the CLI’s current approval policy.

### Claude Code

Claude already has nearly the ideal interface. Its `Notification` hook receives structured fields including:

```json
{
  "session_id": "abc123",
  "cwd": "/project",
  "message": "Claude needs your permission",
  "title": "Permission needed",
  "notification_type": "permission_prompt"
}
```

The hook is explicitly intended for forwarding notifications to another service. ([Claude Platform Docs][3])

Claude documents notifications for both waiting for input and permission, and supports shell-command hooks on Linux. ([Claude Platform Docs][4])

You could disable or ignore Claude’s ordinary desktop notification behavior and configure:

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

The utility reads the complete JSON event from standard input and forwards it to the daemon.

### Kiro CLI

Kiro also supports command hooks with JSON context on standard input, including `session_id`, `cwd`, lifecycle event name, tool name, input, and response. ([Kiro][5])

Kiro CLI v3 introduces standalone hook files and global hooks. That is particularly useful here because a notification adapter belongs in `~/.kiro/hooks/`, not separately in every repository. ([Kiro][6])

So Kiro should be one of the easier integrations:

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

The exact trigger spelling should be matched against the Kiro version you install, since its v3 hook surface is still evolving. Recent Kiro documentation explicitly includes agent lifecycle and turn-completion hooks.

Approval detection may be less direct than completion detection. If Kiro does not expose a specific pre-approval lifecycle event, you would need either:

1. an ACP client integration;
2. a Kiro-specific log/session adapter;
3. a PTY wrapper that recognizes the structured approval UI.

### Antigravity CLI

The current Antigravity product documentation says it exposes hook configuration and has explicit approval interaction, but I could not find an official public event-schema reference comparable to Claude’s or Kiro’s documentation. ([Google Antigravity][7])

Therefore I would implement its adapter in this order:

1. Use native lifecycle hooks if its `/hooks` interface exposes turn and permission events.
2. Use any machine-readable event stream or log it provides.
3. Fall back to a supervised PTY adapter.
4. Avoid parsing visible terminal text unless there is no alternative.

Text parsing should be the last resort because prompts, wording, colors, and TUI rendering can change between versions.

## Where ACP fits

ACP is a standardized JSON-RPC protocol between an agent and an interactive client. It covers session creation, prompts, cancellation, streaming updates, permission requests, and related UI interaction. ([GitHub][8])

Kiro already supports:

```bash
kiro-cli acp
```

and exposes sessions, prompts, model selection, streaming responses, and other capabilities over JSON-RPC standard input/output. ([Kiro][9])

That means HoloNight could eventually implement an **ACP client** and launch Kiro itself:

```text
HoloNight Agent Center
       │
       ├── spawn kiro-cli acp
       ├── exchange JSON-RPC
       ├── render messages
       ├── render approval UI
       └── own all notifications
```

This gives the cleanest integration because HoloNight knows definitively when:

* a prompt starts;
* output is streaming;
* an approval is requested;
* the turn ends;
* an error occurs;
* the session is cancelled.

However, it changes the product considerably. HoloNight would no longer merely observe agents running in terminals; it would become an agent client, similar to an editor’s AI panel.

Also, ACP does not solve all four integrations today:

* **Kiro CLI:** native ACP support.
* **Antigravity CLI:** public evidence indicates ACP support is not currently available.
* **Codex:** use hooks unless an ACP mode is added or a maintained adapter exists.
* **Claude Code:** use hooks unless an ACP adapter is introduced.

Therefore, ACP should be an adapter behind the same daemon interface, not the foundation required by every provider.

## Fixing terminal titles

The title problem is simpler and independent of notifications.

Terminal titles are normally controlled through an OSC escape sequence:

```bash
printf '\033]0;%s\007' 'Codex · holonight-shell'
```

A HoloNight wrapper can set the title before launching the program:

```bash
#!/usr/bin/env bash
set -euo pipefail

project="${PWD##*/}"
printf '\033]0;Codex · %s\007' "$project"

exec codex "$@"
```

But the terminal or shell may overwrite it later. A more reliable approach is to integrate title changes into the shell:

```zsh
function codex() {
    local project="${PWD:t}"
    print -Pn "\e]0;Codex · ${project}\a"
    command codex "$@"
    print -Pn "\e]0;${project}\a"
}
```

There is an even better HoloNight-specific solution: do not depend on the terminal title at all. Pass session metadata when launching:

```bash
systemd-run --user --scope \
    --setenv=HOLONIGHT_AGENT=codex \
    --setenv=HOLONIGHT_PROJECT="$PWD" \
    codex
```

Or use a launcher wrapper:

```bash
hn-agent-run codex
hn-agent-run claude
hn-agent-run kiro
hn-agent-run antigravity
```

`hn-agent-run` can:

* register the process PID;
* determine the terminal window through `WAYLAND_DISPLAY`, `HYPRLAND_INSTANCE_SIGNATURE`, shell PID ancestry, or a generated token;
* set the terminal title;
* associate hook events with that window;
* restore the normal title on exit;
* report crashes and cancellation.

This also enables the notification’s **Open terminal** button to focus the precise Hyprland window rather than merely launching another terminal.

## D-Bus interface

I would expose something close to:

```text
org.holonight.AgentActivity1
/org/holonight/AgentActivity1
```

Methods:

```text
RegisterSession(provider, sessionId, pid, cwd, metadata)
PublishEvent(sessionId, eventType, payload)
AssociateWindow(sessionId, windowAddress)
EndSession(sessionId, result)
ListSessions()
ActivateSession(sessionId)
```

Signals:

```text
SessionAdded
SessionChanged
SessionRemoved
AttentionRequested
```

The tiny `holonight-agent-event` executable would read provider JSON, normalize it, and make one D-Bus call:

```bash
claude hook
   │ JSON through stdin
   ▼
holonight-agent-event --provider claude
   │ D-Bus
   ▼
holonight-agentd
```

Do not make every hook call `notify-send` directly. That would duplicate policy and presentation across provider configurations. Hooks should only publish semantic events; HoloNight decides whether and how to notify.

## Notification policy

Not every completed turn should necessarily produce a popup. A useful policy would be:

* terminal focused: update status silently;
* terminal visible but unfocused: subtle top-bar indicator;
* terminal hidden or on another workspace: desktop notification;
* approval required: always notify, unless the relevant terminal is focused;
* failure: notify;
* completion under a configurable threshold, such as five seconds: suppress;
* multiple events from one session: replace the existing notification;
* multiple background agents: aggregate them.

The notification identity could be stable:

```text
x-holonight-session-id = provider + sessionId
```

so “Working” is replaced by “Approval required” and later by “Completed.”

## Suggested implementation sequence

### Phase 1: hooks and unified notifications

Implement:

```text
holonight-agentd
holonight-agent-event
provider normalizers
D-Bus API
shell notification integration
```

Start with Claude, Codex, and Kiro. These have documented lifecycle-hook mechanisms.

### Phase 2: launcher and terminal association

Add:

```bash
hn-agent-run <provider>
```

This gives you reliable provider names, project names, PID tracking, terminal/window association, and unified titles.

### Phase 3: Antigravity adapter

Use its native hooks when sufficient. Otherwise add a provider-specific PTY or log adapter, isolated from the core.

### Phase 4: ACP client

Implement an optional HoloNight Agent Center or sidebar tab. Initially support Kiro through ACP. Keep the same internal `AgentSession` and `AgentEvent` types, so terminal-hook sessions and ACP sessions appear together.

## My recommendation

Build **a hooks-first event broker with optional ACP adapters**.

That gives you useful integration with the tools you already run in terminals, without forcing HoloNight to become a complete coding-agent frontend. ACP can later provide a richer native experience for compatible agents, while wrappers and hooks continue supporting ordinary CLI sessions.

The important abstraction is not “all agents must use ACP.” It is:

```text
provider-specific signal
        ↓
normalized agent lifecycle event
        ↓
one HoloNight UX
```

That architecture can also become the foundation for the AI activity widget you previously considered: active agents, elapsed time, project, current operation, approval state, and a direct jump to the corresponding terminal.

## Q2

Suggest how to build this agent. Should it be a separate repo or it could be combined with the daemon which I already building which runs system scans we discussed to collect metrics for system health checking. These 2 functionality are not related at all to each other, then it should be separate apps, repos, processes, daemons. From other hand side, HoloNight could have lots of daemons (I mean some possible future functionality)

## A2

I would separate them at the **process and service level**, but not necessarily at the **repository level**.

The right boundaries are:

* `holonight-healthd`: system scanning, metrics, diagnostics, scheduled work.
* `holonight-agentd`: coding-agent sessions, lifecycle events, terminal association, approvals and notifications.
* shared HoloNight libraries for D-Bus, logging, configuration, persistence and process metadata.

These are different failure domains and workloads, even though both ultimately feed HoloNight Shell.

## Why they should be separate processes

`holonight-healthd` is likely to:

* run periodic and potentially expensive checks;
* invoke external commands;
* collect system logs and hardware metrics;
* possibly require elevated helper operations;
* continue operating when no interactive shell session is active;
* have stricter security and privacy constraints.

`holonight-agentd` is likely to:

* be event-driven and almost always idle;
* receive frequent short hook events;
* track terminal windows and user sessions;
* react quickly to approval requests;
* restart without disrupting system-health history;
* run entirely as the interactive user.

Combining them into one daemon would create undesirable coupling. For example:

* a blocked health scan could delay an agent approval event;
* a parser crash in an Antigravity adapter could stop health monitoring;
* restarting agent integration after a CLI update would interrupt system scans;
* permission requirements would become the union of both services;
* configuration and logs would become difficult to reason about.

So I would not create one large `holonightd` process containing both.

## Separate processes do not require separate repositories

Repositories should usually follow development and release boundaries, not process boundaries.

My preferred structure would be a single services repository:

```text
holonight-services/
├── CMakeLists.txt
├── apps/
│   ├── agentd/
│   │   ├── main.cpp
│   │   ├── AgentService.cpp
│   │   ├── SessionRegistry.cpp
│   │   └── adapters/
│   │       ├── ClaudeAdapter.cpp
│   │       ├── CodexAdapter.cpp
│   │       ├── KiroAdapter.cpp
│   │       └── AntigravityAdapter.cpp
│   │
│   ├── healthd/
│   │   ├── main.cpp
│   │   ├── HealthService.cpp
│   │   ├── ScanScheduler.cpp
│   │   └── scanners/
│   │       ├── JournalScanner.cpp
│   │       ├── HardwareScanner.cpp
│   │       └── PackageScanner.cpp
│   │
│   └── agent-event/
│       └── main.cpp
│
├── libs/
│   ├── dbus/
│   ├── config/
│   ├── logging/
│   ├── process/
│   └── storage/
│
├── dbus/
│   ├── org.holonight.Agent1.xml
│   └── org.holonight.Health1.xml
│
├── systemd/
│   ├── holonight-agentd.service
│   ├── holonight-healthd.service
│   └── holonight-services.target
│
└── tests/
```

This gives you:

* separate binaries;
* separate systemd units;
* separate D-Bus interfaces;
* shared infrastructure;
* one build system;
* one packaging definition;
* coordinated API versioning;
* less repository overhead.

A repository can produce many executables without becoming a monolith.

## Why not put them into `holonight-shell`

I would avoid making these daemons part of the `holonight-shell` repository unless you intentionally want `holonight-shell` to become the umbrella repository for the entire desktop environment.

The shell and services have different release characteristics:

* the shell is presentation and interaction;
* daemons are background infrastructure;
* services should survive shell crashes and shell restarts;
* some users may run the services without running the complete shell;
* daemon changes may need security review independently of QML/UI changes.

A clean dependency direction is:

```text
holonight-shell
       │
       │ D-Bus
       ▼
holonight-agentd
holonight-healthd
```

The daemons should never link against shell UI code.

They may share a low-level package such as `holonight-core`, but even that should remain small and stable.

## Avoiding “too many daemons”

Having several daemons is not inherently a problem. A modern Linux desktop already runs many narrowly scoped user services. The real problems are:

* uncontrolled startup;
* duplicated infrastructure;
* excessive idle memory;
* unclear ownership;
* poorly defined D-Bus APIs;
* services that run continuously despite rare use.

You can address those without combining unrelated functionality.

### Use D-Bus activation

`holonight-agentd` does not necessarily need to start at login. It can be activated when:

* `holonight-agent-event` sends its first event;
* the shell opens the agent activity popup;
* `hn-agent-run` registers a session.

Conceptually:

```ini
[D-BUS Service]
Name=org.holonight.Agent1
Exec=/usr/lib/holonight/holonight-agentd
```

This means the service consumes no memory until needed.

For approval notifications, once an agent session is registered, it remains alive until the last session ends. It can exit after an idle timeout.

### Use systemd timers for periodic work

The health functionality may not need a permanently running daemon either.

You could split it into:

```text
holonight-health-scan
holonight-healthd
```

Where:

* `holonight-health-scan` is a one-shot scanner launched by a systemd timer;
* `holonight-healthd` stores state, exposes D-Bus and coordinates on-demand scans;
* or the daemon exits when idle and systemd activates it when required.

For example:

```text
holonight-health-scan.timer
        ↓
holonight-health-scan.service
        ↓
publish results to health database/D-Bus
```

A permanent daemon is justified only when you need continuous monitoring, event subscriptions or coordination.

### Group services operationally

Provide a target:

```ini
[Unit]
Description=HoloNight background services
Wants=holonight-healthd.service
```

Users can then manage the suite through:

```bash
systemctl --user status holonight-services.target
systemctl --user restart holonight-agentd
systemctl --user disable holonight-healthd
```

The target groups services administratively without merging them into one process.

## A useful future service taxonomy

I would classify future HoloNight background functionality into three categories.

### Existing system services should be reused

Do not create a HoloNight daemon when Linux already has an appropriate system service.

Examples:

* NetworkManager for networking;
* UPower for batteries;
* PipeWire/WirePlumber for audio;
* package-manager-specific services where available;
* geoclue for location;
* systemd-logind for sessions.

HoloNight should consume their APIs.

### HoloNight domain services

Create separate services when HoloNight owns meaningful persistent state or orchestration.

Likely examples:

```text
holonight-agentd
holonight-healthd
holonight-packaged
holonight-indexerd
```

These are genuinely separate domains.

### Lightweight shell-owned functionality

Keep functionality in the shell when it is:

* purely presentational;
* cheap;
* tied directly to the current shell process;
* stateless or reconstructible;
* not useful without the shell.

For example, popup state or temporary UI filtering should not become a daemon.

## Shared infrastructure

The services can share a small library:

```text
libholonight-service
```

It might contain:

```cpp
namespace HoloNight {

class ServiceApplication;
class JsonConfig;
class DbusError;
class SqliteStore;
class ProcessIdentity;
class StructuredLogger;
class RateLimiter;

}
```

But avoid creating an excessively broad “framework.” Share only code that has already been duplicated or has a clearly stable contract.

The most important shared element is probably the event envelope:

```cpp
struct EventEnvelope {
    QString domain;       // "agent", "health", "package"
    QString type;         // "approval-required", "scan-completed"
    QString source;
    QString instanceId;
    QDateTime timestamp;
    QJsonObject payload;
};
```

This does **not** mean all daemons need to expose a single generic event API. Their domain-specific D-Bus APIs should remain explicit. The common envelope can be used internally for logging, persistence or forwarding notifications.

## Notification ownership

You should decide where notification policy lives.

My recommendation:

```text
domain daemon
    │ semantic event
    ▼
HoloNight Shell notification coordinator
    │
    ▼
desktop notification UI
```

For example, `holonight-agentd` emits:

```text
ApprovalRequired(sessionId, provider, project, summary)
```

It should not decide the exact visual notification layout.

Likewise, `holonight-healthd` emits:

```text
HealthIssueDetected(issueId, severity, component)
```

The shell can apply unified policies such as:

* do not notify while the relevant view is focused;
* group related notifications;
* suppress low-priority messages;
* show badges in the top bar;
* persist unresolved attention items.

If HoloNight Shell is not running, the daemon may fall back to `org.freedesktop.Notifications`. This fallback can be implemented in a shared notification bridge.

## Packaging recommendation

I would package the repository as one Arch package initially:

```text
holonight-services
```

Containing:

```text
/usr/bin/holonight-agent-event
/usr/bin/hn-agent-run
/usr/lib/holonight/holonight-agentd
/usr/lib/holonight/holonight-healthd
/usr/lib/systemd/user/holonight-agentd.service
/usr/lib/systemd/user/holonight-healthd.service
/usr/share/dbus-1/services/...
```

Later, if dependencies diverge significantly, split binary packages without splitting the Git repository:

```text
holonight-agent-service
holonight-health-service
holonight-services-common
```

Arch packaging can generate several packages from the same source repository using a split `PKGBUILD`.

## My concrete recommendation

Use this arrangement:

```text
Processes
─────────
holonight-shell
holonight-agentd
holonight-healthd

Small utilities
───────────────
holonight-agent-event
hn-agent-run
holonight-health-scan
```

Keep `agentd` and `healthd`:

* in the same services repository;
* as independent executables;
* under independent systemd user units;
* on independent D-Bus names;
* with independent configuration and state directories;
* sharing only low-level service infrastructure.

That gives you isolation without producing an unmanageable collection of repositories. The rule I would follow for future functionality is:

> Split by failure, security and lifecycle boundaries at the process level; split repositories only when ownership, dependencies or release cycles genuinely diverge.
