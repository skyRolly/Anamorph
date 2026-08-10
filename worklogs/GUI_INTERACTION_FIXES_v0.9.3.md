# Multiband add-split line stall + unified pop-up dismissal, and two menu-rendering fixes (v0.9.3)

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


---

## 4. Unified pop-up dismissal — one shield instead of per-control predicates

§2 fixed the Settings case with a predicate on the Settings backdrop: "was a ComboBox pop-up open
when this click arrived". Verification then found the same defect on the **Save Preset** dialog, and
it is worse there — it destroys work.

**The Save Preset case.** `saveNameEditor` is a `juce::TextEditor`; `popupMenuEnabled` defaults true
(`juce_TextEditor.h:817`) and nothing in `src/` turns it off, so a right-click opens a modal
`PopupMenu` (`juce_TextEditor.cpp:1567-1593`). The gate on that branch is
`if (wasFocused || ! selectAllTextWhenFocused)`; we set `setSelectAllWhenFocused (true)` and
`showSavePreset` focuses the field, so `wasFocused` is true and the menu opens. Dismissing it with a
click outside the panel re-delivered that click to `savePresetBackdrop`, whose `onDismiss` is
`showSavePreset (false)` — **the dialog closed and the typed name was gone**.

**Why the §2 predicate could not be extended to it.** `TextEditor::menuActive` is private with no
accessor, and a `TextEditor` is not a `ComboBox`, so `allCombos` sees nothing. The ComboBox fix
worked only because `ComboBox::isPopupActive()` happens to be public.

**Why there is no universal JUCE signal.** All three candidates were read in the pinned tree and all
three fail:

| Candidate | Why not |
|---|---|
| `Component::flags.mouseDownWasBlocked` | private — *and* reset to `false` at `juce_Component.cpp:2525`, before delivery |
| `Component::getNumCurrentlyModalComponents()` | counts only `isActive` items (`juce_ModalComponentManager.cpp:155-163`), and `ModalItem::cancel()` clears that synchronously (`:81-89`) — reads **0** inside our handler |
| `PopupMenu` | only `dismissAllActiveMenus()` is public; `getActiveWindows()` is private |

So the state has to be ours. What makes any such state work is one uniform property: the
**dismissal** is synchronous, but everything that *clears* pop-up state runs from the
**asynchronous** modal callback. So our state still says "open" during the pass-through click, and
can never be a false positive — a genuinely-still-modal pop-up means `internalMouseDown` returned at
`:2517-2522` and we were never called.

**The mechanism: two feeders, one flag, one enforcement point.**

- `AnamorphLookAndFeel::preparePopupMenuWindow` — JUCE calls this from the `MenuWindow` constructor
  (`juce_PopupMenu.cpp:500`) on the **menu's own** look-and-feel. Both `ComboBox` (`:561`) and
  `TextEditor` (`:1578`) set that to ours, so this one hook catches every menu we did not create.
  All three look-and-feel instances (`lnf`, `compactCombo`, `simpleCombo`) are wired, because a menu
  carries the look-and-feel of the box that opened it.
- **The preset menu is tracked directly**, because it is the one menu this hook cannot see:
  `findLookAndFeel` returns `menu.lookAndFeel.get()` (`:1422-1425`), which is null since INC-010
  deliberately dropped its `setLookAndFeel`, and the `lf` used at `:500` is captured at `:368`
  *before* parenting — so JUCE resolves the **default** look-and-feel there, not the one it would
  inherit. Restoring `setLookAndFeel` would re-arm the `~LookAndFeel` assertion INC-010 removed, so
  the counter is the cheaper answer.
- **`PopupShield`** — a transparent full-editor child that overrides `mouseDown`/`Up`/`Drag`/
  `DoubleClick` to do nothing. While it is up it *is* the component the pass-through click lands on.

**Why a shield rather than more predicates.** The contract is "the dismissing click must not act on
anything underneath", and *underneath* is any control the cursor happens to be over. Several act on
the press itself: `ABControl::mouseDown` toggles A/B, `SpectrumImager::mouseDown` can **add a band**,
a `Backdrop` closes its panel. A predicate per control is N places to get right and N places to
forget; one shield is one place, and it covers controls added later for free. The §2 predicate was
removed rather than kept alongside it — two mechanisms for one rule is how they drift apart.

**The z-order is structural, not incidental**, which is what makes the shield safe to ship without a
GUI test. `PopupMenu::MenuWindow` sets `setAlwaysOnTop (true)` in its constructor
(`juce_PopupMenu.cpp:365`), and `Component::toFront` on a component **without** that flag walks its
insert index back past every always-on-top sibling (`juce_Component.cpp:914-922`). The shield does
not set the flag, so it *cannot* be raised in front of a menu — even if it were raised while one was
already open. Nothing in `src/` sets `alwaysOnTop`, so a menu window is the only sibling that can
outrank it. `showPresetMenu` also raises the shield before `showMenuAsync`, so the append order
agrees with the flag order; either alone would be sufficient.

**Focus is left alone**, which matters precisely for the case that motivated this: `toFront (false)`
skips `grabKeyboardFocus` (`juce_Component.cpp:928-934`), and `setMouseClickGrabsKeyboardFocus
(false)` covers the click, so raising the shield cannot pull focus out of the Save Preset field
mid-edit.

**Lowering it.** `componentBeingDeleted` on each tracked window lowers it the instant the window
dies — prompt enough that a fast second click is never swallowed — and `refreshPopupShield()` also
runs from the 24 Hz tick as a backstop. That backstop is not decoration: a shield stuck visible would
make the entire editor unclickable, so it is worth a scan of an almost-always-empty array to make
that state unreachable. The array holds `Component::SafePointer`s, so an entry drops out on its own
even if a notification were ever missed.

## 5. Menu width: measured from what the drawing actually spends

`getIdealPopupMenuItemSize` allowed `textWidth + 30`. `drawPopupMenuItem` spends `12 + 14 + 12 = 38`
on chrome *before* the text has any room — `r.reduced (12, 0)` on both edges plus a 14 px tick
gutter — so every item was measured **8 px narrower than it draws**, and JUCE clipped the longest
one. In the Save Preset field's context menu that is *"Select All"*, which appeared as
*"Select ..."*.

The fix is not a bigger number but a *derived* one: the padding, gutter and trailing room are named
constants that the measuring code now sums, so the two halves cannot drift apart again. That is also
what makes it portable — the failure was never font-specific, but a fixed width would have been.
`getPopupMenuFont()` is virtual, so the compact and Simple variants measure in their own font; and
JUCE passes `item.text + "   " + shortcut` for measurement (`juce_PopupMenu.cpp:333-336`), so the
right-aligned shortcut column is covered by the same measurement.

The 64 px floor is a floor, not a preference: it sits just above the 50 px chrome total, so it cannot
widen a pop-up past the control that opened it. The combo path keeps its own, larger floor —
`getOptionsForComboBoxPopupMenu` passes `withMinimumWidth (box.getWidth())`.

## 6. Disabled menu items look disabled

`drawPopupMenuItem` took `bool /*isActive*/` and ignored it, so a greyed-out entry rendered exactly
like a live one — visible in the Save Preset context menu, where *Cut*/*Copy* are inactive with no
selection and *Paste* is inactive with an empty clipboard. The flag is now honoured for the label,
the tick, the shortcut and the sub-menu arrow, at **0.4** alpha — the disabled alpha this same file
already uses for a disabled button (`drawButtonText`), so the menu matches the rest of the UI instead
of inventing a shade. The highlight fill is also suppressed for an inactive row, so "cannot be
chosen" and "looks choosable" can never contradict each other. Enabled rendering is byte-identical.

## 7. Verification status for §4-§6

Same constraint as §3: all three are editor-only, and the harness instantiates no editor and drives
no pointer. They join the existing **ADR-0025** entry rather than getting one of their own.

**Maintainer sign-off on record (2026-08-09):** the Settings click-through and the Save Preset
context-menu data loss are confirmed real, and the one-click-only dismissal contract is approved.
That sign-off is on the **problem reports and the required contract** — it is not a manual test of
this implementation, and it does not touch any release gate.

**Manual checks owed on the implementation**, none of which a headless suite can stand in for: a
ComboBox drop-down (inside click selects, outside click only dismisses); the Settings panel (the
dismissing click does not reach a control); the Save Preset field (right-click menu works, clicking
outside closes only the menu, typed text survives); the preset menu (dismissal triggers nothing
underneath); `SpectrumImager` (a dismissing click cannot add a band); `ABControl` (cannot toggle); a
second click after dismissal behaving normally; and visually, *Select All* shown in full with
disabled items clearly dimmer, at more than one UI scale.
