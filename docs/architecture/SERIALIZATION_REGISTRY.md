# SERIALIZATION_REGISTRY.md

Field-level ledger of everything written to session state. Companion to
`STATE_SERIALIZATION.md`.

## INVARIANT (binding)

> **Serialization fields are immutable.** Removal is prohibited. Deprecation requires
> migration support (a read path for the old field). Adding a field is allowed only if absence
> is handled (a default), so older sessions still load.

Evidence [Verified]: backward-compat paths at src/PluginProcessor.cpp:914-917 (pre-0.8.4 `migrateFromLegacyApvts`), :652-693 (pre-0.6.4 `readSlot`), :700-705 (v0.2 bare APVTS);
src/InternalState.h:112-197.

## `AnamorphRoot` properties

| Field | Type | Introduced | Migration Required | Required | Default if absent |
|---|---|---|---|---|---|
| `presetName` | String | ≥0.6 (Unverified exact) | No | No | `PresetManager::defaultName()` — **absent ≠ empty**, see below |
| `presetBaseline` | String | 0.6.x (#6) [Partially Verified] | No | No | `adoptRestoredState` clean baseline |

Source: src/PluginProcessor.cpp:845-846 (write), :633-642 (read), :727-741 (adoption).

**`presetName`: absent and empty are different answers.** The property is *absent* only in a session
that predates it (< 0.6); it resolves to `PresetManager::defaultName()`, whose name-fallback tick is
the documented ADR-0024 answer for a session carrying no identity. A *present but empty* value is a
real state — "this state has no preset" — and is adopted verbatim; since 0.9.2 a session saved while
sitting on a nameless A/B slot stores exactly that, so turning it back into a name would invent one.
Neither case may fall back to `presets.currentName()`: `presets` is a processor member and a host
reuses one instance across `setStateInformation` calls, so the live name is the **previous project's**
— the same rule `readSlot` follows for the A/B slots. `adoptRestoredState` therefore assigns the name
unconditionally, and only `setStateInformation` (which can see `hasProperty`) resolves absence.
Source: src/PresetManager.h:103-108 (`defaultName`); src/PresetManager.cpp:365-376
(`adoptRestoredState`).

**A malformed legacy Setting resolves to a valid setting, deterministically** (2026-09-01, ER-STATE-17). Pre-0.8.4 sessions carry Oversampling, UI Scale and Scope Persistence as APVTS `PARAM`s that `InternalState::migrateFromLegacyApvts` converts. Each value now passes the same usability predicate as the session and preset paths (`SerializedNumber.h`: plain decimal text, finite after float narrowing) — anything else means the field's **default**, exactly as an absent node does — and the choice indices are clamped into the ComboBox domain (`oversample` ids 1..4, `uiScale` 1..5; `scopePersist` to 0..1) in double **before** the integer conversion, so that conversion is defined for every input and the `+ 1` cannot overflow. Before this the value went straight into `(int)`, which is undefined for NaN, ±inf and out-of-range doubles, and JUCE's parser accepts "nan"/"inf": measured on x86-64 every such value became −2147483647 in the tree and was written back out on the next save; "2147483647" wrapped to INT_MIN through a second UB; AArch64 saturated the same inputs differently. Valid legacy values convert exactly as before (State tests 5 and 6 unchanged); State test 28 pins 88 synthetic cases over both legacy shapes, plus 36 on the real frozen pre-0.8.4 fixture mutated in place with its surrounding session asserted intact (round 13). Source: src/InternalState.h:139-190.

**A recognised root with no sound child is not a restore either** (2026-09-01, ER-STATE-15). An
`AnamorphRoot` whose `ANAMORPH` child is absent restores no parameter at all, so `setStateInformation`
now returns before the adoption block for it, exactly as the foreign-root case below does. Adopting
the metadata would have relabelled the sound the user already had with the incoming session's preset
name, indicator tick and dirty baseline, and handed it the incoming Settings, while nothing audible
moved — "metadata describing a session that was never loaded", which is the same failure the next
paragraph exists to prevent. `getStateInformation` appends the child unconditionally, so no session
this plug-in has ever written reaches that branch and no valid session changes behaviour; what
reaches it is a truncated, hand-edited or forward-version blob. Source:
src/PluginProcessor.cpp:876-895; State test 27.

**A chunk of neither recognised shape is not a restore at all.** `setStateInformation` handles two
root shapes — `AnamorphRoot` and the bare v0.2 APVTS tree. Anything else (a foreign or
forward-version root) matches neither, so no parameter, Settings value or A/B slot is touched, and
the function **returns before the adoption block**: preset name, identity, checkmark and dirty
baseline all stay exactly as they were. That is the same answer the guard at the top already gives a
blob `getXmlFromBinary` cannot parse — input we do not recognise never becomes state. Source:
src/PluginProcessor.cpp:1025-1059 (the else-branch), :607 (the unparsable-blob guard).

### The preset **indicator identity** (0.9.2, ADR-0024 as amended)

> **Architecture Review sign-off.** Adding these fields is an `ARCHITECTURE_REVIEW_GATE.md` item.
> Cleared by the maintainer on **2026-08-07**, re-confirmed **2026-08-08**; what was reviewed is
> recorded in **ADR-0024 §Amendment → Serialization sign-off**. That sign-off covers these fields
> only — the gate still applies to every future field add, removal or semantic change.

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
src/PresetManager.cpp:382-435 (`encodeSelection` / `decodeSelection`);
src/PluginProcessor.cpp:818-839 (`writeSelection`/`readSelection`), :585 (root write),
:594 / :598 (per-slot write), :638 (root read), :680 (per-slot read).

## `ANAMORPH` child (APVTS)

The full APVTS tree — all 36 parameters from `PARAMETER_REGISTRY.md`. Serialized via
`apvts.copyState()`; restored via `apvts.replaceState` **then** `reassertParameters`. Each
parameter is one `PARAM` node (`id`, `value`, **`raw`**). Field stability is governed by the
**Parameter ID immutability** invariant.

| Field | Type | Introduced | Migration Required | Required | Default |
|---|---|---|---|---|---|
| `<ANAMORPH>` (36 PARAM nodes) | ValueTree | ≥0.2 | No (additive only) | Yes | per-parameter defaults — an absent node resets the parameter on a **reused live instance** too, as `applySoundTree` does for presets. Applied by `apvts.replaceState` itself: it appends an empty PARAM node for every adapter the new tree does not carry, and that append sets the parameter to its default through `setValueNotifyingHost` (measured 2026-08-31, ER-STATE-07 probe step 0b). `reassertParameters`' default branch is a redundant backstop for the same rule, not its source — the ER-STATE-01 claim that absent nodes were skipped entirely was wrong |
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

**"Required: No" means the Default is APPLIED, not that the field is skipped** (2026-09-01,
ER-STATE-18). `InternalState::restoreState` writes all six fields on every restore: one the node
carries is taken from it, one it omits takes the Default column above. `tree` is a processor member
and a host restores into ONE live instance repeatedly, so skipping an absent field does not mean
"leave it alone" — it means "keep the PREVIOUS project's value", which is not a state the incoming
session ever described, and which the next save then writes out as if it were. Measured before the
loop wrote unconditionally (`--partial-settings-probe`): a modern session omitting a single Setting
inherited the previous project's value in **6 cases out of 6**, while the pre-0.8.4/v0.2 path —
`migrateFromLegacyApvts`, which has always written all six — inherited in **0**. A session that
carries the field is unaffected. State test 29 pins all four cases (omitted, explicitly present,
legacy, malformed). Source: src/InternalState.h:107-133.

**‡** Sessions saved **before** 0.8.4 have no `ANAMORPH_INTERNAL` child; these values are
recovered from the legacy APVTS PARAM nodes by `migrateFromLegacyApvts` (choice indices are
0-based legacy → 1-based ComboBox). Evidence [Verified]: src/InternalState.h:140-197;
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
pre-0.6.4 `slotA`, a payload that fails to parse, **or — since 2026-08-31, ER-STATE-02 — a payload
that parses to a tree of the wrong type**: `readSlot` accepts only `apvts.state.getType()`, because
a foreign-typed tree applied via `replaceState` silently re-typed the live APVTS and every later
save then wrote a params child a fresh instance's restore skipped) would otherwise keep the previous
restore's **sound** while its name, baseline and identity were reset around it.

**A rejected slot payload does not leave its metadata behind** (checked 2026-09-01, ER-STATE-16 —
REFUTED as a defect, recorded so the question is not reopened). `readSlot` reads the name, baseline
and identity outside the params branch, so a slot whose payload is refused briefly holds that
payload's metadata. It is unreachable: `StateSet::isValid()` is `params.isValid()`, so the slot is
invalid, and `abEnsureInit()` assigns `slot = currentStateSet()` — the WHOLE struct, metadata
included, not just the params. Every reader of `abSlot[]` (`abSwitchTo`, `abCopyToOther`,
`getStateInformation`) calls `abEnsureInit` first. Gating the three reads on `params.isValid()` was
implemented and measured to change no test outcome, and was reverted; State test 27 pins the
contract that the reseeded slot's sound and label describe the same state.

The params default is not an empty tree but **"lazily initialised from current"** (the table above),
and an **invalid** tree is how this processor already spells that: `StateSet::isValid()` is
`params.isValid()`, and `abEnsureInit()` re-seeds an invalid slot from `currentStateSet()` before
anything can read it. So a slot with no usable payload comes back seeded from the state that was
just restored — sound and metadata from one project.

**A blob that carries no `AB` node at all gets the same answer, applied to the slot set as a
whole** (2026-09-01, ER-STATE-12). `readSlot`'s rule above could only reach blobs that HAVE an `AB`
node, since it is called from inside that node's branch. Two restore paths carry no A/B data —
an `AnamorphRoot` with no `AB` child (every field in the table above is "Required: No", so the node
itself is optional) and a **v0.2** bare-APVTS session, which predates the feature — and both left
`abSlot[]` and `active` holding the PREVIOUS restore's values on a reused instance, so the next A/B
switch recalled the previous project's sound underneath the restored one. `abResetToDefaults()`
now applies the documented defaults (`active` → 0, both slots → invalid, i.e. "lazily initialised
from current") on those two paths, and the existing `abEnsureInit()` re-seeds from the state that
was just restored. A blob that DOES carry an `AB` node is unaffected: its slots restore as before.
Source: src/PluginProcessor.cpp:776-781 (`abResetToDefaults`, beside `abEnsureInit`), :980 (the
v0.2 branch) and :955 (the `AB`-absent branch); State test 26.

**Both slots get that same answer.** Slot B used to be seeded from a *copy of slot A* instead. On the
path that runs every time — construction, where both slots are invalid — the two are
indistinguishable, since slot A has just been seeded from the same live state. They diverged only
when slot A was valid and slot B was not, i.e. an `AB` node whose `slotBParams` alone was missing or
unparsable: slot B came back as a **duplicate of slot A** rather than as the state just restored, and
a later save wrote that duplicate out. Source: src/PluginProcessor.cpp:943-1008
(`readSlot`), :693-711 (`abEnsureInit`); src/PluginProcessor.h:148-161 (`StateSet::isValid`).

An empty `slotABase` / `slotBBase` means **"no baseline was recorded"**, which is *not* the same as
"modified". Only a pre-0.6.4 slot can produce it — every in-memory producer fills it — and
`PresetManager::setMeta` resolves it the way `adoptRestoredState` resolves an absent root
`presetBaseline`: the state being applied becomes its own **clean** baseline. A literal `""` would
compare unequal to every possible `soundSig()`, so such a slot would read as permanently edited and
the top bar would render a bare ` *` — a modified-marker against a preset the slot does not have
(its name is empty by the same rule). Source: src/PresetManager.h:127-155 (`setMeta`);
src/PresetManager.cpp:365-376 (`adoptRestoredState`, the root-side rule).

**◊** Pre-0.6.4 sessions stored params-only under `slotA`/`slotB`; `readSlot` migrates them.
Evidence [Verified]: src/PluginProcessor.cpp:986-987 (the legacy-key fallback inside `readSlot`, :889-942);
the per-slot identity is written at :831 / :835 and read at :918.

## Legacy root formats (read-only compatibility)

| Format | Detection | Handling |
|---|---|---|
| v0.2 bare APVTS tree | `xml->hasTagName(apvts.state.getType())` | `apvts.replaceState` |

Source: src/PluginProcessor.cpp:1010-1024.

## Notes

- Exact "Introduced" versions are **Unverified** where marked: no git tags exist. The 0.8.4
  introduction of `ANAMORPH_INTERNAL` is Partially Verified from README + the migration code.
- No serialization field has been **removed**. The only structural change (0.8.4) *added* a
  child and *moved* fields out of APVTS, with a read-migration — the model future changes must
  follow.
