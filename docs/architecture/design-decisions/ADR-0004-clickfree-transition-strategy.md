# ADR-0004 — Click-free transition strategy (duck / crossfade / warm monitor)

**Status:** Accepted — **amended by [ADR-0035](ADR-0035-oversampling-path-crossfade.md)** (2026-09-03), by **[ADR-0057](ADR-0057-a-bulk-swap-adopts-only-a-completely-applied-destination.md)** (2026-09-27: a forced bulk swap's duck starts at the swap's completion — the *Amendment of 2026-09-27* below) and by the **Correction of 2026-09-21** below (a fourth `discreteDiffers` exclusion, and one path that did not keep this ADR's own swap-at-the-bottom promise): engaging or disengaging the oversampling **wrap** moves from the duck class to the crossfade class, on the evidence that a duck cannot mask it (the duck's gain is applied downstream of the wideners' delay lines). Every other transition keeps the mechanism assigned here; an oversampling **factor** change still ducks.

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
   fade-in (~28 ms). Forced bulk swaps (A/B/preset/undo/redo, `requestAbSwitch` / `requestDuck`)
   defer *all* params to the bottom and snap smoothers there so nothing pops mid-fade. Since
   ADR-0057 (the *Amendment of 2026-09-27* below) the forced duck STARTS at the swap's completion,
   not at its request, so the bottom adopts the complete destination.
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

## Note, 2026-09-24 — the Level-Match gain at a duck bottom (ADR-0007, Amendment of the same date)

Decision 1's "snap smoothers there" never covered `matchGainSmooth`: `snapSmoothers()` leaves it out,
because where it lands is ADR-0007's to decide, and its target is fresh only after the bottom block's
loudness measurement. ADR-0007's Amendment of 2026-09-24 decides it: at a bottom that turns Level
Match on and changes nothing the measurement reads, the smoother is landed on the published value
right after that block's measurement — at a forced bottom and, new for mechanism 1, at an ordinary
one (the Level Match toggle), which otherwise snaps nothing. At an A/B bottom the injected slot gain
sets it, as before (ADR-0007, #23); at any other bottom it keeps gliding. The
three mechanisms, and which control belongs to which, are unchanged. (The measurement side of the
same A/B bottom — the analysis is re-armed there when the two slots differ in anything it reads — is
ADR-0007's F13(2) amendment of the same date; it does not move the applied gain.)

## Amendment, 2026-09-27 — a forced swap's duck starts at its completion (ADR-0057, Accepted)

Decision 1 promised that a forced bulk swap is applied *entirely* at the silent bottom. The request
was the swap's only message and it preceded the writes, so the duck started at the next block and
the bottom adopted whichever snapshot it read — inside the writes, when the host processed faster
than real time or the message thread stalled, a mixture neither side holds (`KNOWN_ISSUES.md` KI-032).
[ADR-0057](ADR-0057-a-bulk-swap-adopts-only-a-completely-applied-destination.md) keeps the mechanism and moves its start:

- The request still precedes the first write, and from the block that takes it the engine adopts
  nothing — the source keeps playing at full level, unducked, while the destination is written.
- The swap's completion (a release publication after its last parameter store) starts the forced
  duck, on the first snapshot read between two acquire takes of the request word after it. That
  snapshot is the duck's target, so the bottom — and a host reset that completes the duck — adopts
  the complete destination, with every parameter deferred to it and the smoothers snapped, exactly
  as decision 1 says.
- The bottom never waits: the wait happens before the fade-out, at full level, and it has no cap.

In real time with no message-thread stall the writes end between two blocks, so the request and
the completion are taken in the same block and the swap is bit-identical to what it was: the same
start, the same bottom, the same output. Under burst processing or a stalled message thread the swap
lands later in the rendered timeline, by the application's wall time, and whole. Mechanisms 2 and 3
and the class of every other control are unchanged. Evidence: ADR-0057, *Evidence*; State tests 139
and 140; DSP Test 75.

## Related code
- `src/dsp/AnamorphEngine.cpp:394-421` (`discreteDiffers`, exclusions), `:480-562` (switch machine)
- `:819-829` (raised-cosine duck), `:872-888` (`bypassBlend`), `:655-707` (`mbEnableBlend`)
- `:831-845` (SoloMonitor every-block); `src/dsp/SoloMonitor.cpp:59-109`

Evidence [Verified]:
- Source: src/dsp/AnamorphEngine.cpp:394-421, 1651-1862; src/dsp/SoloMonitor.cpp:59-109
- Tests: testNoClicksAcrossTransitions, testSoloNoGhostInSilence, testBypassCrossfadeClickFree,
  testMultibandEnableCrossfadeClickFree, testSoloMultibandEnableClickFree,
  testInertDiscreteChangeDoesNotDuck, testAlgoResetSurvivesMidFadeRetarget
- History [Partially Verified]: CHANGELOG.md [0.8.1], [0.8.6], [0.8.7]
- Related incidents: `../../POSTMORTEMS.md` INC-001 (Solo ghost), INC-005 (Bypass click),
  INC-007 (Multiband Enable mute), INC-009 (Solo + Multiband Enable click)
