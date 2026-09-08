# ADR-0043 — A commit carries intent, and a plan is computed where it is used

**Status:** Accepted (maintainer instruction 2026-09-08: a focused audit of the remaining
SpectrumImager ownership and stale-gesture issues after ADR-0038 through ADR-0042, with two review
findings, one documentation item and one informational item, and the instruction to audit every write
path before patching any of them).

**Completes [ADR-0042](ADR-0042-a-store-is-committed-only-when-the-parameter-says-so.md); supersedes
nothing.** The invariant is unchanged: *a gesture stores an authoritative value only while that value
is still the one its plan was computed from.* This ADR closes the last two places where it was not
applied, and both are applications of primitives that already exist.

**Not an Architecture Review Gate item.** No parameter ID, range, default, automation flag or
serialization field changes. No threading-model change: message thread only, no lock, no wait, no
allocation, no audio path, no parameter-model change. Two private member functions are restructured,
one gains an early return, one line is added to `tick()`, and two members are added to the header.

## Context

The full write-path audit is in
`worklogs/SPECTRUMIMAGER_REMAINING_OWNERSHIP_AUDIT_v0.9.8.md` §2. Every crossover, width, band-count,
solo and preview write was traced for its cached state, its ownership check, its reentrancy safety
and its stale behaviour. **The invariant is fully applied everywhere except two rows**, and they are
the two the review named.

* **The primary stores of the reset / text-commit pair.** `projectGaps` pins the edited split and
  then slides the whole cluster — *the pin included* — back inside the plot edges by an amount
  derived from the **neighbours'** snapshot (`SpectrumImager.cpp:300-301`). The plan was computed
  before `beginChangeGesture`, which dispatches to the host, so the value the store wrote could be a
  projection of a world that had already moved. **Measured** on splits `200 / 18000 / 19500`, typing
  `15 kHz` into the first chip: uninterrupted `8440.1 / 11407.5 / 15122.0` — the projection doing its
  job — and, with a host moving a neighbour from inside the gesture open,
  `8440.1 / 3000.0 / 19500.0`, the first split **above** the second and the pin standing on splits
  that were no longer there. The reset path slides the same way in the other direction: a layout
  packed against the left edge resets its third split to `4732.0` Hz rather than its `3000` Hz
  default.
* **The editor that commits a value nobody typed.** `openFreqEditor` seeds the text box from the live
  split (`:896`), and `commitFreqEditor` is reached by Return, by **focus loss** (`:891`) and by any
  `mouseDown` in the component (`:1950`). Opening the chip and clicking away therefore re-committed
  the value the split had when the editor opened. **Measured:** `5000.0 Hz was installed and 200.0 Hz
  was written over it`; and with nothing moving at all, one automation touch and one undo step for an
  edit that never happened.
* **The third consumer of a positional identifier.** `tick()` promoted a press to a held audition on
  a purely **time-based** condition (`:1207-1212`), firing `onSoloPreview (1 << soloPressBand)` with a
  band index latched at `mouseDown`. `mouseDrag` (`:2044`) and `mouseUp` (`:2107`) both ask
  `gestureIsStale()` before acting on it; `tick()` asked nothing.

## Problem

Two different failures, and neither is the one the finding's wording suggests.

* It is **not** that the primary store fails to check its own target. An Alt-click reset targets the
  parameter's *default* and a typed commit targets the *user's typed value*; neither is computed from
  the split's current value, and the user's action is itself the newer authority (ADR-0036 §25). A
  guard of the form "refuse if the split moved" would abandon a deliberate edit whenever automation
  touched that split — a **false refusal**, and State test 75 leg B exists to keep it out.
* What is real is narrower: the store must prove the state its **projection** depended on, and the
  editor must not commit a value that carries no user change.

## Decision

> **A commit carries intent, or it carries nothing. And a plan is computed where it is used: inside
> the gesture that will store it, so that nothing dispatches between the world it reads and the store
> it makes.**

**The plan moves inside the bracket.** `resetCrossover` and `commitFreqEditor` now open the change
gesture *first*, then snapshot, project and store. `projectGaps` is pure, so between the snapshot and
the store nothing dispatches: the projection is a function of the world one statement earlier, and
`storeOwned` still proves the far side. **No refusal and no new predicate** — the plan is simply
computed later, and `spreadSplits` is untouched with its `was[]` now captured from the same fresh
world.

**The editor commits only what the user changed.** Two signals, because one of them is asynchronous:
`TextEditor::onTextChange` is delivered through `postCommandMessage`
(`juce_TextEditor.cpp:594-599`), which is reliable in a running message loop and catches even a
retyped identical string but never arrives without one; the comparison against the string the box was
seeded with is synchronous. Either is sufficient.

**`tick()` asks the question the other two consumers ask.** `if (gestureIsStale()) cancelActiveDrag();`
before the promotion. It fires once rather than every frame, because `cancelActiveDrag` clears
`gestureBands`; and it returns before any repaint when no identifier is latched, so an Alt-click
reset costs one integer store. That also retires the residual ADR-0042 §45 recorded — `resetCrossover`
leaving `gestureX` stale, harmless until now only by luck.

**Architecture: option A, reuse, unchanged.** Nothing is extended and nothing new is built. Option B
had nothing to extend — `ownsSplit`/`ownsWidth`/`storeOwned`/`spreadSplits` are already exactly right
for what they guard, and the two gaps were a plan computed too early and a consumer that never asked.
Option C would put a second answer to "is this gesture still valid?" beside `gestureIsStale()`, which
is the mistake ADR-0038 was written to end.

## Two more the systematic half found

The audit of *every* write path — not only the two the review named — found two further gaps of the
same family, both closed here without a new mechanism.

* **`spreadSplits` re-proved the neighbours but never the PIN.** Every position in the plan was
  computed to make room for the split being spread around, and each neighbour store dispatches to
  the host. Measured: the pin dragged to `300.0` Hz from inside the first neighbour's store, and the
  spread carried on for a pin at `8440`, leaving `300.0 / 11407.5 / 15122.0`. `storeOwned` already
  hands back the value the pin was confirmed to hold; `spreadSplits` now takes it and re-proves it.
* **The wheel's burst had no per-store ownership at all.** `mouseWheelMove` clears `gestureBands` —
  correct, no press is in flight — and `gestureBands < 0` waives **both** `ownsSplit` and the count
  re-proof inside `writeCrossovers`. The burst that follows is up to three stores with a host
  dispatch between each, so the one path ADR-0040 did not cover still carried the defect ADR-0040
  was written for. Measured: `5000.0 Hz was installed and 1476.4 Hz was written over it`. The
  topology is now named alongside the sound `captureDragOrigins()` records, for the duration of the
  burst only. This does not change ADR-0041's decision that the wheel leaves nothing in flight; it
  changes only that the burst owns what it writes.

## The informational item, ruled and not fixed

A spread that abandons part-way can leave the splits out of order **on screen**. The DSP is
unaffected — `MultibandWidth::setCrossovers` and `SoloMonitor::setCrossovers` force strict `1.1×`
ordering on whatever they read — and `handleNearX` is order-independent, so every handle stays
grabbable and one drag restores the order. But `bandAtX` (`:241-248`) scans ascending and `soloHit`
(`:272-278`) requires a positive band width, so the band between an inverted pair has its solo,
width and add affordances unreachable until any edit re-projects the layout. **Accepted behaviour,
not a defect:** fixing it directly would mean drawing a layout the plug-in does not have, or
refusing to abandon a spread and writing a stale value over a newer authority. What this ADR does is
remove its only single-threaded trigger; what remains is the concurrent route already ruled a
bounded trade by ADR-0042 §41.

## Consequences

* Opening the frequency chip and dismissing it without typing now does nothing at all — no value
  change, no automation touch, no undo step. Previously it re-wrote the value the split had at open.
* A reset or typed commit whose projection slid the cluster is now computed from the live world, so
  it can no longer land on a position justified by splits that have moved.
* A held solo audition ends when the topology or the sound moves under it, instead of continuing to
  audition a band by a stale index.

## Related code

`src/gui/SpectrumImager.cpp` — `resetCrossover`, `openFreqEditor`, `commitFreqEditor`, `tick`;
`src/gui/SpectrumImager.h` — `editTextEdited`, `editOpenText`, and the corrected
`soundMovedUnderGesture` comment.

## Evidence + confidence

**Verified.** State test 75 legs A, C, D, F, G and H fail before and pass after; legs B, E and F(i)
are the positive controls a careless fix would break. Mutations N1 (the intent gate removed), N2 and
N3 (the plan computed before the gesture opens, in each function), N5 (the pin re-proof removed) and
N6 (the wheel burst unowned) each killed by exactly the intended leg. State suite 2 707 / 0, DSP
396 / 0.

**Bounded, and stated as such.** F2's trigger lives in `tick()`, which is driven only by
`juce::VBlankAttachment` and has no headless surface, so it ships under ADR-0025's documented
exception with the four disclosures recorded in the worklog §12 and the register entry in
`TESTING.md`.
