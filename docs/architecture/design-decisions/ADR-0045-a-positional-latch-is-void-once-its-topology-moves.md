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
