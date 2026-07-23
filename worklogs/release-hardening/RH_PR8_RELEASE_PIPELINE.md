# RH-PR-8 — Release Pipeline Foundation (v0.8.13 cycle)

Implements the RELEASE_HARDENING_PLAN §1/§10 "Tags + release.yml" skeleton (the audit
roadmap's Phase-1 item; closes the RISK-003 / RH-R6 *infrastructure* gap — the risk itself
closes when the first tag is cut). Infrastructure only: no product-behaviour, DSP, GUI,
serialization, or version change; no installers/signing/notarization (RH-PR-3/5/5b/6).

## 1. Current release flow (investigated before editing)

`build.yml` builds + validates on every branch push / PR / dispatch (3 OSes; DSP + state
suites; pluginval strictness 10 both modes ×3, all blocking; retain-then-strip symbols;
fail-closed customer-artifact gating; `permissions: contents: read`). A "release" is: bump
`project VERSION` → write the CHANGELOG entry → merge → take that push's CI artifacts
(`Anamorph-Linux/-Windows/-macOS`) → manual Level-5 audition → distribute by hand. Version +
build number reach the About box via `-DANAMORPH_BUILD_NUMBER=${{ github.run_number }}`.

**Gaps:** no tags (RISK-003 — shipped bytes not attributable to a source state); no
tag-triggered flow; artifacts carry no version in their names; no checksums/manifest; nothing
machine-enforces that tag ⇄ CMake version ⇄ CHANGELOG agree at release time.

## 2. What was added (minimal, reuse-first)

* **`build.yml`** — one additive trigger: `workflow_call:` (six lines incl. comment). Branch/
  PR/dispatch behaviour byte-identical; tag pushes still don't trigger it directly (the
  existing `branches: ["**"]` filter excludes tag events — verified), so a release builds
  **exactly once**, via the call.
* **`.github/workflows/release.yml`** (new) — three jobs:
  1. `validate` (fail-closed, before any build minutes): tag must be **annotated**
     (`git cat-file -t` = `tag`), `vX.Y.Z` must equal `project VERSION`, and `CHANGELOG.md`
     must already contain `## [X.Y.Z]` — i.e. RELEASE_PROCESS steps 1–2 are enforced.
  2. `build`: `uses: ./.github/workflows/build.yml` — the identical matrix/gates/artifacts.
  3. `draft-release` (`permissions: contents: write` scoped to this one job; top level stays
     `contents: read`): downloads the three customer artifacts from the same run, stages
     versioned zips `Anamorph-<version>-<OS>.zip`, generates `SHA256SUMS.txt` +
     `RELEASE_MANIFEST.txt` (version, tag, commit, CI build number, run URL, per-platform
     hashes), extracts the CHANGELOG section as notes (exact-prefix awk — version dots are
     not regex), and creates a **draft** GitHub Release via `gh` + the ephemeral
     `GITHUB_TOKEN`. Debug-symbol artifacts stay internal (ADR-0021). **Publishing the draft
     is a manual maintainer action** after the Level-5 audition — the pipeline cannot ship.
  * `workflow_dispatch` = **rehearsal**: validate (report-only where a tag is required) +
    full build; the release job is skipped (`is-release == 'true'` guard) — the pipeline can
    be exercised end-to-end without cutting a tag.
* **Tag convention** (documented in RELEASE_PROCESS §Tagging): annotated `vMAJOR.MINOR.PATCH`
  on the release commit on `main`, created after pre-release steps complete. No tag was
  created by this change (infrastructure only; first tag: the v0.8.13 release).

## 3. Traceability model

Tag (annotated: tagger + date) → commit SHA → `RELEASE_MANIFEST.txt` (version, build number,
run URL) → versioned artifact names + SHA-256 sums → About box (version + build number via
the existing `-DANAMORPH_BUILD_NUMBER` wiring, untouched). Existing per-push artifact layout
unchanged — the release assets are *copies* staged from the same artifacts.

## 4. Dependency / security review (scoped)

* `release.yml` introduces **no third-party actions** — only `actions/checkout` /
  `actions/download-artifact` (same trusted set as existing CI, same `@v7` style so
  dependabot manages them uniformly) + the `gh` CLI with the ephemeral `GITHUB_TOKEN`.
* No secrets exist or are referenced; `contents: write` is job-scoped to `draft-release`;
  `build.yml` keeps `contents: read` and (per the plan) will never see signing secrets.
* Known remaining supply-chain items, deliberately untouched here (out of scope): actions
  pinned by major tag rather than SHA across all workflows (dependabot-managed — a future
  hardening decision), and the unpinned pluginval release download (open audit-roadmap item).

## 5. Validation

* Both workflows parse as valid YAML; `build.yml`'s diff is **6 added lines in the `on:`
  block only** (triggers now push/PR/dispatch/workflow_call — verified by parse).
* The validate/stage shell logic was executed locally against the real repo: version parse
  → `0.8.12`; CHANGELOG gate finds `## [0.8.12]`; notes extraction returns exactly the
  46-line 0.8.12 section and stops at the next heading.
* Existing CI behaviour: unchanged by construction (additive trigger; tag pushes triggered
  nothing before and now trigger only `release.yml`). The suites/gates themselves are not
  touched by this change (no code/CMake edits).
* True end-to-end proof requires a tag or a dispatch rehearsal — the rehearsal mode exists
  precisely for that and can be run from the Actions tab after merge.

## 6. Remaining manual release steps (after this change)

1. Version bump + CHANGELOG entry + docs sync + green gates (RELEASE_PROCESS 1–6, unchanged).
2. **Level-5 manual audition** (unchanged; also the open gate for ADR-0022/JUCE 9).
3. `git tag -a vX.Y.Z && git push origin vX.Y.Z` (new — replaces "collect artifacts by hand").
4. Review the draft GitHub Release and **publish it** (new, deliberately manual).
5. macOS de-quarantine guidance for users remains until RH-PR-3 (notarization).

## 7. Future RH items this unblocks

RH-PR-3 (sign+notarize inserts into `release.yml` between build and draft-release),
RH-PR-5/5b (Authenticode + installer sections), RH-PR-6 (.pkg), RH-PR-9 (QA matrix + full
release rehearsal), ADR-0019/0020 (pipeline/signing + tagging-scheme decisions — the tag
convention here is the §1-baseline proposal those ADRs will ratify or amend).
