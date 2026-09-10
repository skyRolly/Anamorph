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

## Applied to all three branches — and the first implementation applied it to one

**Amended 2026-09-09, same day, from a review finding at `SpectrumImager.cpp:2562`.** The first
implementation of this decision moved the clear off the shared line and then put it straight back at
the top of the `pressDeleteBand` and `soloPressBand` branches. That is the same defect in two more
places, and it is what this ADR's own title forbids. The review found it; the argument was already
written in the file.

**Both remaining windows are cross-thread only**, and that is stated rather than glossed:
`deleteHit` is a pure read, and the solo store paths are the `else` of the branch that calls
`endBandMove`, so no dispatch separates the handler's gate from either action and **no deterministic
test can enter them**. It is nevertheless the class ADR-0046, ADR-0047, ADR-0048 and ADR-0051 all
**closed** rather than accepted, and the reason applies unchanged here: with the latch cleared the
question is not merely unasked, it is *unanswerable*, so a later reader cannot add the check without
first finding the clear.

Both branches now take their latched identifiers into locals and clear them **before** anything
dispatches — `cancelActiveDrag` re-runs `onClearSoloPreview` and `endBandMove` while
`soloPressBand`/`soloMovedBand` are set, so a reconcile reaching the held-solo path mid-dispatch
would close the same two change gestures twice — and prove `gestureIsStale()` at the action.

**What can be measured is the other direction, and it is measured.** The risk of adding a gate is
that it refuses something valid. Forcing each new gate to refuse **always**:

| Mutation | Killed |
|---|---|
| the delete branch always refuses | **25 checks**, across State tests 69, 71, 74, 75, 76 and 79 |
| the solo branch always refuses | **6 checks**, across State tests 76, 79 and 80 |

So the actions these gates guard are heavily covered, and a fix that over-refused would have failed
loudly rather than quietly. The defect direction remains unmeasurable, and no probe is shipped for
it: with a lane moving the sound continuously the handler's own gate refuses nearly every release,
so the narrow window contributes nothing an instrument could separate — a probe here would report a
number that means nothing, which is the failure ADR-0051's header exists to warn about.

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

## Amended 2026-09-09 — the other two escalated sites, closed

The paragraph above names "the reentrant double-close class already escalated for **three** sites in
this file" and closes one of them, `mouseUp`. A follow-up review filed the second as a Bug —
*"cancellation closes gestures twice"* — and it is confirmed, reproduced and fixed here. No new
decision: this is the same rule applied to the two sites that were carried, so ADR-0050's Status and
its Decision are unchanged.

**What was actually wrong, and why `gestureBands = -1` was not already cover.** `cancelActiveDrag`
clears the snapshot first and then dispatches with `dragBand` / `dragHandle` / `soloPressBand` still
live. That ordering protects exactly one reentrant path and was mistaken for protecting all of them:

| reentrant path into `cancelActiveDrag` | predicate | covered by the early `gestureBands = -1`? |
|---|---|---|
| `SpectrumImager::tick` | `if (gestureIsStale()) …` | **yes** — the predicate reads the latch, which is already gone |
| `AnamorphAudioProcessorEditor`'s stuck-drag reconcile | `isMouseButtonDownAnywhere() && ! anyPhysicalMouseButtonDown()` | **no** — it never reads the latch; it reads the mouse |

The second predicate is the one that matters, and KI-013 is why it can still be true on the re-entry:
the macOS realtime query does not refresh JUCE's cached button state, so
`isMouseButtonDownAnywhere()` does not go quiet by itself there. Reached that way from inside
`endChangeGesture`, the nested call closed the **same** parameter's gesture a second time, and — the
half the review did not name — cleared the identifiers, so the outer call then **skipped** the
sibling gesture it had not reached yet and left it open.

`endBandMove` is the third site and gets the same two lines.

**Reproduced before it was patched**, because this class is reentrant rather than cross-thread and so
is deterministic: a real `AudioProcessorParameter::Listener` on a real parameter, re-entering
`cancelActiveDrag` from the close it is watching. State test 83, legs A (split drag), B (width drag)
and C (band move) each counted **2** closes on the pre-fix tree; leg D is the control that an
uninterrupted cancellation still closes exactly **once**, so clearing first cannot have turned the
function into a no-op.

**The two sites are one ensemble, and the mutations say so:**

| Mutation | Killed |
|---|---|
| `cancelActiveDrag`'s clear moved back after the dispatches | legs A + B, 2 checks |
| `endBandMove`'s clear moved back after the dispatches | **nothing** |
| both (the pre-fix tree) | legs A + B + C, 3 checks |

So `endBandMove`'s clear is not dead weight and not a single-line proof either: it is the layer that
still refuses the double close once `cancelActiveDrag`'s cheap exit is gone. That is the same "no
single layer is measurable, only the ensemble" shape the `removeBand` count proof already carries,
and the reason a surviving single-line mutation at either site is not evidence of a hole. **An
earlier draft of both source comments called `endBandMove`'s half "defence in depth with no reachable
test"** — that was written from the first mutation alone and is corrected in place; leg C reaches it.

**Ownership and gesture semantics are unchanged.** Both functions make the same calls, on the same
parameters, in the same order; only the point at which the members are cleared moves. What changes is
the reentrant path — from a double close plus a leaked-open sibling, to a cheap exit.

State 2 851 / 0, TSan 0 warnings with `Matched 1 suppressions`, all four topology probes 0.
`worklogs/SPECTRUMIMAGER_TOPOLOGY_TRANSACTION_AUDIT_v0.9.8.md` §58.

## Applied again 2026-09-10 — the record a release action owns can be dropped by something else

**Review finding at `cancelActiveDrag`'s first two lines, *"reentrant cancellation disarms release
ownership"*. CONFIRMED, reproduced deterministically, and it is this ADR's own Decision defeated from
outside the handler the ADR converted.** No new decision: the Decision sentence already says what
must hold.

> A gesture's ownership lasts as long as the on-release actions that depend on it.

`mouseUp` honours that for its own exits — every branch drops the snapshot *after* its action. What
nothing enforced is that **something else must not drop it first**. `cancelActiveDrag` begins with an
unconditional `gestureBands = -1;` and only then takes the cheap exit that is supposed to make a
re-entrant call a no-op. The clear sits *in front of* that exit, so a nested call which closes
nothing still disarms the record — and every ownership predicate in this class self-disables at
`gestureBands < 0`.

**The window is REENTRANT, not cross-thread.** The release branch clears its identifiers before it
dispatches — which is this ADR's own instruction — and `setSoloMask` calls `beginChangeGesture()`
**before** its guard. A host that answers the gesture open by pumping the message loop re-enters
`cancelActiveDrag` while the store is still on the stack.

**There are two re-entry vectors, and the stronger one is inside this class.** An independent
adversarial derivation found it after this ADR was first drafted, and the correction is recorded
rather than smoothed over:

* **`SpectrumImager::tick`'s own reconcile**, `if (gestureIsStale()) cancelActiveDrag();`. Its gate is
  true **precisely in the scenario at issue** — the same-count install that lands inside
  `beginChangeGesture` is what makes `gestureIsStale()` true — and `tick` is a VBlank callback on the
  message thread, so a nested message-loop pump delivers it. This vector needs no mouse state and no
  KI-013. **It is the primary one.**
* **The editor's stuck-drag reconcile** (`PluginEditor.cpp`, `isMouseButtonDownAnywhere() &&
  ! anyPhysicalMouseButtonDown()`). Weaker than first written here: JUCE updates the source's button
  state *before* dispatching `mouseUp`, and the native peers clear the button bit when they handle the
  release, so for a real in-window release that gate is normally already false. It survives as a
  vector only through KI-013's stale macOS cache. Stated at its real strength.

**Measured, deterministically, on a single thread** (State test 79 leg E, which is leg C with the
cancellation added ahead of the identical install):

```
[leg E] a re-entrant cancellation disarmed the record and the solo bit was written anyway:
        mask 0x8, split 1 2000.0 -> 6500.0 Hz
```

Leg C refuses that bit. Leg E is the same install reached around the same guard.

**Branch by branch, at the strength each actually has** — corrected after the adversarial pass,
because the first draft of this section overstated the delete branch:

* **Solo — the live defect.** The only branch where the disarm becomes a wrong write. Its count and
  mask guards survive (they are the locals `pressBands` and `m`); **exactly the sound half this ADR
  added is the half that is lost.**
* **Handle drag — fail-safe but lossy, not merely "defended".** Its bespoke `gestureBands ==
  pressBands` compare makes the nested cancel produce a **refusal**: a legitimate outward-drag removal
  is silently dropped. No wrong write, but not free either.
* **Delete — NOT reachable by reentrancy.** Between `pressDeleteBand = -1` and the
  `deleteHit(...) == dB && ! gestureIsStale()` gate there is no dispatch, and `removeBand`'s entry
  ownership proof is preceded only by pure reads. The first draft here claimed its per-store proofs
  were exposed; that is **too strong**. The one residual sub-window is `removeBand`'s later
  `setSoloMask`, which does dispatch before its own sound clause — but that sits *after* the
  transaction's ownership proof and behind its count and mask checks.
* **Width drag — inert.** It fires no on-release action and nothing after its `endGesture` reads the
  record.

The handle-drag branch names the mechanism in its own comment — *"clearing `dragHandle` above sends a
reentrant reconcile down `cancelActiveDrag`'s cheap exit, which clears `gestureBands` … and
`gestureIsStale()` would answer `false` because the latch is gone."* That was one branch's local
defence against a general defect.

**The fix, and why it is this one.** A scoped flag: `mouseUp` owns the record for the duration of its
release action, and `cancelActiveDrag` declines outright — touching nothing — while it is set. Two
production lines plus an RAII helper.

Rejected: **moving the clear after the cheap exit**, because the clear is deliberately in front of it
(ADR-0039) — `mouseDown` latches `gestureBands` on branches that latch *no* identifier (an Alt-click
reset, a refused add), and nothing else would ever clear those. Rejected: **copying the handle-drag
branch's `gestureBands == pressBands` compare to the other branches**, because that is the same
ad-hoc guard at three more sites and still leaves `removeBand`'s mid-transaction per-store proofs
disarmed; making the record survive fixes all of them in one place.

**Verified:**

| Mutation | Killed |
|---|---|
| `cancelActiveDrag` no longer declines during a release action | State test 79 **leg E** |
| `mouseUp` no longer claims the record | State test 79 **leg E** |
| the handle-drag branch's bespoke `gestureBands == pressBands` compare removed | **nothing** |

The third is the honest result and is why it is worth stating: that compare was always labelled
unmeasured, and with the record surviving it is now **redundant defence in depth**. It is kept — one
integer compare on every release, and a second layer costs nothing — but it is no longer the thing
holding the line, and it is not claimed as such.

**Not a gate item.** No DSP graph, signal flow, parameter registry, serialization, latency or plugin
format. **Thread model** — no new thread, no new cross-thread path, no new atomic ordering; this is
one `bool` written and read on the message thread only, and the change *removes* a state transition
rather than adding a path. **Build system** — untouched. No accepted ADR is conflicted: this applies
this ADR's Decision to the one function that could defeat it. **No human approval is required, and
none is manufactured.**

**A route considered and NOT closed, stated rather than left silent.** A nested `mouseUp` — a host
delivering a queued mouse-up from inside the same dispatch — would find every identifier already
cleared, skip all four branches, and reach the tail, which drops the record. The flag does not stop
that, because the nested frame's own `ScopedReleaseAction` is a plain set/clear rather than a
re-entrancy counter. There is no evidence in this repository that a host does this, no test reaches
it, and closing it speculatively would mean restructuring a handler whose exit ordering is the
subject of this ADR. It is recorded here as an open, unmeasured route rather than fixed on a guess.

## Applied again 2026-09-10 — the one startup that opens two gestures

**Review finding at `SpectrumImager.cpp:2658`, *"reentrant band start leaks gestures"*. CONFIRMED and
reproduced deterministically, and it is this ADR's Decision defeated from a third direction: not an
on-release action this time, but an action that is still *establishing* the gestures it owns.**

### Why this site and no other

`beginBandMove` is the **only** function in the class that brackets more than one parameter, and the
two opens are two statements:

```cpp
if (soloMoveLeft  >= 0) beginGesture (freqP[soloMoveLeft]);    // DISPATCHES
if (soloMoveRight >= 0) beginGesture (freqP[soloMoveRight]);   // not reached yet
```

Every other opening site in the file — `mouseDown`'s three press branches, `resetParam`,
`resetCrossover`, `commitFreqEditor`, `setBands`, `setSoloMask` — opens exactly one, so its bracket
has no interior for a re-entry to land in. That is a structural fact about the file, not a
disposition: an audit of every `beginChangeGesture` call site found two in one function and one
everywhere else.

`mouseDrag` publishes `soloMovedBand = true` **before** calling in, which is what makes the interior
reachable state rather than dead state:

```cpp
if (! soloMovedBand)  { soloMovedBand = true; beginBandMove (soloPressBand, gestureBands); }
```

### What a re-entry did

A host that pumps the message loop from the first `beginChangeGesture` lands `tick`'s reconcile —
`if (gestureIsStale()) cancelActiveDrag();` — in `cancelActiveDrag` with the move's members set and
half its gestures open. Round 7's guard did not cover it: that flag is set only by `mouseUp`. So the
nested call ran `endBandMove()`, which closes **both** pins from the members. Three consequences, all
measured rather than argued:

1. an `endChangeGesture` on a parameter that was **never opened** — the negative open-gesture count
   and spurious undo boundary this ADR already names, arrived at from the opposite side;
2. the members cleared, so the outer frame's second statement opened nothing at all;
3. `soloPressBand` and `gestureBands` cleared, so the handler this returns into auditioned
   `1 << -1` — undefined behaviour, reaching the processor as a mask the press never named — and
   then called `moveBand` with every ownership predicate self-disabled and no gesture open,
   writing the pre-press split positions back over whatever the host had installed. That last one
   is the ADR-0040 / ADR-0047 failure exactly, reached not by a race but by the record being
   dropped mid-startup.

### The reconcile that is live here — and a claim of this section's own first draft, corrected

**Only `tick`'s reconcile reaches this window, which is the opposite of the release-side ones**, and
the first draft of this section named both. During a **drag** the button is genuinely down, so the
editor's stuck-drag reconcile — `isMouseButtonDownAnywhere() && ! anyPhysicalMouseButtonDown()`,
`PluginEditor.cpp` — is **inert**. KI-013 was resolved in round 4 by giving that predicate the OS's
real button state (`+[NSEvent pressedMouseButtons]` on macOS), and that resolution is precisely what
makes it inert here; on the release side, where the button is up, it is the live one.

And `tick`'s gate is **false on entry**: `mouseDrag`'s first statement has just proved the record,
and everything between that proof and the dispatch is a pure computation. So this is not a
free-standing re-entry — it has a **precondition**: the host writes a parameter from inside the
gesture open, which makes `gestureIsStale()` true, and the loop it pumps then runs the reconcile.
That is the order State test 83 leg E performs, and an earlier draft of the fixture had the two the
wrong way round. The direct `cancelActiveDrag()` call in the leg stands in for `tick`, which cannot
be driven from a headless fixture — it returns at `isShowing()` before reaching the reconcile, the
standing residual already recorded for the held-audition guard — so what the leg exercises is the
reconcile's **body**, reached with its gate made true one line above.

### The review's own wording, corrected

The finding says *"the gesture remains open"*. It does not: the leak is in the **other** direction.
`endBandMove` closes both pins, so the second pin is *closed without ever having been opened*, and
the outer frame then skips its open. The observable is a gesture count of **−1**, not a stuck-open
gesture. The rest of the finding — the reachability, the ordering, and `soloPressBand == -1` in the
resumed handler — is exact.

### The options, and why the guard

| | |
|---|---|
| **Extend this ADR's ownership claim to the startup** (chosen) | Two lines. The record simply survives, so all three consequences stop at once. `moveBand`, one statement later, still re-proves the count and every split it writes, so the cancellation is not lost — only deferred past the two opens. |
| B. Publish `soloMovedBand` only after both opens | Turns an unmatched **close** into an unmatched **open**: a nested cancel would then see `soloMovedBand == false`, close nothing, and leave both pins open forever with the identifiers gone. It also leaves (3) untouched. |
| C. Track which pins actually opened and close only those | Fixes (1) alone. (2) and (3) survive, because the members are still cleared under the outer frame. |
| D. Open from locals, assign the members afterwards | Same failure as B — a nested cancel finds `soloMoveLeft/Right` still `-1` and closes nothing. Ordering alone cannot fix this; the record has to survive. |

### A depth, not a flag — and this site is why

Round 7 shipped `bool releaseActionActive` with a note that a nested `mouseUp` would clear it early,
recorded as an open route. Adding a **second** user makes that hazard newly reachable: `beginBandMove`
is called from `mouseDrag`, so a host pumping the loop from the first pin's open can deliver a queued
mouse-up into `mouseUp` while the startup's claim stands, and a `bool` would have had that inner
scope's exit clear the outer one's. The member is now `int gestureActionDepth` and the scope object
`ScopedGestureAction`, counting in and out.

**This is unmeasured and is not claimed otherwise.** Degrading the counter back to a set/clear flag
kills **nothing** in the suite — no test nests the two sites. It is kept because the second site
creates the hazard and the counter costs the same instruction, on the same footing as `removeBand`'s
entry proof: correct by this ADR's rule, inert with nothing nesting, and with no reachable test.

**The nested-`mouseUp` route recorded in the previous section stays open.** The counter stops the
inner scope from clearing the outer claim; it does not stop a nested `mouseUp` from running its tail
and dropping the record there. No evidence in this repository says a host does this, and no test
reaches it.

### Evidence

State test 83 legs E and F. Leg C — the previous coverage — presses the **last** band, whose move
has one pin (`soloMoveRight == -1` because `b == N - 1`), so it could never see the interior; leg E
presses a **middle** band, which has both, found by walking the solo lane rather than by hardcoded
geometry. Leg E is one sequence with three assertions, one per consequence above.

| Mutation | Killed |
|---|---|
| `beginBandMove` no longer claims the record | leg E's three checks, and nothing else |
| `cancelActiveDrag` no longer declines | those three **and** State test 79 leg E — **4 checks** |
| `mouseUp` no longer claims the record | State test 79 leg E only — **1 check** |
| the depth degraded to a set/clear flag | **nothing** — see above |

The first three are the orthogonality proof: each claiming site is measured on its own, neither
subsumes the other, and the shared decline is measured by both. Leg F is the positive control — an
uninterrupted band move still opens both pins exactly once and closes both exactly once, so a "fix"
that simply stopped opening the second pin would fail it.
