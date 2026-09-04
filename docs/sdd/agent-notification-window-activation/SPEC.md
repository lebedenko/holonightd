# Agent Notification Window Activation — Requirements Specification

**Initiative:** `agent-notification-window-activation`
**Work package:** `ANWA-102`
**Baseline:** `dbb6ecc6205f964c3c4dc4e2467503ee940dc34f`
**Status:** Implemented; awaiting publication and umbrella handoff

## Scope

`holonight-agentd` shall retain enough bounded terminal identity for each registered session to ask HoloNight Shell to
activate its existing window. It shall own standard notification action handling and expose a direct session
activation method while remaining independent of compositor IPC.

## Functional requirements

### AG-ACT-001: RegisterSession compatibility and metadata

The existing method remains wire-compatible:

```text
RegisterSession(s provider, s sessionId, u pid, s cwd, s metadata_json) -> s assignedId
```

Existing callers and metadata remain valid. `hn-agent-run` shall add a terminal title string to `metadata_json` when it
can determine one without changing positional arguments or the D-Bus signature. Missing or invalid title metadata
shall be treated as an empty hint, not a registration failure.

At registration, the daemon captures a bounded process lineage beginning with the supplied positive PID. Session
state stores a self-contained activation descriptor: ordered lineage plus optional exact terminal-title hint.

### AG-ACT-002: ActivateSession method

`org.holonight.AgentActivity1` adds:

```text
ActivateSession(s sessionId) -> b accepted
```

There shall be no `ActivationRequested` or activation-completed signal. The method returns `false` for an unknown or
inactive session, invalid descriptor, unavailable shell, timed-out call, or shell rejection. Otherwise it returns the
shell provider's accepted result.

### AG-ACT-003: Standard notification actions

Attention notifications shall include the standard action pair `"open", "Open terminal"`. On the same persistent
session-bus connection used for notifications, the daemon subscribes to:

```text
org.freedesktop.Notifications.ActionInvoked(u notificationId, s actionKey)
org.freedesktop.Notifications.NotificationClosed(u notificationId, u reason)
```

Only action key `"open"` triggers activation. Other keys and unrelated notification IDs are ignored. No private shell
notification hints or shell-side agent specialization are required.

### AG-ACT-004: Notification correlation lifetime

For each successful `Notify` reply, correlate the returned notification ID with a copy of the activation descriptor,
independent of the active `SessionRegistry` entry. Ending or removing the session shall not invalidate an already
displayed notification's Open action. The binding ends on action consumption, matching close signal, replacement by a
new ID, or bounded stale pruning.

### AG-ACT-005: Replacement and closure

When `replaces_id` is reused and the notification server returns either the same or a different ID, atomically move the
binding to the returned ID and discard the obsolete mapping. `NotificationClosed` removes the matching binding.
Repeated, late, or unknown signals are harmless.

### AG-ACT-006: Graceful unavailability

If the shell service is absent, the D-Bus call times out, the session is unavailable, the descriptor is invalid, or
the target window has closed, activation returns or completes as a clean rejection. Notification handling continues,
the daemon remains responsive, and no process is launched.

### AG-ACT-007: Bounds

- Capture no more than 64 PIDs, stop at PID 1, cycles, unreadable `/proc` entries, parse errors, or the bound.
- Accept only positive 32-bit PIDs and never retry ancestry by spawning a helper.
- Limit terminal title hints to 512 UTF-8 bytes and notification bindings to 256 entries.
- Prune notification bindings older than 24 hours during normal notification/action activity.
- Bound the synchronous shell request to at most 500 ms.

## Non-goals

- Compositor detection, `hyprctl`, `swaymsg`, Wayland protocols, or compositor-specific data in agent binaries.
- Niri, KWin, labwc, terminal spawning, or notification-server specialization.
- Changing the existing `RegisterSession` signature or guaranteeing that a terminal exposes the wrapper's hint.

## Acceptance criteria

- Existing registration clients remain compatible.
- Direct activation and notification `"open"` actions submit identical descriptors to the shell boundary.
- Tests cover ancestry bounds/failures, notification replacement/closure/staleness, session-independent bindings,
  shell absence/timeout/rejection, and lack of an activation signal.
- `holonight-agentd` and `hn-agent-run` contain no compositor-specific command or protocol dependency.
