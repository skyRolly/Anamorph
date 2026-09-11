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
  pending batch therefore clears the name. Also found by this round's adversarial pass.
- **A typed value force-committed by a notch is attributed to the scroll.** JUCE's wheel handler calls
  `valueBox->hideEditor (false)` before its own gesture, and that commit opens a gesture of its own
  inside the naming scope. Both belong to one user action (type, then scroll), so merging them is the
  answer this ADR intends; it is recorded here rather than discovered later.
- **The Settings Persistence bar gains the interaction and keeps its exclusion from Undo.** It is
  bound to host-hidden `InternalState` by `juce::Value`, not to an APVTS parameter, so it opens no
  change gesture, contributes nothing to the sound signature and names nothing. The exclusion is
  structural, and State test 86 leg F and State test 88 leg C are what would notice if it stopped
  being.
- **A notch delivered to a component other than the one being dragged still edits that component.**
  JUCE routes a wheel event to whatever is under the POINTER, not to whatever captured the press, so a
  knob drag whose cursor has travelled over the multiband display delivers its notches there. That is
  unchanged behaviour and is left unchanged deliberately; gating the multiband display on the mouse
  button would also silence a notch during one of its own presses that latched no identifier.
- **Two lines of extra work per drag event, and only after a notch.** `applyWheelDragOffset` returns
  on a zero offset, so a press with no notch in it makes exactly the parameter writes it always did.

## Related code

* `src/PluginEditor.h` — `Knob::wheelDragProp`, `Knob::owner`, `applyWheelDragOffset`,
  `mouseDrag`, `mouseUp`, `mouseWheelMove`.
* `src/PluginEditor.cpp` — `attachSlider` seeds `Knob::owner`; the editor wires
  `SpectrumImager::onWheelStep`.
* `src/gui/LookAndFeel.cpp` — `ValueBox::mouseWheelMove`.
* `src/gui/SpectrumImager.cpp` — `mouseWheelMove` (the press branches and the named, bracketed
  standalone burst), `kWheelSplitPx` / `kWheelWidthMin` / `kWheelWidthPer`, `ScopedWheelName`.
* `src/gui/SpectrumImager.h` — `onWheelStep`.
* `src/PluginProcessor.h` — `setWheelStepKey`, `wheelStepKeyFor`, `ScopedWheelStep`,
  `wheelStepKey` / `pendingStepWheelKey` / `lastStepWheelKey`.
* `src/PluginProcessor.cpp` — `parameterGestureChanged` (the name travels with the commit request),
  `pollUndoCoalesceAdopted` (the extend rule), and the four places that end a chain.

## Evidence + confidence

**Verified.** State test 80 (inverted, and its header says so), State tests 86, 87 and 88;
3 012 checks / 0 failures, DSP 396 / 0; sixteen mutations applied one at a time, fifteen killed and
M15 recorded as surviving behind the press branches' own staleness gate. The mutation record is in
`docs/procedures/TESTING.md` and in
`worklogs/SPECTRUMIMAGER_TOPOLOGY_TRANSACTION_AUDIT_v0.9.8.md` §66.
