# ADR-0051 — A pass reads the split row once

**Status:** Accepted (maintainer instruction, 2026-09-09 — *final topology and gesture review round*,
review finding *"concurrent split moves misplace new bands"* at `SpectrumImager.cpp:616`).

**Completes [ADR-0046](ADR-0046-a-derivation-answers-under-the-topology-it-was-given.md),
[ADR-0047](ADR-0047-a-snapshot-is-one-reading.md) and
[ADR-0048](ADR-0048-a-target-and-its-boundaries-answer-under-one-topology.md); supersedes nothing.**
ADR-0046 made the band **count** one reading per pass. ADR-0048 made a band's edges answer under the
count its index was derived from — and recorded, in the source and in its own index row, the residual
it did not close: `lo` and `hi` still read `crossover()` **live**. This ADR closes that residual.

## Context

Three functions read the split row, and each read it for itself:

```cpp
int  bandAtX       (float x, int n)  // compares xToFreq (x) against crossover (i)
int  handleNearX   (float x, int n)  // measures |x - freqToX (crossover (i))|
bool bandAddTarget (int b, float x, float& outX, int n)
{
    const float lo = (b > 0     ? freqToX (crossover (b - 1)) : r.getX())     + 6.0f;
    const float hi = (b < N - 1 ? freqToX (crossover (b))     : r.getRight()) - 6.0f;
```

`mouseDown` calls all three in one press, so a single click took **three** readings of a three-element
row that three threads write. `updateHover`, `mouseDoubleClick` and `mouseWheelMove` did the same.

## Problem

A band index derived against one row, clamped against another. `outX = jlimit (lo, hi, x)` is a
no-op while the two agree — `b` is by construction the band containing `x` under that row — and
arbitrary once they do not. Both directions misplace:

* `bandAtX` reads a split **below** the click and answers "the band above"; `bandAddTarget` then reads
  it **above** the click, so the band's left edge is to the right of the pointer and the new split is
  dragged **up** to it;
* `bandAtX` reads it **above** and answers "the band below"; `bandAddTarget` then reads it **below**,
  so the band's right edge is to the left of the pointer and the new split is dragged **down**.

Every call in the span is a pure read, so nothing dispatches: **cross-thread only**, the same
reachability class as ADR-0046/0047/0048 and the reason no deterministic test enters it.

`mouseDown` carried a second instance of the same shape. The press stamps the whole row at the top
(`captureGestureSound`), and the handle branch then called `captureDragOrigins()` — which stamps
**again**. The index came from the first stamp and the drag's grab anchor from the second.

## Options

| | |
|---|---|
| **A. Capture the row once per pass and pass it to every derivation and every boundary** (chosen) | The shape ADR-0046 and ADR-0048 already established for the count, extended to the values. `nullptr` keeps the live read for callers that stamp nothing, so nothing else in the class changes. |
| B. Thread only `bandAddTarget` | ADR-0048's own correction says why not: it moves the mismatch one line up, to `bandAtX`. |
| C. A `Layout` struct threaded through `deleteBox`/`soloBox`/`bandLeftX`/`bandRightX` as well | Six more signatures for windows that already fail safe; ADR-0046 drew that line deliberately and this ADR keeps it. |
| D. A lock or seqlock over the multiband set | A threading-model change (gate item) for a window a reordering closes. |

## Decision

> **A pass reads the split row once. Every derivation and every boundary in that pass answers under
> that one reading, and a pass that has already stamped the row derives from the stamp rather than
> stamping again.**

`captureSplits (float*)` takes the row; `splitAt (i, fHz)` answers from it, or live when the caller
has none. `mouseDown` pays **nothing** — it derives the row from the stamp `captureGestureSound` has
already taken (`convertFrom0to1` is pure arithmetic on the value that read returned, so it is the same
measurement, not a second one), and the handle branch calls the new `seedDragOrigins()` instead of
re-stamping. The add branch still re-stamps, and must: `addBandAt` has just changed the count **and**
written the row.

## Consequences

* Four passes — hover, press, double click, wheel tick — now have a single topology, count and row.
* Reads are removed, not added: the press loses a whole stamp and three live reads.
* `updateHover`, `mouseDoubleClick` and `mouseWheelMove` gain one capture each and lose two to four
  live reads.

## Related code

`src/gui/SpectrumImager.cpp` — `captureSplits`/`splitAt` (`:252`), `bandAtX`, `handleNearX`,
`bandAddTarget` (`:638`), `seedDragOrigins` (`:511`), `updateHover`, `mouseDown`, `mouseDoubleClick`,
`mouseWheelMove`.

## Evidence + confidence

**Verified by measurement.** `--add-edge-probe`, added here: two bands, the click aimed near 15 kHz,
an automation lane alternating split 0 between 500 Hz and 16 kHz at a **constant** count, and a
message-thread detector watching both clamp directions.

```
before   15, 11, 22, 27  --  75 of 1600
after     0,  0,  0,  0  --   0 of 1600
```

with the correctly-placed count unchanged across the fix (856 before, 847 after), so no add that
previously succeeded is suppressed — the 75 become adds `addBandAt` abandons on its own proofs, which
is the fail-safe direction.

**The instrument had to be aimed three times, and that is recorded in the probe rather than tidied
away.** A detector written for the upward clamp alone measured 0 both before and after; so did a lane
that moved the split without crossing the click. A verdict of 0 from an instrument that cannot see the
defect is indistinguishable from a verdict of 0 from a defect that is not there, which is the same
false-confidence failure the TSan suppression assertion was corrected for on 2026-09-08.

Wired into the `linux` CI job. State 2 840 / 0.
`worklogs/SPECTRUMIMAGER_TOPOLOGY_TRANSACTION_AUDIT_v0.9.8.md` §47.
