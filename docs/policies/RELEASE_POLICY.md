# RELEASE_POLICY.md

Repository Governance Policy. Preconditions that must hold before a version ships.

## Preconditions (all required)

1. **Tests green** — DSP self-tests pass (`scripts/run-tests.sh`); pluginval passes at
   **strictness 10** on the Linux gate (`TESTING_POLICY.md` Levels 2–4).
2. **Compatibility checklist passed** — every item in
   `procedures/RELEASE_COMPATIBILITY_CHECKLIST.md` is checked (parameter IDs unchanged or
   migrated, serialization verified, presets migrated, host matrix, latency, automation, session
   reload).
3. **Version bumped** — `CMakeLists.txt` `project(... VERSION x.y.z)` updated.
4. **CHANGELOG updated** — a dated entry per `CHANGELOG_POLICY.md`, evidence-cited.
5. **Architecture Review cleared** — if the release contains any `ARCHITECTURE_REVIEW_GATE.md`
   change, it has human sign-off and an ADR.
6. **Docs synced** — `DOCUMENTATION_LIFECYCLE_POLICY.md` triggers applied; `HANDOVER.md` status
   fields refreshed.
7. **Manual audition acknowledged** — Level 5 (audio/visual) is the human sign-off; a green build
   is "ready to audition," not final (`README` "What cannot be verified headlessly").

## Artifacts

A release corresponds to the CI artifacts built per push: `Anamorph-Linux`, `Anamorph-Windows`,
`Anamorph-macOS` (universal VST3 + AU + Standalone). See `procedures/PACKAGING.md`.

## Versioning

`MAJOR.MINOR.PATCH`, pre-1.0 (< 1.0.0 = pre-release line), plus a CI build/dev number passed as
`-DANAMORPH_BUILD_NUMBER=${run_number}` and shown in the About box.
Evidence [Verified]: CMakeLists.txt:14,181-187; .github/workflows/build.yml:54,156,373.
