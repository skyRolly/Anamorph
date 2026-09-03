# ADR-0024 — Factory-preset identity is an internal id, carried in plug-in state

**Status:** **Accepted** (**amended 2026-08-07, before merge** — see §Amendment; the original
decision text is preserved verbatim below it). The Amendment adds fields to the Serialization
Registry, which is an `ARCHITECTURE_REVIEW_GATE.md` item: **Architecture Review cleared by the
maintainer on 2026-08-07**, re-confirmed **2026-08-08**. See §*Serialization sign-off* in the
Amendment for what was reviewed.

## Context
The preset browser shows one flat list: ten built-in FACTORY presets followed by the USER presets
found in the local `.anamorph` folder. Nothing prevents a user preset from being saved under a
factory preset's name — "Save Preset…" pre-fills the current name, so saving straight after loading
*Wide Master* produces exactly that.

Until 0.9.2 the manager knew only `juce::String current`, and `currentIndex()` was a name scan.
The factory block is list-front, so the factory row always answered first.

## Problem
With a duplicate name, the drop-down tick could never sit on the user preset — not even
immediately after saving it — and `‹ ›` stepped from the wrong row. The name is a *label*; it was
being used as an *identity*, and the two namespaces (a fixed built-in table and an arbitrary user
folder) are not disjoint.

## Options
- **A. Forbid the collision** (reject or auto-rename a save that matches a factory name). Rejected —
  it takes a naming decision away from the user to work around an internal representation.
- **B. Tie-break the name scan** (user preset wins, or factory wins). Rejected — whichever side
  wins, the other becomes unreachable; it moves the bug rather than removing it.
- **C. Give each factory preset an immutable internal id; identify a user preset by its file.**
  Chosen. Two namespaces that cannot collide. The id never surfaces: the menu, the top bar and the
  Save Preset field all still show the **name**.
- **D. C, plus persist the identity** so a reloaded session also resolves a clash correctly.
  Rejected in the original decision; **adopted by the Amendment below**, with maintainer approval for
  the gated serialization change.

## Decision
1. **Identity, not name.** `PresetManager::Entry` carries a `factoryId` for factory rows;
   `PresetManager::Selection` records what produced the current sound — a factory id, a user file,
   or `unknown`. `currentIndex()` resolves the identity first and, when the identity is *known but
   absent from the list* (a file loaded from outside the preset folder, a user preset deleted or
   renamed on disk), returns **no row** rather than falling back to the name — falling back is
   precisely the mis-tick this ADR removes.
2. **Factory ids are immutable.** They are compile-time strings, not table positions, so reordering
   or renaming the presets is safe. Renaming a preset is a display change; renaming an **id** is
   not permitted — live A/B slots and undo entries may still hold the old one.
3. **The identity is never serialized.** It lives in memory and travels inside `StateSet`, so A/B
   and undo preserve it (ADR-0008), but `getStateInformation` is byte-for-byte unchanged. Writing
   it would add a field to `AnamorphRoot` or `AB`, which is a `SESSION_COMPATIBILITY_POLICY` rule-1
   change, an `ARCHITECTURE_REVIEW_GATE` item and an AI-agent Hard Stop — and a 0.9.1 session has
   nothing to restore into it anyway.
4. **A restored session resolves a clashing name to the FACTORY preset.** That is a *documented
   tie-break*, not an accident: a saved session carries the name alone, and this is the answer every
   earlier version gave. It is pinned by a test so it cannot drift silently.
5. **Saving re-baselines the undo snapshot.** `saveUser` fires `onSaved` → `syncCommitted()`.
   A save changes no parameter value, so the gesture-gated coalescer never notices it; without the
   hook the processor's `committed` snapshot keeps the pre-save name, baseline and identity, and the
   next undo restores them. Re-baselining creates no undo step, which is correct — a save is not a
   sound change. The same reasoning applies to a preset switch whose sound is identical to the
   current one: no undo step, but the baseline still adopts the new metadata.

## Consequences
- A user preset may share a factory preset's name; both rows stay individually selectable, and
  `‹ ›` steps from whichever was actually loaded. The two are told apart in the UI by their
  FACTORY / USER section, since the label is the same by construction.
- A `.anamorph` file loaded from outside the preset folder ticks nothing. Correct: it is on no row.
- ~~**Residual (accepted):** reopening a project restores the name only, so a still-clashing name
  returns the tick to the factory row until the user picks one. Fixing this requires (D) and is
  therefore a Hard Stop — **an agent must not implement it**; it needs human Architecture Review.~~
  **Closed by the Amendment below** (the maintainer supplied the Architecture-Review approval); the
  name fallback now applies only to state that carries no identity at all.
- **Identity matching is a raw path-string compare, deliberately.** `currentIndex()` matches a user
  preset with `e.file == sel.file`, and `juce::File::operator==` compares the path string
  (case-insensitively on Windows) with **no canonicalisation whatsoever**: no symlink resolution, no
  `/private/var`↔`/var` folding on macOS, no UNC↔mapped-drive folding on Windows, no relative-path
  normalisation. A chooser result that reaches a preset-folder file by a different *spelling* than
  `refresh()` produced therefore fails to match its own menu row and shows **no tick**. That is the
  safe direction (never a *wrong* tick) and it is the documented contract, not an oversight:
  `getLinkedTarget()` would change what "the same preset" means — a symlink and its target would
  become one identity — and brings its own failure modes on every platform. On macOS the
  `/private/var` case is common enough that a tester may report it as a bug; this paragraph is the
  answer.
- **Cross-machine resolution holds only for the name-encoded case.** A preset stored by bare file
  name resolves against whatever `presetDirectory()` is on the machine opening the project, so it
  survives the move. A preset stored by absolute path does not: `decodeSelection` treats the stored
  string as a path only when `juce::File::isAbsolutePath` accepts it on the *current* platform, so a
  Windows `C:\…` path reopened on Linux (or the reverse) fails that test, resolves to a file that
  does not exist, and ticks nothing. Safe, and consistent with "unresolvable ticks nothing" — but it
  means the portability property applies to presets sitting **directly in** the preset folder, which
  is the case users actually hit when they share a project alongside a preset folder.
- **A file name that looks like a path is stored as a path.** The bare-name encoding is only used
  when the name cannot be mistaken for one: `juce::File::isAbsolutePath` accepts a leading `~` on
  POSIX, and nothing stops a user dropping `~foo.anamorph` into the preset folder by hand (the
  manual tells them to manage presets as files). Encoding such a direct child by bare name would
  decode to the literal relative string and lose the tick, so it takes the absolute-path branch
  instead — less portable for that one preset, but `decode(encode(s)) == s` holds, which is the
  invariant the whole design rests on.
- **One** user-visible string was added, late and deliberately (constraint C8, maintainer sign-off
  2026-08-08): the top bar renders **No Preset** when the preset name is empty. The ids themselves
  still never surface. The empty name is a real state this change set created — a pre-0.6.4 A/B slot
  carries no preset of its own — and rendering it verbatim left the top-bar button an unlabelled
  clickable region. The substitution lives in `refreshPresetDisplay`, **not** in
  `PresetManager::currentName()`, because that accessor feeds the serialized `presetName` and the
  Save Preset pre-fill: a placeholder there would be written into the session and offered as a preset
  file name. State test 5 pins the separation (stored name and saved property both stay empty).

  **Confirmed by the maintainer, 2026-08-08**, as three standing conditions on that string, not just
  on the wording: the placeholder belongs to the **editor presentation layer only**; the model keeps
  returning an empty name; serialization keeps storing an empty `presetName`. A UI-only placeholder
  satisfies C8 precisely *because* it is none of those things — it must never become part of the
  preset identity or of serialized state. Any change that moves it into `PresetManager` breaks the
  sign-off, and state test 5 fails first.

## Related code
- `src/PresetManager.h:30-36` (`Entry::factoryId`), `:54-76` (`Selection`, incl. its equality
  operators), `:78-94` (`SelectionFields`, `encodeSelection`/`decodeSelection`), `:123`
  (`selection()`), `:127-155` (`setMeta`), `:162-169` (`adoptRestoredState`), `:177-183` (`onSaved`)
- `src/PresetManager.cpp:22-61` (the factory table + `findFactory`), `:108-132` (`currentIndex`),
  `:202-250` (`load`), `:252-266` (`loadFile`), `:278-314` (`saveUser`), `:316-327`
  (`adoptRestoredState`), `:333-386` (`encodeSelection`/`decodeSelection`)
- `src/PluginProcessor.h:176-188` (`StateSet::selection`)
- `src/PluginProcessor.cpp:68-95` (the hooks, incl. `onSaved`), `:256-272`
  (`currentStateSet`/`applyStateSet`), `:430-462` (`commitPresetSwitchUndoStep`, incl. the
  identity-moved guard on redo), `:563-584` (`writeSelection`/`readSelection`), `:665-706`
  (`readSlot`)
- `tests/state_tests.cpp` — state tests 10, 11 and 12

## Amendment — the identity IS persisted, in plug-in state only (2026-08-07, pre-merge)

Decision **3** above (never serialized) and the residual it produced in **Consequences** are
reversed on maintainer instruction, which also supplies the Architecture-Review approval the change
needs. Nothing else in this ADR changes: the identity model (1), the immutability rule (2) and the
save re-baselining (5) all stand, and (4) — the factory tie-break — survives as the *fallback* for
state that carries no identity rather than as the answer for every reload.

**What changed.** `getStateInformation` now writes three additive strings alongside `presetName` /
`presetBaseline`, and three more per A/B slot: a kind (`""` / `"factory"` / `"user"`), a factory id,
and a user-preset file. `setStateInformation` decodes them back into a `Selection`. The encoding
lives on `PresetManager` (`encodeSelection` / `decodeSelection`), so exactly one place knows the wire
form.

**What did NOT change, and is the point of the shape chosen:**

- **User preset files are untouched.** No id, no metadata, no format change — a `.anamorph` written
  by 0.9.1 and one written by 0.9.2 are identical. The identity lives in the *session*, which is the
  only place that knows which preset a given project was using.
- **Parameter restore is independent of identity restore.** The sound comes from the `ANAMORPH`
  child; the identity is metadata applied afterwards via `setMeta` / `adoptRestoredState`, neither of
  which touches a parameter. Every fallback below therefore restores the exact saved sound.
- **`SESSION_COMPATIBILITY_POLICY` rule 1 is respected** — nothing was removed and no existing field
  changed meaning; these are additions, and rule 2 (additions tolerate absence) is what makes the
  pre-0.9.2 path work unchanged.
- A user preset sitting **directly in** the preset folder is stored as its **file name**, not an
  absolute path: there the name is already a complete identity, it keeps the user's home directory
  out of the saved project, and a project opened on another machine still resolves. Everything else
  — a preset loaded from *outside* the folder, one nested in a sub-folder of it, or one whose name
  `juce::File::isAbsolutePath` would accept — stores its absolute path, so `decode(encode(s)) == s`
  stays true instead of silently re-pointing at a same-named file in the folder or decoding to a
  literal relative string. The folder test is a **direct-child** one (`getParentDirectory() ==
  presetDirectory()`), deliberately **not** `juce::File::isAChildOf`, which recurses: a nested file
  would otherwise be stored by bare name and decode to a different file. `refresh()` scans
  non-recursively, so a direct child is the only thing that can ever be a menu row. See the last two
  **Consequences** bullets for the name-ambiguity case and the cross-machine limits.

**Fallbacks, all three verified by state test 12:**

| stored | on reload | result |
|---|---|---|
| a factory id that still exists | resolved | that factory row is ticked |
| a factory id that no longer exists | not found | **no row ticked** — never a same-named substitute |
| a user file that still exists | resolved | that user row is ticked, even against a same-named factory preset |
| a user file that is gone / moved | not found | **no row ticked** — never the same-named factory row |
| nothing (pre-0.9.2, or hand-stripped) | `unknown` | the pre-0.9.2 name fallback, i.e. the factory tie-break of (4) |

The "no row ticked" answers are not new behaviour: they are `currentIndex()`'s existing rule that a
*known* identity which is absent from the list ticks nothing, which is exactly why a wrong-but-well-
formed stored value cannot select the wrong preset.

**Gate status.** This IS a Serialization Registry addition and therefore an
`ARCHITECTURE_REVIEW_GATE.md` item and an AI-agent Hard Stop. It is recorded here as approved by the
maintainer rather than taken by an agent. `SERIALIZATION_REGISTRY.md` carries the six new field rows;
`SESSION_COMPATIBILITY_POLICY.md` rule 4's round-trip list gains the indicator identity.

### Serialization sign-off

**Architecture Review cleared by the maintainer on 2026-08-07, re-confirmed 2026-08-08.** The six
additive fields were **manually reviewed and approved**; they were not merged on a green build.
Recorded here in the form ADR-0023 uses for the other gated change in this line, because
`ARCHITECTURE_REVIEW_GATE.md` §Procedure step 3 makes the ADR the place a gated decision is written
down, and this is the one gate the repository cannot verify from its own tree.

| What was reviewed | State |
|---|---|
| 1 — the change is **additive only**: `presetSource` / `presetFactoryId` / `presetUserFile` on `AnamorphRoot`, and the same trio per A/B slot. No field removed, renamed, or given a new meaning | ✅ `SERIALIZATION_REGISTRY.md`, `AnamorphRoot` + `AB` tables |
| 2 — **backward compatibility**: every addition tolerates absence, and absence decodes to `unknown`, i.e. the pre-0.9.2 name fallback. A 0.9.1 session loads unchanged | ✅ `SESSION_COMPATIBILITY_POLICY.md` rules 1–2; state test 12's pre-0.9.2 case |
| 3 — **forward compatibility**: an older build ignores the unknown properties, so a 0.9.2 session still opens in 0.9.1 with the pre-0.9.2 behaviour | ✅ additive properties on an existing node |
| 4 — **no undocumented serialization behaviour**: every field, its default-if-absent, and the encoding rule are in the registry; the narrative half is in `STATE_SERIALIZATION.md` | ✅ both carriers, citations anchored |
| 5 — **user preset FILES are untouched** — the `.anamorph` format is byte-for-byte what 0.9.1 wrote | ✅ nothing in this change writes a preset file |
| 6 — an ADR records the decision (`ADR_POLICY.md` rule 1, `ARCHITECTURE_REVIEW_GATE.md` step 3) | ✅ this document, registered in `ADR_INDEX.md` |

**Scope of this sign-off.** It covers *these* fields, in *this* change set, on the grounds above. It
is **not** a standing approval: `ARCHITECTURE_REVIEW_GATE.md` still classifies **any** Serialization
Registry field add, removal or semantic change as a gated item and an AI-agent Hard Stop, and the
next one needs its own review and its own record. The release-time
`procedures/RELEASE_COMPATIBILITY_CHECKLIST.md` is a separate gate and is unaffected by this
sign-off — in particular its manual **Session reload** box (a v0.9.1 binary's session opened in
v0.9.3) is still owed before a release is cut.

**Amended related code:** `src/PresetManager.h` (`SelectionFields`, `encodeSelection`,
`decodeSelection`, the two-argument `adoptRestoredState`), `src/PresetManager.cpp` (the encode/decode
bodies), `src/PluginProcessor.cpp` (`writeSelection`/`readSelection`, `getStateInformation`,
`setStateInformation`, `readSlot`), `tests/state_tests.cpp` (state test 12; state test 1's schema
shape; state test 10's reload assertion; state test 5's legacy-slot metadata expectation, which had
pinned the pre-fix "keeps whatever the slot held" behaviour and now asserts the documented default).

---

Evidence [Verified] — **as amended**; this block describes the ADR in force, not the original
decision preserved above it:
- Source: the **Related code** list above, plus the **Amended related code** in the Amendment.
- Tests: `AnamorphStateTests`, 894 checks, green. **State test 10** — the shared-name save, both
  rows selectable, the A/B round-trip, undo after a save, redo invalidation on an identical-sounding
  switch, the outside-folder file and the deleted user preset. **State test 11** — factory-id
  integrity (present, unique, every one resolving), which is what makes `load()`'s assert
  unreachable. **State test 12** — the whole restore matrix of the Amendment: factory restore, user
  restore against a same-named factory preset, an unresolvable factory id, a missing user preset, a
  preset nested in a sub-folder, a direct-child preset whose name `isAbsolutePath` accepts, a
  pre-0.9.2 session with no identity, and per-A/B-slot identity — each asserting the restored
  parameters are **bit-identical**, which is the evidence that the identity is metadata and never
  reaches the sound. **State test 5** covers the other half of the per-slot rule: a legacy
  (params-only) AB node restores the slot's name, baseline and identity to their **defaults**,
  including on a repeated restore into one live instance. **State test 1** pins the new schema shape (the
  three root fields + six slot fields); it was amended for that, and its passing is no longer
  evidence that the saved state is unchanged — the saved state gained six additive fields, by
  design and with approval.
- History: CHANGELOG.md `[0.9.2]`; PR #100.
