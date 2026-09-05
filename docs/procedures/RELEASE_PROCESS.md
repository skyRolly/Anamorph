# RELEASE_PROCESS.md

Step-by-step release procedure. Binding preconditions are in `docs/policies/RELEASE_POLICY.md`;
the hard compatibility gate is `RELEASE_COMPATIBILITY_CHECKLIST.md`.

## Pre-release checklist

1. **Version bump** — update `project(Anamorph VERSION x.y.z ...)` in `CMakeLists.txt:14`.
   The citation gate does not treat this as drift: that line is declared in
   `VERSIONED_LINES` (`scripts/check-citations.py`), which replaces the base comparison
   with a permanent check that the line still contains `project(Anamorph VERSION`. If the
   line ever MOVES, the run fails and the declaration's line number must be re-derived —
   `--fix` cannot do it, because the declaration is what turned the comparison off.
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
   audition," not final. **Scope, per-item checks and the record format are in
   `LEVEL5_AUDITION.md`**, which also states when a previous audition stops counting (a machine-code
   or audible-behaviour change invalidates it — the 2026-08-15 v0.9.4 audition does not carry over
   to v0.9.7). It requires a human; no CI job and no automated agent can supply it.

## Build the release artifacts

Releases publish flat zips archived by `release.yml` from the `Anamorph-<OS>` staging
trees built per push (`CI_CD.md`) — the Linux one carries the install scripts — plus the
installers `Anamorph-Windows-installer` and `Anamorph-macOS-installer` (`PACKAGING.md`
§Installers). Those same `Anamorph-<OS>` artifacts are the loose-file per-push downloads;
the artifact transport drops Unix executable bits on that route, and `release.yml`
restores them before archiving (fail-closed). Push the release commit and use that run's
artifacts, or build locally per
`BUILD.md`. The CI build number is `${{ github.run_number }}`
(`-DANAMORPH_BUILD_NUMBER=...`), shown in the About box.

## macOS signing / notarization

CI ad-hoc codesigns the macOS bundles; they are **NOT notarized**. The shipped
`packaging/macos/INSTALL.txt` documents the `xattr -dr com.apple.quarantine` step required by the
**zip route** (payloads installed by the `.pkg` carry no quarantine attribute, so that route needs
no Terminal step) and how to get past the Gatekeeper prompt on the unsigned `.pkg`
itself (`PACKAGING.md`).
`TODO: notarization is not configured in the repository; document the workflow here if/when added.`

## Versioning

`MAJOR.MINOR.PATCH`, pre-1.0 (< 1.0.0 = pre-release line); the version lives in
`CMakeLists.txt` and the About box. Evidence [Verified]: CMakeLists.txt:14 (`project VERSION`),
:306-331 (the versioning comment, `ANAMORPH_BUILD_NUMBER`, and the two version compile definitions).

## Tagging + release pipeline (RH-PR-8)

**Tag convention:** an **annotated** tag `vMAJOR.MINOR.PATCH` on the release commit on `main`,
created AFTER pre-release steps 1–7 above are complete. The tag must equal the `CMakeLists.txt`
`project VERSION` exactly — `release.yml` fails closed on any mismatch. **The next tag is
`v0.9.7`** (none of 0.9.0 through 0.9.6 was tagged; each was written up and superseded
before a tag was cut):

```bash
git tag -a v0.9.7 -m "Anamorph 0.9.7"
git push origin v0.9.7
```

**Add the version's link definition in the same commit.** Keep a Changelog 1.1.0 asks for linkable
versions, and `CHANGELOG.md` writes every heading as `## [x.y.z]` — a link reference whose definition
can only exist once the tag does. So, in the commit the tag points at, append one line to the
definitions at the foot of the file:

```markdown
[0.9.7]: https://github.com/skyRolly/Anamorph/releases/tag/v0.9.7
```

From the second tag onward make it a comparison against the previous tag
(`.../compare/v0.9.7...v0.9.8`) — the form the specification's own example uses. Never add a
definition for a tag that has not been pushed (`CHANGELOG_POLICY.md` rule 8).

**Date the CHANGELOG heading before tagging — the pipeline now enforces it.** `release.yml`
extracts the `## [x.y.z]` section **verbatim, heading included**, as the release **notes body**
(the release *title* is set separately to `Anamorph <version>`), so a heading still reading
`— Unreleased` would appear at the top of the published notes. Validation therefore **fails
closed unless the heading carries an ISO date** — which covers a bare `## [x.y.z]` with no date at
all, not only the literal word `Unreleased`.

Two practical consequences:

- Date the heading **in the commit the tag points at**, not afterwards — the check reads the
  tagged tree.
- A `workflow_dispatch` rehearsal only *warns* about an undated heading, so rehearsals stay green
  while the real tag does not.

Pushing the tag triggers `.github/workflows/release.yml`, which:

1. **Validates release metadata fail-closed** — the tag must be annotated, must equal the
   `CMakeLists.txt` `project VERSION`, `CHANGELOG.md` must already carry the `## [x.y.z]`
   section **carrying an ISO release date** (i.e. steps 1–2 above are enforced, not assumed).
   An undated heading — `— Unreleased` or bare — is rejected.
2. **Runs the full existing gate exactly once** by *calling* `build.yml` (`workflow_call`) —
   the same 3-OS matrix, DSP + state suites, pluginval strictness 10 both modes ×3, symbol
   retain-then-strip, fail-closed artifact gating. Tag pushes do not trigger `build.yml`
   directly (its `branches` filter excludes tag events), so nothing builds twice.
3. **Creates a DRAFT GitHub Release** with the **exact per-platform payloads CI built and
   validated** (the `Anamorph-<OS>` staging trees) — archived as
   `Anamorph-<version>-<OS>.zip` with the executable bits the artifact transport drops
   restored on the known payload paths and then verified fail-closed inside the zip —
   plus the two installers (`Anamorph-<version>-Windows-Installer.exe`,
   `Anamorph-<version>-macOS.pkg`; already version-named at build time, fail-closed on
   absence or version skew, moved unmodified — the Linux installer is `install.sh` inside
   the Linux zip),
   the user manual (`Anamorph-<version>-UserManual.md`), the third-party attribution and the
   internal testing guide (`Anamorph-<version>-NOTICE.txt`, `Anamorph-<version>-THIRD_PARTY_LICENSES.md`,
   `Anamorph-<version>-SUPPORT.md` — the packages themselves are lean, so these accompany
   every download route from the release page), `SHA256SUMS.txt`
   over all assets, and a `RELEASE_MANIFEST.txt` (version / tag / commit / CI build number /
   hashes / run link), with the CHANGELOG section as the release notes. Debug-symbol artifacts stay
   internal (ADR-0021).

**Publishing the draft is a manual maintainer action** — after the Level-5 audition
(RELEASE_POLICY precondition 7). No signing/notarization yet (RH-PR-3/5); the installers
ship unsigned, with the user-facing consequences documented in `docs/user/INSTALLATION.md`.
A pipeline **rehearsal** without a tag: run `release.yml` via `workflow_dispatch`
(validate + full build; no release is created).

No release tag exists yet — the first will be cut at the **v0.9.7** release (none of 0.9.0 through 0.9.6 was tagged). Historical
CHANGELOG entries keep their commit-SHA evidence; entries from the first tag onward cite the
tag (upgrades CHANGELOG evidence per `CHANGELOG_POLICY.md`; closes RISK-003 when practiced).
Evidence [Verified]: .github/workflows/release.yml; .github/workflows/build.yml (`workflow_call`).

## After release

- Update `CHANGELOG.md` (repository root) if any post-tag fixes land.
- Refresh `docs/HANDOVER.md` (Current Version, Build/Test/Release Status).
- Re-run the compatibility checklist on the next version against the just-shipped one.
