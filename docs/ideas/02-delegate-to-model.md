Exactly. I mean reusing the **Background AI infrastructure**, but introducing a dedicated service task for health-report generation.

For example:

```text
Background AI
├── Chat title generation
├── Chat compaction
├── Conversation summary
├── Health report generation
└── Diagnostic explanation
```

The Linux health subsystem would produce a structured, factual incident:

```json
{
  "incident": "nvidia_kernel_module_mismatch",
  "severity": "high",
  "confidence": 0.94,
  "evidence": [...],
  "available_actions": [...]
}
```

Then Background AI would turn it into a user-friendly explanation. The configured background model could be a cheap local model, independent from whichever model the user currently selected in HoloNight AI.

I would keep the boundaries clear:

```text
holonight-health
    detects and investigates
    produces structured diagnosis
              │
              ▼
Background AI service
    explains and summarizes
    adapts technical depth
    prepares notification text
              │
              ▼
HoloNight Shell / Health Center
```

So Background AI should **not** decide whether the disk is failing, invent shell commands, or execute a repair. It should primarily perform presentation-oriented tasks:

* concise notification text;
* detailed explanation;
* likely user impact;
* readable comparison of repair options;
* technical or beginner-friendly versions;
* follow-up conversational answers based on the diagnostic artifact.

There could be two related Background AI task profiles:

```yaml
health-notification:
  max_output_tokens: 120
  preferred_model: small-local
  temperature: 0.1

health-detailed-report:
  max_output_tokens: 1200
  preferred_model: medium-local
  fallback: user-configured-cloud
  temperature: 0.2
```

That makes Background AI more than a chat-specific feature. It becomes a general internal inference service used by HoloNight applications for low-cost, non-interactive AI tasks.
