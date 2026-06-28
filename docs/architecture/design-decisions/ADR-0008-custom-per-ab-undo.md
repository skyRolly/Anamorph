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
name + dirty baseline). The editor's 24 Hz timer calls `pollUndoCoalesce()`, which commits one
step only once a sound edit has **settled** (signature stable for a tick), folding a whole
gesture into a single entry. **View params** (`pid::viewParams` = Bypass; plus Settings, which
are host-hidden) and A/B switches themselves are never recorded. `requestDuck()` masks the level
jump on undo/redo. Undo stacks are cleared on session restore.

## Consequences
- Per-slot histories survive editor close (the A/B model lives in the processor).
- A `soundSignature()` over non-view params drives coalescing.
- Cost: a hand-rolled history with a 128-entry cap per slot.

## Related code
- `src/PluginProcessor.cpp:162-296` (signature, coalesce, undo/redo, A/B)
- `src/PluginProcessor.h:55-115` (StateSet, UndoStacks, A/B members)
- `src/PluginParameters.h:64-87` (view/preset exclusion lists)

Evidence [Verified]:
- Source: src/PluginProcessor.cpp:162-296
- History [Partially Verified]: README:258-262 (0.5.1, "Replaces JUCE's global undo manager")
