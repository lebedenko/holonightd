# Agent Notification Window Activation — Design

**Initiative:** `agent-notification-window-activation`
**Work package:** `ANWA-102`
**Baseline:** `dbb6ecc6205f964c3c4dc4e2467503ee940dc34f`
**Dependency:** published and pinned `ANWA-101`

## Data flow

```text
RegisterSession(pid, metadata)
  -> bounded /proc lineage + title hint
  -> Session activation descriptor
     -> ActivateSession(sessionId) ---------+
     -> notification ID binding -> "open" -+-> ShellActivationClient -> HoloNight Shell
```

The daemon owns identity and notification correlation. HoloNight Shell owns all window inventory, candidate
resolution, and compositor commands.

## Bounded process ancestry

A small injectable `ProcessAncestryReader` reads `/proc/<pid>/stat` directly. Parsing locates the final `)` of the
parenthesized command field before reading PPID, so spaces and parentheses in process names do not shift fields. It
starts with the PID supplied to `RegisterSession`, appends each positive parent, and stops at PID 1, a repeated PID,
an unreadable/malformed record, or 64 entries. A partial non-empty lineage remains usable; an invalid starting PID
produces no activation descriptor.

Tests inject stat contents/read failures and never depend on the host process tree.

## Activation descriptors and notification bindings

`SessionRegistry` entries gain an `ActivationDescriptor` containing `std::vector<std::uint32_t> process_lineage` and a
bounded `std::string title_hint`. Registration extracts the optional terminal title from metadata JSON and stores the
descriptor with the existing session record.

`NotificationActionBindings` maps a notification ID to `{ActivationDescriptor, created_at}`. It owns copies rather than
session IDs, so bindings survive `EndSession` and registry removal. The table is capped at 256 entries; insertion and
signal handling prune entries older than 24 hours, then evict the oldest entry if capacity is still exhausted.

## Persistent notification bus

Refactor `NotificationBridge` from per-call bus setup into an object owned by the agent daemon's event-loop
composition. Its persistent `sd_bus` connection:

- sends `Notify` with `"open", "Open terminal"` actions;
- installs match slots for `ActionInvoked` and `NotificationClosed` before notifications are sent;
- retains the returned `sd_bus_slot` objects for the bridge lifetime;
- records the returned notification ID only after a successful method reply; and
- dispatches bus signals through the daemon's existing poll/event-loop integration.

For notification replacement, keep the previous binding until `Notify` succeeds. Then remove `replaces_id` and insert
the returned ID in one model operation, whether the IDs are equal or different. `NotificationClosed` erases its ID.
`ActionInvoked` ignores unknown IDs and keys other than `"open"`; a recognized Open action consumes the binding before
calling activation, making duplicate delivery harmless.

## Shell activation client

`ShellActivationClient` is the only dependency on the shell contract. It accepts an `ActivationDescriptor`, constructs
`org.holonight.Shell.WindowActivation1.RequestWindowActivation(au,s)`, and returns an accepted/rejected result with a
diagnostic category. The production implementation uses the persistent session bus and caps the synchronous method
call at 500 ms. It does not retry, start a nested long-running operation, or spawn a process.

Callers depend on an interface/fake so unit tests cover accepted, rejected, unavailable, malformed-reply, and timeout
outcomes without a live shell. Diagnostics avoid logging full ancestry or title contents.

## ActivateSession

The AgentActivity vtable adds `ActivateSession` with input `s` and output `b`. The handler looks up the currently active
session, copies its valid descriptor, calls `ShellActivationClient`, and replies with its boolean. It emits no signal.
Unknown/ended sessions and invalid descriptors reply `false` without contacting the shell.

Both this method and notification actions call the same descriptor-based activation function. Notification actions do
not route back through `ActivateSession`, because their copied descriptor deliberately outlives the active session.

## Wrapper metadata

`hn-agent-run` derives the title hint only from terminal metadata already available in its environment/configuration,
adds it to the existing JSON object, and applies the 512-byte bound before registration. The daemon remains the
authority for validating the field. Absence of metadata preserves current behavior.

Neither `holonight-agentd` nor `hn-agent-run` links compositor libraries, examines compositor environment variables, or
executes compositor commands. Their only window-management dependency is the shell D-Bus interface.

## Test boundaries

- Pure tests cover `/proc` stat parsing, lineage stops, title bounds, descriptor copying, binding replacement,
  consumption, closure, capacity, and stale pruning using an injected clock.
- Vtable/handler tests cover existing RegisterSession compatibility and `ActivateSession` boolean behavior.
- Notification bridge tests use a fake bus boundary to verify matches, action routing, replacement IDs, and cleanup.
- Shell client tests assert the exact destination/interface/member/signature and the 500 ms cap.
- Full daemon verification confirms event-loop integration and absence of compositor dependencies.

Actual notification-server delivery and compositor focus are umbrella manual verification, not daemon unit tests.
