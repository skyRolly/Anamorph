# RELEASE_COMPATIBILITY_CHECKLIST.md

Hard compatibility gate. **Every box must be checked before a release ships.** This enforces
`docs/policies/COMPATIBILITY_POLICY.md` and its subset policies. A failed item blocks the release
(or requires the COMPATIBILITY_POLICY exception: ADR + migration + Architecture Review).

## Completion record — v0.9.6 (2026-09-01)

**Six of eight boxes are checked with measured evidence. Two remain open and require a DAW.**
Every tick below cites what was run and what it produced; a box with no evidence beside it is not
ticked. The two open items are the ones this checklist's own §Notes already names as impossible to
prove headlessly.

| # | Item | v0.9.6 |
|---|---|---|
| 1 | Parameter IDs unchanged | **PASS** |
| 2 | Serialization schema verified | **PASS** |
| 3 | Presets migrated | **PASS** (audible half via the Level-5 audition) |
| 4 | Pluginval passed (both modes) | **PASS** — run here, not inferred from CI |
| 5 | Host matrix verified | **OPEN — needs a DAW** |
| 6 | Latency reporting verified | **PASS** |
| 7 | Automation playback verified | **OPEN — needs a host** |
| 8 | Session reload verified | **PASS** — real v0.9.5 field capture |

**Items 5 and 7 are not blocked on analysis; they are blocked on a host.** The maintainer's
Level-5 audition (PASSED, `LEVEL5_AUDITION.md`) exercised a DAW, and its protocol group C covers
automation of Drive/Algorithm — but that record's **per-item outcomes are NOT RECORDED**, so it
cannot be read as discharging these two. Ticking them from it would be inferring per-item results
from a verdict-level record. Either the maintainer confirms those groups were exercised, or the two
items are run on their own.

## Checklist

- [x] **Parameter IDs unchanged** — **PASS (v0.9.6, 2026-09-01).** — diff the parameter set against the previous release; no `pid::`
      ID renamed or removed. (Display-name changes are allowed; record in CHANGELOG.)
      Ref: `docs/architecture/PARAMETER_REGISTRY.md`, `docs/policies/PARAMETER_COMPATIBILITY_POLICY.md`.
      *Automated since the v0.8.13 cycle:* the registry-snapshot test in `tests/state_tests.cpp`
      (`AnamorphStateTests`, CI-blocking on all three platforms) fails on any ID/name/order/
      range/automation-flag change vs `tests/fixtures/parameter_registry.snapshot`.
- [x] **Serialization schema verified** — **PASS (v0.9.6, 2026-09-01).** — no field removed or semantically changed in
      `AnamorphRoot` / `ANAMORPH` (APVTS) / `ANAMORPH_INTERNAL` / `AB`; additions tolerate absence.
      Ref: `docs/architecture/SERIALIZATION_REGISTRY.md`, `docs/policies/SESSION_COMPATIBILITY_POLICY.md`.
      *Automated since the v0.8.13 cycle:* schema-shape + raw-exact round-trip + the three
      legacy-format fixtures in `tests/state_tests.cpp`. The cross-version step below stays manual.
- [x] **Presets migrated** — **PASS (v0.9.6, 2026-09-01).** — factory presets and a representative user `.anamorph` still load and
      sound identical. Ref: `src/PresetManager.cpp`.
      *Partially automated:* `tests/state_tests.cpp` proves save→reload structural equality +
      exclusion rules + factory loadability; "sound identical" remains a Level-5 (audition) check.
- [x] **Pluginval passed (both modes)** — **PASS (v0.9.6, 2026-09-01).** — `scripts/run-pluginval.sh <n> deterministic` **and**
      `scripts/run-pluginval.sh <n> randomise` (`--randomise` ×3) pass on the Linux gate, where
      `<n>` is `ANAMORPH_PLUGINVAL_STRICTNESS` from `.github/workflows/build.yml` — read it there
      rather than from this line, so a raise cannot leave this checklist certifying the old bar.
      Ref: `docs/procedures/TESTING.md`.
- [ ] **Host matrix verified** — **OPEN: requires a DAW.** — load in the target hosts and confirm load + automation + state.
      (Currently Unverified in-repo; this requires manual DAW testing —
      `docs/architecture/COMPATIBILITY_MATRIX.md`.)
- [x] **Latency reporting verified** — **PASS (v0.9.6, 2026-09-01).** — reported PDC matches the actual chain delay across the
      oversampling settings; OS-off reports 0. Ref: `docs/architecture/LATENCY_MODEL.md`; test
      `testBypassNullAndLatency`.
- [ ] **Automation playback verified** — **OPEN: requires a host.** — recorded automation on host-visible parameters plays back
      with unchanged meaning. Ref: `docs/policies/PARAMETER_COMPATIBILITY_POLICY.md`.
- [x] **Session reload verified** — **PASS (v0.9.6, 2026-09-01).** — save a session in the previous version, load it in the new
      version: sound, preset name, dirty-star, and both A/B slots reproduce exactly.
      Ref: `docs/architecture/STATE_SERIALIZATION.md`.
      *Partially automated:* the round-trip + legacy-fixture tests prove the CURRENT binary reads
      the modelled v0.2 / pre-0.6.4 / pre-0.8.4 formats; the true vN−1-binary → vN load remains
      this manual step (the fixtures are reconstructions, not field captures —
      `worklogs/STATE_HARNESS_v0.8.13.md` §5).

## If any box cannot be checked

Stop. Either fix the regression, or — if the change is intentional — satisfy the
`COMPATIBILITY_POLICY.md` exception: an **ADR** + a **migration plan** + **Architecture Review**
sign-off. Document the migration in `STATE_SERIALIZATION.md` / `PARAMETER_REGISTRY.md` and the
CHANGELOG.

## Notes

- The headless gate (DSP self-tests + pluginval) verifies several of these structurally
  (latency, bypass null, no-NaN), but **Host matrix**, **Automation playback**, and **Session
  reload** require manual validation — they cannot be fully proven headlessly.
- The reference precedent for a compatible surface change *with migration* is the 0.8.4 move of
  view params out of the APVTS (`InternalState::migrateFromLegacyApvts`, ADR-0010).

## Evidence for the v0.9.6 completion

Recorded 2026-09-01 against the working tree at the head of
`claude/anamorph-ci-workflow-8iu7yk`. Test counts are from that run.

1. **Parameter IDs unchanged — PASS.** `AnamorphStateTests` State test 2 (parameter registry
   snapshot) passes, and `tests/fixtures/parameter_registry.snapshot` is byte-identical to
   `origin/main` and unmodified since the v0.8.13 cycle (`git diff origin/main` empty; last touching
   commit `d6bdb13`). No ID renamed or removed.
2. **Serialization schema verified — PASS.** State tests 1 (schema shape vs
   `SERIALIZATION_REGISTRY.md`), 3 (raw-exact byte-stable round-trip) and the three legacy fixtures
   (4/5/6) pass. The cross-version half this item used to defer to is now item 8.
3. **Presets migrated — PASS.** State tests 8 (save→reload round-trip + exclusions), 10
   (factory/user identity with a shared name) and 11 (factory-preset id integrity) pass. The
   "sound identical" half is a Level-5 judgement and is covered by the v0.9.6 audition
   (`LEVEL5_AUDITION.md`, PASS).
4. **Pluginval passed (both modes) — PASS.** Run in this environment at strictness **10** — read
   from `ANAMORPH_PLUGINVAL_STRICTNESS` in `build.yml`, not from this file — against the built VST3
   under `xvfb`:
   `scripts/run-pluginval.sh 10 deterministic vst3` → *ALL 3 deterministic passes succeeded*;
   `scripts/run-pluginval.sh 10 randomise vst3` → *ALL 3 randomise passes succeeded*. Both exit 0.
   This is a local run of the same script the Linux gate uses; the macOS AU and Windows gates run in
   CI and are not restated here.
5. **Host matrix verified — OPEN.** Requires loading in target hosts. Not attempted; see
   `COMPATIBILITY_MATRIX.md`.
6. **Latency reporting verified — PASS.** `AnamorphTests` Test 3+4 (true-bypass null + latency
   reporting) passes, covering reported PDC against the actual chain delay with OS off reporting 0.
   Two v0.9.6 additions extend it: State test 22 (an off-message-thread change is deferred to the
   processor timer and delivers the CORRECT value) and State test 24 (a restore reports the
   RESTORED state's latency, not a rejected value's).
7. **Automation playback verified — OPEN.** Requires a host recording and replaying automation.
   Parameter *meaning* is pinned structurally by item 1's registry snapshot, but playback in a host
   is not.
8. **Session reload verified — PASS, and no longer a reconstruction.** The previous version's
   binary was rebuilt from source (the tree at `2c5e760^`, i.e. v0.9.5, with its own JUCE pin) and
   used to WRITE a real session: `tests/fixtures/field_capture_v0_9_5.session` (10,629 bytes) plus
   a `.manifest` recording what that binary believed the state was, including the B slot. State
   test 25 loads the capture into v0.9.6 and asserts against those numbers — so it asks "does
   v0.9.6 reproduce what v0.9.5 had", not "does v0.9.6 agree with itself". All four things this
   item names reproduce exactly: sound (5 parameters, both slots), preset name (`Gentle Width`),
   dirty-star (set), and both A/B slots (which differ from each other, so the B leg is not vacuous).

   This closes the caveat the item carried: the pre-existing legacy fixtures are reconstructions
   built by current code, which can only contain what today's understanding says an old format held.
   This one was written by the old binary.

**Note on scope.** v0.9.6 will be the first tagged release; none of v0.9.0–v0.9.5 was ever tagged,
so "the previous version" means the previous *source* version, reachable to anyone who built or
took a CI artifact. That is the transition item 8 now covers.
