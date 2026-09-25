# ADR-0005 — Phase-matched dry reconstruction `A(dry)` for the Multiband Mix

**Status:** Accepted — **amended by the Correction of 2026-09-21** below: `A(dry)` now crosses into and out of use on the Multiband Enable crossfade's own curve, instead of being switched in one sample.

## Context
The Multiband uses Linkwitz-Riley crossovers, which impose allpass phase on the wet path. At
`Mix < 100%` the dry/wet recombination must not comb-filter, and mono compatibility (`L+R`) must
hold.

## Problem
A naïve dry path (the raw conditioned input) carries no crossover phase, so mixing it against the
phase-shifted wet combs — and because that phase sits on the Mid, it breaks the mono sum.

## Options
- **A. Mix against the raw dry.** Combs at partial Mix; breaks mono compatibility.
- **B. Reconstruct the dry through the *same* gliding crossovers at unit width (`A(dry)`).** Chosen.

## Decision
`MultibandWidth` runs a **parallel dry bank** sharing the wet's exact gliding cutoffs to produce a
phase-matched `A(dry)`. The Mix crossfades from the **clean** dry (bit-exact null at `Mix=0`) to
`A(dry)` over the first ~5% of Mix (smoothstep, zero slope at 0) so partial Mix never combs while
`Mix=0` stays sample-exact. `A(dry)` also serves as the Level-Match dry reference so the allpass
ripple cancels (≈0 dB at unit width, Multiband on or off — see ADR-0007). This is the recurring
code reference "Known Issue #1" — a *closed* design constraint, not an open bug.

## Correction, 2026-09-21 — `A(dry)` must arrive and leave on a crossfade, not on a block flag

The Decision above crossfades the dry source from clean to `A(dry)` **over Mix**, and that part
held. What it did not cover is the other axis: `A(dry)` exists only while the multiband bank is
running, and the flag the Mix stage picks its dry source from (`dryAligned`, set inside
`if (mbActive)`) is a **block** constant. Multiband Enable is mechanism 2 of
[ADR-0004](ADR-0004-clickfree-transition-strategy.md) — a ~12 ms output crossfade — but that
crossfade covered only the wet contribution. The dry term therefore jumped between `A(dry)` and
the clean dry in **one sample**, at the block boundary where `mbActive` changed, with the Mix
smoothstep sitting at 1 and nothing fading it:

> step = `(1 − Mix) · |A(dry) − dry|`

On a **disable** the boundary is the first block after the ~12 ms blend reaches 0; on an **enable**
it is the blend's first sample, where the multiband contribution is still ~0 and nothing masks it.

**Measured** at 48 kHz, crossovers 200/900/3500 Hz, widths 1.6/0.6/1.5/0.7, as the worst
single-sample delta over the transition against the same signal's own median delta in a steady
window:

| case | before | after |
|---|---|---|
| 1 kHz, 4 bands, Mix 0.05, disable | **+3.9 dBFS**, 18.1× | −18.6 dBFS, 1.3× |
| 100 Hz, 4 bands, Mix 0.25, disable | −13.4 dBFS, 23.7× | −37.1 dBFS, 1.5× |
| 100 Hz, 4 bands, Mix 0.25, block 64 | −1.8 dBFS, **88.8×** | −37.1 dBFS, 1.5× |
| 100 Hz, 4 bands, Mix 0.05, enable | −4.8 dBFS, 63.5× | −36.9 dBFS, 1.6× |

and **identically zero**, before and after, in the three places the Decision's own structure rules
it out: at `Mix = 0` (the smoothstep is 0, so `A(dry)` is never read — the bit-exact null holds),
at `Mix = 1` (the H4 gate drops the dry term altogether), and at one band (no crossover, so
`A(dry) == dry`). The last of these is the control that identifies the mechanism.

**The fix** glides `A(dry)` toward the clean dry on `mbEnableBlend`'s own per-sample value, inside
the loop that already crossfades the wet. That moves the source switch to the instant at which the
two are equal: a disable reaches the boundary with the buffer already holding the clean dry, and an
enable starts the blend at the clean dry the previous block was using. At a settled blend the loop
does not run at all, so every fully-enabled and fully-disabled state — and the `Mix = 0` null —
stays bit-exact.

**One consequence for ADR-0007.** `A(dry)` is also the Level-Match dry reference. For the ~12 ms of
the fade that reference now follows the dry actually being mixed rather than the pure
reconstruction, which is the quantity Match measures against; outside the fade nothing changes.

**Why the existing coverage missed it.** `testMultibandEnableCrossfadeClickFree` (Test 23) runs at
the default `Mix = 1`, where there is no dry term to switch. The defect lives strictly at
`0 < Mix < 1` with more than one band. `testMultibandEnableDrySourceNoStep` (Test 56) now covers
both directions, 2 and 4 bands, Mix 0.05–0.75 and blocks 64–1024, with the one-band case as a
control; it fails against the pre-correction engine on 20 of its 22 checks.

## Consequences
- Mono sum holds at any Mix (test guards Mix=50%).
- `Mix=0` remains a bit-exact null.
- Since the correction above, engaging or disengaging the Multiband at partial Mix moves the dry
  source on a crossfade rather than a one-sample switch.
- Cost: a second crossover bank running in lockstep.

## Related code
- `src/dsp/MultibandWidth.cpp:154-168` (dry bank), `:104-123` (lockstep glide)
- `src/dsp/AnamorphEngine.cpp:1104-1195` (A(dry) production), `:726-759` (smoothstep Mix)

Evidence [Verified]:
- Source: src/dsp/MultibandWidth.cpp:154-168; src/dsp/AnamorphEngine.cpp:1256-1318
- Tests: testMultibandMonoCompat, testMultibandUnityMatch, testMultibandEnableDrySourceNoStep
- History [Partially Verified]: CHANGELOG.md [0.7.5]-[0.7.0] (0.7.2)
