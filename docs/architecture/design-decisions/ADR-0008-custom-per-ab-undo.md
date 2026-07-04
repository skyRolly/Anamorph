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
name + dirty baseline). The editor's timer calls `pollUndoCoalesce()`, which is **gesture-gated**:
the processor listens to parameter begin/end gestures and commits exactly one step after the last
gesture closes, folding a whole drag into a single entry; **host automation** (which opens no
gesture) folds into the baseline **without** a step. A **preset load** also opens no gesture, so it
is bracketed explicitly (`PresetManager::onAboutToLoad` flushes a settled edit, `onLoaded` records
one step via `commitPresetSwitchUndoStep`) — a preset switch is a discrete, undoable action.
**View params** (`pid::viewParams` = Bypass; plus Settings, which are host-hidden) and A/B switches
themselves are never recorded. `requestDuck()` masks the level jump on undo/redo. Undo stacks are
cleared on session restore.

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
- Cost: a hand-rolled history with a 128-entry cap per slot.

## Related code
- `src/PluginProcessor.cpp:162-296` (signature, coalesce, undo/redo, A/B)
- `src/PluginProcessor.h:55-115` (StateSet, UndoStacks, A/B members)
- `src/PluginParameters.h:64-87` (view/preset exclusion lists)

Evidence [Verified]:
- Source: src/PluginProcessor.cpp:162-296
- History [Partially Verified]: CHANGELOG.md [0.6.x and earlier] (0.5.1, "Replaces JUCE's global undo manager")
