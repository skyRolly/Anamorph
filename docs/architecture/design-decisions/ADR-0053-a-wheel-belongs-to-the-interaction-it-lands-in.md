# ADR-0053 — A wheel notch belongs to the interaction it lands in, and a scroll is one undo step

**Status:** Accepted (maintainer instruction, 2026-09-12 — *"modify the mouse-drag, mouse-wheel and
Undo/Redo behavior"* for every knob, slider, numeric value box, Multiband split, Multiband bandwidth
and the Band Solo interaction, with the explicit ruling that **"if previous behavior in the code or
existing documentation conflicts with this request, the behavior specified in this task is the latest
behavior and takes precedence"**).

**Architecture Review Gate: TRIGGERED, and cleared by that instruction.**
`docs/policies/ARCHITECTURE_REVIEW_GATE.md` and `docs/policies/AI_AGENT_POLICY.md` both make *"an
existing Accepted ADR conflict"* a hard stop that a green build cannot clear. This decision reverses a
Consequences line of Accepted [ADR-0041](ADR-0041-a-coupled-update-is-all-of-it-or-none-of-it.md), so
the stop applies. It is cleared the same way ADR-0041 and
[ADR-0052](ADR-0052-an-input-that-performs-no-edit-has-no-side-effects.md) were themselves entered —
by a maintainer instruction that states the new behaviour and rules on the conflict — and by nothing
else. **No other gate item is touched**: no parameter ID, range, default, automation flag or
serialization field changes; no DSP node, stage order or reported latency; no thread, cross-thread
path or atomic ordering (every line of this is message-thread state, as the undo coalescer already
was); no plug-in format and no build change.

**Architecture Review Gate: APPROVED by the owner, 2026-09-15 (round 23).** This is an approval of
the IMPLEMENTED DIRECTION, not a restatement of the 2026-09-12 instruction that opened the work, and
the distinction is the reason this block exists separately from the Status line above. The owner
reviewed the product direction as built -- a wheel notch belongs to the interaction it lands in; a
notch during a press modifies that interaction's own anchor; this ADR supersedes the relevant
wheel-during-drag Consequences line of Accepted ADR-0041 -- and approved it, with the direction
recorded as accepted and not to be reopened. Put to human review rather than decided by a green
build, exactly as `docs/policies/ARCHITECTURE_REVIEW_GATE.md` §Procedure step 2 requires; the
reasoning it was approved on is the *Architecture Review Gate* section below and the Decision it
introduces. Recorded in the form this repository has always used for a gate approval --
[ADR-0052](ADR-0052-an-input-that-performs-no-edit-has-no-side-effects.md)'s
*"Architecture Review Gate: APPROVED by the maintainer, 2026-09-09"* -- because step 2 names no
medium and the word *approval* appears nowhere in `ARCHITECTURE_REVIEW_GATE.md`,
`AI_AGENT_POLICY.md`, `ADR_POLICY.md` or `DOCUMENTATION_LIFECYCLE_POLICY.md` (re-measured 2026-09-15:
zero occurrences in all four).

**Architecture Review Gate: APPROVED by the owner, 2026-09-16 (round 30), for the two behaviours
round 30 changes.** The round-30 brief states both as settled and instructs that neither be put back
to the owner: *"Once a mouse press owns the interaction, every wheel event arriving before mouse-up
belongs to that active press, even when that press has no editable parameter target… Do not ask the
owner to approve this again"*, and *"A wheel adjustment during an active velocity drag must change
the parameter without corrupting the remaining mouse-drag mapping… Do not ask the owner to approve
this again"*. The first **reverses half of round 29's own reasoning** — that a press which latched no
identifier leaves the notch to the pointer — so the gate is triggered for the same reason it was in
rounds 23 and 29, and cleared the same way. **No other gate item is touched**, re-checked line by
line against the round-30 diff: no parameter ID, range, default, automation flag or serialization
field; no DSP node, stage order or reported latency; no thread, cross-thread path or atomic ordering
— the register gains a device key and a vector of cells, all of it message-thread-only and read and
written only from wheel and mouse handlers, exactly as before.

**Architecture Review Gate: APPROVED by the owner, 2026-09-16 (round 29), for the two behaviours
round 29 changes.** The round-29 brief states both as settled and instructs that neither be put back
to the owner: *"Multiband `Bandwidth` and `split frequency` must respond to horizontal trackpad
scrolling… Do not ask the owner to decide it again"*, and *"during an active drag on a Knob, Slider,
or MultiBand Bandwidth/split frequency, wheel input must continue to control the same active drag
target, even when the cursor is outside that control's bounds; the pointer must NOT retarget"*. The
second of those **reverses a Consequences line of this ADR itself** — *"A notch delivered to a
control other than the one being dragged edits THAT control"*, entered in round 14 — so the gate is
triggered for the same reason it was in round 23, and cleared the same way: by an owner instruction
that states the new behaviour and rules on the conflict. The reversed line is superseded below, not
deleted. **No other gate item is touched**, re-checked line by line against the round-29 diff: no
parameter ID, range, default, automation flag or serialization field; no DSP node, stage order or
reported latency; no thread, cross-thread path or atomic ordering (the register is one message-thread
`SafePointer` and one message-thread pointer, read and written only from wheel and mouse handlers);
no plug-in format and no build change. `RELEASE_COMPATIBILITY_CHECKLIST.md` is therefore **not
triggered**, by the same condition row 4 of the table below states.

**Gate compliance, audited 2026-09-14 (round 19) against `ARCHITECTURE_REVIEW_GATE.md` §Procedure,
step by step, because "cleared" above is a claim and this is its evidence.**

| Step | Requirement | Evidence |
|---|---|---|
| 1 | the author flags the change as gated | the paragraph above, and the PR #144 body's opening block |
| 2 | a human reviewer with DSP/audio context reviews against the relevant Policy + ADR | the maintainer instruction of 2026-09-12 quoted in **Status**, which states the new behaviour for every affected control and rules explicitly on the ADR-0041 conflict; ratified again by the owner on 2026-09-14 (*"the current ADR-0053 direction is accepted"*) |
| 3 | if the change is a decision, an ADR is added/updated | this ADR, and the superseded Consequences line recorded in **both** this ADR and ADR-0041 |
| 4 | compatibility-affecting changes additionally run `RELEASE_COMPATIBILITY_CHECKLIST.md` | **not triggered** — no parameter ID, range, default, automation flag or serialization field changes, which is the condition that checklist exists for |

**The form step 2 takes here is the form this repository has always used**, and that is the point of
recording it: ADR-0041 itself is *"Accepted (maintainer instruction 2026-09-07 …)"* and ADR-0052 was
entered the same way. A maintainer instruction that states the behaviour and rules on the conflict is
what clears the stop.

**Re-audited 2026-09-14 (round 20), and the question was put the other way round: does this
repository REQUIRE an `APPROVED` review?** It does not, and that is established by search rather
than by inference — the word *approval* (in any form) appears nowhere in
`ARCHITECTURE_REVIEW_GATE.md`, `AI_AGENT_POLICY.md` or `ADR_POLICY.md`. Step 2 asks for *"a human
reviewer with DSP/audio context reviews against the relevant Policy + ADR"* and names no medium.
The repository's own precedents record maintainer approval **in documentation**, not as a GitHub
review state: `policies/THREADING_POLICY.md` (*"approved by the maintainer"*, KI-027) and
`procedures/TESTING.md` (*"the maintainer reviewed and approved it on 2026-08-11"*). So the artifact
the gate actually requires exists.

**And the missing one cannot be produced from here, which is a fact about GitHub rather than a
choice.** The session's GitHub principal is `skyRolly` — this PR's own author — and GitHub refuses
self-approval. Submitting one would in any case be the agent approving its own work, which is the
thing an architecture gate exists to prevent. It was therefore not attempted.

**~~The exact missing artifact, for the owner~~ SUPERSEDED 2026-09-15 by the approval block at the
top of this ADR.** The two paragraphs that stood here asked for an `APPROVED` review on PR #144 and
recorded that none existed. The owner has since approved the implemented direction, and that approval
is recorded above in the repository's own documentary form. They are left in place rather than deleted
because what they establish is still true and still load-bearing: `ARCHITECTURE_REVIEW_GATE.md` names
no medium for step 2, a GitHub review state was never what the gate required, and no `APPROVED` review
was manufactured from this session -- the principal here is the PR's own author and GitHub refuses
self-approval, which is the outcome the gate exists to produce.

The standing RECOMMENDATION they carried is also unchanged and is still the owner's to take if wanted:
one sentence in `ARCHITECTURE_REVIEW_GATE.md` saying which medium step 2 takes would close the
ambiguity permanently instead of once per round. Three rounds have now re-derived the same answer from
the same four files.

**Re-checked 2026-09-15 (round 21): unchanged in every particular.** The PR still carries five
reviews and all five are still `COMMENTED`; no round-21 change touches this ADR's scope (the wheel
rules, the batching and the 24 Hz cadence are untouched — what changed is which DOOR the editor's
timer enters the poll through, and whether that door may wait for a lock). Round 21 adds a SECOND
gated item of its own, a Thread Model change, whose gate audit lives in **ADR-0036 §26** rather than
here. Both are cleared the same way and both are missing the same optional artifact, so the
recommendation above now covers two ADRs: an approving review on PR #144 referencing ADR-0053 and
ADR-0036 §26.

**Re-checked 2026-09-15 (round 22): unchanged again, and the recommendation now names three items.**
Re-read against `ARCHITECTURE_REVIEW_GATE.md` §Procedure rather than assumed: PR #144 still carries
five reviews, all `COMMENTED` (one code-scanning bot, four owner disposition replies). No round-22
change touches this ADR's scope either — the wheel rules, the batching, the 24 Hz cadence and the
scroll-step naming are untouched. What round 22 changes is (a) WHEN one acquisition of
`soundReplacement` is taken, which is **ADR-0036 §27**'s gated item and audited there, and (b) that
two editor paths now state an ADR-0008 REFUSAL when they produce no value — which uses round 19's
existing mechanism, adds no bit and no state, and is an ADR-0008 correction rather than a gated
change. So the outstanding optional artifact is one review naming ADR-0053, ADR-0036 §26 and
ADR-0036 §27.

**And a governance finding worth stating plainly, because it is the answer to "is the existing
instruction enough?".** It is. No document in this repository — `ARCHITECTURE_REVIEW_GATE.md`,
`AI_AGENT_POLICY.md`, `ADR_POLICY.md`, `DOCUMENTATION_LIFECYCLE_POLICY.md` — requires a GitHub review
state for step 2, and several Accepted ADRs here were entered on a maintainer instruction alone
(ADR-0041, ADR-0052). What IS genuinely short is narrower than "no approval": for §26 and §27 the
cited artifact is an instruction to MAKE the change, written before it existed, not a review OF the
change as made. That gap is recorded in both sections rather than counted as satisfied.

**Supersedes ONE Consequences line of [ADR-0041](ADR-0041-a-coupled-update-is-all-of-it-or-none-of-it.md)
— *"A wheel tick during a drag ends the drag"* — and PRESERVES ADR-0041's Decision, which is what makes
the replacement safe.** ADR-0041 stays **Accepted**: its rule that *a refresh which cannot bring every
piece of state the next write depends on to the same authoritative sound refreshes none of it* is
unchanged, relied on below, and still the reason the obvious implementation of this task is wrong.

**Extends [ADR-0052](ADR-0052-an-input-that-performs-no-edit-has-no-side-effects.md) to a branch that
did not exist when it was written**, and closes the multiband half of
**KI-010** (`docs/KNOWN_ISSUES.md`).

## Context

Five control families, three different wheel behaviours, and none of them was the behaviour asked for.

| While the mouse is held | What the wheel did before this ADR |
|---|---|
| a knob, a slider, a value box | **nothing at all.** `juce::Slider::Pimpl::mouseWheelMove` is wrapped in `! e.mods.isAnyMouseButtonDown()`, so the event was consumed and discarded |
| a Multiband split or bandwidth drag | **ended the press** (ADR-0041): the gesture closed at the notch, the edit so far became its own undo step, and further mouse movement did nothing |
| a held Band Solo button | **ended the press and edited the band's Bandwidth** — the click was swallowed and the wheel latched on whatever the pointer was over |

| With no button held | What a scroll recorded in Undo |
|---|---|
| a knob, a slider, a value box | **one undo step per notch.** JUCE wraps each notch in its own `ScopedDragNotification`, so a ten-notch scroll was ten steps to walk back |
| a Multiband split or bandwidth | **no undo step at all.** Every wheel store was a bare `setValueNotifyingHost` outside any change gesture, so `openGestures` never left zero and `pollUndoCoalesce` folded the value into the committed baseline with nothing to reverse it (KI-010's second path) |

## Problem

### 1. A notch cannot simply write the value, because every drag here recomputes from an anchor

This is the part ADR-0041 already measured, from the other direction. Not one of these drags reads the
live parameter again after it starts:

* a split drag stores `cursor.x - dragGrabDX` (`SpectrumImager::mouseDrag`);
* a width drag stores `yToWidth (cursor.y - dragGrabDY)`;
* a band move stores two pins at `bandStart{Left,Right}X + clamp (cursor.x - bandAnchorX)`;
* the value box stores `downProp + (-dragY) / 180.0`;
* `juce::Slider` recomputes `valueOnMouseDown + mouseDiff / pixelsForFullDragExtent` on **every**
  mouse move (`handleAbsoluteDrag`; `handleRotaryDrag` is unreachable here, no slider uses `Rotary`).

So a notch that writes the value and stops is erased by the very next mouse event. ADR-0041 measured
exactly that shape — *"a wheel tick adopted the installed width 1.700, and the drag then wrote 0.650
from an anchor taken before it"* — and its answer, with the tools it had, was to end the press.

### 2. JUCE's slider drag baseline is unreachable

`valueOnMouseDown`, `mouseDragStartPos`, `valueWhenLastDragged` and `lastAngle` are members of the
private `Slider::Pimpl`; `pimpl` is private and there is no public setter for any of them.
`getThumbBeingDragged()` is the only part of that state a subclass can read.

### 3. An undo step cannot be created from nothing

The coalescer is gesture-gated by design (ADR-0008, ADR-0036): `parameterGestureChanged` counts open
gestures and `pollUndoCoalesceAdopted` commits exactly one step when the last one closes. A change
with no gesture behind it is host automation and is deliberately folded into the baseline. So
"one undo step per scroll" is a statement about **gestures**, not about a new bookkeeping mechanism —
and "the previous step is extended rather than replaced" has to be expressible in a stack whose
entries are whole state snapshots.

## Options

### For the notch itself

| | Option | Verdict |
|---|---|---|
| A | Keep ending the press (ADR-0041, unchanged) | **Rejected by instruction.** It is the behaviour this task replaces |
| B | Refresh the gesture's world at the notch (`captureDragOrigins`) and let the press continue | **Rejected, and it is the defect ADR-0041 closed.** That call re-seeds the ownership record and the projection origins but CANNOT re-seed `dragGrabDX`/`dragGrabDY`, so the press would own a value its anchor predates. ADR-0052 re-measured the closest variant of this in its own round and recorded that it fires State test 73 leg A |
| C | Reimplement the slider drag in `Knob` so the baseline is ours | **Rejected.** It duplicates JUCE's absolute, velocity and snap-to-mouse branches, each with its own modifier handling, to gain one variable — and it silently stops tracking JUCE the next time any of them changes |
| D | End and restart JUCE's drag around the notch, wrapped in an outer gesture so the count never reaches zero | **Rejected.** Four extra gesture notifications reach the host per notch (begin/end/begin/end), which is two spurious automation touch spans; and the outer wrap nests `beginChangeGesture` on a parameter that already has one open, which JUCE itself asserts against (`jassert (! isPerformingGesture)`) |
| **E** | **Move the interaction's own ANCHOR by the notch's amount, and write through the interaction's own owned-store path** | **Chosen** |

### For the undo grouping

| | Option | Verdict |
|---|---|---|
| F | Hold ONE change gesture open across the whole scroll and close it on an inactivity timeout | **Rejected.** It is the literal reading of the request and it is the riskiest thing this repository could build: `pollUndoCoalesceAdopted` records nothing while `openGestures > 0`, so a gesture that fails to close stops undo recording silently — the exact defect measured in this same release (ADR-0050). It also needs a timer, which the task's own performance section asks us not to add when event-driven infrastructure will do |
| G | Leave each notch gesture-less and push undo steps from a new, bespoke API | **Rejected.** A second undo mechanism beside the coalescer, and the host loses the automation touch span it gets today |
| **H** | **Let each notch keep its own gesture, NAME the control while it is open, and make a commit whose name matches the last recorded step EXTEND that step instead of pushing another** | **Chosen** |

## Decision

> **A mouse-wheel notch belongs to whatever interaction it lands in. During a press it moves that
> press's own ANCHOR by the notch's amount and writes through that press's own owned-store path,
> inside the change gesture the press already opened — so the value is the combined result, the drag
> continues from it, further notches accumulate on it, and the whole interaction is one undo step at
> the release. With no press in flight a notch is its own interaction: it opens a change gesture and
> NAMES the control it edits, and a commit carrying a name that matches the most recently recorded
> undo step EXTENDS that step rather than pushing another — so a scroll, however many notches and
> however many pauses, is one Undo back to the value it started from. Any edit that is not that
> scroll's closes its gesture unnamed and ends the chain.**
>
> **And while a Band Solo button is held, a notch moves the BAND — the same thing a sideways drag of
> that button does — provided there is a band to move.**
>
> **A notch is delivered to the control under the POINTER, and that control acts on it whether or not
> some other control holds the press.** JUCE routes wheel events by pointer, not by capture, so a
> press on one control and a pointer that has travelled onto another lands the notch on the second
> one; it edits itself exactly as it would with no button down, inside whatever gesture is open.
>
> **A gesture NAMES the step it is about to request only if that gesture's own edit landed.** A press
> that changed nothing names nothing, and a burst whose owned store was refused names nothing — so
> neither can extend a scroll's step with a value the scroll did not produce.

### Why moving the anchor is the whole mechanism, and why it preserves ADR-0041

The branches this adds perform **no refresh**. They call no `captureDragOrigins`, re-stamp no
`gestureX`/`gestureW`, and read no parameter the press has not already proved. ADR-0041's rule is
about a refresh that cannot bring every piece of state to the same sound; there is nothing here for it
to refuse. What each branch does instead is one assignment:

| Interaction | The single variable | Set to |
|---|---|---|
| Multiband split press | `dragGrabDX` | `cursor.x - clamp (target)` |
| Multiband bandwidth press | `dragGrabDY` | `cursor.y - widthToY (target)` — the exact form the 3 px engage already uses |
| Band move (held solo) | `bandAnchorX` | `cursor.x - clamp (T + notch)` |
| Value box | `downProp` | `+= the proportion that actually fitted` |
| `juce::Slider` knob | `Knob::wheelDragProp` | `+= the proportion that actually fitted`, re-applied after every `Slider::mouseDrag` |

Every one of those targets is **clamped to the same limit the store itself clamps to** before the
anchor is derived from it, so a notch past the end of a control's travel banks no dead travel for the
drag to unwind afterwards. And the write itself still goes through `storeOwned` /
`writeCrossovers` / the press's open gesture, so ADR-0039, ADR-0040, ADR-0041 and ADR-0047 all still
hold at the store: the record stays current by construction, because the store is what maintains it.

### Why the extend rule needs no timer

An undo entry holds the state from **before** the step it undoes. So "preserve the value the scroll
started from and replace only where it ended" is precisely *do not push another entry, and move the
committed baseline on*. The step therefore exists from the first notch and is extended by every notch
after it, which makes *"one Undo returns the parameter to the value it had before the scroll"* true at
**every instant** rather than only after a dwell. That is a strictly stronger guarantee than the
inactivity window the request describes as the mechanism, and it costs no timer, no poll and no
held-open gesture.

The name is `parameterIndex + 1`, so a knob and the numeric box beneath it are one control — which is
what they are to the user. `0` means "not a wheel edit", and every other edit closes its gesture with
it: that is what makes a drag, an Alt-click reset or a typed value start a fresh step.

### What a second review round changed, and what it measured

Three corrections, each reproduced against the implementation before it was touched.

**1. The pointed control was silent where the multiband display was not.** `Slider::Pimpl::mouseWheelMove`
wraps its whole body in `! e.mods.isAnyMouseButtonDown()`, and the in-press branch above only claims a
notch when THIS slider owns the drag (`getThumbBeingDragged() >= 0`). A notch delivered to a knob while
another control held the press therefore reached JUCE and was dropped: measured as *"the notch was
dropped: Width stayed at 1.0000 while another control held the press"*. The fix hands JUCE the same
event with the MOUSE BUTTONS CLEARED and nothing else changed, which keeps JUCE's own wheel amount,
interval, snapping, duplicate-event filter and `ScopedDragNotification` bracketing instead of restating
any of them; it is gated on exactly what JUCE needs to act, so a disabled slider still forwards the
untouched event to its ancestors. This makes the rule uniform — the pointed control acts, everywhere —
rather than leaving the multiband display as the only surface that answered.

**1b. And the fix's own first version was wrong for the value box**, which is why it has a leg of its
own. The box drags by steering `downProp`, an anchor of its own, and it maps 180 px of travel across
a box under 20 px tall — so the cursor leaves the box within a few pixels and JUCE delivers the rest
of that drag's notches to the KNOB. Letting the knob write the value there turned "nothing happens"
into something worse: the value jumped and the box's very next drag event recomputed from `downProp`
and erased it (measured: *"the box's next drag event erased it: back to 2.6700"*). A notch has to
reach the anchor the press is steering, so the knob first asks any child holding a drag gesture to
take it — `DragGestureOwner::takeWheelNotch`, the same named interface the editor's release-outside
reconcile already uses to reach a control living in an anonymous namespace. The child never forwards
the event onward, so the ask cannot come back through `Component::mouseWheelMove` and recurse. The
loop is behind `e.mods.isAnyMouseButtonDown()`, so an ordinary scroll pays nothing for it.

**2. An empty press must be transparent to a scroll.** Gesture closes are batched until the 24 Hz poll,
so a click that opens and closes a gesture without moving a value can share a batch with the next
notch. Its nameless close read as a DISAGREEMENT and cleared the batch's name, and the scroll after it
started a second undo step: measured as *"one Undo stopped at 1.8000"*. The poll already held the
principle — it records `lastStepWheelKey` only where it records a step, because "a gesture that changed
nothing has not interrupted the scroll" — and the close now applies the same test, using the
`soundParamGen` counter the S10 poll skip already maintains: one relaxed load at the batch's first open
and one at its close, no signature rebuild. `pendingStepNamed`, not `pendingGestureCommit`, is now what
says the batch already carries a name.

**3. A refused burst must not attribute someone else's write to the wheel.** The standalone multiband
branches open their gesture BEFORE their store (the store has to be inside it to be undoable), and the
open dispatches; a host answering it by writing the same parameter makes the store refuse (ADR-0047),
but the gesture still closes and the poll still sees a moved signature. Named, that write extended the
previous scroll's step — measured as *"one Undo stopped at 1.0000, not at the 1.1200 the previous
scroll ended on"*. Both branches now name the step only when their own store committed.

**The two halves of rule 3 cover different orderings, and the difference is measured, not assumed.**
`juce::ListenerList` calls listeners in REVERSE order of registration and this processor registers in
its constructor, so a host write made from a parameter listener during the gesture-open always lands
BEFORE the coalescer samples the generation — instrumented directly: open `gen=63`, close `gen=63`, so
the generation test alone already declines to name that burst. What the store result adds is the other
ordering, where the change arrives after the sample: a cross-thread write (ADR-0047's own case), or a
burst that part-wrote and then aborted, which `writeCrossovers` can do single-threaded because it
proves EVERY split in the row and not only the ones it moves. State test 86 leg M drives exactly that
and kills the split half of the rule; the width half has one store and no dispatch of its own before
it, so no single-threaded harness can enter its window (mutation M18, recorded as surviving).

### What a third review round changed, and what it measured

Five corrections and one refusal. Each defect was reproduced as a failing check against the
implementation before anything was touched, and each fix is killed by a mutation.

**1. A notch was worth less travel with a button held than without one — the exact asymmetry this
ADR exists to remove, still standing in the smallest notch.** JUCE floors a notch to at least one
`normRange.interval` of VALUE movement (`jmax (normRange.interval, std::abs (delta))`,
`juce_Slider.cpp`), and `SliderParameterAttachment` copies the parameter's interval onto the slider
(`newRange.interval = range.interval`, `juce_ParameterAttachments.cpp`) — so the grid is real: 0.001
for Amount, Width and the percentages, 0.01 for Drive and the gains, and 0.001 on the Settings
Persistence bar, which sets its own range. The in-drag paths restated JUCE's direction, scale and
rails but not its floor, so a macOS trackpad's smallest precise notch (`deltaY = 0.5/256`) asked for
0.0003 of Amount, `Slider::setValue` snapped it straight back, and the press ate it — however many
arrived. Measured: *20 sub-interval notches inside the press moved Amount by nothing at all, while
the same 20 with no button held moved it 0.0200.* The floor is now applied on the in-drag path too,
from ONE source (`anamorph::gui::wheelTargetValue`), and the accumulator banks what the control
ACTUALLY moved rather than what the notch requested — a request banked past a rail is dead travel
the next mouse move applies as a jump. State test 88 leg J; mutation M30.

**1b. ...and the floor brought JUCE's duplicate-event filter with it, which is the hazard this
round's own first fix created.** JUCE dedupes wheel events on `e.eventTime` for a reason it states
in the same breath as the floor -- *"since we're going to bump the value by a minimum of the
interval, avoid doing this twice"* -- so the two are one mechanism, and it is about two DISTINCT
events bearing one timestamp rather than one event delivered twice. The first version of the fix
argued the filter away on the second reading (single delivery, which the routing does guarantee)
and would have left a held control moving TWICE as far as an unheld one wherever a platform sends a
notch twice: the same contract broken from the other side. Both in-drag callers now keep their own
last-notch stamp. State test 88 leg L, which needs no platform -- it sends two events bearing one
timestamp, which is exactly the input JUCE's filter is written against; mutations M37 and M38.

**2. A drag that returned to where it started left the scroll chain standing.** The poll records the
chain's name only where it records a step, which is right for a gesture that changed nothing and
wrong for one that changed something and put it back: scroll, drag away and back, scroll again gave
ONE undo step and one Undo walked past the drag entirely — measured as *"the round-trip drag left
the chain standing: one Undo jumped past it to 0.0000"*. A batch that moved a sound parameter
(`pendingStepNamed`, the generation test rule 2 above already maintains) now ends the chain even
when the signature did not move — unless the name is the chain's own, because two notches of ONE
scroll can land in a single poll period and net zero, and ending the chain there would split a
continuous scroll into two undo steps. State test 86 legs N and P; mutations M27 and M28.

**3. Host automation in the commit window was attributed to the scroll.** A finished notch leaves
its commit request pending for up to a poll period, and a gesture-less write landing in that window
is folded into the step the poll is about to record. Named, it EXTENDED the step the user had
already finished: measured as *"the batch carrying the host write EXTENDED the first scroll: one
Undo jumped straight back to 0.0000"*. The window has two sides and both are real — the poll runs
after the close, and a whole new batch can open before the poll runs — so the generation is latched
at every gesture EDGE (open, close, poll) and a batch that finds the counter moved between two edges
is carrying somebody else's write. Such a batch extends nothing and is named by nobody, so the next
notch starts its own step rather than merging across the automation. State test 86 legs O and S;
mutations M26 and M29.

**What this does NOT do, stated plainly because the review asked for it.** The automation's VALUE is
still inside the step the poll records, so one Undo takes it back along with the scroll. Two ways out
were weighed and both are refused. A whole-state snapshot taken as the gesture closed would have to
be taken inside `parameterGestureChanged` — where D-2/ADR-0036 forbids the APVTS lock, a lock-order
inversion against a host-thread `replaceState` that `--d2-stress-probe` has already reported.
Patching the pushed baseline per parameter from a foreign-write set is **not** blocked by that lock
(round 12 checked: `parameterValueChanged` already receives the index it discards, and a `fetch_or`
into an atomic mask is lock-free) — it is blocked by what it would MEAN, which is the stronger
objection: an undo entry would stop being a state that ever existed, `dragCrossoverTo`'s pushed
neighbour splits are indistinguishable from automation by the only classifier available, and
`redo()` pushes an unpatched snapshot, so undo and redo would no longer round-trip. It is recorded
here as the open question it is rather than taken on the way past.

**And the residual is ADR-0008's, not this ADR's.** It exists because an undo entry is a whole
`StateSet` snapshot; every wheel rule above is about ATTRIBUTION — which step a write belongs to —
and attribution cannot separate a value from a snapshot that contains it. Round 12 found the same
consequence wearing a second face: a press that edits nothing still records a step when a
gesture-less write moved the signature beside it (State test 86 leg V), and that step's only content
is the write. Gating the push on "did this batch edit anything" removes that face and breaks two
others — the double-click reset, which has no gesture of its own and is undoable ONLY through the
signature rule (leg K), and the refused burst whose recorded step is what stops the next Undo
reaching past the automation into the previous scroll (legs I and J). Measured rather than reasoned:
all three failed under the gate, and it was withdrawn. While an undo entry is a whole state, a
foreign write is either inside the user's step or is a step of its own, and there is no third
answer. Changing that is an ADR-0008 decision for a maintainer, not a wheel fix. What is fixed is the attribution: no step of the
user's is retroactively edited, and no chain merges across automation. Leg O prints the measurement
each run (`after one Undo the host's Width reads 1.0000 (it wrote 1.4000...)`) so the residual is
visible rather than asserted away.

> **SUPERSEDED 2026-09-14 (round 17). An undo entry is no longer a whole `StateSet`, and automation
> is no longer any part of a user's step.** The maintainer-approved ADR-0008 amendment of 2026-09-13
> made an ordinary user step the parameters that action moved with per-parameter endpoints, and the
> round-17 correction finished implementing it: `before` is written once at the instant the batch
> first takes a parameter, `after` only from a value the owning gesture or store produced. The "no
> third answer" sentence above was true only of whole-state entries; with scoped entries the third
> answer is the one that now ships — a foreign write is in NO step. Every wheel rule in this ADR is
> unchanged, because every one of them is about ATTRIBUTION. **This ADR does not define what a user
> step contains; ADR-0008 does.** Round 12's withdrawn push gate is likewise superseded rather than
> revived: the empty-press face it was aimed at is fixed by the endpoints, not by a gate, so legs K,
> I and J stand untouched.
>
> **Extended 2026-09-14 (round 18).** "`after` only from a value the owning gesture or store
> produced" was true of the multiband display's declaring stores and not yet of anything else: every
> control bound by a JUCE parameter attachment declared ownership and no value, so its close still
> took a live read and a host write inside the gesture became the recorded `after`. The editor now
> witnesses the attachment write itself (ADR-0008, round-18 correction). **No wheel rule in this ADR
> moves**, and State test 91 leg G re-asserts chain extension and termination against the change so
> that it cannot be traded away for the endpoint fix.

**3b. ...and the first version of THAT fix broke the guarantee it serves, which is the round's own
best evidence for verifying a fix as adversarially as a finding.** Every program state jump -- Undo,
Redo, a preset load, an A/B switch -- polls FIRST and applies its parameters AFTER, and applying
them notifies the host, so the sound generation advances once per parameter with no poll behind it.
The edge generation was re-synced only at the poll tail, so the first notch after an Undo compared
against a generation stale by N, was read as carrying a foreign write, and went unnamed -- and the
notch after it started a second undo step. A two-notch scroll immediately after an Undo became TWO
steps: ADR-0053's primary guarantee, broken by ADR-0053's own fix. Every jump now re-syncs the edge
after its own writes. Measured as *"the scroll after the Undo was split in two: one Undo stopped at
1.8000"*; State test 86 leg T; mutation M39.

**One window stays open and is now said so in the source rather than implied:** a host write landing
between a gesture's own open and its own close is attributed to that batch by both edges. Closing it
needs a generation latched at the innermost open -- a per-gesture field for a window one gesture wide
-- and rule 3 above (name only if this burst's own store stood) already declines the reachable half.

**4. A notch that could not move a band still swallowed the solo click.** `beginBandMove` learned the
travel limits by SETTING them, so the only way to ask "can this band move?" was to convert the press
into a move first — and at the end of the travel the clamp returns the translation the band already
has, `moveBand` writes nothing, and the release then takes the move branch instead of the toggle's.
Measured: *"it swallowed the click: mask 0x0 where the uninterrupted press gives 0x2"*, with two
host change gestures opened on the band's edges for a band that did not move. The geometry half of
`beginBandMove` is now a pure `bandMovePlan`, which both the measurement and the move derive from,
so they cannot drift; the notch projects the move it would make and returns, untouched, when
`writeCrossovers`' own half-pixel threshold says nothing would be written. State test 87 leg F;
mutation M31.

**5. A standalone notch at a rail opened a host gesture for an edit that never happened.** Both
multiband branches opened the gesture before discovering that the clamped target was the value
already there — a touch/latch punch-in a DAW writes an automation point for. Both now ask first,
from the reading the tick has already taken and proved (ADR-0047), using the write path's own
threshold; the split branch also skips the record stamp, and the in-press width branch no longer
ENGAGES the width drag for a notch at 0.0 or 2.0, which would have left every later one-pixel tremor
writing widths. State test 86 legs Q and R; mutations M32, M33 and M34.

**The refusal: `ScopedWheelStep`'s destructor still writes 0 rather than restoring what it found,
and that is the right answer.** 0 is what it found. There are three construction sites and no call
chain joins any two: the imager's two are mutually exclusive branches of one handler, and
`juce::Component::mouseWheelMove` forwards UP only, with no `Knob` a descendant of another. The one
non-structural interleaving is a host pumping the message loop from inside `beginChangeGesture` and
delivering a queued notch over a different control. Recorded in the source at the destructor rather
than hardened: three lines no reachable path exercises and no test can fail.

> **Corrected in round 12, because the two clauses this paragraph used to end with were false and
> the source comment they came from had already retracted them.** They said the interleaving "leaves
> the outer step unnamed either way, because the disagreement rule already unnames a batch holding
> two differently-named gestures", and that "it would take the host running the 24 Hz poll inside
> that same pumped loop for the restored key to name anything". Neither holds. The disagreement rule
> lives inside the `--openGestures == 0` branch, and the INNER close counts 2 to 1, so it never
> reaches that branch and the rule never fires here. And a restored key would be read immediately,
> by the zero-crossing close's own latch — no poll is involved in naming at all. With a restoring
> destructor the batch would be NAMED, not unnamed, and the name would claim a batch that also holds
> the other control's edit; since an undo entry is a whole state, one Undo of the scroll would then
> revert that edit too. That is the real reason the cleared key is right. The cost of clearing is
> **two** extra undo steps mid-scroll, not one — the unnamed batch cannot extend, and the poll's
> `lastStepWheelKey = 0` stops the notch after it extending either — and there is one sub-case the
> paragraph does not cover: a queued notch over the SAME control, where restoring would keep the
> scroll whole and clearing splits it. Neither policy dominates; clearing errs toward extra undo
> steps, restoring errs toward swallowing another control's edit, and this ADR picks the former.

### What a fourth review round changed, and the one fix it withdrew

**1. The poll's edge was the generation of its FIRST LINE, not of the snapshot it commits.**
`pollUndoCoalesceAdopted` samples `soundParamGen`, builds the ~36-string signature, and only then
captures `committed` from the LIVE parameters. A host write landing in between is therefore inside
the baseline the poll commits while the sampled generation does not name it — so the next gesture,
comparing against that stale edge, read a batch nothing foreign had touched as carrying somebody
else's write, went unnamed, and made the notch after it a second undo step: the same two-notch
scroll becoming two steps that round 11's leg T was written for, arriving from a different
direction. Each branch that refreshes `committed` now re-reads the counter immediately before doing
so and the tail publishes that; a branch that refreshes nothing keeps the top-of-poll sample. The
remaining window is one-directional by construction — a write landing between the read and the
copy's last parameter is marked foreign, an extra undo step, never automation merged into a user's —
and reading after the copy instead would swap that for the opposite error, which is the one §3
exists to prevent. Cross-thread only: nothing the poll body calls re-enters a parameter write, so
State test 86 legs U and U2 place the write through the `insidePollBody` seam. Mutations M40, M41
and M42.

**2. The in-press split wheel banked travel the store refused, and this ADR's own words say it must
not.** The table above promises every anchor is re-derived from a target "clamped to the same limit
the store itself clamps to". The split branch clamped to the FRAME edges, which is not that limit:
`projectFromOrig` pushes the splits between the pin and the edge aside by `kMinGapPx` each and then
runs its ordering pass backwards, which pulls the PIN back to `hi - (M - 1 - handle) * kMinGapPx`.
With the first of three splits scrolled right that is 92 px banked and refused — nine notches to
unwind before the split moves again, and the same 92 px displacing the rest of the press's MOUSE
drag, which reads that offset and never rewrites it. `dragCrossoverTo` now reports where the
projection actually put the handle, and the anchor is derived from that. No second projection: the
reported value is `out[handle]`, the very row the stores are made from (ADR-0047). This APPLIES the
Decision rather than changing it. State test 80 leg G; mutations M43 and M44. The other three wheel
branches already obeyed the rule by three different mechanisms, and the in-source paragraph now says
which — the blanket claim was true of them and false of this one.

**3. `ScopedWheelStep` was re-examined and REFUTED again**, and this time the ADR's own
justification was the thing that had to change: see the correction above.

**4. One fix was withdrawn after the suite refused it.** Gating the undo push on whether the batch
actually edited anything looked right, and broke the double-click reset and the refused-burst
boundary. Recorded above under "the residual is ADR-0008's", because that is what it turned out to
be rather than a wheel defect.

**5. One residual is new and is recorded rather than fixed.** `foreignSinceEdge` is derived from the
raw sound generation, which `parameterValueChanged` bumps for every store; every other "did the
sound change" test in this plug-in asks the RENDERED signature, which snaps to the parameter's own
grid. A host write inside one step of a discrete parameter — or inside one interval of a float one —
therefore moves the counter and not the signature, and ends a scroll's chain that the poll's own
non-gesture branch would have left alone. Making the two agree means rendering inside
`parameterValueChanged`, which the header records as reachable from the AUDIO THREAD, so it is a
realtime and cross-thread-counter question rather than a wheel one. State test 86 leg W prints the
measurement on every run.

### What a fifth review round changed, and the defect the fourth one introduced

**1. Round 12's own fix left the foreign test measuring the wrong instant, and a review caught it
before CI could.** Round 12 correctly moved the published gesture EDGE to the generation of the
snapshot the poll commits — but left `foreign` computed from the sample the poll *opened* with. In
a poll that COMMITS a step those two must be the same instant, and they were not: a host write
landing while the signature was being built was absorbed into the baseline, reported as nothing
foreign at all, and the step's name survived. Every later notch then extended a step the automation
was already inside. Measured before the fix: *"the chain extended straight across the host write:
one Undo went all the way back to 0.0000, taking the automation with it"*.

The correction is an ordering, not a mechanism: **capture the baseline, THEN read the counter, THEN
decide**, and use that one value for both `foreign` and the edge. Reading after the capture is what
makes the residual one-directional — every write that can be inside the baseline is counted, and
the only writes the test can over-report are ones that landed after the capture and are therefore
not in it, which costs an extra undo step and never a merge across automation. Reading before the
capture inverts exactly that, which is the bug it replaces. **The signature is deliberately not
consulted**: it reads parameters one at a time, so it can miss a write the baseline still holds and
cannot answer this question; the counter and the snapshot can, because they are taken at the same
instant and in that order. State test 86 leg U2; mutations M45, M46, M47.

Note that the two branches need OPPOSITE answers from the same edge, which is why each has its own
leg. In the non-gesture FOLD the chain is already ended by `lastStepWheelKey = 0`, so the only
question is whether the scroll after it is penalised a second time (leg U: it must not be). In the
COMMITTING branch nothing else ends the chain, so the edge is the only thing that can (leg U2: it
must).

**2. The automation-inside-a-user-step residual is re-classified, and escalated.** Round 12 recorded
it as an accepted consequence. Round 13 was given the product rule explicitly — *host automation is
not a user Undo/Redo action, and must not become part of a user-action step merely because it
happened inside a pending snapshot or coalescing window* — and against that rule it is a **defect**.
It is also nearer to ADR-0008's own Decision than round 12 allowed: that Decision already says host
automation *"folds into the baseline **without** a step"*, and a value one Undo reverts has been
given a step's worth of undoability. What ADR-0008 does not decide is the case where the write lands
inside somebody else's step — which is this one.

**This ADR does not decide it either, and deliberately does not.** Every correct fix needs
per-parameter attribution, which makes an undo entry a synthesis rather than a state that ever
existed — contradicting ADR-0008's *"stacks of `StateSet` snapshots"* in terms. That is a hard stop
under `ARCHITECTURE_REVIEW_GATE.md`, so it is recorded in `docs/FUTURE_RISKS.md` RISK-012 as
**CONFIRMED, ESCALATED, pending a maintainer decision on ADR-0008**, with the design, the second
(thread-model) trigger, and the measurements. Round 12's claim that the multiband split drag would
break under such a classifier is **withdrawn**: `mouseDown` holds `beginGesture (freqP[h])` for the
whole drag, so the pushed neighbour stores are inside the batch by the classifier that matters.

**3. `ScopedWheelStep` re-checked, still unreachable.** This round's only source change is inside
`pollUndoCoalesceAdopted`: no wheel dispatch, no component routing, no new construction site. The
refutation and its correction above stand unchanged.

### What a sixth review round changed, and the defect it found in the write sequence

Three things, and none of them touches this ADR's Decision: a notch still belongs to the interaction
it lands in, a scroll is still one undo step, and the extend rule still needs no timer.

**1. A drag inside a scrolled press published the PRE-WHEEL value first, and corrected it
afterwards.** `Knob::mouseDrag` called `juce::Slider::mouseDrag` and then `applyWheelDragOffset`, and
JUCE's own drag write is a `setValue (..., sendNotificationSync)` that reaches
`SliderParameterAttachment` -> `setValueAsPartOfGesture` -> `setValueNotifyingHost`. With a notch
banked, the value it published was the pure drag value — the interaction's position with the notch
absent. Measured on the Drive knob with one notch banked and one further 2 px drag event, through a
`juce::AudioProcessorListener` (the host's own view) with the DSP atomic sampled in the callback:

```text
host: BEGIN gesture
host sees norm=0.080000  DSP atom=1.920000      <- pure drag
host sees norm=0.155000  DSP atom=3.720000      <- corrected
-- one further 2 px drag event --
host sees norm=0.087917  DSP atom=2.110000      <- pure drag, a whole notch BACKWARDS
host sees norm=0.162917  DSP atom=3.910000      <- corrected
host: END gesture
```

Two writes per drag event instead of one; a control with no notch banked writes once. Both land
inside the single touch/latch punch-in the press holds, so a host recording automation wrote the
backwards spike into the lane, and the audio thread could read 2.11 dB while the control stood at
3.91. Nothing measured *between* events was ever wrong, which is why forty checks in State test 88
passed over it: the defect is entirely in the write SEQUENCE.

The fix is the rule the value box and the multiband display already obey — compute the combined
value, then write once. `juce::Slider::snapValue` is a virtual JUCE calls immediately before that
same drag write, so `Knob` overrides it and folds the notch total in there; `applyWheelDragOffset`
and the `mouseDrag` override are deleted. It is gated on `wheelDragProp != 0` rather than on
`dragMode`, because JUCE leaves `dragMode == notDragging` for the plain `Rotary` style, and it takes
its base from `snapToLegalValue (attempted)` rather than from the raw attempted value, because that
is what the old `getValue()` returned after `constrainedValue`. Measured equivalent to the previous
code on 8640 sequences — every APVTS parameter x four drag paths x five start values x four wheel
deltas x three notch positions — where the un-snapped form differs on 89 of them. State test 88
leg M is the observable; it needs a processor-level listener, because JUCE walks a parameter's own
listener list in reverse registration order and a test listener added there is called before the
APVTS adapter that stores the atomic.

**2. The double-click reset wrote outside any change gesture, and the comment saying it could not be
wrapped was false.** `PluginEditor.h` recorded that wrapping the double-click reset "would nest
begin/endChangeGesture on the same parameter", on the premise that the second press's
`ScopedDragNotification` is still open. JUCE dispatches `mouseDoubleClick` from
`Component::internalMouseUp`, **after** `mouseUp` — so the press's gesture is already closed, nothing
nests, and JUCE's own `Slider::Pimpl::mouseDoubleClick` wraps its write for exactly that reason. The
reset is now bracketed like the Alt-click path beside it and gated on `resetWouldMove()` (ADR-0052).
A host recording in touch or latch now sees one punch-in span for a double-click reset where it
previously saw a gesture-less write.

**3. The extend rule is unchanged and is now expressed literally.** Under ADR-0008 as amended
(2026-09-13) an undo entry carries the parameters the user's batch moved with both of their
endpoints, so "keep the value the scroll started from and replace only where it ended" is no longer
achieved by *not pushing an entry* — it is the entry keeping each parameter's original `before` and
taking the new `after`, with a parameter the chain touches for the first time joining it. The
observable behaviour of a scroll is the same; what changes is that a host write arriving beside the
scroll is no longer inside it, which is what the amendment was for.

### What a seventh review round changed: the axis, the owner, the anchor

Four things. The Decision is unchanged in every word — a notch belongs to the interaction it lands
in, a scroll is one undo step, the extend rule needs no timer — but round 29 found that *"the
interaction it lands in"* had been implemented as *"the interaction the POINTER is over"*, which is
not the same sentence, and that one of the two ways a notch reaches a drag's anchor was applied on
the wrong side of JUCE's clamp.

**1. BOTH AXES ARE READ, and the rule is JUCE's own, for a reason that is not stylistic.** Devin
`src/gui/SpectrumImager.cpp:R3293` observed that the multiband display read `wheel.deltaY` alone. A
trackpad's sideways two-finger scroll arrives as `deltaX` with `deltaY` at zero, so that gesture moved
every knob in the editor and did nothing at all over the display — the inconsistency the finding
names. The fix is one shared spelling, `anamorph::gui::wheelDominantDelta`, which is the expression
`juce::Slider::Pimpl::mouseWheelMove` already uses and which the knobs therefore already obeyed:

```cpp
(std::abs (deltaX) > std::abs (deltaY) ? -deltaX : deltaY) * (isReversed ? -1 : 1)
```

Two properties of it were measured rather than assumed, because a bespoke rule would have been wrong:

* **The negation of `deltaX` is a cross-platform normaliser, not a taste.** macOS fills `deltaX` from
  Cocoa's `scrollingDeltaX` and passes its sign through
  (`juce_NSViewComponentPeer_mac.mm`, `deltaX = ... / 256.0f` beside the same expression for `deltaY`);
  Windows **negates** it at the peer (`juce_Windowing_windows.cpp`, `deltaX = amount / -256.0f` against
  `deltaY = amount / 256.0f`); Linux/X11 hard-wires `deltaX = 0` and drops buttons 6 and 7
  (`juce_XWindowSystem_linux.cpp`). Reusing JUCE's expression is the only choice that means the same
  gesture on all three; any locally invented sign convention is correct on at most two.
* **Dominant-axis, not a sum.** The knobs have used dominant-axis since they were written, because it
  is JUCE's. A sum would make a diagonal trackpad flick move a control by the two axes added
  together, which is a behaviour change to eleven knobs that nobody asked for. The display now shares
  the knobs' rule rather than acquiring a second one — the answer to *"do not create
  control-dependent behavior without evidence"*.

Nothing downstream of `dy` changed: `kWheelSplitPx`, `kWheelWidthMin`/`kWheelWidthPer`, the band
move's translation, the sub-threshold no-op test and the undo grouping all consume the same scalar.
The gesture simply has a second way of producing the number. State test 105 leg I holds both axes on
both control families; legs C and D hold a **horizontal** notch steering Bandwidth and the split
frequency specifically, which is what §2 of the brief asked to be verified as parameter movement
rather than as a nonzero delta.

**2. THE PRESS OWNS THE NOTCH, AND JUCE CANNOT EXPRESS THAT.** Traced from the source rather than
inferred: `MouseInputSourceImpl::handleWheel` asks `getTargetForGesture`, which is
`peer.getComponent().getComponentAt (pos)` — a bare hit-test that never consults the drag. The only
stickiness JUCE has is `lastNonInertialWheelTarget`, and it applies to the inertial phase of a
gesture, not to a held button. So the routing this decision needs does not exist in the framework and
has to be expressed by the application, once, in a form every control consults:

```cpp
// as round 29 shipped it; round 30 renamed the question and keyed the register by device
void  claimDragWheel   (juce::Component&, WheelDragOwner&, WheelPointer);   // at mouseDown
void  releaseDragWheel (const juce::Component&);                            // at mouseUp / cancel
bool  wheelTakenByAnyPress (juce::Component& self, WheelDragOwner* selfOwner,
                            const juce::MouseEvent&, const juce::MouseWheelDetails&);
```

`WheelDragOwner::takeWheelNotch` is the hook — split out of `DragGestureOwner`, which the value box
already implemented, so the *same body* serves a notch delivered by the pointer and one posted by the
register while the cursor is elsewhere. That is the answer to *"do not implement this only for one
control class if the same interaction abstraction is shared"*: `Knob`, `ValueBox` and `SpectrumImager`
each claim at their own `mouseDown`, each ask `wheelTakenByAnyPress` as the first line of their own
`mouseWheelMove`, and each implement the one hook. The holder is a `juce::Component::SafePointer`, so
an editor torn down mid-press cannot leave a dangling owner, and a wheel event with no button down
clears the register on its way past — the lifetime is the press's, with a second way out.

**AND THE EDITOR IS THE BACKSTOP.** Three controls asking the register covers a pointer that has
wandered onto another CONTROL. Most of this editor's surface is not a control: captions, toggles,
panel backgrounds. None of them overrides `mouseWheelMove`, so `juce::Component`'s version walks the
event up to the nearest enabled ancestor (`juce_Component.cpp`) and it arrives at the editor — which
dropped it, losing the notch of a live drag because the cursor happened to be over a label. The
editor now asks the register first, at the one place all of those paths converge. The single
component that never arrives is `PopupShield`, whose `mouseWheelMove` is deliberately empty; a raised
shield means a pop-up menu owns the mouse, so there is no drag of ours for a notch to belong to.
State test 105 leg D holds this route and asserts that the component under the pointer is neither a
`juce::Slider` nor the display, so the backstop is the only way the notch can have arrived.

**3. THE BOUNDARY DEFECT: THE ANCHOR WAS OUTSIDE JUCE'S CLAMP.** Reported as *"hold the Amount knob,
drag 0 → 50 %, wheel back to 0 without releasing, keep dragging up — the control sticks around
50 %"*. It is not a clamp to special-case; it is the round-14 fold (§*What a sixth review round
changed*, item 1) applied on the wrong side of a clamp that is not ours. `juce::Slider::Pimpl`
computes `newPos = jlimit (0, 1, valueOnMouseDown + mouseDiff / pixelsForFullDragExtent)` and only
then calls `snapValue`, so an offset folded in `snapValue` is subtracted from an ALREADY-SATURATED
position. With the Drive knob (sensitivity 250 px for the whole range):

| step | cursor | JUCE's `newPos` | folded offset | result |
|---|---|---|---|---|
| drag up half a range | `cy − 125` | `0 + 125/250 = 0.500` | none yet | **0.500** |
| 24 notches down, in the press | `cy − 125` | — | offset becomes `−0.500` | **0.000** (the rail) |
| drag up a FULL further range | `cy − 375` | `jlimit(0,1, 375/250) = 1.000` | `1.000 − 0.500` | **0.500** ← stuck |

The fix keeps the fold but moves it **inside** the clamp, by expressing it in the same units JUCE's
own mapping is expressed in — pixels of drag travel — and shifting the EVENT rather than the value:

```cpp
wheelDragPx += (valueToProportionOfLength (getValue()) - base) * pixelsPerWholeRange();
...
juce::Slider::mouseDrag ({ ..., e.position + wheelDragShift(), ... });
```

The same row now reads `newPos = jlimit(0,1, (375 − 125)/250) = 1.000`, and the top is reachable with
the whole remaining travel intact. `pixelsPerWholeRange()` is `getMouseDragSensitivity()` for the
relative styles and `|getPositionOfValue (max) − getPositionOfValue (min)|` for the absolute linear
one, which is JUCE's `sliderRegionSize` measured from outside; `wheelDragShift()` puts the offset on
the axis that style drags along. `snapValue` is deleted, and with it the one-write property round 14
introduced is *kept* rather than restored: the shift is applied before JUCE computes anything, so
there is still exactly one write per drag event.

State test 105 leg F and leg G hold the reported sequence on the knob and on the absolute linear
slider. **Leg L is the one that proves the mechanism rather than the symptom**, and it exists because
legs F and G cannot: a full further range of drag saturates at the rail whether the notch was banked
or discarded, so both of them pass against an implementation that throws the notch away. Leg L brings
the cursor back to the exact point it was pressed at, where the drag contributes nothing by
construction and everything remaining is the notch's — measured `press 0.000 → drag 0.250 → wheel
0.700 → back 0.450` on the knob and `0.050 → 0.300 → 0.750 → 0.500` on the slider, each exactly
`press + (wheel − drag)`.

**4. THE MULTIBAND PARAMETERS WERE VERIFIED, AND ARE NOT CHANGED FOR SYMMETRY.** The brief reports
that Bandwidth and the split frequency do not show the boundary defect and asks for that to be
verified rather than assumed. They do not, and the reason is structural rather than lucky: every
multiband drag anchors in CURSOR space and clamps **once**, to the final target —
`yToWidth (cursorY − dragGrabDY)` for the width, `dragCrossoverTo (cursorX − dragGrabDX)` for the
split, `jlimit (bandTmin, bandTmax, cursorX − bandAnchorX)` for a band move — so a notch that moves
the anchor moves the whole remaining travel with it and nothing is banked outside a clamp. State test
105 leg H drives the reported sequence against the Bandwidth line and reaches the 2.000 rail unaided;
**no line of the width, split or band-move branches is modified**, which is the answer to *"if the
existing implementation genuinely avoids the defect, do not modify it merely for symmetry"*.

**One new rule did fall out of the routing, and it is a rule the pointer used to hide.** A split drag
carried more than 70 px sideways or 50 px vertically outside the frame marks its band for removal on
release and FREEZES, precisely so that *"when the cursor returns, the split is recomputed purely from
the cursor against the drag-start positions"*. Before the register a notch could not reach that
state — a cursor that far outside is over somebody else, and JUCE routes by hit-test — and it can
now. Creeping the frozen split would move one the release is about to merge away, and re-anchoring
`dragGrabDX` is exactly what the freeze exists to prevent, so the press **owns** the notch and adds
nothing to it, which is what a pending delete click has always done one branch above. State test 105
leg K holds the freeze, the ownership and the recovery on return.

### What an eighth review round changed: who owns a notch, and which hand is holding the button

Round 29 asked *"does some other control hold the press"*. Round 30 found that this leaves two
different states answering the same way, and that "the press" was never singular to begin with.

**1. A PRESS THAT DECLINES THE NOTCH STILL OWNS IT.** Devin `src/gui/SpectrumImager.cpp:R3302-3304`
named the sequence: a press starts, no editable parameter identifier is available, a wheel arrives,
the owning-press lookup says *no target*, and the standalone path then edits whatever the pointer is
over. Three presses reach that state — an Alt-click reset held down, a press on the display's blank
area, an add the band count refused — and each of them owns the interaction while holding nothing a
notch can be added to. The two states are not the same question:

| state | round 29 | round 30 |
| --- | --- | --- |
| no active press owns this event | the pointer decides | the pointer decides |
| an active press owns it and has **no** editable target | the pointer decides | **the press decides, and decides nothing** |

The owner ruled the second line: *"Once a mouse press owns the interaction, every wheel event arriving
before mouse-up belongs to that active press, even when that press has no editable parameter
target."* So `wheelTakenByAnyPress` — the rename records the change of question — returns `true` for
**every** event that arrives with a button down on the sending device, and the only `false` it has is
the one that says no button is down. `WheelDragOwner::takeWheelNotch`'s `false` no longer frees the
event; it now says only that this press had nothing to add it to. No caller reads it but the tests.
State test 107 legs A–H cover the six presses, the four-run velocity matrix of §8, the Settings
Persistence bar, and the value box whose own branch declines the drag — each leg carrying a positive
control, because *"nothing moved"* is the easiest assertion in the world to pass by accident.

**2. A NOTCH INSIDE A VELOCITY DRAG WAS DISCARDED, NOT MERELY DISTORTED.** Devin `src/PluginEditor.h:R822-828`
observed that `wheelDragPx` is converted through the normal drag-span model while JUCE's velocity mode
maps differently, and asked for the mapping to be established from source before any fix. It is worse
than a distortion. `Slider::Pimpl::handleVelocityDrag` is an **integrator**: it reads
`owner.valueToProportionOfLength (valueWhenLastDragged)`, adds a speed term derived from the per-event
cursor delta, and writes `valueWhenLastDragged` back (juce_Slider.cpp:815-852). `Pimpl::setValue`
never writes `valueWhenLastDragged` — the five writes are at :762, :812, :843-846, :887 and :941 —
so a notch that writes the VALUE is erased by the next drag event, which recomputes from the stale
base. Measured on Drive with the modifier held: `0.0042 -> notch -> 0.1542 -> next drag 0.0550`,
where the drag should have continued from `0.1542`. Round 29's pixel shift is worse still in that
branch: `mousePosWhenLastDragged = e.position` (:969) banks the shifted position into JUCE's own
reference, so the *next* event reads a delta that includes it and the integrator takes a spurious
one-shot kick.

No public API can write `valueWhenLastDragged`, so the notch is banked **in the integrator's own
space, as a proportion**, and injected into the one expression the integrator reads:
`valueToProportionOfLength` is a public virtual, and `Knob` arms an override for exactly one call
around one `juce::Slider::mouseDrag`. That call is provably `handleVelocityDrag`'s — nothing earlier
in `Pimpl::mouseDrag` reads a proportion (the `useDragEvents` test, the `Rotary` branch, the
`IncDecButtons` threshold and `isAbsoluteDragMode` read none) — and the injection therefore lands
*inside* JUCE's own `jlimit (0, 1, …)` two lines below, so the whole remaining range stays reachable,
it is applied exactly once, and it never passes through the velocity curve. Which branch is live is
**recorded, not predicted**: `snapValue` receives the `DragMode` JUCE chose, and `dragIsVelocity`
restates JUCE's own predicate for the one place that has to decide before the call. State test 106
leg A proves the modifier really selects a different mapping, leg B supplies the clean expectation,
leg C is the numerical reproduction (`0.0042 -> notch -> 0.1542 -> next drag 0.1558`, expected
`0.1558`, error `+0.0000`), and leg D is the boundary: climb to 0.8, wheel back to 0.0, and the
remaining travel still reaches 1.0.

**3. THE REGISTER IS ONE CELL PER POINTING DEVICE, not one per process.** Devin
`src/gui/LookAndFeel.cpp:R16` observed that the register assumes one pointer. It does, and JUCE does
not: `MouseInputSourceList` holds an **array** of sources, each `MouseInputSourceImpl` owns its own
`buttonState`, and `getCurrentModifiers()` is the global modifiers with the mouse buttons stripped and
only *that* device's buttons put back (juce_MouseInputSourceImpl.h:59-64). The reachability envelope
was established per platform rather than asserted:

| platform | second source | concurrent presses | verdict |
| --- | --- | --- | --- |
| macOS | never — `canUseTouch()` is `false` and `addSource()` refuses every index past 0 (juce_NSViewComponentPeer_mac.mm:2986-2999) | — | **unreachable** |
| Linux / BSD | `canUseMultiTouch()` is true whenever XI2 sets up (juce_XWindowSystem_linux.cpp:2299-2306; `JUCE_USE_XINPUT` defaults to 1), every window masks XI_TouchBegin/Update/End unconditionally (:676-678), and a touch dispatches as `InputSourceType::touch` with a per-finger index (:4176-4189) | yes, with **no plug-in-side opt-out** | **reachable** |
| Windows | yes — a synthesised touch or pen message is still typed from `GetMessageExtraInfo()` and `doMouseDown`'s early return is gated on `canUseMultiTouch()` (juce_Windowing_windows.cpp:2606-2613) | not established: `AudioProcessorEditor::usesWindowsMultiTouch()` returns false (juce_AudioProcessorEditor.cpp:260-263) and nothing here overrides it, so `RegisterTouchWindow` is never called and the OS synthesises one cursor | correct by construction, not needed |

So a finger landing on the display would evict the claim the mouse's own drag had made, and the
mouse's next notch would steer whatever the finger was on. The cells are keyed by `WheelPointer` —
the `(type, index)` pair that **is** the identity `getOrCreateMouseInputSource` matches on
(juce_MouseInputSourceList.h:67-89) — so a device can neither take nor clear another device's press.
**The release stays keyed on the CONTROL**, because two of the four release sites are the
lost-release safety nets (`SpectrumImager::cancelActiveDrag`, `ValueBox::abortDragGesture`), which run
from the editor's 24 Hz reconcile with no event and so no device to name; it clears *every* cell that
names the control, since one control can be under two devices at once and has one release to give.
The key is named as a value rather than read out of `e.source` inside the register precisely so the
routing can be tested: nothing public creates a second `MouseInputSource`, so State test 108 drives
the `WheelPointer` overloads directly and only `wheelPointerOf`, two lines, is left to leg G.

**4. WHAT ROUND 30 INVESTIGATED AND DISPROVED.** The Alt-click reset branch returns without calling
`juce::Slider::mouseDown`, which is the only call that clears `Pimpl::useDragEvents`, so
`Pimpl::mouseDrag` really does run its body on the next drag with the anchors of the press before
last. It writes nothing: `~ScopedDragNotification` has already set `sliderBeingDragged = -1`
(`sendDragEnd`, juce_Slider.cpp:396-399) and all three of that function's stores are behind a test on
it, and `Pimpl::mouseDown` re-seeds `valueWhenLastDragged` and `valueOnMouseDown` from the live value
on the next real press (:887-890). A guard was written for it and then **removed**: it changed no
observable behaviour, so nothing could cover it. State test 107 leg G keeps the measurement, because
the answer is JUCE's and an upgrade could take it away.

**5. THE VELOCITY TEST REALLY DOES DIVIDE BY ZERO, and State test 106 is the first thing in this
repository ever to reach it.** `Pimpl::mouseDrag`'s second disjunct is
`(normRange.end - normRange.start) / sliderRegionSize < normRange.interval`, and `sliderRegionSize`
is **0 for every rotary slider in every JUCE application**. Round 30 first "corrected" a round-29
comment that said so, on the strength of the initialiser being 1 — and the sanitizers job disproved
the correction within one push. The member is initialised to 1, but `Pimpl::resized` assigns it for
the horizontal and vertical styles only (:1266-1274, :1324), and `juce::Slider`'s own constructor
runs a layout while the style is still the default `LinearHorizontal` and the bounds are 0×0, which
writes 0; `setupRotary`'s later `setSliderStyle (RotaryVerticalDrag)` re-runs the layout, takes
neither branch, and leaves the 0 there for the object's whole life. The division is reached exactly
when the velocity-swap modifier is held — the one case in which `isAbsoluteDragMode` does not
short-circuit it — which is why no test had ever reached it before this round, and why
`-fsanitize=float-divide-by-zero` fired on State test 106's very first sanitized run. The result is
IEEE `+inf`, `inf < interval` is false, and the velocity branch is taken: JUCE's intended outcome for
an unknown region, and the one the whole of point 2 above is written against. The numerator, the
denominator and the comparison are all JUCE's, so the disposition is a one-file, one-sub-check entry
in `scripts/ubsan-ignorelist.txt`, verified in both directions the way that file requires.

One more in-source claim was corrected: the pop-up-menu click and single-click reset that a
`takeWheelNotch` comment named as the thumb-less presses are **unreachable here**: they need
`menuEnabled` and `singleClickModifiers`, and this editor sets neither.

### What a ninth review round changed: when the mapping is chosen, and who may hand a claim back

Round 31 (2026-09-16). Two confirmed defects, both of them in round 30's own work, and three
investigations that changed no production code.

**1. THE BANK IS CHOSEN WHERE IT IS SPENT, NOT WHERE IT IS FILLED** (Devin
`src/PluginEditor.h:R1045-1047`, *"modifier changes erase wheel adjustments"*). Round 30 gave the
notch two banks — pixels for the absolute mapping, a proportion for the velocity integrator — and
picked between them at the notch, from `lastDragMode`: the mapping JUCE reported for the PREVIOUS
event. That is a prediction. `Pimpl::mouseDrag` asks the NEXT event's own modifiers
(`isAbsoluteDragMode (e.mods)`, juce_Slider.cpp:928), and ctrl, alt or command can go down or come
up between the notch and that event with the mouse perfectly still. When the prediction was wrong
the contribution went into the bank the next event does not read and vanished — while the other bank
kept it, to be spent later if the user changed the modifier back. Measured on Drive: press, drag to
0.0400, notch to 0.1900, press the velocity modifier, drag — and the knob reads **0.0421**, the
integrator having resumed from the pre-notch value, an error of 0.1496 of the range. The mirror
(velocity, notch, release the modifier, drag back to the press point) read **0.0000** where the
notch's own value was 0.1500.

The fix is to fill BOTH banks at every notch and reconcile them where the mapping is known:
`wheelDragPx` is the persistent anchor shift the absolute mapping needs on every event, and
`velocityDebt` is how far JUCE's integrator base has fallen behind the live value — incurred by any
notch (`Pimpl::setValue` never writes `valueWhenLastDragged`) and discharged by whichever mapping
runs next, the injection or the absolute branch's own write. No new scale is introduced: both
numbers are the same measured proportion, one of them multiplied by the travel
`pixelsPerWholeRange()` already defined. `lastDragMode` had no other reader, so the `snapValue`
override that recorded it is gone rather than left recording something nothing reads.

**A third face of the same finding, which the matrix found and the report did not have to be told
about:** the absolute branch hands JUCE a SHIFTED position, and `mousePosWhenLastDragged = e.position`
(juce_Slider.cpp:969) stores whatever it was handed. So one absolute event after a notch banked the
whole pixel offset into the integrator's own reference, and the first velocity event then read a
`mouseDiff` of (physical travel + the offset) and bent it through the speed curve — the notch,
already applied, arriving a second time as a kick, measured at −0.0188 with the sign inverted. A
velocity event now carries the shift the PREVIOUS event carried (`lastPassedShift`), which makes the
difference JUCE reads exactly the physical travel.

**What was NOT changed, and is JUCE's own:** switching from velocity to absolute mid-drag discards
the velocity travel, because `handleAbsoluteDrag` recomputes from `valueOnMouseDown` and the cursor
offset from the press point. That happens with or without a notch and is not this ADR's to alter;
what this round guarantees is that the WHEEL's contribution survives the transition, which State
test 106 leg F measures at the press point and then drags to the top to show the range intact.

**2. A RELEASE NAMES A DEVICE** (Devin `src/gui/LookAndFeel.cpp:R77-82`, *"shared controls lose live
wheel claims"*). Round 30 keyed the claims per device and the RELEASE on the component alone,
clearing every cell that named it — on the premise that a component cannot be held by two devices at
once in a way that outlives the call. The premise is false on the platform round 30 added the cells
for: X11 dispatches a `touch` source per finger alongside the live `mouse` source
(juce_XWindowSystem_linux.cpp:4176-4189), so the first `mouseUp` disowned every other device still
holding that control, and the survivor's notches fell through to whatever the pointer was over.
`releaseDragWheel` now takes the releasing `WheelPointer` and clears that cell only, still subject to
the older half of the rule (the component must be the one that claimed it). The event-less safety
nets keep a broad clear under a separate name, `releaseAllDragWheelClaims`, because they say
something different — *this control has abandoned its gesture*, which is true for every device, since
a control holds exactly one anchor. The separate name is the point: an ordinary release cannot reach
the broad path by forgetting an argument.

**3. THE PROCESS-WIDE TABLE ACROSS INSTANCES: reachable, and correctly isolated** (Devin
`src/gui/LookAndFeel.cpp:36`). Two Anamorph instances in one host share this static — one process,
one copy — and share `juce::Desktop`'s source list with it, so a device has the same identity in
both. That is what makes the sharing right rather than dangerous: the key is the DEVICE, and a device
has one press at a time. Its button state lives in its own `MouseInputSourceImpl` and JUCE routes
every event during a drag to the component that press captured, so the same device cannot begin a
second press in another editor before the first ends; if it somehow did, the later claim would
replace the earlier, which is the live press. An editor destroyed mid-press leaves a `SafePointer`
that reads back null. No production change; State test 108 legs F and K.

**4. THE DEVICE KEYS ARE BOUNDED, so the table cannot grow without bound** (Devin
`src/gui/LookAndFeel.cpp:36`, the registry-lifetime question). Cells are emptied and never erased, so
the question is the key space, and JUCE fixes it: `getOrCreateMouseInputSource` keeps exactly one
`mouse` and one `pen` source matched on TYPE alone — replugging a mouse or adding a second mints no
new key — and touch sources are matched on (type, finger slot) under JUCE's own
`jassert (0 <= touchIndex && touchIndex < 100)` (juce_MouseInputSourceList.h:67-89). The slot is
RECYCLED: `MultiTouchMapper` hands out the lowest free index and `clearTouch` frees it at TouchEnd
(juce_MultiTouchMapper.h:46-62). The ceiling is therefore 102 rows in a process that cannot exist and
two or three in one that can. Not a leak; no lifecycle mechanism added, and no mutation manufactured
for it.

**5. THE `float-divide-by-zero` DISPOSITION WAS CORRECT AND HAD NO EFFECT, because ccache did not
know the ignorelist is an input** (Devin `src/PluginEditor.h:R909`). The section round 30 added is
right — verified again this round on the current head and the current tests, in both directions: with
it the UBSan build of the state suite reports zero runtime errors, without it exactly one, at
`juce_Slider.cpp:929`. The `sanitizers` job nevertheless failed on that same check, because ccache
4.9.1 has special handling only for the older `-fsanitize-blacklist=` spelling and hashes
`-fsanitize-ignorelist=` as a plain argument string: same path, edited content, cache HIT, and the
objects were the ones compiled under the previous list. Reproduced directly with ccache 4.9.1. The
fix is `CCACHE_EXTRAFILES` in that job — ccache's own mechanism for an input that affects the output
without appearing in the preprocessed source — and not a widening of the suppression. State test 106
leg A now also asserts, next to the behaviour that depends on it, that a rotary still reports a
linear region of zero, so a future JUCE that gives it one fails there rather than silently flipping
coarse-interval knobs into absolute mode.

### What a tenth review round changed: one component, one drag

Round 32 (2026-09-16). Devin `src/gui/LookAndFeel.cpp:R106-110`, *"first release strands second
press"*, and it is round 31's own fix seen from the other end.

**THE MISMATCH.** The register can key a claim per device. A COMPONENT cannot key a drag per device:
`juce::Slider::Pimpl` holds one `valueOnMouseDown`, one `mouseDragStartPos` and one
`sliderBeingDragged` that `sendDragEnd` puts back to -1 on the FIRST release (juce_Slider.cpp:396-399);
the multiband display holds one `dragBand`/`bandAnchorX`/`gestureBands`; the value box holds one
`downProp`. Round 31 keyed the RELEASE per device so that one device's `mouseUp` would stop disowning
another's claim — and that left a claim alive past the release that ended the one drag the component
had. The immediate consequence is inert (`takeWheelNotch` refuses while `getThumbBeingDragged()` is
negative, so the notch merely vanishes); the reachable consequence is not. Press the control AGAIN
and the stale claim delivers into the new drag: a device that is not dragging the control steers it.

**THE OWNER'S RULE, implemented.** One component supports one active component drag at a time.
The first accepted press establishes the component's drag and its wheel ownership; a second device
pressing the same component establishes no second claim; and no claim survives the end of that
component's shared drag. So the fix is on the CLAIM side — `claimDragWheel` returns without claiming
when another cell already names the component — and the release goes back to clearing every cell that
names the component, which is now the *statement* of the invariant rather than a scan hoping to find
one. `releaseAllDragWheelClaims` is gone with it: there is nothing for the event paths and the two
event-less safety nets to disagree about, because both say the same thing about the same control.

**WHAT DID NOT CHANGE.** An accepted active press still owns the wheel until mouse-up wherever the
pointer goes (State tests 105, 107 legs A-H and N). A held button with nothing claimed still CONSUMES
the notch and moves nothing (round 30) — which is why these legs count deliveries and parameters
rather than consumption: both the defect and the fix consume. And no per-device drag anchor, no
per-device slider state and no multi-drag architecture was introduced; the register was brought into
line with JUCE's model, not the other way round.

**ONE LINE THE MUTATION SUITE HAD TO EARN.** `claimDragWheel` also clears the claiming device's own
previous cell. That is redundant whenever the new claim is granted — the same cell is overwritten a
line later — so M186 survived until a leg reached the one path where it is not: a claim that is
REFUSED, which can only happen to a device that already had a cell, which can only happen after a
release that never arrived (KI-028's class). State test 108 leg L is that path.

### What round 32 also fixed outside the wheel

Devin `src/StateCommandGate.h:R163-172`, *"unwired preset commands never run"*. A `PresetManager`
with no processor hands the gate a default-built `StateCommandHooks`; the gate read the null
`soundReplacement` as a failed try-lock and fell through to `defer`, whose `enqueue` is also empty.
`saveUser` answered `OpResult::deferred`, nothing was written and no completion was called — the
command was not queued, it was dropped, and both the manager's own `stateCommand` declaration and
`stateCommandAdmission`'s *"no processor: admit everything"* comment had said otherwise since round
28. A null replacement lock is now an **unguarded successful admission**: there is no lock to wait
on, so there is no cycle to break and no queue to join. The configured path is untouched, and a
`jassert` in `defer` now refuses to be silent about a half-wired set. State test 109.

### What an eleventh review round changed: the refusal reaches the caller, and it lasts the whole press

Round 33 (2026-09-17). Devin `src/gui/LookAndFeel.cpp:R101-103`, *"rejected second press steals
drag"*.

**WHAT ROUND 32 LEFT HALF-DONE.** `claimDragWheel` refused the second device's claim — and returned
`void`. The refusal was invisible to the caller, so the rejected press ran the rest of its
`mouseDown` regardless, and that is where a component's one shared drag is actually established:
`juce::Slider::Pimpl::mouseDown` re-seeds `mouseDragStartPos` and `mousePosWhenLastDragged`
(juce_Slider.cpp:856), resets the owner's `ScopedDragNotification` (:857), re-reads
`sliderBeingDragged` (:878) and `valueOnMouseDown` (:887-889) and opens a second host gesture (:899);
`SpectrumImager::mouseDown` re-latches `gestureBands`, the gesture sound, `dragBand`, `bandAnchorX`,
`soloPressBand` and the pending delete; `ValueBox::mouseDown` overwrote `downProp` and its own
`ScopedDragNotification` **one line above** the claim it had not yet asked for. The first device then
went on dragging from the second device's anchor, with its own change gesture already closed.

**THE ANSWER IS RETURNED.** `claimDragWheel` is now `[[nodiscard]] bool` — TRUE when this press now
owns the component's drag, FALSE when another device already does — and every caller asks before it
writes anything of its own. `[[nodiscard]]` is the enforcement: a caller that forgets to ask does not
compile. The device that already holds the component is granted it again, because `claimDragWheel`
clears that device's own cell before scanning, so a duplicate or re-entrant `mouseDown` from the
owner is not treated as its own rival.

**AND A PRESS IS NOT ONLY ITS `mouseDown`.** Proving the above from source showed the same rejected
press still reaching the owner's state through its other two events, which the owner's rule forbids
in the same words. `Pimpl::mouseDrag` never asks whose press it is: it runs on the OWNER's
`useDragEvents` and `sliderBeingDragged` and writes the parameter from whatever cursor it is handed
(juce_Slider.cpp:906-970). `Pimpl::mouseUp` ends with an unconditional `currentDrag.reset()` (:997),
so a refused release closed the owner's host gesture and put `sliderBeingDragged` back to -1 —
and `Knob::mouseUp` then handed back the owner's claim. `SpectrumImager::mouseUp` is worse again: it
fires the ON-RELEASE ACTIONS the owner's press latched, a solo toggle or a band removal. Both
double-click handlers write a parameter outright, and `juce::Label::mouseDoubleClick` opens the
value box's inline editor, whose `isBeingEdited()` is exactly what the owner's own `mouseDrag` tests
before it writes.

So the register gained a read-only twin, `dragWheelHeldByOther`, and every event of a press asks it:
`mouseDrag`, `mouseUp` and `mouseDoubleClick` in all three drag implementations, plus the `Label`
forward in `ValueBox::mouseDown`. It is deliberately **"someone else holds it"** and not **"I hold
it"**: the two differ exactly where the lost-release safety nets live, and a cell emptied while a
button is still down (KI-028's self-heal, a destroyed holder read back through the `SafePointer`)
must still let the component's own release through.

**WHY IT IS ASKED AND NOT REMEMBERED.** A flag set by the refused `mouseDown` would be per
(component, device) state — the thing the owner's decision forbids — and would need clearing on
paths that do not always run. The register already holds the answer, and asking it costs a scan of
two or three elements on the message thread.

**WHAT DID NOT CHANGE.** No per-device drag anchor, no per-device slider state, no multi-drag
architecture. The single-device path is byte-identical (State test 110 leg F). An accepted active
press still owns the wheel wherever the pointer goes, and a held button with nothing claimed still
consumes the notch and moves nothing. The register is still message-thread only, still keyed on the
`WheelPointer` value, still process-global and still bounded by JUCE's own device-identity model:
`dragWheelHeldByOther` adds no cell and reads the holder through the same `SafePointer`, so a
destroyed holder is no device's rival.

**TWO LINES THE MUTATION SUITE HAD TO EARN**, as round 32's did. M194 (drop the knob's `mouseDrag`
guard) and M190 (make the display's `mouseDown` ignore the refusal) both SURVIVED their first run,
and neither was equivalent — the suite was measuring the wrong thing. Leg C compared only the
owner's continuation, which a rival write can land back on through the velocity integrator, so it
now also requires the rival's own three events to have written **exactly zero**; and a re-latched
`dragHandle`/`dragBand`/`gestureBands` on the display is invisible until the OWNER drags again, so
leg B4 now relabels the register back and requires that drag to move the split it was dragging and
no other. A third, M189, is genuinely EQUIVALENT and says so in source: the guard above
`ValueBox::mouseDown`'s condition has already established what `claimDragWheel`'s answer there
would say.

**HOW IT IS MEASURED.** State test 110 drives the real editor and measures real drag state and real
parameters, never "was the event consumed?" — which cannot discriminate these bugs, because round
30's approved rule consumes either way. Leg C is the primary regression: the identical
press-drag-drag-release run twice, the second time with a rival's whole press spliced into the
middle, and the two parameters must be **bit-for-bit equal**, in both drag mappings (the rotary
knob's velocity integrator and the `LinearHorizontal` knob's absolute anchor), plus the same
comparison taken to the end of the travel. Against the unfixed release path it read 5.26 vs 0.00 and
226.665314 vs 437.469788 — the absolute-mapping knob literally jumping to the rival's x — with
`sliderBeingDragged` 0 → -1.


## Consequences

- **A notch during any drag now adds to it**, and the drag continues from the combined value. What a
  user notices: the press no longer dies, and Undo steps back to before the press rather than to the
  notch. This is the reversal of ADR-0041's Consequences line, and the line is superseded, not deleted.
- **A pending Band Solo click that a notch turns into a band move is still not a toggle** — the same
  outcome ADR-0041 recorded, for a different reason: a press that became a drag was never a click.
- **A notch with no band to move is not an edit** (ADR-0052, applied to this ADR's own new branch). At
  one band `beginBandMove` leaves both pins at `-1` and `moveBand` returns at `M <= 0` having written
  and opened nothing; converting the press regardless would swallow the solo click for no gain. Found
  by this round's own adversarial pass, not by a later review.
- **A pending DELETE click is inert to the wheel.** It holds no parameter and no anchor, so there is
  nothing to add a notch to — and under this decision there is nothing to cancel either. The click
  survives and fires on release.
- **Multiband wheel edits are undoable for the first time.** Each notch opens and closes a change
  gesture, which is also what the host sees. This closes KI-010's second path.
- **ONE gesture, up to three parameters, on the multiband split path** — stated rather than implied.
  `dragCrossoverTo` can push neighbouring splits aside and those stores are outside any bracket of
  their own, so a host recording touch/latch sees them as automation. Undo is unaffected
  (`openGestures` is one global count, so the whole burst lands in the single step the bracket
  commits), and this is exactly what the DRAG path has done since 0.6.x. Matching it is deliberate; a
  gesture per pushed neighbour would change how this plug-in reports automation, which is not what
  this round was asked for.
- **A batch is only a scroll's if every gesture in it named the same control.** The poll runs on the
  editor's 24 Hz tick, so two gestures can finish inside one period and collapse into one step — as
  they always have. Taking the last gesture's name would attribute the pair to the scroll and fold a
  released drag into it, leaving the drag no undo point of its own. A disagreeing name inside one
  pending batch therefore clears the name. Also found by this round's adversarial pass. **An EMPTY
  gesture is not one of the batch's**: it changed no sound parameter, so it contributes no name and
  cannot disagree with one, and a click that starts no drag therefore leaves a scroll's chain intact.
- **A typed value force-committed by a notch is attributed to the scroll.** JUCE's wheel handler calls
  `valueBox->hideEditor (false)` before its own gesture, and that commit opens a gesture of its own
  inside the naming scope. Both belong to one user action (type, then scroll), so merging them is the
  answer this ADR intends; it is recorded here rather than discovered later.
- **The Settings Persistence bar gains the interaction and keeps its exclusion from Undo.** It is
  bound to host-hidden `InternalState` by `juce::Value`, not to an APVTS parameter, so it opens no
  change gesture, contributes nothing to the sound signature and names nothing — a pointed notch that
  lands on it during another control's press included. The exclusion is structural, and State test 86
  leg F and State test 88 legs E and G are what would notice if it stopped being.
- **A notch delivered anywhere while a press is held belongs to THAT PRESS — reversed 2026-09-16
  (round 29), by owner instruction.** This bullet used to read *"A notch delivered to a control other
  than the one being dragged edits THAT control"*, and it was true of the implementation it
  described: JUCE routes a wheel event to whatever is under the POINTER, never to whatever captured
  the press, and round 14 made knobs, sliders and value boxes act on such a notch instead of dropping
  it. The owner ruled the other way — the press decides, the pointer does not retarget, and no other
  control may change while the button is held — so the line is **superseded, not deleted**, in the
  form this ADR uses for ADR-0041's. The reasoning the old line gave for not doing it was that *"it
  would need a cross-component registry JUCE does not provide"*: that is accurate, and the registry
  is now `claimDragWheel` / `releaseDragWheel` / `wheelTakenByAnyPress` in `LookAndFeel.cpp` —
  one message-thread `SafePointer`, asked at the top of every wheel handler and, for everything that
  owns no wheel handler at all, at the editor. The second half of the old line's objection — that a
  mouse-button gate *"would silence a notch during one of the multiband display's own presses that
  latched no identifier"* — was answered in round 29 by asking the REGISTER rather than the button, so
  that a notch during an identifier-less press still reached the control it pointed at. **Round 30
  reversed that half too**, by owner instruction: silencing such a notch is now the requirement, not
  the objection. See the eighth round below.
  **A control whose own child holds the press stops being an exception**: the value box claims for
  itself at its own `mouseDown`, so the knob no longer has to know about its child.
- **A frozen split drag owns its notches and adds nothing to them** (round 29). Carried far enough
  outside the frame to arm the merge-on-release affordance, a split drag deliberately writes nothing
  until the cursor returns; the notch is the press's, and the press has nothing to add it to. The
  same answer a pending DELETE click gets, one branch above.
- **A burst that wrote nothing of its own leaves NO undo step — corrected 2026-09-13.** This bullet
  used to say the opposite, and it was true of the implementation it described: the gesture closed over
  a changed signature, so the poll recorded the host's write as a step of its own, and closing that
  "would mean a gesture able to withdraw its own commit request". Under ADR-0008 as amended the poll
  asks what the BATCH owns rather than what the signature did, so a burst whose own store was refused
  records nothing and the host's write beside it is undoable by nobody. State test 86 legs I, J and V
  are re-based onto that; each of them previously asserted the old behaviour.
- **A velocity drag of a ROTARY knob divides by zero inside JUCE, and this decision's coverage
  works around it rather than silencing it.** `Slider::Pimpl` assigns `sliderRegionSize` only for
  horizontal and vertical styles and the constructor takes that branch once under JUCE's default
  style against empty bounds, so every rotary slider keeps `sliderRegionSize == 0` -- measured from
  outside as `getPositionOfValue(max) - getPositionOfValue(min)`: 0.000 for Drive, 246.000 for the
  mono-maker slider. The selection test `(range) / sliderRegionSize < interval` therefore divides by
  zero on a knob; `+inf` is not less than the interval, so the velocity branch is taken as intended
  and nothing behaves wrongly, but UBSan reports it. State test 88 leg H drives the same JUCE branch
  through a linear slider instead. Nothing is added to `scripts/ubsan-ignorelist.txt`, which states
  that `float-divide-by-zero` still instruments the vendored tree in full.
- **One extra comparison per drag event, and only after a notch — and ONE write, not two
  (corrected 2026-09-13; the site moved 2026-09-16).** The fold was `Knob::snapValue` from round 14
  to round 28. Round 29 moved it into `Knob::mouseDrag`, where the offset shifts the EVENT before
  JUCE maps and clamps it, because `snapValue` runs AFTER `jlimit` and therefore folded the notch
  into an already-saturated position — the boundary defect above. The one-write property is
  unchanged and is now structural rather than arranged: the shift happens before JUCE computes
  anything, so there is still exactly one `setValue` per drag event, and a press with no notch in it
  takes an `isOrigin()` early return and makes the writes it always did.

## Related code

* `src/PluginEditor.h` — `Knob::wheelDragPx`, `Knob::owner`, `dragIsRelativeToPress`,
  `pixelsPerWholeRange`, `wheelDragShift`, `mouseDrag` (the fold, round 29 — it replaced the
  round-14 `snapValue` override, which had replaced `applyWheelDragOffset` and an earlier `mouseDrag`
  override), `mouseDown` / `mouseUp` (claim and release), `takeWheelNotch`, `mouseDoubleClick`,
  `mouseWheelMove`, `mouseWheelMoveTail`, `sendWheelToJuce` (a button held over something that owns
  no press).
* `src/PluginEditor.cpp` — `attachSlider` seeds `Knob::owner`; the editor wires
  `SpectrumImager::onWheelStep`; `AnamorphAudioProcessorEditor::mouseWheelMove` is the register's
  backstop for everything that overrides no wheel handler of its own.
* `src/gui/LookAndFeel.cpp` — the wheel register (`claimDragWheel`, `releaseDragWheel`,
  `dragWheelHolder`, `wheelTakenByAnyPress`, `WheelClaim`); `ValueBox::mouseDown` / `abortDragGesture` /
  `mouseWheelMove` / `takeWheelNotch`.
* `src/gui/LookAndFeel.h` — `wheelDominantDelta` (the one spelling of the axis rule),
  `wheelTargetValue`, `WheelDragOwner::takeWheelNotch`, `DragGestureOwner`, `WheelPointer` /
  `wheelPointerOf` (round 30).
* `src/gui/SpectrumImager.cpp` — `mouseWheelMove` (the register, the in-press half and the standalone
  half), `takeWheelNotch` (the press branches, including the frozen-split rule),
  `standaloneWheel` (the named, bracketed burst), `mouseDown` / `mouseUp` / `cancelActiveDrag`
  (claim and release), `kWheelSplitPx` / `kWheelWidthMin` / `kWheelWidthPer`, `ScopedWheelName`.
* `src/gui/SpectrumImager.h` — `onWheelStep`, `takeWheelNotch`, `standaloneWheel`.
* `src/PluginProcessor.h` — `setWheelStepKey`, `wheelStepKeyFor`, `ScopedWheelStep`,
  `wheelStepKey` / `pendingStepWheelKey` / `lastStepWheelKey`, `pendingStepNamed` / `gestureOpenGen`.
* `src/PluginProcessor.cpp` — `parameterGestureChanged` (the name travels with the commit request),
  `pollUndoCoalesceAdopted` (the extend rule), and the four places that end a chain.

## Evidence + confidence

**Verified.** State test 80 (inverted, and its header says so), State tests 86, 87 and 88;
3 110 checks / 0 failures, DSP 396 / 0; twenty-five mutations applied one at a time across the two
rounds, twenty-three killed, with M15 and M18 recorded as surviving — each behind a proof no
single-threaded harness can enter, and each stated as unmeasured rather than as covered. The three
corrections in the second round were each reproduced as failing checks before the code was touched.
The mutation record is in `docs/procedures/TESTING.md` and in
`worklogs/SPECTRUMIMAGER_TOPOLOGY_TRANSACTION_AUDIT_v0.9.8.md` §66 and §67.
