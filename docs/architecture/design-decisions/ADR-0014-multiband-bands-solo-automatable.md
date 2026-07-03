# ADR-0014 — Expose `mbBands` / `mbSolo` to host automation

**Status:** Accepted

## Context
`Multiband Bands` (`mbBands`, Int 1..4) and `Multiband Solo` (`mbSolo`, Int 0..15, 4-bit mask) were
created with `withAutomatable(false)`. They are primarily driven by the `SpectrumImager` drag-to-split
display, not by a conventional knob, so they were kept off the host automation list. They have always
been **serialized** (they travel with state, A/B and — for `mbBands` — presets) and host-**visible** as
VST3 parameters; only the automation-safe flag was off.

## Problem
`withAutomatable(false)` does not actually hide a VST3 parameter in most hosts (REAPER lists it
regardless), so the flag delivered no real "hidden" benefit while making the two multiband controls the
only sound parameters a user could **not** automate or record — an inconsistency with every other
band-affecting control (crossovers, per-band width) which are fully automatable.

## Decision
Remove `withAutomatable(false)` from `mbBands` and `mbSolo` so they are fully automatable, matching the
rest of the parameter set. No ID, range, default, choice ordering, serialization, preset-exclusion
(`mbSolo` stays preset-excluded), or DSP behaviour changes — only the automation-safe flag flips to the
project-wide default (`true`).

## Options
- **A. Leave `withAutomatable(false)`.** Rejected — it does not hide the params in practice and blocks
  a legitimate workflow (automating/recording band-count and solo moves).
- **B. Remove the params from the APVTS tree entirely (make them truly host-hidden `InternalState`).**
  Rejected — they must remain in save/recall, A/B and (for `mbBands`) presets, and are genuine musical
  controls; `InternalState` is for non-musical/view state (see ADR-0010).
- **C. Remove `withAutomatable(false)` (chosen).** Consistent, additive to host capability, and
  compatibility-neutral (ID/range/default/serialization unchanged).

## Consequences
- `mbBands` / `mbSolo` now appear as automatable in the host and can be written by automation lanes.
  Drag-to-split still drives them via begin/end gestures (one undo step per drag).
- No compatibility impact: older sessions load unchanged (same IDs/ranges/defaults); the automation
  flag is not serialized state.
- `PARAMETER_REGISTRY.md` "Auto Safe" is now `yes` for both (footnote ‡).

## Governance
Per `ARCHITECTURE_REVIEW_GATE.md`, changing a parameter's **automatable** flag is a gated Parameter
Registry change requiring human review + an ADR. This ADR + the PR is that review record. The change is
compatibility-neutral (no ID/range/default/serialization change), so it is not a prohibited ID
rename/removal or serialization-schema change.

## Related
- `docs/architecture/PARAMETER_REGISTRY.md` (`mbBands`/`mbSolo` rows, footnote ‡).
- Source: `src/PluginParameters.cpp` (`mbBands` / `mbSolo` layout entries, no `withAutomatable`).
- Related decisions: ADR-0010 (host-hidden `InternalState` — why these stay APVTS, not InternalState).
