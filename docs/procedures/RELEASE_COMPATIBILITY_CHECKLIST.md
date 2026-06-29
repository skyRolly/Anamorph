# RELEASE_COMPATIBILITY_CHECKLIST.md

Hard compatibility gate. **Every box must be checked before a release ships.** This enforces
`docs/policies/COMPATIBILITY_POLICY.md` and its subset policies. A failed item blocks the release
(or requires the COMPATIBILITY_POLICY exception: ADR + migration + Architecture Review).

## Checklist

- [ ] **Parameter IDs unchanged** — diff the parameter set against the previous release; no `pid::`
      ID renamed or removed. (Display-name changes are allowed; record in CHANGELOG.)
      Ref: `docs/architecture/PARAMETER_REGISTRY.md`, `docs/policies/PARAMETER_COMPATIBILITY_POLICY.md`.
- [ ] **Serialization schema verified** — no field removed or semantically changed in
      `AnamorphRoot` / `ANAMORPH` (APVTS) / `ANAMORPH_INTERNAL` / `AB`; additions tolerate absence.
      Ref: `docs/architecture/SERIALIZATION_REGISTRY.md`, `docs/policies/SESSION_COMPATIBILITY_POLICY.md`.
- [ ] **Presets migrated** — factory presets and a representative user `.anamorph` still load and
      sound identical. Ref: `src/PresetManager.cpp`.
- [ ] **Pluginval passed** — `scripts/run-pluginval.sh 10` passes on the Linux gate (strictness 10).
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
