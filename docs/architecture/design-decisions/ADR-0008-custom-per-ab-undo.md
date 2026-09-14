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
  `undo`/`redo`, `commitPresetSwitchUndoStep`, `abCopyToOther`, `parameterGestureChanged`,
  `snapshotSoundValues`, `noteOwnedParamWrite`, `resetBatchOwnership`
- `src/PluginProcessor.h` — `StateSet`, `ParamEdit`, `UndoEntry`, `UndoStacks`, `batchOpenValue` /
  `batchCloseValue` / `batchOwnedParam`, the A/B members
- `src/gui/SpectrumImager.h` / `.cpp` — `onOwnedWrite`, `storeOwned`, `setParam`, `resetCrossover`,
  `commitFreqEditor`, `spreadSplits`
- `src/PluginParameters.h:65-88` (view/preset exclusion lists)

Evidence [Verified]:
- Source: src/PluginProcessor.cpp:425-494, :340-520
- History [Partially Verified]: CHANGELOG.md [0.6.x and earlier] (0.5.1, "Replaces JUCE's global undo manager")
