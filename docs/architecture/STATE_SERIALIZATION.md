# STATE_SERIALIZATION.md

How session state is saved and restored. The field-level ledger is in
`SERIALIZATION_REGISTRY.md`; binding rules are in
`docs/policies/SESSION_COMPATIBILITY_POLICY.md`.

Evidence [Verified]: src/PluginProcessor.cpp:573-603 (`getStateInformation`), :605-744
(`setStateInformation`), :550-571 (the `writeSelection` / `readSelection` helpers);
src/PresetManager.cpp:333-386 (`encodeSelection` / `decodeSelection`).

## On-disk schema (`getStateInformation`)

A root `ValueTree` "AnamorphRoot" serialized to binary via `copyXmlToBinary`:

```
AnamorphRoot
├─ (property) presetName      : String   -- live preset name (survives reload, F2)
├─ (property) presetBaseline  : String   -- clean-signature for the dirty-star (#6)
├─ (property) presetSource    : String   -- "" | "factory" | "user"  ┐ the indicator IDENTITY
├─ (property) presetFactoryId : String   -- set when source=factory  │ (0.9.2, ADR-0024 as
├─ (property) presetUserFile  : String   -- set when source=user     ┘ amended); see below
├─ <ANAMORPH>           (APVTS state — all host parameters; each PARAM node carries id, value,
│                        AND an additive `raw` attribute = exact normalised getValue(), see below)
├─ <ANAMORPH_INTERNAL>  (host-hidden InternalState: oversample, uiScale, scopePersist,
│                        metersOn, tooltipsOn, uiAnimations)
└─ <AB>
   ├─ (property) active         : int       -- 0 = A, 1 = B
   ├─ (property) slotAParams    : String    -- XML of slot A's APVTS tree
   ├─ (property) slotAName      : String
   ├─ (property) slotABase      : String
   ├─ (property) slotASource    : String    ┐ slot A's own indicator identity, same
   ├─ (property) slotAFactoryId : String    │ encoding and same defaults as the root trio
   ├─ (property) slotAUserFile  : String    ┘
   ├─ (property) slotBParams    : String
   ├─ (property) slotBName      : String
   ├─ (property) slotBBase      : String
   ├─ (property) slotBSource    : String    ┐ slot B's own indicator identity
   ├─ (property) slotBFactoryId : String    │
   └─ (property) slotBUserFile  : String    ┘
```

**Root vs slot identity.** The root trio records the identity of the **live** selection — the row
the drop-down should tick when the project is reopened. Each slot carries its **own** trio, so
switching A/B after a reload ticks that slot's row rather than the one that happened to be live at
save time. They are written from different places and can legitimately differ: the root from
`presets.selection()`, each slot from `abSlot[n].selection`, which is refreshed when that slot is
switched into or copied over. On restore only the root's is applied to the live state; a slot's is
applied when the user switches to it.

**Metadata only.** Nothing in the identity trio reaches a parameter. The sound is restored from the
`ANAMORPH` child *before* the identity is read, and the identity is applied through
`PresetManager::setMeta` / `adoptRestoredState`, neither of which writes a parameter — so a session
restores the exact saved sound whether or not its identity still resolves.

**Absence, and values that no longer resolve.** All six are additive and optional. Absent, empty,
half-written or unrecognised decodes to `Kind::unknown` (`PresetManager::decodeSelection`), which is
the pre-0.9.2 behaviour: resolve the row by NAME. A *well-formed* value that no longer resolves —
a factory id removed by a later version, a user preset deleted, renamed or moved — ticks **nothing**
rather than falling back to a same-named row. The field-level ledger, including the file-name vs
absolute-path encoding rule, is in `SERIALIZATION_REGISTRY.md`.

Evidence [Verified]: src/PluginProcessor.cpp:573-603 (`getStateInformation`).

## `getStateInformation` logic

1. `abEnsureInit()` — lazily materialise both A/B slots.
2. Build "AnamorphRoot"; attach preset name + baseline as properties, then the indicator
   identity via `writeSelection` (`presetSource` / `presetFactoryId` / `presetUserFile`,
   encoded by `PresetManager::encodeSelection`).
3. Append **`copyStateWithRawValues()`** — `apvts.copyState()` with each PARAM node additively
   stamped with its exact normalised `getValue()` as a **`raw`** attribute (see the `raw`-attribute
   note + `SERIALIZATION_REGISTRY.md`; ADR-0013). The A/B slot snapshots (via `currentStateSet()`)
   use the same helper, so their XML also carries `raw`.
4. Append `internal.copyState()` ("ANAMORPH_INTERNAL").
5. Append the "AB" child with both slots' params (as XML strings) + name + baseline + each
   slot's own identity trio, through the same `writeSelection` helper.
6. `copyXmlToBinary`.

## `setStateInformation` logic

1. `getXmlFromBinary` → root tree.
2. **If `AnamorphRoot`:**
   - `apvts.replaceState` from the `ANAMORPH` child, **then `reassertParameters(params)`** —
     synchronously re-apply each parameter from the restored tree, preferring the exact `raw`
     attribute (falling back to the denormalised `value` for legacy sessions). This makes a
     wholesale `replaceState` deterministic + idempotent and round-trips discrete params exactly.
   - Restore InternalState from `ANAMORPH_INTERNAL` **if present**, **else**
     `migrateFromLegacyApvts(params)` (pre-0.8.4 sessions had these as APVTS params).
   - Restore preset name + baseline (dirty-star reproduced) and decode the indicator identity
     (`readSelection` → `PresetManager::decodeSelection`); absent or unrecognised yields `unknown`,
     i.e. the pre-0.9.2 name fallback.
   - Restore A/B slots; the `active` index is **clamped** to a valid slot (`clampAbSlotIndex`,
     `src/AbSlotIndex.h`); per-slot reader falls back to pre-0.6.4 "slotA"/"slotB" (params-only)
     keys. Each slot is **reset to its default first**, then overlaid with whatever the node carries:
     `abSlot[]` are processor members and a host may restore into one live instance repeatedly, so an
     absent field must mean the default rather than whatever the previous session left there — for the
     slot as a **whole**, so its sound and its metadata can never come from two different projects. A
     slot whose params are missing or unparsable comes back **invalid** and `abEnsureInit()` re-seeds
     it from the state just restored (the documented "lazily initialised from current" default) —
     **both** slots, symmetrically; slot B used to be seeded from a copy of slot A, which only
     differed when slot A alone was readable and made slot B a duplicate of it. A
     slot that comes back with an **empty baseline** (only a pre-0.6.4 slot can) becomes clean at its
     own state when it is switched into — "no baseline recorded" is not "modified". Both rules are
     stated field-by-field in `SERIALIZATION_REGISTRY.md`, `AB` child.
3. **Else if the root is the bare APVTS state type:** backward-compat path for v0.2 sessions
   (`apvts.replaceState` + `reassertParameters`).
4. **Else — neither shape:** a foreign or forward-version root. Nothing above ran, so the function
   **returns here**: no parameter, Settings value or A/B slot was touched, and step 5 must not run
   either. Everything in step 5 *adopts* — it would clear the undo history for a session that never
   loaded and write a preset name, identity and baseline describing it, relabelling the sound the
   user actually has. Same answer as the `getXmlFromBinary` guard in step 1: input we do not
   recognise is not a restore.
5. Clear undo history; adopt preset metadata **including the decoded identity**
   (`setMeta` / `adoptRestoredState`); `syncCommitted()`. The name is resolved here and only here,
   because only this scope can tell an **absent** `presetName` (a session predating the field →
   `PresetManager::defaultName()`) from a **present but empty** one (a real "no preset" state →
   adopted verbatim). Neither resolves to the live `presets.currentName()`: a host reuses one
   processor across restores, so that is the *previous* project's label — the same rule step 2
   applies to the A/B slots. See `SERIALIZATION_REGISTRY.md`, `AnamorphRoot` properties.

Evidence [Verified]: src/PluginProcessor.cpp (`getStateInformation` / `setStateInformation` /
`reassertParameters` / `copyStateWithRawValues`).

## Backward-compatibility paths (all must be preserved)

| Legacy format | Handling | Source |
|---|---|---|
| **v0.2**: root *is* the APVTS tree | `setStateInformation` else-branch `apvts.replaceState` | :700-705 |
| **pre-0.6.4**: A/B slots stored params only (`slotA`/`slotB`) | `readSlot` legacy-key fallback | :688-692 (within `readSlot`, :652-693) |
| **pre-0.8.4**: Oversampling/view were APVTS params (no `ANAMORPH_INTERNAL`) | `migrateFromLegacyApvts` | :628-631; InternalState.h:106-128 |
| **pre-0.9.2**: no indicator identity in the session | `decodeSelection` yields `unknown` → name fallback | src/PresetManager.cpp:369-386; :128-131 |

## View-parameter preservation on restore

`applyStatePreservingView` restores a snapshot but **keeps the current** shared view params
(`pid::viewParams` = `bypass`) so an A/B / undo / preset apply never flips the view state.
Evidence [Verified]: src/PluginProcessor.cpp:256-271 (`applyStatePreservingView`).

## Invariants

- **Serialization fields are immutable** — see `SERIALIZATION_REGISTRY.md`. Removal is
  prohibited; deprecation requires a migration path (the 0.8.4 InternalState migration is the
  reference precedent).
- A round-trip save→load must reproduce the exact sound, preset name, dirty-star, A/B slots,
  the active slot, and — since 0.9.2 — the preset indicator identity. The identity is metadata: the
  sound must restore identically even when it does not resolve.
- Changing the schema (new child, renamed property, changed semantics) requires an ADR +
  migration support + Architecture Review (`docs/policies/ARCHITECTURE_REVIEW_GATE.md`).
