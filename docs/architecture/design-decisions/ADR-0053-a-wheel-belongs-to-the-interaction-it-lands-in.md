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
- **A notch delivered to a control other than the one being dragged edits THAT control.** JUCE routes a
  wheel event to whatever is under the POINTER, not to whatever captured the press. The multiband
  display always behaved this way; knobs, sliders and value boxes dropped such a notch silently, and now
  do not. The edit lands inside the open gesture, so the press and the foreign notch share one undo
  step. **A control whose own child holds the press is the exception, and it is not a special case
  so much as the same rule one level down**: the knob asks its children first, and the value box
  takes the notch into the anchor its drag is steering rather than letting it be written behind that
  anchor's back and erased by the next drag event. Gating on the mouse button instead — making a notch reach only the control being dragged —
  was rejected: it would need a cross-component registry JUCE does not provide, and it would also
  silence a notch during one of the multiband display's own presses that latched no identifier.
- **A burst that wrote nothing of its own still leaves an undo step for what the host wrote inside
  it.** Unnaming it stops the misattribution — the previous scroll keeps the value it ended on — but the
  gesture did close over a changed signature, so the poll records that change as a step of its own. This
  is the generic property of gesture coalescing (any host write bracketed by any gesture joins that
  gesture's step), not something this decision introduces, and closing it would mean a gesture able to
  withdraw its own commit request. Recorded rather than claimed fixed.
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
- **Two lines of extra work per drag event, and only after a notch.** `applyWheelDragOffset` returns
  on a zero offset, so a press with no notch in it makes exactly the parameter writes it always did.

## Related code

* `src/PluginEditor.h` — `Knob::wheelDragProp`, `Knob::owner`, `applyWheelDragOffset`,
  `mouseDrag`, `mouseUp`, `mouseWheelMove`, `sendWheelToJuce` (the pointed-control delivery).
* `src/PluginEditor.cpp` — `attachSlider` seeds `Knob::owner`; the editor wires
  `SpectrumImager::onWheelStep`.
* `src/gui/LookAndFeel.cpp` — `ValueBox::mouseWheelMove`, `ValueBox::takeWheelNotch`.
* `src/gui/LookAndFeel.h` — `DragGestureOwner::takeWheelNotch`.
* `src/gui/SpectrumImager.cpp` — `mouseWheelMove` (the press branches and the named, bracketed
  standalone burst), `kWheelSplitPx` / `kWheelWidthMin` / `kWheelWidthPer`, `ScopedWheelName`.
* `src/gui/SpectrumImager.h` — `onWheelStep`.
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
