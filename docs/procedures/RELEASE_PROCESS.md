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

`MAJOR.MINOR.PATCH`, pre-1.0 (< 1.0.0 = pre-release line); the version lives in
`CMakeLists.txt` and the About box. Evidence [Verified]: CMakeLists.txt:14,181-187.

## Tagging + release pipeline (RH-PR-8)

**Tag convention:** an **annotated** tag `vMAJOR.MINOR.PATCH` (e.g. `v0.8.13`) on the release
commit on `main`, created AFTER pre-release steps 1–7 above are complete:

```bash
git tag -a v0.8.13 -m "Anamorph 0.8.13"
git push origin v0.8.13
```

Pushing the tag triggers `.github/workflows/release.yml`, which:

1. **Validates release metadata fail-closed** — the tag must be annotated, must equal the
   `CMakeLists.txt` `project VERSION`, and `CHANGELOG.md` must already carry the `## [x.y.z]`
   section (i.e. steps 1–2 above are enforced, not assumed).
2. **Runs the full existing gate exactly once** by *calling* `build.yml` (`workflow_call`) —
   the same 3-OS matrix, DSP + state suites, pluginval strictness 10 both modes ×3, symbol
   retain-then-strip, fail-closed artifact gating. Tag pushes do not trigger `build.yml`
   directly (its `branches` filter excludes tag events), so nothing builds twice.
3. **Creates a DRAFT GitHub Release** with the **exact per-platform archives CI built and
   validated** — renamed to `Anamorph-<version>-<OS>.zip`, never unpacked or re-packed, so
   Unix permissions, symlinks and the signed macOS bundle layout inside them are untouched —
   plus `SHA256SUMS.txt` and a `RELEASE_MANIFEST.txt` (version / tag / commit / CI build
   number / per-platform hashes / run link), with the CHANGELOG section as the release notes.
   Debug-symbol artifacts stay internal (ADR-0021).

**Publishing the draft is a manual maintainer action** — after the Level-5 audition
(RELEASE_POLICY precondition 7). No signing/notarization/installers yet (RH-PR-3/5/5b/6).
A pipeline **rehearsal** without a tag: run `release.yml` via `workflow_dispatch`
(validate + full build; no release is created).

No release tag exists yet — the first will be cut at the v0.8.13 release. Historical
CHANGELOG entries keep their commit-SHA evidence; entries from the first tag onward cite the
tag (upgrades CHANGELOG evidence per `CHANGELOG_POLICY.md`; closes RISK-003 when practiced).
Evidence [Verified]: .github/workflows/release.yml; .github/workflows/build.yml (`workflow_call`).

## After release

- Update `CHANGELOG.md` (repository root) if any post-tag fixes land.
- Refresh `docs/HANDOVER.md` (Current Version, Build/Test/Release Status).
- Re-run the compatibility checklist on the next version against the just-shipped one.
