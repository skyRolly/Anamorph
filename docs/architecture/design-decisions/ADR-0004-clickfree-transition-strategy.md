# ADR-0004 — Click-free transition strategy (duck / crossfade / warm monitor)

**Status:** Accepted

## Context
Toggling discrete controls (algorithm, routing, band count, OS path) or jumping many params at
once (A/B, preset, undo) steps the signal and clicks. Some toggles (Bypass, Multiband Enable,
Band Solo) must transition without any mute or dropout.

## Problem
A single mechanism does not fit every case: a hard discrete rewire needs silence to swap state;
a continuous-feeling toggle must not mute.

## Options
- **A. Per-control bespoke click patches.** Rejected — unmaintainable; 0.8.1 explicitly folded
  these into one layer.
- **B. Three complementary mechanisms chosen by the kind of change.** Chosen.

## Decision
Three coordinated mechanisms:
1. **Raised-cosine duck** for genuine discrete changes (`discreteDiffers` set): fade out (~6 ms,
   asymmetric), swap state at the silent bottom (clearing stale tails/oversamplers), gentle
   fade-in (~28 ms). Forced bulk swaps (A/B/preset/undo, `requestDuck`) defer *all* params to the
   bottom and snap smoothers there so nothing pops mid-fade.
2. **Output crossfade** for Bypass (`bypassBlend`, ~10 ms) and Multiband Enable (`mbEnableBlend`,
   ~12 ms): the chain stays running, output crossfades between processed and the alternative;
   bit-exact at the endpoints, no duck.
3. **Warm monitor crossfade** for Band Solo (`SoloMonitor`): band-pass filters kept always-on,
   smoothed per-band gains morph passthrough↔soloed — runs **every block** so toggling never
   clicks and never needs a duck.

Bypass, Multiband Enable, and Band Solo are therefore deliberately **excluded** from
`discreteDiffers`.

## Consequences
- Toggles are click-free during playback **and** while stopped/paused/zero-buffer (no ghost).
- A subtle invariant: SoloMonitor and the crossfades must run every block even when "inactive,"
  or their settled-state advance breaks (the 0.8.7 regression: gating SoloMonitor on the
  instantaneous `mbEnable` reintroduced a click — fixed by always running it, mask-gated).

## Related code
- `src/dsp/AnamorphEngine.cpp:158-185` (`discreteDiffers`, exclusions), `:480-562` (switch machine)
- `:819-829` (raised-cosine duck), `:872-888` (`bypassBlend`), `:655-707` (`mbEnableBlend`)
- `:831-845` (SoloMonitor every-block); `src/dsp/SoloMonitor.cpp:59-109`

Evidence [Verified]:
- Source: src/dsp/AnamorphEngine.cpp:158-185,819-888; src/dsp/SoloMonitor.cpp:59-109
- Tests: testNoClicksAcrossTransitions, testSoloNoGhostInSilence, testBypassCrossfadeClickFree,
  testMultibandEnableCrossfadeClickFree, testSoloMultibandEnableClickFree
- History [Partially Verified]: CHANGELOG.md [0.8.1], [0.8.6], [0.8.7]
- Related incidents: `../../POSTMORTEMS.md` INC-001 (Solo ghost), INC-005 (Bypass click),
  INC-007 (Multiband Enable mute), INC-009 (Solo + Multiband Enable click)
