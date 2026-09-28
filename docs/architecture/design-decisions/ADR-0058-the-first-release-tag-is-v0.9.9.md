# ADR-0058 — The first release tag is v0.9.9

**Status:** **Accepted** (2026-09-28). The owner delegated the choice between the two strategies below to the
release-cleanup round and asked for the one that best keeps the public release history true. This record is
that choice. It changes `CHANGELOG_POLICY.md` rules 2 and 8 (a Policy change, so an ADR: `ADR_POLICY.md`
rule 5) and the constant `check-docs.py` enforces them with.

## Context
`CHANGELOG_POLICY.md` rule 8, `RELEASE_PROCESS.md` §Tagging and `check-docs.py`'s `FIRST_TAGGED_VERSION` all
named **v0.9.7** as the first annotated release tag, because 0.9.0 through 0.9.6 had each been written up and
then superseded before a tag was cut. `CHANGELOG.md` carried link definitions to match: `[0.9.7]` a tag page,
`[0.9.8]` and `[0.9.9]` comparisons against the version below.

Neither v0.9.7 nor v0.9.8 was ever tagged. Both were closed in turn the same way 0.9.0–0.9.6 were:
- **0.9.7** was the project version on `main` from `ac47151` (2026-09-04, PR #135) to `2ed512c`
  (2026-09-07, PR #142), eight first-parent commits. `faac9fa` (PR #143, 2026-09-11) moved the version to 0.9.8
  and its changelog commit says so: "this PR's entries are 0.9.8, not 0.9.7" (`3c8bc78`).
- **0.9.8** was the version from `faac9fa` to `661a90b` (2026-09-18, PR #148), six first-parent commits;
  `b6af84e` (PR #149, 2026-09-19) moved it to 0.9.9.
- No `v*` tag and no GitHub Release exist (2026-09-28: `git tag -l` empty; the repository's tags and
  releases API both return `[]`).
- No release sign-off exists for either. `LEVEL5_AUDITION.md` §Recorded auditions holds one PASS, v0.9.6;
  the v0.9.7 audition was left OPEN, and none was recorded for 0.9.8 (ADR-0054's 2026-09-17 audition is the
  `DEPENDENCY_POLICY.md` rule-2 check of the JUCE 9.0.2 bump, not a release sign-off).
  `RELEASE_PROCESS.md` creates a tag only "AFTER pre-release steps 1–7 above are complete", and step 7 is
  that audition; `RELEASE_POLICY.md` publishes only after it.

So the tagging rule could not be met as written: cutting v0.9.9 would have published release notes whose
`compare/v0.9.8...v0.9.9` link points at a tag that does not exist.

## Problem
Make the first tag the line can actually cut consistent with the policy, the checker and the changelog,
without claiming a release that did not happen.

## Options
- **A. Cut v0.9.7 and v0.9.8 retroactively** on `2ed512c` and `661a90b`, the last `main` commits at each
  version. **Rejected.**
  - Neither commit met the tagging precondition: no step-7 audition was performed for either.
  - A tag is this repository's release record. Pushing one runs `release.yml`, which drafts a GitHub
    Release of that commit's artifacts, so it would assert two releases that never happened.
  - The choice of commit is not unambiguous either. Each version spanned several `main` commits, and the
    changelog kept changing after each closed (12 lines deleted and 934 added between `2ed512c` and
    `b755cfb`), so the tagged tree would carry a `[0.9.7]` entry that is not the one the file carries today.
- **B. Make v0.9.9 the first tag.** **Chosen.** It is exactly the treatment 0.9.0–0.9.6 already
  received. Nothing is reconstructed, and the only tag the repository ever gains is the one for the version
  actually released.

## Decision
- **`FIRST_TAGGED_VERSION = (0, 9, 9)`** in `scripts/check-docs.py`.
  - `[0.9.9]` is a **tag page**, `https://github.com/skyRolly/Anamorph/releases/tag/v0.9.9`.
  - `[0.9.8]` and `[0.9.7]` have **no definition**, like every version before them. Their headings and
    entries stay exactly as written.
  - The version after 0.9.9 compares against v0.9.9.
- **The self-test keeps its fixtures.** They describe a synthetic line whose first tag is 0.9.7, so it binds
  `FIXTURE_FIRST_TAGGED_VERSION` for that loop only. Four new cases pin the real value:
  - 0.9.9 as a tag page with 0.9.8 undefined passes;
  - a `[0.9.8]` definition is refused;
  - a `[0.9.9]` comparison against 0.9.8 is refused;
  - a missing `[0.9.9]` is refused.

  With the constant set back to (0, 9, 7), three of the four fail.
- **The documents follow:**
  - `CHANGELOG_POLICY.md` rules 2 and 8, and its template, name v0.9.9;
  - `RELEASE_PROCESS.md` §Tagging names `v0.9.9` as the next and first tag, with its commands;
  - the `CHANGELOG.md` preamble and link definitions;
  - `FUTURE_RISKS.md` RISK-003, `RELEASE_HARDENING_PLAN.md`, `HANDOVER.md`, `COMMERCIAL_STATUS.md`.
- **Nothing is tagged by this change.** The tag is cut by the owner once the release preconditions hold.

## Consequences
- The `[0.9.7]` and `[0.9.8]` headings render as plain text rather than links, as 0.9.0–0.9.6 always have.
  That is the true state: there is no page to link.
- The v0.9.9 release notes link only to the v0.9.9 tag page, and every later release compares against the
  tag before it.
- RISK-003 closes when v0.9.9 is tagged, not v0.9.7. From that tag on, a changelog entry may cite the tag.
- If a future version is again closed without a tag, the same situation recurs. The rule this record
  applies: **a version is tagged only when it is released**; the first-tag constant names the first one
  that was.

## Related code
- `scripts/check-docs.py` — `FIRST_TAGGED_VERSION`, `FIXTURE_FIRST_TAGGED_VERSION`, `first_tagged()`,
  `check_changelog_links`, and the self-test's "repository's own first tag" cases.
- `CHANGELOG.md` — the preamble and the `[0.9.9]` definition.
- `.github/workflows/release.yml` — unchanged: it validates tag ⇄ CMake version ⇄ dated heading and never
  reads link definitions.

## Evidence + confidence
- **[Verified]** No tag or release exists: `git tag -l`; `GET /repos/skyRolly/Anamorph/tags` and `/releases`
  both `[]` (2026-09-28).
- **[Verified]** The version history: `git show <c>^1:CMakeLists.txt` against `git show <c>:CMakeLists.txt` for
  `ac47151` (0.9.6 → 0.9.7), `faac9fa` (0.9.7 → 0.9.8) and `b6af84e` (0.9.8 → 0.9.9); `2ed512c` and `661a90b`
  read 0.9.7 and 0.9.8.
- **[Verified]** No release audition for 0.9.7 or 0.9.8: `docs/procedures/LEVEL5_AUDITION.md` §Recorded
  auditions.
- **[Verified]** The checker: `python3 scripts/check-docs.py --self-test` passes, 468 cases; the mutant with
  (0, 9, 7) fails 3; `check-docs.py` over the tree is clean with the new definitions.
