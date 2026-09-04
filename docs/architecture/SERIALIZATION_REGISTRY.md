# SERIALIZATION_REGISTRY.md

Field-level ledger of everything written to session state. Companion to
`STATE_SERIALIZATION.md`.

## INVARIANT (binding)

> **Serialization fields are immutable.** Removal is prohibited. Deprecation requires
> migration support (a read path for the old field). Adding a field is allowed only if absence
> is handled (a default), so older sessions still load.

Evidence [Verified]: backward-compat paths at src/PluginProcessor.cpp:1385-1388 (pre-0.8.4, `resolveLegacy`), :1287-1360 (pre-0.6.4 `readSlot`), :1388-1410 (v0.2 bare APVTS);
src/InternalState.h:198-374.

## `AnamorphRoot` properties

| Field | Type | Introduced | Migration Required | Required | Default if absent |
|---|---|---|---|---|---|
| `presetName` | String | ≥0.6 (Unverified exact) | No | No | `PresetManager::defaultName()` — **absent ≠ empty**, see below |
| `presetBaseline` | String | 0.6.x (#6) [Partially Verified] | No | No | `adoptRestoredState` clean baseline |

Source: src/PluginProcessor.cpp:1243-1251 (write), :1268-1277 (read), :1104-1105 (adoption).

**`presetName`: absent and empty are different answers.** The property is *absent* only in a session
that predates it (< 0.6); it resolves to `PresetManager::defaultName()`, whose name-fallback tick is
the documented ADR-0024 answer for a session carrying no identity. A *present but empty* value is a
real state — "this state has no preset" — and is adopted verbatim; since 0.9.2 a session saved while
sitting on a nameless A/B slot stores exactly that, so turning it back into a name would invent one.
Neither case may fall back to `presets.currentName()`: `presets` is a processor member and a host
reuses one instance across `setStateInformation` calls, so the live name is the **previous project's**
— the same rule `readSlot` follows for the A/B slots. `adoptRestoredState` therefore assigns the name
unconditionally, and only `setStateInformation` (which can see `hasProperty`) resolves absence.
Source: src/PresetManager.h:103-108 (`defaultName`); src/PresetManager.cpp:638-650
(`adoptRestoredState`).

**A malformed legacy Setting resolves to a valid setting, deterministically** (2026-09-01, ER-STATE-17). Pre-0.8.4 sessions carry Oversampling, UI Scale and Scope Persistence as APVTS `PARAM`s that `InternalState::migrateFromLegacyApvts` converts. Each value now passes the same usability predicate as the session and preset paths (`SerializedNumber.h`: plain decimal text, finite after float narrowing) — anything else means the field's **default**, exactly as an absent node does — and the choice indices are clamped into the ComboBox domain (`oversample` ids 1..4, `uiScale` 1..5; `scopePersist` to 0..1) in double **before** the integer conversion, so that conversion is defined for every input and the `+ 1` cannot overflow. Before this the value went straight into `(int)`, which is undefined for NaN, ±inf and out-of-range doubles, and JUCE's parser accepts "nan"/"inf": measured on x86-64 every such value became −2147483647 in the tree and was written back out on the next save; "2147483647" wrapped to INT_MIN through a second UB; AArch64 saturated the same inputs differently. Valid legacy values convert exactly as before (State tests 5 and 6 unchanged); State test 28 pins 88 synthetic cases over both legacy shapes, plus 36 on the real frozen pre-0.8.4 fixture mutated in place with its surrounding session asserted intact (round 13). Source: src/InternalState.h:254-299.

**A recognised root with no sound child is not a restore either** (2026-09-01, ER-STATE-15). An
`AnamorphRoot` whose `ANAMORPH` child is absent restores no parameter at all, so `setStateInformation`
now returns before the adoption block for it, exactly as the foreign-root case below does. Adopting
the metadata would have relabelled the sound the user already had with the incoming session's preset
name, indicator tick and dirty baseline, and handed it the incoming Settings, while nothing audible
moved — "metadata describing a session that was never loaded", which is the same failure the next
paragraph exists to prevent. `getStateInformation` appends the child unconditionally, so no session
this plug-in has ever written reaches that branch and no valid session changes behaviour; what
reaches it is a truncated, hand-edited or forward-version blob. Source:
src/PluginProcessor.cpp:1283-1353; State test 27.

**A chunk of neither recognised shape is not a restore at all.** `setStateInformation` handles two
root shapes — `AnamorphRoot` and the bare v0.2 APVTS tree. Anything else (a foreign or
forward-version root) matches neither, so no parameter, Settings value or A/B slot is touched, and
the function **returns before the adoption block**: preset name, identity, checkmark and dirty
baseline all stay exactly as they were. That is the same answer the guard at the top already gives a
blob `getXmlFromBinary` cannot parse — input we do not recognise never becomes state. Source:
src/PluginProcessor.cpp:1511-1546 (the else-branch), :607 (the unparsable-blob guard).

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
src/PresetManager.cpp:656-709 (`encodeSelection` / `decodeSelection`);
src/PluginProcessor.cpp:1013-1035 (`writeSelection`/`readSelection`), :585 (root write),
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

**A repair is durable even when the repaired value equals the one already in force**
(2026-09-03, ER-STATE-25). `reassertParameters` repairs a malformed `PARAM` and writes the canonical
value back into the live tree, which is what the next `copyState()` serialises. That write-back used
to sit inside the "did the live value move?" branch, so the durability of a repair depended on an
unrelated fact — and the two coincide often. Unusable text reads as the denormalised **0** through
JUCE's own parser, and the repair resolves to the parameter **default**, so for every parameter whose
range starts at its default (Drive 0..24 dB default 0, Amount 0..1 default 0, Channel Mode's first
choice, …) the corrupt file resolved to the value already loaded, the branch was skipped, and the
malformed text stayed in the tree and in every later save. Measured: `value="abc"` on `channelMode`
restored correctly, left `"abc"` in the live APVTS **and** in the re-saved session, and was still
there after a second full save/reload cycle. The same held for a usable-but-out-of-range `raw="-7"`,
which clamps to the value in force.

The write-back is therefore conditioned on **the input having been repaired**, not on the value
moving: text that is not a usable number at all, or a usable number outside the field's range. Both
attributes are rewritten when the node carries them — `raw` cannot reach a *file* corrupt (it is
re-stamped from the live parameter on every save) but it can sit in the live tree, which A/B slots
and undo snapshots copy and which the next restore prefers over `value`. **Snapping is not repair**:
a stepped parameter moving a legitimate `"0.4"` to its nearest step leaves the text alone, so a
genuinely valid value that merely happens to equal the default is not rewritten. State test 36 pins
all three cases apart, and the `raw`→`value` fallback is unchanged.

Source: src/PluginProcessor.cpp `getStateInformation` / `setStateInformation` / `reassertParameters`.

### The user **preset file** carries this same tree, and its ROOT is the acceptance test

A `.anamorph` preset file is `apvts.copyState().createXml()` — an `<ANAMORPH>` root with the same
`PARAM` nodes. **A file whose root is anything else is not an Anamorph preset and is refused**, by
both loaders, exactly as an unparsable file is (2026-09-02, ER-STATE-24). `PresetManager::loadFile`
returns `false`, `PresetManager::load` is a clean no-op that never opens the undo bracket, and
neither the sound nor the preset identity is touched.

**Why the root and not the fields.** `applySoundTree` resolves each parameter with
`getChildWithProperty ("id", …)`, which searches by PROPERTY and does not care what the root is
called, so a foreign document did two wrong things at once. Every parameter it did not name took the
"absent means default" branch written for a genuinely missing `PARAM` node — and every parameter it
*did* name was **adopted**. Measured against a non-default sound with a two-child
`<SomeOtherPluginPreset>`: `drive` and `width` took the foreign file's values (0.95 and 0.05 plain),
`algorithm`, `monoMakerFreq` and `chorusRate` were reset to their defaults, and `loadFile` returned
**true**. Making the per-parameter fallback keep the current value instead would have left the
foreign file *accepted and merely inert*, which is a weaker contract than the one this registry
states, so the test is on the root and it runs before any per-parameter fallback can see the
document.

This is the same rule the `AB` child already applies to a slot payload (ER-STATE-02: `readSlot`
accepts only `apvtsStateType` (the APVTS root type, captured once at construction) and refuses a foreign-typed tree precisely as it refuses an
unparsable one). **Unchanged by it:** a valid `<ANAMORPH>` root with genuinely missing `PARAM` nodes
keeps the documented per-parameter default behaviour above, and a malformed value inside a matched
node keeps its `SerializedNumber.h` fallback. State test 34 pins all three cases apart.

Source: src/PresetManager.cpp `parseSoundFile` (the shared acceptance test), `load`, `loadFile`.

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
legacy, malformed). Source: src/InternalState.h:194-248.

**‡** Sessions saved **before** 0.8.4 have no `ANAMORPH_INTERNAL` child; these values are
recovered from the legacy APVTS PARAM nodes by `migrateFromLegacyApvts` (choice indices are
**A value that is PRESENT but not valid is REPAIRED on restore, and the repaired value is what gets
persisted** (maintainer decision of 2026-09-02, "Policy B"; measured and implemented rounds 16-18,
ER-STATE-21). Every field above states a Default for its ABSENCE, settled in round 14. A
present-but-invalid value used to be adopted verbatim: measured across nineteen malformed inputs
(`AnamorphStateTests --modern-settings-probe`), all nineteen survived into the next save, eight left
an out-of-domain ComboBox id in the tree and three left a non-finite `int_scopePersist`, and opening
the editor repaired only four. The ingress is bounded — the four writers of these values (the
defaults table, `restoreState`, the clamped `migrateFromLegacyApvts`, and the Settings widgets) all
produce legal values, so a malformed modern value can only come from a hand-edited or corrupted
file — but the damage was durable, and one consequence of it was a real defect: `scopePersist` is
the only setting whose read applies no clamp, and a `nan` or any NEGATIVE stored value reached
`Vectorscope::windowFrames()`'s `(int)` conversion non-finite, which is undefined (the negative case
because `applyScopePersist` raises the value to a fractional power first). That was fixed at the
consumer in round 17 and `Vectorscope::setPersistence` keeps its finiteness guard as the backstop.

Under the approved policy the value is resolved deterministically to one inside its documented
domain, the live state takes it, and it is written back into the tree — so the next save carries the
repaired value and a reload reads it back unchanged. A valid present value is preserved exactly, and
an ABSENT field keeps taking its documented default, which is a separate rule and stays separate.

| field | valid present | finite out of domain | not usable as a number |
|---|---|---|---|
| `int_oversample` | preserved | clamp to the nearest id in 1..4 | default `1` |
| `int_uiScale` | preserved | clamp to the nearest id in 1..5 | default `3` |
| `int_scopePersist` | preserved | clamp into 0..1 | default `0.5` |
| `int_metersOn` | preserved | default `false` † | default `false` |
| `int_tooltipsOn` | preserved | default `false` † | default `false` |
| `int_uiAnimations` | preserved | default `true` † | default `true` |

"Usable as a number" is the repository's existing predicate (`SerializedNumber.h`), the same one the
legacy migration asks — one copy, shared, rather than the two that had drifted apart before. A
ComboBox id is clamped in DOUBLE before the integer conversion, so that conversion is defined for
every input reaching it (the discipline ER-STATE-17 established for the same `[conv.fpint]` reason),
and a fractional id resolves by truncation after the clamp, which is what the ComboBox already did
with it. **No schema change and no property renamed** — only the value a damaged file resolves to,
and the fact that the resolution is now durable instead of re-decided on every load. Measured across
39 cases: 29 invalid values repaired and persisted, 10 valid values preserved unchanged (State test
33; **83 of its checks fail against the pre-policy build** — re-measured at the 39-case size after
round 20 extended it; the figure at its original 30-case size was 62).

**†** A boolean's domain is `{0, 1}` and nothing else — corrected 2026-09-02, round 20, ER-STATE-22.
The first implementation of this policy resolved a boolean as `v != 0.0`, which is the C coercion,
not a domain: `-1`, `-2` and `7` are all outside the two spellings this plug-in's own writer emits
(`juce::var(bool)` reaches XML as "0" or "1"), yet each of them SWITCHED THE SETTING ON, and the
policy then persisted that as a genuine `true`. The asymmetry is what gives the rule away: `0` was
the only value in the whole real line that could not turn a setting on. A value outside `{0, 1}` is
damage exactly as an out-of-domain ComboBox id is, and the policy's own words — a present-but-
invalid value is repaired to a deterministic valid value — resolve it to the field's documented
default, which is also what an ABSENT field resolves to. Booleans are the one Kind with no
"nearest valid value" to clamp toward, so the default is the whole finite-out-of-domain rule for
them; the ComboBox and unit-range fields keep their clamps unchanged. Nine cases were added to
State test 33 for this (30 → 39); **12 of its checks fail against the `v != 0.0` build.**

0-based legacy → 1-based ComboBox). Evidence [Verified]: src/InternalState.h:255-309;
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
that parses to a tree of the wrong type**: `readSlot` accepts only `apvtsStateType`, because
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
switch recalled the previous project's sound underneath the restored one. The restore's decode
(`decodeRestore`, since D-2; `abResetToDefaults()` before it) now yields the documented defaults
(`active` → 0, both slots → invalid, i.e. "lazily initialised from current") on those two paths,
`adoptRestoreTail` assigns the slot set as a whole, and the existing `abEnsureInit()` re-seeds from
the state that was just restored. A blob that DOES carry an `AB` node is unaffected: its slots restore as before.
Source: src/PluginProcessor.cpp:1484-1508 (the `AB`-absent branch of `decodeRestore`), :1388-1410 (the
v0.2 branch) and :1063-1108 (`adoptRestoreTail`, which assigns the slot set and resets the
Level-Match memory); State test 26.

**The per-slot Level-Match gain is part of the slot, and resets with it** (2026-09-02,
ER-STATE-20). `abMatchGain[]` is the one piece of a slot that is **never serialized** — it is a
runtime cache of what the loudness matcher had settled on when that slot was last left, restored on
the way in by `abSwitchTo`'s closing `engine.injectMatchGainDb (abMatchGain[slot])`. Because it is
absent from the format, nothing on the restore path ever overwrote it, so it survived every restore
on a reused instance and handed the NEW project's matcher the OLD project's figure. Measured on the
real restore paths: the first switch injected the previous project's remembered value verbatim —
−1.040 dB for slot B, −2.438 dB for slot A, against the 0 dB a fresh instance injects. It is now
reset alongside the slot itself in **both** places a slot is reset: the decode's defaults (the two
paths that carry no `AB` node) and `readSlot` (per slot, so an `AB` node that exists but carries no
usable payload is covered too — that path never took the defaults, and with `active` = 1
it is the only one that exposes slot A, since the first switch overwrites the slot it leaves before
reading the one it enters). 0 dB is not a chosen sentinel but the member's initialiser, which is
what makes a reused instance behave exactly like a fresh one. **Not a format change**, and nothing a
valid session carries is affected: there is no remembered match in any file, so a valid slot's own
sound, name, baseline and identity restore exactly as before and the matcher re-measures as it
always has. The *audible* consequence of the stale injection was measured separately in round 9 and
found inert (`--legacy-match-probe`: `setParameters` re-targets the smoother from the live
measurement every block); what was wrong, and is fixed here, is the state. Source:
src/PluginProcessor.cpp (`adoptRestoreTail`, which resets it for the whole set on every restore since D-2; `readSlot`); State test 31.

**Both slots get that same answer.** Slot B used to be seeded from a *copy of slot A* instead. On the
path that runs every time — construction, where both slots are invalid — the two are
indistinguishable, since slot A has just been seeded from the same live state. They diverged only
when slot A was valid and slot B was not, i.e. an `AB` node whose `slotBParams` alone was missing or
unparsable: slot B came back as a **duplicate of slot A** rather than as the state just restored, and
a later save wrote that duplicate out. Source: src/PluginProcessor.cpp:1409-1482
(`readSlot`), :904-932 (`abEnsureInit`); src/PluginProcessor.h:188-201 (`StateSet::isValid`).

An empty `slotABase` / `slotBBase` means **"no baseline was recorded"**, which is *not* the same as
"modified". Only a pre-0.6.4 slot can produce it — every in-memory producer fills it — and
`PresetManager::setMeta` resolves it the way `adoptRestoredState` resolves an absent root
`presetBaseline`: the state being applied becomes its own **clean** baseline. A literal `""` would
compare unequal to every possible `soundSig()`, so such a slot would read as permanently edited and
the top bar would render a bare ` *` — a modified-marker against a preset the slot does not have
(its name is empty by the same rule). Source: src/PresetManager.h:127-156 (`setMeta`);
src/PresetManager.cpp:638-650 (`adoptRestoredState`, the root-side rule).

**◊** Pre-0.6.4 sessions stored params-only under `slotA`/`slotB`; `readSlot` migrates them.
Evidence [Verified]: src/PluginProcessor.cpp:1456-1457 (the legacy-key fallback inside `readSlot`, :991-1078);
the per-slot identity is written at :831 / :835 and read at :918.

## Legacy root formats (read-only compatibility)

| Format | Detection | Handling |
|---|---|---|
| v0.2 bare APVTS tree | `xml->hasTagName(apvtsStateType)` | repair on a private copy → `apvts.replaceState` → `reassertParameters` (`applySoundTree`) |

Source: src/PluginProcessor.cpp:1510-1534.

## Notes

- Exact "Introduced" versions are **Unverified** where marked: no git tags exist. The 0.8.4
  introduction of `ANAMORPH_INTERNAL` is Partially Verified from README + the migration code.
- No serialization field has been **removed**. The only structural change (0.8.4) *added* a
  child and *moved* fields out of APVTS, with a read-migration — the model future changes must
  follow.
