# ADR-0059 — Version references and release tags are bare

**Status:** **Accepted** (owner decision, 2026-09-28). The owner set the repository's version format and
asked for the release workflow and every tag-related policy to be brought into line with it. This record
enacts that as a Policy change (`ADR_POLICY.md` rule 5). It changes `RELEASE_POLICY.md` §Artifacts,
`CHANGELOG_POLICY.md` rule 8 and its template, and `RELEASE_PROCESS.md` §Tagging. Amended 2026-09-29:
the convention governs version *references*, not captured output of an older binary, which keeps its
exact bytes (§Decision).

## Context
- **The repository wrote versions in two ways.** The version itself was bare everywhere it is defined:
  - `CMakeLists.txt:14` (`project(Anamorph VERSION 0.9.9 ...)`);
  - every `CHANGELOG.md` heading (`## [0.9.9] — 2026-09-29`);
  - the About box (`ANAMORPH_VERSION_STRING`);
  - the release asset names (`Anamorph-0.9.9-Linux.zip`).

  Prose, comments, test output, worklog file names and the release tag carried a `v` prefix: 825 prefixed
  tokens in 79 files, and 28 file names.
- **The tag convention came from RH-PR-8, not from an ADR.** It was an annotated tag `v` + the CMake
  version:
  - `release.yml` triggered only on a tag of that prefixed shape;
  - it stripped the prefix before comparing the tag with the CMake version;
  - it created the draft release under the prefixed tag.

  `check-docs.py` required changelog links to name prefixed tag pages and prefixed comparisons. The two forms described the same number, so every reader had to
  translate between them, and every check had to know about the prefix to strip it.
- **No tag exists yet** (ADR-0058: the first will be 0.9.9). Changing the tag format now costs no
  published link.

## Problem
Choose one spelling for a version and use it for the tag as well, so the tag, the CMake version, the
changelog heading, its link and the prose all agree without translation.

## Options
- **A. Keep the prefix for tags only, and bare numbers elsewhere.** Rejected. It is the translation this
  decision removes, and it keeps two spellings of one fact.
- **B. Accept both tag forms.** Rejected. Two triggers and two link forms would let the same release exist
  under two names, and the checker could not tell which form a definition should use.
- **C. Bare everywhere, the tag included.** **Chosen.** The tag *is* the CMake project version.

## Decision
- **Every version reference is written `MAJOR.MINOR.PATCH` with no prefix**, e.g. `0.9.9`. This applies to
  documentation, scripts, the changelog, release records, policy, comments, tests, examples and file
  names. It is the permanent convention: a prefixed version is not introduced again.
- **The release tag is the bare version**: `git tag -a 0.9.9 -m "Anamorph 0.9.9"`.
  - `release.yml` triggers on `[0-9]+.[0-9]+.[0-9]+` only.
  - It asserts that the tag has that shape, including for a `workflow_dispatch` started from a tag ref.
  - It requires tag == CMake `project VERSION`.
  - It creates the draft release under that tag.
  - A prefixed tag matches no trigger and starts no release.
- **Changelog links name the bare tag**: `/releases/tag/0.9.9`, `/compare/0.9.9...0.9.10`, and
  `/compare/<last tag>...HEAD`. `check-docs.py` requires exactly that. A prefixed tag page or comparison
  is a finding, pinned by self-test cases that build the retired prefix from one constant (`PFX`), so no
  prefixed version is written out.
- **Existing text was normalized.**
  - 825 prefixed version tokens in 79 files were rewritten bare.
  - The prefixed placeholders and the prefixed tag glob were rewritten. Where the old text
    described what RH-PR-8 did at the time, it now says so ("prefixed at the time; the bare version since
    ADR-0059") rather than restating history in the new form.
  - 28 files were renamed, with every reference rewritten:
    - ADR-0058;
    - 23 worklogs, e.g. `STATE_HARNESS_0.8.13.md`;
    - four test files: the field capture `field_capture_0_9_5.session` and its manifest, the legacy
      fixture `legacy_0_2_bare_apvts.xml`, and its fuzz seed `legacy_0_2_bare_apvts.bin`.

    Their contents are unchanged, except the legacy fixture's header comment, which is repository prose:
    that fixture is a hand-modelled reconstruction (`worklogs/STATE_HARNESS_0.8.13.md` §2.3), not captured
    output, and the comment is not part of what it models. The sweep also rewrote the field capture's
    manifest label. That was wrong, and the label is restored (see "output captured from an older binary"
    below).
  - A later sweep caught what a word-boundary search had missed: a prefix after `_` or `…`
    (`…_0.9.8.md` in ADR-0046) and the symbolic prefixed "N" / "N−1" placeholders, now "version N" / "N−1".
- **Preserved, deliberately: third-party identifiers whose spelling is the upstream's own.**
  - the version comment after each pinned GitHub Action SHA, which Dependabot maintains as the upstream
    tag name;
  - the upstream release name of `microsoft/msvc-code-analysis-action`, in `msvc.yml` and
    `dependabot.yml`;
  - prose that names those upstream tags: `CI_CD.md` on mutable action tags, and the action bumps that
    `DOCUMENTATION_COVERAGE.md`, the 0.9.8 JUCE-upgrade worklog and the engineering-review programme record.

  Writing them bare would name tags that do not exist upstream. They are references to other projects'
  versions, not to this one's.
- **Preserved, deliberately: output captured from an older binary** (amended 2026-09-29, from a review
  finding). A fixture that records what a historical Anamorph binary wrote is evidence, not a version
  reference, so it keeps its exact bytes whatever notation it uses. The tree holds one such capture,
  `tests/fixtures/field_capture_0_9_5.session` and its `.manifest`; only the manifest carries a version
  token.
  - Its first line, `emitter=v0.9.5`, is the label the rebuilt 0.9.5 binary wrote (committed in `72fe2e0`),
    in the prefixed spelling the project used then.
  - The sweep rewrote it to the bare form and changed State test 25's assertion to match, so the test
    checked a rewritten record instead of the old binary's output.
  - The manifest is restored byte for byte from `72fe2e0`. Its file *name* follows the convention: the name
    is repository metadata, not captured output.
  - State test 25 pins both capture files by content hash (FNV-1a-64 of the raw bytes) and asserts the
    label as written. A rewrite of either file fails it, a repository-wide sweep included.
    `.gitattributes` marks both files `-text`, so no checkout converts the manifest's CRLF line endings.
  - Any later capture of an older binary's output is treated the same way.

## Consequences
- The first tag is pushed as `0.9.9`. A tag pushed with the old prefix by habit starts no release; it
  should be deleted and the tag re-pushed bare.
- A link from outside the repository to one of the 28 old file names no longer resolves. Every link
  inside the repository was rewritten.
- The citation gate saw one cited source span change text: the comments of `decodeRestore`'s
  bare-APVTS branch. That anchor was re-read and re-spelled with a declared re-aim (`check-citations.py`
  `DELIBERATE_REAIMS`). No code line changed.
- The convention is machine-checked where it carries release meaning: `release.yml` accepts only a bare
  tag, and `check-docs.py` accepts only bare links. Elsewhere it is kept by review, the same way the other
  wording conventions are.

## Related code
- `.github/workflows/release.yml` — the trigger pattern, the tag-shape assertion, `TAG="${VERSION}"`, and
  `gh release create "${VERSION}"`.
- `scripts/check-docs.py` — `check_changelog_links` (`tag = key`), and the `PFX` self-test cases.
- `CHANGELOG.md` — the `[0.9.9]` definition and the preamble.
- `scripts/check-citations.py` — one `DELIBERATE_REAIMS` entry.

## Evidence + confidence
- **[Verified]** `python3 scripts/check-docs.py --self-test`: 561 cases pass. They include a prefixed tag
  page, a prefixed comparison and a prefixed 0.9.9 tag page, each refused, and a prefixed git tag, which
  is not the version's tag (so it makes no comparison base and no `[Unreleased]` base, and a prefixed
  higher tag needs no changelog entry). With the prefix restored in `check_changelog_links`, 101 cases
  fail, and the real `CHANGELOG.md` is refused; with a prefixed git tag counted as a release, 4 fail.
  (Re-measured 2026-09-29 after the checker began requiring an entry for every release tag in `HEAD`'s
  history, ADR-0058; first recorded as 492 and 51, then 511 and 65, then 537, 79 and 2.)
- **[Verified]** The tag-shape test from `release.yml`, run in bash: `0.9.9` matches; the prefixed form and
  `0.9.9x` are refused.
- **[Verified]** A repository-wide search for a prefixed version token outside the preserved third-party
  identifiers above returns nothing, apart from the 0.9.5 capture's label and the places that quote it
  (State test 25, this record, `ADR_INDEX.md`, `REPOSITORY_MAP.md` and `DOCUMENTATION_COVERAGE.md`), and
  the capture's pre-rename path where it names the historical git object
  (`72fe2e0:tests/fixtures/field_capture_v0_9_5.session.manifest`).
- **[Verified]** The restored manifest is byte-identical to the file `72fe2e0` committed
  (`git show 72fe2e0:tests/fixtures/field_capture_v0_9_5.session.manifest | cmp -`), and so is the session
  blob, which the sweep did not change. State test 25's controls, each run through the whole State suite:
  - the manifest's label rewritten to the bare form: 2 failures (the manifest hash, the label);
  - the label assertion relaxed to the bare form, with the fixture rewritten to match, which is what the
    sweep did: 1 failure (the manifest hash);
  - the label assertion relaxed, with the historical fixture: 1 failure (the label);
  - a slot value nudged inside the 1e-5 tolerance the value checks use, a blob byte the restore does not
    read, or the manifest converted to LF line endings: 1 failure each (the hash).
- **[Verified]** No tag or release exists to be re-pointed (`git tag -l`; the tags and releases API,
  2026-09-28).
