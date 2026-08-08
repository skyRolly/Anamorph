# Preset drop-down lifetime/crash + factory-preset identity, and the macOS key-repeat investigation (v0.9.2)

> Five maintainer-reported items. Four produced code; one produced a root-cause and a Known Issue
> instead, because the mechanism turned out to sit below the plug-in. This worklog keeps the
> reasoning that does not belong in `CHANGELOG.md` or an ADR — in particular the JUCE-source traces,
> the rejected fixes, the six defects review found before merge, and the follow-up round in which
> the indicator identity moved into plug-in state (§5-6).

- **Date:** 2026-08-07 · **Version:** 0.9.2 (PR #100) · **Branch:** `claude/beautiful-sagan-JAUFI`.
- **Reference tree:** JUCE 9.0.0 at the pinned commit `f8f8864…` (`CMakeLists.txt:36-38`), fetched
  and read locally; all JUCE line citations below are against that commit.
- **Suites:** `AnamorphTests` 140 checks, `AnamorphStateTests` 842 checks (was 774), both green.

---

## 1. Save Preset key auto-repeat — investigated, no code change (KI-017)

**Report.** Holding a letter or a digit in the Save Preset name field types the character once and
stops. Symbol/punctuation keys repeat normally. Elsewhere on macOS, holding a key repeats.

**Trace.** `saveNameEditor` is a `juce::TextEditor`, therefore a `juce::TextInputTarget`. Once it
holds JUCE keyboard focus, `ComponentPeer::findCurrentTextInputTarget()` returns non-null
(`juce_ComponentPeer.cpp:291-301`), and **both** `keyDown:` and `performKeyEquivalent:` funnel into
`NSViewComponentPeer::sendEventToInputContextOrComponent`, whose first act is
`[inputContext handleEvent: ev]` (`juce_NSViewComponentPeer_mac.mm:1655-1662`). JUCE's own
`redirectKeyDown` → `TextEditor::keyPressed` runs **only** if the input context declines
(`:1667-1668`).

That splits the keyboard in exactly the reported way:

| key class | return path | repeats? |
|---|---|---|
| printable characters | `insertText:replacementRange:` (`:2396-2435`) → `insertTextAtCaret` | decided by `NSTextInputContext` |
| backspace, return, tab, escape, arrows | `doCommandBySelector:` (`:2437-2467`) → `redirectKeyDown` → `TextEditor::keyPressed` | yes, on every delivery |

Two AppKit behaviours live on the first path and both produce "letters and digits dead, other keys
fine": a **non-Roman input source** (Pinyin/Zhuyin/ABC-Extended consumes `a`–`z` as composition
keys and `0`–`9` as candidate selectors, passing punctuation through — this matches the reported set
exactly), and **`ApplePressAndHoldEnabled`** (default on; hands accent-capable keys to the
press-and-hold panel — on its own it does not account for digits). JUCE's own comments at `:2409`
and `:2580` show it deliberately supports the press-and-hold panel on this path.

**Everything inside the plug-in was eliminated by inspection**, not by assumption:

| suspect | why it cannot be the cause |
|---|---|
| `focusSaveNameField` retry | bounded at 4 × 50 ms, only called from `showSavePreset(true)`, and `grabKeyboardFocus()` on the already-focused field is a no-op |
| `setSelectAllWhenFocused(true)` | fires inside `TextEditor::focusGained`, once per focus gain — its failure mode would be "text keeps getting re-selected", not "no repeat" |
| 24 Hz timer / `VBlankAttachment` | message-thread repaints only; a repaint cannot consume a queued key event, and starvation would delay *all* keys uniformly, not select letters and digits |
| the UI-scale `AffineTransform` | key routing goes through `Component::getCurrentlyFocusedComponent()` and the peer; the transform is never consulted |
| `getCurrentModifiersRealtime()` | both call sites are gated behind a held mouse button; on all three platforms it is a pure state query that dequeues nothing |
| the `Backdrop` overlay | a plain `Component` with `paint`/`mouseDown`; repo-wide there is no `keyPressed`, `keyStateChanged`, `KeyListener`, `enterModalState` or `setWantsKeyboardFocus` in `src/` |
| the JUCE `lastSeenKeyEvent` dedupe | its key tuple includes `[event timestamp]` **and** `isARepeat`, so two genuine repeats can never compare equal; `isARepeat` occurs twice in the whole module tree |

**Rejected fixes, and why.**

1. *Write `ApplePressAndHoldEnabled` from the plug-in.* `NSUserDefaults` is **process-wide**: a
   plug-in doing this silently changes the host's own text fields and every other plug-in loaded in
   that process. Out of bounds.
2. *Bypass the input context for printable keys.* That is where dead keys, accents and CJK/IME
   composition live — it would trade a repeat annoyance for broken non-Latin input, and it is a
   JUCE source patch, i.e. a Build System change under `ARCHITECTURE_REVIEW_GATE.md` +
   `DEPENDENCY_POLICY.md` (the pin is an immutable SHA, ADR-0022).
3. *Synthesise the repeat from a `Timer` in a `TextEditor` subclass.* It double-types for every user
   who has press-and-hold disabled, and it cannot read the user's System Settings repeat delay/rate.

**Outcome.** `KI-017`, with the two discriminating checks (switch the input source to ABC/U.S.;
`defaults write -g ApplePressAndHoldEnabled -bool false`) and the user-side workaround. No
`CHANGELOG` entry — nothing user-visible changed (`CHANGELOG_POLICY` rule 3). The sibling plug-in
Anabasis lands on the identical path and must behave identically; if it does **not** on the same
machine, the attribution is wrong and KI-017 says so and names the next place to look.

---

## 2. Preset drop-down lifetime + crash (INC-010)

The first draft of the fix — and its code comment — named the wrong mechanism. Recorded here
because the corrected version is less obvious than the wrong one.

**What is actually true, read out of `juce_PopupMenu.cpp`:**

- With no parent, the `MenuWindow` is an always-on-top **desktop** window owned by a
  `PopupMenuCompletionCallback` held by the process-global `ModalComponentManager`. The editor holds
  no reference to it and is not consulted when it dies.
- JUCE's self-heal is present but cannot fire. `windowIsStillValid()` dismisses when
  `componentAttachedTo != options.getTargetComponent()` — and **both are `WeakReference<Component>`
  to `presetName`**, an editor member, so they null in the same instant and the comparison is false.
  That is the 残留.
- The lost item styling is **not** a use-after-free. `PopupMenu` stores its look-and-feel as a
  `WeakReference<LookAndFeel>`, so `setLookAndFeel (&lnf)` *nulled*; `Component::getLookAndFeel()`
  then falls back to `LookAndFeel_V4`. Cosmetic. (It does trip `~LookAndFeel`'s debug assertion
  about live weak references.)
- The **crash** is the raw `this` in the `showMenuAsync` callback. `withDeletionCheck` was never
  used, so JUCE's `resultID = 0` escape hatch was inert and the lambda ran with a real item id.

**Fix and why each half is load-bearing.** `withParentComponent (this)` takes the
`pc->addChildComponent (this)` branch instead of `addToDesktop`, so the menu is clipped to the
editor, stacks with it, and is cancelled with result 0 by `ModalComponentManager`'s
`ComponentMovementWatcher` on destruction **or hide**. The `SafePointer` is not decoration: that
cancel is *asynchronous*, and in the gap between `~Component` and the async dismissal the menu is
parentless with its 20 Hz `MouseSourceState` timer still running, still able to reach
`triggerCurrentlyHighlightedItem()` and emit a non-zero result.

**Two side effects of parenting, neutralised in the same change** (both would have been regressions
introduced by the fix, not pre-existing behaviour):

- A parented menu is budgeted against the **editor** (`parentArea.getHeight() - 24` ≈ 688 px in
  Simple mode, ≈ 868 in Advanced) rather than the display, and JUCE reacts to overflow by adding
  **columns** before it scrolls. Past ~14 user presets in Simple mode the preset list would have
  silently become two columns. `withMaximumNumColumns (1)` — the same option the combo popups
  already use — keeps today's shape.
- `MenuWindow::paintOverChildren` calls `drawResizableFrame` **only** when parented, painting two
  translucent black rects into the 3 px border ring on top of our own hairline. A no-op override in
  `AnamorphLookAndFeel` removes the doubled edge; Anamorph has no resizable windows, so the override
  has no other caller.

**Alternatives, with the reason each was rejected:** `dismissAllActiveMenus()` in the destructor
walks a process-global list and would close **another instance's** (or another JUCE plug-in's) open
menu — the same reason Anabasis rejected it; a shared `static` LookAndFeel trades the problem for
static-destruction order at DLL unload; `withDeletionCheck` alone kills the crash but leaves the
floating window and the styling loss; `SafePointer` alone leaves both visible symptoms.

**Audit of the same shape elsewhere in the editor.** The `showLoadPreset` `FileChooser` callback had
the identical raw-`this` capture and is *worse* — the OS chooser is modal to the host, not to us, so
it survives editor destruction with certainty. Fixed with the same guard; this is the one place the
change reaches past the literal report. Everything else is clear: the three `Backdrop` overlays,
every `Button::onClick` / `ComboBox::onChange` / `Slider::onValueChange`, and the `VBlankAttachment`
are all owned by editor members. **Reported, not fixed:** the combo-box popups store an
editor-member LookAndFeel the same way (same styling loss, same 残留) but their callback is
`ModalCallbackFunction::forComponent`, i.e. already SafePointer-based — no memory-safety defect, no
reported symptom, and changing it touches seven call sites across two LookAndFeel subclasses.

**No regression test.** A GUI-lifetime use-after-free is not expressible in
`tests/state_tests.cpp`, which links the editor but never instantiates it. Recorded as an explicit
`TESTING_POLICY` waiver; prevention is by construction instead — a parented menu has no independent
lifetime to get wrong.

---

## 3. Factory-preset identity (ADR-0024) — and the three defects review found

The design is in ADR-0024. What belongs here is the review trail: the first implementation passed
its own tests and was still wrong in three places, each found by walking a path the happy-path test
did not.

**H1 — the identity scan fell through.** The loop over `list` did not `return -1` on failure, so
control reached the name scan even when the identity was *known*. Loading a `.anamorph` from outside
the preset folder — identity known, on no row — then ticked whatever shared the name, i.e. the
same-named **factory** row. The code's own comment claimed the opposite. One `return -1`.

**H2 — session restore carried the name only.** Kept in the first round, deliberately: the
alternative is a serialized field, i.e. a Hard Stop. **Closed in the follow-up round**, after the
maintainer supplied the Architecture-Review approval — see §5. Resolving the name lazily on first
`currentIndex()` was considered and dropped even then: it converges anyway on the first `load()`,
and resolving inside `setStateInformation` would need `refresh()`, i.e. filesystem I/O on whichever
thread the host calls it from — a new cross-thread path, and another gate item.

**H3 — `saveUser` never re-baselined the undo snapshot.** A save changes no parameter, so the
gesture-gated coalescer never notices it and the processor's `committed` keeps the *pre-save* name,
baseline and identity — indefinitely. Load factory *Wide Master* → save as *Wide Master* → turn a
knob → undo, and the tick jumps back to the factory row with a stale dirty baseline. Fixed with an
`onSaved` hook → `syncCommitted()`, which creates no undo step (correct: a save is not a sound
change). Writing the test exposed a **second instance of the same gap**:
`commitPresetSwitchUndoStep` skipped its whole body when the new preset's sound was identical to the
current one, so switching between two identically-sounding presets left the previous preset's
metadata on the baseline. Now the metadata is adopted in that branch too, still with no undo step.

**H4 — `readSlot` left a stale identity on an A/B slot.** `abSlot[]` are processor members and many
hosts call `setStateInformation` repeatedly on one live instance. The identity is not serialized, so
there is nothing to read it from; without clearing it first, a second session's slot inherited the
first session's identity. `dst.selection = {}` at the top of the reader.

**Residual, accepted and documented:** `juce::File` equality is a path-string compare (JUCE does no
canonicalisation), so a chooser result reaching a preset-folder file by a different spelling
(symlinked `$HOME`, `/private/var` vs `/var`, UNC vs mapped drive) fails the identity match. With H1
fixed it degrades to "no tick", which is safe. Not normalised on purpose.

**Anabasis was checked and is not the model here.** Its `rememberPresetSource` is an *editor-local
hint* consulted only by `stepPreset`, and only when the display name still confirms it; its menu tick
is pure name, so in the duplicate-name case it ticks **both** rows. Its factory identity is a table
index, not a stable string, so reordering the table would silently re-point a live hint. Nothing was
ported.

**Test-count delta:** state test 10 adds 23 checks (774 → 797). Each of H1, H3 and H4 has an
assertion that was verified to **fail** with its fix disabled — a test that cannot fail is not a
test.

---

## 4. The two wording changes

`Window Size` → **UI Scale** is display-only: `int_uiScale` and the pre-0.8.4 legacy APVTS id
`uiScale` its migration reads are untouched, so nothing serialized moved and no ADR is owed
(`PARAMETER_COMPATIBILITY_POLICY` permits it; recorded with the repo's own `※` footnote form,
mirroring `Haas Side` → `Haas Focus`). The tooltip moved with the label — flagged, because
constraint C8 makes UI copy maintainer-owned and only the label was named in the request; leaving it
would have shipped a visible in-product contradiction.

Installer component **titles** are title-cased; prose sentences are not. The boundary matters: the
Windows destination-page *labels* changed, but the `MsgBox` sentences ("Enter a folder for the VST3
plug-in.") and the parenthetical inside the label did not. The Windows `[Components]` descriptions
("Install VST3" / "Install Standalone") contain neither phrase, so they are unchanged and the five
documents quoting them verbatim stay valid. No CI or self-check assertion matches a title — the
macOS self-check matches `<choice id=…>` and the package identifiers.

**Drift found while doing this, reported per C6:** `docs/user/INSTALLATION.md`'s macOS Component
table listed *AU (Audio Unit)* and *Standalone app*, neither of which ever matched the installer;
corrected in the same pass since the column is headed "Component". And the sibling Anabasis still
carries the lowercase installer wording in its own `packaging/`, so the two RollyTech installers are
now inconsistent — that is a separate repo and was not touched.

---

## 5. Follow-up round — the indicator identity moves into plug-in state (ADR-0024 amendment)

The maintainer asked for the H2 residual to be closed *without* touching user preset files, and
supplied the Architecture-Review approval the change needs. That approval is the only reason an
agent may do this: it is a Serialization Registry addition, i.e. an `ARCHITECTURE_REVIEW_GATE` item
and an AI-agent Hard Stop.

**Shape.** Six additive strings — `presetSource` / `presetFactoryId` / `presetUserFile` on
`AnamorphRoot`, and the same trio per A/B slot. `PresetManager::encodeSelection` /
`decodeSelection` are the single place that knows the wire form; `PluginProcessor`'s
`writeSelection` / `readSelection` are three-line adapters so the root node and both slots cannot
drift apart.

**Three properties the design is built to guarantee, each asserted:**

1. **User preset files are untouched.** A `.anamorph` written by 0.9.1 and one written by 0.9.2 are
   identical. The identity belongs to the *project*, not to the preset — a preset file has no way to
   know which sessions reference it, and putting an id in it would also make every hand-copied or
   shared preset carry someone else's identity.
2. **Parameter restore is independent of identity restore.** The sound comes from the `ANAMORPH`
   child; the identity is applied afterwards through `setMeta` / `adoptRestoredState`, neither of
   which touches a parameter. State test 12 asserts bit-identical parameters on **all five** paths,
   including the two where the identity deliberately fails to resolve.
3. **A wrong-but-well-formed stored value cannot select the wrong preset.** It resolves or it ticks
   nothing — `currentIndex()`'s existing rule that a *known* identity absent from the list returns
   -1 (the H1 fix) is what makes the new fallbacks safe rather than merely tidy.

**Why a file NAME and not a path**, for a preset inside the preset folder: the name is already a
complete identity there, it keeps the user's home directory out of the saved project, and a project
opened on another machine still resolves. A preset opened through "Load Preset…" from *outside* the
folder has no such anchor, so its absolute path is stored instead — it is on no menu row either way,
but this keeps `decode(encode(s)) == s` true rather than silently re-pointing it at a same-named file
in the folder.

**Backward and forward compatibility.** Absent, empty, half-written and unrecognised all decode to
`unknown`, which is precisely the pre-0.9.2 name fallback — so an old project behaves exactly as it
did (`SESSION_COMPATIBILITY_POLICY` rule 2). Nothing was removed and no existing field changed
meaning (rule 1). A 0.9.2 project opened in 0.9.1 simply ignores six unknown attributes.

**ADR handling.** ADR-0024 is amended in place rather than superseded by a new ADR: it has not
reached `main`, the amendment is dated and keeps the original decision text verbatim above it, and
only clause 3 (and the residual it produced) is reversed — clauses 1, 2 and 5 stand unchanged. A
separate superseding ADR-0025 would have been the other defensible shape; flagged for the maintainer
rather than chosen silently.

## 6. Follow-up round — the other four review items

**Accepted.** `docs/REPOSITORY_MAP.md` still said 9 state tests (the one carrier the previous round
missed). `saveUser` now flushes pending undo coalescing before re-baselining, matching
`onAboutToLoad` and `undo()`/`redo()` — without it, `syncCommitted()`'s `pendingGestureCommit = false`
silently discarded a closed-but-unpolled knob gesture's undo step. `load()` resolves a factory id
BEFORE the undo bracket opens (the rule the user-preset parse in the same function already followed),
asserts it, and fails as a clean no-op instead of applying defaults under an identity that resolves
to nothing. The `focusSaveNameField` comment was rewritten: parenting removed the desktop window it
blamed, but not the `peer->isFocused()` abort it actually works around.

**Declined, with evidence: restoring `m.setLookAndFeel (&lnf)`.** Item measurement already uses
`AnamorphLookAndFeel` — `MenuWindow` parents itself at `juce_PopupMenu.cpp:370-372` and builds items
at `:457`, and `ItemComponent` calls `parent.addAndMakeVisible` before `getIdealSize` (`:139-146`),
which resolves through `getLookAndFeel()`. Restoring it would re-arm the `~LookAndFeel` assertion
(it fires on any live `WeakReference`, and `lnf` is a member destroyed before this editor's
`Component` base — i.e. before the menu's asynchronous cancel). The two calls that genuinely see the
default look-and-feel are bound one line before the parenting: `setOpaque` (same answer —
`colours::bgPanel` is opaque) and `preparePopupMenuWindow` (a no-op we do not override). Both inert;
now recorded in the code as the latent trap they are.

## 7. Third review round — two defects in the §5 work

An independent review of the finished change set found two more, both introduced by §5 and both now
fixed with a discriminating assertion.

**`juce::File::isAChildOf` recurses.** `encodeSelection` used it to decide "is this preset in the
preset folder, so I can store just its name?" — but JUCE implements it as
`return getParentDirectory().isAChildOf (potentialParent);` (`juce_File.cpp:400-414`), i.e. true at
*any* depth. A preset opened through "Load Preset…" from a **sub-folder** of the preset folder was
therefore stored as its bare file name and decoded on reload as
`presetDirectory().getChildFile(name)` — a **different file**. With a same-named preset sitting
directly in the folder (exactly the duplicate-name situation this whole change is about), the
reloaded session ticked *that* row, and `‹ ›` then stepped from it. The sound was still correct; only
the identity was wrong, and nothing warned, because both files carry the same display name. It broke
the `decode(encode(s)) == s` invariant the header and the ADR Amendment both state.

Fixed by testing for a **direct child** — `s.file.getParentDirectory() == presetDirectory()` — which
is also the more honest test: `refresh()` scans non-recursively, so a direct child is the only thing
that can ever *be* a menu row. Everything else now takes the absolute-path branch and round-trips
exactly. State test 12 gained a nested-path case that runs with the same-named flat preset present.

**The new `else` branch of `commitPresetSwitchUndoStep` did not clear redo.** The `if` branch does,
one line above, with the comment "a new user action invalidates the redo stack" — and a sonically
identical preset switch is no less a user action. Leaving redo alive there meant: undo an edit, then
select the same-sounding preset on the *other* row, then press Redo — and the tick jumped back to the
previous preset's row, restored out of an abandoned `StateSet` whose `.selection` was stale. One
line; both outcomes of a preset load now invalidate redo identically. Deliberately **not** applied to
the `onSaved` path: ADR-0024 §5 states a save is not a sound change, so leaving redo alone there is
the consistent answer.

**Reported, not fixed — pre-existing.** `saveUser` builds its target with
`dir.getChildFile (name + kPresetExt)`, and `juce::File::getChildFile` short-circuits to the raw
`File` constructor for anything `isAbsolutePath` accepts. On macOS/Linux a leading `~` qualifies and
`File::createLegalFileName` does not strip it, so saving a preset named `~foo` writes outside the
preset folder (relative to the host's CWD when `getpwnam` fails), `saveUser` still returns `true`, and
the preset never appears on the menu. Pre-existing, unchanged by this work, and not worsened by it —
the resulting identity round-trips and correctly ticks nothing. Filed here rather than fixed, because
it is an input-validation defect in its own right.
