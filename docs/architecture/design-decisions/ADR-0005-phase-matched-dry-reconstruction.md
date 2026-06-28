# ADR-0005 — Phase-matched dry reconstruction `A(dry)` for the Multiband Mix

**Status:** Accepted

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

## Consequences
- Mono sum holds at any Mix (test guards Mix=50%).
- `Mix=0` remains a bit-exact null.
- Cost: a second crossover bank running in lockstep.

## Related code
- `src/dsp/MultibandWidth.cpp:154-168` (dry bank), `:104-123` (lockstep glide)
- `src/dsp/AnamorphEngine.cpp:655-690` (A(dry) production), `:726-759` (smoothstep Mix)

Evidence [Verified]:
- Source: src/dsp/MultibandWidth.cpp:154-168; src/dsp/AnamorphEngine.cpp:726-759
- Tests: testMultibandMonoCompat, testMultibandUnityMatch
- History [Partially Verified]: README:183-191 (0.7.2)
