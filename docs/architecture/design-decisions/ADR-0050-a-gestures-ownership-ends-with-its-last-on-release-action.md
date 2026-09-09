# ADR-0050 — A gesture's ownership ends with its last on-release action, not with its first

**Status:** Accepted (maintainer instruction, 2026-09-09 — *final topology and gesture review round*,
review finding *"release-time automation loses a band"* at `SpectrumImager.cpp:2441`).

**Completes [ADR-0038](ADR-0038-gesture-state-is-void-once-topology-moves.md) and
[ADR-0039](ADR-0039-a-gesture-owns-the-world-it-was-latched-in.md); supersedes nothing.** ADR-0038 put
the staleness gate at the top of `mouseUp` and ADR-0039 made the gate see a same-count sound install
as well as a count move. Both were defeated by the line immediately after the gate.

## Context

`mouseUp` is where the on-release actions live — remove a band, toggle a solo bit, commit a band move.
It ran the gate, dropped the gesture snapshot, and *then* performed the actions:

```cpp
if (gestureIsStale()) { cancelActiveDrag(); updateHover (e.position); return; }
const int pressBands = gestureBands;
gestureBands = -1;                       // <- here
...
if (dragHandle >= 0)
{
    endGesture (freqP[dragHandle]);      // <- DISPATCHES
    if (dragRemovePending)
        removeBand (dragHandle + 1, pressBands);
}
```

`gestureIsStale()` is `topologyMovedUnderGesture() || soundMovedUnderGesture()`
(`SpectrumImager.h:416`), and **both** halves return early on `gestureBands < 0`. Clearing the latch
therefore does not merely leave the question unasked; it makes the question unanswerable.

## Problem

`endGesture` on the dragged split calls `endChangeGesture`, which reaches every listener synchronously
— an automation lane recording the release, a control-surface echo, the sound half of a state restore.
A **same-count** install landing there is invisible to everything downstream:

* `pressBands` and `removeBand`'s `expectedBands` compare **counts**, and the count did not move;
* `ownsSplit` / `ownsWidth` are the only things that see a same-count sound change, and they were
  disarmed one line above.

The pending removal then merges a band of a layout the press never saw. The window is **reentrant**:
it contains a dispatch, so a deterministic single-threaded test enters it.

This is the same window State test 69 leg C already covers from the other side — that leg moves the
**count** from inside `endGesture` and is caught. The sound half was uncovered.

## Options

| | |
|---|---|
| **A. Move the clear below the on-release actions** (chosen) | The clear is duplicated at the two early-return branches, which is where the press for those branches really is over. Nothing in either branch reads `gestureBands` — `setParam`, `setBands`, `setSoloMask`, `toggleSoloBit` and `endBandMove` do not — so their behaviour is unchanged. |
| B. Re-arm the latch around the dispatch inside the drag branch | Same effect, but it reads as a workaround: a value cleared and then restored says the invariant is elsewhere. |
| C. Re-prove only the dragged split's own value | Narrower than the rule ADR-0039 already carries, and a restore installs a whole sound, not one split. |
| D. Accept and document | The window is reentrant and reachable, and the consequence is a band the user did not remove. |

## Decision

> **A gesture's ownership lasts as long as the on-release actions that depend on it. The snapshot is
> dropped on each exit path after those actions, not before them, and an action that follows a
> dispatch re-proves ownership after that dispatch.**

`if (dragRemovePending && ! gestureIsStale())` costs three float compares against a stamp the drag's
own stores keep current (`writeCrossovers` stores **through** `gestureX[k]`, see `storeOwned`), so an
uninterrupted release still removes exactly as before: the stamp equals the world it just wrote.

## Consequences

* A same-count install during the release dispatch now refuses the removal instead of performing it.
  That is the ADR-0038 disposition — *a gesture whose world moved fires none of its on-release
  actions* — applied to the window it had been missing.
* No behaviour change on any clean release, and none for the solo or delete branches.
* The two early-return branches keep clearing the latch exactly where they did.

## Related code

`src/gui/SpectrumImager.cpp` — `mouseUp` (`:2514` onward).

## Evidence + confidence

**Verified.** State test 69 **leg G**: four bands, the top split dragged outward to arm the removal, a
listener on that split writing `mbFreqLow` from inside `endChangeGesture`. Leg C is its other half
(the count move) and leg E is the positive control (a steady outward drag still removes). Mutation
record:

| Mutation | Killed |
|---|---|
| the post-dispatch `! gestureIsStale()` removed | leg G only, 1 check |
| the clear restored above the branches (the pre-fix shape) | leg G only, 1 check |
| the `gestureBands == pressBands` latch test removed | **nothing** — see below |

The first two halves are load-bearing and each is measured on its own.

**And keeping the latch alive across the dispatch has a second consequence, which this decision
handles rather than leaves.** `tick`'s 24 Hz reconcile is `if (gestureIsStale()) cancelActiveDrag();`
and it now has something to find, so a host that pumps the message loop from inside
`endChangeGesture` can reach it while that call is still on the stack. With `dragHandle` still set,
that reconcile would call `endGesture` on the same parameter a **second** time — a negative
open-gesture count and a spurious undo entry, the reentrant double-close class already escalated for
three sites in this file. So the identifier is latched into a local and the member cleared **before**
the dispatch, which sends such a reconcile down `cancelActiveDrag`'s cheap exit instead.

That exit clears `gestureBands` without clearing `dragRemovePending`, which is why
`gestureBands == pressBands` is there: without it the removal would proceed under a gesture something
else had just cancelled, and `gestureIsStale()` would answer `false` because the latch is gone. **It
is unmeasured, and that is stated at the guard rather than implied** — the path needs a host that
pumps the message loop from inside a gesture callback, so no deterministic test reaches it and
removing the line alone leaves all 2 840 checks green. Same disposition, and same reason, as
`removeBand`'s delete-x call site.

**And this decision SUBSUMES a mutation the previous round used as evidence, which is worth stating
because it looks like a coverage loss and is the opposite.** That round measured that passing a live
`bandCount()` instead of `pressBands` at the outward-drag call site kills State test 69 leg C twice.
Re-measured on this tree, it kills **nothing**: `! gestureIsStale()` now stands in front of the call
and `topologyMovedUnderGesture` compares the live count against the still-latched `gestureBands`, so
leg C's `mbBands` move is refused a level earlier whatever the argument says. Removing **both** fails
leg C twice and leg G once. `pressBands` is kept as defence in depth — it is the only cover if a
future change moves the staleness gate again, and the delete-x and solo call sites have no gate in
front of them at all — and the guard's own comment now records this so the next reader does not
delete it as dead.

State 2 840 / 0. `worklogs/SPECTRUMIMAGER_TOPOLOGY_TRANSACTION_AUDIT_v0.9.8.md` §46.
