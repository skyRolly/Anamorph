# SERIALIZATION_REGISTRY.md

Field-level ledger of everything written to session state. Companion to
`STATE_SERIALIZATION.md`.

## INVARIANT (binding)

> **Serialization fields are immutable.** Removal is prohibited. Deprecation requires
> migration support (a read path for the old field). Adding a field is allowed only if absence
> is handled (a default), so older sessions still load.

Evidence [Verified]: backward-compat paths at src/PluginProcessor.cpp:618-621 (pre-0.8.4 `migrateFromLegacyApvts`), :642-688 (pre-0.6.4 `readSlot`), :690-695 (v0.2 bare APVTS);
src/InternalState.h:92-128.

## `AnamorphRoot` properties

| Field | Type | Introduced | Migration Required | Required | Default if absent |
|---|---|---|---|---|---|
| `presetName` | String | ≥0.6 (Unverified exact) | No | No | `PresetManager::defaultName()` — **absent ≠ empty**, see below |
| `presetBaseline` | String | 0.6.x (#6) [Partially Verified] | No | No | `adoptRestoredState` clean baseline |

Source: src/PluginProcessor.cpp:567-568 (write), :623-632 (read), :700-714 (adoption).

**`presetName`: absent and empty are different answers.** The property is *absent* only in a session
that predates it (< 0.6); it resolves to `PresetManager::defaultName()`, whose name-fallback tick is
the documented ADR-0024 answer for a session carrying no identity. A *present but empty* value is a
real state — "this state has no preset" — and is adopted verbatim; since 0.9.2 a session saved while
sitting on a nameless A/B slot stores exactly that, so turning it back into a name would invent one.
Neither case may fall back to `presets.currentName()`: `presets` is a processor member and a host
reuses one instance across `setStateInformation` calls, so the live name is the **previous project's**
— the same rule `readSlot` follows for the A/B slots. `adoptRestoredState` therefore assigns the name
unconditionally, and only `setStateInformation` (which can see `hasProperty`) resolves absence.
Source: src/PresetManager.h:103-108 (`defaultName`); src/PresetManager.cpp:316-327
(`adoptRestoredState`).

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
folder *and* that name cannot be mistaken for a path; its absolute path in every other case — a file
opened through "Load Preset…" from elsewhere, one nested in a sub-folder, or one whose name
`juce::File::isAbsolutePath` accepts (a leading `~` on POSIX). Both conditions exist to keep
`decode(encode(s)) == s` true: the direct-child test is not a descendant test, because `refresh()`
scans non-recursively and a nested file encoded by name would decode to a *different* same-named
file in the folder; and a `~`-leading bare name would decode to a literal relative path rather than
the file in the folder. Encoding and decoding live in one place —
`PresetManager::encodeSelection` / `decodeSelection`.

Resolution is a **raw path-string compare** with no canonicalisation (`juce::File::operator==`), so
a path that reaches the same file by a different spelling, or an absolute path read on a different
platform, resolves to nothing and ticks nothing. Accepted and explained in ADR-0024 §Consequences.

Absent, empty, half-written or unrecognised all decode to `unknown`, which is the pre-0.9.2 name
fallback (rule 2 of `SESSION_COMPATIBILITY_POLICY.md`). A well-formed value that no longer resolves —
a removed factory id, a deleted or moved user preset — ticks **nothing**; it never falls back to a
same-named preset. Source: src/PresetManager.h:54-76 (`Selection`), :78-94 (`SelectionFields`,
`encodeSelection` / `decodeSelection`);
src/PresetManager.cpp:333-386 (`encodeSelection` / `decodeSelection`);
src/PluginProcessor.cpp:540-561 (`writeSelection`/`readSelection`), :575 (root write),
:584 / :588 (per-slot write), :628 (root read), :670 (per-slot read).

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
and fallbacks, so switching A/B after a reload ticks each slot's own row.

**`readSlot` resets the whole slot first, then overlays what the node carries.** `abSlot[]` are
processor members and a host may restore into one live instance repeatedly, so absent must mean the
default rather than "whatever the previous session left" — and that has to hold for the slot as a
**whole**, not field by field, or the two halves of one slot come out of two different projects.
Concretely: an `AB` node that exists but whose slot params cannot be read (no `slotAParams` *and* no
pre-0.6.4 `slotA`, or a payload that fails to parse) would otherwise keep the previous restore's
**sound** while its name, baseline and identity were reset around it.

The params default is not an empty tree but **"lazily initialised from current"** (the table above),
and an **invalid** tree is how this processor already spells that: `StateSet::isValid()` is
`params.isValid()`, and `abEnsureInit()` re-seeds an invalid slot from `currentStateSet()` before
anything can read it. So a slot with no usable payload comes back seeded from the state that was
just restored — sound and metadata from one project. Source: src/PluginProcessor.cpp:642-688
(`readSlot`), :490-498 (`abEnsureInit`); src/PluginProcessor.h:113-126 (`StateSet::isValid`).

An empty `slotABase` / `slotBBase` means **"no baseline was recorded"**, which is *not* the same as
"modified". Only a pre-0.6.4 slot can produce it — every in-memory producer fills it — and
`PresetManager::setMeta` resolves it the way `adoptRestoredState` resolves an absent root
`presetBaseline`: the state being applied becomes its own **clean** baseline. A literal `""` would
compare unequal to every possible `soundSig()`, so such a slot would read as permanently edited and
the top bar would render a bare ` *` — a modified-marker against a preset the slot does not have
(its name is empty by the same rule). Source: src/PresetManager.h:120-141 (`setMeta`);
src/PresetManager.cpp:301-306 (`adoptRestoredState`, the root-side rule).

**◊** Pre-0.6.4 sessions stored params-only under `slotA`/`slotB`; `readSlot` migrates them.
Evidence [Verified]: src/PluginProcessor.cpp:678-682 (the legacy-key fallback inside `readSlot`, :642-688);
the per-slot identity is written at :584 / :588 and read at :670.

## Legacy root formats (read-only compatibility)

| Format | Detection | Handling |
|---|---|---|
| v0.2 bare APVTS tree | `xml->hasTagName(apvts.state.getType())` | `apvts.replaceState` |

Source: src/PluginProcessor.cpp:690-695.

## Notes

- Exact "Introduced" versions are **Unverified** where marked: no git tags exist. The 0.8.4
  introduction of `ANAMORPH_INTERNAL` is Partially Verified from README + the migration code.
- No serialization field has been **removed**. The only structural change (0.8.4) *added* a
  child and *moved* fields out of APVTS, with a read-migration — the model future changes must
  follow.
