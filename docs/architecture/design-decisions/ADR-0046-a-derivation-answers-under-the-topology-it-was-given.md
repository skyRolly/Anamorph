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
