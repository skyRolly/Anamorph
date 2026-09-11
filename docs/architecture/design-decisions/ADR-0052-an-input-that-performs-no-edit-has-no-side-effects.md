# ADR-0052 — An input that performs no edit has no side effects

**Status:** Accepted (maintainer instruction, 2026-09-09 — *final topology and gesture review round*,
review finding *"ignored wheel input ends held presses"* at `SpectrumImager.cpp:2583`).
**Architecture Review Gate: APPROVED by the maintainer, 2026-09-09.** The scope narrowing of
Accepted [ADR-0041](ADR-0041-a-coupled-update-is-all-of-it-or-none-of-it.md) was raised as a
hard-stop item under `docs/policies/ARCHITECTURE_REVIEW_GATE.md`, put to human review in the pull
request rather than decided by a green build, and approved. The reasoning it was approved on is the
*Architecture Review Gate* section below, unchanged.

**Clarifies [ADR-0041](ADR-0041-a-coupled-update-is-all-of-it-or-none-of-it.md) and
[ADR-0043](ADR-0043-a-commit-carries-intent-and-a-plan-is-computed-where-it-is-used.md); supersedes
nothing.** ADR-0041 decided that a wheel tick **finishes** a held press, because two gestures cannot
own the same state at once. That decision is unchanged and re-affirmed here. What was wrong was its
**scope**: the code applied it to every wheel *event*, including the ones that perform no edit at all.

## Context

`mouseWheelMove` began with `cancelActiveDrag()` and only tested the wheel delta forty lines later:

```cpp
cancelActiveDrag();                 // ends the drag, closes its gesture, drops pending clicks
const int N = bandCount();
... scrollHandle / scrollBand latch ...
const float dy = (wheel.isReversed ? -1.0f : 1.0f) * wheel.deltaY;
if (std::abs (dy) < 1.0e-4f) return;      // ...and only here does the tick become an edit
```

`cancelActiveDrag` calls `endGesture` on the dragged parameter, clears `dragHandle`, `dragBand`,
`soloPressBand` and `pressDeleteBand`, and clears `gestureBands`.

## Problem

`deltaX` is read **nowhere** in this class, so a horizontal-only trackpad scroll arrives with
`deltaY == 0` and is ignored — after it has ended the user's drag, closed that drag's host change
gesture early (committing the drag so far as its own undo step), and swallowed a pending solo or
delete click. A sub-threshold vertical delta, and any host that delivers a zero-delta wheel event, do
the same.

ADR-0041's rule is about **owners**. An event that writes nothing is not a second owner: it has no
claim to make on the state the press holds, so it has nothing to end.

## Options

| | |
|---|---|
| **A. Hoist the delta test above `cancelActiveDrag`** (chosen) | Two lines moved. A real tick behaves exactly as ADR-0041 decided; an ignored one becomes a no-op, latch included. |
| B. Make the wheel a no-op during any held press | This is the option ADR-0041 rejected and this round re-measured: it fires State test 73 leg A, because `captureDragOrigins` re-seeds the ownership record but cannot re-seed `dragGrabDX`/`dragGrabDY`, so the press writes from an anchor that predates the wheel's own install. Reintroduces the defect ADR-0041 closed. |
| C. Cancel only the drag and keep pending clicks | Splits one rule into two with no principle behind the split, and State test 80 leg C exists to pin exactly that consequence of the one rule. |
| D. Accept and document | The cost is a user's drag and click discarded by an input that does nothing, on hardware where horizontal scroll is ordinary. |

## Decision

> **An input event that performs no edit has no side effects. The threshold that decides whether an
> event is an edit is tested before anything the edit would cost.**

`cancelActiveDrag()` is unchanged and still runs on every real tick, so ADR-0041's rule and all four
of its measured consequences (State test 80 legs A–C) stand exactly as they were. The `scrollHandle`
/ `scrollBand` / `scrollAnchor` / `scrollBands` latch also stops being re-derived by an event that
will not use it.

## Architecture Review Gate — considered explicitly

`docs/policies/ARCHITECTURE_REVIEW_GATE.md` lists *conflict with an Accepted ADR* among the hard-stop
changes a green build does not clear, and ADR-0041's Decision reads *a wheel tick ends an in-flight
press*, with the swallowed solo/delete click pinned in its Consequences as intended. **This is a
scope clarification, not a conflict, and the reasoning is recorded here rather than assumed:**
ADR-0041's premise is stated in its own first sentence — *the wheel is its own instantaneous edit* —
and an event that writes nothing is not one. Every event ADR-0041 reasoned about behaves identically
after this change, and its three measured consequences (State test 80 legs A–C) all still hold; what
changes is only the set of events the rule is applied to. The change was made under an explicit
maintainer instruction that put this exact question — *whether an ignored wheel event should have any
side effect* — and asked for an evidence-based decision.

The in-source ADR-0041 comment block was reconciled rather than left stranded: it now opens by saying
that every sentence in it is about an event that makes an edit.

## Consequences

* A horizontal or sub-threshold wheel event no longer ends a held drag, no longer closes its host
  gesture early, no longer creates an undo step, and no longer swallows a pending click.
* No change to any wheel event that carries a real vertical delta.

## Related code

`src/gui/SpectrumImager.cpp` — `mouseWheelMove` (`:2685` onward).

## Evidence + confidence

**Verified.** State test 80 **leg D**, in two halves that mirror legs A and C with the only change
being that the wheel event carries `deltaY == 0` and `deltaX == 0.6`: a held width drag keeps writing
and its gesture does not close at the event; a held solo press still toggles on release. Mutation:
restoring `cancelActiveDrag()` in front of the threshold kills all three of leg D's checks and nothing
else. State 2 840 / 0.
`worklogs/SPECTRUMIMAGER_TOPOLOGY_TRANSACTION_AUDIT_v0.9.8.md` §48.
