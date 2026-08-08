# DOCUMENTATION_COVERAGE.md

Permanent documentation-coverage audit. **Future contributors/AI must update this on every
documentation-affecting change** (`docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md`).

Coverage = how well the module/topic is documented. Confidence = strength of the evidence behind
that documentation (Verified / Partially Verified / Unverified / Not Supported).

Last updated: for the **0.9.2 change set** (2026-08-07) — the first `src/` change since 0.9.0.
Four changes, one investigation, three new regression tests, and one governance amendment.

**Governance: `TESTING_POLICY` rule 1 gains a narrow exception (ADR-0025).** The rule ("every bug fix
ships a regression test") was stated unconditionally, while the project has in practice shipped one
fix — INC-010 — without one, because no automated surface reaches a defect that only exists while a
modal child is open and its owner is destroyed. That deviation had been recorded in a Procedure and
in this ledger, both of which rank **below** Policy, so nothing at or above Policy level described
what the project actually does. **ADR-0025** closes that: the default is unchanged, the release gate
is untouched, and the exception is available **only** where the repository has no stable automated
surface reaching the defect (GUI/component lifetime, host-owned UI behaviour, OS-level asynchrony) —
never for a test that is merely hard to write. Invoking it requires four disclosures (why no test
exists, what replaced it, where the gap is tracked, whether infrastructure could close it), and the
exception lapses when the surface appears. `docs/procedures/TESTING.md` §"Gaps in the automated
coverage" is named as the register — the role it already played for the AU-conformance and
golden-audio gaps that `KNOWN_ISSUES.md` KI-014 and `RELEASE_HARDENING_PLAN.md` RH-F3 cite. Per
`ADR_POLICY` rule 5 / `SOURCE_OF_TRUTH`, the ADR is the instrument that makes the Policy change; per
rule 1 it is registered in `ADR_INDEX.md`. **A one-off waiver was explicitly rejected** — the goal
was a rule that describes the engineering reality, not an escape hatch for one entry.

**Preset drop-down lifetime + crash (`src/PluginEditor.cpp`).** Filed as **INC-010**. Three facts,
separated after an adversarial re-read of the pinned JUCE source — the first draft of this entry
(and of the code comment) got the mechanism wrong and is corrected here rather than left standing.
(1) The **leftover menu** is not an oversight in JUCE: `MenuWindow::windowIsStillValid()` dismisses when
`componentAttachedTo != options.getTargetComponent()`, but both are `WeakReference` to
`presetName`, so they null *together* and the comparison is false. (2) The **lost styling is not a
use-after-free** — the MenuWindow copies the look-and-feel into its own `Component::lookAndFeel`
slot (`juce_PopupMenu.cpp:366`), a `WeakReference` that nulls and falls back to `LookAndFeel_V4`
(the `PopupMenu` itself is a stack local, gone long before the editor). (3) The **crash** is the raw `this` in the callback.
The fix is `withParentComponent (this)` (JUCE parents the MenuWindow as a CHILD, cancelled with
result 0 by `ModalComponentManager` on destruction *or* hide; `Component::getLookAndFeel()` then
resolves our LookAndFeel by walking the tree) plus a `SafePointer` callback — which is **not**
redundant, since that cancel is asynchronous and the menu's 20 Hz timer can still emit a non-zero
result in the gap. Two side effects of parenting were neutralised in the same change:
`withMaximumNumColumns (1)` (a parented menu is budgeted against the editor, and JUCE adds COLUMNS
before it scrolls — past ~14 user presets the list would have silently gone two-column) and a
no-op `drawResizableFrame` (JUCE paints a frame over the border ring only when parented). The
"Load Preset…" file chooser, reachable from the same menu, got the same `SafePointer` guard.
No regression test: the failure is a GUI-lifetime use-after-free, which `tests/state_tests.cpp`
cannot express — the harness links the editor but never instantiates it. This is **not** a one-off
waiver: **ADR-0025** amends `TESTING_POLICY` rule 1 with a narrow, disclosure-bound exception for
defects that no automated surface reaches, the default stays "every bug fix ships a regression test",
the release gate is untouched, and INC-010 is the first invocation. Its four required disclosures —
why no test exists, what replaced it (removal of the lifetime by construction, plus a `SafePointer`
for the residual asynchronous window), where the gap is tracked, and what infrastructure would close
it — are recorded in `TESTING.md` §"Gaps in the automated coverage", which that ADR names as the
register, and summarised in INC-010's Prevention field. Synced:
`CHANGELOG.md`, `README.md`, `HANDOVER.md`, `POSTMORTEMS.md` (INC-010).
**Reported, not fixed (C6):** the combo-box popups store an editor-member LookAndFeel the same way
and would lose styling identically, but their callback is `ModalCallbackFunction::forComponent`,
i.e. already SafePointer-based — no memory-safety defect, no reported symptom, seven call sites
across two LookAndFeel subclasses. Out of scope. Likewise `SpectrumImager`'s `freqEditor`
`onFocusLost` can fire during teardown; it is owned by the editor it belongs to, so it is a
different (and lesser) class of hazard.

**Factory-preset identity (`src/PresetManager.{h,cpp}`, `src/PluginProcessor.{h,cpp}`).** The
preset list was searched by NAME and the factory block is list-front, so a user preset sharing a
factory preset's name could never hold the drop-down tick. A factory preset now carries an
immutable internal `factoryId` and a user preset is identified by its file
(`PresetManager::Selection`); the menu, the top bar and the Save Preset field still show the
**name**. The identity rides on `StateSet` through A/B and undo, and — after the maintainer
supplied the Architecture-Review approval the gate requires — **also with the session**, so
reopening a project ticks the row that produced the sound. Six additive metadata fields (3 in
`AnamorphRoot`, 3 per A/B slot); **user preset FILES are unchanged**, parameter restore is
independent of identity restore, and anything unresolvable ticks nothing rather than a same-named
substitute. Recorded as **ADR-0024** (registered in `ADR_INDEX.md`), whose original "never
serialized" clause is reversed by a dated **Amendment** that keeps the original text verbatim above
it — the reversal, its approval and its fallback table are exactly what a future agent would
otherwise re-litigate straight into a Hard Stop. Synced: `SERIALIZATION_REGISTRY.md` (six new field
rows), `SESSION_COMPATIBILITY_POLICY.md` (rule 4's round-trip list), `API_REFERENCE.md`,
`USER_MANUAL.md` §7.2, `TESTING.md`, `TESTING_POLICY.md`, `RELEASE_HARDENING_PLAN.md`,
`REPOSITORY_MAP.md`, `HANDOVER.md`, `CHANGELOG.md`, and **`PRIVACY.md`** — that document states
every claim about what reaches disk, and the session can now carry a preset **file name**, or an
absolute **path** in the one case where the selected preset was opened from outside the preset
folder. The path case is the same class as the Standalone's `lastStateFile` entry the document
already carves out, and it is now carved out alongside it, with the reason the in-folder case stores
a name instead. `ADR-0008` gained the third `StateSet` field and
re-based line anchors (a factual re-sync, not a reversal; ADRs stay append-only). State tests 10,
11 and 12 pin the live behaviour, the id integrity and the whole restore matrix including every
fallback.

Six defects found by review and fixed before merge, each with its own assertion, and every one
verified to fail with its fix disabled. Three from the first adversarial pass: the identity scan
**fell through** to the name scan when the identity was known but absent from the list, so a `.anamorph`
loaded from outside the preset folder ticked the same-named factory row — the exact mis-tick this
change exists to remove; `saveUser` never re-baselined the processor's undo snapshot, so the first
undo after a save restored the pre-save name/identity (fixed with an `onSaved` hook →
`syncCommitted()`, which creates no undo step because a save is not a sound change — and the same
gap existed for a preset switch whose sound is identical to the current one); and `readSlot` left
a stale identity on an A/B slot when a host restored a second session into one live instance.

Three more from the maintainer's follow-up review. **`saveUser` did not flush pending undo
coalescing** before re-baselining: `syncCommitted()` clears `pendingGestureCommit`, so a knob
gesture that had closed but not yet been polled was folded into the new baseline with no undo step
— the edit silently stopped being undoable. `onSaved` now does `pollUndoCoalesce(); syncCommitted();`,
matching the two other program-state jumps (`onAboutToLoad`, and `undo()`/`redo()`). **A factory id
that fails to resolve** applied the plain defaults and then adopted the factory identity anyway;
`load()` now resolves it BEFORE the undo bracket opens — the same rule the user-preset parse three
lines above already followed — asserts it, and fails as a clean no-op otherwise. State test 11 pins
the invariant that makes the assert unreachable: ids present, unique, and every one resolving.

**Two more from a third, independent review of the finished change set**, both introduced by the
plug-in-state work and both now fixed with a discriminating assertion. `encodeSelection` used
`juce::File::isAChildOf` to decide whether a preset lives in the preset folder — but JUCE implements
that **recursively**, so a preset opened from a **sub-folder** was stored by bare name and decoded to
a *different*, same-named file directly in the folder, breaking the `decode(encode(s)) == s`
invariant the header and the ADR both state. Now a **direct-child** test, which is also the honest
one: `refresh()` scans non-recursively, so only a direct child can ever be a menu row. And the new
`else` branch of `commitPresetSwitchUndoStep` did not clear redo, unlike the `if` branch one line
above whose comment states the rule — so undo, then select the same-sounding preset on the other row,
then Redo, and the tick jumped back out of an abandoned `StateSet`. (That branch then had to be
narrowed again — see below.)

**Raised and REFUTED, recorded so it is not re-raised:** a `saveUser` defect for preset names with a
leading `~`. The JUCE facts are real as far as they go — `getChildFile` short-circuits for anything
`isAbsolutePath` accepts, and on macOS/Linux a leading `~` survives `createLegalFileName` — but the
write cannot succeed. `replaceWithText` does not open the target: it writes a hidden sibling built
from `getParentDirectory()`, and for a separator-less path that is the path itself, which is not a
directory. `createLegalFileName` strips `/` and `\`, so every tilde-leading name hits the same
degenerate parent. `saveUser` therefore returns **false**, nothing is written anywhere, and the Save
dialog stays open with the text intact — the save fails *visibly*, which is exactly what the proposed
guard was meant to produce. Verified empirically against the pinned `juce_core` for `~foo`, `~/foo`,
`~` and `~root`, with a normal name as the control. **No code change; no defect.** The refutation and
its probe are in `worklogs/PRESET_MENU_AND_IDENTITY_v0.9.2.md` §7.

**Redo invalidation, narrowed after review.** The `else` branch above cleared redo unconditionally,
so *re-picking the row that is already ticked* — identical sound, identical identity — silently threw
away a redo the user was about to press. It now clears redo only when the identity actually **moved**
(`presets.selection() != committed.selection`), which is the only case where a surviving redo entry
could drag the tick off the row just chosen. The same-sound/**different**-row case still invalidates,
and its assertion is unchanged; a second assertion covers the re-select case, and both were verified
against the pre-fix behaviour.

**The encoder's second ambiguity: a direct-child name that `isAbsolutePath` accepts.** The
`isAChildOf` fix above closed the *nesting* route into a broken `decode(encode(s)) == s`; a leading
`~` was the other one. `decodeSelection` reads a bare name back through
`presetDirectory().getChildFile(name)`, and `getChildFile` short-circuits to the raw `File`
constructor for anything `isAbsolutePath` accepts, so `~foo.anamorph` sitting **directly in** the
preset folder decoded to a literal relative path and the row lost its tick on reload. This does not
contradict the `saveUser` refutation recorded above — that refutation is about the **save** path,
which genuinely cannot create such a file; `USER_MANUAL.md` tells users to manage presets as files, so
a hand-copied one reaches `refresh()` and can be loaded and encoded like any other. The encoder now
requires the bare name to be unambiguous (`! juce::File::isAbsolutePath (name)`) and otherwise takes
the absolute-path branch it already shares with outside-the-folder and sub-folder presets. No preset
file format change, no canonicalisation, no weakening of the no-name-fallback rule. State test 12
gained the round-trip case; verified to fail with the fix disabled.

**A/B slot metadata now follows "absence means default".** `readSlot` read `dst.name` and
`dst.baseline` *inside* the `hasProperty("slotAParams")` branch, so the pre-0.6.4 legacy shape — params
only — left both untouched. `abSlot[]` are processor members and a host may call
`setStateInformation` on one live instance repeatedly, so a legacy session restored after a modern one
kept the **previous** session's preset name and dirty-baseline attached to freshly restored
parameters. Both reads moved out of the branch, next to the identity read that already had this right.
The resulting defaults (`""` / `""`) are the ones `SERIALIZATION_REGISTRY.md` already documented, so
the code caught up to the ledger; no field was added, removed or renamed. **An existing assertion was
changed, not merely added:** state test 5's `slotAName == "Default"` under the comment "legacy slot
keeps pre-restore meta" *pinned the defect* — it described a fresh instance's construction snapshot as
if it were the rule. It now asserts the default, alongside a repeated-restore case that shows why.

**"No baseline recorded" is not "modified" (fifth review round).** The A/B fix above left a second
half unfinished: a pre-0.6.4 slot restores with an empty *baseline* as well as an empty name, and
`isDirty()` is `soundSig() != sigAtLoad`. `soundSig()` is never empty, so an empty baseline compares
unequal to every possible sound and the slot read as **permanently modified** — with no name, the top
bar rendered a bare ` *`: a modified-marker against a preset that does not exist. The project already
has a rule for "restored parameters, no recorded baseline": `adoptRestoredState` sets the restored
state as the clean one, which `SERIALIZATION_REGISTRY.md` documents for the root `presetBaseline` and
state test 4 pins for a v0.2 session. `setMeta` now applies that same rule, so it is one rule with one
spelling instead of two answers to the same question. Unreachable from undo, redo, A/B and copy —
every in-memory producer fills the baseline — so the branch is legacy-restore only. The *empty name*
was left as-is deliberately: the slot genuinely has no preset, and the pre-fix "Default" was a
factual error (the slot's parameters were not the defaults). Maintainer confirmation of the direction
is recorded per the review sign-off; no serialization field changed and `""` keeps its meaning
("absent"), so this is a read-path interpretation, not an `ARCHITECTURE_REVIEW_GATE` item.

**A slot must reset as a whole, or its two halves come from two projects (sixth review round).** The
"absence means default" rule was applied field by field — `dst.selection`, `dst.name` and
`dst.baseline` — but `dst.params` was still only touched inside the two params-present branches. An
`AB` node that exists while a slot's payload cannot be read (neither `slotAParams` nor the pre-0.6.4
`slotA`, or a payload that fails to parse) therefore kept the **previous restore's sound** while its
metadata was reset around it: one slot holding one project's sound under another project's label.
Before this PR both halves were inherited together — consistently stale, which is wrong but not
*mixed* — so this was a defect the earlier rounds introduced, not a pre-existing one. `readSlot` now
resets the slot to a default `StateSet` first and overlays what the node carries. The params default
is not an empty tree but **"lazily initialised from current"**, which the registry already recorded
and which `abEnsureInit()` already implements off `StateSet::isValid()` — so no new mechanism, no new
field, and the slot comes back seeded from the state just restored. The reset also covers the
present-but-unparsable payload for free. Both cases are pinned by state test 9 and were verified to
fail with the reset removed. **No `CHANGELOG.md` entry:** no shipped version writes an `AB` node
lacking both params keys, so there is no user-visible change to report under `CHANGELOG_POLICY` rule
3 — this is corrupt/truncated-state robustness, the category state test 7 covers. Maintainer
confirmation of the direction is recorded per the review sign-off.

**The root preset NAME had the same leak as the slots (seventh review round).** `readSlot`'s rule —
metadata never inherits across a repeated restore — was not applied to `AnamorphRoot`. Both adoption
paths fell back to the live `presets.currentName()`: the `haveBaseline` branch via
`restoredName.isNotEmpty() ? restoredName : presets.currentName()`, and `adoptRestoredState` via
`if (name.isNotEmpty()) current = name;`. `presets` is a processor member, so on a host's second
`setStateInformation` into one instance that is the **previous project's** label — new sound, new
identity, old name, and with no stored identity the name scan could then tick the old project's row.
This became reachable *because* of this PR: an empty preset name is now a real state (a session saved
while sitting on a nameless A/B slot stores `presetName=""`).

**Absent and empty are different answers**, and only `setStateInformation` can tell them apart — the
distinction `haveBaseline` already drew for the sibling field. Absent means a session predating the
field (< 0.6) and resolves to the new `PresetManager::defaultName()`, a **constant**, whose
name-fallback tick is the documented ADR-0024 answer for identity-less state; present-but-empty is
adopted verbatim. `adoptRestoredState` now assigns the name unconditionally, so "what the session
carried" and "what absence means" stop being decided in two places. No serialization field changed,
and no existing assertion moved — state test 4's `preset name falls back to Default` still passes,
because a v0.2 blob has no `presetName` property. Four cases (empty/absent × baseline/no-baseline)
are pinned in state test 12; all eight new assertions were verified to fail with the fix reverted.
Maintainer confirmation of the direction is recorded per the review sign-off.

**Re-raised and re-refuted: the `~foo` `saveUser` claim.** A later review reported this ledger as
still asserting that `saveUser` "writes outside the folder and still returns success". It does not,
and has not since the round recorded in `worklogs/…v0.9.2.md` §8 — the sentence was removed there and
the entry above has stated the refutation ever since (introduced `9b67b8d`, corrected `55e062d`). The
repository holds no conflicting description: `DOCUMENTATION_COVERAGE.md` and worklog §7 both say the
write fails and `saveUser` returns **false**, and §9 records that the *encode*-side sibling — a
`~`-named file a user copies in by hand — was a separate, real defect. Because the claim keeps coming
back, the refutation now also lives in the **code**, at the `getChildFile` call it is raised against;
per `SOURCE_OF_TRUTH` that outranks every document and is the first thing a reader of
`saveUser` sees.

**Documentation follow-up on the identity match (no behaviour change).** ADR-0024's Consequences now
state the three properties plainly: the match is a raw path-string compare with **no**
canonicalisation (`getLinkedTarget()` considered and rejected — it resolves symlinks but not
`/private/var`, mount aliases or UNC spellings, trading a predictable "no tick" for a partial one);
cross-machine resolution holds only for the name-encoded case, because a stored absolute path fails
`isAbsolutePath` on the other platform; and a file name that looks like a path is stored as a path.
`SERIALIZATION_REGISTRY.md` gained both encoder conditions and the raw-compare note.
`API_REFERENCE.md`, `STATE_SERIALIZATION.md` and the ADR had their `src/` citations re-anchored where
this round's edits moved them.

**Declined, with evidence: restoring `PopupMenu::setLookAndFeel (&lnf)`.** The stated goal was to
make item *measurement* use `AnamorphLookAndFeel` — but it already does. `MenuWindow` parents itself
at `juce_PopupMenu.cpp:370-372` and only then builds items (`:457`), and `ItemComponent` calls
`parent.addAndMakeVisible` *before* `getIdealSize` (`:139-146`), which resolves through
`getLookAndFeel()`. Restoring it would instead re-arm the `~LookAndFeel` assertion, which fires on
any live `WeakReference`: `lnf` is a member and so is destroyed *before* this editor's `Component`
base, i.e. before the menu is asynchronously cancelled. The only two calls that still see the
default look-and-feel are bound one line before the parenting — `setOpaque` (same answer,
`colours::bgPanel` is opaque) and `preparePopupMenuWindow` (a no-op we do not override). A **third**
resolves through it earlier still and is **load-bearing**: `getParentComponentForMenuOptions`
(`juce_PopupMenu.cpp:353`, in the member-init list), whose return value is what installs the parent —
so a process-global default look-and-feel overriding it to return `nullptr` would silently discard
the parenting. Every JUCE look-and-feel inherits `LookAndFeel_V2`'s pass-through, and the default is
not ours to control; recorded in the code as the latent trap it is.

**`focusSaveNameField`'s comment was stale, not its behaviour.** It justified the retry by the
preset menu's own desktop window owning OS focus — which parenting removed. The retry stays, because
the abort it works around is not menu-specific: `Component::takeKeyboardFocus` gives up while the
plug-in's own peer is not OS-focused, and whether it is, at that instant, is the host's call (the
failure KI-009 tracks in REAPER). Comment rewritten; the bounded 4 × 50 ms retry is untouched.

**`Window Size` → `UI Scale` (display name only).** `PARAMETER_COMPATIBILITY_POLICY` permits a
display-name change; the identifier `int_uiScale` and the pre-0.8.4 legacy APVTS id `uiScale` its
migration reads are untouched, so this is not a serialization change and needs no ADR. Recorded
with the repo's own footnote form in `PARAMETER_REGISTRY.md` (`※`, mirroring the `Haas Side` →
`Haas Focus` precedent). Synced: `PARAMETER_REFERENCE.md`, `REPOSITORY_MAP.md`, `USER_MANUAL.md`
(×3), `README.md`, the six source comments naming the control, and a **clarifying annotation** in
ADR-0010 — the ADR body is otherwise left verbatim, since ADRs are append-only.

**Installer component titles.** macOS `<choice title=…>` → *VST3 Plug-in* / *AU Plug-in* /
*Standalone Application*; the two Windows destination-page **labels** → *VST3 Plug-in folder* /
*Standalone Application folder*. Prose sentences keep lowercase "plug-in"/"application" (the
`MsgBox` strings, the `:90` parenthetical, every legal/manual use). The Windows `[Components]`
descriptions ("Install VST3" / "Install Standalone") contain neither phrase and are unchanged, so
the five doc quotes of them stay valid. No CI or self-check assertion matches a title — the macOS
self-check matches `<choice id=…>` and the package identifiers. Synced: `PACKAGING.md`,
`INSTALLATION.md` (the macOS Component table, which had drifted twice over: *AU (Audio Unit)* and
*Standalone app* never matched the installer even before this change — corrected here in the same
pass, and reported rather than silently changed).

**macOS key auto-repeat: investigated, no code change (KI-017).** Holding a letter or digit in a
text field types once and stops while punctuation repeats. Traced through the pinned JUCE: a
focused `TextEditor` makes `findCurrentTextInputTarget()` non-null, so every key-down goes to
`[inputContext handleEvent:]` first and printable characters return via `insertText:` — the path
macOS press-and-hold and the IME own — while "special" keys return via `doCommandBySelector:` and
repeat normally. Everything inside the plug-in was eliminated by inspection (the bounded focus
retry, `setSelectAllWhenFocused`, the 24 Hz timer, the VBlank attachment, the UI-scale transform,
both `getCurrentModifiersRealtime` call sites). Filed as **KI-017** with the two discriminating
checks; no `CHANGELOG` entry, since nothing user-visible changed (`CHANGELOG_POLICY` rule 3).

**First-tag renumbering, swept this time.** The 0.9.0 → 0.9.1 renumbering was recorded here as
incomplete; the 0.9.1 → 0.9.2 one repeated it and is now closed in the same pass:
`CHANGELOG.md` preamble, `CHANGELOG_POLICY.md`, `FUTURE_RISKS.md` (×2), `COMMERCIAL_STATUS.md`
(×3) and `RELEASE_HARDENING_PLAN.md` (×5) all named v0.9.1 as the first annotated tag. Neither
`FUTURE_RISKS.md` nor `COMMERCIAL_STATUS.md` had been touched by the version bump at all.

Prior: for the **third review pass on the 0.9.1 change set** (2026-07-30). Three findings
fixed, three were confirmations. No `src/` change.

**The `Unreleased` guard had a residual hole.** It rejected only a heading containing the word
`Unreleased`; a heading written as a bare `## [0.9.1]` is equally undated and would have published
undated notes. The check now requires an **ISO date** in the heading, which subsumes both cases and
matches the format every existing entry already uses. Exercised against five heading forms
(`— Unreleased`, bare, em-dash-dated, hyphen-dated, two-digit minor) plus both real CHANGELOG
sections. Synced: `release.yml` (tag branch + rehearsal warning), `RELEASE_PROCESS`, `CI_CD`,
`HANDOVER`.

**`FUTURE_RISKS` was edited for 0.9.1 but kept a v0.9.0 version-sync lead**, leaving it and
`KNOWN_ISSUES` disagreeing about which version the status documents are synced to. Re-led to
v0.9.1, recording that ADR-0023 adds no new *risk* — the one-time session break is a known issue
(KI-016), not a forward-looking one.

**More reported-then-corrected line drift (C6), in a document this change set touched.** `BUILD.md`
carried three stale `CMakeLists.txt` citations the previous pass did not report:
`ANAMORPH_BUILD_TESTS` `:27,212` → `:27,219`, `ANAMORPH_BUILD_NUMBER` `:183` → `:188`, compile
definitions `:185-194` → `:190-199`. Sweeping for the same class found a fourth, shared by
`RELEASE_PROCESS` and `RELEASE_POLICY`: the versioning citation `:181-187` → `:186-192`. All five
re-verified against the file. (`build.yml:60,180,438` for the build-number Configure steps was
checked and is correct.)

**Confirmed, no change:** the auval recipe is consistent in every maintained carrier; the guard
interacts correctly with the verbatim notes extraction; the `curl`/`unzip` fix matches what
`run-pluginval.sh` invokes and its re-based citations all resolve.

Prior: for the **second review pass on the 0.9.1 change set** (2026-07-30). Six findings
fixed, no `src/` change.

**The exception was over-claimed.** ADR-0023's status line said the `ARCHITECTURE_REVIEW_GATE` item
and the `COMPATIBILITY_POLICY` exception were "both now cleared". The exception needs all four
conditions and **condition 3 — the Release Compatibility Checklist — has never been completed for
this release**, as `HANDOVER` says in the same change set. The ADR now carries a per-condition table
marking 3 **OPEN** (a release-time gate: it blocks the tag, not the merge), and the exceptions table
in `COMPATIBILITY_POLICY` says the same. The ADR's own rule — "must not claim a green gate it did
not observe" — was what the blanket sentence broke.

**The carve-out was scoped wrongly.** 2a read "no annotated tag exists for any build carrying the
old identity", and the exceptions table called it "spent", which would have closed the only route
for a later `PLUGIN_CODE` or `PRODUCT_NAME` change (a product rename before the first tag) on the
strength of a manufacturer-code change. 2a is a **condition on the state of the world, not a token
an exception consumes**: true while no tag exists — for every identity field at once — and
permanently false from the first tag, again for every field at once. Reworded in
`COMPATIBILITY_POLICY` and ADR-0023.

**The 0.9.0 → 0.9.1 first-tag renumbering was incomplete.** Still claiming v0.9.0 as the first tag:
`RELEASE_PROCESS` (§After release), `CHANGELOG_POLICY` rule 2, `FUTURE_RISKS` RISK-003,
`RELEASE_HARDENING_PLAN` (Version-management row, RH-R6, RH-PR-8 row, RH-F3 timing), `HANDOVER`
(Branch Strategy). All corrected.

**`RELEASE_PROCESS` contradicted the new pipeline check.** It still told maintainers an undated
`Unreleased` heading "would be published with that word in it" and that "the validation only checks
that the section *exists*" — the same change set had made that a fail-closed rejection. Corrected,
with the two practical consequences spelled out (date the heading **in the tagged commit**;
rehearsals only warn).

**The guard's justification was wrong about *what* it protects.** The release **title** is set
separately (`--title "Anamorph <version>"`); the extracted section becomes the **notes body**. The
check is right and stays; the wording is fixed in `release.yml`, `CI_CD`, `RELEASE_PROCESS` and
`HANDOVER`.

**Reported-then-corrected line-number drift (C6).** The previously reported `ADR-0001` citation
`CMakeLists.txt:62-73` → `:124-135` (and `:149-166` → `:228-237` for the tests-link-the-core
range), plus the same drift in `TROUBLESHOOTING` (`:115-125` → `:124-135`), found by the reviewer.
Both re-verified against the file. Reporting came first, in the prior pass; this is the correction.

**Stale version snapshots refreshed:** `HANDOVER`'s snapshot preamble (v0.9.0 was never tagged;
0.9.1 is the release in preparation and the first tag) and `COMMERCIAL_STATUS` (§Last reviewed,
§2, §6). `COMMERCIAL_STATUS` keeps its **2026-07-26** review date deliberately — its substance
(product model, distribution model, open owner/legal decisions) is untouched by a version
renumbering, and a review date that moves on every version bump stops meaning anything.

Prior: for the **ADR-0023 sign-off + the pluginval dependency fix** (2026-07-30). Two
changes, neither touching `src/`.

**ADR-0023 is now `Accepted`.** The maintainer signed off the Architecture Review and performed the
Level-5 identity check on 2026-07-30 — the 0.9.1 build registers under the new identity in a host,
and `auval -v aufx Anmr RTec` was run on macOS. That was the only check that exercises the change,
and no automated gate in this repository could have stood in for it, since nothing in the suite
observes plug-in identity. Status synced in ADR-0023 (incl. its *Verification performed* section),
`ADR_INDEX`, `COMPATIBILITY_POLICY` (exceptions table), and `HANDOVER` (Current Version, Release
Status, Known Blockers — the v0.9.1 tag blockers drop from five to **four**; the remaining four are
the missing licence plus three `RELEASE_POLICY` preconditions, all carried unchanged from the
v0.9.0 audit and all still requiring a human). Deliberately **not** upgraded: the ADR's
`AnamorphTests` / `AnamorphStateTests` / pluginval rows stay `Unverified in-repo` — the sign-off
covered the identity behaviour, which is what needed a human; the machine-checkable gates are
reported by CI on the change set, and an ADR must not claim a green gate it did not observe (C2/C7).

**`scripts/setup-linux.sh` now installs `curl` and `unzip`.** `run-pluginval.sh` calls both to
fetch and extract the pluginval release, and neither was installed — `libcurl4-openssl-dev` is the
development headers, not the CLI. GitHub-hosted runners preinstall both, which is exactly why the
gap never showed in CI and would only have bitten on a fresh machine or a minimal container, i.e.
the case this script exists to cover. Found while fixing the same defect in the sibling repository.
Synced: `BUILD.md` (§Linux dependencies — package list plus a paragraph separating the three
pluginval-only packages from the build dependencies; §Network domains), `TROUBLESHOOTING.md` (a new
`command not found` row). The script gained lines, so the `setup-linux.sh:NNN` citations in both
documents were re-based: package list `:24-32` → `:29-38`, EGL note `:13-15` → `:18-20`, network
domains `:8-13` → `:8-12`, webkit `:31`/`:36` → `:37`/`:42`, `libegl-dev` `:30` → `:36`.

Prior: for the **vendor manufacturer-code change** (2026-07-30, on top of `main` @
`c0fca30`). **No `src/` change; DSP, parameter surface and serialized state bit-identical to
0.9.0.** `PLUGIN_MANUFACTURER_CODE` changes `Anmf` → `RTec` (`CMakeLists.txt:153`) so the vendor
code spells RollyTech rather than the first product, ahead of the second product line member
(Anabasis) adopting the same value. Version bumped to **0.9.1** (`CMakeLists.txt:14`). The code is
host-facing identity — the AU component's manufacturer field, and an input to JUCE's VST3 class
UID — so pre-0.9.1 sessions report the plug-in as missing; that is documented, not fixable, and
one-time. Added: **ADR-0023** (options incl. "keep `Anmf` forever" and the rejected
`Roll`/`RolT`/`RlyT` candidates; opened as `Proposed`, `Accepted` 2026-07-30 — see the head entry)
+ its `ADR_INDEX` row; **KI-016** + its summary-table row.
ADR count in the self-coverage table synced 17 → **18**.
Synced: CHANGELOG (`[0.9.1] ### Changed`, evidence = PR #97 per `CHANGELOG_POLICY` rule 2; the
preamble's "from [0.9.0] onward each release is tagged" claim corrected — 0.9.0 was written up but
never tagged, so the first annotated tag will be v0.9.1), README (§Project status), HANDOVER
(Current Version, Release Status incl. the tag name `v0.9.1`, Known Blockers), COMPATIBILITY_POLICY
(new *Plugin identity change* prohibited-row; an **identity carve-out** to exception condition 2 —
enacted by ADR-0023, because condition 2 as written is unsatisfiable by construction for an
identity change and the policy would otherwise have contradicted itself; and an
"Exceptions granted so far" table recording that the carve-out's 2a ground is spent),
RELEASE_PROCESS (§Tagging — next tag is `v0.9.1`, and date the heading before tagging),
`release.yml` + CI_CD (a fail-closed check rejecting a tag whose CHANGELOG heading is still marked
`Unreleased`, since the heading is published verbatim as the release-notes title), TRADEMARKS
(§1 — the code is a RollyTech name-bearing identifier), PACKAGING (§Plugin identifiers),
KNOWN_ISSUES (version-sync lead), and every `auval -v aufx Anmr Anmf` invocation → `RTec`
(`packaging/macos/INSTALL.txt`, `docs/user/INSTALLATION.md`, `PACKAGING.md`, `TROUBLESHOOTING.md`,
`TESTING.md`, `KNOWN_ISSUES.md` KI-014, `RELEASE_HARDENING_PLAN.md` RH-F3).
**Deliberately not changed:** `worklogs/PRODUCT_READINESS_ROADMAP_v0.8.13.md:36` still carries the
old `auval` recipe — worklogs are a historical evidence trail, not maintained documents, and
rewriting one to match today's code would falsify the record.
**Drift observed, not corrected (constraint C6):** `ADR-0001` cites `CMakeLists.txt:62-73` for the
`AnamorphDSP` INTERFACE library, which now lives at `:124-135` (as `ARCHITECTURE.md` correctly
states); this predates the present change and is out of its scope.
The comment added beside the new code is deliberately same-line, so no `CMakeLists.txt:NNN`
citation anywhere in the documentation shifts.

Prior: for the **product video script worklog** (2026-07-29, on top of `main` @
`82b2f61`). **No `src/` change; no product-document change.** Added (and subsequently revised,
in the same unmerged branch) `worklogs/KEYNOTE_SCRIPT_v0.9.0.md` — a session work product
(marketing draft): a locked product positioning ("width is a method"; the plugin as instrument,
not assistant) and a complete ~7-minute developer-walkthrough video script with production
guardrails, all derived from the existing developer chain. An earlier keynote-style draft in
the same file was superseded by this revision; a Chinese adaptation is deferred until the
English script is approved. It is explicitly marked derived content, may never be cited
as evidence, quotes no unmeasured performance numbers (constraint C2), uses no ™/® symbols
(`TRADEMARKS.md`), and does not alter product status (`docs/COMMERCIAL_STATUS.md`: v0.9.0
remains internal-testing, not for sale). Worklogs sit outside the four documentation classes
(`docs/REPOSITORY_MAP.md` describes `worklogs/` generically), so no
`REPOSITORY_MAP`/`SOURCE_OF_TRUTH`/README class-table change applies; this entry satisfies the
audit obligation. Not a changelog entry (no user-visible product change, `CHANGELOG_POLICY.md`
rule 3).

Prior: for the **artifact & INSTALL.txt cleanup pass** (2026-07-26, on top of `main` @
`2d0a906`). **No `src/` change; no installer or runtime behaviour change.** The three internal
`Anamorph-<OS>-release` artifacts are **removed**, along with the archive-creation steps that fed
them (`zip -ry` / `Compress-Archive` / `ditto -c -k`): each platform now uploads exactly one
customer artifact (`Anamorph-<OS>`, loose files) plus its `-debug` symbols. `release.yml`
downloads those same trees, restores the executable bits the artifact transport drops
(`Anamorph`, `install.sh`, `uninstall.sh`, `*.so` on Linux; `*/Contents/MacOS/*` on macOS;
Windows carries no Unix modes), archives each tree with its entries at the archive root, and then
**fails closed** unless every expected executable is present in the published zip with its mode —
so release assets keep their names, contents and permissions, and no nested archive is
reintroduced. The three `packaging/*/INSTALL.txt` files lose their "Testing & third-party
attribution" section entirely and now carry installation instructions, paths, platform notes and a
copyright line only; the mandatory IJG acknowledgement therefore rests solely on the
release-page `NOTICE` asset that `RELEASE_POLICY` requires on every published release. Synced:
CI_CD (pipeline step 7, artifact table, route note), PACKAGING (artifact table, routes,
attribution table), RELEASE_POLICY (§Artifacts, §Third-party attribution), RELEASE_PROCESS
(§Build the release artifacts, §Tagging step 3), REPOSITORY_MAP, HANDOVER (snapshot base,
distribution), COMMERCIAL_STATUS, KNOWN_ISSUES (KI-015), FUTURE_RISKS (RISK-006), TRADEMARKS §3,
CHANGELOG `[0.9.0]`, this file.

Prior: for the **internal-testing preparation & closed-source product documentation pass**
(2026-07-26, PR #94, on top of `main` @ `aecd448`). **No `src/` change.** The v0.9.0 **release date moved to
2026-07-26** in `CHANGELOG.md` and its two HANDOVER restatements (PR-landing and audit-run dates
left untouched). `SUPPORT.md` was **rewritten from a public support document into the internal
testing guide** — evaluation-only permission, no source-code rights, no redistribution, the
project's testing channel, and six mandatory report fields (version+build, OS, DAW/host, format,
reproduction steps, logs/screenshots); the bug-report form became "Test report — bug" and carries
the closed-source + public-tracker notice. **New:** `EULA.md` (an **unapproved draft**, not in
force and not shipped, every open owner/legal decision marked), `PRIVACY.md` (collects nothing,
sends nothing; every disk write and the one About-screen link cited to source), `TRADEMARKS.md`
(name status, third-party marks used descriptively, the naming obligations IJG/Xiph/zlib impose,
and the `Dim-D` / "Roland Dimension-D-style" review item) and `docs/COMMERCIAL_STATUS.md` (the
internal index of product model, distribution model and the eight open owner/legal decisions —
including the newly recorded fact that the GitHub repository is public with forking enabled while
the product model is closed-source). README regrouped its documentation index into **four
classes** (user / internal-testing / legal / developer). Product-model wording was then **rebalanced on owner instruction**: it is
stated once for a general audience in `README.md` §Licensing and otherwise kept only where it is
operative — the legal class, the internal/testing class (`SUPPORT.md` §1, the bug-report form) and
the developer documents that derive the JUCE-tier consequence. The user-facing set stays on using
the product: `USER_MANUAL` and `INSTALLATION` end with a plain copyright line, every `INSTALL.txt`
carries one in its own bilingual section above and separate from the mandatory third-party
(**superseded 2026-07-26 — see the head entry: `INSTALL.txt` is installation-only, so no
attribution section remains for it to sit above**)
attribution (which is unchanged), and the manual's Quick start and FAQ carry no legal wording at
all.
Synced: README, SUPPORT, REPOSITORY_MAP (root + `docs/` trees, `user/` branch), SOURCE_OF_TRUTH
(four-class scope + per-class authority), HANDOVER (snapshot base, release date, KI-015 wording),
DOCUMENTATION_LIFECYCLE_POLICY (documentation-only trigger table), THIRD_PARTY_LICENSES
(§"Open licensing decisions" #2), CHANGELOG `[0.9.0]`, issue templates, this file (self-coverage
tiers, ADR count).

Prior: for the **flat-artifact / lean-package / closed-source documentation pass**
(2026-07-26, PR #93). **No `src/` change.** The `Anamorph-<OS>` artifacts now upload loose
staged files (payload + `INSTALL.txt`; Linux adds `install.sh`/`uninstall.sh` — extract the
artifact zip once to see them directly; the transport drops exec bits on that route), new
`Anamorph-<OS>-release` artifacts carry the permission-preserving source archives that
`release.yml` publishes byte-identically (**superseded 2026-07-26 — see the head entry: those
artifacts are removed and `release.yml` archives the release zips itself**), and `NOTICE`/`THIRD_PARTY_LICENSES.md`/`SUPPORT.md`
ship **only** as version-named release-page assets — no longer inside any zip or installer
payload; each `INSTALL.txt` carries the IJG acknowledgement + pointer (**superseded 2026-07-26 —
`INSTALL.txt` is installation-only**). README now states the
product model (closed-source commercial; docs grouped user/legal/developer — the grouping
was superseded 2026-07-26, see the head entry), and the
licensing blocker set (KI-015/RISK-006/RH-R11/RH-F1, THIRD_PARTY_LICENSES, NOTICE, HANDOVER)
uniformly records that the model rules out the AGPLv3 arm, so the commercial JUCE licence is
required before commercial distribution. Synced: PACKAGING, CI_CD, RELEASE_PROCESS,
RELEASE_POLICY, REPOSITORY_MAP, SOURCE_OF_TRUTH (doc-class scope), USER_MANUAL (online link
fallbacks), CHANGELOG `[0.9.0]`, this file.

Prior: the **v0.9.0 release-hardening & commercial-readiness audit** (2026-07-25,
on `main` @ `0a98ebd`, PR #92; record: `worklogs/RELEASE_HARDENING_AUDIT_v0.9.0.md`). **No `src/`
change.** Six parallel investigation lenses + adversarial verification over the repository and
the pinned JUCE tree. **New:** `NOTICE` and `THIRD_PARTY_LICENSES.md` (verified third-party
inventory — every component classified compiled-in vs vendored-but-not-built from
`build/build.ninja` and object symbols rather than from JUCE's manifest, which is how FreeType
and stb, both vendored *inside* PlutoVG, were found; the Steinberg VST 3 SDK is **MIT** in JUCE
9.0.0, correcting RH-R10's GPLv3/proprietary claim, with the trademark/distribution review left
explicitly open); `SUPPORT.md`; `.github/ISSUE_TEMPLATE/{bug_report,config}.yml`. **Packaging
(superseded 2026-07-26 — see the head entry):** both attribution files at that point shipped
inside all three zips, installed unconditionally by the Windows installer, and were attached as
version-named release assets (covering the `.pkg` route) — several vendored licences (IJG,
FLAC, Ogg Vorbis) require the notice to accompany a binary distribution; the release-page
assets are now the sole route, with the IJG line in every `INSTALL.txt`. **User docs:** USER_MANUAL gained a Quick start, a Standalone-application
section, system requirements, a TOC and a rewritten FAQ (rescanning per DAW, Windows paths,
Gatekeeper both routes, presets, CPU, latency, automation, session compatibility), and three
defects were fixed — "set Mix to 0 %" was the manual's most-repeated instruction but `mixK` is
Advanced-only (`PluginEditor.cpp:856`), seven controls were documented under host-parameter
names rather than GUI labels, and the MULTIBAND `On` toggle was undocumented; INSTALLATION
gained the missing macOS `mkdir -p`; macOS `INSTALL.txt` dropped its "unsigned developer build
for testing" line. **Policy/plan:** RELEASE_POLICY (artifact list + a new third-party-attribution
precondition), RELEASE_HARDENING_PLAN (RH-R10 corrected, **RH-R11** added for the missing
LICENSE/EULA, §12a post-v0.9.0 follow-ups RH-F1..F6), PERFORMANCE_BUDGET (a required benchmark
procedure for RISK-002 — no infrastructure added), TESTING (a "gaps in the automated coverage"
section: the AU is never auval-validated, and no frozen golden-audio reference exists by
design), KNOWN_ISSUES (**KI-014** AU unvalidated, **KI-015** no declared licence), HANDOVER
(release status: four unsatisfied RELEASE_POLICY preconditions, none fixable by code),
REPOSITORY_MAP, PACKAGING, CI_CD, README, CHANGELOG `[0.9.0]`. Validation: Release build green,
140-check DSP + 774-check state suites green, pluginval strictness 10 green in both modes ×3.
Prior: for the **post-v0.9.0 maintenance audit** (2026-07-24, on `main` @ `4226d2c`):
a repository-wide drift/maintainability pass with **no behaviour change** — no DSP, GUI,
parameter, serialization or CI-gate change, so **no CHANGELOG entry and no version bump**
(CHANGELOG_POLICY rule 3). Fixed: `CMakeLists.txt` — the 9-file wrapper/GUI source list was
duplicated verbatim between the plugin target and `AnamorphStateTests` (a new source added to
one only would silently desync the state suite's coverage) → single `ANAMORPH_PLUGIN_SOURCES`
variable, build graph provably identical (`ninja: no work to do` after reconfigure), plus the
stale "DSP self-tests" section header for a block that builds both suites;
`.github/dependabot.yml` — comment still said JUCE **8.0.14** pinned to a **tag** (it is 9.0.0
pinned by immutable commit SHA, ADR-0022); `release.yml` — stale `v0.8.13` tag example (the
first tag is v0.9.0); `packaging/windows/Anamorph.iss` — validation note cited a CI run and step
name that predate the rewritten script; `packaging/windows/INSTALL.txt` — the zip's own notes
told the reader to run an installer `.exe` that is not in the zip (macOS INSTALL.txt already
said "from the GitHub release"); `BUILD.md` — the Linux dependency list omitted **`libegl-dev`**,
required since JUCE 9, and still called the JUCE pin a "tag" (+ a new EGL row in
TROUBLESHOOTING); `SOURCE_OF_TRUTH.md` — authority level 2 named only `tests/dsp_tests.cpp`,
not `tests/state_tests.cpp`; **KI-002 rewritten** — it claimed manual `xattr` is required for
macOS artifacts full stop, contradicting the v0.9.0 `.pkg` route (payloads are not quarantined;
what remains there is the one-time Gatekeeper approval of the unsigned package), with the same
zip-vs-pkg scope applied in `PACKAGING.md`, `RELEASE_PROCESS.md` and `TROUBLESHOOTING.md`;
`HANDOVER.md` — snapshot HEAD frozen at `86b4273` (pre-#88/#89) → `4226d2c`, and "the three
installable packages" → the two installers + the in-zip Linux scripts; `FUTURE_RISKS.md` /
`KNOWN_ISSUES.md` version-sync headers extended to PR #89. Stale `file:line` evidence
citations corrected where they pointed at unrelated code: RELEASE_POLICY (`build.yml:54,156,373`
→ `:60,180,432`), KNOWN_ISSUES KI-002 (`build.yml:495-498` → `:558-561`; macOS INSTALL.txt
ranges), DEPENDENCY_POLICY (`run-pluginval.sh:34` → `:43-48`), TROUBLESHOOTING
(`run-pluginval.sh:42-44` → `:50-53`; `setup-linux.sh:33` → `:31,36`), PACKAGING (two bare
`INSTALL.txt:` cites qualified to `packaging/macos/`). Validation: Release build green, 140-check
DSP + 774-check state suites green, no new compiler warnings. Reported but NOT fixed (need an
owner / exceed "minimal"): the CMake-version-parsing regex exists in three independent copies
(build.yml Windows PowerShell, build.yml macOS `sed`, release.yml `sed`); the Windows installer
does not remember the VST3 folder across upgrades and its `UninstallDisplayIcon` points at
`Anamorph.exe` even on a VST3-only install; HANDOVER's status cells are multi-thousand-character
single table rows. Prior: for the **v0.9.0 installer/packaging rework** (2026-07-24, PR #89): the
Windows Inno Setup installer gains a **component page** (Install VST3 / Install
Standalone, both pre-selected, ≥1 enforced) and a **single destination page with both
paths** (VST3 above Standalone; the launch-after-install checkbox is removed); the macOS
`.pkg` gains **component selection** (hand-written distribution, `customize="allow"`,
full-install default, system-wide domain); the Linux `install.sh`/`uninstall.sh` move
**into the zip** and switch to **system-wide** installs (`/usr/lib/vst3`,
`/usr/local/bin`, root required) — the separate `Anamorph-<version>-Linux.tar.gz`
package artifact is REMOVED (release archives are flat ZIPs only, payload at the archive
root); all three `INSTALL.txt` files restructured (Installer vs Manual sections, both
system-wide). Docs synced per the lifecycle triggers: PACKAGING (artifact table,
archive-contract note, §Installers rewrite, install-locations table incl. Standalone
row), CI_CD (pipeline item 8 + artifact table), RELEASE_PROCESS (asset list),
INSTALLATION.md (all three platforms + version-number placeholders replacing literal
versions), USER_MANUAL (version-agnostic wording), README (Installing section),
KNOWN_ISSUES (KI-005 wording), RELEASE_HARDENING_PLAN (installer rows/plan wording),
HANDOVER (status rows), REPOSITORY_MAP (packaging rows), CHANGELOG `[0.9.0]` packaging
bullet. Prior: for the **v0.9.0 release preparation** (2026-07-24, PR #87, on top of
`main` @ `86b4273`): version bump 0.8.12 → **0.9.0** + CHANGELOG `[0.9.0]`; **installable
packages** added to CI (Linux `Anamorph-<version>-Linux.tar.gz` + `packaging/linux/`
install/uninstall scripts, Windows `Anamorph-<version>-Windows-Installer.exe` via
`packaging/windows/Anamorph.iss`, macOS `Anamorph-<version>-macOS.pkg` via
`packaging/macos/build-pkg.sh` — all built from the same validated staging dirs, uploaded
as three NEW artifacts, staged fail-closed into the draft release by release.yml alongside
the unchanged zips); **NEW user docs area `docs/user/`** (USER_MANUAL.md — full end-user
manual, attached to releases; INSTALLATION.md — per-platform install guide) closing the
roadmap's "zero user docs" P0 gap; `INSTALL.txt` now ships in all three zips (previously
macOS only). Docs synced per the lifecycle triggers: PACKAGING (installable-packages
section replaces the "no installer" TODO; artifact table + install-locations evidence),
RELEASE_PROCESS (v0.9.0 tag examples; release-asset list), CI_CD (triggers/pipeline/
artifact table incl. the macOS-debug best-effort correction), README (version, user-docs
links, Releases distribution), HANDOVER (all status rows), KNOWN_ISSUES (KI-005 resolved —
installers exist; header re-synced), FUTURE_RISKS (header re-synced), CHANGELOG_POLICY +
CHANGELOG preamble (tags exist from v0.9.0), RELEASE_HARDENING_PLAN (RH-R5 mitigated,
RH-PR-5b/6 skeletons landed, first-tag references v0.8.13 → v0.9.0), REPOSITORY_MAP
(docs/user/ + packaging/* rows), and one stale code comment (SpectrumImager.h alt-click
solo semantics, pre-0.8.10 wording). Prior: for the **product-readiness roadmap review** (v0.8.13 cycle, 2026-07-23, on
`main` @ `dcfad73`; extended the same day with the **item-by-item re-evaluation + independent
gap hunt**: 14 carried items re-classified (Must-now / before-1.0 / nice / defer, with
reasons); NEW findings — **Steinberg VST3-SDK licence compliance + third-party NOTICES**
(recorded as RH-R10 in RELEASE_HARDENING_PLAN §2, the pass's one doc fix beyond the roadmap),
support-workflow gap (no issue templates/SUPPORT.md), the undo/gesture-coalescer test gap
(largest hand-verified-only subsystem; now cheap to cover via the state-harness target), and
a "what 1.0 commits to" policy gap; plus an explicit outdated-assumptions retirement list.
Original entry: (v0.8.13 cycle, 2026-07-23, on
`main` @ `dcfad73` — PRs #82/#83/#84/#85 all merged). Roadmap-only pass, deliberately NOT
another audit: drift review limited to correctness-affecting items (none found). NEW
`worklogs/PRODUCT_READINESS_ROADMAP_v0.8.13.md` — maturity assessment (engineering High /
release Medium-high / commercial Low / UX Medium), blockers split (pre-1.0 vs 0.9.x vs
optional), 4-phase ordered roadmap (v0.8.13 completion → user-facing readiness → commercial
infra → v1.0 prep), documentation-review verdicts (user docs MISSING → Phase-2 item 1;
developer + release docs sufficient/complete), and technical-order rationale (auval before
host matrix; presets before golden-audio; signing before installers; licensing last).
HANDOVER Roadmap row re-pointed at the new plan (the previous pointer directed the next agent
at already-finished work). Prior: for the **RH-PR-8 release-pipeline foundation + its review follow-up
(release-artifact integrity)** — the follow-up archives customer artifacts **at the source**
(`zip -ry` / `Compress-Archive` / `ditto`) because the artifact transport preserves neither
Unix permissions nor symlinks, and turns the release job's staging into a **rename-only**
step (the archives CI validated are published byte-identically; Linux round-trip proven
locally with real build output — 755 bits + `cmp`-identical; annotated-accept /
lightweight-reject tag tests replicated green; PACKAGING.md artifact-layout table updated
to the single-archive contents) — (v0.8.13 cycle, 2026-07-23,
branch `claude/beautiful-sagan-JAUFI` on `main` @ `ee82380` — PR #83 merged). Infrastructure
only, no product behaviour/version change. NEW `.github/workflows/release.yml` (annotated
`vX.Y.Z` tag → fail-closed tag⇄version⇄CHANGELOG validation → the FULL existing `build.yml`
gates reused via a new additive `workflow_call` trigger (6-line `on:`-block diff, branch/PR
behaviour byte-identical; tag pushes triggered nothing before) → **draft** GitHub Release
with versioned artifact copies + `SHA256SUMS.txt` + `RELEASE_MANIFEST.txt` + CHANGELOG-section
notes; `workflow_dispatch` rehearsal mode; `contents: write` scoped to the one release job;
no third-party actions beyond `actions/*` + `gh`; publishing stays manual per RELEASE_POLICY).
No tag created (first: the v0.8.13 release — closes RISK-003 when cut). Docs synced:
RELEASE_PROCESS (§Tagging + release pipeline; stale "no tags TODO" replaced), RELEASE_POLICY
(Artifacts note), CI_CD (Triggers + source-of-truth), FUTURE_RISKS RISK-003 mitigation,
RELEASE_HARDENING_PLAN (§1 baseline rows + §10 RH-PR-8 row per its §13 update protocol),
REPOSITORY_MAP, HANDOVER. Work record:
`worklogs/release-hardening/RH_PR8_RELEASE_PIPELINE.md` (incl. the scoped dependency/security
review: no new third-party actions; SHA-pinning of actions + the pluginval download pin remain
open supply-chain items). Validation: both workflows YAML-parse; the validate/stage shell
logic executed locally against real repo data (version parse, CHANGELOG gate, 46-line notes
extraction); end-to-end proof = the post-merge `workflow_dispatch` rehearsal. Prior: for the **JUCE 8.0.14 → 9.0.0 migration & dependency hardening** (v0.8.13
cycle, 2026-07-23, branch `claude/beautiful-sagan-JAUFI` on `main` @ `1502077` — PR #82
merged). **Dependency migration, zero C++ source changes**: the complete 9.0.0
breaking-change surface has no project exposure (audit table in
`worklogs/JUCE9_MIGRATION_v0.8.13.md` §1.1). CMake pin → the tag's **immutable commit SHA**
`f8f8864…` with new `ANAMORPH_JUCE_VERSION` (supply-chain hardening, audit roadmap item);
`scripts/setup-linux.sh` + `libegl-dev` (JUCE 9 Linux GL uses EGL, not GLX). Validation:
engine output **bit-identical** 8.0.14 vs 9.0.0 (32-scenario twin dump incl. latencies);
140 + 774 suites green under 9.0.0 with the 8.0.14-frozen registry snapshot passing
**unchanged**; `juce_recommended_warning_flags` byte-identical and DSP-TU warnings identical
under both versions (no new warnings); pluginval on the CI gates (local egress 403, ADR-0012
precedent). New **ADR-0022** (Proposed — pending Architecture-Review sign-off + the
DEPENDENCY_POLICY Level-5 audition) + index row. Docs synced: DEPENDENCY_POLICY (SHA-pin rule +
EGL), BUILD, README, TROUBLESHOOTING (pin row + the discovered stale-CMake-cache trap row),
REPOSITORY_MAP, COMPATIBILITY_MATRIX, FUTURE_RISKS RISK-001, KNOWN_ISSUES (KI-011/KI-013
evidence re-verified against the JUCE 9 tree; KI-013 not fixed upstream), HANDOVER — plus a
repo-wide `CMakeLists.txt:NN` citation sweep (+5 shift from the pin block; every cite
re-verified, two pre-existing stale cites fixed: ARCHITECTURE.md, COMPATIBILITY_MATRIX VST3
row). No version bump / CHANGELOG entry (stays inside v0.8.13; a JUCE bump is user-visible at
release time — the release-prep changelog entry will record it, per the 8.0.14 precedent
where the bump shipped inside `[0.8.8]`). Prior: for the **state-serialization & parameter-compatibility regression harness**
(v0.8.13 cycle, 2026-07-23, branch `claude/beautiful-sagan-JAUFI` on `main` @ `823bfbe` —
PR #81 merged). **Validation infrastructure only** — no parameter, serialization, DSP or
user-visible behaviour change; no version bump / CHANGELOG entry (release-prep steps; the
changelog scopes to user-visible changes). NEW: `tests/state_tests.cpp` (9 headless
state-compatibility tests exercising the real `AnamorphAudioProcessor`: schema shape vs
SERIALIZATION_REGISTRY, parameter-registry snapshot vs a frozen fixture, raw-exact
save→load→save round-trip, the v0.2 / pre-0.6.4 / pre-0.8.4 legacy-migration paths via frozen
fixture XMLs, corrupt/foreign-state robustness, user-preset round-trip + exclusion rules, A/B +
view-param preservation), `tests/fixtures/` (registry snapshot + 3 legacy session models), the
`AnamorphStateTests` CMake console target (test block only — shipped targets untouched), and the
blocking CI wiring (`scripts/run-tests.sh` runs both suites fail-closed; the Windows job runs
both exes; step ids/gating unchanged). Docs synced: TESTING.md (new suite section + snapshot
workflow), TESTING_POLICY (Level-2 row + hard gate), RELEASE_COMPATIBILITY_CHECKLIST
(automation annotations on 4 items), CI_CD.md (pipeline step 4), REPOSITORY_MAP, BUILD.md,
DEVELOPMENT.md, README, RELEASE_HARDENING_PLAN (QA-gate row), HANDOVER. The whole edit set was
adversarially verified pre-commit (3 lenses: citation accuracy, test quality, policy/scope);
the pass surfaced and fixed one missed required sync (CI_CD.md), several overstated wordings,
and four test hardenings (recorded in the worklog §4). Design + architecture
record: `worklogs/STATE_HARNESS_v0.8.13.md` (includes the honest remaining-gaps statement:
legacy fixtures are reconstructions; cross-version vN−1→vN reload stays manual). Prior: for the **post-v0.8.12 repository audit & documentation-consistency pass**
(2026-07-22, branch `claude/beautiful-sagan-JAUFI` at `main` @ `64e87c4` — PR #80 merged).
**Documentation-only.** Two things: (1) **retroactive coverage of PR #80** (v0.8.12 GUI interaction
fixes: bare-press no-write + relative Width drag with 3 px threshold in `src/gui/SpectrumImager.{h,cpp}`,
release-outside stuck-press reconcile in `src/PluginEditor.cpp`; recorded in
`worklogs/BANDWIDTH_DRAG_FIX_v0.8.12.md` + `worklogs/MOUSE_RELEASE_STATE_FIX_v0.8.12.md` — PR #80
synced CHANGELOG/HANDOVER/worklogs but missed this file, a lifecycle slip closed here); and
(2) a **full drift audit with minimal corrections**: CHANGELOG `[0.8.12]` re-dated 2026-07-22 (two
of its fixes landed that day) and "MultiBand"/"Bandwidth" normalized to the registry terms
"Multiband"/"Width"; HANDOVER snapshot-HEAD + Build/Release-Status rows refreshed to v0.8.12 (were
frozen at v0.8.11/136 checks) and RH-PR-2 marked shipped; KNOWN_ISSUES + FUTURE_RISKS headers
re-synced (were at v0.8.10) with **KI-013 added** (macOS-inert release-outside reconcile — platform
limitation of the v0.8.12 fix); stale line-number evidence citations refreshed in KNOWN_ISSUES
(KI-001/002/003/006/009/012), FUTURE_RISKS (RISK-002 incl. marking the shipped H1/Wave-3
SoloMonitor skip, RISK-004), POSTMORTEMS (INC-003/004/006/007/009), REPOSITORY_MAP (test count
23→33, `FrameClock.h` + `LR4Xover.h` rows added, CMake cites), README (3-OS pluginval gate scope),
CI_CD (actions @v7), DEPENDENCY_POLICY (`JUCE_*` flags at `CMakeLists.txt:183-188`; "then-current"
qualifiers), PACKAGING + COMPATIBILITY_MATRIX (CMake line cites), ADR_INDEX (130-check/23-test
wording), BUILD + TESTING_POLICY + CODE_STYLE + TROUBLESHOOTING + RELEASE_PROCESS + TESTING (the
same class of post-RH-PR-2 stale CMake/script cites, caught by the pre-commit verification pass),
PERFORMANCE_BUDGET (GUI-redraw row gained its missing Wave-6/0.8.12 record), RELEASE_HARDENING_PLAN
("then-current 136-check" qualifier). The whole edit set was adversarially verified pre-commit
(3 lenses: citation accuracy, history preservation, completeness — see the worklog §1).
Roadmap + deferred-item review recorded in `worklogs/POST_v0.8.12_AUDIT_AND_ROADMAP.md`. No code
change; no version bump. Prior: for **performance Wave 6 — GPU/GUI rendering-efficiency (v0.8.12)** (2026-07-21,
branch `claude/beautiful-sagan-JAUFI`, restarted from `main` @ `c6f3226` — PR #78 merged). **One
behaviour-neutral code change** (`src/gui/SpectrumImager.cpp`, `paintHeadphone`): the per-band solo-
headphone transparency layer was allocating a **plot-sized offscreen framebuffer every Advanced-mode
frame** (JUCE sizes a transparency-layer offscreen to the current clip, which was the whole plot
rounded-rect, not the ~18×15 px glyph); it is now clipped to the glyph (+4 px, covering the earcups +
AA → pixel-identical) and skipped entirely at full opacity. A 5-lens adversarial Workflow (14 agents)
confirmed the idle/Simple/hidden GPU paths are already ~0 and at their frontier, and that the spectrum
**cannot** be made opaque pixel-identically (it nests bottom-flush in a translucent rounded panel, so
its bottom corners straddle a two-colour arc no flat pre-fill reproduces). Build + **140-check suite
green**; no DSP/threading/parameter/serialization/latency change; GPU measurement unavailable in the
headless container (analytical estimate — the affected GL path is macOS/Windows-only, Linux is CPU per
ADR-0011). Version bump `0.8.11 → 0.8.12` (`CMakeLists.txt:14`). Synced: this file, CHANGELOG
(`[0.8.12]` **### Changed**), HANDOVER (Current-Version + Pending-Tasks rows), README (version line).
Evidence: `worklogs/performance/WAVE6_GPU_RENDER_INVESTIGATION.md`. Prior: for the **v0.8.11 final
performance pass & release-readiness audit** (2026-07-20,
branch `claude/beautiful-sagan-JAUFI`, restarted from `main` @ `4aac4eb` — PR #76 = Waves 4+5,
merged). **No code change:** the three remaining named candidates were closed with measured
verdicts. The long-open **GUI fresh-eyes sweep** is DONE — carried in-line after the Workflow
lens was lost to the org token limit a third time; the GUI paint + message-thread surface is
already exhaustively cached/gated across Waves 1–4, the only residual (per-call `Path`/`Font`
locals in the shared `LookAndFeel` slider draws) transient and not worth a restructure. **W3-10**
deferred as Class B (a 50 M-sample probe: `applyWidth(·,·,1.0f)` differs from identity in 15.5 %
of samples, ~1 ULP). **W5-D** prototyped (`scratchpad/kwbench.cpp`): bit-exact vs the scalar
K-weighting chains but only 1.10× at the frozen SSE2 flags — the 4-wide win needs an AVX2/`-march`
build decision (itself numerics-frozen + FMA-divergent). `loudness.process()` confirmed
intentionally unconditional (feeds the live match readout). Release-readiness audit: build +
140-check suite green, no version/test-count drift, no release blockers; documentation-only, so
**no CHANGELOG entry** per CHANGELOG_POLICY rule 3. Synced: this file, PERFORMANCE_BUDGET (final-
pass bullet), HANDOVER (Pending-Tasks + Release-Status rows). Evidence:
`worklogs/performance/FINAL_PASS_v0.8.11_INVESTIGATION.md`. Prior: for **performance Wave 5 — per-block/settled-state runtime optimisation +
v0.8.11 changelog consolidation** (2026-07-20, branch `claude/beautiful-sagan-JAUFI`, rebased
onto main @ `912a755` — the security-tooling/CodeQL-autofix PRs #65–#75; the one rebase
conflict (both sides' new head entry in THIS file) was resolved by keeping both in order).
Eight Class-A trims from a two-lens fresh-eyes Workflow sweep (per-block + per-sample; the GUI
lens was lost to the org token limit for the second time — still open): the
`sameParameters` bitwise no-change gate on per-block parameter adoption (~250 → ~91
instructions per unchanged snapshot), the VelvetNoise parked fast loop (env/gate/history kept
per W3-9), the settled-Width hoist, meter-publish db reuse + bar-fall cache, Level-Match
estBoost memo + MEASURE-coeff cache + silent-block LUFS skip. Rejected with reasons in the
worklog: atomic-exchange load-gating (THREADING_POLICY conservatism), generation-keyed
snapshot cache (incomplete contract); deferred: K-weighting SIMD bank (W5-D), lat==0
mix-ring elimination (W5-A). Callgrind A/B: transparent −4.5 %, hostlike-b64 −5.5 %; twin
dump bit-exact ×19; suite 140 checks; warning set unchanged. Also corrected Wave 4's
drift-contaminated small-buffer datum (real overhead +10–20 %, not 2×). **v0.8.11
consolidation (maintainer instruction):** the `[Unreleased]` Wave-4 entry moved into
`[0.8.11]`, now dated **2026-07-20**, with a new Wave-5 sibling entry; HANDOVER
Current-Version/Release-Status/Pending-Tasks rows re-synced (PRs #60/#61/#62/#63/#76 named;
the CI-/test-only security PRs noted as changelog-exempt per CHANGELOG_POLICY rule 3);
PERFORMANCE_BUDGET gained the Wave-5 bullet and its Wave-4 CHANGELOG citations now point at
`[0.8.11]`. Evidence: `worklogs/performance/WAVE5_INVESTIGATION.md`. Prior: for **performance Wave 4 — idle/background runtime optimisation** (2026-07-19,
unreleased cycle, branch `claude/beautiful-sagan-JAUFI`). Implements the Wave-3 handover's
remaining ranked candidates, all Class A: LevelMeter static-layer cache + opaque (the H2/H13/N2
recipe — the last of the four visualizers; −29…−31 % per meter frame, raw-pixel-identical),
SpectrumImager per-transform dB cache (−92 % of the decay-tail tick) + paint `Path` reuse,
editor 24 Hz memoisations (preset-name shaping keyed on inputs, combo-hover pre-gate, match
readout on value change), Vectorscope hidden-editor gate, Haas parked fast path (rings keep
recording; new Test 34 `testHaasParkedWarmHistory` guards the warm history), vectorized NaN-scan
detector (bit-identical healing, NaN-injection twin rows), segmented scope/bypass ring copies
(publication contract unchanged). Callgrind A/B: transparent floor −4.9 %, haas-parked −12.4 %,
bypass-on −3.0 % whole-run instructions; 19-scenario twin dump bit-exact; suite 33 DSP tests +
A/B guard, checks 136→**140**; warning set byte-stable. A 4-lens verification/discovery
Workflow was lost to an org spend limit — verification was carried in-line against primary
sources; the fresh-eyes sweep is recorded as a follow-up in the worklog. Synced:
PERFORMANCE_BUDGET (GUI row + two new Wave-4 cost bullets), CHANGELOG (`[Unreleased]`, folded
into `[0.8.11] — 2026-07-20` by the Wave-5 consolidation),
TESTING_POLICY + TESTING + README + RELEASE_HARDENING_PLAN QA row (32/136 → 33/140), HANDOVER
(Test Status / Pending Tasks); investigation + validation evidence in
`worklogs/performance/WAVE4_INVESTIGATION.md`. Prior: for the **security-tooling configuration
review** (2026-07-19, branch `security-tooling/config-review`). The four generated GitHub
security configs were optimized against the repository's actual shape: `dependabot.yml` was
**invalid as generated** (`package-ecosystem: ""` — rejected by the Dependabot schema) and now
monitors the only supported ecosystem here, `github-actions` (weekly, grouped into one PR; JUCE
stays FetchContent-pinned + review-gated per `DEPENDENCY_POLICY.md`); `codeql.yml` switched
`c-cpp` from `build-mode: none` (near-zero include resolution — JUCE is absent from the bare
checkout) to a **manual build** mirroring the Linux CI steps but compiling only `Anamorph_VST3`
+ `AnamorphTests` with `-DANAMORPH_BUILD_STANDALONE=OFF`, with alerts scoped to repo-own code
(`paths-ignore: build` excludes the FetchContent'd JUCE tree) and docs-only changes skipping
the workflow; `msvc.yml` gained the **required** build step (juceaide-generated files),
JUCE-as-external suppression (`ignoredIncludePaths`/`ignoredTargetPaths` → `build/_deps`),
path-filtered triggers, and `upload-sarif` v3→v4; `dependency-review.yml` comments on failure
only. Validated: schema (github-workflows + dependabot vendor schemas), local build of the
exact analysis targets, 136/136 self-tests. Synced: CI_CD (§Security scanning),
REPOSITORY_MAP. Prior: for **RH-PR-2 Build Hardening + review follow-up** (2026-07-18, release-hardening
program, ADR-0021, PR #63 `release-hardening/build-hardening`, rebased onto the v0.8.11 bump —
the CHANGELOG entry now lives under `[0.8.11]` **### Security**). Behaviour-neutral binary
hygiene: an `AnamorphHardening` INTERFACE target pins `-fstack-protector-strong`, section GC,
Release `-g`, full RELRO (`-z,relro,-z,now,-z,noexecstack`) on Linux, `-Wl,-dead_strip` on
macOS, and `/guard:cf` + `/DYNAMICBASE /NXCOMPAT` + Release `/Zi`+`/DEBUG /OPT:REF,ICF` on
Windows; CI runs a retain-then-strip pipeline (split `.debug`/dSYM/PDB captured as separate
`Anamorph-<OS>-debug` artifacts, public binaries stripped — Linux VST3 −19.8%, `nm: no
symbols`, dynamic exports untouched; Linux strips before pluginval so the gate validates
shipped bytes; macOS order dsymutil → strip → codesign with `|| true` swallowing removed;
`if-no-files-found: error` everywhere). **Review follow-up (artifact-safety):** customer
uploads are now gated on their strip/staging steps succeeding (`steps.<id>.outcome` — the old
`if: always()` could upload an unstripped Linux binary after a strip failure), the Windows
staging purges ALL debug material from the public copy immediately after the copy and before
any abortable validation (the old order could leak the in-bundle PDB), and both public staging
steps end with an explicit no-symtab/no-`.debug`/no-PDB self-validation. Numerics-affecting
flags untouched; proven by a byte-identical twin engine dump + a green full suite (136 checks
post-Wave-3). Baseline finding recorded: symbol visibility was ALREADY hidden via JUCE's
plugin helpers (plan §1 drift corrected). Synced: new ADR-0021 (+ ADR_INDEX row),
RELEASE_HARDENING_PLAN (§1/§2/§6.1/§10/§12 statuses + the pending QA-row 32/136 sync noted by
the version-bump entry below), CI_CD, PACKAGING, BUILD, REPOSITORY_MAP (worklogs/ entry merged
with Wave 3's), CHANGELOG (`[0.8.11]` ### Security); investigation + validation + review
evidence in `worklogs/release-hardening/RH_PR2_INVESTIGATION.md`. Prior: for the **v0.8.11 version preparation** (2026-07-18, PR
`release/v0.8.11-version-bump` — version/release metadata only, no functional change).
`CMakeLists.txt` project version 0.8.10 → **0.8.11** (single source: `ANAMORPH_VERSION_STRING`
and the JUCE plugin version derive from it); README version line; HANDOVER status rows
(Current Version / Build / Release / Pending Tasks — the completed Wave-3 candidate removed
from the backlog text). CHANGELOG: the `[Unreleased]` Wave-3 entry became **`[0.8.11] —
2026-07-18`** (evidence PR #62, merge `b2481db`), and the two post-release maintenance fixes
recorded under `[0.8.10]` after it shipped — the slow-drag follower regression (PR #60,
`3268cc2`) and the 192 kHz terminal-snap robustness fix (PR #61, `c72d3c3`) — **moved into
`[0.8.11]`** with their evidence lines updated: the released 0.8.10 binaries (PR #59,
2026-07-14) predate both, so `[0.8.10]` claiming them was recorded drift against
CHANGELOG_POLICY rule 2 (no invented history). Deliberately untouched: PR #63's build-hardening
work and files (CMake hardening/CI/ADR-0021/RELEASE_HARDENING_PLAN — including that doc's
still-pending 32/136 QA-row sync noted in the previous entry). Prior: for
**performance Wave 3 — runtime optimisation** (2026-07-18, unreleased cycle,
PR `performance/wave3-runtime-optimization`). Investigation-first wave (baselines, callgrind
attribution and the full decision record live in `worklogs/performance/WAVE3_INVESTIGATION.md`
— a new top-level `worklogs/` directory for session-local records, added to REPOSITORY_MAP).
Four DSP changes + one GUI flag: **(1)** SoloMonitor's H1 cold gate decoupled from cutoff
proximity (gains alone prove the passthrough; a no-solo split drag — ~22 % of the drag-profile
instructions — no longer wakes the bank; Class A, guarded by new `testSoloColdThroughDrag`,
Test 33, proven to fail pre-change); **(2)** per-split LR4 coefficient sharing
(`LR4Xover::copyCoefficientsFrom`): x/dx/ax/dax always share one cutoff, so the glide, the
aligned-block resync and `setBankCutoffs` compute `tan` once per split (12→3 per sample worst
case) and the never-processed `ax[0]`/`dax[0]` are not updated at all (Class A); **(3)** the
phase-compensation allpass is the ladder's first 2nd-order section computed directly
(`LR4Xover::processSampleAllpass` — the recorded 0.8.10 follow-up; Class B ≤ 1.2e-7, 2–24
samples per 204,800 in the twin dump); **(4)** settled output-stage and settled-Mix per-sample
constants hoisted per block (Class A); **(5)** SpectrumImager FFT `ignoreNegativeFreqs=true`
(consumers read bins ≤ N/2 only; identical visuals). Rejected with reasons (recorded in the
worklog): LoudnessMatch off-gating (Measure readout + Apply are live consumers with Match off),
LevelMeters editor-closed gating (held peaks must persist), velvet parked-envelope freeze.
Fair interleaved before/after (session-local, 48 kHz): drags −35…−50 %, settled multiband
−9…−17 %, transparent floor −6.6 %. Suite 32 DSP tests + A/B guard, checks 130→**136**, twin
dump bit-exact on every Class-A row. Synced: PERFORMANCE_BUDGET (allpass follow-up marked done,
H1/crossover-move/GUI rows updated, stale process() line-range corrected), CHANGELOG
([Unreleased]), README, TESTING_POLICY, TESTING, HANDOVER, REPOSITORY_MAP (worklogs/).
**Deliberately NOT touched** (a parallel release-hardening PR owns release documentation):
RELEASE_HARDENING_PLAN.md — its QA-gate row still reads "31 DSP self-tests … (130 checks)" and
needs the one-line 32/136 sync once the PRs land (recorded drift, not silently fixed).
Prior: the **high-sample-rate crossover terminal-snap robustness fix** (2026-07-17,
v0.8.10 maintenance, PR `fix/high-sr-crossover-snap`). Review of the slew-limited smoother found
a numerical edge case, confirmed by exact-float simulation: the per-sample one-pole add stalls
once its move drops below `ulp(f)/2`, and the terminal-snap eps (0.05 + 2e-4·f) out-runs that
stall only up to 96 kHz (margin ≥ 1.76×; 3.55–4.27× at 44.1/48 kHz) — at 192 kHz the margin is
**0.88–0.98×** just past every binade edge ≥ 2048 Hz (parameter-range hard-stall zones
[2049–2093] [4097–4437] [8194–9125] [16388–18500] Hz, resting gap up to 3.75 Hz, both
directions; higher binades to the 86.4 kHz DSP clamp stall too, ≤ 0.4 cents), so a moved crossover
could rest short of its target forever: audio < 0.4 cents off, but the SoloMonitor settled fast
path (H1, needs ≤ 0.05 Hz) never engaged and filters/smoothers stayed hot. Minimal fix in
`MultibandWidth.cpp`/`SoloMonitor.cpp`: the glide **also snaps exactly when the float add can no
longer move the cutoff** — eps, R(f), smoothing, fade thresholds untouched; ≤ 96 kHz
bit-identical (eps snap always fires first). Guarded by `testHighRateCrossoverSnap` (Test 32;
DSP tests 30→**31**, checks 115→**130**): bitwise-exact landing + cold-path engagement at four
rates; pre-fix fails at 192 kHz only (0.4688/0.9375/1.8750/3.75 Hz, never cold — proven by
stash-rebuild). Synced: ADR-0015 (new "High-Sample-Rate Terminal-Snap Robustness" section),
CHANGELOG, README, TESTING_POLICY, TESTING, HANDOVER, RELEASE_HARDENING_PLAN QA row. Test-only
`getLiveCutoff`/`isSettledCold` accessors added to the two headers. Prior: the **crossover
follower slow-drag regression fix** (2026-07-17, post-merge
v0.8.10 maintenance, new PR). The v0.8.10 final flat ~4 oct/s cap was calibrated at a 150 Hz
crossing, but the display maps ~10 octaves onto ~900 px, so ordinary 400–2000 px/s drags are
4–22 oct/s — every normal drag trailed by octaves and crawled after release while violent flicks
escaped via the discrete-jump fade (the reported slow-vs-fast inversion). The glide in
`MultibandWidth`/`SoloMonitor` is now a **slew-limited smoother**: a ~20 ms one-pole demand
clamped per sample to a **frequency-proportional cap R(f) = 4·max(1, f/300 Hz) oct/s** — the
swept-allpass shift stays ≤ 1.25 Hz below 300 Hz (150 Hz crossing still ~14 cents) and ~7 cents
of the crossing above; the one-pole leg de-staircases the 60 Hz UI cadence and tapers arrivals
(a bare clamp landing measured −24 dBc; fref = 300 is the measured spur knee, −41.3 dBc at the
floor). Normal drags now track 1:1 (600 px/s converges 0.01 s after release, was 0.63 s); all
prior artifact bounds hold at the same values. Test 29 gained a normal-drag tracking regression
on both paths (checks 112→**115**; flat-cap re-pin fails both — verified in both directions).
Synced: ADR-0015 (new "Crossover Follower Slow-Drag Regression" section, + ADR_INDEX row),
CHANGELOG, DSP_ALGORITHMS, DSP_GRAPH_REFERENCE, DSP_POLICY inv. 3, PERFORMANCE_BUDGET,
REALTIME_SAFETY_AUDIT, FUTURE_RISKS RISK-002, KI-012, TESTING, HANDOVER. Prior: the
**PR #59 final review fixes** (2026-07-17, two items). (1) **Forced duck
during an ordinary fade-out** — a forced request (undo/redo/A-B/preset) landing in the ~6 ms
fade-out window of a non-forced discrete duck was consumed but dropped, so the swap finished
with normal-duck semantics (no silent-bottom wholesale swap/smoother snap/clean-slate reset —
a stale Haas tail replayed at 0.494 peak against silent input). The engine now upgrades the
in-flight duck to forced in place (dry-fill stays off: never engaged mid-fade). CHANGELOG +
`testForcedSwapDuringOrdinaryFadeOut` (Test 31; DSP tests 29→**30**, checks 106→**112**;
README, TESTING_POLICY, TESTING, HANDOVER synced). (2) **Crossover fade comments corrected**
(comment-only, `MultibandWidth.cpp/.h`, `SoloMonitor.cpp`): the discrete-jump bank fade's
destination is latched at fade start — movement during the fade waits (glide paused), and after
the fade lands a NEW fade may start toward the then-current targets (skipped if within 0.1 oct);
the old wording implied the fade always (re)targets the newest cutoffs. Prior: the
**v0.8.10 final follower decision** (2026-07-17, PR #59). The
bounded-convergence follower (1.25 oct/s cap + release consolidation) was evaluated in
interactive testing and **rejected for interaction latency**; final design (ADR-0015
"v0.8.10 final decision"): the rate cap rises to a hard **~4 oct/s** (drags ≤ 4 oct/s track
exactly — zero GUI/DSP gap; faster movement keeps a controlled ~15-cent worst FM at a 150 Hz
crossing, ~half the pre-fix implementation; a 6-oct flick catches up in ~1.25 s of continuous
motion) and the **release consolidation is removed entirely** (no timers, no delayed jump);
the discrete-jump bank fade is the only special event left. Synced: ADR-0015 (+ ADR_INDEX row),
DSP_ALGORITHMS, DSP_GRAPH_REFERENCE, DSP_POLICY inv. 3, PERFORMANCE_BUDGET,
REALTIME_SAFETY_AUDIT, FUTURE_RISKS RISK-002, KI-012 (rewritten to the accepted controlled-FM
trade), CHANGELOG, TESTING, HANDOVER;
Test 29 re-thresholded to the final operating point (18-cent bound, 1.7–2.2 s convergence
window; both rejection directions re-verified; checks stay **106**). Prior: the
**pre-release hardening plan** (2026-07-17, PR #59, docs-only): new
`docs/architecture/RELEASE_HARDENING_PLAN.md` — the planning artifact for the commercial-release
program (licensing, anti-piracy posture, build hardening, signing/notarization, installers,
release pipeline, multi-agent parallelization contract). No code change; decisions it proposes
are gated on future ADR-0016..0020 + Architecture Review. Architecture self-coverage count
updated (15 docs; ADR count synced to 15 after ADR-0015). Prior: the **v0.8.10
follower refinement + investigation record** (2026-07-14, PR #59) — bounded convergence via
rate cap 1.0 → 1.25 oct/s plus release consolidation, with the complete A–H3 architecture
investigation history (including the H3 hostile-review failure on width purity and
the linear-phase roadmap direction) made permanent as **ADR-0015**. Prior: the **third v0.8.10 pre-merge correctness
round** (2026-07-14, PR #59), two
items. (1) **Split-movement final design** — pure-sine testing rejected the second round's
one-pole tracker too (it FMs at the full drag rate: ~50 cents measured at a fast crossing). A
candidate matrix (rate caps, one-pole, chained/consolidated fades) was measured against the
sine protocol; shipped: a **hard ~1 oct/s cutoff rate cap** (swept-allpass shift bounded at
~0.31 Hz, below the pure-tone JND at any drag speed — worst measured chunk 3.6 cents at a
150 Hz crossing, spurs at the −41 dBc floor) plus a **discrete-jump bank crossfade** (target
steps > 1.5 oct between consecutive blocks land in ~12 ms). The audible-position-eases-at-
~1 oct/s trade is recorded as **KI-012** (with the linear-phase escape hatch gated behind an
Architecture Review). Docs: DSP_ALGORITHMS, DSP_GRAPH_REFERENCE, PERFORMANCE_BUDGET,
REALTIME_SAFETY_AUDIT, DSP_POLICY inv. 3, FUTURE_RISKS RISK-002, CHANGELOG; Test 29 reworked to
grade the whole movement (drag + entire ease incl. the tone crossing + discrete-jump landing).
(2) **Forced-duck dry-fill output-gain latch** — the fill played the raw ring at unity while
the processed path around it was scaled by Output Gain × Balance; at −24 dB an undo/redo Mix
toggle spiked 15.8×. The fill gain is now latched at fade-out entry like `dryDuckLat`
(SIGNAL_FLOW forced-swap note, CHANGELOG); new `testDryFillRespectsOutputGain` (Test 30). DSP
test count 28→**29**, checks 97→**102** (README, TESTING_POLICY, TESTING, HANDOVER). Prior: the
**second v0.8.10 pre-merge correctness round** (2026-07-14, PR #59), two
fixes. (1) **Split-drag transition rework** — pure-sine testing of the first round's chained bank
crossfades showed modulation sidebands around the tone (−25…−28 dBc during a fast drag: a chain
of ~12 ms fades is amplitude/phase modulation and cannot preserve the magnitude response
mid-fade). Final hybrid, picked by measurement: a bounded-time per-sample one-pole cutoff glide
(τ ≈ 15 ms — flat magnitude at every instant, smooth phase, settles ~75 ms after the last move)
for continuous movement, plus a single bank crossfade only for multi-octave jumps (> 1.5 oct,
where the fade's mod-2π phase wrap beats a glide's chirp). Documented across DSP_ALGORITHMS,
DSP_GRAPH_REFERENCE, PERFORMANCE_BUDGET, REALTIME_SAFETY_AUDIT, DSP_POLICY inv. 3, FUTURE_RISKS
RISK-002, CHANGELOG; Test 29 gained a spectral-purity check (max spur < −31 dBc while a split
crosses a 1 kHz tone at 60 Hz UI cadence; the chained fades measure −28.5 and fail), checks
96→**97** (TESTING, HANDOVER). (2) **KI-011, Apple-Silicon-native tooltip white corners** —
juce::TooltipWindow declares itself opaque while drawTooltip leaves the capsule corners
unpainted; the undefined pixels render white on ARM-native AppKit (Intel/Rosetta showed the
stale transparent backing). The editor now marks the TooltipWindow non-opaque on macOS
(KNOWN_ISSUES KI-011, CHANGELOG; hardware re-test pending, KI-006 pattern). Prior: the first
**v0.8.10 pre-merge correctness round** (2026-07-14, PR #59), three fixes:
(a) **Split-drag pitch shift** — `MultibandWidth` and `SoloMonitor` no longer glide their
crossover cutoffs per sample (a swept LR4's allpass phase rotation audibly detuned the audio
during and after a fast split/band drag); cutoff changes are now ~12 ms fixed-coefficient bank
crossfades (state-copied idle bank at the newest targets). Documented in DSP_ALGORITHMS
(MultibandWidth + SoloMonitor), DSP_GRAPH_REFERENCE (shared crossover sub-bank), PERFORMANCE_BUDGET
(crossover-move cost + the allpass-compensation candidate's obsolete sub-item), DSP_POLICY
invariant 3 wording, CHANGELOG; guarded by `testMultibandSplitDragNoPitchShift` (Test 29 — fails
at ~24 cents on the pre-fix glide). DSP test count 27→**28**, checks 90→**96** (README,
TESTING, HANDOVER). (b) **Band Solo alt-click redesign** — alt-clicking an UNSOLOED band's icon
now solos only that band (exclusive) instead of all bands; soloed-band alt-click (clear all) and
plain click unchanged; CHANGELOG (GUI-only, same `mbSolo` single-gesture write). (c) **Option/
double-click reset undo fix** — `Knob::doReset` now wraps the value write in a host change
gesture (the imager's split/width resets already did), so a reset is one undoable step that
clears redo; `undo()`/`redo()` flush a settled-but-unpolled gesture first. Conforms to ADR-0008's
gesture-coalesced design (no ADR change); CHANGELOG. No parameter/serialization/latency/threading
change; the split-drag fix changes only the transition behaviour of moving crossovers (settled
output bit-identical). Prior: the **v0.8.10 release finalization** (2026-07-14, PR #59). The `[Unreleased]`
CHANGELOG entries (undo/redo forced-duck dry-fill + rapid-swap robustness, multiband flat
recombination, adaptive `FrameClock` GUI refresh) are folded into the `[0.8.10]` section; the
version is bumped to 0.8.10 across CMakeLists / README / HANDOVER / KNOWN_ISSUES / FUTURE_RISKS;
KI-009 (REAPER Save Preset) is carried forward as an open, host-specific issue (not fixed).
Includes the pre-merge review round: (a) Multiband
flat recombination — the crossover reconstruction now phase-compensates each lower band by the
splits above it (allpass telescoping), removing the −17.75 dB dip at close crossovers; documented
in DSP_ALGORITHMS (MultibandWidth) + CHANGELOG, guarded by `testMultibandFlatRecombination`
(Test 28). (b) Rapid forced-swap dry-fill robustness — every forced swap re-evaluates dry-fill,
never reusing a prior swap's stale offset; CHANGELOG + `testRapidForcedSwapDryFill` (Test 27).
(c) FrameClock review — the Advanced-only SpectrumImager now stops its display-rate clock while
hidden (Simple mode), mirroring the meters (no unnecessary vblank ticks). DSP test count
25→**27**, checks 77→**90** (README, TESTING_POLICY, TESTING, HANDOVER; `testRapidForcedSwapDryFill`
gained fade-in and fade-out latency-crossing retarget cases during the pre-merge verification pass).
No parameter/automation/
preset/serialization/latency change; the multiband fix changes only the multiband audio output
(the intended fix — twin dump confirms latency unchanged, non-multiband scenarios identical).
Prior: for the **post-v0.8.9 PR** (three items + a fresh profiling baseline). (1) Undo/Redo
audible-dropout fix — the forced switch duck is now dry-filled from the true-bypass ring;
documented in SIGNAL_FLOW (forced-swap note) + CHANGELOG `[Unreleased]`, guarded by the new
`testForcedSwapNoDropout` (Test 26, count 24→**25** DSP tests, 73→**77** checks). (2) Adaptive
display-rate GUI refresh — new `gui::FrameClock` (VBlank, capped ~120 Hz) replaces the four fixed
60 Hz visualizer timers, with dt-corrected ballistics; new module coverage row + THREAD_MODEL timer
table/top-row + PERFORMANCE_BUDGET GUI row + CHANGELOG `[Unreleased]`. (3) **KI-009** added — the
REAPER Save Preset focus report (host-specific, pending manual investigation), version-sync header
updated. A post-v0.8.9 DSP+GUI profiling baseline was produced (callgrind Ir + wall-clock +
EdBench A/B); per established convention the report stays in the session scratchpad and is **not**
committed (no volatile clock-dependent numbers enter the permanent budget). Prior: the **v0.8.9
release** (finalized 2026-07-12, PR #58) — the `[Unreleased]` CHANGELOG entries from Wave-2 Step-1
and Step-2 (H3/H4/H5/H6/H11/H15/ALG-4, the tooltip revert, and the `viewGenWatcher` destructor
lifecycle fix) folded into `[0.8.9]`; every `CHANGELOG [Unreleased]` evidence citation across the
docs set (PERFORMANCE_BUDGET) updated to `CHANGELOG [0.8.9]`. One new module row (`LR4Xover`, the
flat-state LR4 crossover); H3/H4/H5/H6/H15 documented across DSP_ALGORITHMS, DSP_GRAPH_REFERENCE,
SIGNAL_FLOW, PERFORMANCE_BUDGET, REALTIME_SAFETY_AUDIT, THREAD_MODEL/THREADING_POLICY (two new
generation counters, same staleness-hint pattern), TESTING (new `testDryAlignGateRecomb`, test
count 23→24).
Prior: Wave-2 Step-1 (PR #58) — no module-coverage change; the H11/ALG-4 DSP work documented in
DSP_ALGORITHMS + PERFORMANCE_BUDGET + CHANGELOG, and `AI_AGENT_POLICY.md` gained constraint C8
(UI text requires explicit instruction). Retro-covers PR #57 (KNOWN_ISSUES KI-008 added; no
coverage change — this header was missed in that PR). Prior: the initial 0.8.9 version bump
(PR #56) — no coverage change; the 0.8.8 idle-performance PR (#54) — threading paths
(`soundParamGen`) and the ScopeBuffer per-block publication model documented; prior full audit at
HEAD `c605fbe` (JUCE 8.0.14).

## Code-module coverage

| Module | Documented in | Coverage | Confidence |
|---|---|---|---|
| `AnamorphEngine` (chain/switch machine) | SIGNAL_FLOW, DSP_GRAPH_REFERENCE, DSP_ALGORITHMS, ADR-0004/0005/0006 | Full | Verified |
| `EngineParameters` (POD boundary) | ARCHITECTURE, API_REFERENCE, ADR-0001 | Full | Verified |
| `PluginParameters` / APVTS | PARAMETER_REGISTRY, PARAMETER_REFERENCE, ADR-0002 | Full | Verified |
| `InternalState` | PARAMETER_REGISTRY, STATE_SERIALIZATION, ADR-0010 | Full | Verified |
| `PresetManager` | API_REFERENCE, STATE_SERIALIZATION | Partial (interface + role; preset file format not exhaustively documented) | Verified |
| State save/recall | STATE_SERIALIZATION, SERIALIZATION_REGISTRY | Full | Verified |
| `MidSide` | DSP_ALGORITHMS | Full | Verified |
| `HaasProcessor` | DSP_ALGORITHMS | Full | Verified |
| `VelvetNoise` | DSP_ALGORITHMS | Full | Verified |
| `ChorusEngine` | DSP_ALGORITHMS | Full | Verified |
| `MonoMaker` | DSP_ALGORITHMS, SIGNAL_FLOW, ADR-0006 | Full | Verified |
| `MultibandWidth` | DSP_ALGORITHMS, ADR-0005/0009 | Full | Verified |
| `LR4Xover` (flat-state LR4 crossover, Wave 2 / H6) | DSP_GRAPH_REFERENCE, DSP_ALGORITHMS, PERFORMANCE_BUDGET, REALTIME_SAFETY_AUDIT + its own bit-exactness contract comment | Full | Verified |
| `SoloMonitor` | DSP_ALGORITHMS, ADR-0004/0006 | Full | Verified |
| `LoudnessMatch` | DSP_ALGORITHMS, ADR-0007 | Full | Verified |
| `Correlation` / `LevelMeters` / `ScopeBuffer` | DSP_ALGORITHMS, THREAD_MODEL | Full | Verified |
| Threading / OpenGL gate | THREAD_MODEL, ADR-0011 | Full | Verified |
| Latency / PDC | LATENCY_MODEL, ADR-0003 | Full | Verified |
| Real-time safety | REALTIME_SAFETY_AUDIT, REALTIME_AUDIO_POLICY | Full | Verified |
| `gui/FrameClock` (adaptive display-rate refresh, post-0.8.9) | THREAD_MODEL, PERFORMANCE_BUDGET, CHANGELOG + its own header contract | Full | Verified |
| `PluginEditor` / `gui/*` | THREAD_MODEL, REPOSITORY_MAP | Partial (threading + lifecycle documented; per-widget layout/LookAndFeel not exhaustively) | Verified |
| Build / CI / packaging | BUILD, CI_CD, PACKAGING | Full | Verified |
| Tests | TESTING, TESTING_POLICY | Full | Verified |
| Performance (numbers) | PERFORMANCE_BUDGET | Structural only | Unverified (no benchmark data — TODOs) |
| Host (DAW) compatibility | COMPATIBILITY_MATRIX | Listed | Unverified (no in-repo DAW tests) |
| AAX, mono→mono | COMPATIBILITY_MATRIX, COMPATIBILITY_POLICY | Documented as excluded | Not Supported |

## Documentation-set self-coverage (deliverables present)

| Tier | Files | Status |
|---|---|---|
| docs root | SOURCE_OF_TRUTH, HANDOVER, REPOSITORY_MAP, DOCUMENTATION_COVERAGE, POSTMORTEMS, KNOWN_ISSUES, FUTURE_RISKS, COMMERCIAL_STATUS | Present |
| user | USER_MANUAL, INSTALLATION | Present |
| architecture | 15 docs (incl. RELEASE_HARDENING_PLAN) + ADR_INDEX + 18 ADRs (0016–0020 reserved, see plan §8) | Present |
| worklogs | performance/ (Waves 3–6 + the v0.8.11 final-pass and crossover-glide investigations), release-hardening/ (RH program working evidence; finalized decisions live in ADRs), root-level v0.8.12 GUI-fix records (`BANDWIDTH_DRAG_FIX_v0.8.12.md`, `MOUSE_RELEASE_STATE_FIX_v0.8.12.md`) + `POST_v0.8.12_AUDIT_AND_ROADMAP.md` + `STATE_HARNESS_v0.8.13.md` | Present |
| procedures | 8 docs | Present |
| policies | 15 docs | Present |
| root — developer/status | README, CHANGELOG, CLAUDE | Present |
| root — legal | NOTICE, THIRD_PARTY_LICENSES, EULA (unapproved draft — not in force), PRIVACY, TRADEMARKS | Present |
| root — internal/testing | SUPPORT | Present |
| .github | ISSUE_TEMPLATE/{bug_report,config}.yml, workflows/*, dependabot.yml | Present |

## Known coverage gaps / TODOs

- **Performance numbers** — `PERFORMANCE_BUDGET.md` carries explicit TODOs; populate from profiling.
- **DAW host matrix** — `COMPATIBILITY_MATRIX.md` hosts are Unverified; populate from manual testing.
- **GUI per-widget reference** — editor layout/LookAndFeel is documented at the threading/lifecycle
  level only; a per-widget reference is not present (low priority — GUI changes don't gate releases).
- **Pre-0.6 version history** — CHANGELOG entries for early versions are Partially Verified (README
  + commits); no git tags exist for exact per-version attribution.

## Update protocol

On any change, set this file's "Last updated" to the new HEAD and adjust the affected rows. A new
module → add a row; a new doc → add to self-coverage; new perf/host data → upgrade the confidence.
