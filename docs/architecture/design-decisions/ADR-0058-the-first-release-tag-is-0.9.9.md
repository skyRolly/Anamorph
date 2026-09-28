# ADR-0058 — The first release tag is 0.9.9

**Status:** **Accepted** (2026-09-28). The owner delegated the choice between the two strategies below to the
release-cleanup round and asked for the one that best keeps the public release history true. This record is
that choice, and the owner confirmed it on 2026-09-28: 0.9.9 is the first formal tagged release, and 0.9.7 and
0.9.8 are not tagged retroactively. It changes `CHANGELOG_POLICY.md` rules 2, 7 and 8 (a Policy change, so an ADR:
`ADR_POLICY.md` rule 5) and the rule `check-docs.py` enforces them with. Tags are written as the bare version
(ADR-0059).

## Context
`CHANGELOG_POLICY.md` rule 8, `RELEASE_PROCESS.md` §Tagging and `check-docs.py`'s `FIRST_TAGGED_VERSION` all
named **0.9.7** as the first annotated release tag, because 0.9.0 through 0.9.6 had each been written up and
then superseded before a tag was cut. `CHANGELOG.md` carried link definitions to match: `[0.9.7]` a tag page,
`[0.9.8]` and `[0.9.9]` comparisons against the version below.

Neither 0.9.7 nor 0.9.8 was ever tagged. Both were closed in turn the same way 0.9.0–0.9.6 were:
- **0.9.7** was the project version on `main` from `ac47151` (2026-09-04, PR #135) to `2ed512c`
  (2026-09-07, PR #142), eight first-parent commits. `faac9fa` (PR #143, 2026-09-11) moved the version to 0.9.8
  and its changelog commit says so: "this PR's entries are 0.9.8, not 0.9.7" (`3c8bc78`).
- **0.9.8** was the version from `faac9fa` to `661a90b` (2026-09-18, PR #148), six first-parent commits;
  `b6af84e` (PR #149, 2026-09-19) moved it to 0.9.9.
- No tag and no GitHub Release exist (2026-09-28: `git tag -l` empty; the repository's tags and releases API
  both return `[]`).
- No release sign-off exists for either. `LEVEL5_AUDITION.md` §Recorded auditions held one PASS, 0.9.6; the
  0.9.7 audition was left OPEN, and none was recorded for 0.9.8 (ADR-0054's 2026-09-17 audition is the
  `DEPENDENCY_POLICY.md` rule-2 check of the JUCE 9.0.2 bump, not a release sign-off).
  `RELEASE_PROCESS.md` creates a tag only "AFTER pre-release steps 1–7 above are complete", and step 7 is
  that audition; `RELEASE_POLICY.md` publishes only after it.

So the tagging rule could not be met as written: cutting 0.9.9 would have published release notes whose
comparison link pointed at a 0.9.8 tag that does not exist.

## Problem
Make the first tag the line can actually cut consistent with the policy, the checker and the changelog,
without claiming a release that did not happen — and make the rule hold for any later version that also closes
without a tag, rather than only for this one.

## Options
- **A. Cut 0.9.7 and 0.9.8 retroactively** on `2ed512c` and `661a90b`, the last `main` commits at each
  version. **Rejected.**
  - Neither commit met the tagging precondition: no step-7 audition was performed for either.
  - A tag is this repository's release record. Pushing one runs `release.yml`, which drafts a GitHub
    Release of that commit's artifacts, so it would assert two releases that never happened.
  - The choice of commit is not unambiguous either. Each version spanned several `main` commits, and the
    changelog kept changing after each closed (12 lines deleted and 934 added between `2ed512c` and
    `b755cfb`), so the tagged tree would carry a `[0.9.7]` entry that is not the one the file carries today.
- **B. Make 0.9.9 the first tag.** **Chosen.** It is exactly the treatment 0.9.0–0.9.6 already
  received. Nothing is reconstructed, and the only tag the repository ever gains is the one for the version
  actually released.

## Decision
- **`FIRST_TAGGED_VERSION = (0, 9, 9)`** in `scripts/check-docs.py`.
  - `[0.9.9]` is a **tag page**, `https://github.com/skyRolly/Anamorph/releases/tag/0.9.9`.
  - `[0.9.8]` and `[0.9.7]` have **no definition**, like every version before them. Their headings and
    entries stay exactly as written.
- **After the first tag, the changelog records which versions were tagged.** A version above the first tag
  is tagged exactly when its entry carries a link definition. One that closes without a tag keeps its entry
  and has none, as 0.9.7 and 0.9.8 do.
  - **The comparison base is the most recent earlier TAGGED version** (`previous_of` in `check-docs.py`),
    not the entry directly below. So after `0.9.9` tagged, `0.9.10` and `0.9.11` untagged, `0.9.12` compares
    against `0.9.9`.
  - The newest version entry, while it is the release in preparation (no `## [Unreleased]` section above it),
    must carry its definition; so must the first tag, which stays a comparison base by fact.
  - `[Unreleased]` compares from the newest tagged version, so it exists only once a version is tagged:
    while no version in the file is tagged it is refused, whatever its definition names. This was added
    on 2026-09-28 from a second review finding: the first spelling checked only the URL's shape there,
    so `.../compare/0.9.8...HEAD` passed above versions that were never tagged. Only a well-formed entry
    counts, and the first tag's entry counts as tagged as soon as it is in the file -- for 0.9.9,
    throughout the cycle that prepared it -- so until the tag is pushed the procedure, not the checker,
    keeps the section out (`RELEASE_PROCESS.md` §Tagging).
  - This rule was added on 2026-09-28 from a review finding: the first spelling fixed 0.9.9 alone and still
    took the entry below as the base, so a later skipped tag would have needed manual reconciliation.
- **The self-test keeps its fixtures.** They describe a synthetic line whose first tag is 0.9.7, so it binds
  `FIXTURE_FIRST_TAGGED_VERSION` for that loop only. Cases of its own pin the real value and the rule:
  - 0.9.9 as a tag page with 0.9.8 undefined passes;
  - a `[0.9.8]` definition is refused;
  - a `[0.9.9]` comparison against 0.9.8 is refused;
  - a missing `[0.9.9]` is refused;
  - cases A–E: the first tag; one and two skipped versions; consecutive tags; and the same entries with only
    one version's tagging changed, where the base follows it;
  - the first tag without a definition below a newer entry; the `[Unreleased]` base past an untagged newest
    version; and the text of the skipped-base finding;
  - `[Unreleased]` in each of the line's three states:
    - nothing tagged: refused with no definition, or with one from 0.9.7, 0.9.8, 0.9.9 or no version
      at all, once, at its heading, with text that describes the definition truly; a pre-first-tag
      definition does not make its version a base;
    - a misspelled first-tag heading: its own finding and the refusal, since nothing counts as
      tagged until it is fixed (the check fails closed);
    - the first tag: from 0.9.9, never from the untagged 0.9.8;
    - later tags: from the newest one, never from an untagged version between two tagged ones.
- **The documents follow:**
  - `CHANGELOG_POLICY.md` rules 2 and 8, and its template, name 0.9.9 and state the base rule, and rule 7
    sends unreleased work to `[Unreleased]` only once a version is tagged;
  - `RELEASE_PROCESS.md` §Tagging names `0.9.9` as the next and first tag, with its commands, and says what
    to do when a version closes without a tag;
  - the `CHANGELOG.md` preamble and link definitions;
  - `FUTURE_RISKS.md` RISK-003, `RELEASE_HARDENING_PLAN.md`, `HANDOVER.md`, `COMMERCIAL_STATUS.md`.
- **Nothing is tagged by this change.** The tag is cut by the owner once the release preconditions hold.

## Consequences
- The `[0.9.7]` and `[0.9.8]` headings render as plain text rather than links, as 0.9.0–0.9.6 always have.
  That is the true state: there is no page to link.
- The 0.9.9 release notes link only to the 0.9.9 tag page, and every later release compares against the
  tag before it.
- RISK-003 closes when 0.9.9 is tagged, not 0.9.7. From that tag on, a changelog entry may cite the tag.
- If a future version is again closed without a tag, the commit that adds the next version's entry deletes
  the closed one's definition, and the checker then skips it as a comparison base. The rule this record
  applies: **a version is tagged only when it is released**; the first-tag constant names the first one that
  was, and the definitions record every one after it.
- The checker cannot see a tag the changelog does not record. A definition deleted from a version that WAS
  tagged reads as "closed without a tag", and it surfaces when the next tagged version's comparison names a
  base the checker refuses. The converse, a definition left on a version that closed untagged, reads as a tag;
  the guard is procedural (`RELEASE_PROCESS.md` §Tagging: the commit adding the next entry deletes it).

## Related code
- `scripts/check-docs.py` — `FIRST_TAGGED_VERSION`, `FIXTURE_FIRST_TAGGED_VERSION`, `first_tagged()`,
  `check_changelog_links` (`tagged`, `previous_of`), and the self-test's "repository's own first tag" and
  "comparison base" cases.
- `CHANGELOG.md` — the preamble and the `[0.9.9]` definition.
- `.github/workflows/release.yml` — validates tag ⇄ CMake version ⇄ dated heading and never reads link
  definitions; its tag format is ADR-0059's.

## Evidence + confidence
- **[Verified]** No tag or release exists: `git tag -l`; `GET /repos/skyRolly/Anamorph/tags` and `/releases`
  both `[]` (2026-09-28).
- **[Verified]** The version history: `git show <c>^1:CMakeLists.txt` against `git show <c>:CMakeLists.txt` for
  `ac47151` (0.9.6 → 0.9.7), `faac9fa` (0.9.7 → 0.9.8) and `b6af84e` (0.9.8 → 0.9.9); `2ed512c` and `661a90b`
  read 0.9.7 and 0.9.8.
- **[Verified]** No release audition for 0.9.7 or 0.9.8: `docs/procedures/LEVEL5_AUDITION.md` §Recorded
  auditions.
- **[Verified]** The checker: `python3 scripts/check-docs.py --self-test` passes, 511 cases. Nineteen mutants
  of the rule each fail it (re-measured 2026-09-28 with the `[Unreleased]` cases):
  - `previous_of` set back to "the entry directly below": 11 fail, including the tagged-base cases of B, C
    and E; with the old every-version definition rule as well: 12;
  - the constant set back to (0, 9, 7): 37;
  - a prefixed tag: 65;
  - the first tag no longer required, or no longer a base by fact: 2 each;
  - the skipped-base message inverted: 1;
  - the newest entry required even under `[Unreleased]`: 2;
  - `[Unreleased]` from the newest entry whether tagged or not: 9; from the newest DEFINED entry: 2;
    from the newest entry both tagged and defined: 1;
  - the refusal with nothing tagged removed, restoring the shape-only check: 12; removed at the heading
    alone: 12; made to need a definition: 1 (the wording check on the section with none, where the count
    alone cannot tell the two findings apart);
  - the refusal reported at the definition line: 3, or at the first version heading: 4;
  - its definition clause always appended: 1, naming the line instead of the URL: 2, or claiming an
    untagged version for a URL that names none: 1.
  - `check-docs.py` over the tree is clean.
