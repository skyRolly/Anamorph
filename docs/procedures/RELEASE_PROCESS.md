# RELEASE_PROCESS.md

Step-by-step release procedure. Binding preconditions are in `docs/policies/RELEASE_POLICY.md`;
the hard compatibility gate is `RELEASE_COMPATIBILITY_CHECKLIST.md`.

## Pre-release checklist

1. **Version bump** — update `project(Anamorph VERSION x.y.z ...)` in `CMakeLists.txt:14`.
2. **CHANGELOG** — add a dated, evidence-cited entry per `docs/policies/CHANGELOG_POLICY.md`
   (commit/PR reference; mark reconstructions).
3. **Tests green** — `scripts/run-tests.sh` passes; `scripts/run-pluginval.sh 10` passes on Linux in
   **both modes** (`deterministic` and `randomise` ×3) (`TESTING.md`).
4. **Compatibility gate** — complete every item in `RELEASE_COMPATIBILITY_CHECKLIST.md`.
5. **Architecture Review** — if the release contains any
   `docs/policies/ARCHITECTURE_REVIEW_GATE.md` change, confirm human sign-off + an ADR.
6. **Docs synced** — apply `docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md` triggers; refresh
   `docs/HANDOVER.md` status fields.
7. **Manual audition** — Level 5 (audio/visual) signed off in a DAW; a green build is "ready to
   audition," not final.

## Build the release artifacts

Releases correspond to the CI artifacts built per push (`CI_CD.md`):
`Anamorph-Linux`, `Anamorph-Windows`, `Anamorph-macOS`. Push the release commit and use that run's
artifacts, or build locally per `BUILD.md`. The CI build number is `${{ github.run_number }}`
(`-DANAMORPH_BUILD_NUMBER=...`), shown in the About box.

## macOS signing / notarization

CI ad-hoc codesigns the macOS bundles; they are **NOT notarized**. The shipped `INSTALL.txt`
documents the required `xattr -dr com.apple.quarantine` step for end users (`PACKAGING.md`).
`TODO: notarization is not configured in the repository; document the workflow here if/when added.`

## Versioning

`MAJOR.MINOR.PATCH`, pre-1.0 (< 1.0.0 = pre-release line). There are currently **no git tags** in
the repository; the version lives in `CMakeLists.txt` and the About box.
Evidence [Verified]: CMakeLists.txt:14,176-182. (Tagging releases is recommended but not
currently practiced — `TODO: adopt git release tags to make CHANGELOG reconstruction Verified.`)

## After release

- Update `CHANGELOG.md` (repository root) if any post-tag fixes land.
- Refresh `docs/HANDOVER.md` (Current Version, Build/Test/Release Status).
- Re-run the compatibility checklist on the next version against the just-shipped one.
