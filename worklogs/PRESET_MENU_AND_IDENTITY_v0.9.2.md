# Preset drop-down lifetime/crash + factory-preset identity, and the macOS key-repeat investigation (v0.9.2)

> Five maintainer-reported items. Four produced code; one produced a root-cause and a Known Issue
> instead, because the mechanism turned out to sit below the plug-in. This worklog keeps the
> reasoning that does not belong in `CHANGELOG.md` or an ADR — in particular the JUCE-source traces,
> the rejected fixes, the six defects review found before merge, and the follow-up round in which
> the indicator identity moved into plug-in state (§5-6).

- **Date:** 2026-08-07 · **Version:** 0.9.2 (PR #100) · **Branch:** `claude/beautiful-sagan-JAUFI`.
- **Reference tree:** JUCE 9.0.0 at the pinned commit `f8f8864…` (`CMakeLists.txt:36-38`), fetched
  and read locally; all JUCE line citations below are against that commit.
- **Suites:** `AnamorphTests` 140 checks, `AnamorphStateTests` 894 checks (was 774), both green.

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
  That is the leftover menu.
- The lost item styling is **not** a use-after-free. The MenuWindow **copies** the menu's
  look-and-feel into its own `Component::lookAndFeel` slot (`juce_PopupMenu.cpp:366`), and that slot
  is a `WeakReference`, so it *nulled*; `Component::getLookAndFeel()` then falls back to
  `LookAndFeel_V4`. (The `PopupMenu` itself is a stack local in `showPresetMenu` and is gone long
  before the editor — it cannot be the reference that outlives us.) Cosmetic. (It does trip
  `~LookAndFeel`'s debug assertion about live weak references.)
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
editor-member LookAndFeel the same way (same styling loss, same leftover menu) but their callback is
`ModalCallbackFunction::forComponent`, i.e. already SafePointer-based — no memory-safety defect, no
reported symptom, and changing it touches seven call sites across two LookAndFeel subclasses.

**No regression test.** A GUI-lifetime use-after-free is not expressible in
`tests/state_tests.cpp`, which links the editor but never instantiates it. Prevention is by
construction instead — a parented menu has no independent lifetime to get wrong. Recorded first as a
disclosed deviation from `TESTING_POLICY` rule 1; **superseded by ADR-0025**, which amends that rule
with a narrow exception for defects no automated surface reaches and names `TESTING.md` §Gaps as the
register. INC-010 is its first invocation, with all four required disclosures recorded there.

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

**H4 — `readSlot` left a stale identity on an A/B slot.** *(Superseded by §5: the identity is now
serialized per slot, so `readSlot` DECODES it rather than clearing it. The defect and the reason the
assignment must be unconditional are unchanged.)* `abSlot[]` are processor members and many
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

**Test-count delta:** state test 10 added 23 checks (774 → 797); §5, §7 and §8 take the suite to 847, §9 to 856, §10 to 858, §11 to 866, §12 to 878, §13 to 893 and §14 to 894. Each of H1, H3 and H4 has an
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
   which touches a parameter. State test 12 asserts bit-identical parameters on **all seven** paths,
   including the three where the identity deliberately fails to resolve.
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
`Component` base — i.e. before the menu's asynchronous cancel). **Three** calls resolve through the
default look-and-feel, not two. Two are inert and bound one line before the parenting: `setOpaque`
(same answer — `colours::bgPanel` is opaque) and `preparePopupMenuWindow` (a no-op we do not
override). The third is **load-bearing** and earlier still: `getParentComponentForMenuOptions`
(`juce_PopupMenu.cpp:353`, in the member-init list) is what actually installs the parent, so a
process-global default look-and-feel that overrode it to return `nullptr` would silently discard the
parenting and drop the menu back to a desktop window, leaving only the SafePointer. Every JUCE
look-and-feel inherits `LookAndFeel_V2`'s pass-through and nothing Anamorph owns can reach it — now
recorded in the code as the latent trap it is.

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

**Raised and then REFUTED — `saveUser` returns FALSE for a `~`-leading name; nothing is written and
no code changed.** (Verdict first, because this paragraph has been mis-read as *reporting* the defect
and the claim has been re-raised twice since. The refutation now also lives in the code at
`src/PresetManager.cpp`'s `getChildFile` call — see §12.) A reviewer proposed that
`saveUser` can silently succeed for a name with a leading `~`: `juce::File::getChildFile`
short-circuits to the raw `File` constructor for anything `isAbsolutePath` accepts, a leading `~`
qualifies on macOS/Linux, and `File::createLegalFileName` does not strip it — so
`dir.getChildFile ("~foo.anamorph")` really does yield the unresolved relative path
`"~foo.anamorph"`. All of that is true, and it is where the analysis stopped.

The write cannot succeed. `File::replaceWithText` does not open the target: it writes a hidden
sibling built from `target.getParentDirectory()`, and for a path with no separator
`getPathUpToLastSlash()` returns the path itself — so the "parent directory" is `"~foo.anamorph"`,
which is not a directory, the temp write fails, and `replaceWithText` returns false. That is
guaranteed rather than incidental: `createLegalFileName` strips `/` and `\`, so a name reaching
`getChildFile` can never contain a separator, and every tilde-leading name hits the same degenerate
parent. `saveUser` therefore returns **false** at its own write check; `refresh()`, `current`, `sel`
and `onSaved()` are all unreachable, nothing is written anywhere, and the editor's `if (saveUser(...))`
leaves the dialog open with the text still in the field. The save fails **visibly** — which is
exactly the outcome the proposed guard was meant to produce, so the guard would have been a no-op.
Verified empirically against the pinned `juce_core`, for `~foo`, `~/foo`, `~` and `~root`, with a
normal name as the control.

## 8. Post-merge review round — the redo guard, narrowed

§7's redo fix was correct in direction and too broad in reach. `commitPresetSwitchUndoStep`'s
same-sound branch cleared redo **unconditionally**, so re-picking the row that is *already ticked* —
identical sound, identical identity — threw the redo stack away. Reproduction: load preset A, move a
knob (one undo step), Undo (redo now holds that edit), click preset A again in the drop-down; the
signature matches the committed one, the else branch runs, redo is discarded, and the edit the user
was about to redo is gone for good.

The justification only ever held for a *moved* identity: a surviving redo entry carries the previous
preset's identity, so redoing it after a switch would drag the tick off the row just chosen. Where
the identity did not move there is nothing for the redo entry to contradict. The guard is now exactly
that condition — `presets.selection() != committed.selection` — which required a small equality
operator on `Selection` that compares only the fields the `kind` actually uses, so a stale
`factoryId` left on a `userFile` selection cannot make two identical rows compare different.

Both halves are asserted, and both were checked against the pre-fix behaviour: the existing
"a sonically identical preset switch still invalidates redo" (different row) passes before **and**
after, and the new "re-selecting the already-current preset preserves redo" fails before and passes
after. That pairing is the point — it proves the change narrowed the rule rather than removed it.

Two documentation items landed in the same round. `STATE_SERIALIZATION.md` — the narrative half of
the pair whose ledger half (`SERIALIZATION_REGISTRY.md`) was synced in §5 — still drew the pre-0.9.2
`AnamorphRoot` layout, so it described a saved session the software no longer produces; its schema
block, both step lists, its backward-compatibility table and its evidence anchors are now current,
and it gained the root-vs-slot, metadata-only and absence/unresolvable rules that the ledger states
field-by-field. And `DOCUMENTATION_COVERAGE.md` still carried the `~foo` claim as a pre-existing
defect, contradicting §7's refutation of it in the same change set; the ledger now records the
refutation instead, so a future agent does not "fix" a non-defect.

## 9. Fourth review round — the tilde round-trip, and "absence means default" for A/B metadata

**A direct-child file name that `isAbsolutePath` accepts broke `decode(encode(s)) == s`.** §7 fixed
the *nesting* half of the encoder and left a second way in. `encodeSelection` stored a direct child of
the preset folder as its bare file name; `decodeSelection` reads that back with
`presetDirectory().getChildFile(name)`, and `getChildFile` short-circuits to the raw `File`
constructor for anything `juce::File::isAbsolutePath` accepts — a leading `~` on POSIX. So a preset
named `~foo.anamorph` sitting directly in the folder encoded as `"~foo.anamorph"` and decoded as the
literal *relative* string, which matches no row: the tick vanished on reload and `‹ ›` had nothing to
step from. The sound was unaffected, as always with the identity fields.

§7 established that `saveUser` cannot *create* such a name — `replaceWithText`'s degenerate parent
directory makes the write fail visibly — and that refutation stands for the **save** path. It says
nothing about the **encode** path: `USER_MANUAL.md` tells users to manage presets as files, so a
hand-copied `~foo.anamorph` reaches `refresh()`, becomes a menu row, and can be loaded and encoded
like any other. The fix is one extra condition on the name-encoding branch —
`! juce::File::isAbsolutePath (name)` — so such a file falls through to the absolute-path branch it
already shares with outside-the-folder and sub-folder presets. That one preset loses cross-machine
portability of its tick; the round-trip invariant holds, which is the property the design depends on.
Deliberately **not** fixed by canonicalising or rewriting names: the preset file format is untouched
and the no-name-fallback rule is untouched.

**A/B slot metadata did not follow "absence means default".** `readSlot` assigned `dst.selection`
unconditionally (§5 got that right) but read `dst.name` and `dst.baseline` *inside* the
`hasProperty (pk)` branch, so the pre-0.6.4 legacy shape — params only — left both untouched.
`abSlot[]` are processor members and a host may call `setStateInformation` on one live instance
repeatedly, so restoring a legacy session after a modern one left the **previous** session's preset
name and dirty-baseline attached to the freshly restored parameters: the A/B slot showed a preset
name that had nothing to do with its sound. Moving the two reads out of the branch is the whole fix;
the defaults it now produces (`""` / `""`) are the ones `SERIALIZATION_REGISTRY.md` already documented,
so this is the code catching up to the ledger rather than a format change. No serialization field was
added, removed or renamed.

**An existing assertion was changed, not just added.** State test 5 asserted
`slotAName == "Default"` under the comment "legacy slot keeps pre-restore meta" — it *pinned the
defective behaviour*, describing a fresh instance's construction snapshot as if it were the rule. It
now asserts `""` for both `slotAName` and `slotABase`, with the repeated-restore case that shows why
the old expectation was wrong.

**Documentation.** ADR-0024's Consequences now states the three properties of the identity match
plainly: it is a raw path-string compare with no canonicalisation (`getLinkedTarget()` was considered
and rejected — it resolves symlinks but not `/private/var`, mount aliases or UNC spellings, so it
would trade a predictable "no tick" for a partial one); cross-machine resolution holds only for the
name-encoded case; and a file name that looks like a path is stored as a path. `SERIALIZATION_REGISTRY.md`
gained both encoder conditions and the raw-compare note, and its drifted citation was corrected
(`src/PresetManager.h:54-77` → `:54-76` for `Selection`, `:78-94` for `SelectionFields` and the two
functions). No behaviour was introduced by any of this. The citations that this round's own edits
moved — everything after `readSlot`'s body in `PluginProcessor.cpp` and after the encoder comment in
`PresetManager.h`/`.cpp` — were re-anchored in the same pass across `SERIALIZATION_REGISTRY.md`,
`STATE_SERIALIZATION.md`, `API_REFERENCE.md` and this ADR.

**Negative controls.** All four new assertions were run with their fix reverted and observed to fail:
`a legacy slot carries no name of its own` (got `"Default"`), `...and no baseline of its own` (got the
signature string), `a legacy restore does not leave the previous session's slot name attached` (got
`"Gentle Width"`), and `a tilde-named preset keeps its tick across a reload`. Suite: 847 → **856
checks**, 0 failures; `AnamorphTests` 140, unchanged.

## 10. Fifth review round — "no baseline recorded" is not "modified"

§9 fixed *which* metadata a legacy A/B slot restores with, and got the second half of the question
wrong. A pre-0.6.4 slot now comes back with `name == ""` **and** `baseline == ""`. `isDirty()` is
`soundSig() != sigAtLoad` and `soundSig()` is never empty, so an empty baseline compares unequal to
every possible sound: switch into that slot and it reads as **permanently modified**, with no name to
attach the marker to. `refreshPresetDisplay` builds `name + (isDirty() ? " *" : "")`, so the top bar
renders a bare ` *`.

The reviewer flagged it as a *possible* UX consequence and asked whether it was intended. It is not,
and the repository already answers the question. `adoptRestoredState` ends with
`sigAtLoad = soundSig(); // restored state counts as the clean baseline`, `SERIALIZATION_REGISTRY.md`
records that as the root `presetBaseline` default-if-absent, and state test 4 pins it for a v0.2
session (`restored v0.2 state adopts a clean baseline`). A pre-0.6.4 A/B slot is the *same situation*
— restored parameters with no recorded baseline — so it should get the same answer. Two answers to
one question was the defect.

**The fix is the branch, not a new rule:** `setMeta` treats an empty `baselineSig` as "the state being
adopted is its own clean baseline". Chosen over a special case in `applyStateSet` because
`PresetManager` is where "what a baseline means" already lives (next to `adoptRestoredState`), and
because `soundSig()` is private to it. It is provably legacy-only: the constructor, `load`,
`loadFile`, `saveUser` and `adoptRestoredState` all fill `sigAtLoad`, and `currentStateSet()` reads
it, so every undo, redo, A/B and copy snapshot carries a real baseline and never reaches the branch.
`setMeta` lost its `noexcept` — it now calls `soundSig()`, which allocates, exactly as
`adoptRestoredState` always has.

**The empty NAME was deliberately left alone.** The slot really does carry no preset; the pre-fix
`"Default"` was not a friendlier label but a factual error, since the slot's parameters were not the
defaults. Rendering it as an empty top-bar label is what "no preset" looks like, and inventing a
placeholder string would be UI copy under constraint C8. Flagged for the maintainer rather than
decided here.

**Not a gate item.** No serialization field was added, removed or renamed, and `""` keeps its meaning
("no baseline recorded"). What changed is how the reader *interprets* that absence — the same class
of change `SERIALIZATION_REGISTRY.md`'s INVARIANT contemplates when it requires absence to have a
handled default. The maintainer's review sign-off covers the direction; recorded here and in
`DOCUMENTATION_COVERAGE.md`.

**Verification.** `a legacy slot switched into reads as clean, not as permanently modified` fails with
the branch reverted (the sole failure in an otherwise green run) and passes with it. Its companion,
`...and shows no preset name rather than borrowing the other slot's`, passes both ways — §9's
unconditional `dst.name` assignment already guarantees it; it is kept as a pin so a future change
cannot restore the borrowed name without tripping something. Suite: 856 → **858 checks**, 0 failures;
`AnamorphTests` 140, unchanged.

**Also this round:** the PR description still reported the pre-§9 count (844 checks) while
`HANDOVER.md`, `RELEASE_HARDENING_PLAN.md` and ADR-0024 had moved on; all carriers now read 858. The
repository itself contained no stale `844`.

## 11. Sixth review round — a slot has to reset as a WHOLE

§9 applied "absence means default" to `dst.selection`, `dst.name` and `dst.baseline` and left
`dst.params` where it was: assigned only inside `if (ab.hasProperty (pk))` / `else if (ab.hasProperty
(legacyKey))`. So an `AB` node that exists while a slot's payload cannot be read — neither
`slotAParams` nor the pre-0.6.4 `slotA`, or a payload present but unparsable — kept the **previous
restore's sound** while its name, baseline and identity were reset around it. One slot, one project's
sound under another project's label.

The direction of the regression matters. Before this PR both halves were inherited together:
consistently stale, which is wrong but at least internally coherent. §9 made them separable, so this
is a defect the earlier rounds introduced, not one they exposed. Correctness of a *rule* is not
correctness of its *application*: applying it to three of four fields is worse than applying it to
none.

**The fix uses a mechanism that already existed.** `readSlot` now does `dst = {}` before the overlay
reads. The right default for the params is not an empty tree — `SERIALIZATION_REGISTRY.md` has said
"lazily initialised from current" for `slotAParams`/`slotBParams` since 0.6.4 — and an **invalid**
tree is how this processor already spells that: `StateSet::isValid()` is `params.isValid()`, and
`abEnsureInit()` re-seeds an invalid slot from `currentStateSet()` before `getStateInformation`,
`abSwitchTo` or `abCopyToOther` can read it. So the slot comes back seeded from the state that was
just restored, sound and metadata from one project, with no new field, no new sentinel and no change
to the A/B design. The whole-slot reset also repairs the present-but-unparsable payload, which the
old code left holding the previous tree for the same reason.

**Rejected: patching only the two branches** (`else { dst.params = {}; }`). It fixes the one shape the
review named and leaves the rule stated field by field, which is what produced the defect. Resetting
first states the invariant once and survives the next field being added.

**Not extended to a missing `AB` child.** When the whole `AB` node is absent (a v0.2 session, or a
stripped modern blob) `readSlot` is never called and `abSlot[]` — and `abActive` — persist from the
previous restore. That is the *consistently stale* shape, pre-existing since 0.6.4, and outside the
finding. Recorded here so it is a known gap rather than an oversight.

**Verification.** State test 9 gained a two-variant block — params key absent, and params payload
unparsable — each: stale a distinctive sound into slot A, park the restored session on slot B so
switching away cannot mask it, restore into the **same live instance**, then switch in and assert the
sound is the restored one. Both discriminating assertions fail with `dst = {}` removed (`got
0.900000036, expected 0.45`, twice). The companion assertion that the slot's name matches the same
restore passes both ways *in this construction* — the broken blobs keep `slotAName`, so the pre-fix
slot showed the **new** project's name over the **old** project's sound, which is the defect itself;
it is kept as a pin on the pairing. Suite: 858 → **866 checks**, 0 failures; `AnamorphTests` 140,
unchanged.

**No `CHANGELOG.md` entry.** No shipped version writes an `AB` node lacking both params keys, so
nothing user-visible changes for any session this project can produce (`CHANGELOG_POLICY` rule 3).
This is corrupt/truncated-state robustness — state test 7's category — and it is recorded in
`DOCUMENTATION_COVERAGE.md` instead. The maintainer's review sign-off covers the fix; no ADR is owed,
since no serialization field was added, removed or changed in meaning.

## 12. Seventh review round — the root preset name, and the tilde claim's third appearance

**The root had the same leak the slots did.** §9 and §11 established that A/B slot metadata never
inherits across a repeated restore. `AnamorphRoot` was never brought under that rule. Both adoption
paths fell back to the live `presets.currentName()`:

```
if (haveBaseline) presets.setMeta (restoredName.isNotEmpty() ? restoredName : presets.currentName(), …);
else              presets.adoptRestoredState (restoredName, …);   // if (name.isNotEmpty()) current = name;
```

`presets` is a processor member, so on a host's second `setStateInformation` into one instance that
fallback is the **previous project's** label: new sound, new identity, old name — and with no stored
identity, `currentIndex()`'s name scan could then tick the old project's row. This PR is what made it
reachable: §9 turned an empty preset name into a real state, and a session saved while sitting on a
nameless A/B slot writes `presetName=""`.

**Absent and empty are different answers.** That is the whole design question, and the file already
had the idiom: `haveBaseline` distinguishes a *present* `presetBaseline` from an absent one. So
`haveName` now does the same, and the adoption block resolves once:

- **absent** — a session predating the field (< 0.6) — resolves to `PresetManager::defaultName()`,
  a **constant**. Its name-fallback tick is ADR-0024 Decision 4's documented answer for state that
  carries no identity, so nothing about that decision moves.
- **present but empty** is adopted verbatim: "this state has no preset". Turning it back into a name
  would invent one, which is the §10 mistake in a different place.

`adoptRestoredState` now assigns `current = name` unconditionally. The point is not the line saved —
it is that "what the session carried" and "what absence means" stop being answered in two places, and
only the caller can see `hasProperty`.

**Rejected: adopting verbatim in both cases** (absent → `""` as well). It is the more uniform rule and
it is what §10 argued for the A/B slot, but it would change what every pre-0.6 session displays on a
*fresh* instance too — behaviour the finding did not raise — and it would empty the name that
ADR-0024 Decision 4's tie-break is defined in terms of. The narrower fix satisfies every stated
invariant; the broader one is an ADR question, not a bug fix.

**`defaultName()` is new, and small on purpose.** The constructor already hard-coded `"Default"`; it
now reads `current { defaultName() }`, so the restore path and the constructor cannot drift. No
behaviour hook, no serialization field.

**Verification.** State test 12 pins all four combinations — `presetName` empty vs absent × baseline
present vs absent — each: load a named factory preset (project A), build project B from that session
with the identity stripped so the name fallback is what resolves the tick, then restore into the
**same live instance** and assert both the name and that the tick did not stay on project A's row.
All **eight** assertions fail with the fix reverted (`got "Gentle Width"` in every case). No existing
assertion changed: state test 4's `preset name falls back to Default` still passes, because a v0.2
blob has no `presetName` property at all — which is exactly the absent/empty distinction the fix
introduces. Suite: 866 → **878 checks**, 0 failures; `AnamorphTests` 140, unchanged.

**No `CHANGELOG.md` entry.** Reaching it needs a session whose `presetName` is empty or absent
restored into an instance that already had a project open. Empty is only produced by 0.9.2 itself,
which has never shipped; absent means a pre-0.6 session. Nothing user-visible changes for any session
a released build can have written (`CHANGELOG_POLICY` rule 3).

**The tilde claim, third appearance.** A review reported `DOCUMENTATION_COVERAGE.md` as still
asserting that `saveUser` "writes outside the folder and still returns success", conflicting with §7.
It does not: that sentence was introduced in `9b67b8d` and removed in `55e062d` (the round recorded in
§8), and the ledger has carried the refutation ever since. A sweep of `docs/`, `worklogs/` and the
root `*.md` for the claim, for `saveUser`, and for `isAbsolutePath`/`getChildFile` found **no**
surviving statement of it anywhere — the finding was generated against a pre-`55e062d` tree, which the
same review batch corroborates by also reporting the check count as 844.

So there was no contradiction left to remove — but the claim has now been raised three times, which is
itself the signal. Two changes, both aimed at the re-raise rather than at a live inconsistency:
§7's paragraph now leads with the **verdict** instead of with the claim it refutes (it had been
mis-read as *reporting* the defect), and the refutation is recorded in the **code**, in `saveUser`
at the `getChildFile` call it is raised against — which per `SOURCE_OF_TRUTH` outranks every document
and is the first thing a reader of that function sees. The comment also names the distinction the
last two reports blurred: the *encode* side of the same character **was** a real defect (§9), the
*save* side is not.

Maintainer sign-off for both items in this round is recorded per the review confirmation; neither is
an `ARCHITECTURE_REVIEW_GATE` item, since no serialization field was added, removed or changed in
meaning and no ADR decision moved.

## 13. Eighth review round — an unrecognised chunk is not a restore, and A/B slot symmetry

**`setStateInformation` had a third case nobody was maintaining.** It handles two root shapes,
`AnamorphRoot` and the bare v0.2 APVTS tree. A chunk matching neither — a foreign or forward-version
root, which state test 7 has exercised since 0.8.13 — fell straight through to the adoption block:

```
abUndo[0] = {}; abUndo[1] = {};                       // "fresh session"
const auto adoptedName = haveName ? restoredName : PresetManager::defaultName();
if (haveBaseline) presets.setMeta (adoptedName, restoredBaseline, restoredSelection);
else              presets.adoptRestoredState (adoptedName, restoredSelection);
```

With nothing restored, `haveName`/`haveBaseline` are both false, so the live sound was **relabelled**
`"Default"`, its identity dropped to `unknown` (so `currentIndex()`'s name scan ticked the factory
*Default* row), its dirty-star re-baselined, and the undo history cleared — all describing a session
that never loaded.

**Only part of this was new.** §12 made `adoptRestoredState` assign the name unconditionally, which
is what put the *label* in the blast radius. The *identity* and *baseline* halves were pre-existing:
`sel = restoredSel` and `sigAtLoad = soundSig()` were always unconditional. So §12 widened a defect
rather than creating one, and the correct scope is all four fields, which is what the maintainer's
brief says.

**The fix is `else { return; }`, and the argument for it is already in the function.** Six lines
above, `if (xml == nullptr) return;` gives exactly this answer to a blob `getXmlFromBinary` cannot
parse. An unrecognised *root* is the same situation one layer down: input we do not recognise never
becomes state. Guarding the four fields individually would have been the special-case hack the brief
warns against, and would have left the undo-history clear — which is the same error in a different
member. **Disclosed:** stopping that clear was not named in the finding. It follows from the same
rule (nothing loaded ⇒ nothing to protect the user's history *from*) and discarding undo for a no-op
is strictly worse than keeping it, but it is a behaviour change beyond the four listed fields.

**Confirmed by the maintainer, 2026-08-08**, including that second half: unrecognised state data must
not mutate live preset metadata, and leaving the undo history alone for a no-op restore attempt is
accepted. So the split — a *recognised* restore clears undo, an *unrecognised* chunk does not — is a
deliberate asymmetry with a sign-off behind it, not an oversight to be "tidied" later.

**A/B slot initialisation was asymmetric.** `abEnsureInit()` seeded an invalid slot A from
`currentStateSet()` and an invalid slot B from **a copy of slot A**. Worth being precise about when
that mattered: at construction *both* slots are invalid, so slot A is seeded from the live state and
slot B copies it — the same value either way, which is why the asymmetry survived this long. It
diverged only when slot A was valid and slot B was not: an `AB` node whose `slotBParams` alone was
missing or unparsable, which §11's whole-slot reset made reachable as "invalid" rather than "stale".
Slot B then came back as a **duplicate of slot A** instead of the state just restored, and a later
save wrote that duplicate out.

Both `SERIALIZATION_REGISTRY.md` and `STATE_SERIALIZATION.md` already described the rule as
symmetric — "re-seeded from the state that was just restored" — so the code was the half that was
wrong. It is now one loop over `abSlot`, and since `currentStateSet()` builds a fresh tree per call,
the explicit `createCopy()` that kept the slots independent is no longer needed.

**API: `setMeta`'s identity-less overload is gone.** `setMeta(name, baseline)` forwarded a
default-constructed `Selection`, so *forgetting which row produced the sound* — the mis-tick ADR-0024
exists to remove — was something a caller could do without writing it down. Its only caller was a
test, which now passes `Selection()` explicitly, which is the point: the call site states the intent.
The one-argument `adoptRestoredState` overload had the identical shape and no callers at all, so it
went with it rather than being left as the same trap under a different name. No behaviour change.

**Documented: the precondition `setMeta`'s baseline fallback rests on.** The empty-baseline branch
calls `soundSig()`, which reads the **live** APVTS — so "the state being adopted is its own clean
baseline" is only true because every caller applies the parameters *first* (`applyStateSet` does
`applyStatePreservingView()` then `setMeta`; `setStateInformation` restores the `ANAMORPH` child long
before the adoption block). A future caller adopting metadata before applying its parameters would
baseline against the outgoing sound and mis-report the dirty star from then on, silently. The
signature cannot express that, so the header now states it.

**Verification, each fix reverted independently.**

| reverted | failures |
|---|---|
| the `else { return; }` | 6 — name (`got "Default"`), identity/checkmark, baseline, dirty-star, and both repeated-attempt assertions |
| `abEnsureInit`'s symmetry | 2 — exactly the slot-B variants, `got 0.700000048` (slot A's sound), slot A untouched |

State test 7's foreign-root block now runs with real metadata present (a loaded factory preset, then
an edit so the dirty-star is on) and asserts name, checkmark, baseline and dirty-star across **two**
consecutive unknown-chunk restores into one live instance. State test 9's payload block gained the
slot dimension: four variants (slot A / slot B × absent key / unparsable payload), each staging three
distinct sounds — `0.70` in the other slot, `0.90` stale in the broken slot, `0.45` live — so a
duplicate, a stale carry-over and the correct answer are all distinguishable. Suite: 878 → **893
checks**, 0 failures; `AnamorphTests` 140, unchanged.

**No `CHANGELOG.md` entry.** The A/B symmetry and the unknown-chunk guard both need a session shape no
released build writes (an `AB` node missing one slot's payload; a root tag this plug-in never
produces), so nothing user-visible changes for any session a shipped version can have saved
(`CHANGELOG_POLICY` rule 3). The `setMeta` overload removal is an internal API change with no
behaviour.

**Considered and DECLINED, so it is not re-raised: an `AnamorphRoot` carrying no `ANAMORPH` child.**
An adversarial probe run against this change set asked whether the sibling case is the same defect:
a chunk that IS a recognised `AnamorphRoot` but has no parameters child restores nothing sonically
(`params.isValid()` is false, so no `replaceState`) while still adopting metadata — so the live sound
ends up labelled `defaultName()` with a clean baseline. It reproduces. It is nevertheless **not**
changed, for four reasons that need to survive the next reviewer:

1. **It is recognised input.** The brief, and the rule this round implements, is about chunks of
   *neither* shape. An `AnamorphRoot` declares itself one of ours.
2. **Field-by-field is the deliberate design for a recognised root**, and something already depends
   on it: state test 7's `restoreWithActive` feeds an `AnamorphRoot` carrying *only* an `AB` child
   and requires the clamped `active` to be applied. Aborting the restore when the params child is
   missing would break that, and would be the special-case hack the brief warns against.
3. **The obvious alternative re-introduces the bug this PR spent three rounds removing.** "Skip the
   adoption when `params.isValid()` is false" means a repeated restore into a live instance keeps the
   *previous* project's name, identity and baseline — exactly the leakage §12 closed. Neither answer
   is clean for a chunk that carries structure but no sound; the current one at least never leaks
   across projects, and its metadata comes from the chunk in front of it.
4. **No shipped version can write one.** `getStateInformation` always appends
   `copyStateWithRawValues()`, so this is hand-edited/corrupt territory — state test 7's remit.

The same probe re-raised the v0.2 branch leaving `abSlot[]` and `abActive` from the previous restore.
That is the *consistently stale* gap already recorded at the end of §11: pre-existing since 0.6.4,
both halves of each slot from the same (old) project, and outside this round.

Maintainer sign-off for every item in this round is recorded per the review confirmation. None is an
`ARCHITECTURE_REVIEW_GATE` item: no serialization field was added, removed or changed in meaning, no
ADR decision moved, and the wire format is byte-compatible in both directions.

## 14. Ninth review round — the empty label gets a placeholder, and two documentation items

**The empty preset label.** §10 decided that a pre-0.6.4 A/B slot shows an empty name rather than the
factually-wrong `"Default"`, and flagged the resulting blank top-bar button for the maintainer as UI
copy under constraint C8. That call has now been made: the button renders **No Preset**.

The whole question was *where*. `refreshPresetDisplay` builds `name + marker` from
`PresetManager::currentName()`, and `currentName()` has two other readers — `getStateInformation`
writes it to the serialized `presetName` property, and `showSavePreset` pre-fills the Save Preset
field with it. Putting the placeholder in the accessor, which is the tidier-looking option, would
therefore have written `"No Preset"` into every session saved from a nameless slot and offered it as
the default *preset file name*. It stays in the editor:

- **Display:** `refreshPresetDisplay` substitutes the placeholder before measuring, so abbreviation
  and clipping apply to what is actually drawn.
- **Model:** `currentName()` still returns `""`, `selection()` is still `unknown`, `currentIndex()`
  still ticks nothing. No serialization meaning moved and no identity was invented.
- **Save dialog:** untouched, and commented as deliberate — an empty field the user types into is the
  right pre-fill for a state that has no preset.

State test 5 gained the assertion that closes the loop: after switching into a legacy slot, a re-save
must still write `presetName=""`. With the substitution moved into `currentName()` as a control, four
assertions fail — the two existing name pins in tests 5 and 12, plus this new serialized-property
one — which is exactly the guard rail wanted, since that is the refactor a future reader is most
likely to attempt.

`ADR-0024`'s Consequences said *"No user-visible string was added (constraint C8): the ids never
surface."* That is now false in its first half, so it is corrected in place rather than left to drift:
one string was added, with the sign-off date and the reason it lives in the editor. The `CHANGELOG`
entry for the legacy-slot fix now names the label it produces.

**`TESTING.md`'s restore-path count.** The procedure said *"in EVERY one of those seven paths — the
six that go through the reload helper plus the A/B slot check"*. Counting `restoreInto` call sites in
`testPresetIndicatorIdentityAcrossRestore`: factory, factory-fallback, user, user-nested,
user-fallback, pre-0.9.2, and the `~`-named round-trip — **seven** — plus the A/B slot check, so
**eight**. Both numbers were one behind; the tilde case (§9) was added after that sentence was
written. ADR-0024 and the PR description already said eight. Tests are the source of truth here, so
the prose moved, not the tests; the fallback list in the same sentence gained the tilde case it had
also missed.

**The `setMeta` ordering invariant** was already recorded in `PresetManager.h` (added in the previous
round: *"PRECONDITION for that fallback, not enforced by the signature…"*). What was missing is the
statement at the place a future edit would actually break it — `applyStateSet`, whose two lines *are*
the order. It now says so, and says what breaks: swapping them baselines a pre-0.6.4 slot against the
outgoing sound and leaves its dirty-star wrong with nothing to catch it.

**Verification.** Suite: 893 → **894 checks**, 0 failures; `AnamorphTests` 140; full build of every
target exits 0. The new assertion was verified to fail (with three others) under the
placeholder-in-the-model control. No serialization field, ADR decision or preset-identity rule
changed; the only behaviour difference is one string drawn in the top bar.

Maintainer sign-off for all three items is recorded per the review confirmation, including the C8
sign-off for the new UI string.

## 15. Tenth review round — PRIVACY.md was the one carrier left describing two of three cases

`PRIVACY.md` is the document that claims to enumerate *everything* that reaches disk, so an
incomplete list there is a different kind of error from an incomplete list anywhere else. Its preset
reference bullet said a reference is a filesystem path **"only when"** the preset was opened with
**Load Preset…** from outside the preset folder or from a sub-folder of it. `encodeSelection` has a
third path-storing condition, added in §9: a preset sitting *directly in* the folder whose file name
`juce::File::isAbsolutePath` accepts — a leading `~` on POSIX — also stores its absolute path.
ADR-0024 and `SERIALIZATION_REGISTRY.md` both gained that condition at the time; `PRIVACY.md` did
not, so an "only when" claim in the privacy disclosure was false.

The bullet now states the **rule** rather than a list of symptoms — file name alone when the preset
is a direct child *and* its name cannot be mistaken for a path, full path otherwise — and then gives
the three situations that produce the path form. Stating the rule first is what stops this recurring:
a list can fall behind the code, a complement of the code's own condition cannot. The third case is
called out as the one that does **not** involve **Load Preset…**, because the previous wording tied
path storage to that dialog and a reader would otherwise conclude a drop-down preset is always safe.

Documentation only: no encoding logic, no serialization behaviour, no identity rule changed. No
`CHANGELOG` entry — nothing user-visible changed (`CHANGELOG_POLICY` rule 3); this corrects a
statement about behaviour that has been in the change set since §9.

**Maintainer sign-offs recorded in the same round**, at the records they constrain rather than as a
list here:

- the **No Preset** placeholder is editor-presentation only — model returns an empty name,
  serialization stores an empty `presetName`, and the placeholder must never become persisted
  metadata; and C8 is satisfied *because* the string is none of those things. Recorded in ADR-0024
  §Consequences, next to the string it governs, with state test 5 named as what fails first if it
  moves.
- the unknown-root-chunk behaviour, **including** leaving the undo history untouched for a no-op
  restore attempt. Recorded in §13 beside the disclosure, so the recognised/unrecognised asymmetry
  reads as a deliberate split with a sign-off rather than an inconsistency.
