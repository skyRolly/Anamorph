# ADR-0010 — Host-hidden `InternalState` for non-musical parameters

**Status:** Accepted

## Context
Oversampling, Window Size, Scope Persistence, Tooltips, UI Animations, and Show Meters are
view/engine-config, not musical parameters. Exposed as VST3 parameters they cluttered the host's
automation list.

## Problem
JUCE's `withAutomatable(false)` does **not** hide a parameter in all hosts — REAPER lists every
VST3 parameter regardless. The only reliable way to hide a parameter is to keep it out of the
parameter tree entirely.

## Options
- **A. Keep them as APVTS params with `withAutomatable(false)`.** Rejected — still listed in REAPER.
- **B. Move them out of the APVTS into a dedicated host-hidden state object.** Chosen (0.8.4).

## Decision
These six controls live in `anamorph::InternalState` (a `juce::ValueTree`, not the APVTS). They:
persist with the session (serialised as the `ANAMORPH_INTERNAL` child), bind two-way to the GUI
via `juce::Value`, and — for Oversampling only — drive the DSP via an atomic with an
`onOversampleChanged` callback so the wrapper re-reports PDC. They never participate in A/B, Undo,
or preset recall. Pre-0.8.4 sessions (where these were APVTS params) are migrated by
`migrateFromLegacyApvts`. The Multiband parameters were intentionally **left** as full APVTS
parameters.

## Consequences
- The host automation list is de-cluttered; these controls cannot be automated.
- A migration path is mandatory (the model for any future parameter-surface change — see ADR-0002).
- This is the one precedent for removing IDs from the APVTS surface; it was done *with* migration.

## Related code
- `src/InternalState.h:10-138` (whole class), `:100-122` (migration)
- `src/PluginParameters.cpp:183-190` (rationale comment); `src/PluginProcessor.cpp:311,345-348`

Evidence [Verified]:
- Source: src/InternalState.h:10-138; src/PluginProcessor.cpp:304-348
- History [Partially Verified]: CHANGELOG.md [0.8.4]; commit 6bd158b
