# ADR-0008 — Custom per-A/B-slot Undo/Redo (replaces JUCE UndoManager)

**Status:** Accepted

## Context
A/B compare keeps two independent sound states. Undo must not mix the two histories, must not
record A/B switches or view/Settings changes, and must fold a knob gesture into one step.

## Problem
JUCE's global `UndoManager` records everything globally — it cannot express per-slot histories
or exclude view params, and would treat an A/B switch as undoable.

## Options
- **A. JUCE global UndoManager.** Rejected — wrong granularity, global history.
- **B. Custom per-A/B-slot undo over `StateSet` snapshots, gesture-coalesced.** Chosen.

## Decision
Each A/B slot owns its own undo/redo stacks of `StateSet` snapshots (sound parameters + preset
name + dirty baseline + — since 0.9.2 — the preset **identity**, i.e. which factory id or user
file produced the sound; ADR-0024). The editor's timer calls `pollUndoCoalesce()`, which is **gesture-gated**:
the processor listens to parameter begin/end gestures and commits exactly one step after the last
gesture closes, folding a whole drag into a single entry; **host automation** (which opens no
gesture) folds into the baseline **without** a step. A **preset load** also opens no gesture, so it
is bracketed explicitly (`PresetManager::onAboutToLoad` flushes a settled edit, `onLoaded` records
one step via `commitPresetSwitchUndoStep`) — a preset switch is a discrete, undoable action.
**View params** (`pid::viewParams` = Bypass; plus Settings, which are host-hidden) and A/B switches
themselves are never recorded. `requestDuck()` masks the level jump on undo/redo. Undo stacks are
cleared on session restore.

## Decision — amendment, 2026-09-13 (round 14; maintainer-approved)

**What is amended.** The first clause of the Decision above — *"Each A/B slot owns its own undo/redo
stacks of `StateSet` snapshots"* — and the Consequence it produced, which RISK-012 recorded and the
"OPEN — pending a maintainer decision" note above raised. Everything else in the Decision stands
unchanged: the per-slot histories, the gesture gating, the `pollUndoCoalesce` commit, the preset
bracket, the view-param and A/B-switch exclusions, `requestDuck()`, the 128-entry cap, and the fact
that undo history is never serialized and is cleared on session restore.

**The rule.**

> A user Undo/Redo step is the set of parameters THAT USER ACTION MOVED, together with the value
> each of them held immediately before the action and the value each of them held immediately
> after it. Undo writes the before-values; Redo writes the after-values; a parameter the action did
> not move is not in the step and is not written by either. A later host automation write therefore
> cannot redefine either endpoint of a step that already exists, and cannot be taken back by
> undoing one.

Read as the sequence the review states:

```text
parameter starts at A
user edit          A -> B     one step is created: { this parameter: before A, after B }
host automation    B -> C     no step; folds into the baseline (unchanged from the original Decision)
Undo                     -> A  the step's before-value
Redo                     -> B  the step's after-value, NOT the automated C
```

**The answers, one by one.**

* **What a step represents.** A single user action — one gesture batch as the coalescer already
  defines it: everything between the first change gesture opening and the last one closing, plus a
  wheel chain's continuation of it.
* **Before-state and after-state.** The values the step's own parameters held at those two edges.
  They are read at the edges, not at commit time and not at Undo time, so nothing that happens
  afterwards can move them.
* **Later host automation.** It is outside the step. It creates no entry (that clause of the
  original Decision is unchanged and is now enforced rather than merely intended), it does not
  become the Redo destination, and it does not redefine the Undo baseline.
* **Is host automation excluded from plug-in Undo history?** Yes, completely. Before this
  amendment it was excluded only when it fell outside every pending step's window; inside one it
  was carried by the whole-state snapshot, which is the defect.
* **How Redo stays tied to the user's post-edit state.** The entry records the after-values and the
  SAME entry moves between the undo and redo stacks. Redo no longer manufactures its destination
  from the live parameters at the moment Undo was pressed, which is what let automation between the
  step and the Undo become the Redo target.
* **Automation before an Undo.** Preserved. The Undo writes only the step's own parameters; a host
  value on any other parameter stands. On a parameter the step itself owns, Undo restores that
  step's before-value, because that is what "restore the value from immediately before the user's
  action" means.
* **Automation between an Undo and a Redo.** Same answer, symmetrically: Redo writes only the
  step's own parameters at their after-values, and leaves everything else where the host put it.
* **What Redo restores.** The after-values of the step's own parameters, plus the preset
  name / dirty baseline / identity recorded with that end of the step.
* **Why a new representation is necessary.** A whole-state snapshot cannot express "this parameter
  goes back, that one stays" — every value in it is restored or none is. Attribution cannot be
  bolted onto it either: the entry pushed beside a user edit necessarily predates any write that
  arrived in the commit window, so one Undo reverts that write whatever the attribution says. This
  is the point ADR-0053's round-12 and round-13 sections reached from two directions and recorded as
  having no third answer while an entry was a whole state.
* **Why this is the minimum.** The declaration already exists: a change gesture on a parameter is
  the plug-in saying "the user is editing this". The amendment reads the index JUCE already passes
  to `parameterGestureChanged` and takes two bounded snapshots of the parameter values at the batch
  edges; nothing else is added. `StateSet` is untouched, so A/B slots, the committed baseline and
  every serialized field keep their meaning and the Serialization Registry is unaffected. There is
  no new thread, no new cross-thread path, no new atomic and no new ordering: `parameterGestureChanged`
  is message-thread-only (JUCE dispatches it only from `begin/endChangeGesture`, which no host calls
  on a plug-in's parameters and which this plug-in calls only from GUI code), which is precisely why
  this is NOT the `parameterValueChanged` design RISK-012 flagged as gated on the Thread Model.
* **The two entries that stay whole.** A preset load and an A/B Copy both replace a sound wholesale
  with no gesture anywhere, so there is no per-parameter attribution to be had and none is invented.
  `before.params` being valid is what marks such an entry, and both directions of it were already
  whole states; they behave exactly as they always have.
* **The one store family that declares itself another way.** The multiband display writes
  neighbouring splits and shifted widths inside a gesture held on one parameter
  (`SpectrumImager::storeOwned` / `setParam`). Those writes call `noteOwnedParamWrite`, so the step
  owns them; without it one Undo would restore the split the user scrolled and leave the neighbour
  it pushed, which is a layout neither the user nor the previous state produced. Opening a gesture
  per pushed neighbour was rejected again for the reason ADR-0053 gives: it would change how this
  plug-in reports automation.

## Decision — correction, 2026-09-13 (round 15)

**A declaration carries the value the store installed, and only a store that stood makes one.**
The round-14 amendment above reads a declared parameter's ending value out of `batchCloseValue`,
the whole-parameter-list snapshot retaken at every zero-crossing gesture close. That is the right
instant for a coupled store made *inside* a bracket, and the wrong one for a coupled store made
after the last of them — which the reset paths are. `SpectrumImager::resetCrossover` and
`commitFreqEditor` close the primary split's gesture and only then call `spreadSplits`
(ADR-0043 put the PLAN inside the bracket; the spread has always been outside it, so that the
pin is proved committed before anything is moved to make room for it). Every neighbour the spread
pushes is therefore stored while nothing is open and after the snapshot that was supposed to read
it back: declared owned, but with `before == after`, which the poll's own move test drops. Measured
on a packed row, an Alt-click reset moving split 0 from 50 Hz back to its 180 Hz default and pushing
its two neighbours from 200/280 Hz to 249.8/366.6: one Undo restored 50 Hz and left the neighbours
at 249.8/366.6 — a split row no user action produced. The typed-value commit has the identical
shape and the identical failure.

So `noteOwnedParamWrite` takes the value with the declaration and writes that one slot of
`batchCloseValue`; a later gesture close simply retakes the whole snapshot over it, leaving every
store made inside a bracket exactly as it was. And the declaration moves to *after* `storeOwned`'s
read-back proof, so a store an authoritative write refused claims nothing — previously such a
parameter was owned while holding somebody else's value, which was harmless only for as long as
nothing read the ending value back. It is read back now.

**What was rejected, and why.** Holding the primary's gesture open across `spreadSplits` would also
put the neighbours inside a bracket, but it changes the touch span this plug-in reports for a
reset, does nothing for the `setParam` stores in `addBandAt` / `removeBand`, and leaves the refused
store owned — with the close snapshot then absorbing the foreign value, making the mis-attribution
worse rather than better. The chosen correction is three lines and fixes both halves.

**The touch span is unchanged, and that was checked rather than assumed.** A coupled neighbour has
never had a change gesture of its own on any path: `writeCrossovers` writes the drag's neighbours
with a gesture open on the DRAGGED split only, and a host correlates touch per parameter. The reset
differs only in that no gesture is open on any split while the spread runs, which is not a
difference the neighbour's own automation lane can see.

**What this does NOT change.** Host automation still folds into the baseline without a step. A
preset switch is still one undoable step. `soundSignature()` still drives coalescing and still asks
the RENDERED value, and the amendment asks the same rendered grid when deciding whether a step's
own parameter moved — so a sub-step write on a discrete parameter still records nothing, exactly as
before (ADR-0036 §17).

**The residual this leaves, stated rather than implied.** A host write that lands between one of the
batch's own gestures opening and that same gesture closing is inside the batch by every test the
coalescer has — the pre-existing window ADR-0053 already records — and if it lands on a parameter the
batch owns, that parameter's after-value is the host's. It cannot reach a parameter the batch does
not own, which is the case RISK-012 was about, and it cannot make automation undoable on its own.

> **SUPERSEDED 2026-09-14 (round 17), and it was wrong in both directions.** It was too GENEROUS:
> the paragraph describes a window one gesture wide, and the implementation's window was the whole
> pending batch, because the closing snapshot re-read every parameter rather than the ones the
> closing gesture was about. It was also too WEAK as a statement of intent: an automation value
> becoming a user step's endpoint is not a residual this decision accepts, it is a violation of the
> decision's own sentence. Both are corrected below; what genuinely remains is recorded there as an
> implementation residual with its exact scope, and it no longer includes any parameter a store
> declares.

## Decision — correction, 2026-09-14 (round 16)

**A store declares itself only if it stood, on EVERY unbracketed path — and there is now one store
that does it.** Round 15 gave `SpectrumImager::storeOwned` that rule and left
`SpectrumImager::setParam` with a declaration of its own: convert, store, declare, unconditionally.
The declared VALUE was never the problem — round 15 passes the value the store asked for,
deliberately, so that a spread running after its gesture has closed still has an ending value — but
the OWNERSHIP FLAG is, because `batchCloseValue` is retaken IN FULL at the next zero-crossing close,
and `setBands` ends every add and every remove. So a host answering a topology store's own dispatch
left the parameter holding somebody else's value, declared the user's anyway, and the close snapshot
then read that value back as the user's endpoint. `addBandAt` and `removeBand` cannot catch it
either: their loops prove slot *i* BEFORE storing slot *i* and never again, and `setBands`' own
guard proves only the count.

Measured on a three-band add with a listener answering the first split store that moves:
*"Undo took back an authoritative write the user's Add never made: the split is at 300.0 Hz, not the
4000.0 that was installed"*, and Redo reinstalled the host's value as though the Add had produced it.
The same measurement through the delete x, on the remove transaction.

`setParam` is now `storeOwned` — the whole body, not a copy of the proof — because the two differ
only in what they do with the result and `setParam` has no caller that consumes one (ADR-0046: the
four stores inside `addBandAt` / `removeBand` prove the count in their own loops). The transaction is
deliberately NOT aborted: `setParam` has always returned void and its callers have always continued,
and withholding the declaration is the whole of what the defect needs.

**The line this draws, and the residual it deliberately does not move.** A store that brackets a
gesture of its own — `resetParam`, `setBands`, `setSoloMask` — keeps the other rule: *a gesture IS
the declaration*, whatever the store did. A host write landing inside one of those brackets stays
the narrow residual recorded at the end of this section, not this defect. The distinction is exactly
"who declared the parameter": a gesture, which says the user is editing it, or a store, which can
only speak for a value that is still there. State test 86 leg Z5 is written on split 1 rather than
split 0 for this reason — the click that adds a band opens a gesture on split 0 for the drag that
may follow, so split 0 is the residual and split 1 is the defect.

**And the 128-entry cap is enforced where the Consequences below always said it was.** It lived as
two hand-copied `push_back` / `size() > 128` / `erase (begin())` triples — the poll's step push and
the preset-switch push — while `abCopyToOther` had neither, so repeated A/B Copies grew the target
slot's history without limit, and a Copy's entry is the expensive kind: two whole `ValueTree`s
rather than a handful of `{index, before, after}` triples. One `pushCapped` helper now holds the
bound for all three. `undo()` and `redo()` MOVE an entry between the stacks rather than growing
either, so they are not capped and do not need to be. State test 89 measures it end to end.

## Decision — correction, 2026-09-14 (round 17)

**The endpoints are per parameter, and nothing but the user's own action may write either of them.**
Rounds 14-16 built the ownership set per parameter and left both ENDPOINTS as whole-parameter-list
snapshots: one taken when the batch opened, one retaken at every zero-crossing gesture close. That
representation cannot express this ADR's own sentence, and three measured failures followed from it
(State test 90):

* **A later gesture's close replaced an earlier gesture's endpoint.** Two gestures that finish inside
  one 24 Hz period share a pending batch — deliberately, and that grouping is unchanged. The second
  one's close re-read the whole list, so a host write that landed between them overwrote the first
  gesture's `after`. Measured: *Drive 0 → 6 by the user, host → 9, then a Width edit; Redo restored
  Drive 9.* The value the user produced was recoverable from neither end of the step.
* **`before` came from the batch's open, not from the parameter's own first ownership.** A parameter
  the user first touches late in a batch took the value it had when some OTHER parameter's gesture
  opened the batch — so automation that moved it in between became the value Undo restored.
* **An empty press made a host write undoable.** A click that starts no drag still declares its
  parameter; the close then handed that parameter whatever the host had written, and `before` and
  `after` differed only by the automation. (Round 12 tried to fix this face by gating the push on
  whether the batch edited anything, and withdrew it because that gate took away a double-click
  reset's step and the legs I and J boundary. Per-parameter endpoints fix it without any push gate:
  `before` and `after` are the same value, so the poll records nothing.)

**The rule, stated once.** For every parameter the pending batch owns:

```text
before = the value it held at the instant the batch first took it
after   = the latest value the owning gesture or store actually produced for it
```

`before` is written exactly once, by `noteFirstOwnership`, whose only job is the already-owned guard.
`after` is written by a declaring store (`noteOwnedParamWrite`, which now carries both ends) and, for
controls that write through JUCE's attachments and declare nothing, by a live read at the close of
the gesture episode **that parameter's own gesture belongs to** — never the whole list. A store's
declaration outranks that live read for the rest of its episode, because a store knows what it
installed and a live read can only see what is there.

**What this does NOT change.** The grouping is untouched: two gestures still share a pending batch,
and `resetBatchOwnership` still re-bases only on a batch the poll has already consumed. The 24 Hz
poll, the wheel attribution and chain rules (ADR-0053), the whole-state entries for a preset load and
an A/B Copy, and the message-thread-only ownership model (ADR-0036) are all as they were. No timer
was added, no lock, and nothing moved into `parameterValueChanged`.

**`foreignSinceEdge` is a different concern and stayed one.** It gates whether a scroll chain
extends and whether a step is nobody's scroll. It is read on no writer path of the endpoint arrays,
before or after this correction — verified by enumeration — so automation detection is not, and must
not become, a substitute for ownership bookkeeping.

**The implementation residual that genuinely remains, with its exact scope.** For a parameter whose
writes go through JUCE's attachment rather than through a declaring store, a host write that lands
after the user's last write to that parameter and before that same parameter's own gesture closes is
indistinguishable at the close: the live read is all there is. The window is one gesture wide on one
parameter, and closing it would need a per-write user hook, which on this architecture means
`parameterValueChanged` — audio-thread-reachable, and forbidden by ADR-0036. It is an implementation
limit, not a product rule: automation is still never a user Undo step, never redefines a `before`,
and for every parameter a store declares it cannot reach `after` either — which is measured rather
than asserted, by State test 86 leg Z7: a host write placed in exactly that window, after the drag's
final proved store and inside the same split's gesture close, does not become the value Redo
restores. Mutation M65 removes the rule and the leg fails.

> **SUPERSEDED 2026-09-14 (round 18). It was not a residual, it was the same defect one control
> family further out — and its stated reason was wrong.** The window is real and the scope is
> accurately described, but calling it an implementation limit understated it: a user drags Drive,
> the host writes Drive before the button comes up, and Redo restores the host's value as though the
> user had produced it. That is the sentence this decision opens with, violated. The claim that
> closing it "would need `parameterValueChanged`" is also false: the write is observable on the
> MESSAGE thread, at the control, because JUCE's attachment writes the parameter inside the
> control's own value-changed dispatch. The correction is below; nothing about ADR-0036 had to move.

## Decision — correction, 2026-09-14 (round 18)

**A control that writes a parameter says what it wrote, and no later read may replace it.** Round 17
made the endpoints per parameter and gave a declaring store a way to state its own `after`
(`noteOwnedParamWrite`, episode bit 1). Everything else — every knob, slider, numeric value box,
button and combo, which is to say the whole editor outside the multiband display — writes through a
JUCE parameter attachment, which declares OWNERSHIP when its gesture opens and declares no VALUE at
all. The close therefore had nothing but a live read of the parameter, and a host write landing
after the user's last attachment write and before that gesture closed became the recorded `after`:

```text
user drags Drive to 1.92 dB
host automation writes Drive 9.00 dB     (before the button comes up)
gesture closes
    -> after(Drive) = 9.00       Redo restores the host's value
```

Measured exactly that way at `8b136fa`, on the real Drive knob driven by synthetic mouse events, at
both instants that can reach it: a host write interposed between the last `mouseDrag` and `mouseUp`,
and one fired from inside the `endChangeGesture` dispatch itself. State test 91 legs A, B, D, E and
G all failed.

**The rule, unchanged; the missing half, supplied.**

```text
before = the value it held at the instant the batch FIRST took it
after   = the latest value the OWNING gesture or control actually produced for it
```

`after` now has a witness on the UI side for the attachment families too. `noteOwnedParamEndpoint`
is the narrow half of `noteOwnedParamWrite`: it sets the same episode bit 1, writes the same
`batchCloseValue`, and **refuses to create ownership**, because a write the batch does not already
own is not a user step's endpoint and declaring one would hand an undo step to the gesture-less
writes ADR-0052 deliberately leaves alone.

**Why the UI side can tell a user write from a host write, which is the whole difficulty.** It
cannot do it from the value: `SliderParameterAttachment::setValue` pushes the host's value in with
`sendNotificationSync`, so `Slider::valueChanged`, `Slider::Listener::sliderValueChanged` and
`Slider::onValueChange` all fire for a host write exactly as they do for a user write, and JUCE's
`ignoreCallbacks` guard is private with no accessor. `Slider::snapValue` *is* user-only, but its
coverage hole is categorical — it is declared on `juce::Slider` alone, so it can see no Button and
no ComboBox write, and four of this editor's own user-write sites call `Slider::setValue` directly
and never reach it.

So the discriminator is not the value, it is **who moved the parameter**. The editor registers two
hooks per parameter-backed control, one before JUCE's attachment and one after it — `ListenerList`
dispatches in registration order, and the attachment writes the parameter inside its own callback,
so the pair straddles the write:

```text
user write : before = P_old   ... attachment writes P ...   after = P_new    -> P moved, record
host push  : before = P_host  ... attachment suppresses ... after = P_host   -> P did not move
```

A host push writes the parameter first and only then sets the control, so the parameter cannot move
during the control's own notification. A user write that lands on the value the parameter already
holds moves nothing either, and JUCE skips it outright
(`ParameterAttachment::callIfParameterValueChanged`) — correctly, since it produced nothing.

**What is recorded is what the control ASKED FOR**, not a second reading of the parameter: a host
answering the write re-entrantly would already be in the live value, which is the same reason
`SpectrumImager::storeOwned` passes its own installed value (round 15). State test 91 leg H places
exactly that reentrant write, and mutation M68 — record the live value instead — fails it.

**What this does NOT change.** No gesture span, no host touch or latch span, no parameter ID, range,
default or serialization field, no DSP node or stage order, no reported latency, no thread and no
new cross-thread path: the witness runs on the message thread, on the same synchronous stack as the
user's own event handler. JUCE's own attachments are kept — nothing is replaced or reimplemented —
the 24 Hz poll is untouched, the sequential-gesture batching is untouched, ADR-0053's wheel rules
are untouched (State test 91 leg G re-asserts chain extension and termination), and the whole-state
entries for a preset load and an A/B Copy are untouched.

**The residual is now stated in one line, and it is not this one.** For a parameter a control
declares, `after` is the control's own value and no read can replace it. What remains is what
ADR-0052 already governs: a write made with no change gesture open is not a user step at all, and
this decision does not give it one. State test 91 legs I and J hold that line.

## Decision — correction, 2026-09-14 (round 20)

**An attachment-backed endpoint must be known BEFORE the gesture it belongs to can be polled.** The
round-18 witness states the endpoint after the attachment's write, which is early enough for a
slider — its attachment writes in one callback (`setValueAsPartOfGesture`) and closes the gesture in
a later one (`sliderDragEnded`), so the endpoint is standing before the close. It is NOT early
enough for a ComboBox or a Button: JUCE does begin/write/end for those inside a SINGLE listener
callback (`setValueAsCompleteGesture`, juce_ParameterAttachments.cpp:59-67), and the witness is the
next listener in that same pass — so the gesture closes, and its endpoint is read, while the witness
is still pending.

**The close is also where the batch becomes pollable, and the host is handed control in between.**
`pendingGestureCommit = true` and the endpoint live read both happen inside
`parameterGestureChanged`; the plug-in is an ORDINARY parameter listener while the host's wrapper is
the parameter's `finalListener`, called last (juce_AudioProcessorParameter.cpp:103-108). A host that
pumps its message loop from the gesture-end callback therefore gets a nested `pollUndoCoalesce` in
that gap, and what it commits is final — `UndoEntry` stores the value, so the witness's later
declaration cannot reach it:

```text
user selects Algorithm item 3, from item 0
a controller answers the write from inside `setValueNotifyingHost`, Algorithm 1
gesture closes  -> after(Algorithm) = 1        (the live read)
the host pumps  -> nested poll commits the step
the witness finally runs and states 3, into an entry that has already been pushed
    -> Redo restores 1, a value the user never selected
```

Measured on the real Algorithm combo at `e9a0353` (State test 93 legs A, E and J — two failing
checks at the head, three once leg J was added).

**TWO INGREDIENTS ARE NEEDED, and the report named one.** A host that merely pumps at the close makes
the poll commit the live read, which IS what the user asked for. The endpoint is poisoned only if a
host write landed EARLIER, inside `setValueNotifyingHost`, before the close read it. Both halves are
the host's own two callbacks.

**The correction is a REQUEST, armed before the attachment runs.** The witness's before-hook — which
already runs ahead of JUCE's attachment, and by which time the control is holding its new value —
tells the processor what this control is about to ask for; the close prefers that to its live read.
It is deliberately weaker than every `note*` beside it: no ownership, no episode bit, no step. A host
push arms and disarms it with no gesture in between and nothing consumes it. The previous request is
handed back rather than cleared, so one control's notification nested inside another's cannot strand
the outer one.

**Scope, established rather than assumed.** Combo boxes are affected. Buttons reach the same window
but cannot carry a wrong endpoint here: every toggle in this editor drives a `RawBool`
(`getNumSteps() == 2`), so the only value a host can install that is not the one the user just
produced is the one the user just left — which restores the committed sound, and the batch's own
`sig != committedSig` gate then correctly records nothing. Sliders and the imager's stores are out
of reach for the structural reason above.

**Round 19's "where the live read still runs, stated exactly" was incomplete, and is corrected here.**
It named two cases — an empty press, and the imager's bare-`setValueNotifyingHost` stores. There was
a third: an attachment-backed parameter whose complete gesture closes before its witness can speak.
That third case is what this round closes. The first two are unchanged, and the second remains the
open window RISK-012 is recorded against.

**What this does NOT change.** No gesture span, no host touch or latch span, no parameter ID, range,
default or serialization field, no DSP node or stage order, no reported latency, no thread and no new
cross-thread path. Nothing was added to `parameterValueChanged`; `foreignSinceEdge` was not touched.
The 24 Hz poll, the batching and ADR-0053's wheel rules are untouched, and no poll was suppressed:
the fix makes the nested poll commit the right value rather than preventing it from running.

## Decision — correction, 2026-09-14 (round 19)

**A refused store states no endpoint, and that is a fact the close has to be told.** Round 18 gave
the close a second source of endpoints — the editor's attachment witness — and left the third case
speaking only by silence. `SpectrumImager::storeOwned` reads its own write back and REFUSES the
declaration when what is in the slot is not what it installed; the refusal means somebody else's
value is there, so the store produced nothing the user can be said to have made. But "no declaration"
is exactly what an attachment-backed control that has not written yet looks like, so the close could
not tell them apart and fell through to its live read:

```text
user presses a bandwidth, Width 1.0
user drag stores Width 1.1
a controller answers the store from inside `setValueNotifyingHost`, Width 1.4
`storeOwned` reads back, sees 1.4, refuses
gesture closes
    -> after(Width) = 1.4       Redo restores a value the user never produced
```

Measured on the real multiband display at `98464db`, on three paths: the bandwidth drag when no
earlier store stood, the standalone bandwidth notch, and a multi-notch scroll whose last store is
refused (State test 92 legs B, C and E — six failing checks).

**The correction is one bit and one call.** `batchEpisodeParam` gains bit 2: *a store on this
parameter was refused in this episode*. `storeOwned` reports it through `onOwnedRefused` on the same
line that already returns `false`, and the close skips any parameter carrying it. The endpoint then
stays exactly where the last thing that actually stood left it — the value `noteFirstOwnership`
seeded at the gesture open, or the last store that stood — so a refusal costs the step that
parameter rather than inventing an endpoint for it.

**It states no value, deliberately**: a store that did not stand has none to state. And it makes no
ownership test, because it only ever SUPPRESSES a read.

**What this does NOT change.** No gesture span, no host touch or latch span, no parameter ID, range,
default or serialization field, no DSP node or stage order, no reported latency, no thread and no new
cross-thread path. Nothing was added to `parameterValueChanged`; `foreignSinceEdge` was not touched,
because scroll continuation and endpoint attribution are different questions and stay separate. The
24 Hz poll, the batching and ADR-0053's wheel rules are untouched.

**Where the live read still runs, stated exactly.** After this round it survives for two cases only:
a gesture that produced no write at all (an empty press — where the live value equals the `before`
unless a host writes inside the press), and the imager's gesture-bracketed stores that write with a
bare `setValueNotifyingHost` rather than through `storeOwned` (`resetParam`, `setBands`,
`setSoloMask`), for which it is the only endpoint source. Those are the pre-existing narrow
in-gesture window this decision has recorded since round 16, and this round does not move it.

## Decision — correction, 2026-09-15 (round 21)

**The three bare imager stores are not an exception to the rule; they were an omission, and they
are closed.** Review finding `src/PluginProcessor.cpp:R1078-1081` names the second half of the
paragraph directly above: `SpectrumImager::resetParam`, `setBands` and `setSoloMask` opened a change
gesture, wrote with a bare `setValueNotifyingHost` and declared nothing, so the close fell back to a
LIVE READ to learn the endpoint. Every round since 16 has recorded that window as "narrow" and left
it; this round measured what is actually in it.

**What the window contains.** The write is `setValueNotifyingHost`, whose listeners run
SYNCHRONOUSLY inside it. A host answering that write — a control surface echoing, automation
writing back, anything in the wrapper's seat — is sitting in the parameter when the close reads it.
The host's value therefore became the user action's `after`, and so the destination of the user's
Redo. That is not a narrow window on a benign value: it is exactly the rule this ADR's round-14
amendment exists to state, broken in three places. State test 94 leg A measures it — with the
pre-round-21 stores in place, a width reset the host answered has **Redo landing on the host's
value** — and leg H2 measures the same for the solo mask.

**The fix uses the mechanism that was already correct.** No parallel attribution system: the three
stores now take the read-back shape `storeOwned` has used since round 15 — capture `was`, compute
the value the parameter will render, write, compare — and report through the same `onOwnedWrite` /
`onOwnedRefused` callbacks the batch bookkeeping already consumes. What is there afterwards either
is what was installed or is somebody else's, and those are different facts.

**The guard branches are refusals too.** When the topology check ahead of the store fails, the
gesture has opened and closed having written nothing, so the live value at the close is whatever a
host left there — the one value that must not become this action's endpoint. Each of the three now
reports a refusal on that branch as well, and `setBands` / `setSoloMask` set their `stored` result
only on the branch that actually stood.

**Where the live read still runs, restated.** One case, not three: a gesture that produced no write
at all (an empty press), where the live value equals the `before` unless a host writes inside the
press. RISK-012 stays OPEN against that alone.

**What this does NOT change.** No gesture span, no parameter ID, range, default or serialization
field, no DSP node or stage order, no reported latency, no thread and no new cross-thread path. The
topology guards themselves are untouched — the same check, in the same place, deciding the same
thing; only what the store REPORTS afterwards is new.

## Decision — correction, 2026-09-15 (round 22)

**A CONTROL THAT PRODUCED NOTHING SAYS SO, AND THE CLOSE'S LIVE READ SURVIVES FOR THE GESTURES THIS
PLUG-IN NEVER OPENED.** RISK-012's last enumerated window was the EMPTY PRESS: a change gesture
opened on a parameter, no store declared an endpoint and none was refused (`ep == 1` exactly), so the
batch close fell back to reading the parameter live — and during the press that value is whatever host
automation left there. A value no user operation produced then became that operation's Undo/Redo
endpoint, which is the one thing the round-14 amendment forbids.

**The repair that does NOT work, recorded because it is the obvious one and it was measured.**
`noteFirstOwnership` already seeds `after` to the same value as `before`, precisely so that "a
declaration that never produces anything — an empty press — reads as `before == after` and the poll
records nothing for it". Deleting the live read should therefore be enough. It is not: **42 assertions
across the state suite fail**, because a gesture the EDITOR DID NOT OPEN reaches the close in the same
state. A host's own generic editor brackets `setValueNotifyingHost` in a begin/end pair through the
wrapper and declares nothing — that is a real user edit, and the live value at the close is the only
record of its endpoint. At the close the two are indistinguishable; the discriminator has to live where
the difference exists.

**Where it lives.** In the editor, which knows whether one of ITS controls produced a value:
- `AttachmentWitness` (`src/PluginEditor.h`) states a refusal at `sliderDragEnded` when nothing the
  control did moved the parameter between drag start and drag end. JUCE opens the gesture from
  `SliderParameterAttachment::sliderDragStarted` and closes it from `sliderDragEnded`; the witness's
  BEFORE hook is registered ahead of JUCE's attachment, so the refusal lands before `endChangeGesture`.
  ComboBox and Button need nothing: their attachments write only on a real change, and the round-20
  request already covers the endpoint their close cannot see.
- `SpectrumImager::endGesture` states the same refusal for every gesture opened through it, which is
  most of the display's — the drags, the wheel branches and the width press — and is the one place
  those close. The five that bracket their own gestures directly (`resetCrossover` and
  `commitFreqEditor` this round; `resetParam`, `setBands` and `setSoloMask` since round 21) state it at
  their own site. It is unconditional: it carries no value, and the close skips a parameter whose store
  declared an endpoint (bit 2) before it consults the refusal bit, so a drag that DID store keeps the
  value its last `storeOwned` installed.

**No new bookkeeping.** Bit 4 is round 19's refusal and has suppressed the close's live read since
then. Round 22 adds the sentence at the two places that never said it, and adds no bit, no vector and
no state. `ep == 1` remains "a gesture opened and nothing spoke for it"; what changed is that
Anamorph's own controls no longer leave the close in that state when they produced nothing.

**What is still open**, stated rather than implied: a HOST-OPENED, HOST-EMPTY gesture — the host
brackets a gesture through the wrapper, writes nothing inside it, and its own automation moves that
parameter during the bracket. `FUTURE_RISKS.md` RISK-012 carries it, still OPEN.

**Regression coverage.** State test 88 leg O (knob press with automation inside it, plus the control
leg where a press that moves the knob is still one undoable step with the user's own Redo destination)
and State test 94 leg K (the same on the imager's width line). Mutations M91 and M92 each fail exactly
their own leg.

State test 94 **leg L** covers the two resets that bracket their own gesture, and it is a NEGATIVE
result: it builds the window (a host lane drops Bands and automates the split from inside the reset's
gesture open, so the reset skips its store entirely) and measures no undo step — and **M93**, which
removes that refusal, measures no undo step either. Something older than round 22 already shuts that
one, and this round did not isolate which rule; the refusal is kept because it makes the property
local rather than dependent on a mechanism two subsystems away, and M93 is recorded as a survivor.

## Decision — correction, 2026-09-15 (round 23)

**EVERY GESTURE THIS PLUG-IN OPENS DECLARES WHAT IT PRODUCED, AND THE LAST TWO THAT DID NOT ARE
FIXED.** Review finding `src/PluginProcessor.cpp:R1117-1119` — the batch close's live read of the
parameter — reported as a generic-host-editor defect: the host opens a gesture, writes A, automation
writes B, and Redo lands on B.

**The reported path does not exist, and saying so is half the correction.** No JUCE plug-in wrapper
calls `beginChangeGesture`/`endChangeGesture` INBOUND on a plug-in's parameters. Measured:
`grep -rnE '(\.|->)(begin|end)ChangeGesture' build/_deps/juce-src/modules/juce_audio_plugin_client/`
returns zero across VST3, AU, AUv3, AAX, LV2, VST2, Standalone and Unity; all 14 "ChangeGesture" hits
there are the outbound `audioProcessorParameterChangeGestureBegin/End` overrides, and LV2 discards a
host touch outright. A host write arrives as a VALUE, never as a bracket. The round-22 comment that
justified keeping the live read — *"a host's own generic editor brackets `setValueNotifyingHost` in a
begin/end pair through the wrapper"* — was therefore false, and it contradicted `PluginProcessor.cpp`'s
own correct statement 79 lines above it. Corrected in place.

**The defect is real, and it is ours.** Enumerating every gesture opener in the tree turned up two
first-party paths that opened a change gesture and declared nothing:

- **`AnamorphAudioProcessor::applyAutoGain`** — the editor's *Apply Gain* button. A bare
  begin/`setValueNotifyingHost`/end on Output Gain, and another on Level Match. It is the last bare
  bracket in the tree, and round 21 never reached it because that round's scope was the imager's three
  bare stores. `setValueNotifyingHost` dispatches to every listener synchronously from inside itself,
  so a host answering this very write is sitting in the live value when the close reads it. Both
  stores now use `storeOwned`'s read-back shape, unchanged since round 15: capture `was`, compute what
  the parameter will render, write, compare the read-back, then `noteOwnedParamWrite` if it stood or
  `noteOwnedParamRefused` if it did not.
- **`Knob`'s Alt-click and double-click resets.** Their ADR-0052 guard `resetWouldMove()` asks the
  SLIDER — deliberately, because that is the space `Slider::setValue` compares in — while the
  PARAMETER can already be sitting on the reset value. A parameter written without notifying its
  listeners never reaches `ParameterAttachment`, and an off-message-thread write reaches it only
  through `triggerAsyncUpdate`, so the control lags by up to one message-loop turn. In that window the
  guard says "this moves something", the gesture opens, JUCE's attachment drops the write because the
  parameter already holds that value, and nothing is declared. Both paths now state a refusal
  unconditionally before their close, exactly as `SpectrumImager::endGesture` has since round 22 — the
  close skips a parameter whose store DECLARED an endpoint before it consults the refusal bit, so a
  reset that really moved the parameter keeps the endpoint its witness stated.

**No new machinery.** Bits 2 and 4 are rounds 15 and 19; `noteOwnedParamWrite` and
`noteOwnedParamRefused` are the same API every other store in the tree calls. Round 23 adds no
episode bit, no vector, no hook in `parameterValueChanged` and no write-time attribution framework.
A write-time hook was considered and rejected on evidence: `parameterValueChanged` is audio-thread
reachable (VST3 `process` → `processParameterChanges` → `setValueNotifyingHost`), the batch vectors
are non-atomic message-thread-owned state, and a message-thread guard would not help anyway — the hook
receives no provenance, and a message-thread `parameterValueChanged` also fires for undo, redo, A/B
and preset loads through `reassertParameters`.

**What this leaves.** Every gesture Anamorph's UI opens now declares or refuses, and no host can open
one, so the close's `ep == 1` arm is unreachable in any shipped format. Its only remaining consumers
are the 42 harness assertions that bracket a bare `setValueNotifyingHost` to stand in for a user edit
— a shape no wrapper produces. `FUTURE_RISKS.md` RISK-012 carries the full twelve-row attribution
matrix and the disposition.

**Regression coverage.** State test 96: leg A (Apply Gain answered re-entrantly — before the fix,
`Undo -> 6.0000, Redo -> -11.5000`), leg B (the control: an uninterrupted Apply is still one undoable
step whose Redo restores what Apply produced), leg C (a Knob reset whose guard answered on a stale
slider, with the host write landing at the gesture CLOSE).

## Decision — correction, 2026-09-15 (round 24)

**ONE USER ACTION IS ONE UNDO STEP, EVEN WHEN IT IS NINE STORES.** Review finding
`src/PluginProcessor.cpp:R1092` — the line that raises `pendingGestureCommit` when a gesture closes —
reported that Undo can record a PARTIAL topology. Reproduced, and the report is right.

**The mechanism, reconstructed from the source rather than from the report.** A multiband topology
change is not one write. `SpectrumImager::addBandAt` and `removeBand` compute a plan from a snapshot
and apply it as six to nine `setValueNotifyingHost` calls (ADR-0040) — and two of those stores bracket
a change gesture of their own: `setSoloMask` at the FRONT and `setBands` at the BACK. The front one
CLOSES, in the middle of the transaction. At that close `--openGestures` reaches zero and
`pendingGestureCommit` is raised, which is the whole of what `pollUndoCoalesceAdopted` asks before it
commits — while the widths, the splits and the count have not been written yet.

**The poll can land there, and the door is the one JUCE dispatches LAST.**
`AudioProcessorParameter::endChangeGesture` notifies every `AudioProcessorParameter::Listener` first —
the processor among them, which is what drops `openGestures` — and only then the `finalListener`,
which is `AudioProcessor::ParameterChangeForwarder`, which fans out to every `AudioProcessorListener`,
i.e. to the HOST (`juce_AudioProcessorParameter.cpp:102-108`, `juce_AudioProcessor.cpp:1476-1487`). A
host that pumps its message loop from that callback lets the editor's 24 Hz tick run, and all that
tick does is poll. This is the same seat State test 94 legs F and G use and the same one RISK-009 and
ADR-0036 §26 are written for; it is a production path, not a harness artefact.

**Measured before the fix, State test 98 leg B** — one Add-band click on a two-band layout with band 1
soloed, one Undo: `bands 2 (started 2), solo 0x4 (started 0x2), more undo available: yes`. A solo word
naming band 2 in a two-band layout, which `SoloMonitor::process` masks with `((1 << bands) - 1)` down
to **nothing**: the user's soloed band silently gone, in a state no completed action ever produced, and
a second Undo needed to finish undoing one click.

**THE CLASS IS WIDER THAN THE TWO FUNCTIONS THE FINDING NAMES, and the sweep that found the rest is
the reason this section exists rather than a two-line patch.** Every multi-store action that closes an
inner gesture and keeps writing has the same window:

| Action | Closes an inner gesture at | ...and then still writes |
|---|---|---|
| `SpectrumImager::addBandAt` | `setSoloMask` | up to four widths, three splits, the count |
| `SpectrumImager::removeBand` | `setSoloMask` | the widths, the splits, the count |
| `SpectrumImager::resetCrossover` | the primary split's own bracket | `spreadSplits`' neighbours |
| `SpectrumImager::commitFreqEditor` | the primary split's own bracket | `spreadSplits`' neighbours |
| `AnamorphAudioProcessor::applyAutoGain` | Output Gain's bracket | Level Match's bracket |

The last two rows are worth naming out loud. `resetCrossover` splitting into two steps is
**R515's defect (round 15) arriving through a different door** — one Undo puts the reset back and
leaves the pushed neighbours where the reset shoved them — and `applyAutoGain` is code ROUND 23 wrote:
its two read-back stores bracket two gestures, and the first one's close is a commit point. Neither
was reported; both were found by asking the same question of every multi-store action in the tree.

**Decision.** A multi-store user action declares its own LIFETIME, and the undo poll does not commit
inside one. `AnamorphAudioProcessor::beginUserTransaction` / `endUserTransaction` count depth on the
message thread; `pollUndoCoalesceAdopted` adds `|| userTransactionDepth > 0` beside the
`openGestures > 0` test it has always had. The imager reaches the counter through a new
`onUserTransaction` callback (it holds no processor pointer, by design) and both sides drive it only
through an RAII scope — `addBandAt` alone has ten early returns, and a hand-written pair would miss
them.

**What this deliberately does NOT do**, because each of these was a way to get the same symptom wrong:

- It does not CONSUME, clear or discard `pendingGestureCommit`. The guard SKIPS, exactly as the
  `openGestures` guard does: the batch vectors keep accumulating and the first poll after the
  transaction ends commits the whole action as one step. Mutation M97 makes the guard discard instead,
  and 14 checks fail — the step vanishes rather than being made whole.
- It is not a delay, an inactivity timer or a sleep, and it does not depend on the host behaving. The
  scope is the transaction's own stack lifetime.
- It does not move the commit to a new place or introduce a second undo model. Nothing about
  endpoints, ownership bits, wheel-step naming or the batch's own rules changes; the only new fact is
  *when the poll may act*.
- It does not widen what a step contains. Host automation landing inside the transaction is in no
  step, for the reason it already was not: an entry carries only parameters the user's own batch
  DECLARED, and a host lane declares nothing (State test 98 leg D).

**Coverage.** State test 98, seven legs: A (the control, no pump), B (the add under a pumping host),
C (the removal), D (automation inside the transaction is in no step), E (a refused action records
nothing), F (the crossover reset and its spread), G (Apply Gain). Mutations M96-M100 — the guard
removed (11 checks fail), the guard discarding the commit (14), the scope started after the inner
close (4), only `addBandAt` protected (4), the transaction end omitted (44).

**Architecture Review Gate: APPROVED by the owner, 2026-09-15 (round 24).** The approval covers the
topology transaction/Undo change as implemented in this round, the ruling that `addBandAt` and
`removeBand` are each ONE user Undo action, and the prevention of intermediate timer commits during a
transaction.

| Step | Requirement | Evidence |
|---|---|---|
| 1 | the author flags the change as gated | this section, and the PR #144 body |
| 2 | a human reviewer with DSP/audio context reviews against the relevant Policy + ADR | **The owner's ruling of 2026-09-15**, which states the invariant (*"a multiband topology mutation such as `addBandAt` or `removeBand` is one user action and must produce one coherent Undo step"*), names the shape (*"prefer an explicit transaction-lifetime guard/scope over implicit inference from `openGestures`"*) and the prohibitions (no arbitrary sleep, no polling delay, no inactivity timer, no reliance on the host being well behaved, no silently discarded commit), and states that the direction is already approved and is not to be asked again |
| 3 | if the change is a decision, an ADR is added/updated | this section of ADR-0008; no new ADR — the invariant is ADR-0008's own (*one user topology operation produces one coherent set of parameter endpoints*) and this is its enforcement |
| 4 | compatibility-affecting changes additionally run `RELEASE_COMPATIBILITY_CHECKLIST.md` | **not triggered** — no parameter ID, range, default, automation flag, serialization field or reported-latency value changes; the guard is one message-thread int |

**This is an implementation correction, not a new model.** Nothing here reverses or competes with an
Accepted ADR: ADR-0040's re-validated burst, ADR-0044's mask proofs, ADR-0052's no-op rule and
ADR-0053's wheel rules are all untouched, and the step's CONTENT is decided exactly as rounds 17-23
left it. What changed is that the poll now knows when an action is still running.

## Decision — correction, 2026-09-15 (round 25)

**A STATE-REPLACING COMMAND DOES NOT RUN INSIDE A USER TRANSACTION — IT WAITS FOR ONE, AND IT IS
NEVER DROPPED.** Review finding `src/PluginProcessor.cpp:R1279-1283` — the round-24 poll guard —
reported that a re-entrant Undo corrupts a topology transaction. Reproduced, and the report is right.

**Round 24 closed one door and not the other.** It stopped the undo POLL from committing half a
topology. It did not stop a whole COMMAND from replacing the state underneath one, and the window is
the same window: a topology burst's stores dispatch synchronously to the host
(`setValueNotifyingHost` → every `AudioProcessorParameter::Listener`, then the `finalListener`, then
every `AudioProcessorListener`), a host that pumps its message loop from one of those callbacks
dispatches whatever UI events are queued, and the editor's Undo, Redo, A/B and preset buttons are
ordinary `onClick`s on the message thread.

**THE OWNERSHIP WIPE IS THE CORRUPTION, not the value restore.** `undo()` does not merely read: it
installs an entry's `before` end, retakes `committed` from the LIVE — half-applied — sound, clears
`openGestures` and `pendingGestureCommit`, and calls `resetBatchOwnership()`. The stores the burst
has already issued lose their declarations; the ones still to come declare into a fresh batch;
`setBands` closes its gesture and the step the next poll commits describes only the TAIL of the
action. Measured before the fix, State test 99 leg B: after an Add-band click interrupted this way,
`bands 3, solo 0x4, Drive 3.00`, and one Undo of the recorded step gave
`bands 2 and solo 0x4 disagree` — a solo word naming band 2 in a two-band layout, which
`SoloMonitor::process` masks away to nothing. R1092's mixed topology, through a door round 24 left
open.

**THE COMMAND MATRIX**, from the source rather than from the report. "Reachable" means: reachable
re-entrantly, on the message thread, with `userTransactionDepth > 0`.

| Command | Entry point | Reachable | Mutates immediately | What it does to a transaction |
|---|---|---|---|---|
| Undo | `undoButton.onClick` → `AnamorphAudioProcessor::undo` | yes | yes | restores values, retakes `committed` from a half-applied sound, clears `pendingGestureCommit`, wipes batch ownership |
| Redo | `redoButton.onClick` → `::redo` | yes | yes | identical shape to Undo |
| A/B switch | `abControl.onToggle` → `::abToggle` → `abSwitchToAdopted` | yes | yes | replaces the whole live sound AND calls `syncCommitted()`, which clears `pendingGestureCommit` — so the transaction's step is not reordered, it is DELETED |
| A/B switch (explicit) | `::abSwitchTo(int)` | yes (public primitive) | yes | as above; `abToggle` does not route through it, so it is guarded separately |
| A/B Copy | `copyButton.onClick` → `::abCopyToOther` | yes | yes | photographs the LIVE half-applied topology into the other slot and pushes a whole-state undo entry for it |
| Preset step | `presetPrev/Next.onClick` → `PresetManager::step` | yes | yes | relative: re-deriving the row later is required (ADR-0036 §23), so it is deferred at the OUTERMOST entry point, not at the absolute load it computes |
| Preset load / file load | `PresetManager::load` / `::loadAdopted` / `::loadFile` | yes | yes | whole-sound replacement; `onAboutToLoad`'s flush SKIPS during a transaction, so the interrupted step is deleted |
| Preset save | `PresetManager::saveUser` | yes | no parameter, yes bookkeeping | writes no parameter, but the processor's `onSaved` hook calls `syncCommitted()` — the transaction's undo step disappears and the user's Add silently stops being undoable |
| Host `setStateInformation` (the write itself) | the host's own thread | **not this class** | — | arrives through `pendingRestore`; the sound is installed on that thread under ADR-0036 §25/§27 and never on this one |
| **Host-restore ADOPTION** | `AnamorphAudioProcessor::adoptPendingHostState`, reached from `pollUndoCoalesceFromTimer` and `timerCallback` | **yes — the ninth row, and it was found by writing leg I, not by the report** | yes | the drain is the FIRST line of the timer door, ahead of round 24's guard, so `adoptRestoreTail` → `syncCommitted()` clears `pendingGestureCommit` and calls `resetBatchOwnership()` under the open transaction — the same ownership wipe, through the adoption |

**THE NINTH ROW IS A CORRECTION OF THIS SECTION'S OWN FIRST DRAFT, and it is recorded as one.** The
row above it originally read *"it is not a message-thread command"* and stopped there, which is true
of the host's WRITE and false of its ADOPTION. State test 99 leg I was written to assert that an
adoption reached re-entrantly would truncate the burst coherently; it FAILED, and the failure is the
finding: `bands 3, solo 0x4` after the click and `bands 2 with solo 0x4` after one Undo of the
recorded step — R1092's mixed topology again. The burst did not abort because ADR-0036 §12 makes
`reinstallRestoredSound` deliberately SKIP the sound half when `soundSetGen` has not moved since the
decode (the user edited the restored session rather than replacing it), so the adoption changed no
parameter the burst's guards test, ran its tail anyway, and wiped the bookkeeping from under it.

**Decision for the ninth row: REFUSED, not queued — and the difference is the cell.** A deferred user
command must be queued because dropping it loses something the user asked for. A restore cannot be
lost by refusing: `adoptPendingHostState` consumes nothing, the cell keeps the restore whole, and the
next door adopts it — the same timer 50 ms later, or the user action that follows. That is the same
answer, and deliberately the same sentence, as the failed try-lock immediately below it. It is scoped
to the NON-BLOCKING arm (`mayBlock == false`), which is exactly the timer class already contracted to
come back later; the blocking arm's callers (`getStateInformation`, `setStateInformation`'s inline
arm, `applyAutoGain`) depend on a drain to a FIXED POINT for session coherence (ADR-0036 §15), and
weakening that was not done on evidence this finding did not produce. See *Examined residuals* below.

**This row is an EXTENSION of the owner's approved direction, not a separate approval.** The ruling
of 2026-09-15 is written about *commands*; the invariant it states — a state replacement does not
execute re-entrantly while a user transaction is active — is what the ninth row applies, at a site the
ruling's text does not enumerate. It is recorded here as such rather than counted as separately
approved. It changes no threading model: the adoption still happens on the message thread, at a door,
exactly as the existing try-lock refusal already defers one by a timer period.

**Decision.** Every one of those entry points asks first. `deferWhileUserTransactionActive` queues
the command and returns `true` while a transaction is running, and the caller returns having touched
nothing; with no transaction it returns `false` and the caller proceeds exactly as before. The
`PresetManager` reaches the same queue through a `deferIfBusy` hook the processor sets, in the style
of the `adoptPending` / `onAboutToLoad` hooks it already has.

**THE ORDERING INVARIANT, at the outermost `1 → 0` transition, in this order and for these reasons:**

1. **Nothing happens at an inner boundary.** Only the depth reaching zero means the state is once
   again one a completed user action produced. (State test 99 leg F; mutation M103.)
2. **Nothing happens when nothing was deferred.** The flush is gated on a non-empty queue precisely
   so round 24's timing is untouched: an ordinary Add still leaves its commit to the poll that
   follows the press, and click-to-add-then-drag stays ONE step instead of being split by an eager
   commit at the end of `addBandAt`. (Leg H.)
3. **The transaction's own step is committed FIRST, whole,** by `pollUndoCoalesce()`. This is round
   24's invariant being honoured, not replaced: `pendingGestureCommit` is standing and the batch
   describes the entire action. Running the command first would leave `undo()`/`redo()` to flush it
   through their own `pollUndoCoalesce()` — that call exists so an edit finished just before the
   click is not jumped past — but **`abToggle` and `abCopyToOther` have no such flush, and
   `abSwitchToAdopted` calls `syncCommitted()`**, which clears `pendingGestureCommit` outright. So
   relying on each command's internals would make the ordering depend on which command happened to
   arrive, and for the A/B and preset paths it would DELETE the step rather than reorder it. The
   order is stated once, here. (Legs D and E; mutations M104 and M106.)
4. **Then the commands, in the order the user gave them.** Two Undos inside one pumped burst are two
   Undos; an Undo then a preset switch is both, in that order. Coalescing to the last would be
   dropping a command. (Leg G; mutation M102.)
5. **A command that finds nothing to do has still executed.** A deferred Redo after the transaction
   commits finds an empty redo stack, because `pollUndoCoalesceAdopted` clears it when it pushes a
   step ("a new user action invalidates the redo stack"). That is the model's existing answer — undo,
   make a new edit, press Redo, and nothing happens today either — not a silent drop. (Leg C.)
6. **The blocking door is the right one here.** `pollUndoCoalesce` is the user-action door, and every
   transaction this runs under is a user action on the message thread. RISK-009's rule is about the
   TIMER doors (ADR-0036 §26); this is not one of them.
7. **The flush cannot re-enter itself.** `runningDeferredCommands` guards the loop, and with the
   depth already at zero a command cannot queue itself into the list it is being run from. The queue
   is also DETACHED by `std::move` before it is walked, so a `push_back` from a nested transaction
   lands in the fresh vector and cannot reallocate under the loop.
8. **And the flush DRAINS rather than flushing once.** A command runs at depth zero, so a transaction
   it starts is an ordinary one that can itself have a command deferred into it — and that inner
   `1 → 0` close finds `runningDeferredCommands` raised and returns. Walking the queue once would
   leave such a command sitting in the list until some LATER user transaction happened to close,
   which for a user who performs no further multi-store action is never. *Nothing is dropped* has to
   mean bounded, not merely not-erased. The loop cannot spin: every further iteration needs a fresh
   command, and a command is only queued by a real user action the host pumped in. (Leg J; mutation
   M109.)
9. **`endUserTransaction()` is no longer `noexcept`.** Round 24's body was one clamped decrement and
   could not throw; it now runs `pollUndoCoalesce()` — which copies the whole parameter tree and
   formats a signature — and then arbitrary `std::function` bodies, both of which allocate. The
   exposure is unchanged, because both callers are destructors and a destructor is implicitly
   `noexcept`, so a throw terminates either way; what changed is that the declaration no longer
   claims otherwise.

**What this deliberately does NOT do.** No command is dropped or coalesced away. No timer, no sleep,
no polling delay is used as the synchronisation mechanism — the scope is the transaction's own stack
lifetime. The nested event dispatch that delivered the command is not suppressed. The undo/redo model
is unchanged: nothing about endpoints, ownership bits, wheel-step naming or what a step contains
moves. Round 24's guard is preserved exactly — both invariants are required, and mutation M107
(nothing ever deferred) and the round-24 mutations M96–M100 each still fail on their own legs.

**Coverage.** State test 99: leg A (the control — the same command with no transaction running), leg
B (Undo), an explicit ordering leg, C (Redo), D (preset step, with the Add still a step of its own
beneath it), E (A/B switch and Copy, with the switch's `syncCommitted` as the discriminator), F
(nested transactions), G (two commands from one close), H (nothing deferred → round-24 timing
unchanged), **I (the host-restore adoption reached through the timer door)**, **J (a command stranded
in a transaction a command itself started)**. Mutations M101–M109 plus M105b, each killed by its own
leg.

**Examined residuals, stated rather than closed.**

* **The blocking arm of `adoptPendingHostState` inside a transaction.** `applyAutoGain`,
  `getStateInformation` and `setStateInformation`'s inline arm all drain before they act, and all
  three are message-thread entry points a host could in principle deliver from inside a parameter
  callback during a click. The refusal is not extended to them because their contract is a drain to a
  FIXED POINT (ADR-0036 §15): an older restore left unadopted there would later stamp its metadata
  over a newer session, which is a worse failure than the one being closed. No evidence of that shape
  was produced this round; it is recorded, not fixed.
* **A deferred `loadFile` / `saveUser` returns `true`.** Reporting failure would make the editor say
  the file could not be read when it simply has not been read YET, so the deferred return is success.
  The consequence, stated: the Save panel closes before the file exists, and a genuine failure inside
  the deferred run is reported nowhere. UI convergence itself is fine — `refreshPresetDisplay()` runs
  on the editor's 24 Hz tick, so the caller's immediate reads are corrected within one period.
  > **WITHDRAWN in round 27 — see the amendment below.** Review finding
  > `src/PresetManager.cpp:R640` reported exactly the consequence this bullet recorded and declined
  > to fix, and the owner has ruled against the reasoning. The two are not the same question: "has
  > not been read YET" is a statement about TIMING, and the caller was reading it as a statement
  > about OUTCOME. A third answer -- deferred -- costs nothing and says both.

**Architecture Review Gate: APPROVED by the owner, 2026-09-15 (round 25).** The approval covers
preventing state-replacing commands from executing re-entrantly inside `userTransactionDepth`,
deferring them until the outermost transaction completes, and preserving one coherent topology Undo
step.

| Step | Requirement | Evidence |
|---|---|---|
| 1 | the author flags the change as gated | this section, and the PR #144 body |
| 2 | a human reviewer with DSP/audio context reviews against the relevant Policy + ADR | **The owner's ruling of 2026-09-15**, which states the invariant (*"state-replacing commands must not execute re-entrantly while a user transaction is active"*), names the mechanism (*"defer the command; allow the current user transaction to finish; on the transition from outermost transaction depth 1 → 0, execute the deferred state-replacement command(s)"*), and states the prohibitions (do not drop commands, no sleeps, no arbitrary timers, no polling delays, do not rely on the host not pumping, do not suppress the nested dispatch, do not redesign the Undo/Redo model), and states that the direction is already approved and is not to be asked again |
| 3 | if the change is a decision, an ADR is added/updated | this section of ADR-0008; no new ADR — the invariant is ADR-0008's own and this is its enforcement |
| 4 | compatibility-affecting changes additionally run `RELEASE_COMPATIBILITY_CHECKLIST.md` | **not triggered** — no parameter ID, range, default, automation flag, serialization field or reported-latency value changes |

ADR-0036 and ADR-0053 are untouched by this round and keep the owner approvals already recorded in
them; neither was reopened.

## Decision — amendment, 2026-09-15 (round 27): queued is not done

Review finding `src/PresetManager.cpp:R640`, *"deferred preset failures report success"*. Confirmed.
It is the round-25 examined residual above, reported — and the reasoning recorded there is withdrawn.

**What the defect actually was.** Round 25 made `saveUser` and `loadFile` deferrable and had them
return `true` for work they had merely queued, with the deferred re-entry written
`(void) saveUser (rawName)`. Three consequences, all real:

| path | what the user saw | what had happened |
|---|---|---|
| `PluginEditor.cpp` Save button | the panel closed, the preset list refreshed | nothing was on disk yet |
| the deferred write then failing (read-only folder, full disk) | nothing at all | the save never happened, and the `(void)` discarded the answer |
| `PluginEditor.cpp` chooser load of a foreign file | the knobs swept and the display refreshed | the load was refused later, by a call whose result nobody held |

**The contract, as the owner ruled it.** A synchronous result means the operation really completed.
Anything that cannot complete synchronously reports its FINAL result through an explicit completion,
and the initiating UI stays pending until that completion arrives. Concretely:

* `PresetManager::OpResult` is `{ failed, completed, deferred }` — a **scoped** enum, so
  `if (saveUser (n))` cannot compile and quietly mean "queued OR saved" again. Changing the type
  rather than the meaning of `true` is what makes every caller a compile error instead of a reader's
  problem.
* `onComplete`, when supplied, is called **exactly once** with the final answer: synchronously before
  returning for `completed` and `failed`, later from the deferred execution for `deferred`. A caller
  may ignore the return value entirely and still be correct, which is what the editor does.
* **Failures knowable without touching the disk are decided before anything is queued.** An empty or
  illegal name is a property of the argument; "is this an Anamorph preset" is a property of the
  bytes. `saveUser` checks the name first, `loadFile` parses first — so a deferred load has no
  failure mode left at all, and a deferred save can only fail in I/O. An open transaction does not
  turn a bad name or an unreadable file into queued work.
* The UI: the Save panel stays open and its button is disabled while the save is queued; on success
  it closes as before, on failure it stays open with the text intact and the field marked. The
  completions capture a `juce::Component::SafePointer`, which is the pattern the OS file chooser on
  the same screen already used.

**There is no cancellation, and that is stated rather than invented.** A queued command is never
dropped — that is this ADR's round-25 rule — so a completion always runs, and it may run after the
editor that asked for it has gone. The lifecycle constraint is therefore *the completion must survive
its initiator*, not *the initiator must be able to cancel*. State test 102 leg H builds a real editor,
queues a save from it, destroys the editor and then reaches the boundary; ASan, UBSan and valgrind all
run that suite.

**Architecture Review Gate: NOT TRIGGERED, and the determination is recorded rather than assumed.**
`ARCHITECTURE_REVIEW_GATE.md` lists eight gated areas — DSP graph, signal flow, thread model,
parameter registry, serialization registry, latency, plugin format, build system — and this change
touches none of them: no parameter, no serialized field, no thread, no reported latency. `ADR_POLICY.md`
makes an ADR mandatory for nine categories and this is in none of them either, so **no new ADR was
created**; it is recorded here, as an amendment to the ADR whose own round-25 deferral produced the
defect. The owner's completion-semantics ruling of 2026-09-15 is the decision being recorded, and it
was given before the work started and is not asked again.

| Step | Requirement | Evidence |
|---|---|---|
| 1 | the author flags the change as gated | **not gated** — the determination above, against the eight areas by name |
| 2 | a human reviewer with DSP/audio context reviews against the relevant Policy + ADR | **The owner's ruling of 2026-09-15 (round 27)**: *"A deferred operation must not return synchronous success merely because it was queued"* — with the synchronous/deferred split, the requirement that the final result travel on an explicit completion and not be discarded, the UI requirement that a queued operation is not presented as completed, and the section-9 requirement that synchronously-knowable failures are rejected before queuing |
| 3 | if the change is a decision, an ADR is added/updated | this amendment to ADR-0008; **no new ADR**, per `ADR_POLICY.md`'s categories |
| 4 | compatibility-affecting changes additionally run `RELEASE_COMPATIBILITY_CHECKLIST.md` | **not triggered** — the preset FILE format is byte-for-byte unchanged; only the C++ return type of two internal entry points changed, and no session or preset written by any earlier build reads differently |

**What was checked and left alone.** `PresetManager::load (int)` and `step (int)` return `void`, so
they report no success to misread; the editor's `refreshPresetDisplay()` after them can show the
pre-load name for up to one 24 Hz tick, which the editor's own tick then corrects — a transient, not
a discarded failure, and not the R640 class. The A/B and undo commands
(`undo`, `redo`, `abToggle`, `abSwitchTo`, `abCopyToOther`) return `void` for the same reason and are
unchanged.


## Consequences
- Both A/B slots are snapshotted to the **open (Default) state in the constructor** (`abEnsureInit`),
  not lazily on the first switch — so editing A before ever visiting B does not leak into B; the slots
  are independent from open, deterministically (the lazy path made "B == open state" depend on host
  `getStateInformation` timing). The switch/apply/serialization logic is unchanged; only *when* the
  first snapshot is taken.
- Per-slot histories survive editor close (the A/B model lives in the processor).
- A `soundSignature()` over non-view params drives coalescing.
- A **preset switch is one undo step** in the *active* A/B slot's history (via the bracket hooks);
  consecutive switches within a slot chain, while the two slots keep independent histories.
- Cost: a hand-rolled history with a 128-entry cap per slot, enforced since 2026-09-14 by the single
  `pushCapped` helper that every growing append goes through (State test 89).
- **Since the 2026-09-13 amendment, a scoped entry is CHEAPER than the snapshot it replaces**: a
  handful of `{index, before, after}` triples and two short strings instead of a whole `ValueTree`.
  Only the two whole-state entries (preset load, A/B Copy) still carry trees, and they carry two.
- **An Undo no longer writes every sound parameter.** It writes the step's own, through the same
  `applyStateSet` path as before — the live state with those values overwritten — so the §24
  replacement lock, the view-param preservation, `reassertParameters`' exact-value assert and
  `noteWholeSoundReplaced()` all still happen, and `reassertParameters`' own "exactly equal, or it
  is written" gate is what makes the rest of the tree a no-op.

## Related code
- `src/PluginProcessor.cpp` — `soundSignature`, `pollUndoCoalesceAdopted`, `applyUndoEntry`,
  `undo`/`redo`, `commitPresetSwitchUndoStep`, `abCopyToOther`, `pushCapped`,
  `parameterGestureChanged`, `noteFirstOwnership`, `noteOwnedParamWrite`, `noteOwnedParamEndpoint`,
  `noteOwnedParamRefused`, `resetBatchOwnership`, `beginUserTransaction` / `endUserTransaction`,
  `deferWhileUserTransactionActive`, `adoptPendingHostState` (its round-25 non-blocking refusal —
  ADR-0036 §28) (`snapshotSoundValues` was deleted in round 17 — both of its
  call sites were the defect)
- `src/PluginProcessor.h` — `StateSet`, `ParamEdit`, `UndoEntry`, `UndoStacks`, `kUndoDepth`,
  `batchOpenValue` / `batchCloseValue` / `batchOwnedParam` / `batchEpisodeParam`, the A/B members
- `src/PluginEditor.h` / `src/PluginEditor.cpp` — `AttachmentWitness`, `makeWitness`, and the three
  attachment sites that straddle it (`attachSlider`, `setupCombo`, `setupToggle`)
- `src/PresetManager.h` / `.cpp` — `deferIfBusy`, and the five entry points that ask it (`load`,
  `loadAdopted`, `loadFile`, `step`, `saveUser`)
- `src/gui/SpectrumImager.h` / `.cpp` — `onOwnedWrite`, `onOwnedRefused`, `onUserTransaction`,
  `ScopedUserTransaction`, `storeOwned`, `setParam`, `addBandAt`, `removeBand`, `resetCrossover`,
  `commitFreqEditor`, `spreadSplits`
- `src/PluginParameters.h:65-88` (view/preset exclusion lists)

Evidence [Verified]:
- Source: src/PluginProcessor.cpp:470-847, :340-520
- History [Partially Verified]: CHANGELOG.md [0.6.x and earlier] (0.5.1, "Replaces JUCE's global undo manager")
