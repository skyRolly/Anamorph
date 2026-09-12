# ADR-0052 — An input that performs no edit has no side effects

**Status:** Accepted (maintainer instruction, 2026-09-09 — *final topology and gesture review round*,
review finding *"ignored wheel input ends held presses"* at `SpectrumImager.cpp:2583`).
**Architecture Review Gate: APPROVED by the maintainer, 2026-09-09.** The scope narrowing of
Accepted [ADR-0041](ADR-0041-a-coupled-update-is-all-of-it-or-none-of-it.md) was raised as a
hard-stop item under `docs/policies/ARCHITECTURE_REVIEW_GATE.md`, put to human review in the pull
request rather than decided by a green build, and approved. The reasoning it was approved on is the
*Architecture Review Gate* section below, unchanged.

**Extended by [ADR-0053](ADR-0053-a-wheel-belongs-to-the-interaction-it-lands-in.md) (2026-09-12),
which applies this rule to a branch that did not exist when this was written: a notch while a Band
Solo button is held now MOVES the band, and at one band there is no band to move -- `beginBandMove`
leaves both pins at -1 and `moveBand` returns having written and opened nothing. Converting the press
into a "move" regardless would swallow the solo click on release for no gain whatsoever, which is
this ADR's own defect shape in a new place. Note that ADR-0053 supersedes the ADR-0041 Consequences
line this ADR re-affirms ("a wheel tick FINISHES a held press"); the ADR-0041 DECISION that both
depend on is unchanged.**

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

### Where the rule was applied next (round 11 of the ADR-0053 review)

The rule was written about an event carrying no usable delta. A later review found four sites where
the delta was real and the EDIT still was not, and each of them paid for an edit it did not make. All
four are the same shape — the target is clamped to the value the control already holds — and all four
are now answered by asking the write path's own question before anything is established:

* **A notch at the end of a band's travel** converted the solo press into a move, opened one or two
  host change gestures and started the hold audition, and the release then took the move branch —
  swallowing the solo click exactly as the one-band case in this ADR's own Consequences would have.
  The travel limits used to be knowable only by SETTING them (`beginBandMove`), which is why the
  test could not be made first; the geometry is now a pure `bandMovePlan` both paths derive from.
* **A standalone notch at a split's travel limit, or on a bandwidth already at 0.0 or 2.0**, opened
  a change gesture and closed it again around a store that wrote nothing — a touch/latch punch-in
  for an edit that never happened. Both branches now predict the no-op from the reading the tick has
  already taken and proved (ADR-0047), against `writeCrossovers`' own half-pixel threshold, so the
  prediction cannot drift from the write path.
* **An in-press notch at a bandwidth rail** additionally ENGAGED the width drag that the 3 px
  threshold had not, leaving every later one-pixel tremor writing widths.
* **An Option/Alt-click on a knob already at its default** bracketed the reset in a host change
  gesture and ran the sweep animation, for a `setValue` JUCE then dropped. The same question
  (`resetWouldMove()`) is now asked before the gesture opens and again inside `doReset`, which is the
  double-click path's half of it.

A fifth site was examined and left alone: the wheel latch (`scrollHandle` / `scrollBand` /
`scrollAnchor` / `scrollBands` / `scrollFx`) is still established before the no-edit test. It is
pointer memory rather than edit state — a `mouseMove` writes the same fields with no edit in sight —
and its only consumers are the next tick's staleness test and re-derivation, so latching it for a
notch that then does nothing changes no later answer.

## Consequences

* A horizontal or sub-threshold wheel event no longer ends a held drag, no longer closes its host
  gesture early, no longer creates an undo step, and no longer swallows a pending click.
* No change to any wheel event that carries a real vertical delta.
* **A wheel event whose delta is real but whose target is clamped to the value already held is
  treated the same way** (round 11): no gesture, no store, no engage, no audition, no repaint — and,
  for a solo press, the click still lands on release. Same for an Alt-click reset with nothing to
  reset. Regression coverage: State test 86 legs Q and R, State test 87 leg F, State test 88 leg K.

## Related code

`src/gui/SpectrumImager.cpp` — `mouseWheelMove` (`:2685` onward).

## Evidence + confidence

**Verified.** State test 80 **leg D**, in two halves that mirror legs A and C with the only change
being that the wheel event carries `deltaY == 0` and `deltaX == 0.6`: a held width drag keeps writing
and its gesture does not close at the event; a held solo press still toggles on release. Mutation:
restoring `cancelActiveDrag()` in front of the threshold kills all three of leg D's checks and nothing
else. State 2 840 / 0.
`worklogs/SPECTRUMIMAGER_TOPOLOGY_TRANSACTION_AUDIT_v0.9.8.md` §48.
