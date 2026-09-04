# Agent Notification Window Activation — Tasks

**Initiative work package:** `ANWA-102`
**Implementation baseline:** `dbb6ecc6205f964c3c4dc4e2467503ee940dc34f`
**Depends on:** published and pinned `ANWA-101` (`c2fa018aabb4dc3c9125d9e956237757d2a6975a`)

Complete and commit this work package only in `holonightd`.

- [x] **AG-ACT-T01 — Process lineage:** add injectable `/proc/<pid>/stat` parsing and bounded ancestry collection with
  partial-lineage, PID-1, cycle, malformed-entry, and unreadable-entry tests.
- [x] **AG-ACT-T02 — Session activation model:** add bounded activation descriptors to session registration while
  preserving the existing `RegisterSession` wire signature and callers.
- [x] **AG-ACT-T03 — Notification binding model:** implement copied descriptor bindings, replacement moves, one-shot
  Open consumption, close cleanup, 24-hour pruning, and the 256-entry cap with an injected clock.
- [x] **AG-ACT-T04 — Persistent-bus subscriptions:** make `NotificationBridge` daemon-owned, retain standard
  `ActionInvoked`/`NotificationClosed` match slots, add the `"open"` action, and integrate signal processing with the
  daemon event loop.
- [x] **AG-ACT-T05 — Shell activation client:** implement the exact
  `org.holonight.Shell.WindowActivation1.RequestWindowActivation(au,s)->b` call with a 500 ms maximum wait and testable
  accepted, rejected, unavailable, malformed, and timeout outcomes.
- [x] **AG-ACT-T06 — ActivateSession:** export `ActivateSession(s)->b`, share descriptor submission with notification
  actions, reject unknown/ended sessions, and confirm introspection contains no activation signal.
- [x] **AG-ACT-T07 — Wrapper metadata:** add optional bounded terminal-title metadata in `hn-agent-run` without changing
  its CLI or `RegisterSession` signature.
- [x] **AG-ACT-T08 — Unit tests:** cover ancestry, metadata validation, session-independent notification lifetime,
  replacement/close/stale behavior, Open-only routing, shell failures, and duplicate signals.
- [x] **AG-ACT-T09 — Architecture verification:** confirm `holonight-agentd` and `hn-agent-run` have no compositor tool,
  environment, protocol, or library dependency and no fallback process-launch path.
- [x] **AG-ACT-T10 — Automated verification:** run `task format-check`, `task tidy-src`, `task tidy-tests`, and
  `task test`; record exact commands and results in the repository handoff.
- [x] **AG-ACT-T11 — Full daemon smoke check:** exercise registration, notification replacement/action/closure, direct
  activation, unavailable shell, and ended-session behavior on the persistent bus.

## Verification record

2026-09-04 automated verification:

- `task format-check` — passed.
- `task tidy-src` — passed.
- `task tidy-tests` — passed.
- `task test` — passed, 111/111 tests.
- Architecture search found no compositor command, environment, protocol, or library dependency in the agent daemon
  or activation path. The existing `hn-agent-run` process launch remains limited to its documented wrapper role.
- An isolated `dbus-run-session` smoke check confirmed the unchanged `RegisterSession(ssuss)->s` method,
  `ActivateSession(s)->b`, absence of an activation signal, unavailable-shell `false`, successful session removal, and
  ended-session `false`.

2026-09-04 manual desktop verification:

- Registration through the built `hn-agent-run` wrapper succeeded with a running terminal session.
- Direct `ActivateSession` returned `true` and focused the expected terminal.
- Notification delivery, replacement, Open action routing, one-shot behavior, and closure behavior passed.
- After the wrapped process ended, `ActivateSession` returned `false`.
