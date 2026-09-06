# Legacy pre-0.6.4 A/B slot path — architecture review and migration decision (v0.9.7 cycle)

Work record for the follow-up task that ADR-0036 §22 and the D-2 round-15 worklog left open:
the one surviving **live-read baseline** in the program-state model, reached only by a
pre-0.6.4 A/B slot payload. The decision it produced is **ADR-0037**
(`docs/architecture/design-decisions/ADR-0037-legacy-ab-slot-baseline-at-the-boundary.md`);
this file is the evidence trail and the implementation chronology. The two are meant to be read
independently: the ADR states the decision and why, this worklog shows the measurements the
decision rests on.

**Scope guard.** No parameter ID, no serialization field, no threading path, no DSP order and no
reported latency changes. Every legacy *read* path stays (`SESSION_COMPATIBILITY_POLICY.md`
rule 3). What changes is where a legacy slot's clean baseline is decided — at the decode
boundary, from the slot's own bytes — and, so that the decision is exact by construction, how
the session-shaped apply path derives the value it asserts.

---

## 1. What was investigated, and where the legacy path actually is

All references are to the tree at `737d3c1` (the `claude/anamorph-ci-workflow-m4rohp` head,
which carries no `src/` or `tests/` change over `origin/main`).

### 1.1 The format

A session written before 0.6.4 stores its A/B slots as **parameter values only**: the `AB` node
carries `slotA` / `slotB` attributes holding an APVTS tree as an XML string, with no per-slot
name, baseline or (since 0.9.2) identity. `SERIALIZATION_REGISTRY.md` §`AB` child records the
modern trio (`slotAParams` / `slotAName` / `slotABase`, introduced 0.6.4, "#6") and footnote ◊:
*"Pre-0.6.4 sessions stored params-only under `slotA`/`slotB`; `readSlot` migrates them."* The
frozen fixture `tests/fixtures/legacy_pre_0_6_4_ab_slots.xml` is that shape: each slot holds a
single `width` PARAM, `value` only, no `raw`. It is a **reconstruction** from the read path, not
a field capture (`worklogs/STATE_HARNESS_v0.8.13.md` §5) — no session written by a real
pre-0.6.4 binary exists in the repository.

### 1.2 The read path (the only legacy-specific code)

`AnamorphAudioProcessor::decodeRestore`, `src/PluginProcessor.cpp:1609-1682` — the `readSlot`
lambda. It resets the slot, then adopts the params from `slotAParams`, **else from the legacy key**:

```
1656:   else if (ab.hasProperty (legacyKey)) // pre-0.6.4 slots: params only
1657:       adoptIfAnamorph (ab.getProperty (legacyKey).toString());
```

Everything after that line is the modern path: `dst.name` and `dst.baseline` are read from the
modern keys (absent → `""`), `dst.selection` from the identity trio (absent → `unknown`). The
result is a canonical `StateSet { params, name="", baseline="", selection=unknown }` carried in
`RestoreDecode::abSlot[]` (`src/PluginProcessor.h:438`) and assigned to `abSlot[]` by
`adoptRestoreTail` (`:1240-1343`) like every other slot.

So the legacy *decode* is two lines and produces the canonical struct. The legacy **semantic**
is what those two lines leave behind: a `StateSet` whose `baseline` is **empty**.

### 1.3 Where the empty baseline goes

* `abSwitchToAdopted` (`:1076`) → `abApplySlot` (`:1059`) → `applyStateSet (abSlot[slot])`
  (`:462-471`):
  ```
  applyStatePreservingView (s.params);              // replaceState + reassertParameters
  presets.setMeta (s.name, s.baseline, s.selection);
  ```
  with the comment *"ORDER IS LOAD-BEARING: parameters first, metadata second. setMeta resolves
  an EMPTY baseline by calling soundSig(), which reads the LIVE apvts."*
* `PresetManager::setMeta`, `src/PresetManager.h:155-161`:
  ```
  sigAtLoad = baselineSig.isNotEmpty() ? baselineSig : soundSig();
  ```
  — **the live read.** Its 30-line comment block (`:127-153`) states the precondition ("the
  parameters this metadata describes must ALREADY be applied"), names the pre-0.6.4 slot as the
  only in-tree producer of an empty baseline, and records that the host-restore path stopped
  reaching this fallback in round 15.
* `undo` / `redo` (`:990`, `:1011`) also go through `applyStateSet`, but their `StateSet`s come
  from `currentStateSet()` (`:455-458`), whose baseline is `presets.baseline()` — never empty
  after construction (`src/PresetManager.cpp:71`). Unreachable for them, by the round-15 audit.

### 1.4 The canonical baseline rule this bypasses

Since D-2 rounds 9, 10 and 15 (ADR-0036 §17, §18, §22) every other baseline is decided **from
bytes, never from a live read**:

| site | baseline built by | ADR-0036 |
|---|---|---|
| `saveUser` | `soundSignatureForSavedTree` — the bytes written | §17 |
| `load` / `loadFile` | `soundSignatureAfterLoading` — the bytes applied | §18 |
| host restore, absent/empty `presetBaseline` | `restoredSoundSig` = `soundSignatureAfterLoading (params)` at decode (`:1578`, `:1714`), through `baselineOfRestore` (`:1365`) | §22 |
| `applyStateSet` → `setMeta ("")` | **live read**, pre-0.6.4 slot only | §22, *"recorded, not changed"* |

The round-15 record (ADR-0036 §22 last paragraph; `ENGINEERING_REVIEW_PROGRAMME.md:1741-1747`)
declined to close it because closing it means deriving the baseline from `s.params`, and that
sound is applied by `replaceState` + `reassertParameters` rather than by `applySoundTree` — so it
first requires **measuring** that `soundSignatureAfterLoading` models that path bit for bit.
Assuming the equality is what round 10 got wrong (§19). That measurement is §3 below.

### 1.5 Complete reference list

Searched with `grep -rn "0\.6\.4\|applyStateSet\|applyStatePreservingView"` over the repository:
`src/PluginProcessor.{h,cpp}`, `src/PresetManager.h`, `src/PluginEditor.cpp:1931` (a comment
about the empty name), `src/dsp/AnamorphEngine.{h,cpp}` (unrelated: the 0.6.4 duck),
`tests/state_tests.cpp` (State test 5 `:553-641`; the key-removal fixture `:975`),
`docs/architecture/SERIALIZATION_REGISTRY.md:259-261, 274, 341-356`,
`docs/architecture/STATE_SERIALIZATION.md:124-134, 158-166`,
`docs/architecture/design-decisions/ADR-0024-preset-identity.md:99`,
`ADR-0036-program-state-ownership.md:1064-1073, 1351`,
`docs/policies/SESSION_COMPATIBILITY_POLICY.md` rule 3, `COMPATIBILITY_POLICY.md`,
`docs/procedures/RELEASE_COMPATIBILITY_CHECKLIST.md:74-78`, `docs/procedures/TESTING.md`,
`CHANGELOG.md:917-923` (the 0.9.2 entry that made a legacy slot read as *No Preset*),
`docs/HANDOVER.md`, `docs/REPOSITORY_MAP.md`, `docs/DOCUMENTATION_COVERAGE.md`, and the
worklogs. No other legacy branch, version check or compatibility switch exists for this vintage.

## 2. The compatibility contract, as the repository actually states it

| Question | Answer | Evidence |
|---|---|---|
| Does the product intentionally support loading pre-0.6.4 state? | **Yes, by policy.** | `SESSION_COMPATIBILITY_POLICY.md` rule 3: *"Every legacy read path stays. The v0.2, pre-0.6.4, and pre-0.8.4 read paths must remain."* |
| Is it documented? | Yes, in four places that must agree. | `SERIALIZATION_REGISTRY.md` ◊ + §Legacy root formats; `STATE_SERIALIZATION.md` §Backward-compatibility paths *"(all must be preserved)"*; `RELEASE_COMPATIBILITY_CHECKLIST.md` item 8 (the legacy-fixture tests are the automated half of *Session reload verified*); `CHANGELOG.md` [0.9.2] |
| Required by tests? | Yes. | State test 5 (`legacy_pre_0_6_4_ab_slots.xml`): live width, active slot, Settings migration, re-save under modern keys, empty name/baseline, repeated restore, switch-into reads clean. State test 27's key-removal variants remove `slotA` too. |
| Required by a release / import workflow? | The release checklist cites the fixture tests; no import tool exists. | `RELEASE_COMPATIBILITY_CHECKLIST.md:74-78` |
| Internal tooling? | None consumes the format. | grep |
| Still generated anywhere? | **No.** Every writer emits `slotAParams`/`slotAName`/`slotABase` (+ identity) — `writeState`, `:1394-1431`. Re-saving a legacy session modernises it (State test 5). | code |
| Has the product shipped? | **No annotated tag exists** (`git tag` empty; `HANDOVER.md`: v0.9.7 is the first tag in preparation). Builds reach testers as per-push CI artifacts for internal/beta testing (`COMMERCIAL_STATUS.md` §2). | repo |
| Could a pre-0.6.4 session exist outside the repository? | Unknowable from the repository: the format predates its visible history (first commit `86b4273`, 2026-07-24; `CHANGELOG.md` records 0.6.x as *"2026-06 (reconstructed)"*). No such session is checked in. | git |
| Can support be dropped? | Only through the `COMPATIBILITY_POLICY.md` exception: ADR + migration plan + release checklist + Architecture Review. A read-path removal is a *Serialization Registry change* (gate item) and an AI-agent hard stop. | policies |

**Conclusion of §2.** Support is a written product rule, not an accident of history; nothing
generates the format any more; the cost of *reading* it is two lines. The question this task is
really about is not the read path but the **internal semantic** the read path leaves behind.

## 3. Measurement: does `soundSignatureAfterLoading` model the slot apply path?

**Method.** A temporary probe (`--legacy-slot-baseline-probe`, source kept in this session's
scratch area and reproduced as the permanent State test 65 in §7) drove the **real** path for
every case: build an `AnamorphRoot` blob whose `AB` node carries the slot under the legacy
`slotA` key, `setStateInformation`, `abSwitchTo (0)`; then compare three quantities:

* `live` = `PresetManager::soundSignatureFor (apvts)` — what `isDirty()` compares against;
* `base` = `presets.baseline()` — what `setMeta`'s live read stored;
* `pred` = `PresetManager::soundSignatureAfterLoading (apvts, slotTree)` — the bytes-only
  prediction, computed from the tree **as parsed from the slot's XML text**, i.e. exactly what
  `readSlot` holds.

Per case it also recorded how many store/report passes (`normalisedAsRendered`, "nAR") separate
the resolved normalised value from the value the parameter finally reports. 33 preset-carried
parameters; 86 062 restore-and-switch cycles.

| shape | n | `live != pred` | `base != live` | passes k=1 | k=2 | k=3 | other |
|---|---|---|---|---|---|---|---|
| A1 value-only, full slot, start far from target | 23 133 | **0** | 0 | 23 079 | 48 | 6 | 0 |
| A2 value-only, full slot, start equal to target | 23 133 | **0** | 0 | 23 091 | 37 | 5 | 0 |
| A3 value-only, full slot, start 1 ulp from target | 23 133 | **0** | 0 | 23 081 | 37 | 5 | 10 (k=0) |
| B value + `raw` (modern slot), start far | 6 633 | **0** | 0 | 3 816 | 0 | 0 | 2 817 (`raw` kept verbatim by `RawChoice`/`RawBool`, off-grid; the signature snaps it) |
| C value-only, ONLY this parameter present (the fixture's shape) | 3 333 | **0** | 0 | 3 328 | 3 | 2 | 0 |
| D malformed text (`abc`, `nan`, `inf`, `""`, `1e39`, `0x10`) and out-of-range values | 330 | **0** | 0 | 330 | 0 | 0 | 0 |
| E the ROOT restore (round 15's `restoredSoundSig`), `raw`-bearing session, no `presetBaseline` | 6 633 | **0** | 0 | — | — | — | — |

**Result 1 — at the signature's own resolution the prediction is exact in every measured case.**
0 of 86 062 five-decimal signatures differ, for the pre-0.6.4 shape, the modern shape, the
partial shape, malformed input and the root restore.

**Result 2 — it is not exact by construction.** For the four log-mapped frequency ranges
(`mbFreqLow/Mid/High`, `monoMakerFreq`; `logFreqRange` / `logFreqRangeCentred`,
`src/PluginParameters.cpp:107-131`) the value the parameter ends at is `nAR^k (n)` with
**k ∈ {1, 2, 3}, and k = 0 in the 1-ulp case**, where the prediction models exactly k = 1. The
differences are ≤ 6·10⁻⁸ in normalised units — inaudible, and far below the 10⁻⁵ signature
bucket — but a value that sits within that distance of a five-decimal boundary would print
differently. Estimated rate ≈ (k≠1 fraction ≈ 0.25 %) × (boundary-crossing fraction ≈ 0.15 %)
≈ 4·10⁻⁶ per apply of a frequency parameter; consistent with observing 0 in 86 062, and not
a proof.

**Result 3 — the mechanism, isolated** (probe leg F: `replaceState` alone, then an emulation
of `reassertParameters`, 2 001 values per range):

| range | `replaceState` alone: k=1 | tree `value` REWRITTEN by `replaceState` | after reassert: k=1 / k=2 / k=3 |
|---|---|---|---|
| mbFreqLow | 2001 / 2001 | 316 | 1974 / 23 / 4 |
| mbFreqMid | 2001 / 2001 | 316 | 1974 / 23 / 4 |
| mbFreqHigh | 2001 / 2001 | 316 | 1974 / 23 / 4 |
| monoMakerFreq | 2001 / 2001 | 1189 | 1963 / 32 / 6 |

`AudioProcessorValueTreeState::replaceState` is exactly one pass — and then it calls
`flushParameterValuesToValueTree()` (pinned JUCE, `juce_AudioProcessorValueTreeState.cpp:439`),
which writes each adapter's `unnormalisedValue` — `denormalise (parameter.getValue())`, i.e. the
parameter's **rendered** value `F(nAR(n))`, not the text's `F(n)` — back into the tree wherever
it is not `approximatelyEqual`. That tree is the same object the caller still holds:
`apvts.replaceState (copy)` assigns `state = copy`, and `ValueTree` assignment shares the node.
`reassertParameters` then reads `copy`'s rewritten `value`, normalises it again, and asserts
`nAR²(n)` — one pass more than the bytes say. `repairSerializedValues` has the same shape one
step earlier: it writes the repaired `value` back as `F(norm)` text, which `reassertParameters`
re-normalises to `nAR(norm)`. Both are the same defect: **the value asserted is re-derived from a
denormalised text that was itself derived from a normalised value**, instead of being the
resolved normalised value. The preset path (`PresetManager::applySoundTree`,
`src/PresetManager.cpp:307-322`) has neither step — it writes the resolved value once — which is
why round 11's 20 000 × 33 + 3 000 measurement was exact there by construction.

**Result 4 — this already applies to D-2 round 15.** `restoredSoundSig` (`:1578`, `:1714`)
is the same predictor against the same `replaceState` + `reassertParameters` path
(`applySoundTree`, `:845-862`). Leg E measured 0 of 6 633 mismatches, by the same
non-constructive margin. State test 60's oracle is a control instance restoring the same bytes,
so it is blind to this by symmetry. Cosmetic if it ever fired (a modified-marker on a freshly
restored session at one rare frequency value, self-correcting on the next save), recorded here
so it is a measured fact rather than an assumption.

## 4. The engineering cost of retaining the legacy semantic

Concrete, not "it is old":

1. **A second baseline rule.** Every producer but one decides the baseline from bytes; `setMeta`
   carries a live-read rule for the one that does not, with a precondition ("apply first") that
   the type system cannot enforce and a comment block of 30 lines explaining why the call order
   in `applyStateSet` is load-bearing.
2. **The same defect class the programme removed three times.** ADR-0036 §18 (KI-029) treated a
   read-back window that host automation can land in as a genuine defect and fixed it. The slot
   path has that window: an audio-thread automation write between `applyStatePreservingView`
   and `setMeta` is absorbed into the clean baseline. The window is microseconds and the trigger
   is legacy-only, which is why round 15 recorded rather than fixed it — but it is the same class,
   and the standard the programme set (§19: *one equivalence, no tolerance, no live read*) is
   not met by it.
3. **An observable difference in the saved file.** A re-saved legacy session writes
   `slotABase=""` today (State test 5 asserts it), so the *modernised* session still carries the
   legacy semantic forward — the empty baseline survives every re-save until the slot is switched
   into and away from. The migration is not finished at the boundary; it is deferred to a user
   action.
4. **A trap for any future producer.** `StateSet::baseline` may legally be empty, and the only
   thing that makes that safe is a call-order convention in one function. A future `StateSet`
   producer that adopts metadata before applying parameters gets a silently wrong star, as the
   `applyStateSet` comment itself warns.

What retaining it does **not** cost: threads, locks, generations or publication order. The path is
message-thread only and inside the D-2 model; ADR-0036 §22 was right that it is not the
host-thread class.

## 5. Feasibility of migrating at the boundary

The canonical representation for "a slot with no recorded baseline" already exists — it is what
`baselineOfRestore` gives the ROOT for the same situation: *the signature of the sound these
bytes install*, from the bytes, at decode time, on whichever thread decodes.
`soundSignatureAfterLoading` is thread-neutral (a pure function of the tree and the parameter
ranges; already evaluated on the host thread at `:1578`). So `readSlot` can fill
`dst.baseline` the moment it has `dst.params`, and the slot then enters the model as a fully
modern `StateSet`. Name stays `""` and identity `unknown` — those are already canonical values
("No Preset", ADR-0024), not legacy ones. **No information is lost**, because the legacy payload
carries nothing but the parameters.

The one thing feasibility depends on is §3: the prediction must be what the slot apply path
actually produces. It is, at the signature's resolution, in every measured case — and it can be
made so **by construction** with a change that is small and local: `reassertParameters` must
assert the *resolved normalised value* rather than re-derive it from a denormalised text, and the
resolver must be one function shared with `repairSerializedValues` and with the predictor. That
change also makes round 15's root-restore baseline exact by construction (§3 result 4).

## 6. Options

### A — Retain the live-read fallback, harden the contract

Keep `setMeta`'s fallback; add an assertion that the caller applied first; document the
automation window and its rate. Benefits: zero source change in the apply path. Costs: items
1–4 of §4 stay; the `slotABase=""` re-save stays; the programme's own standard stays unmet at one
site; the round-15 root-restore prediction stays non-constructive (§3 result 4) and nobody would
have measured it. Rejected: it preserves a special case whose only justification was an
unmeasured equivalence, and the equivalence has now been measured.

### B — Migrate at the boundary into the canonical representation (chosen)

`readSlot` derives an absent or empty slot baseline from the slot's own bytes with the
session-shaped predictor; `setMeta` loses the live read (an empty baseline becomes a caller
error, asserted); `applyStateSet`'s order stops being load-bearing; and the session-shaped apply
path asserts the resolved value once, through one resolver shared with the repair and the
predictor, so prediction and live value agree by construction for every parameter kind and every
payload shape. Legacy knowledge stays exactly where it is — two lines in `readSlot` — and the
struct that leaves the decode is indistinguishable from a modern slot's. Costs: one new resolver
function; `reassertParameters` and `repairSerializedValues` read it; two call sites pass the
original tree; State test 5's `slotABase=""` assertion inverts (the re-save now writes the
derived baseline — the visible sign the migration is complete at the boundary); a new State test.
Risk: the apply-path change touches the D-2 install path — bounded by the measurement in §3 and
by the regression suite (State tests 37–64, the TSan probes). No format, field, thread or
ordering changes.

### C — Remove pre-0.6.4 support

Delete the `legacyKey` branch, the fixture and State test 5's legacy legs; amend
`SESSION_COMPATIBILITY_POLICY.md` rule 3, the registry, `STATE_SERIALIZATION.md`, the release
checklist and the 0.9.2 changelog statement; ADR + Architecture Review (gate item, hard stop).
What stops loading: a pre-0.6.4 session's A/B slots (its main sound still loads; the slots fall
back to "lazily initialised from current", so the slot sounds are **lost** silently). What can be
deleted: ~4 lines of code, one fixture, ~40 lines of test. What it does **not** remove: the
empty-baseline semantic — a truncated or hand-edited modern blob with `slotABase=""` reaches the
same `setMeta` fallback, so C alone leaves the live read in place. Rejected on both counts: it
breaks a documented contract for no architectural gain, and the actual problem survives it. Being
unreleased makes the break *possible*, not *useful*.

## 7. Decision and implementation plan

**Decision: Option B** — recorded as ADR-0037 (Accepted under the maintainer's brief for this
task; the Architecture Review Gate is not entered because no gated item changes, which the ADR
states and the diff shows). Compatibility is **migrated**: the pre-0.6.4 format is still read, and
what it decodes into is the canonical current representation.

Implementation, in order, each step validated before the next:

1. **One resolver.** `anamorph::sessionNormalisedValue (rp, node)` in `src/PluginParameters.h`
   (beside `normalisedAsRendered`, the file that already owns "the value as the plug-in renders
   and stores it"): `raw` if usable (clamped 0..1), else `value` if usable (`convertTo0to1`), else
   the default — the precedence `repairSerializedValues` and `reassertParameters` already apply,
   written once; returns whether the input was repaired.
2. **`reassertParameters` asserts the resolved value**, taken from the tree the caller passed
   (the original, un-repaired one — `applySoundTree` and `applyStatePreservingView` hand JUCE the
   repaired copy and this function the original), so nothing JUCE writes back can change what is
   asserted; `repairSerializedValues` computes its repaired value through the same resolver.
3. **`PresetManager::soundSignatureAfterRestoring (apvts, tree)`** — `signatureAfterApplying`
   with the session resolver; `soundSignatureAfterLoading` keeps the preset (file) resolver. Both
   `restoredSoundSig` fills switch to it.
4. **`readSlot`** fills an absent/empty baseline of a valid slot from its bytes with it.
5. **`setMeta`** stores the baseline it is given; empty is asserted against. The comment block
   is rewritten to the new invariant. `applyStateSet`'s order comment is replaced by the
   invariant that every `StateSet` carries a non-empty baseline.
6. **Tests.** State test 5's `slotABase` assertion inverts and gains the round trip; new State
   test 65 covers the legacy payload's decode (baseline non-empty, equal to the predictor, equal
   to the live signature after switching in, repeated restore, partial and malformed payloads),
   the modern-shape control, and a bounded equivalence sweep over the four log-mapped ranges
   that asserts `getValue()` equals **one** pass of the resolved value bit for bit (the
   assertion that fails on the pre-change tree at the rate §3 measured).
7. **Docs** per the trigger map: ADR-0037, `ADR_INDEX.md`, ADR-0036 §22 (a pointer, append-only),
   `SERIALIZATION_REGISTRY.md`, `STATE_SERIALIZATION.md`, `SESSION_COMPATIBILITY_POLICY.md` §6
   note, `PresetManager.h`/`PluginProcessor.cpp` comments, `TESTING.md`,
   `DOCUMENTATION_COVERAGE.md`, `CHANGELOG.md` [0.9.7] Fixed, this worklog, the round-15 residual
   row in `ENGINEERING_REVIEW_PROGRAMME.md`.

Validation plan: State test 65 and 5 focused; full state suite, DSP suite; the four D-2 TSan
probes; `check-realtime.py`; `check-docs.py`, `check-citations.py`; `preflight.sh`; the CI
matrix on the final head; a mutation pass (restore the live read; restore `copy` as the tree
`reassertParameters` reads; drop the boundary derivation) — each must fail a named check.

## 8. Implementation chronology and results

Branch `claude/legacy-ab-slot-baseline-m4rohp`, on top of `737d3c1`.

1. **`b172c40` — the decision, recorded first.** This worklog (§1–§7) and ADR-0037 with its
   `ADR_INDEX.md` row; no source change.
2. **`8f152c9` — the implementation.** `anamorph::sessionNormalisedValue` and
   `usableSerializedNumber` in `src/PluginParameters.h`; `repairSerializedValues` and
   `reassertParameters` read the resolver, the file-local `readSerializedValue` in
   `PluginProcessor.cpp` is gone (the preset path keeps its own copy in `PresetManager.cpp` — its
   resolver is deliberately a different one); `applySoundTree` and `applyStatePreservingView` pass
   the caller's original tree to `reassertParameters`; `PresetManager::soundSignatureAfterRestoring`
   (declared beside `soundSignatureAfterLoading`, defined beside it); both `restoredSoundSig` fills
   use it; `readSlot` captures `this` and derives an absent/empty baseline of a valid slot;
   `setMeta` stores the baseline verbatim behind `jassert (baselineSig.isNotEmpty())`;
   `applyStateSet` gains the seam `betweenStateSetApplyAndMeta`. State test 5's `slotABase == ""`
   inverted; State test 65 added (legs a–f).
3. **`68d76b3` — the searched values.** A temporary 4 000 000-value search per log-mapped range for
   raw values at which the preset predictor (`value` = F(raw), one rendering pass further in) and
   the session predictor print different five-decimal signatures:

   | range | `nAR² ≠ nAR³` (float) | five-decimal crossings | first such raw |
   |---|---|---|---|
   | mbFreqLow | 51 866 / 4 000 000 | **192** | 0.690675139 |
   | mbFreqMid | 52 282 | **188** | 0.711325169 |
   | mbFreqHigh | 52 377 | **167** | 0.996955156 |
   | monoMakerFreq | 57 172 | **679** | 0.518795192 |

   So the two predictors are not interchangeable on `raw`-bearing trees — roughly 5·10⁻⁵ of raw
   values per range — and round 15's choice of the preset predictor for `restoredSoundSig` was a
   latent (rare, cosmetic) defect: a session saved with no `presetBaseline` restored *modified* at
   those values. State test 65 gained leg (d)'s raw-bearing slot at 0.690675139 and leg (g), the
   root restore at all four values, which the round-15 tree fails.

**The measurement, repeated on the changed tree.** State test 65 leg (f): 3 216 session-shaped
applies (root install and slot switch, value-only and `raw`-bearing, the four ranges × 201 values)
— **0** report a value other than one pass of the resolved one, **0** signature mismatches. The
§3 probe on the changed tree gives the same k = 1 for every case. Suite: **2 496 checks, 0
failures** (2 439 before this task).

**Mutation results** (each mutant applied to the committed tree, the suite rebuilt and run):

| mutant | result | killed by |
|---|---|---|
| M1a — `readSlot`'s derivation removed | **killed**, 32 checks | State test 5 ("a baseline derived from its own bytes at decode"); 65 (a), (b), (d) ×5 shapes, (e) |
| M1b — the pre-ADR-0037 behaviour: no derivation AND `setMeta` reads live | **killed**, 16 checks | 65 **(c)** "a write between apply and metadata leaves the slot DIRTY, not absorbed"; (a), (d), test 5 |
| M2 — `reassertParameters` reads the copy handed to JUCE (both sites) | **killed**, 1 check | 65 (f) "every apply reports exactly one store/report pass of the resolved value" |
| M3 — the preset predictor in the session seats | **killed**, 2 checks | 65 (d) the searched raw: "a raw-bearing slot's derived baseline is the SESSION prediction" |

No survivor; nothing to argue equivalent.

**ThreadSanitizer** (`build-tsan`, RelWithDebInfo, `-fsanitize=thread`): the four D-2 probes
`--state-thread-probe`, `--state-prepare-race-probe`, `--reprepare-race-probe`,
`--d2-stress-probe` — exit 0, **0 ThreadSanitizer warnings** each; the full state suite under
TSan — 2 496 / 0, 0 warnings. DSP suite 396 / 0. `check-realtime.py`: 47 files, 0 violations.

**Hidden-dependency sweep after the change.** `soundSig()` is read live in exactly two places:
the `PresetManager` constructor (`src/PresetManager.cpp:71`, before any operation an edit could
follow) and `isDirty()` itself (the comparison). No `isNotEmpty() ? … : soundSig()` fallback
remains; `adoptRestoredState` is gone since round 15. Legacy identifiers (`legacyKey`, `"slotA"`,
`"slotB"`) occur only in `readSlot` and in comments — *legacy knowledge at the boundary, canonical
state internally.* `soundSignatureAfterLoading` is used by the preset load paths only (`load`,
`loadFile`); every session seat uses `soundSignatureAfterRestoring`.

**Citations.** The source edit moved every span below `repairSerializedValues`; 12 declared
re-aims and 37 plain moves were re-anchored (`check-citations.py --fix`), one by hand
(`STATE_SERIALIZATION.md`'s `applyStatePreservingView` span, whose lines were edited).

**Residuals carried forward** (all recorded in ADR-0037 §Consequences):

- The 1-ulp adjacency case: a parameter already holding a *different* denormalised float whose
  normalised report equals the resolved value skips the reassert write; live signature `nAR(n)`,
  prediction `nAR(nAR(n))`. No path in the plug-in produces such a neighbour after this change.
  Measured, not fixed: changing the write condition would touch the host-restore notification
  contract for a case with no known producer.
- `PresetManager.cpp` keeps its own `readSerializedValue` for the preset resolver; the predicate
  itself is `SerializedNumber.h`'s in both places, so the two cannot disagree about a number.
