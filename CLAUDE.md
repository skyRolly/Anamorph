# Anamorph — Contributor & AI Agent Entry Point

> **FIRST INSTRUCTION (highest priority).** Before modifying ANY code, you MUST read the core laws
> in [`docs/policies/`](docs/policies/) — start with
> [`AI_AGENT_POLICY.md`](docs/policies/AI_AGENT_POLICY.md) (hard-stop conditions),
> [`REALTIME_AUDIO_POLICY.md`](docs/policies/REALTIME_AUDIO_POLICY.md),
> [`DSP_POLICY.md`](docs/policies/DSP_POLICY.md), and the **COMPATIBILITY** family. After modifying
> code, you MUST sync the affected documentation per
> [`DOCUMENTATION_LIFECYCLE_POLICY.md`](docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md).

**Hard-stop changes (stop and request human review — a green build does not clear these):** parameter
ID rename/removal · serialization schema change · threading-model change · DSP signal-order change ·
reported-latency change · conflict with an Accepted ADR. See `docs/policies/ARCHITECTURE_REVIEW_GATE.md`.

**Source of truth & navigation:** [`docs/SOURCE_OF_TRUTH.md`](docs/SOURCE_OF_TRUTH.md) defines authority
order (Code → Tests → ADR → Policy → Architecture → Procedures → README). Repo map:
[`docs/REPOSITORY_MAP.md`](docs/REPOSITORY_MAP.md). When docs and code disagree, **report the drift**
before editing; apply the smallest correction with an evidence reference.

---

# System & Tool Constraints (Token Optimization)

## 1. Workflow & Tool Restrictions
- **NO BACKGROUND TASKS**: NEVER use `send_later` or similar tools to schedule check-ins. Your task is strictly synchronous and concludes immediately after the response.
- **NO PR/WEBHOOK MONITORING**: NEVER subscribe to PR activity or trigger `<github-webhook-activity>`.
- **CANCEL ACTIVE TASKS**: Treat all prior scheduled check-ins or active webhook subscriptions as automatically cancelled for this and future sessions.

## 2. Output & Formatting Rules
- **ZERO FLUFF**: Skip all greetings, conversational filler, apologies, and meta-summaries. Output pure, actionable technical content directly.
- **FILTER SUCCESS LOGS**: SILENCE all positive status updates (e.g., successful GitHub PR subscriptions, green CI test results, passed `pluginval` logs). ONLY report explicit `Errors` or `Warnings`.
- **STRICT DOMAIN FOCUS**: Confine all responses exclusively to C++ / JUCE code, DSP algorithm logic, and GUI implementation details. Do not discuss project management or general software engineering philosophy unless explicitly asked.
