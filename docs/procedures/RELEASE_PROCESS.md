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
   (commit/PR reference; mark reconstructions), and the version's link definition at the foot of the
   file (rule 8; the exact line is under §Tagging below). That policy's §The structural grammar
   states the restricted Markdown subset both `check-docs.py` and the notes extractor enforce, and
   why each restriction exists. `check-docs.py` gates the *structure* of
   both — the heading grammar and its ISO date, newest-first order, the category names and their
   order, and a link definition of exactly the form the version calls for — and rejects the file
   until each is right. What it cannot judge is the content: whether a bullet is in the right
   category, whether the evidence citation is true, and whether the change was worth recording stay
   with the author (policy rules 2, 3 and 6).
3. **Tests green** — `scripts/run-tests.sh` passes; `scripts/run-pluginval.sh 10` passes on Linux in
   **both modes** (`deterministic` and `randomise` ×3) (`TESTING.md`).
4. **Compatibility gate** — complete every item in `RELEASE_COMPATIBILITY_CHECKLIST.md`.
5. **Architecture Review** — if the release contains any
   `docs/policies/ARCHITECTURE_REVIEW_GATE.md` change, confirm human sign-off + an ADR.
6. **Docs synced** — apply `docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md` triggers; refresh
   `docs/HANDOVER.md` status fields.
7. **Manual audition** — Level 5 is the required human sign-off. The owner explicitly attests
   final **0.9.9 / JUCE 9.0.4 PASS (A–G)**, recorded **2026-10-21 UTC** in `LEVEL5_AUDITION.md`
   and the standing engineering worklog. Architecture Review is APPROVED, compatibility is
   **8/8 PASS**; required human release gates are **COMPLETE**. Actual human test date, exact
   artifact and environments are **NOT RECORDED**. Planned release: **2026-10-22**. Earlier
   audition records remain historical; no automated agent supplies the human result.

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
:467-469 (the versioning comment and `ANAMORPH_BUILD_NUMBER`), :492 and :495 (the two version
compile definitions).

## Tagging + release pipeline (RH-PR-8)

**Tag convention:** an **annotated** tag on the release commit on `main`, named with the **bare
version** `MAJOR.MINOR.PATCH` — no prefix (ADR-0059) — created AFTER pre-release steps 1–7 above
are complete. The tag must equal the `CMakeLists.txt` `project VERSION` exactly — `release.yml`
triggers only on a bare `x.y.z` tag and fails closed on any mismatch; a prefixed tag starts no
release at all. **The line's first official tag is `0.9.9`** (ADR-0058: none of 0.9.0
through 0.9.8 was tagged; each was written up and closed before a tag was cut, and none is tagged
retroactively):

```bash
git tag -a 0.9.9 -m "Anamorph 0.9.9"
git push origin 0.9.9
```

**The release commit carries the version's link definition; the tag follows it.** Keep a
Changelog 1.1.0 asks for linkable versions, and `CHANGELOG.md` writes every heading as `## [x.y.z]`,
a link reference. A tag can only point at a commit that already exists, so the definition cannot
wait for the tag — it is written **in the release commit**, naming the tag that commit is about to
carry, and the tag is pushed straight after. The name is not a guess: `release.yml` refuses any tag
that is not exactly the CMake `project VERSION`, so the tag `x.y.z` is fixed before it exists. The
sequence, literally:

1. In the release commit (the one **pre-release step 2** dates): the `## [x.y.z] — YYYY-MM-DD` heading, the
   CMake version bump, and one line among the definitions at the foot of `CHANGELOG.md` —

   ```markdown
   [0.9.9]: https://github.com/skyRolly/Anamorph/releases/tag/0.9.9
   ```

   for the line's first tag (already in `CHANGELOG.md`); from the second tag onward a comparison
   against the most recent earlier **tagged** version, `[0.9.10]: https://github.com/skyRolly/Anamorph/compare/0.9.9...0.9.10`
   for a hypothetical next version — the form the
   specification's own example uses. "The release commit" means the commit the tag will point at:
   what is binding is that the tagged tree carries the dated heading and the definition, so work
   that landed earlier on the branch already satisfies this and needs no re-commit.
2. `check-docs.py` (every push) checks the definitions against the repository's **git tags**, not
   against what `CHANGELOG.md` declares. It counts a tag as a release of this line when it is a bare
   `x.y.z` (no leading zero) not older than the first tag and this repository's `main` holds its commit
   — every release in `main`'s history but those cut after `HEAD`, so a release tagged on `main` after a
   branch forked binds the branch before it merges `main`. A tag `main` does not hold — cut on a branch
   never merged, even the checked-out one, or on a fork's `main` — is no release and is ignored here;
   `main` is the `main` of the remote whose URL is this repository (a fork clone's `upstream`, never also
   the fork's `origin`), else `origin/main` when `origin` is the only remote. Every such release tag has its
   `## [x.y.z]` entry — one without is reported, so no link can compare past a release the file
   omits — and a definition naming its own tag, compared against the most recent earlier release tag
   (the first tag points at its own tag page); no other past version has one; no version older than
   the first tag has one. The newest version entry is the release in preparation and must carry its definition before
   its tag exists, because this commit writes it. That link is unresolvable only between this commit
   and the tag push in step 3, which is the same interval in which the dated heading names a release
   that does not exist yet.
3. Tag that commit and push the tag (the `git tag -a` / `git push` pair above). The definition
   resolves the moment GitHub sees the tag; nothing is moved, amended or rewritten afterwards.

**A version that closes without a tag** (written up, then superseded by the next version before
it is tagged, as 0.9.7 and 0.9.8 were) keeps its entry and loses its definition: the commit that
adds the newer version's entry also deletes the older one's `[x.y.z]:` line. Git has no tag for it,
so `check-docs.py` skips it as a comparison base, and the next tagged version compares against the
most recent earlier release tag (`CHANGELOG_POLICY.md` rule 8). The checker enforces both directions:
a definition left on it names a tag that was never cut, and a tagged version without a definition is
missing its link. Never tag such a version retroactively.

(Steps 1–3 immediately above are this section's own. Everywhere else — including the
"pre-release step *n*" references `release.yml` prints in its error messages — a bare step number
means the **Pre-release checklist** at the top of this file.)

If an `## [Unreleased]` section is kept between releases, its definition is
`[Unreleased]: https://github.com/skyRolly/Anamorph/compare/<last tag>...HEAD`, and the release
commit renames the section to the version heading and re-points it. The section needs a tag to
compare from, so it exists only after the first tag, `0.9.9`, has been pushed. `check-docs.py`
refuses it, whatever its definition names, while this line has no release tag — including
throughout the cycle that prepares 0.9.9, when the 0.9.9 entry is in the file but its tag is not.
Add the section in a commit after the tag push, and first fetch the tags and `main` from this
repository's remote, as the checker names it (`git fetch --tags --refmap= origin
+refs/heads/main:refs/remotes/origin/main` in an ordinary clone; in a fork clone the remote that is this repository,
not the fork): the checker reads the tag refs of the checkout it runs in, with no network access, so a
clone that has not fetched the new tag refuses the section. It compares from the newest release tag on the line, so
tag the release commit on `main` itself: every later `main` commit, and every branch made from one,
then has the tag in its history. A branch made before the tag, or not merged with `main` since, is
bound by the release all the same — CI checks a same-repo pull request at its tip, so the checker reads
`main`'s tags for any branch headed there, and reports the release's missing entry as one on `main`
the branch's history does not hold — so merge `main` into it before adding the section; and a tag cut on a
branch other than `main` is no release, not even on that branch, until `main` holds its commit. CI's `docs` job fetches
the full history, every branch and every tag. Where the tags cannot be read at all (a directory that is
not the root of a git checkout), the checkout is a shallow clone, which cannot tell which tags are in
`HEAD`'s history (run `git fetch --unshallow --tags`), or it has no `main` of this repository (run the
command the checker prints: `git fetch --refmap= origin +refs/heads/main:refs/remotes/origin/main` in an ordinary
clone, or, where no remote is recognisably this repository, adding it as one), the checker refuses the
links that depend on them and says why.

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
   section **carrying an ISO release date**, and that section must actually EXTRACT — the notes
   extractor itself runs here (`scripts/changelog-section.awk`, the same file the notes step runs,
   not a second implementation of it), so a `## [x.y.z]` line that only appears inside a fenced
   example fails now rather than after the build matrix (i.e. **pre-release steps 1–2** are
   enforced, not assumed). The date is then read off the **extracted** heading, so a dated example
   elsewhere in the file cannot vouch for an undated real entry; an undated heading — `— Unreleased`
   or bare — is rejected. The link definition is not re-checked here: `check-docs.py` has already
   gated it on every push.
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
(RELEASE_POLICY precondition 7). Publishing makes the binaries publicly downloadable. The open
owner/legal decisions (`docs/COMMERCIAL_STATUS.md` §4; KI-015, the licence) are not a tag
precondition, but they are the owner's to settle before public or commercial distribution. No
signing/notarization yet (RH-PR-3/5); the installers ship unsigned, with the user-facing
consequences documented in `docs/user/INSTALLATION.md`.
A pipeline **rehearsal** without a tag: run `release.yml` via `workflow_dispatch`
(validate + full build; no release is created). That holds from any ref, a tag included: `validate`
takes the release path only for a tag **push**, so a rehearsal started from an existing tag writes
`is-release=false` and `draft-release` does not run (ADR-0059). A release is cut only by pushing the
annotated tag.

The owner authorizes the first annotated tag **`0.9.9`** and its **draft-only** release execution
(2026-10-21 UTC), after the sign-off records are integrated on final `main` and mandatory gates
pass. No version from 0.9.0 through 0.9.8 was tagged (ADR-0058). The remote tag and release manifest
record execution; historical CHANGELOG entries retain commit-SHA evidence permanently.
Publishing remains a separate maintainer action and is not authorized by this execution request.
Evidence [Verified]: .github/workflows/release.yml; .github/workflows/build.yml (`workflow_call`).

## After release

- Update `CHANGELOG.md` (repository root) if any post-tag fixes land.
- Refresh `docs/HANDOVER.md` (Current Version, Build/Test/Release Status).
- Re-run the compatibility checklist on the next version against the just-shipped one.

## Current package namespace (ADR-0063)

Base `tech.rolly.anamorph`; macOS receipts below it use `.vst3/.au/.app`. Expanded metadata
is checked by `build-pkg.sh`. Exact named component-to-receipt, choice and payload ownership must agree; a correct ID set alone does not pass. Windows AppId stays `{D1E3D8F8-C9CE-415C-AC73-A6AA842987BD}`.
The owner accepts the pre-release receipt break; there is no old-receipt alias or fallback.
ADR-0063 is Accepted; architecture review and the owner decision are approved, and #171 is merged.
Actual receipt installation and DAW recall remain unverified; this record grants no new release approval.

The Windows installer icon gate (Accepted ADR-0064) verifies the exact checked-in B0 ICO before
ISCC and the generated Setup MAINICON afterward. Installer artifact upload also requires the
isolated component/install/reinstall/uninstall tests. No icon download or generic fallback
is allowed; normal signing/release authorization remains unchanged.

## Linux desktop payload and ownership

The Linux customer tree additionally contains `desktop-integration.sh` (sourced by
neither binary), `desktop-assets/tech.rolly.anamorph.desktop`, eight hicolor PNGs,
scalable SVG and their fixed SHA-256/provenance records. Both installer entry points
call the helper through `sh`; artifact transport need not preserve its executable bit.
The release ZIP archives that same validated tree and renders/downloads no artwork.
Extraction and portable Standalone execution change no desktop/system files.

System preflight, publication and uninstall (including already-root runs) execute
only a private helper snapshot verified against the SHA-256 embedded in both trusted
entry scripts. Keep the helper and both pins synchronized; run
`python3 tests/linux_desktop_helper_tests.py` after a helper edit. The root bootstrap
uses checked `/tmp`, private 0700/0600 single-link capture, and exact-file/directory
cleanup. It passes the original archive assets as data to the unchanged hash/copy
checks. A mismatched helper fails closed; use matching files from a trusted
distribution. The entry scripts/distribution and system tools still need independent
trust/authenticity; this pin protects subsequent helper replacement, not distribution
authenticity. SIGKILL can leave a private snapshot; no broad cleanup is attempted.
See Accepted ADR-0065's defensive follow-up for assumptions and unverified runtime.

User data root: absolute XDG_DATA_HOME, otherwise HOME/.local/share (relative XDG is
ignored visibly). System-local data root: /usr/local/share, ignoring HOME/XDG; binary
and VST3 destinations are unchanged. Install applications/tech.rolly.anamorph.desktop
and exact icons/hicolor paths. Generated Exec uses `/usr/bin/env --` plus the escaped
absolute installed executable to handle GIO's literal-percent path check without a shell.
Do not guess StartupWMClass or claim embedded ELF/host plug-in-browser artwork.

A private 0700 `.tech.rolly.anamorph` directory owns ten resource hashes and a recovery
journal. Foreign/modified paths, symlink ancestry and unknown journal entries stop the
operation while preserving files. Preflight precedes binary writes; resources stage and
publish atomically per file with rollback after ordinary failures/signals and next-run
SIGKILL reconciliation. Binary updates remain independent; a resource error is a failed
install even if valid new binaries are already present. Serialize same-root operations;
this is process-interruption recovery, not a power-loss guarantee. Uninstall uses the
same XDG root, checks ownership first, keeps modified/foreign files and never removes
shared theme directories, other products or presets. Move an intentional edited/foreign
resource to a safe backup before retrying. Optional cache refresh errors are visible;
real GNOME/KDE/WM/cache display remains manual. Accepted ADR-0065 is Accepted by owner confirmation 2026-10-25; merge remains pending.
