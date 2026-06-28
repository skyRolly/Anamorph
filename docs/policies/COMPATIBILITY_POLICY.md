# COMPATIBILITY_POLICY.md

**Highest compatibility authority.** The unified compatibility contract for the plugin. This
policy governs `SESSION_COMPATIBILITY_POLICY.md` and `PARAMETER_COMPATIBILITY_POLICY.md` (its
subsets) and the latency contract.

## The contract

A user's saved session — in any host, from any prior shipped version — must reload to the same
sound, with automation and presets intact. The following are **absolutely prohibited** unless an
exception (below) is satisfied:

| Prohibited change | Why it breaks the field |
|---|---|
| **Parameter ID rename or removal** | Sessions/automation key by ID. |
| **Serialization field removal** | Old sessions lose state silently. |
| **Preset schema break** | Saved/factory presets stop loading correctly. |
| **Host-visible parameter semantic change** | Automation lanes now mean something different. |
| **Reported-latency behaviour change** | Host PDC desyncs; timing shifts. |
| **Automation behaviour change** | Recorded automation plays back differently. |

## The only exception

A prohibited change may proceed **only if all** of the following are satisfied:

1. an **ADR** records the decision (`ADR_POLICY.md`), and
2. a **migration plan** preserves old sessions (a read path / default for the old form), and
3. the **Release Compatibility Checklist** passes (`procedures/RELEASE_COMPATIBILITY_CHECKLIST.md`), and
4. the change clears the **Architecture Review Gate** (human review).

The reference precedent is the 0.8.4 move of view params out of the APVTS, done with
`InternalState::migrateFromLegacyApvts` (ADR-0010).

## Backward-compatibility paths that must be preserved

- v0.2 bare-APVTS session format (`setStateInformation` else-branch).
- pre-0.6.4 A/B slots (params-only `slotA`/`slotB`).
- pre-0.8.4 legacy APVTS view params (migrated to `InternalState`).

Evidence [Verified]: src/PluginProcessor.cpp:327-396; src/InternalState.h:100-122.

## Subset policies

- **Parameters:** `PARAMETER_COMPATIBILITY_POLICY.md` + ledger `PARAMETER_REGISTRY.md`.
- **Session/serialization:** `SESSION_COMPATIBILITY_POLICY.md` + ledger `SERIALIZATION_REGISTRY.md`.
- **Latency:** `docs/architecture/LATENCY_MODEL.md` (latency changes require an ADR).

## Status taxonomy (for `COMPATIBILITY_MATRIX.md`)

Verified · Partially Verified · Unverified · **Not Supported** (a deliberate exclusion, e.g.
**AAX** and **mono→mono** — these are not "unverified," they are out of scope by decision).
