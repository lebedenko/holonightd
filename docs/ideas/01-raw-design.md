## Yes, the idea makes sense—but I would change the role of the custom model

What you are describing could become a genuinely valuable HoloNight subsystem:

> A local Linux health service that continuously detects abnormal conditions, gathers relevant evidence, determines likely causes, and presents safe remediation options.

The important adjustment is this:

**Do not begin by training a small language model to “investigate Linux.”**

Begin by building a deterministic diagnostic platform. Later, train one or more small models for narrowly defined decisions inside that platform.

A model cannot investigate a computer unless something else gives it:

* reliable system state;
* historical context;
* permission-controlled diagnostic actions;
* knowledge of which commands are appropriate;
* a way to distinguish harmless log noise from actual faults;
* evidence connecting symptoms to causes.

Most of the difficult work is therefore **data collection, normalization, correlation, and evaluation**, not language generation.

---

# The architecture I would recommend

```text
┌─────────────────────────────────────────────────────────────┐
│                     HoloNight UI                            │
│ Health status, incidents, evidence, proposed actions        │
└──────────────────────────────▲──────────────────────────────┘
                               │
                      user-friendly report
                               │
┌──────────────────────────────┴──────────────────────────────┐
│                  Report-generation model                    │
│ Larger local/cloud LLM; explanation only                    │
└──────────────────────────────▲──────────────────────────────┘
                               │ structured diagnosis
┌──────────────────────────────┴──────────────────────────────┐
│                  Diagnostic reasoning layer                 │
│ Rules + correlations + small model + knowledge retrieval    │
└──────────────────────────────▲──────────────────────────────┘
                               │ normalized observations
┌──────────────────────────────┴──────────────────────────────┐
│               Collectors and diagnostic probes              │
│ systemd, journal, kernel, disks, memory, network, packages, │
│ GPU, thermal state, batteries, filesystems, desktop session │
└──────────────────────────────▲──────────────────────────────┘
                               │
┌──────────────────────────────┴──────────────────────────────┐
│               Privileged helper / system daemon             │
│ Restricted read actions; separately approved repair actions │
└─────────────────────────────────────────────────────────────┘
```

This separation is critical.

The diagnostic layer should output something machine-readable such as:

```json
{
  "incident_type": "filesystem_space_pressure",
  "severity": "warning",
  "confidence": 0.96,
  "affected_component": "/",
  "symptoms": [
    {
      "metric": "filesystem.used_percent",
      "value": 96.8,
      "threshold": 95
    }
  ],
  "likely_causes": [
    {
      "cause": "systemd journal storage growth",
      "confidence": 0.84,
      "evidence": [
        "/var/log/journal occupies 18.4 GiB",
        "journal growth increased sharply during the last 3 days"
      ]
    }
  ],
  "recommended_actions": [
    {
      "action_id": "vacuum_journal",
      "risk": "low",
      "requires_root": true,
      "automatic": false
    }
  ]
}
```

A stronger model can turn that into:

> Your root filesystem is 96.8% full. Most of the recent growth comes from persistent systemd journal files, which currently occupy 18.4 GiB.
>
> You can safely reduce retained logs to 2 GiB, inspect the unusually noisy service first, or postpone the action. No files will be deleted without confirmation.

The small diagnostic system does not need to be eloquent. It needs to be **consistent, calibrated, evidence-based and difficult to fool**.

---

# What should not be delegated to an LLM

A language model should not be the primary mechanism for detecting:

* high disk usage;
* failed systemd units;
* SMART degradation;
* memory pressure;
* filesystem read-only remounts;
* kernel machine-check events;
* repeated application crashes;
* package database corruption;
* thermal throttling;
* battery health degradation;
* failing network interfaces;
* boot performance regressions;
* outdated kernel after an upgrade;
* a reboot being required;
* mismatched kernel modules and running kernel;
* degraded RAID arrays;
* OOM kills;
* DNS failures.

These can usually be determined through explicit checks, state machines, statistical thresholds and known correlations.

For example, `systemctl` already exposes unit state, `journalctl` exposes structured journal events, and `systemd-analyze` exposes boot performance and other system-manager diagnostics. ([Freedesktop][1])

The Linux kernel also has subsystem-specific health mechanisms. For example, `devlink` health reporters can expose real-time device alerts, diagnostic data and, for supported drivers, recovery operations. ([Kernel Documentation][2])

Use these authoritative signals directly. Do not ask a model to rediscover them from arbitrary terminal output.

---

# Where a small custom model would actually help

## 1. Incident classification

Input:

* selected journal events;
* system facts;
* relevant metric changes;
* package operations;
* recent configuration changes.

Output:

```text
network.dns_resolution_failure
network.wifi_signal_instability
storage.disk_media_degradation
graphics.driver_kernel_mismatch
service.configuration_error
```

This can be a relatively small encoder or classifier rather than a generative LLM.

## 2. Evidence ranking

Suppose the journal contains 10,000 events around a crash. A model can rank the 20 most relevant events.

That is a much easier and more measurable task than asking:

> “Read this enormous log and determine what happened.”

## 3. Root-cause ranking

Given a list of candidate causes created by deterministic rules, rank them:

```json
[
  ["nvidia_module_mismatch", 0.91],
  ["wayland_compositor_bug", 0.31],
  ["gpu_hardware_failure", 0.08]
]
```

The model does not invent arbitrary diagnoses. It chooses among constrained candidates and cites evidence.

## 4. Novelty or anomaly detection

A non-language model can learn what is normal for one machine:

* typical memory use by time of day;
* normal boot duration;
* normal service restart frequency;
* usual temperature and fan relationships;
* expected disk growth;
* typical network latency;
* normal battery discharge curve.

This could identify:

> `NetworkManager` restarted 14 times today; the usual rate is zero to one.

For this use case, statistical and time-series methods may outperform an LLM while consuming dramatically fewer resources.

## 5. Probe selection

Eventually, a small model could choose the next safe diagnostic probe:

```text
Observation: DNS resolution fails but ICMP connectivity works.
Next probe: query configured DNS servers directly.
```

This is agent-like behaviour, but it should choose from a restricted registry of read-only probes rather than generate shell commands.

---

# Do not train a model from scratch

Training a foundational language model from scratch would require:

* an enormous corpus;
* significant compute;
* tokenizer and architecture decisions;
* pretraining infrastructure;
* extensive evaluation;
* months of experimentation;
* much more data than Linux documentation and logs alone can provide.

It is unnecessary for this project.

Instead, start from an existing small pretrained model and use **parameter-efficient fine-tuning**, such as LoRA or QLoRA. Hugging Face’s PEFT tooling is specifically designed to adapt models by training a relatively small number of additional parameters instead of updating the entire base model. Quantization can also reduce memory requirements, and QLoRA combines low-bit quantization with LoRA adaptation. ([Hugging Face][3])

However, even fine-tuning should come much later than the first working prototype.

---

# Your first version should contain no trained model

I would build version 0.1 as:

```text
HoloNight Health
├── collector daemon
├── normalized observation database
├── diagnostic rule engine
├── diagnostic probe registry
├── incident correlation engine
├── static remediation catalogue
└── optional report-generation LLM
```

That will let you solve the most important unanswered question:

> What information must be collected to produce a reliable Linux diagnosis?

Without that knowledge, you cannot construct a meaningful training dataset.

---

# A practical diagnostic event model

Every collector should emit the same general structure:

```json
{
  "timestamp": "2026-07-29T22:31:14+03:00",
  "source": "systemd",
  "category": "service",
  "subject": "bluetooth.service",
  "signal": "unit_failed",
  "value": true,
  "severity": "error",
  "attributes": {
    "result": "exit-code",
    "exit_status": 1,
    "restart_count": 4
  }
}
```

Other examples:

```json
{
  "source": "kernel",
  "category": "memory",
  "subject": "system",
  "signal": "oom_kill",
  "attributes": {
    "killed_process": "firefox",
    "cgroup": "...",
    "memory_pressure": 0.98
  }
}
```

```json
{
  "source": "smart",
  "category": "storage",
  "subject": "/dev/nvme0n1",
  "signal": "media_errors",
  "value": 12,
  "previous_value": 2
}
```

This normalized representation becomes useful for:

* deterministic rules;
* historical comparisons;
* model training;
* test fixtures;
* UI rendering;
* anonymization;
* reproducing bugs.

---

# Collectors worth implementing

Start with a deliberately small set.

## Systemd and journal

Collect:

* failed units;
* restart loops;
* activation timeouts;
* dependency failures;
* coredumps;
* boot duration;
* previous-boot critical events;
* repeated message signatures.

The journal is structured, so prefer exported fields over parsing visually formatted `journalctl` text. Systemd also supports remote journal reception, although for HoloNight the default should probably remain local-only. ([Freedesktop][4])

## Kernel

Look for structured signatures involving:

* OOM;
* I/O errors;
* filesystem faults;
* GPU resets;
* firmware crashes;
* thermal throttling;
* USB resets;
* PCIe errors;
* machine-check exceptions;
* watchdog events.

## Storage

Use tools and interfaces such as:

* `statvfs`;
* `/proc/diskstats`;
* SMART/NVMe health data;
* mount state;
* inode consumption;
* filesystem-specific status;
* RAID state where applicable.

## Package state

For Arch:

* pacman database consistency;
* partial upgrades;
* orphaned packages;
* package operations since the previous boot;
* running kernel versus installed kernel;
* changed configuration files;
* `.pacnew` and `.pacsave`;
* failed hooks;
* AUR helper activity as a lower-trust source.

This subsystem should be adapter-based, matching your package-manager project idea:

```text
PackageHealthProvider
├── PacmanHealthProvider
├── AptHealthProvider
└── DnfHealthProvider
```

## Desktop-specific state

For HoloNight, this becomes a differentiator:

* compositor crashes;
* portal failures;
* PipeWire node and route problems;
* WirePlumber errors;
* graphical session failures;
* tray application crashes;
* notification daemon failures;
* GPU acceleration status;
* disconnected output configurations;
* DBus service activation failures.

A generic server diagnostics system usually does not understand desktop experience. Yours could.

---

# The diagnostic knowledge base

Do not put all Linux knowledge directly into model weights.

Store explicit diagnostic definitions:

```yaml
id: graphics.nvidia.kernel-module-mismatch
title: NVIDIA module does not match running kernel

conditions:
  all:
    - fact: package.installed_kernel_version
      differs_from: system.running_kernel_version
    - fact: module.nvidia.load_failed
      equals: true

evidence:
  - system.running_kernel_version
  - package.installed_kernel_version
  - journal.kernel.nvidia_errors

causes:
  - id: reboot_pending
    confidence: 0.9

actions:
  - id: reboot_system
    risk: medium
    requires_confirmation: true

references:
  - type: distro_documentation
    key: arch-kernel-module-lifecycle
```

Advantages:

* auditable;
* testable;
* translatable;
* version-controlled;
* distro-specific;
* updateable without retraining;
* can explain exactly why a diagnosis fired.

The model can supplement this knowledge base, but it should not replace it.

---

# How to create training data

This is likely the most interesting part of the journey.

## Build a Linux failure laboratory

Use virtual machines rather than your main workstation.

A possible lab:

```text
diagnostic-lab/
├── images/
│   ├── arch/
│   ├── fedora/
│   └── ubuntu/
├── scenarios/
│   ├── systemd-restart-loop/
│   ├── disk-full/
│   ├── broken-dns/
│   ├── invalid-fstab/
│   ├── package-partial-upgrade/
│   └── memory-pressure/
├── expected/
└── captured/
```

Each scenario should define:

```yaml
id: service.restart-loop
distribution: arch
setup:
  - install_test_service
  - configure_exit_failure
  - configure_restart_always

trigger:
  - start_test_service

expected:
  incident: service.restart_loop
  root_cause: service_process_exits_immediately
  evidence:
    - restart_count_increasing
    - exit_status_nonzero
```

Then automate:

1. create VM snapshot;
2. inject failure;
3. run workload;
4. collect observations;
5. restore snapshot;
6. save expected diagnosis;
7. verify your engine;
8. add the case to the dataset.

This produces far better data than scraping random forum posts because you know the actual root cause.

## Record real incidents

With explicit user consent, save sanitized incident bundles:

```text
incident-2026-07-29/
├── observations.jsonl
├── system-profile.json
├── timeline.json
├── diagnosis.json
├── user-feedback.json
└── resolution.json
```

Feedback is extremely valuable:

```json
{
  "diagnosis_correct": true,
  "selected_action": "restart_wireplumber",
  "resolved": true,
  "false_positive": false
}
```

Eventually, these examples can train a ranker or classifier.

## Generate synthetic variations

For every scenario vary:

* distro;
* kernel version;
* package version;
* service name;
* log wording;
* locale;
* hardware availability;
* event ordering;
* irrelevant log noise;
* time delay between cause and symptom;
* multiple simultaneous failures.

The point is not to generate prose. It is to prevent the model from memorizing one exact log line.

---

# What kind of model to use

There may eventually be several models.

## Model A: Event classifier

A small encoder model could classify a journal event or event cluster.

Potential scale:

* tens to hundreds of millions of parameters;
* CPU-capable;
* possibly exported to ONNX;
* predictable structured output.

Input:

```text
UNIT=NetworkManager.service
RESULT=exit-code
RESTART_COUNT=8
MESSAGES=[...]
```

Output:

```text
service.restart_loop
```

This does not need an autoregressive LLM.

## Model B: Evidence reranker

Given a diagnosis hypothesis and 100 observations, rank the most relevant evidence.

This could use:

* embeddings;
* a cross-encoder;
* a compact reranker.

## Model C: Structured diagnostic model

Once you have enough data, fine-tune a small instruct model to output constrained JSON:

```json
{
  "hypotheses": [],
  "missing_evidence": [],
  "next_probe_ids": []
}
```

It must be validated against a schema, and unknown probe IDs must be rejected.

## Model D: User-facing narrator

This can remain replaceable:

* local Ollama model;
* user-selected model;
* cloud provider;
* no model at all, using templates.

This fits very well with your HoloNight AI design: the diagnostic engine generates a stable artifact, and a configurable “service model” converts it into user-facing language.

---

# Safety model

This project becomes dangerous when diagnosis and repair are conflated.

Use at least four action classes:

| Class                 | Example                               | Policy                            |
| --------------------- | ------------------------------------- | --------------------------------- |
| Read-only             | Inspect unit status                   | Automatic                         |
| Harmless transient    | Restart a user service                | User confirmation                 |
| System-changing       | Modify config, remove packages        | Explicit confirmation and preview |
| Destructive/high-risk | Filesystem repair, firmware operation | Never automatic; strong warning   |

Repairs should not be arbitrary model-generated commands.

Instead, register actions in code:

```json
{
  "id": "restart_user_wireplumber",
  "executable": "/usr/bin/systemctl",
  "arguments": ["--user", "restart", "wireplumber.service"],
  "privilege": "user",
  "risk": "low",
  "rollback": null
}
```

The model may recommend `restart_user_wireplumber`. It must not generate:

```bash
sudo sh -c '...'
```

For root operations, use a tightly controlled privileged helper, probably exposed through D-Bus and polkit. The desktop process should never receive unrestricted root execution.

---

# Avoiding notification spam

Your idea includes detecting issues that users have not noticed. That is useful, but it can quickly become irritating.

Every incident should have:

* severity;
* confidence;
* expected user impact;
* persistence;
* recurrence count;
* suppression period;
* whether immediate action is required;
* whether the issue resolved itself.

For example:

```text
INFO
One Bluetooth reconnection failure occurred and recovered.
Do not notify.

WARNING
Bluetooth has restarted seven times in two hours.
Show in Health Center, no popup.

HIGH
The root filesystem has less than 1 GiB free and package updates may fail.
Send notification.

CRITICAL
NVMe media errors increased and the drive reports degraded health.
Persistent notification with backup recommendation.
```

A “health center” is better than treating every detection as an urgent notification.

---

# A realistic development roadmap

## Phase 1 — Health dashboard

Implement five checks only:

1. failed systemd units;
2. low filesystem capacity;
3. OOM kills;
4. package-update/reboot mismatch;
5. SMART/NVMe warnings.

No model required.

Display:

* issue;
* evidence;
* first observed;
* last observed;
* impact;
* manually written actions.

## Phase 2 — Incident correlation

Group individual observations into incidents.

Example:

```text
Package upgrade
→ new kernel installed
→ system not rebooted
→ out-of-tree NVIDIA module unavailable
→ graphical application falls back or fails
```

This is where the project becomes much more than a system monitor.

## Phase 3 — Diagnostic probe registry

Add controlled on-demand probes.

For a network incident:

```text
check_link_state
check_default_route
ping_gateway
query_dns_server
resolve_known_hostname
check_captive_portal
```

The engine follows an explicit decision graph.

## Phase 4 — Report-generation integration

Send the structured incident to your HoloNight AI service model.

You already have the conceptual architecture for cheap internal models used for titles and compaction. The same abstraction could support:

```text
Service task: health-report-generation
Preferred model: qwen-small-local
Fallback model: selected cloud model
```

## Phase 5 — Failure laboratory and dataset

Automate VM scenarios and begin accumulating labelled diagnostic records.

## Phase 6 — Train the first narrow model

Do not train a general Linux investigator yet.

Train one of:

* journal-event classifier;
* incident category classifier;
* evidence relevance scorer;
* root-cause candidate ranker.

Measure whether it performs better than the existing deterministic baseline.

## Phase 7 — Adaptive investigation

Only after the safe probe system and dataset exist should a model select the next diagnostic step.

---

# Suggested technology stack

For your current projects, I would use:

### System service

* C++ daemon;
* Qt D-Bus or `sdbus-c++`;
* systemd service;
* SQLite for observations and incidents;
* polkit for privileged actions;
* systemd journal API rather than shelling out where practical.

### Diagnostic definitions

* YAML or JSON;
* JSON Schema validation;
* versioned inside a separate repository or shared package;
* distro-specific overlays.

### ML experimentation

* Python;
* PyTorch;
* Hugging Face Transformers;
* PEFT/LoRA;
* TRL for supervised fine-tuning;
* scikit-learn as the first baseline;
* ONNX Runtime for lightweight production inference where applicable.

Hugging Face’s TRL documentation provides PEFT integration for supervised fine-tuning workflows, including LoRA and QLoRA. ([Hugging Face][5])

### Test laboratory

* QEMU/KVM;
* libvirt;
* cloud-init;
* Ansible or Python orchestration;
* VM snapshots;
* containers only for userspace scenarios.

VMs are necessary for kernel, bootloader, filesystem, driver and systemd-level faults. Containers will not faithfully reproduce many of those conditions.

---

# How this could fit HoloNight

I would probably make this a separate project:

```text
holonight-health
├── daemon/
├── privileged-helper/
├── collectors/
├── diagnostics/
├── actions/
├── model-runtime/
└── cli/
```

Then integrate it with:

```text
holonight-shell
├── health indicator
├── notifications
└── sidebar health overview

holonight-settings
└── Health & Diagnostics settings

holonight-ai
└── report generation and interactive explanation
```

Possible D-Bus interface:

```text
org.holonight.Health1
├── ListIncidents()
├── GetIncident(id)
├── RunScan(profile)
├── RunProbe(incidentId, probeId)
├── ProposeActions(incidentId)
├── ExecuteAction(actionId)
└── IncidentChanged
```

This separation also allows non-HoloNight desktops to use the daemon eventually.

---

# My strongest recommendation

Treat the project as building a **Linux diagnostic expert system with optional learned components**, not as training a Linux-specialized chatbot.

The progression should be:

```text
observability
→ normalized facts
→ deterministic diagnostics
→ controlled investigation
→ labelled dataset
→ narrow ML models
→ adaptive diagnostic agent
```

Not:

```text
collect logs
→ fine-tune small LLM
→ allow it to run commands
```

The first path can become trustworthy software. The second will probably produce impressive demonstrations but unreliable diagnoses, invented causes and unsafe repair suggestions.

It is also an excellent learning project because it naturally exposes you to:

* Linux internals;
* observability;
* system programming;
* event correlation;
* knowledge representation;
* anomaly detection;
* dataset engineering;
* model fine-tuning;
* evaluation;
* agent safety.

Your first milestone could be very concrete: **detect and explain five classes of Linux health incidents on Arch, with no AI used for diagnosis and an optional local LLM used only to rewrite the structured report.** That would already be a useful HoloNight feature and the correct foundation for later model training.

[1]: https://www.freedesktop.org/software/systemd/man/journalctl.html?utm_source=chatgpt.com "journalctl"
[2]: https://docs.kernel.org/networking/devlink/devlink-health.html?utm_source=chatgpt.com "Devlink Health"
[3]: https://huggingface.co/docs/peft/developer_guides/quantization?utm_source=chatgpt.com "Quantization"
[4]: https://www.freedesktop.org/software/systemd/man/journald.conf.html?utm_source=chatgpt.com "journald.conf"
[5]: https://huggingface.co/docs/trl/peft_integration?utm_source=chatgpt.com "PEFT Integration"
