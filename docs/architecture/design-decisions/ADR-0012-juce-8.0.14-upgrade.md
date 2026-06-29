# ADR-0012 — JUCE dependency upgrade 8.0.8 → 8.0.14

**Status:** Accepted

## Context
JUCE is pinned to an exact tag (ADR-0003, `docs/policies/DEPENDENCY_POLICY.md`). Under that policy
and `docs/policies/ARCHITECTURE_REVIEW_GATE.md`, **any JUCE version bump is a Build System change
that requires an ADR + verification**. A maintenance upgrade from JUCE **8.0.8 → 8.0.14** was
requested. This is the **first dependency bump enforced under that rule** (the bootstrap ADR for it).

## Problem
Move to JUCE 8.0.14 while guaranteeing **no change** to DSP output, reported latency, parameter
semantics, or serialization, and satisfying the policy requirement for an ADR + CI verification.

## Options
- **A. Stay on 8.0.8.** Rejected — forgoes upstream maintenance/bugfixes within the same minor line.
- **B. Bump to 8.0.14 with full CI verification.** Chosen — within the 8.0.x line; low risk, gated by
  the existing self-tests + pluginval.

## Decision
Set `ANAMORPH_JUCE_TAG` **8.0.8 → 8.0.14** (`CMakeLists.txt:33`) — the only logic-bearing change. **No
DSP, signal-chain, parameter, or serialization source is touched.** Verify via CI: build + the 23 DSP
self-tests + pluginval strictness 10 on the authoritative Linux gate.

## Consequences
- CI Linux gate **green** on the upgrade confirms: the build succeeds with JUCE 8.0.14; the 23 DSP
  self-tests pass (bit-exact bypass null, transparency, MS round-trip, Level-Match, crossover-safety)
  → **DSP output unchanged**; `pluginval PASSED at strictness 10`; no parameter/serialization code
  changed → **state compatibility unchanged**.
- The version-lock *rationale* (ADR-0003 / DEPENDENCY_POLICY) is unchanged — only the pinned tag
  advanced within the 8.0.x line.
- Future JUCE bumps follow the same pattern (a new ADR + CI verification). See RISK-001.
- **Open:** the local sandbox cannot build (egress policy blocks fetching `juce-framework/JUCE`), and
  the manual audition (Level 5, DEPENDENCY_POLICY rule 2) is not headlessly verifiable — both are
  noted limitations; CI is the verification path used here.

## Related code
- `CMakeLists.txt:33` (`ANAMORPH_JUCE_TAG "8.0.14"`).

Evidence:
- Source [Verified]: `CMakeLists.txt:33` (tag 8.0.14).
- Verification [Verified]: CI run on commit `41acaa7` — Linux gate **success**; log: `pluginval:
  PASSED at strictness 10 (attempt 1/3)`; the 23 DSP self-tests run before pluginval and the job
  concluded success.
- Local build [Unverified — environment]: sandbox egress policy returns 403 cloning
  `juce-framework/JUCE`; verification performed by CI.
- Policy basis: `docs/policies/DEPENDENCY_POLICY.md` (Upgrade rules), `docs/policies/ARCHITECTURE_REVIEW_GATE.md`.
- History: `CHANGELOG.md [Unreleased]` (the 8.0.8 → 8.0.14 entry); commit `41acaa7`.
