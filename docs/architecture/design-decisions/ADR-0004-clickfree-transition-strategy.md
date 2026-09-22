# ADR-0004 — Click-free transition strategy (duck / crossfade / warm monitor)

**Status:** Accepted — **amended by [ADR-0035](ADR-0035-oversampling-path-crossfade.md)** (2026-09-03) and by the **Correction of 2026-09-21** below (a fourth `discreteDiffers` exclusion, and one path that did not keep this ADR's own swap-at-the-bottom promise): engaging or disengaging the oversampling **wrap** moves from the duck class to the crossfade class, on the evidence that a duck cannot mask it (the duck's gain is applied downstream of the wideners' delay lines). Every other transition keeps the mechanism assigned here; an oversampling **factor** change still ducks.

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
`discreteDiffers` — joined, since the Correction of 2026-09-21 below, by a `dimMode` move that
reaches no module.

## Consequences
- Toggles are click-free during playback **and** while stopped/paused/zero-buffer (no ghost).
- A subtle invariant: SoloMonitor and the crossfades must run every block even when "inactive,"
  or their settled-state advance breaks (the 0.8.7 regression: gating SoloMonitor on the
  instantaneous `mbEnable` reintroduced a click — fixed by always running it, mask-gated).

## Correction, 2026-09-21 — the duck is for changes that can be *heard*, and it must keep its own promise

Two defects found by measurement (worklog
`../../../worklogs/R4_F11_F8_MEASUREMENT_AND_RESOLUTION.md`), both in mechanism 1. Neither
changes the three mechanisms or which control belongs to which; both make mechanism 1 behave the
way this Decision already says it does.

### 1. A fourth exclusion: `dimMode` when neither side is Dimension D

`dimMode` is read by exactly one line — `chorus.setDimMode (p.dimMode)`, inside
`else if (p.algorithm == Algorithm::DimensionD)` — so under any other algorithm the value reaches
no module and there is nothing for a duck to swap at silence. It was nevertheless in
`discreteDiffers`, so a host lane crossing one of its step boundaries opened a full-depth duck
every time.

**Measured** (1 kHz sine, Haas at amount 0.7, width 1.4, multiband on, steady output as dB against
the identical un-automated engine; confirmed through `AnamorphAudioProcessor` +
`setValueNotifyingHost` to within 0.03 dB of the engine-level figure):

| toggle interval | steady output |
|---|---|
| 2.667 ms | **−42.21 dB** |
| 5.333 ms | −30.30 dB |
| 10.667 ms | −18.82 dB |
| 21.333 ms | −9.01 dB |

Two properties of mechanism 1 that this ADR did not previously state, and that the table makes
explicit:

- **The governing variable is the toggle interval in milliseconds, not the block count.** 2.667 ms
  reads −42.21 dB at 48 kHz/256, at 96 kHz/512 and at 192 kHz/1024 alike, to two decimals. The
  re-arm guard fires only from `FadeIn` and does **not** reset `switchPhase`, so a lane that keeps
  crossing boundaries restarts the ~6 ms fade-out from wherever the ~28 ms fade-in had reached.
- **The duck is not perpetual.** Once the automation stops the level is back within 0.5 dB in
  8–10 blocks (21–27 ms at 48 kHz/128) — the fade-in's own length, no more.

The exclusion is written symmetrically: the duck still fires if **either** side is Dimension D.
When only one side is, `algorithm` already differs and the guard changes nothing; the one
behaviour removed is a duck for a `dimMode` move between two states where no module can observe
it. Nothing is lost by not ducking, because `sameParameters` still compares `dimMode`: the value
is adopted the ordinary continuous way, and a later switch **to** Dimension D is an `algorithm`
difference that ducks, adopts the whole snapshot at the bottom and runs `chorus.setDimMode` with
the value already in `p`.

`haasSide` is deliberately **not** given the same treatment: `haas.setSide` runs unconditionally,
so that value reaches a module whatever the algorithm is. The test for an exclusion is "does the
field reach a module", not "does the algorithm use it".

Post-fix the inert case is **bit-exact** against the un-automated engine — zero differing samples
at 44.1/48/96 kHz and blocks 64/128/512 — while every audible discrete probe (band count,
algorithm, oversampling factor, Band Solo, `haasSide`) is byte-identical to its pre-fix figures.

### 2. A retarget during the fade-out must refresh `pendingAlgoReset`

Mechanism 1 promises to "swap state at the silent bottom (clearing stale tails/oversamplers)".
`pendingAlgoReset` is what clears the widener modules there, and it was recomputed at all three
duck **entries** but not on the fourth path into `pendingP`: a plain retarget arriving while the
fade-out is already running, which the `FadeIn`-only re-arm guard lets fall straight through. So
an ordinary two-step sequence — band count on one block, algorithm on the next — reached the
bottom, adopted the new algorithm and skipped the clear.

It can only be **heard** on a change between Chorus and Dimension D, because a module is processed
only while it is the selected algorithm, so every other incoming module starts from silence
anyway. Those two are voices of one `ChorusEngine`, and there the incoming voice inherited a delay
line full of the outgoing voice's audio. Measured through `AnamorphAudioProcessor` with host-style
parameter writes, against the identical end state reached by the entry route: peak **0.587**
(Chorus → Dimension D, block 64) and **1.519** (Dimension D → Chorus, block 128) on a
0.7-amplitude source; 0.000 for Haas → Velvet / Chorus / Dimension D and for Chorus → Haas. With
the flag refreshed, every route is bit-identical to the entry route.

### Consequences of the correction
- An inert discrete change costs nothing at all, and an audible one ducks exactly as before.
- A duck's bottom now clears the outgoing algorithm however the target arrived.
- Regression protection: `testInertDiscreteChangeDoesNotDuck` (Test 55) and
  `testAlgoResetSurvivesMidFadeRetarget` (Test 57) in `tests/dsp_tests.cpp`. Both fail against the
  pre-correction engine — 27 and 12 checks respectively.

## Related code
- `src/dsp/AnamorphEngine.cpp:313-340` (`discreteDiffers`, exclusions), `:480-562` (switch machine)
- `:819-829` (raised-cosine duck), `:872-888` (`bypassBlend`), `:655-707` (`mbEnableBlend`)
- `:831-845` (SoloMonitor every-block); `src/dsp/SoloMonitor.cpp:59-109`

Evidence [Verified]:
- Source: src/dsp/AnamorphEngine.cpp:313-340, 1207-1418; src/dsp/SoloMonitor.cpp:59-109
- Tests: testNoClicksAcrossTransitions, testSoloNoGhostInSilence, testBypassCrossfadeClickFree,
  testMultibandEnableCrossfadeClickFree, testSoloMultibandEnableClickFree,
  testInertDiscreteChangeDoesNotDuck, testAlgoResetSurvivesMidFadeRetarget
- History [Partially Verified]: CHANGELOG.md [0.8.1], [0.8.6], [0.8.7]
- Related incidents: `../../POSTMORTEMS.md` INC-001 (Solo ghost), INC-005 (Bypass click),
  INC-007 (Multiband Enable mute), INC-009 (Solo + Multiband Enable click)
