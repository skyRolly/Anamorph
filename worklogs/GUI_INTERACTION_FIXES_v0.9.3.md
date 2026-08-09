# Multiband add-split line stall + Settings drop-down dismissal (v0.9.3)

> Two maintainer-reported GUI interaction bugs. Both turned out to be one-line-class fixes sitting on
> top of non-obvious mechanisms — a repaint optimisation that could not see the moving thing, and a
> JUCE modal rule that deliberately re-delivers the click it just consumed. This worklog keeps the
> traces; `CHANGELOG.md` keeps the user-facing statements.

- **Date:** 2026-08-09 · **Version:** 0.9.3 (PR #101, commit `7afd07e`) · **Branch:** `claude/beautiful-sagan-JAUFI`.
  PR #100 (v0.9.2) is merged and is this branch's merge-base — it is not the source of these fixes.
- **Reference tree:** JUCE 9.0.0 at the pinned commit `f8f8864…` (`CMakeLists.txt:36-38`), fetched
  and read locally; all JUCE line citations below are against that commit.
- **Scope:** editor only. No parameter, serialization or DSP change — 0.9.3 is state- and
  sound-identical to 0.9.2, which is why neither fix has an automated regression test (see §3).

---

## 1. The Multiband "add split" preview line stalls while the pointer moves

**Report.** In Advanced mode, hovering the Multiband spectrum shows a preview line marking where a
click would add a split. Moving the mouse, the line sometimes freezes at one position; it unsticks
and jumps to the cursor once the pointer reaches something else that redraws — a Solo headphone, a
split handle. Reported on macOS; suspected to be a consequence of an earlier rendering optimisation.

**The suspicion was right, and the mechanism is exact.** `SpectrumImager` does not repaint from its
mouse handlers; it repaints from the **S2 gate** at the end of the vblank tick:

```
if (dataMoved || uiMoved || frameDirty) { frameDirty = false; repaint(); }
```

`dataMoved` tracks the spectrum magnitudes and the red-level decay. `uiMoved` is set by the `ease()`
lambda — an eased **alpha** actually moved — and by a change in the drawn split/width positions or
the band count. The gate's own comment states the assumption it rests on: the frame is a pure
function of that state *"plus mouse-driven fields whose handlers already repaint explicitly"*.

`updateHover()` is the handler that did not. It writes `hoverHandle`, `hoverWidth`, `hoverAdd`,
`hoverDelete`, `hoverDeleteExact`, `hoverSolo` **and `addX`** — and calls neither `repaint()` nor
sets `frameDirty`. Of those, only the indices feed eased alphas; **`addX`** — the preview line's X,
read by `paint()` — feeds nothing the gate watches.

So the freeze needs three things at once, which is why it is intermittent rather than constant:

1. the pointer moving **within one band's add zone**, so `hoverAdd` does not change and `addA` has
   already eased to 1.0 and stopped moving;
2. no other hover index changing, so no other alpha moves;
3. the spectrum **settled** — silence, or decays finished — so `dataMoved` is false.

All three hold on a quiet track with the cursor sweeping one band. The gate stays shut, the last
painted X stays on screen, and the line sits still under a moving cursor. Cross a band boundary or
touch a Solo hotspot and an alpha target changes, `uiMoved` goes true, and the line jumps to the
cursor — the reported "unstick".

**Fix.** `updateHover` snapshots what it may change, and sets `frameDirty` when any of it did.
`frameDirty` rather than `repaint()` on purpose: the imager's `FrameClock` runs whenever the
component is visible (`visibilityChanged()` starts/stops it) and a hidden component receives no mouse
events, so the flag is always consumed by the next vblank tick — which keeps painting at one frame
per vblank instead of one per mouse event, i.e. the thing the gate exists for. The `!isShowing()`
early-return at the top of `tick()` does not clear the flag, so a hide/show cannot swallow it.

**What was deliberately not changed.** The gate itself. An idle, settled view still stops repainting
exactly as before; the fix only adds one more input to "did anything move". `mouseExit` was left
alone too: it clears the hover indices, which moves their alphas on the next tick, so it already
repaints through `uiMoved`.

**Checked for the same class elsewhere.** `scrollHandle` / `scrollBand` are the only other fields the
mouse handlers write without repainting, and `paint()` does not read them — they are gesture state.

## 2. A Settings drop-down's dismissing click also closed Settings

**Report.** With a drop-down open in Settings: clicking inside the Settings panel but outside the
drop-down closes only the drop-down (Settings stays); clicking **outside** the panel closes the
drop-down **and** Settings. Wanted: while any drop-down is open, one click anywhere outside it closes
only the drop-down, wherever it lands. With no drop-down open, behaviour unchanged.

**Why one click did two things.** Read out of the pinned tree, `Component::internalMouseDown`
(`juce_Component.cpp:2507-2544`):

```
if (target->isCurrentlyBlockedByAnotherModalComponent())
{
    target->flags.mouseDownWasBlocked = true;
    target->internalModalInputAttempt();          // <- dismisses the menu
    if (checker.shouldBailOut()) return;
    // If processing the input attempt has exited the modal loop, we'll allow the event
    // to be delivered.
    if (target->isCurrentlyBlockedByAnotherModalComponent()) { ...; return; }
}
...
target->mouseDown (me);                           // <- and then delivers it anyway
```

`internalModalInputAttempt()` dismisses the pop-up; `ModalItem::cancel()` clears `isActive`
synchronously (`juce_ModalComponentManager.cpp:81-89`), so the block is gone by the second test and
JUCE **deliberately** delivers the same mouse-down to the component underneath. That component is
`Backdrop`, whose `mouseDown` dismisses the panel for any click outside `panel` — hence both.

The inside-the-panel case only differed because the click missed `panel`'s dismiss test; the pop-up
was being dismissed by the same mechanism there too.

**Fix, and why the test is exact rather than a heuristic.** `Backdrop` gained an optional
`swallowsDismissClick` predicate, consulted first; for `settingsBackdrop` it returns true when any
box in the editor's existing `allCombos` reports `isPopupActive()`. Two facts make that precise:

- **It is still true at that moment.** `menuActive` is cleared by `comboBoxPopupMenuFinishedCallback`
  → `ComboBox::hidePopup()`, a **modal callback**, and `ModalComponentManager` dispatches those
  asynchronously — `cancel()` only `triggerAsyncUpdate()`s. Our handler runs synchronously inside the
  very mouse-down that dismissed the menu, strictly before that callback.
- **It cannot be a false positive.** If a menu were *still* modal we would never be called at all:
  `internalMouseDown` returns at `:2517-2522` while the block is in force. So a live flag inside our
  `mouseDown` can only mean "this click just closed it".

**And it cannot get stuck**, which matters because the backdrop click is the *only* way to close
Settings — `settingsButton` opens it and sits behind the backdrop. `menuActive` is cleared by
`~ComboBox` and by `enablementChanged()` when a box is disabled, and `showPopup()` early-returns
**before** setting it when disabled, so the "deferred `showPopup` on a now-disabled box" path leaves
it false rather than true. These boxes are never disabled in any case.

`allCombos` rather than naming `oversampleBox`/`uiScaleBox`: it already exists, and it stays correct
if a drop-down is added to the panel later. A combo elsewhere in the editor cannot have a menu open
while the backdrop is up — the backdrop covers the editor and eats the click that would open one —
and swallowing a stray click would be the safe direction anyway.

**Not wired to the other backdrops.** About and Save Preset host no combo, and only one backdrop is
visible at a time. The hook is opt-in and empty elsewhere.

## 3. No automated regression tests, and why

Both defects live entirely in the editor. `tests/state_tests.cpp` links the editor but never
instantiates it, and neither suite has a mouse or a display: bug 1 needs a real vblank tick plus
pointer motion over a settled spectrum, bug 2 needs JUCE's modal machinery to deliver a real click.
This is the **ADR-0025** exception, invoked with its four required disclosures:

1. **Why no test exists** — no automated surface reaches either defect; the harness instantiates no
   editor and drives no pointer.
2. **What replaced it** — root causes traced to specific lines in the pinned JUCE and in our own
   render gate, with the exact reachability conditions written down above, plus a manual check per
   platform at the Level-5 audition.
3. **Where the gap is tracked** — `docs/procedures/TESTING.md` §"Gaps in the automated coverage",
   the same register KI-014 and RH-F3 use.
4. **Whether infrastructure could close it** — yes, and it is the same infrastructure INC-010 already
   needs: a headless harness that instantiates the editor and drives synthetic mouse events. Until
   that exists these two join the same list rather than being waived silently.

**Manual verification — performed and signed off by the maintainer, 2026-08-09.** Both checks the
headless suites cannot stand in for: bug 1 on macOS with a silent track, sweeping the pointer within
one band; bug 2 on each platform, including the edge cases enumerated in §2. This closes ADR-0025
disclosure 2 for these two entries — the exception is discharged, not merely declared.

It signs off **these two fixes only**. The `RELEASE_POLICY` preconditions that still gate a tag are
untouched by it: the Level-5 **audio** audition and the `RELEASE_COMPATIBILITY_CHECKLIST` remain
open, as `HANDOVER.md` §Release Status records.
