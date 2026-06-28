# SESSION_COMPATIBILITY_POLICY.md

Subset of `COMPATIBILITY_POLICY.md`. Governs state serialization
(`getStateInformation`/`setStateInformation`). Ledger: `docs/architecture/SERIALIZATION_REGISTRY.md`.

## Rules

1. **Serialization fields are immutable.** No field in `AnamorphRoot`, `ANAMORPH` (APVTS),
   `ANAMORPH_INTERNAL`, or `AB` may be removed or have its meaning changed without an ADR +
   migration.
2. **Additions must tolerate absence.** A new field must have a default applied when an older
   session lacks it, so old sessions still load.
3. **Every legacy read path stays.** The v0.2, pre-0.6.4, and pre-0.8.4 read paths must remain
   (see `SERIALIZATION_REGISTRY.md` → "Legacy root formats").
4. **A save→load round-trip must reproduce** the sound, preset name, dirty-star, both A/B slots,
   and the active slot.
5. **View params are preserved on restore.** `applyStatePreservingView` keeps the current
   `pid::viewParams` (Bypass) across an A/B/undo/preset apply.

## Required verification before release

- `[ ] Session reload verified` (save in vN−1, load in vN — sound identical).
- `[ ] Presets migrated` (factory + a user `.anamorph` still load).

These same checks are enforced at release time via the release compatibility checklist
(`docs/procedures/RELEASE_COMPATIBILITY_CHECKLIST.md`).

Evidence [Verified]: src/PluginProcessor.cpp:198-209,304-396; src/InternalState.h:86-122.

## Enforcement

A serialization schema change is an **Architecture Review Gate** item and an **AI Agent Hard
Stop** (`AI_AGENT_POLICY.md`). Changing this policy requires an ADR.
