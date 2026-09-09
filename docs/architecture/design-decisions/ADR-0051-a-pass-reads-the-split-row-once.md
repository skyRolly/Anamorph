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

## Applied again 2026-09-09 — the band move, the second caller of this rule

The Decision above is general — *"a pass that has already stamped the row derives from the stamp
rather than stamping again"* — and names exactly one exception, the add branch. When it was written
only `mouseDown`'s handle branch was converted. A follow-up review filed the band-move path as a Bug
(*"band drags adopt later automation"*, `SpectrumImager.cpp:829`), and it is the same rule's second
site. **No new decision, and nothing here is narrowed or amended:** ADR-0051's Status and Decision
stand as accepted, and no `ARCHITECTURE_REVIEW_GATE.md` item is triggered.

**Why the band move is inside the rule, and the add branch still is not.** `beginBandMove` is reached
from `mouseDrag`, whose first statement is `if (gestureIsStale()) { cancelActiveDrag(); return; }`.
So at that point the press's stamp exists *and has just been proved* — all three splits and all four
widths, compared with `juce::exactlyEqual` in normalised units. Between that gate and the call there
is `plot()`, two integer assignments and one float assignment: no store, therefore no dispatch. The
add branch is different for the reason this ADR already gives: `addBandAt` has just changed the count
**and** written the row, so the press's first stamp describes a layout that no longer exists. The
wheel's call is a third case and also legitimate — `cancelActiveDrag()` has cleared `gestureBands`, so
no record is in force there at all.

**This one is worse than a duplicated reading, which is why the review filed it as a Bug and not a
cleanup.** `captureDragOrigins()` is `captureGestureSound(); seedDragOrigins();`, and the stamping
half is a *blanket, provenance-free* copy of the live row into the ownership record. Called where a
proved record is in force, a foreign write landing in the window is not merely missed — it is
**adopted**: written into the record every later check proves against, after which `ownsSplit` and
`ownsWidth` compare the foreign value with itself and answer "mine" for the rest of the gesture.

The width half is the worse one. A band move never writes a width, so `writeCrossovers` has no
per-store check that could catch a laundered one — `gestureW` is proved by the per-event gate alone.
Adopted there, a foreign width change is invisible to that gate, to `mouseUp`'s gate and to `tick`'s
reconcile for the whole rest of the drag: ADR-0040's width half, silently disarmed.

**Evidence, on an instrument built for it** (`--band-move-adopt-probe`; the existing
`--band-move-probe` drives `mbBands` and cannot see a value-half defect at all):

```
before   148 / 18000 band moves
after      0 / 18000
```

one second either way, with the probe's mandatory control line printing *late crossover stores SEEN*
in both. The mutation is the one-word revert.

**Inert outside the race, and measured to be so rather than argued.** With the fix and with the
pre-fix line restored, State test 84 leg A reports the same frequencies to the digit, and the whole
2 860-check suite is unchanged — which is what the gate one step earlier guarantees: past a *passing*
`soundMovedUnderGesture`, `gestureX[k] == freqP[k]->getValue()` bit-for-bit, so the dropped stamp
would have written back identical bits and `convertFrom0to1` is pure arithmetic on them.

**What it does NOT do, stated rather than implied.** It makes the race adoption-free, not write-free.
A foreign width landing in the old window is now refused at the *next* event's gate, so the crossover
burst of the event already in flight still goes out. That is unchanged in kind from any other foreign
write during a drag, and it is what the probe measures as the post-fix behaviour.

`beginBandMove`'s `n` lost its default argument in the same change: the function now depends on its
caller having proved the record a moment earlier, and a defaulted parameter would let a future second
caller reach that dependency with nothing proved and no diagnostic.

State 2 860 / 0, DSP 396 / 0, TSan 0 warnings, valgrind 0 errors, all five probes 0.
`worklogs/SPECTRUMIMAGER_TOPOLOGY_TRANSACTION_AUDIT_v0.9.8.md` §60.
