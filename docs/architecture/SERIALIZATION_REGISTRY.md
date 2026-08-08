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

### The preset **indicator identity** (0.9.2, ADR-0024 as amended)

Three additive `AnamorphRoot` properties, plus three per A/B slot below. They record which preset
row the indicator should point at, so a reopened project ticks the row that produced the sound even
when a user preset shares a factory preset's NAME. **Metadata only** — the sound is restored from
the `ANAMORPH` child and is bit-identical whether or not these resolve — and **nothing is written
into a user preset FILE**: that format is unchanged.

| Field | Type | Introduced | Migration Required | Required | Default if absent |
|---|---|---|---|---|---|
| `presetSource` | String (`""` / `"factory"` / `"user"`) | 0.9.2 | No | No | `""` → identity `unknown` |
| `presetFactoryId` | String | 0.9.2 | No | No | `""` |
| `presetUserFile` | String | 0.9.2 | No | No | `""` |

`presetUserFile` holds the preset's **file name** when it sits **directly in** the user preset
folder, and its absolute path in every other case — a file opened through "Load Preset…" from
elsewhere, or one nested in a sub-folder. The distinction is a direct-child test, not a descendant
test: `refresh()` scans the folder non-recursively, so only a direct child can ever be a menu row,
and encoding a nested file by name would decode to a *different* same-named file in the folder.
Encoding and decoding live in one place — `PresetManager::encodeSelection` / `decodeSelection`.

Absent, empty, half-written or unrecognised all decode to `unknown`, which is the pre-0.9.2 name
fallback (rule 2 of `SESSION_COMPATIBILITY_POLICY.md`). A well-formed value that no longer resolves —
a removed factory id, a deleted or moved user preset — ticks **nothing**; it never falls back to a
same-named preset. Source: src/PresetManager.h:38-95; src/PresetManager.cpp (`encodeSelection`,
`decodeSelection`); src/PluginProcessor.cpp (`writeSelection`/`readSelection`).

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
| `slotASource` / `slotBSource` | String (`""` / `"factory"` / `"user"`) | 0.9.2 | No | No | `""` → identity `unknown` |
| `slotAFactoryId` / `slotBFactoryId` | String | 0.9.2 | No | No | `""` |
| `slotAUserFile` / `slotBUserFile` | String | 0.9.2 | No | No | `""` |

The per-slot trio is the same indicator identity as the root one, with the same encoding, defaults
and fallbacks, so switching A/B after a reload ticks each slot's own row. `readSlot` **assigns** it
unconditionally rather than merging: `abSlot[]` are processor members and a host may restore into one
live instance repeatedly, so absent must mean the default, not "whatever the previous session left".

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
