# AI_AGENT_POLICY.md

Repository Governance Policy — the collaboration codex for AI agents (and the expectations for
humans). Read this **before** modifying any code.

## Before changing code (mandatory reading)

1. `SOURCE_OF_TRUTH.md` (authority order, confidence levels).
2. The relevant **System Policies**: `REALTIME_AUDIO_POLICY.md`, `THREADING_POLICY.md`,
   `DSP_POLICY.md`, and the **COMPATIBILITY_POLICY** family.
3. The relevant Architecture doc + any ADR governing the area you touch.

## During work

- **Report drift, never silently fix it (C6).** If documentation and code disagree, state the
  drift with an evidence reference before editing; prefer the minimal correction.
- **Update docs incrementally (C5).** Smallest change that re-syncs; preserve hand-written
  content; do not regenerate a doc wholesale unless explicitly asked. If a structural rewrite
  seems necessary, **stop and ask**.
- **Re-scan the workspace before each phase / on resume (C4).** The filesystem is the authoritative
  execution state, not chat history. Continue incrementally; never regenerate existing work.

## After changing code

- Follow `DOCUMENTATION_LIFECYCLE_POLICY.md` and update the triggered docs **in the same change**.
- Keep the relevant DSP self-tests green (`TESTING_POLICY.md`); add a regression test for any bug fix.
- Update `CHANGELOG.md` per `CHANGELOG_POLICY.md` for user-visible changes.

## Hard Stop conditions (stop and request human review)

The agent must **immediately stop and request Human Review** — not proceed — when it detects any
of:

- **Parameter ID changes** (rename/removal) detected.
- **Serialization schema changes** detected.
- **Threading model changes** detected.
- **DSP signal-order changes** detected.
- **Reported-latency changes** detected.
- **An existing Accepted ADR conflict** detected (the change contradicts an ADR).

These map one-to-one to the `ARCHITECTURE_REVIEW_GATE.md` items. A passing build/test/pluginval
does **not** clear a Hard Stop — only human review does.

## Inherited operational constraints (from `CLAUDE.md`)

The repository's `CLAUDE.md` operational constraints (no background tasks / no PR-webhook
monitoring; output discipline) remain in force and are not overridden by this policy.
