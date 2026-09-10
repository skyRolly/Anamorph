# ADR-0045 — A positional latch is void once its topology moves, and a reset proves its topology inside its own bracket

**Status:** Accepted (maintainer instruction, 2026-09-08 — *final review round: topology identity,
wheel ownership, residual decisions*).

**Completes [ADR-0038](ADR-0038-gesture-state-is-void-once-topology-moves.md) and
[ADR-0039](ADR-0039-a-gesture-owns-the-world-it-was-latched-in.md); supersedes nothing.** Both stated
the rule for the *drag* gesture. Two paths in the same class were never brought under it, and this
round's own audit — not the review — found them.

## Context

`SpectrumImager` has three kinds of positional identifier. Two are stamped with the topology they
were taken in (`gestureBands`, tested by `gestureIsStale()`), and one was not:

| Latch | Lifetime | Stamped? |
|---|---|---|
| `dragHandle`, `dragBand`, `soloPressBand`, `pressDeleteBand` | one press | **yes** — `gestureBands` + `gestureIsStale()` |
| `editingHandle` | one chip edit | **yes** — re-proved against the live count at open *and* inside the commit's own bracket |
| `scrollHandle`, `scrollBand` | one wheel **burst** | **no** — bounds only |

And one store had a gesture bracket with no topology proof inside it: `resetParam`, whose callers
derive the band index from `bandAtX` **outside** the call.

## Problem

**The wheel latch.** `scrollHandle`/`scrollBand` are latched at the first tick of a burst and reused
by every later one. They are dropped by a `mouseMove` of more than 3 px and by `mouseExit`, and by
nothing else — so a burst survives a band-count change that arrives from a host lane, an undo or a
preset while the hand is still. The display is re-laid out under a pointer that has not moved, and
the next tick steers whatever the *old* index named. Bounds do not close this: an index that
**lands inside** the new range is exactly the case ADR-0039 refused to accept for `removeBand`.

Measured (State test 77 leg A, mutation R1):

```
[leg A] the wheel steered a band its latch named in another topology: band 1 moved
        1.060 -> 1.120 after the count changed under a hand that never moved
```

**`resetParam`.** It is `beginChangeGesture(); setValueNotifyingHost (default); endChangeGesture();`
with no check between the open and the store — and the open dispatches to every listener. Its two
call sites are `const int b = bandAtX (p.x); if (nearWidthLine (p, b)) resetParam (widthP[b]);`, so a
host answering the gesture open by dropping Bands left it resetting the width of a band that no
longer exists: an automation touch and an undo step for a band that is not there, and a value that
reappears in the sound if the count ever rises again.

Measured (State test 77 leg C, mutation R2):

```
[leg C] a reset stored a width after the topology moved under it: 1.600 / 1.000 / 1.600
        (all were 1.600)
```

## Decision

> **A positional identifier is stamped with the topology it was taken in and is void once that
> topology moves. A store whose gesture bracket dispatches before it proves the topology inside the
> bracket, never outside it.**

* `scrollBands` records `bandCount()` beside the latch. A tick whose live count differs drops the
  latch and re-derives it from the pointer — which is precisely what a `mouseMove` already did, so
  **an uninterrupted burst behaves exactly as before** (leg B is the control).
* `resetParam` takes `expectedBands` and proves it between its own `beginChangeGesture` and its
  store, the shape `setBands` and `setSoloMask` have carried since ADR-0041 and ADR-0044. `-1` means
  the caller has no topology to prove.

## Tightened after the first implementation, on the review's own wording

The first implementation wrote `resetParam (widthP[b], bandCount())` — deriving the index and *then*
reading the count. That closes the **dispatch** window, which is what the reentrancy chain is about,
and leaves a second one a few instructions wide: `mbBands` is written by the **audio thread** as well
(host automation through the format wrapper), so a write landing between `bandAtX` and `bandCount()`
makes the guard agree with a live count while `b` was derived under the old one. The review named
exactly that pair. Both call sites now read the count **first** and derive from it
(`const int n = bandCount(); const int b = bandAtX (p.x); if (b >= 0 && b < n && …) resetParam (widthP[b], n);`),
so a count that moves in that window makes the guard disagree and the reset refuses. Ordering two
reads costs nothing, changes no UX, and fails in the safe direction — a refusal, per ADR-0039.

## What was proposed and REJECTED, because a shipped test said so

The same audit proposed narrowing `soundMovedUnderGesture()` to the slots the latched count uses —
at two bands, `mbFreqMid`, `mbFreqHigh`, `mbWidth2` and `mbWidth3` are unread by the DSP, so a host
lane parked on one of them appeared to cancel drags for nothing. It was implemented, and it **broke
State test 71 leg G**, which asserts deliberately that at two bands a foreign write to slot 2 must
stop the drag. The test is right and the proposal was wrong: the gesture's plan is `projectFromOrig`
over **all** slots from a `captureDragOrigins()` that seeded **all** of them, so a slot this pass did
not write is still part of the world the plan was computed in — and the count can rise at any moment
and start reading it. The predicate is left as it was, with the rejection recorded beside it so the
next reader does not re-propose it.

## Related code

`src/gui/SpectrumImager.cpp` — `mouseWheelMove`, `mouseMove`, `mouseExit`, `resetParam`,
`soundMovedUnderGesture` (the rejection comment); `src/gui/SpectrumImager.h` — `scrollBands`,
`resetParam`'s `expectedBands`.

Evidence [Verified]:
- State test 77 legs A and C fail under mutations R1 and R2 respectively and pass on the fix; legs B
  and D are the controls (an uninterrupted burst still steers the same thing; an ordinary alt-click
  still resets) and are green under both mutants, so neither fix is a false refusal.
- The rejected narrowing: State test 71 leg G (`tests/state_tests.cpp`), measured failing.
- Suites: state 2 749 / 0, DSP 396 / 0.

## Applied again 2026-09-09 — the wheel latch's other half, the split ROW

The Decision above is general: *a positional identifier is stamped with the topology it was taken in
and is void once that topology moves.* When it was written, `scrollBands` recorded `bandCount()` and
nothing else — and ADR-0039 had already settled that **the count is not the whole topology**, with
ADR-0051 making the split row the other half explicitly. A follow-up review filed the gap as a Bug
(*"wheel bursts target stale bands"*, `SpectrumImager.cpp:2943`). It is confirmed. **No new decision:
this ADR's Status and Decision stand, and no `ARCHITECTURE_REVIEW_GATE.md` item is triggered.**

**The bullet above argues this case in its own words.** It says a tick whose live count differs
"drops the latch and re-derives it from the pointer — which is precisely what a `mouseMove` already
did". A **same-count split move** re-lays the display out under a stationary hand in exactly the same
way, and none of the three existing invalidations sees it:

| invalidation | what it watches | sees a same-count split move? |
|---|---|---|
| `mouseMove`, `> 3 px` from `scrollAnchor` | the **pointer** | no — the pointer has not moved |
| `mouseExit` | the pointer leaving | no |
| `scrollBands != bandCount()` | the **count** | no — the count is unchanged |

So a burst latched over band 1 kept editing band 1's width after automation slid a split across the
cursor, and a burst latched on handle 1 kept steering handle 1 after the row put a different handle
under the pointer.

**Deterministic, unlike the ADR-0046/0047/0051 windows.** Those live between two pure reads inside one
handler and need a probe. This window is **between two wheel ticks** — user time — so a
single-threaded test walks straight into it. State test 77 **leg E** does, and failed on the pre-fix
tree with the diagnostic *"band 1 moved 1.060 → 1.120 after a same-count split move under a hand that
never moved"*.

**The fix is the row beside the count.** `scrollFx[3]` records the split row the latch was derived in,
from the same single `captureSplits` reading the derivation uses (ADR-0051), and a tick whose live row
differs drops the latch and re-derives — in the same tick, so the user's tick still edits, it just
edits what the pointer is actually over. Re-hit-testing every tick was rejected: the wheel's own edits
move the split it is steering, so the burst would jump to a neighbour mid-burst. Invalidating the
burst outright was rejected for the same reason the count case re-derives.

**The stamp follows the burst's own edits, and that is load-bearing.** A split burst writes the very
row the test watches. `writeCrossovers` stores through `gestureX[k]` via `storeOwned`, so `scrollFx`
is **derived** from that record after the store rather than re-read from the parameters — one reading
per pass (ADR-0047/0051), and no window in which a foreign write could be adopted. A refused store
leaves the stamp at the pre-store row, so the next tick retargets: correct, because a store this burst
did not land is not a row this burst owns.

**ADR-0041 and ADR-0052 are untouched.** The delta test and `cancelActiveDrag()` above are unchanged
and still run first, so an input that performs no edit still has no side effects and a real tick still
finishes a held press.

**Mutations:**

| Mutation | Killed |
|---|---|
| the row comparison removed (count-only, the pre-fix shape) | leg E, 1 check |
| the stamp refresh after a split store removed | leg F, 1 check |
| the seed at latch creation removed | **nothing** — see below |

The seed survives mutation and that is recorded rather than hidden. With it gone, `scrollFx` starts at
zeros and a **width** burst re-derives its latch on every tick; over an unchanged row that
re-derivation is idempotent, so no observable differs. Its real effect is that the latch — and with it
`scrollAnchor` — persists instead of being re-stamped each tick, which is what makes the `> 3 px`
pointer test measure drift from where the burst began. No test in this suite separates those, and the
line is kept as the correct expression of "the row it was derived in" rather than deleted as unkilled.

State 2 874 / 0, DSP 396 / 0, TSan 0 warnings, valgrind 0 errors, all five probes 0.
`worklogs/SPECTRUMIMAGER_TOPOLOGY_TRANSACTION_AUDIT_v0.9.8.md` §61.

## Applied again 2026-09-10 — the SOUND half inside the solo bracket

This ADR's Decision has two sentences. The second — *"a store whose gesture bracket dispatches before
it proves the topology inside the bracket, never outside it"* — was implemented in `setSoloMask` for
the **count**: `beginChangeGesture()` fires, then the guard checks `bandCount() == expectedBands` and
`soloMask() == expectedMask`. ADR-0039 settled that the count is not the whole topology, and the
sound half of that guard was never written. A review filed it as *"release actions target replacement
layouts"* (`SpectrumImager.cpp:2667`) and it is confirmed. **No new decision:** this is the same
sentence applied to the half it did not cover, so ADR-0045's Status stands and no
`ARCHITECTURE_REVIEW_GATE.md` item is triggered.

**REENTRANT, and the file said otherwise.** `mouseUp`'s ADR-0050 comment asserted that the solo store
paths "are CROSS-THREAD ONLY and no deterministic test enters them". That is true of `mouseUp`'s own
body and false of the window that matters: it continues into `setSoloMask`, whose
`beginChangeGesture()` notifies every listener **synchronously** ahead of the guard. A host answering
that open with a same-count sound install lands after the caller's gate and before the store, and the
band index the click latched then names a band whose boundaries the user never saw. The comment is
corrected in place; the delete half of the same sentence stands (that window really is bounded by
pure reads).

**The fix is one clause** — `&& ! soundMovedUnderGesture()` — inside the bracket, where the ADR says
it belongs. It self-disables when no gesture is in force (`gestureBands < 0`), so `removeBand`'s own
mask remap and every record-less caller are untouched; and with nothing racing, the caller's gate has
just proved these values, so nothing that used to commit stops committing.

**Measured.** State test 79 **leg C** reaches it with a real `AudioProcessorParameter::Listener` and
no thread; **leg D** is the control that an undisturbed solo click still writes the mask. Removing the
clause fails leg C.

State 2 879 / 0, DSP 396 / 0, TSan 0 warnings, valgrind 0 errors, all five probes 0.
`worklogs/SPECTRUMIMAGER_TOPOLOGY_TRANSACTION_AUDIT_v0.9.8.md` §62.
