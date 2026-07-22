# MultiBand Bandwidth drag-behaviour fix (v0.8.12 GUI interaction)

> A bare mouse **press** on a band's Width ("Bandwidth") line changed the value immediately (the
> divider snapped to the click position and the parameter was written) instead of waiting for an
> actual drag. Fixed so a press only *begins* the interaction; the Width updates on the first
> `mouseDrag`, using the existing drag mapping. Minimal event-state correction — one line removed.

- **Date:** 2026-07-21 · **Version:** 0.8.12 (PR #80, commit `c0cbd05`) · **Branch:** `claude/beautiful-sagan-JAUFI`.

## 1. Original issue

In the MultiBand spectral editor, clicking-and-holding on a band's horizontal width line
immediately set the Width to the cursor's vertical position — so a click without dragging (or a
click landing a few pixels off the line, within the 8 px grab tolerance) moved the divider and wrote
the parameter. Expected: a press begins the interaction; only a drag changes the value.

## 2. Root cause

`SpectrumImager::mouseDown` (`src/gui/SpectrumImager.cpp`) had, in the width-grab branch:

```cpp
if (nearWidthLine (p, b))
{
    dragBand = b; dragHandle = -1;
    beginGesture (widthP[b]);
    setParam (widthP[b], yToWidth (p.y));   // <-- wrote the value on PRESS
    repaint();
    return;
}
```

The `setParam(...)` on `mouseDown` is the whole bug: it maps the click Y straight to a Width value
before any movement. By contrast the **vertical crossover-handle** branch a few lines above already
did the right thing — it begins the gesture on press and only moves the split in `mouseDrag`
(`dragCrossoverTo`). The width path was the sole interaction writing its parameter on the press.

`mouseDrag` already writes the width from the live cursor, identically:

```cpp
else if (dragBand >= 0)
    setParam (widthP[dragBand], yToWidth ((float) e.position.y));
```

So the press-time write was purely redundant with (and premature relative to) the drag write.

## 3. Fix (minimal event-state correction)

Remove the single `setParam(widthP[b], yToWidth(p.y))` line from `mouseDown`. The press now only
sets `dragBand = b` and calls `beginChangeGesture()`; the value is written by `mouseDrag` on the
first movement (existing calculation, mapping and smoothing all untouched). `mouseUp` still calls
`endChangeGesture()`, so a click that never drags produces an **empty** begin/end gesture — no value
change, no automation write, no undo step — exactly as the crossover handle already behaved.

**Files changed:** `src/gui/SpectrumImager.cpp` (one line removed + an explanatory comment).
Nothing else touched — no DSP, no parameter mapping, no serialization, no automation semantics, no
other MultiBand interaction (crossover drag, add/remove band, solo, alt-click reset, scroll-wheel
Width, double-click all remain as-is).

## 4. Why it is minimal and safe

- The drag path (`yToWidth(e.position.y)`) is **unchanged**, so "once dragging starts, existing drag
  behaviour is unchanged" holds by construction — the removed press-write used the identical mapping.
- The pattern now **matches the crossover handle** (press = begin gesture, drag = change, release =
  end gesture), which has shipped this way for many versions — so the change moves the width path
  onto a proven contract rather than inventing one.
- Risk cases: **click, no move** → empty gesture, no change; **click + tiny move** → drag begins,
  width tracks the cursor (same as the crossover, bounded by the 8 px grab tolerance); **normal drag /
  release** → untouched; **multiple bands** → per-band `widthP[b]`, unchanged; **alt-click reset /
  scroll Width** → separate branches, untouched.

## 5. Validation

- **Build:** `Anamorph_VST3` + `AnamorphTests` build and link **clean** (exit 0). `SpectrumImager.cpp`
  — the only changed file — compiled **warning-free**. (The full rebuild surfaced the codebase's
  pre-existing baseline warnings in `ScopeBuffer.h` / `PluginParameters.cpp` / `AnamorphEngine.cpp` /
  `VelvetNoise.cpp` / `PluginProcessor.cpp`; those files are byte-identical to `main`, i.e. **no new**
  warnings — they only re-emitted because the v0.8.12 version-string bump forced a full recompile.)
- **DSP suite:** `AnamorphTests` — **140 checks, 0 failures**, identical to baseline (no DSP touched).
  GUI mouse interaction has no unit test (the suite is a DSP console app); the fix is verified by the
  event-path reasoning above and by matching the established, long-shipped crossover-handle contract.
- **Behaviour:** press-without-drag writes nothing (empty begin/end gesture); drag writes the Width via
  the unchanged `yToWidth` mapping; divider motion and every other MultiBand interaction are unchanged.

## 6. Follow-up (same v0.8.12): relative drag + click-vs-drag threshold

A second pass refined the drag itself (the click fix above left the drag ABSOLUTE — it still snapped
Width to the cursor Y on the first move). The drag is now **relative** and gains a 3 px engage
threshold, mirroring the crossover handle:

- `mouseDown` remembers `widthPressY` and leaves `widthHoldActive = false`; still no value write.
- `mouseDrag`: once `|e.position.y - widthPressY| > 3 px`, it engages and anchors
  `dragGrabDY = e.position.y - widthToY(bandWidth(dragBand))` **at that moment**, then writes
  `yToWidth(e.position.y - dragGrabDY)`. Because the anchor is taken at engagement against the
  still-original Width, the very first write is `yToWidth(widthToY(origW)) = origW` **exactly**
  (`yToWidth∘widthToY` is the identity on the 0–2 range — verified by the adversarial review) — no
  jump to the absolute cursor, no step at the 3 px boundary — and thereafter the value follows the
  mouse delta (the line stays attached to the grabbed point). Below the threshold nothing moves, so a
  click or hand jitter never nudges Width. Anchoring at engagement (not at press) is what avoids a
  ~3 px step.
- `mouseUp` and `cancelActiveDrag` reset `widthHoldActive`; the flag is only consulted while
  `dragBand >= 0`, and the same `mouseDown` that sets `dragBand` also clears it, so it can never be
  stale-true.

This is the grab-offset + 3 px gate the vertical crossover already used (`dragGrabDX` /
`handleHoldActive`); parameter mapping and smoothing are unchanged. It also composes correctly with
the divider visual: `busy` (`dragBand >= 0`) snaps `drawnW` to the live parameter every frame, which
now holds the original value until the drag engages — so the line holds still on press, exactly the
desired feel. Full record of the accompanying release-outside stuck-state fix:
`worklogs/MOUSE_RELEASE_STATE_FIX_v0.8.12.md`.
