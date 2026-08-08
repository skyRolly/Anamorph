# ADR-0024 — Factory-preset identity is an internal id, carried in plug-in state

**Status:** Accepted (**amended 2026-08-07, before merge** — see §Amendment; the original decision
text is preserved verbatim below it)

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
- `juce::File` equality is a path-string compare (JUCE does no canonicalisation), so a chooser
  result that reaches a preset-folder file by a different spelling (symlinked `$HOME`,
  `/private/var` vs `/var`, UNC vs mapped drive) fails the identity match and degrades to "no tick",
  which is safe. Not normalised on purpose — `getLinkedTarget()` would change what "the same preset"
  means and has its own failure modes.
- No user-visible string was added (constraint C8): the ids never surface.

## Related code
- `src/PresetManager.h:30-70` (`Entry::factoryId`, `Selection`, `selection()`, `setMeta`, `onSaved`)
- `src/PresetManager.cpp:19-58` (the factory table + `findFactory`), `:96-135` (`currentIndex`),
  `:214-250` (`load`/`loadFile`), `:276-300` (`saveUser`, `adoptRestoredState`)
- `src/PluginProcessor.h:113-125` (`StateSet::selection`)
- `src/PluginProcessor.cpp:36-41` (the hooks), `:233-250` (`currentStateSet`/`applyStateSet`),
  `:410-433` (`commitPresetSwitchUndoStep`), `:576-600` (`readSlot` clears the identity)
- `tests/state_tests.cpp` — state test 10

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
  — a preset loaded from *outside* the folder, or one nested in a sub-folder of it — stores its
  absolute path, so `decode(encode(s)) == s` stays true instead of silently re-pointing at a
  same-named file in the folder. The test is a **direct-child** one (`getParentDirectory() ==
  presetDirectory()`), deliberately **not** `juce::File::isAChildOf`, which recurses: a nested file
  would otherwise be stored by bare name and decode to a different file. `refresh()` scans
  non-recursively, so a direct child is the only thing that can ever be a menu row.

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

**Amended related code:** `src/PresetManager.h` (`SelectionFields`, `encodeSelection`,
`decodeSelection`, the two-argument `adoptRestoredState`), `src/PresetManager.cpp` (the encode/decode
bodies), `src/PluginProcessor.cpp` (`writeSelection`/`readSelection`, `getStateInformation`,
`setStateInformation`, `readSlot`), `tests/state_tests.cpp` (state test 12; state test 1's schema
shape; state test 10's reload assertion).

---

Evidence [Verified]:
- Source: as listed above.
- Tests: `AnamorphStateTests` state test 10 (797 checks total, green) — the shared-name save, both
  rows selectable, the A/B round-trip, undo after a save, the outside-folder file, the deleted user
  preset, and the documented restore tie-break. State test 1 (serialized schema shape) passing
  unchanged is the evidence that nothing was added to the saved state.
- History: CHANGELOG.md `[0.9.2]`; PR #100.
