# SERIALIZATION_REGISTRY.md

Field-level ledger of everything written to session state. Companion to
`STATE_SERIALIZATION.md`.

## INVARIANT (binding)

> **Serialization fields are immutable.** Removal is prohibited. Deprecation requires
> migration support (a read path for the old field). Adding a field is allowed only if absence
> is handled (a default), so older sessions still load.

Evidence [Verified]: backward-compat paths at src/PluginProcessor.cpp:557-560 (pre-0.8.4 `migrateFromLegacyApvts`), :576-593 (pre-0.6.4 `readSlot`), :596-600 (v0.2 bare APVTS);
src/InternalState.h:92-128.

## `AnamorphRoot` properties

| Field | Type | Introduced | Migration Required | Required | Default if absent |
|---|---|---|---|---|---|
| `presetName` | String | ≥0.6 (Unverified exact) | No | No | falls back to current name |
| `presetBaseline` | String | 0.6.x (#6) [Partially Verified] | No | No | `adoptRestoredState` clean baseline |

Source: src/PluginProcessor.cpp:516-517 (write), :562-567 (read), :608-610 (default).

**Deliberately NOT serialized: the preset *identity*.** Since 0.9.2 a factory preset is
identified in memory by an immutable internal id and a user preset by its file
(`PresetManager::Selection`), so a user preset sharing a factory preset's name no longer
mis-ticks the menu. That identity rides on `StateSet` through A/B and undo but is **not** written
here: adding a root field is an `ARCHITECTURE_REVIEW_GATE` item (rule 1 of
`SESSION_COMPATIBILITY_POLICY.md`), and a 0.9.1 session would have nothing to restore into it.
A restored session therefore carries `presetName` alone and resolves a clashing name to the
factory preset — the same answer every earlier version gave. Source: src/PresetManager.h:38-70;
src/PluginProcessor.h:113-125.

## `ANAMORPH` child (APVTS)

The full APVTS tree — all 36 parameters from `PARAMETER_REGISTRY.md`. Serialized via
`apvts.copyState()`; restored via `apvts.replaceState` **then** `reassertParameters`. Each
parameter is one `PARAM` node (`id`, `value`, **`raw`**). Field stability is governed by the
**Parameter ID immutability** invariant.

| Field | Type | Introduced | Migration Required | Required | Default |
|---|---|---|---|---|---|
| `<ANAMORPH>` (36 PARAM nodes) | ValueTree | ≥0.2 | No (additive only) | Yes | per-parameter defaults |
| `PARAM/@value` | denormalised (real) value | ≥0.2 | No | Yes | per-parameter default |
| `PARAM/@raw` | float — exact normalised `getValue()` | **post-0.8.7** | No ◊ | No | falls back to `@value` |

**◊** `@raw` is **additive and backward-compatible**: APVTS serialises the *denormalised/snapped*
value, which for **discrete** params (Bool/Choice/Int) can differ from the raw `getValue()` by more
than pluginval's `0.1` state-restoration tolerance. `getStateInformation` stamps each `PARAM` with
its exact normalised `getValue()` as `@raw`; `reassertParameters` restores from `@raw` when present,
else from `@value`. Older sessions (no `@raw`) load unchanged; older plugins ignore the unknown
attribute. No field is removed or renamed — the schema is a strict superset.
Evidence [Verified]: src/PluginProcessor.cpp (`getStateInformation` stamps `raw`;
`reassertParameters` prefers it).

Source: src/PluginProcessor.cpp `getStateInformation` / `setStateInformation` / `reassertParameters`.

## `ANAMORPH_INTERNAL` child (InternalState)

| Field | Type | Introduced | Migration Required | Required | Default |
|---|---|---|---|---|---|
| `int_oversample` | int (1..4) | 0.8.4 | Yes ‡ | No | 1 ("Off/1x") |
| `int_uiScale` | int (1..5) | 0.8.4 | Yes ‡ | No | 3 ("M") |
| `int_scopePersist` | double | 0.8.4 | Yes ‡ | No | 0.5 |
| `int_metersOn` | bool | 0.8.4 | Yes ‡ | No | false |
| `int_tooltipsOn` | bool | 0.8.4 | Yes ‡ | No | false |
| `int_uiAnimations` | bool | 0.8.4 | Yes ‡ | No | true |

**‡** Sessions saved **before** 0.8.4 have no `ANAMORPH_INTERNAL` child; these values are
recovered from the legacy APVTS PARAM nodes by `migrateFromLegacyApvts` (choice indices are
0-based legacy → 1-based ComboBox). Evidence [Verified]: src/InternalState.h:106-128;
[Partially Verified] introduced-0.8.4: CHANGELOG.md [0.8.4].

## `AB` child

| Field | Type | Introduced | Migration Required | Required | Default |
|---|---|---|---|---|---|
| `active` | int (0/1) | ≥0.3 (Unverified exact) | No | No | 0 |
| `slotAParams` / `slotBParams` | String (XML of APVTS tree) | 0.6.4 (#6) [Partially Verified] | Yes ◊ | No | lazily initialised from current |
| `slotAName` / `slotBName` | String | 0.6.4 (#6) | No | No | "" |
| `slotABase` / `slotBBase` | String | 0.6.4 (#6) | No | No | "" |

**◊** Pre-0.6.4 sessions stored params-only under `slotA`/`slotB`; `readSlot` migrates them.
Evidence [Verified]: src/PluginProcessor.cpp:586-590 (the legacy-key fallback inside `readSlot`, :576-593).

## Legacy root formats (read-only compatibility)

| Format | Detection | Handling |
|---|---|---|
| v0.2 bare APVTS tree | `xml->hasTagName(apvts.state.getType())` | `apvts.replaceState` |

Source: src/PluginProcessor.cpp:596-600.

## Notes

- Exact "Introduced" versions are **Unverified** where marked: no git tags exist. The 0.8.4
  introduction of `ANAMORPH_INTERNAL` is Partially Verified from README + the migration code.
- No serialization field has been **removed**. The only structural change (0.8.4) *added* a
  child and *moved* fields out of APVTS, with a read-migration — the model future changes must
  follow.
