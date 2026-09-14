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
  `noteOwnedParamRefused`, `resetBatchOwnership` (`snapshotSoundValues` was deleted in round 17 —
  both of its call sites were the defect)
- `src/PluginProcessor.h` — `StateSet`, `ParamEdit`, `UndoEntry`, `UndoStacks`, `kUndoDepth`,
  `batchOpenValue` / `batchCloseValue` / `batchOwnedParam` / `batchEpisodeParam`, the A/B members
- `src/PluginEditor.h` / `src/PluginEditor.cpp` — `AttachmentWitness`, `makeWitness`, and the three
  attachment sites that straddle it (`attachSlider`, `setupCombo`, `setupToggle`)
- `src/gui/SpectrumImager.h` / `.cpp` — `onOwnedWrite`, `onOwnedRefused`, `storeOwned`, `setParam`,
  `resetCrossover`, `commitFreqEditor`, `spreadSplits`
- `src/PluginParameters.h:65-88` (view/preset exclusion lists)

Evidence [Verified]:
- Source: src/PluginProcessor.cpp:426-565, :340-520
- History [Partially Verified]: CHANGELOG.md [0.6.x and earlier] (0.5.1, "Replaces JUCE's global undo manager")
