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

> **Superseded within this same PR — see §4.** The **Report** and the **Why one click did two things**
> analysis below still stand and are the origin of the whole pop-up work. The **fix** described here
> — `Backdrop::swallowsDismissClick` + a `ComboBox::isPopupActive()` predicate — was **removed** and
> replaced by the editor-level `PopupShield` once the same defect turned up on a `TextEditor` context
> menu (INC-011), which a `ComboBox`-shaped predicate cannot express. Nothing named
> `swallowsDismissClick` exists in `src/`; read §4 for the shipped implementation. The design record
> below is kept because §4's rationale is a direct answer to it.

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

**Fix as first attempted — SUPERSEDED by §4, not in `src/`.** `Backdrop` gained an optional
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

*(End of the superseded design. **What actually ships is §4.** That last paragraph is exactly why the
predicate had to go: "not wired to the other backdrops" was correct for the reported symptom and
wrong for the defect class — the Save Preset backdrop hosts a `TextEditor`, whose context menu
reaches the same JUCE re-delivery path.)*

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

Two more were added by the 2026-08-10 review sign-off. The **Widen combos in Simple mode** (15.5 pt)
are accepted as-is in code and owed only a look: the menu-width allowance now sums to 50 px against
the 38 px the drawing actually spends, so every menu measured on its item text — not on
`withMinimumWidth (box.getWidth())` — is up to 20 px wider than in 0.9.2. A **host that hides rather
than destroys the editor** while a drop-down is open was first accepted as-is too, then **fixed** in
the following round — see §9; what is still owed there is the on-device confirmation in such a host,
not a decision.


---

## 8. Follow-up review: three corrections and one accepted limitation

### 8.1 The shield and hover state — reasoning corrected in the follow-up review

**What this section originally claimed** (kept for context; the mechanism below supersedes it):
raising the shield with `setVisible (true)` / `toFront` sends a fake mouse move
(`juce_Component.cpp:559`, `:883`), and the raise happens from the `MenuWindow` **constructor** —
before the menu enters its modal state — so the control under the cursor was not yet blocked and
received a genuine `mouseExit`, clearing `SpectrumImager`'s hover indices and `ABControl::hovered`
for as long as the menu stayed open. The shield was switched from visibility-toggling to
**interception-toggling** on that reasoning, with the ordering in `refreshPopupShield` (re-order
first, intercept second) presented as what kept the fake move landing on the control underneath.

**The mechanism is not that.** A later trace against the pinned JUCE found the premise wrong at its
root: **the fake move is asynchronous.** `Component::sendFakeMouseMove` calls
`MouseInputSource::triggerFakeMove`, which is a bare `triggerAsyncUpdate()`
(`juce_MouseInputSourceImpl.h:449-451`). It is dispatched a message-loop pass later — after
`showWithOptionalCallback` has run `setVisible (true)`, `enterModalState` **and** `toFront` on the
menu (`juce_PopupMenu.cpp:2290-2294`) and returned. So the ordering in `refreshPopupShield` buys
nothing: the deferred move always hit-tests against an already-intercepting shield, and JUCE's own
move from the menu's `setVisible (true)` is the same deferred move, not a second, earlier one.

Hover survives for two reasons that hold independently of any ordering we choose:

1. **Modality gates the delivery, not the hit test.** By dispatch time the menu is modal, and
   `Component::internalMouseEnter` / `internalMouseExit` *both* early-return before touching any
   state when the target `isCurrentlyBlockedByAnotherModalComponent()` (`juce_Component.cpp:
   2414-2420`, `:2452-2458`). `MenuWindow` does not override `canModalEventBeSentToComponent`, so
   every editor child — the control under the cursor **and** the shield — is blocked
   (`juce_ComponentHelpers.h:213-219`). `componentUnderMouse` moves to the shield as bookkeeping, but
   no `mouseExit`/`mouseEnter` callback is delivered, so the only two event-driven hover consumers in
   `src/` (`SpectrumImager::mouseExit`, `ABControl`'s `hovered`) cannot be cleared. *(The cursor
   reverting to the arrow while a menu is up is JUCE's own designed behaviour for any modal
   component — `internalMouseExit` calls `showMouseCursor (NormalCursor)` on that same early-out —
   and is unrelated to the shield.)*
2. **This editor does not derive hover from enter/exit anyway.** `stepMicroAnims` computes
   `over` geometrically — `mouseInside && c->isShowing() && c->getLocalBounds().contains
   (c->getMouseXYRelative())` (`src/PluginEditor.cpp:1331-1333`) — to drive `hovA`, and the combo
   `"hov"` flag uses the same test (`:1072-1073`). That is the v0.6.1 stuck-hover fix, and it makes
   `hovA` immune to `componentUnderMouse` churn by construction.

A third, partial gate applies to the common path: `ComboBox` and `TextEditor` open their menus from
`mouseDown` with the button still held (`juce_ComboBox.cpp:567-575`), and `sendFakeMouseMove`
early-returns while the source is dragging (`juce_Component.cpp:2756`), so for those two no move is
posted at all. It does not cover the preset menu, which opens from a button `onClick` on mouse-up.

**The shipped design is unchanged and remains correct**, on a different justification: toggling
interception avoids `setVisible`'s repaint side effects — a full-editor `repaint()` on every menu
open, a `repaintParent()` plus a cached-image release on every close (`juce_Component.cpp:555-563`) —
where `setInterceptsMouseClicks` is pure flag assignment (`:1336-1341`). It is also the shape
`dimOverlay` already uses in this editor — an always-visible, non-intercepting full-editor overlay —
so it is one fewer idiom, not one more.

### 8.2 "Does the preset menu really miss the look-and-feel hook?" — re-verified, yes (unchanged)

The natural objection is that the preset menu is parented to the editor, so it should inherit our
look-and-feel and reach `preparePopupMenuWindow` after all, making the separate counter redundant.
It does not, and the reason is sharper than the original comment gave: `MenuWindow` binds
`auto& lf = getLookAndFeel()` at `juce_PopupMenu.cpp:368`, **before** `pc->addChildComponent (this)`
at `:372`, and calls `preparePopupMenuWindow` through that same bound **reference** at `:500` — its
only call site in the file. Parenting afterwards cannot rebind a reference. With this menu's own
look-and-feel null (`findLookAndFeel` returns `menu.lookAndFeel.get()`, `:1422-1425`), what `:368`
resolves is the process **default** look-and-feel. So the counter stays, and the comment now carries
the reference-binding argument rather than the weaker "captured before parenting" phrasing.

### 8.3 The backstop's scope (comment corrected, no code change)

The 24 Hz backstop prunes `openMenus`; it does not reconcile `presetMenusOpen`, and the comment was
read twice as claiming otherwise. It does not need to: `showPresetMenu` always adds at least three
items — the FACTORY section header plus Save/Load Preset, all unconditional — so
`PopupMenu::createWindow` can never return null for it, which is the one path on which
`showMenuAsync` drops the callback without invoking it. The decrement therefore always runs. The
comment now says which half it covers and why the other half needs no cover. No recovery machinery
was added for a statically unreachable state.

### 8.4 Tooltips: off now means off (fixed)

Disabling Tooltips left a visible tooltip on screen, and moving quickly to another control could
still raise a new one. Both come from one cause: `applyTooltipsEnabled` only pushed
`millisecondsBeforeTipAppears` to a huge value, and that value is only consulted on
`TooltipWindow::timerCallback`'s **slow** path. While a tip is visible — or within 500 ms of one
hiding — the timer takes a fast path and calls `showTip()` on any tip change *without consulting the
delay at all* (`juce_TooltipWindow.cpp:242-247`). So the "switch" was never a switch; it was a very
long delay that a visible tooltip bypassed.

`TooltipWindow::getTipFor` is **virtual**, so the fix is to switch tooltips off at the source: a
small `GatedTooltipWindow` returns nothing while disabled, and JUCE's own state machine does the
rest — the same fast path hides on an empty tip instead of showing one, and the slow path has
nothing to show either. `applyTooltipsEnabled` additionally calls `hideTip()` so the transition is
immediate rather than up to one timer tick late. One override and one call: no second tooltip
system, no timer of our own, and the enabled path is untouched.

*Follow-up:* the `tooltipsOn ? 600 : 0x3fffffff` line was left in place at first, so two mechanisms
encoded the same off state. It has since been **removed** — the delay now sits only where the member
is constructed (`GatedTooltipWindow tooltips { nullptr, 600 }`) and nothing changes it at runtime.
Behaviour is unaffected: `millisecondsBeforeTipAppears` is read on one branch only, already inside
`newTip.isNotEmpty()` (`juce_TooltipWindow.cpp:250-256`), which the gate's empty tip never reaches.

### 8.5 The dismissing click still counts toward the double-click run — **KI-018**

Confirmed and *not* fixed, deliberately. The shield stops the dismissing click reaching any control,
but it cannot un-count it: JUCE's multi-click run lives on the input source, and
`registerMouseDown` records position/time/buttons/peer only — **not the target component**
(`juce_MouseInputSourceImpl.h:577-595`, `:561`) — and runs during dispatch, before any component is
consulted. So a fast second click can arrive at a control as a double-click.

Every available lever is out of bounds: `MouseInputSource` exposes no reset for the run;
`MouseEvent::setDoubleClickTimeout` is **process-global** and would change the host's and every other
plug-in's behaviour (the objection that already ruled out writing `ApplePressAndHoldEnabled` in
KI-017); holding the shield up for the timeout would swallow the legitimate second click, breaking
the contract it exists to enforce; per-control guards re-create the approach the shield replaced; and
patching JUCE is a gated Build System change for a Low-severity cosmetic race. Filed as **KI-018**
with the mechanism, the workaround and what would close it upstream.

**Also corrected in this round:** `docs/COMMERCIAL_STATUS.md` still named v0.9.2 as the release in
preparation in three places. Only those three statements changed; its genuinely historical references
(which versions were never tagged, what 0.9.2 contained) are preserved, and its review date stands —
the file's substance is unaffected by a renumbering, which is what that date tracks.

---

## 9. A drop-down outliving the plug-in window being hidden (fixed)

The last round accepted this as "behaviour unchanged". The second pass traced what the unchanged
behaviour actually costs the user, and it is a defect rather than a residue, so it was reopened and
fixed.

**Symptom.** With a Settings drop-down or a right-click text menu open, a host that *hides* the
plug-in view rather than destroying it leaves the menu behind: a floating always-on-top strip over a
window that is no longer there. On return, the first click goes to dismissing that leftover instead
of to the control it was aimed at.

**Why the existing machinery misses it.** JUCE already cancels a modal component the moment its owner
stops showing — `ModalComponentManager::ModalItem` is a `ComponentMovementWatcher`, and
`componentVisibilityChanged` / `componentPeerChanged` both end in
`if (! component->isShowing()) cancel();` (`juce_ModalComponentManager.cpp:60-68`). The watcher
registers on the component **and its ancestors**, which is exactly why INC-010's parenting fix earns
that cancel for free: the preset menu is an editor child, so hiding the editor reaches its watcher.

A `ComboBox` or `TextEditor` drop-down is not. JUCE builds it as a free-standing **desktop** window
(neither path passes a parent component, and INC-010's `withParentComponent` applies only to
`showPresetMenu`), so the watcher's ancestor set is the menu alone and the editor's visibility is
invisible to it. The menu's own visibility does not change, so nothing cancels.
`MenuWindow::windowIsStillValid` is no second line of defence either: it compares `componentAttachedTo`
against `options.getTargetComponent()` (`juce_PopupMenu.cpp:806-816`), two `WeakReference`s to the same
still-alive control, so it reads valid across a hide. Editor *destruction* was never the problem — the
destructor removes the listeners and the array goes with the object.

**What that costs, precisely.** Three things, only the first of which is about the shield:

- `openMenus` stays non-empty, so `refreshPopupShield` keeps the shield intercepting. The re-shown
  editor spends its first click dismissing the stray menu — one dead click, not a permanent lock,
  because the menu is still modal and JUCE's own `internalModalInputAttempt` dismisses it. But the
  click is spent, and the editor looks broken for exactly as long as it takes to work that out.
- The stray menu is a visible always-on-top window with nothing behind it. That is INC-010's reported
  symptom verbatim, one menu type later.
- It is still modal, and modality is **process-global**: every other JUCE component in the process —
  including another Anamorph instance's editor — is blocked while it is up.

**Fix.** Do for those windows exactly what JUCE's watcher does for a parented one: same trigger, same
action. `dismissOrphanedPopupMenus` calls `exitModalState (0)` on every tracked window once
`! isShowing()`, which is the call `ModalItem::cancel` itself ends in; it self-guards on
`isCurrentlyModal`, so a window already on its way out is a no-op, and deletion stays asynchronous so
`componentBeingDeleted` lowers the shield when it lands.

Deliberately **not** `PopupMenu::dismissAllActiveMenus()` — process-global, and it would close another
instance's menu. That is the objection that already ruled it out in INC-010.

**Why the 24 Hz tick rather than an event.** There is no event to listen for. The editor going off
screen can come from an ancestor's `setVisible`, a peer change, or a minimise; `isShowing()` folds all
three (`juce_Component.cpp`), but none of them notifies *us* — `ComponentMovementWatcher` gets there by
registering on every ancestor, which is machinery for a case the tick already covers. The tick runs
regardless of visibility (`startTimerHz (24)` in the constructor, `stopTimer()` only in the
destructor) and already scans `openMenus` as the shield's backstop, so the cost is one `isShowing()`
call on an almost-always-empty array, and the latency is invisible by definition — the editor is
hidden.

**Blast radius.** The predicate is `! isShowing()`, so nothing at all changes while the editor is
showing: the dismissal contract, the shield's z-order, one-click dismissal and the scroll/pinch
swallow are untouched. The one behaviour it does add is that minimising the host window with a
drop-down open now closes the drop-down — which is what already happened to the preset menu, and what
a native menu does.

**Honest limit.** `isShowing()` is false when an ancestor is hidden, when the peer is gone, or when
the peer reports minimised. A host that hides its own native window without any of those leaves
`isShowing()` true and the menu alive — but that case emits no signal any in-bounds mechanism could
read, and it is identical to what the preset menu has done since 0.9.2. Consistency with the parented
case is the guarantee being offered here, not omniscience.

**Manual check owed:** in a host that hides rather than destroys the editor, open a Settings
drop-down, hide the window, re-show it, and confirm no stray menu and that the first click presses the
control it lands on. Not reachable headlessly for the reasons in §3.
