# HoloNight Daemon (`holonightd`) Roadmap

This document outlines the strategic engineering roadmap for `holonightd` — a deterministic, evidence-based Linux diagnostic daemon with safe remediation capabilities and optional AI-powered explanation and classification.

This roadmap is derived from the vision detailed in:
- [01-raw-design.md](file:///home/andrii/Projects/pet/holonight/holonightd/docs/ideas/01-raw-design.md) — Architecture, Safety Model, Diagnostic Strategy & ML Progression
- [02-delegate-to-model.md](file:///home/andrii/Projects/pet/holonight/holonightd/docs/ideas/02-delegate-to-model.md) — Background AI Integration for Presentation & Summarization

---

## 🎯 Strategic Principles

1. **Deterministic Diagnostics First**: Observability $\rightarrow$ Normalized Facts $\rightarrow$ Deterministic Rules $\rightarrow$ Controlled Probes $\rightarrow$ Dataset $\rightarrow$ Narrow ML Models. Never delegate system diagnostics to raw, unconstrained LLM shell generation.
2. **Strict Safety Model**: Diagnostics are read-only; actions are explicitly registered and classified by risk level. High-risk/root operations require Polkit authorization and explicit confirmation.
3. **Structured Event Model**: All collectors emit normalized JSON-compatible observation events.
4. **Decoupled Architecture**: `holonightd` collects and diagnoses system state; `Background AI` handles user-facing explanation and presentation; `holonight-shell` / UI displays status and incident details.
5. **Anti-Spam Notification Policy**: Low-urgency events are recorded in the Health Center; notifications are reserved for high/critical incidents.

---

## 📊 Roadmap Overview & Status Legend

- `[x]` Completed
- `[ ]` Planned / In Progress

---

## 🚀 Phases & Milestones

### Phase 1 — Core Foundation & Deterministic Diagnostic Engine (v0.1)
> **Goal**: Establish the normalized event model, initial system collectors, rule-based diagnostic engine, and basic CLI status reporting. No AI models required.

- [x] **1.1 Standardized Observation Event Schema**
  - [x] Implement C++23 event structure matching normalized schema (`timestamp`, `source`, `category`, `subject`, `signal`, `value`, `severity`, `attributes`).
  - [x] Implement SQLite/JSON persistence layer for observation events.
- [x] **1.2 Core System Collectors**
  - [x] **Systemd / Journal**: Monitor failed units, unit restart counts, boot performance (`systemd-analyze`), and coredumps.
  - [x] **Storage / Filesystem**: Integrate `statvfs` disk space usage scanner and NVMe/SMART health checks.
  - [x] **Memory & Kernel**: Detect OOM-killer events, memory pressure metrics (`/proc/pressure/memory`), and critical kernel log signatures.
  - [x] **Package State Adapter (Pacman)**: Detect running vs installed kernel mismatch, partial upgrade states, broken `.pacnew` files, and database lock failures.
- [x] **1.3 Declarative Knowledge Base & Rule Engine**
  - [x] Implement YAML/JSON diagnostic rule schema (conditions, evidence required, candidate causes, suggested actions).
  - [x] Build rule evaluation engine to match collected events against diagnostic rules.
- [x] **1.4 Basic Daemon Loop & CLI Status Output**
  - [x] Refine `holonightd` daemon job scheduler and configuration parser (`holonightd.toml`).
  - [x] Add basic CLI formatting to display active incidents, evidence, and recommended actions.

---

### Phase 2 — Incident Correlation, Safety Model & D-Bus Service (v0.2)
> **Goal**: Group raw events into high-level incidents, enforce action safety policies via Polkit, and expose D-Bus interface.

- [ ] **2.1 Incident Correlation Engine**
  - [ ] Implement time-window and causality event grouping (e.g., package upgrade $\rightarrow$ kernel update $\rightarrow$ missing module failure).
  - [ ] Add confidence calculation and severity scoring for incidents.
- [ ] **2.2 Safety Model & Action Registry**
  - [ ] Implement registered action execution engine (Class 1: Read-only, Class 2: Transient, Class 3: System-changing, Class 4: Destructive).
  - [ ] Disallow raw model shell string execution; only execute pre-registered binaries and arguments.
- [ ] **2.3 Privileged Helper & Polkit Integration**
  - [ ] Design and implement separate privileged helper daemon for root operations.
  - [ ] Implement D-Bus authorization checks using `polkit`.
- [ ] **2.4 D-Bus Interface (`org.holonight.Health1`)**
  - [ ] Implement D-Bus service interface (`ListIncidents`, `GetIncident`, `RunScan`, `ProposeActions`, `ExecuteAction`).
  - [ ] Emit D-Bus signals for `IncidentChanged` and status alerts.
- [ ] **2.5 Notification Suppression & Health Center Policies**
  - [ ] Implement incident recurrence tracking and cooldown timers.
  - [ ] Classify incidents into quiet Health Center items vs desktop popup notifications.

---

### Phase 3 — Desktop Experience Collectors & Dynamic Probe Registry (v0.3)
> **Goal**: Expand Linux desktop diagnostics and build safe, interactive diagnostic probes.

- [ ] **3.1 Desktop Session Collectors**
  - [ ] **Audio Subsystem**: PipeWire & WirePlumber node, route, and crash detection.
  - [ ] **Display & Graphics**: Wayland compositor status, GPU acceleration checks, driver/kernel module mismatch detection.
  - [ ] **Desktop Integration**: Desktop portals status, notification daemon availability, DBus service activation failures.
- [ ] **3.2 Multi-Package Manager Abstraction**
  - [ ] Extract `PackageHealthProvider` interface.
  - [ ] Implement `AptHealthProvider` (Debian/Ubuntu) and `DnfHealthProvider` (Fedora/RHEL).
- [ ] **3.3 Dynamic Diagnostic Probe Registry**
  - [ ] Build registry for on-demand diagnostic checks (e.g., DNS resolution check, default gateway ping, DBus ping).
  - [ ] Implement execution graph for multi-step probe investigation.

---

### Phase 4 — Background AI Integration for Presentation (v0.4)
> **Goal**: Connect `holonightd` structured JSON diagnoses to the Background AI service for user-friendly presentation.

- [ ] **4.1 Background AI Client Integration**
  - [ ] Implement client integration to request non-interactive presentation inference from HoloNight Background AI.
- [ ] **4.2 Health AI Task Profiles**
  - [ ] Define `health-notification` profile (concise, low latency, low token count).
  - [ ] Define `health-detailed-report` profile (in-depth explanation, beginner vs technical detail levels, risk explanation).
- [ ] **4.3 Technical Depth Adaptation & Fallbacks**
  - [ ] Generate structured markdown incident reports from JSON artifacts.
  - [ ] Implement non-AI template fallback when no local or cloud LLM is configured.

---

### Phase 5 — Linux Failure Laboratory & Dataset Engineering (v0.5)
> **Goal**: Build automated VM-based testing infrastructure to inject real Linux failures, collect ground-truth diagnostic datasets, and evaluate system accuracy.

- [ ] **5.1 QEMU/KVM Failure Laboratory**
  - [ ] Build VM automation pipeline (Arch, Fedora, Ubuntu) using QEMU/KVM + Cloud-Init.
  - [ ] Implement failure injection scenarios (systemd loop, broken DNS, full disk, invalid `/etc/fstab`, kernel module mismatch).
- [ ] **5.2 Ground-Truth Dataset Pipeline**
  - [ ] Create automated scenario runner to execute failure $\rightarrow$ capture observations $\rightarrow$ verify expected diagnosis.
  - [ ] Implement anonymized incident bundle exporter for user-consented real-world logs and user feedback (`diagnosis_correct`, `resolution_status`).
- [ ] **5.3 Synthetic Event Generator**
  - [ ] Build synthetic event variations generator (varying distro, kernel version, timing, noise events) to augment dataset without log memorization.

---

### Phase 6 — Learned Components & Narrow ML Models (v0.6+)
> **Goal**: Introduce specialized, lightweight ML models to enhance classification, evidence ranking, and root-cause candidate selection.

- [ ] **6.1 Model A: Micro Journal Event Classifier**
  - [ ] Train lightweight encoder / ONNX model to classify noisy journal events into normalized signals.
  - [ ] Benchmark CPU inference latency and compare against regex/rule baselines.
- [ ] **6.2 Model B: Evidence Reranker**
  - [ ] Train cross-encoder/reranker to select top-N relevant log lines out of thousands of candidate events surrounding an incident.
- [ ] **6.3 Model C: Root-Cause Candidate Ranker**
  - [ ] Train ranker to order rule-generated candidate root causes based on historical VM lab data.
- [ ] **6.4 Evaluation Framework**
  - [ ] Implement automated regression evaluation comparing rule engine vs ML-augmented accuracy.

---

### Phase 7 — Adaptive Autonomous Diagnostic Agent (v1.0)
> **Goal**: Enable adaptive multi-step investigation while maintaining strict safety guarantees and full UI integration.

- [ ] **7.1 Adaptive Safe Investigation Loop**
  - [ ] Allow ML model / decision engine to select next safe, read-only diagnostic probe from registry based on intermediate findings.
- [ ] **7.2 Full UI Integration**
  - [ ] Integrate D-Bus client with `holonight-shell` (status indicators, notification center).
  - [ ] Integrate settings and diagnostic toggles in `holonight-settings`.
- [ ] **7.3 Production Release & Hardening**
  - [ ] Conduct security audit on Polkit helper and D-Bus interfaces.
  - [ ] Perform long-running stability testing across supported Linux distributions.

---

## 🛠️ Technology Stack Summary

- **Daemon Core**: C++23, CMake, SQLite3, sdbus-c++ / Qt D-Bus, Polkit.
- **Rules & Schemas**: YAML, JSON Schema.
- **AI Presentation Service**: HoloNight Background AI (local Ollama / small LLM / cloud fallback).
- **ML Experimentation (Phases 6-7)**: Python, PyTorch, Hugging Face Transformers, PEFT/LoRA, ONNX Runtime.
- **Failure Lab**: QEMU/KVM, libvirt, Cloud-Init, Ansible/Python orchestration.
