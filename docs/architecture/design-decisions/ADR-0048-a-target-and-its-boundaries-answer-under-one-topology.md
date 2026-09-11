# ADR-0048 — A target and its boundaries answer under one topology, and a frequency needs no topology at all

**Status:** Accepted (maintainer instruction, 2026-09-09 — *release-and-add topology review round*).

**Completes [ADR-0046](ADR-0046-a-derivation-answers-under-the-topology-it-was-given.md); supersedes
nothing.** ADR-0046 gave `bandAtX` and `handleNearX` the count to answer under, and converted
`mouseWheelMove`, `mouseDown`'s handle/width/alt branches and `mouseDoubleClick` to one reading per
handler. It missed one branch. This ADR closes it, and records the measured reason the *symmetric*
change on the other side of the same call was **rejected**.

## Context

`mouseDown`'s add branch derives the band under the pointer from the LATCHED count and then asks
`bandAddTarget` where that band's edges are:

```cpp
const int b = bandAtX (p.x, gestureBands);   // answers under the press's own topology
...
if (bandAddTarget (b, p.x, ax))              // reads mbBands AGAIN, for itself
{
    int addedBands = bandCount();            // a third reading, dead on arrival
    const int idx = addBandAt (xToFreq (ax), addedBands);
```

and `bandAddTarget` decided the band's right edge from its own reading:

```cpp
const int N = bandCount();
const float hi = (b < N - 1 ? freqToX (crossover (b)) : r.getRight()) - 6.0f;
```

Every call in that span is a pure read — `bandAtX`, `nearWidthLine`, `bandAddTarget` — so nothing
dispatches and **reentrancy cannot cross the window; only another thread can**, which is the same
reachability class as ADR-0046 and ADR-0047 and the reason no deterministic test can enter it.

## Problem

The target index and the boundary come from two readings of a parameter three threads write. A count
raised between them flips the `b < N - 1` ternary: the click is clamped against a split edge that
belongs to a layout the press never saw, and the band is added where the user did not click.

Worked, and then measured. Two bands, split at 1 kHz, `mbFreqMid` parked at 8 kHz. A click at 15 kHz
sits in the top band, whose right edge is the plot edge, and the control places the new split at
**15030.7 Hz** every time. With the count raised to three between the two readings, band 1's right
edge becomes the 8 kHz split and the same click is clamped to just under it.

`AnamorphStateTests --add-target-probe` drives exactly that: an automation thread moving `mbBands`
while the message thread clicks. The verdict is a message-thread write into [7 kHz, 8 kHz) — a value
the correctly targeted path never produces and the lane never writes.

| | run 1 | run 2 | run 3 | pooled |
|---|---|---|---|---|
| before | 6 / 1600 | 28 / 1600 | 21 / 1600 | **55 / 4800 (1.1 %)** |
| after | 0 | 0 | 0 | **0 / 4800** |

The "no add" rate — clicks the existing ADR-0040 per-store proofs abandon because the lane moved the
count mid-burst — is **1534–1582 before and 1569–1588 after**, so the change suppresses no add that
previously succeeded. That was measured rather than assumed, because a fix that quietly turns clicks
into no-ops would be a worse defect than the one it closes.

## Decision

**`bandAddTarget` takes the topology its caller derived the index under**, exactly as `bandAtX` and
`handleNearX` have since ADR-0046:

```cpp
bool bandAddTarget (int b, float x, float& outX, int n = -1) const noexcept;
```

`mouseDown` passes `gestureBands`. `updateHover` passes the `N` it already reads at the top of the
pass — **and threads it into `handleNearX` and `bandAtX` there too**, which the first draft of this
change did not. Passing `N` to the add target alone would have moved the mismatch one line up rather
than closing it: the hover's band index would still have come from a fresh reading while its edges
came from `N`. The audit caught that in the working tree; it is the same defect this ADR is about,
one severity band down (display only, so it cannot fail open), and it costs two reads to remove
rather than to reason about. `-1` keeps the live read for any caller with nothing to name. The third
reading — `addedBands = bandCount()`, which `addBandAt` overwrites with its own before the caller can
use it — is gone.

## What was rejected, and why it is not a scope decision

The obvious symmetric change is to give `addBandAt` the `expectedBands` contract `removeBand` has
carried since ADR-0039, so that an add refuses rather than acting under a topology it did not
validate. It was implemented, measured, and **removed**:

* **It closed nothing.** `--add-target-probe` reports 0 misplacements with and without it, across
  4800 clicks each. The clamp argument is what the defect needed.
* **It is wrong in principle, and the principle is already written down.** `removeBand`'s input is a
  BAND INDEX, and a count change RETARGETS an index onto a different band — which is why ADR-0039
  had to refuse rather than clamp. `addBandAt`'s input is a FREQUENCY, and a frequency means the same
  thing under every topology. That is exactly the asymmetry [ADR-0045](ADR-0045-a-positional-latch-is-void-once-its-topology-moves.md)
  ruled for `commitFreqEditor`, which needs no topology stamp for the same reason.
* **It cost the user a click.** Under a moving lane it turned an add the user asked for into a no-op.

The asymmetry between `addBandAt` and `removeBand` is therefore deliberate and now recorded at both
sites, rather than being a gap someone will "fix" later.

## Consequences

* The add branch answers under one topology end to end: the index, the band's edges and the hover
  affordance that offered the position.
* One parameter read is removed from the press path and one from the hover path; none is added.
* Behaviour with nothing racing is unchanged — the whole suite is unchanged at 2814 checks, and the
  probe's control places the split at 15030.7 Hz before and after.
* **The count is only half of a topology, and this closes only that half.** `lo` and `hi` still read
  `crossover()` live, so the band's edges can be split VALUES a same-count layout change has already
  repositioned. Closing that needs ADR-0047's instrument rather than this one — a single capture of
  the split array shared by the derivation and the target — and it is recorded as a residual rather
  than reached for. Also worth stating: only the RISING count direction misplaces. A count falling in
  the window widens the clamp, and one rising past four drops the click entirely — fail-safe but
  lossy, in ADR-0046's own vocabulary.
* **The deterministic suite cannot see this defect, and State test 82 says so in its own header.**
  Reverting this ADR leaves all 2814 checks green. The probe is the coverage, and the `linux` job
  runs it as an assertion. Its discriminating power is real but probabilistic — pooled 0.83 %
  across every pre-fix run — so a single CI run is a strong detector, not a certain one.
