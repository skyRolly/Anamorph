# ADR-0058 — The first release tag is 0.9.9

**Status:** **Accepted** (2026-09-28). The owner delegated the choice between the two strategies below to the
release-cleanup round and asked for the one that best keeps the public release history true. This record is
that choice, and the owner confirmed it on 2026-09-28: 0.9.9 is the first formal tagged release, and 0.9.7 and
0.9.8 are not tagged retroactively. It changes `CHANGELOG_POLICY.md` rules 2, 7 and 8 (a Policy change, so an ADR:
`ADR_POLICY.md` rule 5) and the rule `check-docs.py` enforces them with. Tags are written as the bare version
(ADR-0059). Amended the same day: whether a version was tagged is read from the repository's git tags, not
from the changelog (§Decision). Amended 2026-09-29: only a release tag in `HEAD`'s history counts, and every
one needs its changelog entry (§Decision).

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
- **Which versions were tagged is what the repository's git tags say** (amended 2026-09-28 and
  2026-09-29, below). A version is tagged when its bare tag (ADR-0059) is one of this line's release
  tags: in the tag refs of the checkout `check-docs.py` runs in, in `HEAD`'s history, and not older than
  the first tag. The changelog's definitions must agree with the tags: a tagged version carries a
  definition; one that closed without a tag keeps its entry and has none, as 0.9.7 and 0.9.8 do.
  - **The comparison base is the most recent earlier TAGGED version** (`previous_of` in `check-docs.py`),
    not the entry directly below. So after `0.9.9` tagged, `0.9.10` and `0.9.11` untagged, `0.9.12` compares
    against `0.9.9`. This rule was added on 2026-09-28 from a review finding: the first spelling fixed 0.9.9
    alone and still took the entry below as the base, so a later skipped tag would have needed manual
    reconciliation.
  - The newest version entry, while it is the **release in preparation** (no `## [Unreleased]` section
    above it), must carry its definition whether or not its tag exists yet: the release commit writes it,
    and the tag is pushed onto that commit.
  - `[Unreleased]` compares from the newest release tag, so it exists only once one does:
    until then it is refused, whatever its definition names -- including throughout the cycle that
    prepares 0.9.9, when the 0.9.9 entry is in the file but the `0.9.9` tag is not. This was added on
    2026-09-28 from a second review finding: the first spelling checked only the URL's shape there, so
    `.../compare/0.9.8...HEAD` passed above versions that were never tagged.
  - **Amended 2026-09-28, from a third review finding: "tagged" is read from git, not from the changelog.**
    The second spelling read "tagged" from the file: after the first tag, a version with a definition; and
    the first tag's entry "by fact", as soon as it was in the file. So `[Unreleased]:
    .../compare/0.9.9...HEAD` above the 0.9.9 entry passed while no `0.9.9` tag existed, and the record
    left the window to the procedure. Three things are distinct: a changelog entry DECLARES a release;
    `FIRST_TAGGED_VERSION` names the version that may be the first tag; only the tag refs say a tag
    EXISTS. The checker reads them locally (`git for-each-ref refs/tags`, no network), after checking
    that the directory it checks is the root of a git checkout. Its git processes run without git's
    repository-location variables (`GIT_DIR`, `GIT_WORK_TREE`, `GIT_INDEX_FILE` and the rest of
    `git rev-parse --local-env-vars`), which a hook or `git rebase -x` exports and which would otherwise
    point them at another repository.
  - **Where the tags cannot be read** (not a git checkout, a subdirectory of one, a checkout git will not
    open, tags git cannot list, no git), the state is unknown, never "no tags" and never the declarations.
    If the file holds an `[Unreleased]` section or a version at or above the first tag other than the
    release in preparation, the checker refuses its release links with one finding that gives the reason,
    and still checks what needs no tags (a definition older than the first tag, a definition with no
    entry). A file whose only such version is the release in preparation (today's) needs no tags and is
    checked in full.
  - **CI fetches the tags and the history.** The `docs` job's checkout sets `fetch-tags: true` and, since
    the 2026-09-29 amendment, `fetch-depth: 0`. The default single-commit checkout is shallow and fetches
    no tags: until 2026-09-29 that read as "nothing was ever tagged", and since then any shallow clone
    reads as unknown, so every link that needs a tag would be refused.
    `release.yml` reaches the same job through `workflow_call` on the tag push.
  - **Amended 2026-09-29, from a fourth review finding ("Missing release hides the newest tag"): a release
    tag must have its entry, and only this line's tags are releases.** The third spelling took "tagged" as
    the intersection of the tags with the file's entries: `newest_tagged` was the newest ENTRY whose tag
    exists. So with tags `0.9.9` and `0.9.10` and no `## [0.9.10]` entry, `[Unreleased]:
    .../compare/0.9.9...HEAD` passed, silently comparing past a real release; `previous_of` was drawn from
    entries too. The invariant is now: **every applicable release tag has its `## [x.y.z]` entry before it
    can be the newest comparison base.** Four rules implement it:
    - An **applicable release tag** is a tag ref of the bare form `x.y.z` (ADR-0059), not older than
      `FIRST_TAGGED_VERSION`, whose commit is in `HEAD`'s history (`git for-each-ref --merged=HEAD
      refs/tags`). Annotated and lightweight tags count alike (`release.yml` still refuses a lightweight
      one). A prefixed tag, an older one, and one reachable only from another branch are not this line's
      releases: they need no entry and are no base. `TagState` keeps the unreached ones apart
      (`elsewhere`) so the findings that turn on one can say it exists outside this history. Releases are tagged
      on `main` (`RELEASE_PROCESS.md` §Tagging), so a later `main` commit reaches every earlier one; a tag
      cut on another branch becomes `main`'s release only when that branch is merged into `main`, and a
      branch made before a release, or not merged with `main` since, does not see it until it merges
      `main`. A version counts as tagged exactly when its tag is applicable.
    - **Each applicable tag without an entry is a finding**, newest first, at the file's first entry.
      Nothing is inferred for it: the file must record the release before anything can compare against it.
    - **The bases come from the tags, not the entries.** `newest_tagged` is the newest applicable tag, and
      `previous_of[v]` the newest applicable tag older than `v`. Where every tag has its entry this is the
      old result; where one lacks it, the base names that release and the missing-entry finding explains
      it, rather than the base falling back to the newest recorded one.
    - **A shallow clone reads as unknown**, with the reason and `git fetch --unshallow --tags` as the
      remedy, whatever tags it holds: `--merged` cannot see ancestry past the cut, and `git clone --depth`
      does not even fetch a tag whose commit lies beyond it, so a pushed release could read as another
      line's or as absent. (The first spelling of this amendment called a shallow clone known when every
      release-shaped tag it listed was reached; the review of 2026-09-29 showed a depth-1 clone that never
      fetched the `0.9.9` tag reading as "no tags", with false findings and a remedy that could not clear
      them.)
    - **A tag that exists outside `HEAD`'s history is named as such** wherever a finding turns on it: a
      definition naming it; an `[Unreleased]` section, or an `[Unreleased]` or version definition, that
      would compare from it; a version with no older release tag in this history; and an entry below a
      comparison. The finding says the tag exists but is not in this checkout's history and, if it is
      this line's release, to merge the history that carries it (`main`, for a release tagged there). It
      does not say where the commit sits, which the checker does not know, and never says git has no such
      tag. The remedy is offered only for a tag that could be a release (bare, not older than the first
      tag): merging a prefixed, leading-zero or older tag in would make it none. A tag git holds that is
      no release -- below the first tag, or with a leading zero -- is named as such, never as absent: a
      definition below the first tag is "no release of this line, whatever git holds".
    - **A release tag names a version as a heading does**, `0|[1-9]\d*` per component (`RELEASE_TAG`): a
      tag such as `0.09.10` is no release, since no heading can record it and `release.yml` cannot cut it.
    The earlier invariants hold unchanged: the first tag is `0.9.9`; `[Unreleased]` is refused while no
    applicable tag exists, whatever its definition names, including with the 0.9.9 entry and its tag page in
    the file; skipped (untagged) versions are never a base; a tagged version needs its definition, and one
    that closed untagged has none.
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
  - every case names the git tags it runs against, handed to the same `tag_state()` the tree run uses,
    so the decision logic under test is the production logic;
  - the same file under two tag states, where only the tags change the verdict; a definition left on a
    version whose tag does not exist, and one deleted from a version whose tag does, each refused; a
    prefixed tag, which is not the version's tag;
  - `[Unreleased]` in each of the line's states:
    - no tag: refused with no definition, or with one from 0.9.7, 0.9.8, 0.9.9 or no version at all,
      once, at its heading, with text that describes the definition truly; a pre-first-tag definition
      does not make its version a base;
    - **0.9.9 declared, its tag absent**: refused, and the 0.9.9 link with it; the repository's own
      `CHANGELOG.md` with the section added above 0.9.9 is refused before the tag and accepted after it;
    - a misspelled first-tag heading: its own finding, so the file fails (until 2026-09-29 the refusal
      as well; the base is now the tag itself, so `[Unreleased]` from 0.9.9 is right, and the heading is
      not reported a second time as a missing release);
    - the first tag: from 0.9.9, never from the untagged 0.9.8;
    - later tags: from the newest one, never from an untagged version between two tagged ones, with one
      and with two skipped;
  - a git tag below the first tag (a stray `0.9.8`): no `[Unreleased]` base and no comparison base;
  - every release tag needs its entry (2026-09-29): with `0.9.9` and `0.9.10` tagged and both recorded,
    `[Unreleased]` from 0.9.10 passes; with 0.9.10 tagged but not recorded, `[Unreleased]` from 0.9.9 fails
    twice (the missing entry, named at the first entry's line, and the base, which is 0.9.10) and from
    0.9.10 once; two missing releases are both reported, newest first; 0.9.10 and 0.9.11 recorded untagged
    below a tagged 0.9.12 give the base 0.9.12, and with 0.9.9 the only tag the base 0.9.9; a release in
    preparation compares from the missing release, and comparing past it names it rather than calling it
    "closed without one"; a higher tag on another branch needs no entry, and a link to it is refused as
    another line's; a tag below the first tag, a prefixed one, or one with a leading zero (`0.09.10`)
    needs no entry; a tag outside this history is named as such, with the merge remedy, in the
    `[Unreleased]` refusal and its definition, in an `[Unreleased]` definition once a release is in
    this history, in a release in preparation with no older release tag in this history, and for an
    entry below a comparison; an `[Unreleased]` definition from a tag below the first tag says it
    predates the first tag, and from a leading-zero tag that it names no version; a pre-first tag
    outside this history is not offered for a merge; a definition below the first tag with a stray tag
    in this history is refused as no release, not as "never tagged". Each synthetic fixture runs against the tags of
    the versions it records (`fixture_tags()`), so the rule does not flag them;
  - tags that cannot be read: `[Unreleased]` or a past release's link refused once, with the reason, and
    a pre-first-tag or entry-less definition still found; the release in preparation alone checked in full;
  - the fixture built from the repository's own `CHANGELOG.md` is cut back to the file as it stands while
    0.9.9 is the newest entry, and a case proves the cut survives the file's later `[Unreleased]` section
    and next entry, so the self-test does not break at the next changelog edit;
  - the reader itself, against real temporary git repositories, all run with `GIT_DIR`, `GIT_WORK_TREE`
    and `GIT_INDEX_FILE` naming a decoy repository, which must gain no commit and no tag:
    - an untagged one is known and empty, and the production path refuses `[Unreleased]` there;
    - with an annotated `0.9.9` it accepts it;
    - lightweight and prefixed tags are listed as they are;
    - a depth-1 clone reads as unknown with the `git fetch --unshallow --tags` remedy, without the tags
      and with every tag fetched and reached, and the production path refuses once, naming the shallow
      clone; unshallowed, it holds every tag and refuses the unrecorded lightweight `0.9.10` as the full
      clone does (until 2026-09-29 the depth-1 clone read as known, and with the tags fetched accepted
      `[Unreleased]`);
    - a lightweight `0.9.10` on `HEAD` with no entry is a release of the line: the production path refuses
      `[Unreleased]` from 0.9.9, naming it;
    - two lines (2026-09-29): `main` tags 0.9.9, a `maint` branch tags 0.9.10, `main` moves on. On `main`
      the reader holds `{0.9.9}` with 0.9.10 elsewhere and the production path accepts `[Unreleased]` from
      0.9.9; checked out on `maint` it refuses the file for the missing 0.9.10; a shallow clone of `main`
      cut below both tags reads as unknown with the `git fetch --unshallow --tags` remedy and refuses once;
      after that fetch it reads `{0.9.9}` and accepts;
    - a subdirectory, a plain directory, a checkout git will not open ("dubious ownership"), a checkout
      whose tags cannot be listed, no `git`, and a `git` that cannot run all read as unknown, with the
      reason; in the plain directory and with no `git`, the production path refuses once, naming it.
- **The documents follow:**
  - `CHANGELOG_POLICY.md` rules 2 and 8, and its template, name 0.9.9 and state the base rule, and rule 7
    sends unreleased work to `[Unreleased]` only once a version is tagged;
  - `RELEASE_PROCESS.md` §Tagging names `0.9.9` as the next and first tag, with its commands, says what
    to do when a version closes without a tag, and says the `[Unreleased]` section follows the tag push;
  - the `CHANGELOG.md` preamble and link definitions;
  - `FUTURE_RISKS.md` RISK-003, `RELEASE_HARDENING_PLAN.md`, `HANDOVER.md`, `COMMERCIAL_STATUS.md`.
- **Nothing is tagged by this change.** The tag is cut by the owner once the release preconditions hold.

## Consequences
- The `[0.9.7]` and `[0.9.8]` headings render as plain text rather than links, as 0.9.0–0.9.6 always have.
  That is the true state: there is no page to link.
- The 0.9.9 release notes link only to the 0.9.9 tag page, and every later release compares against the
  tag before it.
- RISK-003 closes when 0.9.9 is tagged, not 0.9.7. From that tag on, a changelog entry may cite the tag.
- If a future version is again closed without a tag, git has no tag for it: the checker skips it as a
  comparison base, and the commit that adds the next version's entry deletes the closed one's definition,
  which the checker then requires. The rule this record applies: **a version is tagged only when it is
  released**; the first-tag constant names the first one that may be, the tags say which were, and the
  definitions link to them.
- The checker reads the tags; it does not guess them. A definition deleted from a version that WAS tagged,
  and one left on a version that closed untagged, are both refused, as is `[Unreleased]` before the first
  tag is pushed. The second spelling's two documented blind spots and its procedural window are closed.
- The check is as good as the checkout's tag refs and history, and it reads no network. A clone that has
  not fetched a new tag reads as if it did not exist (the finding says how to fetch them), a shallow clone
  reads as unknown, and a local tag that was never pushed reads as existing. CI's `docs` job fetches the full history and every tag, so CI sees
  the pushed ones and which of them `HEAD` reaches.
- A release cannot hide behind the file (2026-09-29). A pushed release tag in `HEAD`'s history without its
  entry fails every later check until the entry is written, and `[Unreleased]` and the next version compare
  against that release, not an older one. A tag cut on another branch after this line's branch point is not
  this line's release and raises nothing here; a maintenance line that merges back into `main` brings its
  tags into `main`'s history and so needs their entries, which is the history the merge asserts.
- It verifies that a tag EXISTS, not that it is annotated: a lightweight `0.9.9` would count. A release tag
  is annotated because `release.yml` refuses anything else and drafts no Release for it.
- `check-docs.py --self-test` needs `git` to prove the reader; without it that case fails rather than
  passing unproved.

## Related code
- `scripts/check-docs.py` — `FIRST_TAGGED_VERSION`, `FIXTURE_FIRST_TAGGED_VERSION`, `first_tagged()`,
  `TagState` (`tags` in `HEAD`'s history, `elsewhere`), `GIT_LOCATION_VARS`, `git_env()`, `read_git_tags()`
  (`for-each-ref --merged=HEAD`, a shallow clone unknown), `RELEASE_TAG`, `tag_state()`, `check_changelog_links` (`releases`,
  `tagged`, `in_prep`, `verifiable`, `previous_of`, `newest_tagged`, the missing-entry finding), and the
  self-test's "repository's own first tag", "comparison base", "`[Unreleased]` exists only once a tag
  does", "every release tag needs its entry" and "tag reader" cases, with `fixture_tags()` giving each
  synthetic fixture the tags of the versions it records.
- `.github/workflows/build.yml` — the `docs` job's checkout, `fetch-depth: 0` and `fetch-tags: true`.
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
- **[Verified]** The checker: `python3 scripts/check-docs.py --self-test` passes, 564 cases (537 before the
  2026-09-29 amendment). Thirty-eight mutants of the 2026-09-29 rule each fail it, with the number of
  failing cases (measured on a clean clone of the final checker):
  - the bases from the tags that have an entry (the intersection, the finding's own repro): 8; the
    missing-entry finding removed: 10; the base taken from the newest entry with a tag-looking link: 19;
  - tags on another branch counted as this line's: 3; the no-tag `[Unreleased]` refusal removed,
    restoring the shape-only check: 21; a prefixed tag counted as a release: 4; a leading-zero tag
    counted as a release: 2;
  - `previous_of` set back to "the entry directly below": 25; drawn from the tagged entries only: 3;
  - a changelog definition taken as proof of a tag: 8; the tags ignored: 12; the first tag counted by
    fact: 7; `releases` without the not-older-than-the-first-tag clause: 4;
  - a malformed heading also reported as a missing release: 1;
  - a shallow clone read as known: 5; read as known when it reaches every release-shaped tag it lists
    (the first spelling): 3;
  - a definition on an untagged past version accepted: 8; a tagged version without one accepted: 3;
    the release in preparation required to be tagged already: 18; unreadable tags read as no tags: 9;
  - the inherited `GIT_DIR` kept in the reader: 1; the constant set back to (0, 9, 7): 68; a prefixed
    tag in the URL: 101; `[Unreleased]` from the newest entry whether tagged or not: 31;
  - missing releases reported oldest first: 1; comparing past a missing release explained as "the entry
    below closed without a tag": 1;
  - a tag outside `HEAD`'s history not named as such: in a definition, 1; in the `[Unreleased]` refusal
    ("git has no tag"), 1; in its definition ("a version with no git tag"), 1; in the no-base finding, 1;
    for the entry below a comparison ("closed without one"), 1; as the base of an `[Unreleased]`
    definition once a release is in this history, 1; as the base of a version definition, 1;
  - the remedy claiming the tagged commit is on `main`: 4; offered for a tag that can never be a base: 1;
  - a non-release tag git holds called "a version with no git tag": 2; a leading-zero one said to
    predate the first tag: 1; a definition below the first tag called "never tagged": 1.
- **[Verified]** Before the 2026-09-29 amendment, 537 cases; twenty-eight mutants of the rule each failed
  it (measured 2026-09-28 with the git-tag source of truth and the review's additions):
  - the no-tag `[Unreleased]` refusal removed, restoring the shape-only check: 19;
  - a changelog definition taken as proof of a tag: 6;
  - the tags ignored, restoring the file-only rule: 9;
  - the first tag counted as tagged by fact, the second spelling: 6, including the repository's own
    `CHANGELOG.md` with the section before the tag;
  - `previous_of` set back to "the entry directly below": 17;
  - a prefixed tag counted as the version's tag: 2; a prefixed tag in the URL: 79;
  - a definition on an untagged past version accepted: 6; a tagged version without one accepted: 3;
  - the release in preparation required to be tagged already: 14;
  - `tagged()` without its not-older-than-the-first-tag clause: 2;
  - unreadable tags read as no tags: 7; or as the declarations: 7; nothing checked after the refusal: 2;
  - the reader answering a subdirectory with the enclosing checkout's tags: 1; not counting lightweight
    tags: 2; reading as a checkout with no tags a directory that is none: 3, no git: 2, an unlistable
    checkout: 1, a git that cannot run: 1; every refusal of `rev-parse` called "not a git checkout": 1;
  - a shallow clone read as unknown: 3; the shallow hint dropped: 1;
  - the inherited `GIT_DIR` not cleared in the reader: 11; nor in any git process: 1 (the self-test's
    own setup fails against the decoy);
  - the real-file fixture following the live file: 1;
  - the constant set back to (0, 9, 7): 45;
  - `[Unreleased]` from the newest entry whether tagged or not: 16.
  - `check-docs.py` over the tree is clean.
