# RELEASE_COMPATIBILITY_CHECKLIST.md

Hard compatibility gate. **Every box must be checked before a release ships.** This enforces
`docs/policies/COMPATIBILITY_POLICY.md` and its subset policies. A failed item blocks the release
(or requires the COMPATIBILITY_POLICY exception: ADR + migration + Architecture Review).

## Checklist

- [ ] **Parameter IDs unchanged** — diff the parameter set against the previous release; no `pid::`
      ID renamed or removed. (Display-name changes are allowed; record in CHANGELOG.)
      Ref: `docs/architecture/PARAMETER_REGISTRY.md`, `docs/policies/PARAMETER_COMPATIBILITY_POLICY.md`.
      *Automated since the v0.8.13 cycle:* the registry-snapshot test in `tests/state_tests.cpp`
      (`AnamorphStateTests`, CI-blocking on all three platforms) fails on any ID/name/order/
      range/automation-flag change vs `tests/fixtures/parameter_registry.snapshot`.
- [ ] **Serialization schema verified** — no field removed or semantically changed in
      `AnamorphRoot` / `ANAMORPH` (APVTS) / `ANAMORPH_INTERNAL` / `AB`; additions tolerate absence.
      Ref: `docs/architecture/SERIALIZATION_REGISTRY.md`, `docs/policies/SESSION_COMPATIBILITY_POLICY.md`.
      *Automated since the v0.8.13 cycle:* schema-shape + raw-exact round-trip + the three
      legacy-format fixtures in `tests/state_tests.cpp`. The cross-version step below stays manual.
- [ ] **Presets migrated** — factory presets and a representative user `.anamorph` still load and
      sound identical. Ref: `src/PresetManager.cpp`.
      *Partially automated:* `tests/state_tests.cpp` proves save→reload structural equality +
      exclusion rules + factory loadability; "sound identical" remains a Level-5 (audition) check.
- [ ] **Pluginval passed (both modes)** — `scripts/run-pluginval.sh 10 deterministic` **and**
      `scripts/run-pluginval.sh 10 randomise` (`--randomise` ×3) pass on the Linux gate (strictness 10).
      Ref: `docs/procedures/TESTING.md`.
- [ ] **Host matrix verified** — load in the target hosts and confirm load + automation + state.
      (Currently Unverified in-repo; this requires manual DAW testing —
      `docs/architecture/COMPATIBILITY_MATRIX.md`.)
- [ ] **Latency reporting verified** — reported PDC matches the actual chain delay across the
      oversampling settings; OS-off reports 0. Ref: `docs/architecture/LATENCY_MODEL.md`; test
      `testBypassNullAndLatency`.
- [ ] **Automation playback verified** — recorded automation on host-visible parameters plays back
      with unchanged meaning. Ref: `docs/policies/PARAMETER_COMPATIBILITY_POLICY.md`.
- [ ] **Session reload verified** — save a session in the previous version, load it in the new
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
