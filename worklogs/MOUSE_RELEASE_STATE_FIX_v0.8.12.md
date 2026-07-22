# Mouse release-outside stuck-state fix (v0.8.12 GUI interaction)

> A control (knob, slider, or the MultiBand drag) could stay stuck in the pressed/dragging state if
> the physical mouse button was released **outside** the plugin window. Root cause: the host delivers
> that mouse-up over itself/the desktop, so JUCE never routes it to the editor and its cached button
> state stays stale-"down". Fix: reconcile against the **real OS button state** on the 24 Hz timer and
> in the micro-anim glow, gated so the OS is only queried while a button actually appears held.

- **Date:** 2026-07-21 · **Version:** 0.8.12 (PR #80) · **Branch:** `claude/beautiful-sagan-JAUFI`.

## 1. Problem

Press-and-hold a knob / slider / MultiBand handle, drag the cursor outside the plugin window, release
the button there. The plugin often never receives the `mouseUp` (the OS routed it to whatever was under
the cursor). The control then stays visually lit (press glow) and logically "held" — and JUCE's own
cached modifier state (`ModifierKeys::getCurrentModifiers()`, `isMouseButtonDownAnywhere()`) stays
stale-"down" until the next real event, so it can persist even after the cursor returns.

## 2. Root cause

Two independent stale-state sources feed the pressed appearance:

- **`juce::Slider::isMouseButtonDown()`** — a knob-body drag; cleared by JUCE only on `mouseUp`.
- **the `"dragging"` component property** — set by the knob's value-box `mouseDown`
  (`LookAndFeel.cpp:749`) and cleared in its `mouseUp` (`:758`); the persist-bar uses `persistDragging`
  via `onDragStart/onDragEnd`.

Both the LookAndFeel press glow and the micro-anim `actA` read `isMouseButtonDown() || "dragging"`. The
MultiBand editor separately holds `dragBand`/`dragHandle`/`soloPressBand`/`pressDeleteBand`. When the
`mouseUp` is lost, none of these clear.

`getCurrentModifiersRealtime()` — unlike the cached modifiers — queries the OS for the **actual**
current button state on **Linux/X11 (`XQueryPointer`) and Windows (`GetAsyncKeyState`)**, so it is
reliable there even when no JUCE event arrived; crucially it also **writes back** to JUCE's cached
`currentModifiers`, so one reconcile pass re-synchronizes everything that reads the cached state.
**Platform caveat (verified in the vendored JUCE 8):** the macOS implementation refreshes only the
*keyboard* modifiers and returns the *cached* mouse-button flags — it never queries
`[NSEvent pressedMouseButtons]` — so on macOS the reconcile's "cached-down && realtime-up" gate can
never be satisfied and the whole mechanism is deliberately inert: behaviour there equals the pre-fix
state (no regression; macOS also loses outside releases far less often, since AppKit delivers the
mouse-up to the window that captured the mouse-down). The recovery therefore covers Windows/Linux.

## 3. Fix (minimal, message-thread only)

Two small additions in `src/PluginEditor.cpp`, plus one method in `SpectrumImager`:

1. **`timerCallback` (24 Hz) reconcile.** Pre-gate on `juce::Component::isMouseButtonDownAnywhere()`
   (cheap, no syscall) — a lost-up leaves it stale-true, a genuine idle leaves it false. Only when it
   is true do we ask the OS: if `getCurrentModifiersRealtime().isAnyMouseButtonDown()` is false, the
   button is really up, so clear every animated widget's stuck `"dragging"` flag (and repaint it), drop
   `persistDragging`, and call `imager->cancelActiveDrag()`. During a real drag the button is genuinely
   down, so the block is inert (the OS is queried, returns true, nothing changes). Because the realtime
   query refreshes the cached modifiers, only the FIRST tick after a lost release does any work — the
   gate then reads false and goes quiet (no repaint storm).
2. **`stepMicroAnims` glow gate.** `buttonHeld = (isMouseButtonDown() || "dragging") &&
   physicalButtonDown()`, where `physicalButtonDown()` is a **lazy** query of the real OS state —
   evaluated at most once per call and only when a slider actually reports held (short-circuit), so
   there is **no cost during hover/idle and no press-onset lag** (a fresh query, not a cached flag, so
   a genuine press lights instantly). The stale glow clears within a frame of the real release.
3. **`SpectrumImager::cancelActiveDrag()`** — ends any open parameter gesture
   (`widthP[dragBand]` / `freqP[dragHandle]` / the band-move `freqP` edges via `endBandMove`) and
   clears the drag flags **without** firing the on-release actions (delete band / toggle solo / commit
   a band move): a lost release is not a deliberate release-over-target. Early-returns when nothing is
   active.

**Why no double-gesture / no regression:** `cancelActiveDrag` and the imager's own `mouseUp` both run
on the message thread (no interleaving) and both guard on the same `dragBand`/`dragHandle`/... flags,
so whichever runs first, the other is a no-op — a parameter's `endChangeGesture` is never called twice.
The reconcile can only fire when the OS says the button is up, so it can never interrupt a live drag —
including a legitimate drag whose cursor is outside the window with the button still held. The glow
gate only *removes* a press indication when the button is really up; during a real press the OS says
down, so behaviour is identical.

**Files:** `src/PluginEditor.cpp` (timerCallback reconcile + stepMicroAnims gate),
`src/gui/SpectrumImager.cpp`/`.h` (`cancelActiveDrag` + declaration). No DSP / serialization / parameter
change; the OS query runs on the message thread only, and only while a button appears held.

## 4. Validation

- **Build:** `Anamorph_VST3` + `AnamorphTests` clean (exit 0); `PluginEditor.cpp` / `SpectrumImager.cpp`
  compiled warning-free.
- **DSP suite:** `AnamorphTests` — **140 checks, 0 failures** (no DSP touched).
- **Independent review:** a 4-lens adversarial Workflow — 3 lenses completed (width-drag,
  release-outside, gesture-safety), **no blocker/major/minor defect**: press-onset verified lag-free;
  a live drag (including one whose cursor leaves the window with the button held) can never be
  interrupted; double-`endChangeGesture` proven impossible (message-thread serialization + shared flag
  guards); only the first reconcile tick repaints; the Persist-bar reveal is unaffected during a normal
  drag; `getCurrentModifiersRealtime()`'s write-back to the cached modifiers self-heals the idle gate.
  The 4th (fresh-eyes) lens was lost to the org token limit and carried inline: index bounds hold
  (`dragBand` ∈ [0,3] vs `widthP[4]`, `dragHandle` ∈ [0,2] vs `freqP[3]`), all API signatures proven by
  the clean compile, and touch input is safe (all three platforms synthesize primary-contact touch as
  OS mouse-button state, so the realtime query reports "down" during a touch drag and the reconcile
  stays inert).
- GUI mouse interaction has no unit test (the suite is a DSP console app); verified by the event-path
  reasoning above and the adversarial review.

## 5. Follow-up: the rotary press glow could stay lit (idle-gate seal ordering)

A review found one residual edge: after a lost outside-release, the rotary **press glow** (`actA`)
could remain at full brightness until the cursor re-entered the window.

**Root cause — a message-thread ordering hole in the fix above.** With the cursor outside and the
glow settled at `actA = 1.0`, the micro-anim driver is idle (`microSettled = true`). On a lost
release, if the **24 Hz reconcile tick runs before the next vblank `stepMicroAnims` pass**, its
`getCurrentModifiersRealtime()` call refreshes JUCE's cached modifiers — flipping
`isMouseButtonDownAnywhere()` false — and `stepMicroAnims`' S11 idle gate (cursor outside + no
button + settled + generations unchanged) then early-returns **before its widget loop ever saw the
release**: the `physicalButtonDown()` AND-gate that would have eased `actA` to 0 never executes, and
nothing re-opens the gate until `mouseInside` becomes true again. (The vblank-first ordering escapes:
the pass runs while the cached flag is still stale-true, starts the ease, and the in-flight motion
keeps `microSettled = false` until converged.) The same seal affected the value-box case: the
reconcile cleared its `"dragging"` flag and repainted, but the repaint draws the **eased** `actA` —
still 1.0.

**Fix (one line).** The reconcile block also sets `microSettled = false`. That re-opens both idle
gates for exactly one pass; the widget loop runs, sees the physical button up, and eases
`actA`/`hovA` down exactly as a normal release would (repainting only as values move), then
re-settles — every gate closes again by itself. No gate is removed, no frequency raised, no
continuous repaint: the extra cost is the ~150 ms ease a normal release always costs, and the block
still cannot fire during a live drag (the OS reports the button down). **Confirmed visual-only:** the
parameter gestures were already safe — imager gestures close in `cancelActiveDrag`, and a JUCE
`Slider`'s own drag/gesture ends via the mouseUp JUCE synthesizes on the next real mouse event; only
the eased glow had no wake-up path.

**Independent verification (2-agent adversarial Workflow, both `CONFIRM`):** root cause and fix
sufficiency confirmed in both message-thread orderings — `microSettled` is ANDed into **both** idle
gates, so the woken pass cannot be blocked by the FNV probe gate either; the ease is self-sustaining
(`microSettled = !anyMotion`) until convergence; a no-op fire costs exactly one no-repaint pass and
the realtime write-back quiets the pre-gate on the next tick (one-shot, no storm). Accepted narrow
caveats, none introduced by this line: (a) the **macOS inertness** above (pre-existing at the
mechanism level; behaviour unchanged there); (b) a **~42 ms release-then-repress race** — if a new
physical press lands anywhere between the timer wake and the first woken vblank pass, the glow
re-seals at full until the cursor re-enters the editor (recoverable, extremely narrow); (c) the
pre-existing terminal-ease residual (`actA` stalls at ~0.01–0.03 because the 0.0015 no-op threshold
beats the 0.004 snap at 60+ Hz) — identical for every normal release, painted contribution
≤ ~2/255.
