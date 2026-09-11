# ADR-0046 — A derivation answers under the topology it was given, and a handler reads that topology once

**Status:** Accepted (maintainer instruction, 2026-09-08 — *final topology identity review round*).

**Completes [ADR-0045](ADR-0045-a-positional-latch-is-void-once-its-topology-moves.md); supersedes
nothing.** ADR-0045 gave the wheel latch a topology stamp and gave `resetParam` a proof inside its
own bracket. Both are still right. What neither settled is *which* reading of the count the stamp
and the proof are made of — and this round's review finding is that the wheel made them out of a
**later** reading than the one its index was derived under, which is the one arrangement that cannot
work.

## Context

`bandCount()` is `mbBands` read through the parameter. Three threads write it: the message thread
(this component), the audio thread (host automation, through the format wrapper) and the host state
thread. `bandAtX`, `handleNearX`, `deleteHit` and `soloHit` each used to read it **for themselves**.
So a handler that read the count, derived an index, and then stamped that index with a count was
taking *two* reads of a value three threads write, and pairing an index answered under one with a
stamp taken from the other.

There is no dispatch anywhere in that span. `bandAtX`, `handleNearX`, `crossover`, `bandWidth` and
`bandCount` are plain `p->getValue()` reads; `setValueNotifyingHost` and `beginChangeGesture` are the
only things in this class that dispatch, and neither is called between the reads. **So reentrancy
cannot cross this window — only another thread can.** That is exactly what the review finding says:
*"if automation changes the band count between deriving a target using `bandAtX`, reading or
validating `bandCount`, and recording the topology stamp"*.

## Problem

`mouseWheelMove` took **three** readings of the count in one tick and let the derivation take a
fourth, and it stamped with the last of them:

```cpp
const int N = bandCount();                          // reading 1 -- used as the write BOUND
if (scrollBands >= 0 && bandCount() != scrollBands) // reading 2 -- the staleness test
    scrollHandle = scrollBand = scrollBands = -1;
if (scrollHandle < 0 && scrollBand < 0)
{
    const int h = handleNearX ((float) e.position.x);   // reading 3, inside the helper
    if (h >= 0) scrollHandle = h;
    else        scrollBand = bandAtX ((float) e.position.x);
    scrollAnchor = e.position;
    scrollBands  = bandCount();                     // reading 4 -- the STAMP, taken LAST
}
```

Two distinct failures follow, and they fail in opposite directions.

**Fail-open — the stamp.** An index derived at three bands and stamped with the four that arrived a
few instructions later claims a topology it was never derived in. `scrollBands` is not re-derived
for the rest of the burst; every later tick compares the live count against that claim and **passes**.
This is the case ADR-0039 refused for `removeBand`: an index that lands inside the new range is still
the wrong band, because a band index names a different frequency span under a different count. A
stamp that names the wrong topology is worse than no stamp, because it silences the check that
exists to catch precisely this.

**Fail-safe but lossy — the bound.** Reading 1 is taken before the staleness test and before the
derivation, and is then used as the bound at both write branches (`scrollHandle < N - 1`,
`scrollBand < N`). If the count *rises* in that window, the latch is re-derived under the new
topology and then bounded by the old, smaller one: the index is refused and the tick writes nothing.
No wrong band is touched, but a user edit is dropped for a reason the user cannot see.

`mouseDown` never had the fail-open half — it reads the count at the very top, before any branch
derives anything, so its stamp is the *older* reading and a disagreement always resolves as a
refusal. `SpectrumImager.cpp:2362` states the invariant it relies on: *"handleNearX and addBandAt
both return an index inside the count they read"*. That sentence is only usable if the count they
read is the count the caller proved. It was not.

The same second-reading shape sat in the two `resetParam` call sites, which ADR-0045 had already
ordered count-first: `const int n = bandCount(); const int b = bandAtX (p.x);` — ordered, but still
two reads. There the residue is only the lossy half (the guard `b < n` refuses), which is why the
previous round's ordering fix was correct as far as it went and is why this one is not a correction
of it but a completion.

## Decision

> **A derivation answers under the topology it is given. A handler takes ONE reading of the count,
> and that reading is what the derivation answers under, what the index is stamped with, what the
> bound is taken from and what the store proves.**

Concretely:

1. `bandAtX (float x, int n = -1)` and `handleNearX (float x, int n = -1)` answer under `n`.
   `-1` keeps the old behaviour and is what the hover and paint callers pass, because they stamp
   nothing and prove nothing — they want to know what is under the cursor *now*.
2. `mouseWheelMove` takes one reading, `N`, and uses it for the staleness test, both derivations, the
   stamp, both write bounds, and the `gestureBands` it hands `writeCrossovers`.
3. `mouseDown` passes `gestureBands` — the reading it already takes at the top — into `handleNearX`
   and `bandAtX`, and its Alt-click reset proves that same reading instead of taking a fresh one.
4. `mouseDoubleClick` takes one reading, `N`, already used to bound its chip loop.
5. `setParam (p, plain, int expectedBands = -1)` refuses a store whose topology has moved, the same
   contract `resetParam` has carried since ADR-0045. The wheel's width store is the one store in the
   class that had no topology proof at all; the split branch beside it has had one since ADR-0043,
   through `gestureBands` inside `writeCrossovers`.

## What was NOT done, and why

**`soloHit` and `deleteHit` still re-read the count.** Threading a topology into them means
threading it through `deleteBox`, `soloBox`, `bandLeftX` and `bandRightX` as well — six signatures.
Their window is the fail-safe half only: `gestureBands` is read at the top of `mouseDown`, so it is
the *older* reading, and a disagreement makes `gestureIsStale()` refuse on the next event. The line
drawn here is deliberate and is stated in the source: **fail-open is fixed; fail-safe-but-lossy is
fixed where it costs one argument, and named where it does not.**

**No lock, no seqlock, no marshalling.** ADR-0038 already rejected a lock because `mbBands` is
written from the audio thread. Making the derivation and the stamp *atomic with each other* in the
strong sense would need one; making them **the same read** needs nothing, and is what closes the
window. This is not a threading-model change and is not an `ARCHITECTURE_REVIEW_GATE` item.

**The cross-thread partial-layout trade (RISK-010) is unchanged.** This ADR is about a reader taking
two readings of one parameter. RISK-010 is about `PluginParameters::toEngine` taking ten separate
`load()` calls over ten different parameters. Nothing here touches that; nothing here changes the
ADR-0042/0044 conclusion that a write-side commit is re-torn by that reader and buys nothing.

## Amended 2026-09-09 — the sibling this ADR named and did not convert

**Review finding at `SpectrumImager.cpp:807`, *"band drags adopt replacement layouts"*.** This ADR
converted `mouseWheelMove`, `mouseDoubleClick` and `mouseDown`'s handle/width/alt branches, and gave
`dragCrossoverTo` an `n` argument. It did not convert the **band move**, and its own comment on
`dragCrossoverTo` — one screen above `moveBand` — already described what that costs:

> Reading it here instead would make the plan's extent a different reading from the one
> `writeCrossovers` proves each store against — which cannot write a wrong value (the first store's
> `bandCount() != gestureBands` refuses the whole burst) but does leave the burst's extent and the
> burst's proof disagreeing, **and an ABA return to the stamped count between the two reads would let
> a plan sized under the wrong topology through.**

A band move took **three** readings and proved one of them: `beginBandMove`'s `bandCount()` (the two
pins and the T range), `moveBand`'s `bandCount() - 1` (the plan's **extent**), and
`writeCrossovers`'s per-store `bandCount() != gestureBands` (against the **press**). Readings 1 and 2
are separated by `beginBandMove`'s own `beginGesture` calls, which dispatch, so a host answering the
gesture open moves them apart deterministically; nothing proves reading 2 at all.

**Measured** at three bands against a lane alternating `mbBands` 3/4, pooled over 3 600 band moves:

| | out-of-range writes |
|---|---|
| before | **40 / 3 600 (1.1 %)** |
| after | **0 / 3 600** |

The observable is a message-thread store to `mbFreqHigh` during a three-band band move — a split that
layout does not use — written inside the user's own change gesture, and so into the host's automation
lane and undo history.

**The two halves are not equally load-bearing, and the escalation note that raised this finding said
they were.** Measured separately:

| Mutation | out-of-range writes |
|---|---|
| only `moveBand` reverted (the **extent**) | 7 / 1 200 |
| only `beginBandMove` reverted (the **pins** and T range) | **0 / 1 200** |

The extent is what produces this signature. The pins are threaded anyway — one reading for the
derivation and the proof is this ADR's rule, it removes a read rather than adding one, and a wrong T
range is a wrong *clamp* that `projectFromOrig`'s safety pass re-clamps — but that half is
**unmeasured by this instrument and is not claimed as measured**. `worklogs/…_v0.9.8.md` §44 item C
is corrected accordingly.

**Not a new gate item.** This changes no accepted decision: it applies this ADR's own rule to the one
site it named and skipped. Two private member functions gain a defaulted argument; no parameter ID,
serialization, threading-model, DSP-order or reported-latency change, no lock, no allocation.

## Consequences

**Verified (State test 78, and State test 77 which was already in the tree):**

| Mutation | What it makes the code do | Killed by |
|---|---|---|
| Q1 | the wheel derives under a topology the tick did not prove (`bandAtX (x, 4)`) | State test 77 leg A **and** State test 78 leg B (`steered nothing: 0 1 -1 -1`) |
| Q2 | the Alt reset derives under an unproved topology, bound kept | **nothing** — the bound `b < n` already refuses it |
| Q2b | the Alt reset derives *and* bounds under an unproved topology | State test 78 leg C (`reset a width for band 3, which a two-band topology does not have`) |
| Q3 | the wheel's width store loses its topology proof (the pre-ADR-0046 shape) | **nothing** |
| Q4 | the wheel stamps with a later read (the exact pre-ADR-0046 shape) | **nothing** |

**Q3 and Q4 surviving is the honest result, not a gap that was overlooked.** The window this ADR
closes contains no dispatch, so no deterministic test can enter it: a test would have to move
`mbBands` from another thread inside a span of a few instructions and then observe the store and the
count atomically, which it cannot do. Q3 and Q4 were run to *measure* that claim rather than assert
it. What State test 78 holds is the **contract** the fix rests on — that a derivation given a
topology answers under it, so the index and the stamp cannot disagree — and Q1 and Q2b show that
contract is live. A stress probe was considered and rejected: it can assert no invariant across this
window that is sound without observing the store and the live count together, and a probe that
cannot fail for the right reason is worse than none.

**No behaviour changes with the count still.** Every mutation aside, the suite is unchanged at a
fixed topology: the four probes of State test 78 leg A steer four different bands left to right at
four bands, and legs B/C hold that none of them reaches a band a two-band topology has not got.

Message thread only. No lock, no allocation, no blocking, no audio path touched, no parameter ID,
serialization schema, threading model, DSP signal order or reported latency change.

## Amended 2026-09-10 — "What was NOT done" was wrong about `soloHit`, and this ADR said so itself

**Review finding at `SpectrumImager.cpp:R2484`, *"solo presses target replacement layouts"*.
CONFIRMED, and it falsifies a claim in this document rather than proposing a new rule.** The
Decision above is unchanged and already authorises the fix; what changes is the scope paragraph.

**The claim that was wrong.** "What was NOT done" says of `soloHit` and `deleteHit`:

> Their window is the fail-safe half only: `gestureBands` is read at the top of `mouseDown`, so it
> is the *older* reading, and a disagreement makes `gestureIsStale()` refuse on the next event.

`gestureIsStale()` compares the live world against **the stamp**. The reading `soloHit` actually used
is recorded nowhere, so the "disagreement" the sentence relies on is not observable by any later
guard — and an ABA return erases it outright. **This document had already made that argument**, in
the amendment one section above, and reached the opposite conclusion for `beginBandMove`:

> …an ABA return to the stamped count between the two reads would let a plan sized under the wrong
> topology through.

Same shape, same reasoning, opposite conclusion, in one file.

**Why `soloHit` is the half that was open.** `soloPressBand` is consumed by three places —
`tick`'s hold audition, `mouseDrag`'s band move, `mouseUp`'s toggle — and **none re-derives it**.
Nothing downstream can catch a wrongly-derived index either: `toggleSoloBit` range-checks `b` not at
all, and `setSoloMask` deliberately admits a bit above the live count, because a **parked** solo bit
is a designed, tested state (State test 79 leg A). The index had to be right at derivation or not at
all.

**The geometry is exact, not marginal.** The three split parameters are independent and
unconstrained: `kMinGapPx` is applied only by `projectGaps` to plans this component computes, and
`MultibandWidth`'s ordering clamp is applied to a local copy that is never written back. A host,
preset, A/B apply or undo may legally install `mbFreqLow == mbFreqMid == mbFreqHigh`. There bands 1
and 2 have zero width, fail the 30 px gate that hides a headphone, and are skipped — putting band 3's
headphone centre exactly on band 1's two-band centre. One pointer position, two answers, selected by
the count alone. The consequence is the failure ADR-0043 recorded and believed closed: `0x8` stored
at two bands is masked to nothing by `SoloMonitor`, so **the user clicks a headphone, hears no solo,
and still pays one automation write and one undo entry**.

**Measured**, at the collapsed row, against a lane alternating `mbBands` 2/4, pooled over 1 200
presses (`--solo-alias-probe`; the lane is stopped and the count pinned to 2 before every release, so
a press that legitimately latched four bands is refused rather than counted):

| | presses that soloed a band the press's layout has not got |
|---|---|
| before | **491 / 1 200 (41 %)** |
| after | **0 / 1 200** |

**`deleteHit` was NOT open, and the sentence above never gave the reason it is safe.** Three
independent mechanisms cover it, none of them `gestureIsStale()`: `deleteBox` depends on `bandLeftX`
alone, which reads no count, so a count rise can only *append* candidates above the first match;
`removeBand` rejects `b >= expectedBands` outright; and `mouseUp` re-runs the hit-test and requires
it to name the same band. It is threaded anyway — one reading per pass is this ADR's rule and it
*removes* reads — and that half is **not claimed as a defect fixed**, on the same footing as the
band-move "pins" half in the amendment above.

**`nearWidthLine` is threaded too, on the same terms.** It was the last derivation in `mouseDown`
still reading a sound parameter for itself. Bounded in consequence — a wrong answer starts, or fails
to start, a width drag on the band the *proved* row put under the cursor, and every store it then
makes is refused by `ownsWidth` against that same record — so this half is likewise rule-completion,
not a defect claimed as fixed. The wheel's Alt branch keeps the live read: it has no stamped row to
answer under.

**Not a new gate item.** No accepted decision changes: this applies this ADR's own Decision to the
two derivations it named and skipped. Six private member functions gain defaulted arguments and the
hit-tests now resolve the count **once** and hand it down — so even callers that pass nothing lose
`bandRightX`'s internal re-read. No parameter ID, serialization, threading-model, DSP-order or
reported-latency change; no lock, no atomic, no allocation. The change strictly *removes* parameter
reads, so the cross-thread surface shrinks.

**Verified (State test 85, and `--solo-alias-probe`):**

| Mutation | What it makes the code do | Killed by |
|---|---|---|
| M1 | `mouseDown` takes the live hit-test again (the exact pre-fix shape) | **nothing** deterministic — `--solo-alias-probe` 491/1200 |
| M2 | the press hit-test answers under an unproved count of 4 | State test 85 legs B, C and E (5 checks) |
| M3 | `bandRightX` ignores the threaded count | **nothing** |
| M4 | `soloBox` ignores the threaded split row | **nothing** |
| M5 | the release-time delete confirmation reads live again | **nothing** |
| M6 | that confirmation answers under an unproved count | State test 85 leg D, and 27 checks across the delete tests |

**M1, M3, M4 and M5 surviving is the honest result, not a gap.** The window holds no dispatch, so no
single-threaded test can enter it — the position this ADR already recorded for State test 78. What
State test 85 holds is the **contract**: a hit-test given a topology answers under it, so a press can
never reach a band its own layout has not got. The probe is what measures the window itself, and
unlike the wheel probe withdrawn in ADR-0047 it **can fail for the right reason**: its control
soloes band 1 on a clean tree, and the pre-fix lane reaches the defect 491 times in 1 200.
