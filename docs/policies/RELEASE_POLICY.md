# RELEASE_POLICY.md

Repository Governance Policy. Preconditions that must hold before a version ships.

## Preconditions (all required)

1. **Tests green** — DSP self-tests pass (`scripts/run-tests.sh`); pluginval passes at the
   configured strictness on the Linux gate (`TESTING_POLICY.md` Levels 2–4). The value is
   `ANAMORPH_PLUGINVAL_STRICTNESS` in `.github/workflows/build.yml`; this precondition is that the
   gate passes, not that it passes at a number restated here.
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
`Anamorph-macOS` (universal VST3 + AU + Standalone; loose files — a downloaded artifact
extracts straight to the payload), plus the two installer artifacts
`Anamorph-Windows-installer` and `Anamorph-macOS-installer`. See
`procedures/PACKAGING.md`.
Since RH-PR-8, pushing an annotated `vX.Y.Z` release tag additionally produces a **draft**
GitHub Release carrying the **exact staging trees CI built and validated**, archived as
`Anamorph-<version>-<OS>.zip` with the executable bits the artifact transport drops
restored and verified fail-closed, + the two installers (moved
unmodified, fail-closed on version skew) + the user manual + `NOTICE`,
`THIRD_PARTY_LICENSES.md` and `SUPPORT.md` as version-named assets + SHA-256 sums over all
assets + a traceability manifest, via `.github/workflows/release.yml` (metadata validated
fail-closed; the existing `build.yml` gates are reused unchanged). **Publishing the draft is
a manual maintainer action** after precondition 7 (Level-5 audition) — the pipeline cannot
ship a release on its own. See `procedures/RELEASE_PROCESS.md` §Tagging.

## Third-party attribution (required with every release)

`NOTICE` and `THIRD_PARTY_LICENSES.md` must **accompany every binary distribution**, not
only sit in the repository: several licences JUCE vendors (libjpeg/IJG, FLAC, Ogg Vorbis)
require their notice with a binary distribution. Since 2026-07-26 (owner decision — the
packages stay lean for the closed-source commercial product) they are discharged as
**version-named release-page assets** published next to every zip/installer. Since
2026-07-26 (owner decision) `INSTALL.txt` carries installation instructions only, so those
assets are the **sole** carrier of the mandatory IJG acknowledgement: a release **must not
be published** without the `Anamorph-<version>-NOTICE.txt` and
`Anamorph-<version>-THIRD_PARTY_LICENSES.md` assets attached; redistributing the binaries
outside the release page requires carrying those files along.
Recorded as a fact, not as a determination: the **per-push CI artifacts** are a separate,
internal-testing download route and carry no attribution file at all. Whether that route
needs one is an owner/legal question this repository does not answer (`KI-015`;
`docs/COMMERCIAL_STATUS.md` §4).
`THIRD_PARTY_LICENSES.md` must be **re-verified after any JUCE version bump** — the inventory
is derived from the pinned tree, and two components (FreeType and stb, vendored inside PlutoVG)
do not appear in JUCE's own `LICENSE.md`. See `procedures/PACKAGING.md` §"Third-party
attribution & support files".

## Versioning

`MAJOR.MINOR.PATCH`, pre-1.0 (< 1.0.0 = pre-release line), plus a CI build/dev number passed as
`-DANAMORPH_BUILD_NUMBER=${run_number}` and shown in the About box.
Evidence [Verified]: CMakeLists.txt:14, 266-291 (the versioning block: the cache variable, then the
`set_source_files_properties` that attaches it to the one translation unit reading it);
.github/workflows/build.yml:562, 1186, 1608 (the per-OS Configure steps passing
`-DANAMORPH_BUILD_NUMBER`).
