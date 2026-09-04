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
   the active slot, and — since 0.9.2 (ADR-0024 as amended) — the **preset indicator identity**,
   which is metadata: the sound must restore identically even when the identity does not resolve.
5. **View params are preserved on restore.** `applyStatePreservingView` keeps the current
   `pid::viewParams` (Bypass) across an A/B/undo/preset apply.
6. **`presetBaseline` is a comparison key, not a value the plug-in must reproduce.** It carries a
   signature of the sound a state set was clean at, so `isDirty()` can answer "has the sound moved
   since". Rule 1 governs the FIELD — it is still written, still read, still tolerates absence — and
   nothing about the sound, the name or the identity depends on it. What it is compared against may
   therefore change when the definition of "the same sound" is corrected, and the only consequence
   is which way a modified-marker points. **Recorded change (0.9.7, ADR-0036 §17):** the signature is
   now taken on the grid the plug-in actually renders and stores
   (`anamorph::normalisedAsRendered` = `convertTo0to1(convertFrom0to1(v))`), which is a no-op for
   every stock parameter and snaps the custom `RawChoice`/`RawBool` values. A session saved by an
   older build can therefore restore showing a modified-marker it did not show before, in **two**
   cases: a host-automated sub-step value on a discrete parameter (always), and any of the four
   custom-mapped frequency parameters at a value where the float round trip crosses a 5-decimal
   boundary (roughly 1 value in 500). Cosmetic, no sound change, no field change, and self-correcting
   on the next save or preset load — both recompute the baseline under the new definition. The
   corrected definition is the point: the old one marked a preset clean against a sound its own file
   could not hold.

## Required verification before release

- `[ ] Session reload verified` (save in vN−1, load in vN — sound identical).
- `[ ] Presets migrated` (factory + a user `.anamorph` still load).

These same checks are enforced at release time via the release compatibility checklist
(`docs/procedures/RELEASE_COMPATIBILITY_CHECKLIST.md`).

Evidence [Verified]: src/PluginProcessor.cpp:1048-1296 (write), :595-685 (read), :540-561
(the identity helpers); src/InternalState.h:197-321.

## Enforcement

A serialization schema change is an **Architecture Review Gate** item and an **AI Agent Hard
Stop** (`AI_AGENT_POLICY.md`). Changing this policy requires an ADR.
