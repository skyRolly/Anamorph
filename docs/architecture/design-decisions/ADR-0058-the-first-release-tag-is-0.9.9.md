# ADR-0058 — The first release tag is 0.9.9

**Status:** **Accepted** (2026-09-28). The owner delegated the choice between the two strategies below to the
release-cleanup round and asked for the one that best keeps the public release history true. This record is
that choice, and the owner confirmed it on 2026-09-28: 0.9.9 is the first formal tagged release, and 0.9.7 and
0.9.8 are not tagged retroactively. It changes `CHANGELOG_POLICY.md` rules 2, 7 and 8 (a Policy change, so an ADR:
`ADR_POLICY.md` rule 5) and the rule `check-docs.py` enforces them with. Tags are written as the bare version
(ADR-0059). Amended the same day: whether a version was tagged is read from the repository's git tags, not
from the changelog (§Decision). Amended 2026-09-29: only a release tag on this line counts, and every
one needs its changelog entry; the same day, the line was defined as `main`'s history for a branch headed
there, after the finding was shown still to apply at a pull request's tip (§Decision).

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
  tags: in the tag refs of the checkout `check-docs.py` runs in, on its release line (`HEAD`'s history,
  and `main`'s for a branch headed there, below), and not older than the first tag. The changelog's definitions must agree with the tags: a tagged version carries a
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
      `FIRST_TAGGED_VERSION`, whose commit is on this checkout's **release line** (`git for-each-ref
      --merged`). Annotated and lightweight tags count alike (`release.yml` still refuses a lightweight
      one). A prefixed tag, an older one, and one off the line are not this line's releases: they need no
      entry and are no base. `TagState` keeps the ones off the line apart (`elsewhere`) so the findings
      that turn on one can say it exists outside this history. A version counts as tagged exactly when
      its tag is applicable.
    - **The release line** (the first spelling of this amendment took `HEAD`'s history alone; see the
      next bullet). Releases are tagged on `main` (`RELEASE_PROCESS.md` §Tagging, `RELEASE_BRANCH`):
      - a checkout **on `main`'s first-parent line** (`main`, an older `main` commit, a release tag) has
        `HEAD`'s history as its line. A tag `main` gained later is that commit's future, not a release it
        omits;
      - a **branch headed for `main`** (`HEAD` not in its history, `git merge-base --is-ancestor`) has
        `HEAD`'s history **and `main`'s**. A release `main` gained after the branch forked is one the
        branch will land on, so it binds the branch before the branch merges it; those tags are also kept
        in `ahead`, and the missing-entry finding names them as releases on `main` that the branch's
        history does not hold, with "merge `main`" as the remedy;
      - a pull request's commit **merged into `main` off its first-parent line** -- the tip, or any
        earlier commit of the pull request -- landed with the oldest first-parent commit that holds it
        (the first-parent commits of `main` that are also in `git rev-list --ancestry-path HEAD..main`;
        `--ancestry-path` together with `--first-parent` marks a merge only when HEAD is its own second
        parent, which left every earlier commit bound by nothing). It landed on `main` as it stood just
        before that merge: the tags there bind it, and later ones are its future. Reading it as `main`'s
        past let a re-run on a merged commit pass what failed before the merge. A merge `main` never
        holds is headed for it even when its parents are all on its line: a branch tip merging two `main`
        commits with edits of its own has that shape, and so does a fork pull request's test merge
        (`refs/pull/N/merge`) checked again after the pull request merged. Git cannot tell them apart by
        ancestry, so both are bound by the whole line (binding such a merge as its parents are, tried in
        `af4ac33` and reverted, hid the release `main` tagged after those parents at such a branch tip);
      - `main` is the remote-tracking `main` of `origin` -- which CI's full-history checkout fetches
        (`actions/checkout` with `fetch-depth: 0` maps `+refs/heads/*:refs/remotes/origin/*`) -- and of
        any remote whose URL is this repository under any form GitHub serves it at (https, scp-style,
        ssh:// with a port or through `ssh.github.com:443`; `REPOSITORY_REMOTE`, anchored on the host and
        matched on `git remote get-url`, which applies `insteadOf` aliases -- a local path or another host
        whose path merely ends in `/github.com/skyRolly/Anamorph` is a mirror, not this repository), as a
        fork clone's `upstream`; HEAD is placed on each and
        their bound tags are united. The local branch is read only where no such remote-tracking branch
        exists: a local `main` can hold unpushed commits no release line has, and treating HEAD as its
        past hid `origin/main`'s newer release. A checkout with none is **unknown**, with `git fetch
        origin main:refs/remotes/origin/main` as the remedy (a single-branch clone's `git fetch origin
        main` writes only `FETCH_HEAD`): without the line, a release on it could not be told from
        another branch's tag. A remote under another URL is not read;
      - a tag cut on a branch other than `main` is `main`'s release only once that branch is merged into
        `main`; one on a branch never merged is another line's.
    - **Amended again 2026-09-29: the finding still applied at a pull request's tip.** The review
      finding stayed open on `482a2b0`, anchored at `newest_tagged = releases[-1]`. Reproduced in a real
      repository: `main` tags 0.9.9; a PR branch forks; `main` writes and tags 0.9.10; the branch adds
      `[Unreleased]` from 0.9.9. On the branch's tip, `--merged=HEAD` held 0.9.9 alone, so `newest_tagged`
      was 0.9.9 and the file passed with 0 findings. CI checks a same-repo pull request only there: its
      `docs` job runs on the branch push, and `merge-check`, which builds the merge commit, runs no
      `check-docs.py`. After the merge `main` failed with 2 findings. A review of that fix confirmed three
      local false negatives in its first spelling (a local `main` holding HEAD hid `origin/main`'s
      release; a merged pull request's commit read as `main`'s past; a fork's `upstream` ignored), a
      remedy that did not work, and a crash on a non-UTF-8 tag name; the placement above answers the
      first three. The literal state (both tags in
      `HEAD`'s history) was already refused; the release line closes the remaining path.
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
      comparison. The finding says the tag exists but is not in this checkout's history -- "nor in
      `main`'s" for a branch headed there (`unmerged`) -- and, if it is this line's release, to merge the
      history that carries it; for a commit on or merged into `main`, where the tag may be one `main`
      gained later, it adds "(`main`, for a release tagged there)", and for a branch headed there, whose
      line holds `main`'s tags already, it does not. It does not say where the commit sits, which the checker does not know, and never says git has no such
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
  - the review's acceptance cases A–J (2026-09-29, `R2196` in the self-test): no tag with `[Unreleased]`
    (1); 0.9.9 declared without its tag (2); the real 0.9.9 (0); 0.9.10 tagged and not recorded (2), also
    with 0.9.10 on `main` not yet merged (2, named as `main`'s with the merge remedy); 0.9.10 and 0.9.11
    missing (3); 0.9.10 recorded untagged below a tagged 0.9.11 (0), and with 0.9.11 not recorded,
    `[Unreleased]` from 0.9.10 or 0.9.9 (2 each, the base 0.9.11, never the untagged 0.9.10); every form of
    `[Unreleased]` over a missing 0.9.10 -- from 0.9.9, 0.9.10, 0.9.8, no version, no definition, no
    section -- refused and naming 0.9.10; an unrelated branch's higher tag (0); 0.9.10 tagged and
    recorded, with and without `[Unreleased]` from it (0);
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
      after that fetch it reads `{0.9.9}` and accepts. The test repositories are created on `main`;
    - the release line (2026-09-29, the finding's remaining path): `main` tags 0.9.9, a `feature` branch
      forks, `main` writes and tags 0.9.10, an unrelated `side` branch tags 0.9.11. On `feature`, the
      reader holds `{0.9.9, 0.9.10}` with 0.9.10 in `ahead` and 0.9.11 elsewhere, and the branch's
      `[Unreleased]` from 0.9.9 fails twice (0.9.10 missing, named as a release on `main`, and the base);
      from 0.9.10 with no entry it fails once; with `main` merged and 0.9.10 recorded it passes; `main`
      itself passes; the 0.9.9 tag commit, in `main`'s past, passes with 0.9.10 as its future; a
      single-branch clone of `feature`, which has no `main`, is unknown and refuses once, and after the
      remedy it names reads the line; a local `main` with an unpushed commit, behind a fetched
      `origin/main` that holds 0.9.10, is bound by it (2); a merged pull request's tip and its earlier
      commit are each bound by the 0.9.10 `main` had then and not by the 0.9.11 tagged after the merge
      (2 each, none naming 0.9.11); in a fork clone whose `origin` is the fork, the repository's own
      `main` under `upstream` binds the branch, with `upstream` at an https, `ssh.github.com:443`, ssh
      port, scp-style and `insteadOf`-alias URL, and not under another repository's URL, a local
      mirror path, a proxy, or a `#@` URL; a branch tip merging two `main` commits (0.9.9 and the one
      before the pull request's merge) is headed for `main` and names the 0.9.11 tagged after them; a
      tag name that is not UTF-8 is read, not fatal;
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
  against that release, not an older one. That holds on a branch too: a release `main` gained after the
  branch forked binds the branch at its tip, before it merges `main`, so a pull request checked after the
  release is tagged cannot pass with a changelog that `main` would refuse once merged; the remedy is to merge
  `main`. A pull request whose last check predates the tag keeps that green status: CI re-checks a branch
  only when it is pushed, so re-run its checks (or push) after a release, and `main`'s own `docs` job checks
  the merge. The cost is that every open branch whose file lacks a new release's entry fails until it
  merges `main`. A commit `main` never holds reads as headed for it, so it errs toward reporting: a
  squash- or rebase-merged pull request's own commits, and a fork pull request's test merge re-run after
  the pull request merged, are bound by releases tagged after the merge and may fail where `main` itself
  passes (`main`'s own check is the one that counts). A tag on a branch never
  merged into `main` is another line's release and raises nothing here; a maintenance line that merges back
  into `main` brings its tags into `main`'s history and so needs their entries, which is the history the
  merge asserts. A checkout without `main` cannot tell, and refuses what needs the tags.
- It verifies that a tag EXISTS, not that it is annotated: a lightweight `0.9.9` would count. A release tag
  is annotated because `release.yml` refuses anything else and drafts no Release for it.
- `check-docs.py --self-test` needs `git` to prove the reader; without it that case fails rather than
  passing unproved.

## Related code
- `scripts/check-docs.py` — `FIRST_TAGGED_VERSION`, `FIXTURE_FIRST_TAGGED_VERSION`, `first_tagged()`,
  `RELEASE_BRANCH`, `REPOSITORY_REMOTE`, `TagState` (`tags` on this line, `elsewhere`, `ahead`,
  `headed`, `unmerged`), `GIT_LOCATION_VARS`, `git_env()`, `read_git_tags()` (`git remote get-url`, `for-each-ref
  --merged`, `merge-base --is-ancestor`, `rev-list --first-parent` and `--ancestry-path` for HEAD's
  placement, a shallow clone or a checkout without `main` unknown), `RELEASE_TAG`, `tag_state()`, `check_changelog_links` (`releases`,
  `tagged`, `in_prep`, `verifiable`, `previous_of`, `newest_tagged`, the missing-entry finding), and the
  self-test's "repository's own first tag", "comparison base", "`[Unreleased]` exists only once a tag
  does", "every release tag needs its entry" and "tag reader" cases, with `fixture_tags()` giving each
  synthetic fixture the tags of the versions it records.
- `.github/workflows/build.yml` — the `docs` job's checkout, `fetch-depth: 0` (every branch, so
  `origin/main`) and `fetch-tags: true`.
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
- **[Verified]** The finding's remaining path, reproduced in real repositories with the production
  `read_git_tags` and `check_changelog_links`: on `482a2b0`, 0.9.9 and 0.9.10 both in `HEAD`'s history with
  no 0.9.10 entry failed (2), but 0.9.10 tagged on `main` after the branch forked passed at the branch's
  tip (0 findings) and failed on `main` after the merge (2). With the release line, the branch's tip fails
  (2), naming 0.9.10 as a release on `main` that the branch's history does not hold.
- **[Verified]** The checker: `python3 scripts/check-docs.py --self-test` passes, 638 cases (537 before the
  2026-09-29 amendments; each step of the tag-reader case now counts as a case of its own, so a mutant's
  count is the number of failing cases and steps alike). Fifty-six mutants of the 2026-09-29 rule each
  fail it, none by crashing, with the number of failing cases (measured on a clean clone of the final
  checker):
  - the bases from the tags that have an entry (the intersection, the finding's own repro): 21; the
    missing-entry finding removed: 36; the base taken from the newest entry with a tag-looking link: 33;
    `newest_tagged` taken from the releases the file records, before the missing ones are reconciled: 21;
  - `main`'s tags ignored for a branch headed there (the finding's remaining path): 12; `main`'s later
    tags counted on `main`'s own past: 1; a checkout without `main` read as known: 3; a release on `main`
    the branch lacks called "in this checkout's history": 4; every tag counted as `main`'s: 8; HEAD in
    ANY `main`'s history reading HEAD's history alone (a local `main` hiding `origin/main`'s release): 3;
    a merged pull request's commit read as `main`'s past: 2; bound by `main`'s later tags too: 2; its
    landing found by a first-parent-only ancestry walk (an earlier commit bound by nothing): 1; a merge
    `main` never holds whose parents are all on it bound as its parents are (`af4ac33`'s rule, which hid
    the later release at a branch tip merging two `main` commits): 1; only `origin` read (a fork's
    `upstream` ignored): 5; the raw configured URL matched (an `insteadOf` alias unresolved): 1; the URL
    pattern without `ssh.github.com` or a port: 2; unanchored (a mirror path matching): 3; a merged
    commit's later tag called absent from `main`: 1; git output decoded strictly: 1; the no-`main`
    remedy reverted to `git fetch origin main`: 1;
  - tags on another branch counted as this line's: 22; the no-tag `[Unreleased]` refusal removed,
    restoring the shape-only check: 22; a prefixed tag counted as a release: 4; a leading-zero tag
    counted as a release: 2;
  - `previous_of` set back to "the entry directly below": 26; drawn from the tagged entries only: 3;
  - a changelog definition taken as proof of a tag: 9; the tags ignored: 14; the first tag counted by
    fact: 9; `releases` without the not-older-than-the-first-tag clause: 4;
  - a malformed heading also reported as a missing release: 1;
  - a shallow clone read as known: 5; read as known when it reaches every release-shaped tag it lists
    (an earlier spelling): 3;
  - a definition on an untagged past version accepted: 9; a tagged version without one accepted: 3;
    the release in preparation required to be tagged already: 18; unreadable tags read as no tags: 11;
  - the inherited `GIT_DIR` kept in the reader: 1; the constant set back to (0, 9, 7): 92; a prefixed
    tag in the URL: 125; `[Unreleased]` from the newest entry whether tagged or not: 45;
  - missing releases reported oldest first: 1; comparing past a missing release explained as "the entry
    below closed without a tag": 1;
  - a tag outside `HEAD`'s history not named as such: in a definition, 1; in the `[Unreleased]` refusal
    ("git has no tag"), 1; in its definition ("a version with no git tag"), 1; in the no-base finding, 1;
    for the entry below a comparison ("closed without one"), 1; as the base of an `[Unreleased]`
    definition once a release is in this history, 2; as the base of a version definition, 1;
  - the remedy claiming the tagged commit is on `main`: 5; offered for a tag that can never be a base: 1;
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
