# State-Serialization & Parameter-Compatibility Regression Harness (v0.8.13 cycle)

Work record for the first v0.8.13 task from `worklogs/POST_v0.8.12_AUDIT_AND_ROADMAP.md`
(roadmap Phase 1, item #1): an automated safety net for the COMPATIBILITY policy family,
closing the audit's largest policy-vs-automation gap (before this harness the only automated
state check was the A/B-index clamp guard in `tests/dsp_tests.cpp`).

**Scope guard:** validation infrastructure only. No parameter IDs, serialization fields, DSP
behaviour, or user-visible behaviour changed. No version bump / CHANGELOG entry — both are
release-prep steps (`RELEASE_PROCESS.md` step 1/2), and `CHANGELOG_POLICY.md` scopes the
changelog to user-visible changes; the v0.8.13 release record will be written at release time.

---

## 1. Serialization architecture (as investigated, at `15c4159`)

### 1.1 Host chunk format

`getStateInformation` (src/PluginProcessor.cpp:512-533) writes one `AnamorphRoot` ValueTree,
XML-serialized and framed by JUCE's `copyXmlToBinary`:

| Node / field | Content |
|---|---|
| `AnamorphRoot@presetName` / `@presetBaseline` | live preset name + clean-signature baseline (dirty-star) |
| `ANAMORPH` child | full APVTS tree — one `PARAM` node per parameter: `id`, `value` (denormalised), `raw` (exact normalised `getValue()`, post-0.8.7, additive) |
| `ANAMORPH_INTERNAL` child | host-hidden Settings (`int_oversample`, `int_uiScale`, `int_scopePersist`, `int_metersOn`, `int_tooltipsOn`, `int_uiAnimations`) |
| `AB` child | `active` (clamped on read via `anamorph::clampAbSlotIndex`), per-slot `slotAParams`/`slotAName`/`slotABase` + B equivalents (params as nested XML strings) |

### 1.2 Versioning mechanism

There is **no schema version field**. Compatibility is structural: readers probe for fields and
fall back (`hasProperty` / `getChildWithName` / `hasTagName`), so the schema is
forward-tolerant (unknown fields ignored) and backward-compatible by explicit legacy branches.
Parameter identity is carried by `ParameterID { id, kVersion }` with `kVersion = 1`
(src/PluginParameters.cpp), bumped only on a deliberate parameter-set change.

### 1.3 Migration paths (all in `setStateInformation`, src/PluginProcessor.cpp:535-613)

| Legacy format | Detection | Handling |
|---|---|---|
| v0.2 bare APVTS | root tag == `ANAMORPH` | `replaceState` + `reassertParameters` (no InternalState, no A/B) |
| pre-0.6.4 A/B slots | `AB@slotA`/`@slotB` (params-only) | slot params parsed; slot name/baseline keep their pre-restore values (the old format carried no per-slot meta) |
| pre-0.8.4 Settings | no `ANAMORPH_INTERNAL` child | `InternalState::migrateFromLegacyApvts` maps legacy APVTS `PARAM` nodes (0-based choice index → 1-based combo ID) |
| pre-0.8.7 no `raw` | `PARAM` lacks `@raw` | `reassertParameters` falls back to `convertTo0to1(@value)` |
| corrupt/forward `AB@active` | out of `[0,1]` | `clampAbSlotIndex` |
| missing `presetBaseline` | `hasProperty` probe | `adoptRestoredState` (restored state = clean baseline) |

### 1.4 Related persistence surfaces

* **User presets** (`PresetManager::saveUser`): plain `apvts.copyState()` XML in
  `<presetdir>/<name>.anamorph` — saved **without freshly stamped `raw` attributes** (stale
  `@raw` from an earlier session restore can persist in the copied tree, but the load path
  ignores it); reload applies non-excluded sound params from `@value` (missing params →
  defaults). Exclusions: `pid::viewParams` (Bypass),
  `mbSolo`, `advancedMode`; load resets `mbSolo` to default.
* **A/B + undo**: in-memory `StateSet` (params-with-raw + preset meta); only the two slots +
  active index are serialized. Undo stacks are deliberately NOT persisted (cleared on restore).
* **View-param preservation**: `applyStatePreservingView` keeps live Bypass across A/B, undo
  and preset applies (SESSION_COMPATIBILITY_POLICY rule 5); a host session restore, by
  contrast, restores Bypass from the chunk.

### 1.5 Coverage before this harness

Automated: `testAbActiveClampOnCorruptState` (dsp_tests, dependency-free clamp guard) +
pluginval's generic state-restoration fuzzing. **Not** automated: round-trip equality,
registry stability, all three legacy paths, preset reload, exclusion rules, InternalState
persistence — previously human checklist items in `RELEASE_COMPATIBILITY_CHECKLIST.md`.

---

## 2. Harness design

### 2.1 Test target

`AnamorphStateTests` (CMakeLists.txt, inside `ANAMORPH_BUILD_TESTS`): a `juce_add_console_app`
that compiles `tests/state_tests.cpp` **plus the real plugin sources** (processor, parameters,
presets, editor + GUI translation units). Rationale: the tests must exercise the production
`AnamorphAudioProcessor` code paths; JUCE's shared-code plugin target links its modules
`PRIVATE` (per JUCE's design, to keep module objects out of the format targets), so reusing it
would require restructuring the shipped build — compiling the sources into the test target is
the same pattern the `AnamorphDSP` INTERFACE lib already uses. The editor/GUI files are
compiled only because `createEditor()` references them; **no editor is ever instantiated** —
the suite is headless (`ScopedJuceInitialiser_GUI` provides the message-thread context only).

### 2.2 Tests (tests/state_tests.cpp)

| # | Test | Guards |
|---|---|---|
| 1 | `testSerializedSchemaShape` | every SERIALIZATION_REGISTRY.md field present (schema tripwire) |
| 2 | `testParameterRegistrySnapshot` | IDs/names/order/flags/steps exact + range mapping probed at 5 normalised points vs `tests/fixtures/parameter_registry.snapshot` |
| 3 | `testStateRoundTripExact` | save→load→save byte-identical; per-param raw bit-exact; InternalState/AB/preset-meta reproduce; undo cleared |
| 4 | `testLegacyV02BareApvts` | v0.2 read path (fixture) |
| 5 | `testLegacyPre064AbSlots` | pre-0.6.4 slots + Settings migration + modernisation-on-resave (fixture) |
| 6 | `testLegacyPre084InternalMigration` | pre-0.8.4 view-param migration (fixture) |
| 7 | `testCorruptAndForeignState` | garbage/truncated blob, foreign root, `active` clamp e2e, unknown future fields, corrupt slot XML |
| 8 | `testPresetSaveReloadRoundTrip` | user preset save/reload equality + exclusion rules + `loadFile` + factory path (self-cleaning: deletes its test preset) |
| 9 | `testAbAndViewParamPreservation` | slot contents across round-trip; Bypass preserved on A/B apply but restored by host restore |

Registry snapshot comparison is **numerically tolerant** (1e-4 relative) for the numeric
fields only (the five range-mapping probes, the default, the interval) — `NormalisableRange`
lambdas go through `std::pow`/`std::log`, which differ by ULPs across the three CI platforms'
libms, while one snapshot fixture is shared by all three. Everything else (IDs, names,
ordering, flags, step texts, counts) compares exactly.

### 2.3 Fixtures (`tests/fixtures/`)

* `parameter_registry.snapshot` — generated by `AnamorphStateTests --write-snapshot`.
  **Regenerating it is the documented act of intentionally changing the parameter surface**
  and requires the PARAMETER_COMPATIBILITY_POLICY process (ADR + `PARAMETER_REGISTRY.md`).
* `legacy_v0_2_bare_apvts.xml`, `legacy_pre_0_6_4_ab_slots.xml`,
  `legacy_pre_0_8_4_view_params.xml` — hand-modelled period-correct session XML (frozen; the
  binary chunk framing is applied at test runtime via JUCE's own `copyXmlToBinary`, reached
  through a protected-access helper subclass). These are **models** of the legacy formats
  reconstructed from the read paths + SERIALIZATION_REGISTRY.md, not captured field blobs —
  see Remaining gaps.

### 2.4 CI integration

`scripts/run-tests.sh` now runs both binaries fail-closed (missing binary = gate failure);
the Windows job's inline step runs both `.exe`s with explicit exit-code propagation. Step
`id: tests` is unchanged, so the fail-closed artifact-upload gating is untouched. The suite
is deterministic (fixed values, no randomness, no hardware/GUI dependency).

---

## 3. Files changed

* `tests/state_tests.cpp` — NEW: the 9-test suite.
* `tests/fixtures/` — NEW: registry snapshot + 3 legacy session fixtures.
* `CMakeLists.txt` — NEW `AnamorphStateTests` console target (test block only; shipped
  targets untouched).
* `scripts/run-tests.sh`, `.github/workflows/build.yml` — both suites in the headless gate.
* Docs: `docs/HANDOVER.md`, `docs/DOCUMENTATION_COVERAGE.md`, `docs/REPOSITORY_MAP.md`,
  `docs/policies/TESTING_POLICY.md`, `docs/procedures/TESTING.md`,
  `docs/procedures/RELEASE_COMPATIBILITY_CHECKLIST.md` (automation annotations).

## 4. Validation

* **State suite: 9 tests, 774 checks, 0 failures** (Linux Release, truly headless — no X
  display required; `ScopedJuceInitialiser_GUI` provides only the message-thread context).
* **DSP suite unchanged and green** (140 checks) — run together via `scripts/run-tests.sh`.
* Full Release build (VST3 + Standalone + both test targets) green.
* **No new warnings.** `tests/state_tests.cpp` compiles clean (float equality uses
  `juce::exactlyEqual`, avoiding `-Wfloat-equal`). Compiling the production sources into the
  test target reproduces the SAME pre-existing baseline warnings the plugin build already
  emits (verified by recompiling the plugin TUs side-by-side): `ScopeBuffer.h:76`
  `-Wsign-conversion`, `PluginProcessor.cpp:545` `-Wshadow`, the JUCE
  `juce_audio_processors_headless/.../juce_AudioProcessor.h:292` hidden-virtual note, and
  `dsp_tests.cpp:161/:1032` `-Wfloat-equal` in the existing DSP test target. All pre-date this
  change and are left untouched ("do not expand into unrelated cleanup").
* **Two initial test-expectation corrections** surfaced real contracts (test bugs, not product
  bugs — both now encoded in the tests): (1) legacy pre-0.6.4 slots keep the slot's
  pre-restore preset meta (a fresh instance's "Default"), since the old format carried no
  per-slot name; (2) the preset path's contract is **snap-equivalence**, not raw-exactness —
  `.anamorph` files are read from the denormalised `@value` only (no raw stamping on save; any
  `@raw` present in the tree is ignored on load), so a mid-step discrete raw reloads at its
  snapped step; raw-exactness is the host-session path's contract (proven byte-identical in
  the round-trip test).
* **Registry-tripwire proof**: mutating one snapshot line (`name=Input Channel` → renamed)
  makes the suite exit 1 with a precise line diff + the policy pointer; regenerating via
  `--write-snapshot` restores green — the comparator is live, not vacuous.
* **Pre-commit adversarial verification** (3 independent lenses: citation accuracy, test
  quality, policy/scope — project practice) confirmed no BLOCKER and corrected this change
  before commit: the missed `CI_CD.md` pipeline-step sync (the one MAJOR), README /
  RELEASE_HARDENING_PLAN / DEVELOPMENT.md / CMake-option-string suite mentions, three
  overstated wordings fixed above (§1.3 legacy-slot meta, §1.4 preset `raw` claim, snapshot
  tolerance scope), a stale `run-tests.sh` line cite, the HANDOVER snapshot/mechanism
  wordings, and four test hardenings: the cleared-on-restore assertion now has REAL undo
  history as its precondition, the round-trip blob now carries genuinely differing A/B slot
  payloads (asserted), the corrupt-slot check text no longer overstates what it proves, and
  the preset test parks/restores a real user file of the harness name.

## 4b. Architecture-Review-Gate note (Build System)

`ARCHITECTURE_REVIEW_GATE.md` lists **Build System change (CMake structure)** as a gated item.
This change adds one additive console-app target inside the existing `if(ANAMORPH_BUILD_TESTS)`
block; the shipped `Anamorph` target block is byte-identical, and nothing links against a
console app, so no shipped-artifact property can change. Flagged here and in the PR for
maintainer review per the gate procedure — a green build does not self-clear a gated item.

## 5. Remaining gaps (honest coverage statement)

* **Legacy fixtures are reconstructions**, not field captures. A session saved by a real
  v0.2/v0.6.x/v0.8.3 binary would be the gold standard; if one surfaces, freeze its blob under
  `tests/fixtures/` and load it in a new test.
* **Cross-version round-trip** (save in vN−1 binary, load in vN) still needs the manual
  release-checklist step — the harness only proves the CURRENT binary reads the modelled
  legacy formats.
* **Automation playback / host matrix** remain manual (Level 5 / KI-004 territory).
* **Factory-preset sound stability** is guarded structurally (loadable, clean), not
  audibly — golden-audio rendering stays a 0.9.x roadmap item.
* Undo stacks are not persisted (by design, ADR-0008 scope) — documented, not tested.

## 6. How future parameter/schema changes interact with the harness

1. An **unintentional** change fails test 2 (registry) or tests 1/3 (schema/round-trip) in CI
   on all three platforms.
2. An **intentional** change follows the policy: ADR + Architecture Review + registry/ledger
   update (`PARAMETER_REGISTRY.md` / `SERIALIZATION_REGISTRY.md`) + migration path with a NEW
   legacy fixture guarding the old format, then `AnamorphStateTests --write-snapshot` to
   re-freeze the snapshot. The snapshot diff in the PR is the reviewable record.
