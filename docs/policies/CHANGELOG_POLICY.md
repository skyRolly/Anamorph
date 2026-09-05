# CHANGELOG_POLICY.md

Repository Governance Policy. How `CHANGELOG.md` is maintained.

**Format authority: [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/).** Read it before
creating or materially editing an entry — the current published spec, not what you remember of it —
and follow it unless a rule below deliberately overrides it (rule 2 does, on Evidence Sources; rule 3
does, on what is notable enough to record). Where the two agree, the spec's wording governs.

## Rules

1. **Format: Keep a Changelog.** Sections per version: Added / Changed / Deprecated / Removed /
   Fixed / Security, newest first — the specification's own order, and the order rule 6 enforces.
   (This line read `Added / Changed / Fixed / Removed / Deprecated / Security` until 2026-09-05,
   which is where the file's misordered entries came from.) `MAJOR.MINOR.PATCH` per
   [Semantic Versioning](https://semver.org/): pre-1.0, so a `0.y.z` release may change behaviour.
2. **No invented history.** Never infer that a past version contained a feature by reasoning
   backward from current code. Each entry cites an **Evidence Source** — a commit SHA, commit
   range, or PR (entries up to `[0.8.12]` predate git tags, so a release tag alone was never
   available as evidence for them; from the first annotated release tag — **`v0.9.7`** (0.9.0 through 0.9.6 were each written up but never tagged) — onward the
   tag is also citable). An entry that cannot be tied to such evidence is marked
   `[Unverified Historical Reconstruction]`.
3. **User-visible changes only.** Refactors, cleanups, formatting, and renames are **not**
   changelog entries **unless** a PR/commit explicitly states a user-visible impact.
4. **Renames are Changed, not Removed.** A display-name change with an unchanged ID (e.g. `Haas
   Side`→`Haas Focus`) is a "Changed" entry; it is **not** a parameter removal.
5. **Compatibility-affecting entries cross-link** the relevant ADR and note any migration.
6. **The six categories, in the spec's order, once each per release.** `### Added`, `### Changed`,
   `### Deprecated`, `### Removed`, `### Fixed`, `### Security` — that order, no others invented, no
   category split across two sections. A release-level note that is not a change (a compatibility
   statement, a known issue) goes in the entry's lead, above the first category, not in a category of
   its own. `check-docs.py` enforces the name, the order and the once-each rule; it cannot judge
   whether a bullet is in the right category, and that stays with the author.
7. **Latest version first; every version heading carries an ISO `YYYY-MM-DD` release date.** The
   heading grammar is `## [x.y.z] — YYYY-MM-DD` (a plain `-` for the dash is accepted, being the
   specification's own spelling; ` [YANKED]` may follow), at column 0, versions strictly decreasing
   down the file, the date a real calendar date. Work that is not yet released goes under
   `## [Unreleased]`, which sits above the first version and nowhere else — a version heading that
   reads `— Unreleased` or has no date is not a release and is rejected. `check-docs.py` enforces
   all of this on every push; `release.yml` re-checks the tagged version's heading and date at tag
   time. Two reconstructed headings at the foot, `[0.7.5] – [0.7.0]` and `[0.6.x] and earlier`,
   predate this policy and are accepted by exact text; no new heading may take that form.
8. **Version headings are linkable.** The bracketed version is a link reference, and from `0.9.7` —
   the first version this line tags — every one has a definition at the foot of the file naming its
   own tag: `.../releases/tag/v<x.y.z>` for the first tag, `.../compare/v<previous>...v<x.y.z>` after
   it. The definition is written **in the release commit**, before the tag exists, because a tag can
   only point at a commit that already does (`RELEASE_PROCESS.md` §Tagging gives the sequence); the
   name is deterministic, `release.yml` refusing any tag other than `v` + the CMake version. Versions
   older than `0.9.7` were never tagged and must have no definition — there is no page to link.
   `check-docs.py` enforces both directions and the URL form.

## Writing an entry

Establish the facts from the repository, then write for the reader:

- **Audit before writing.** Read the commits, merged PRs and source changes between the previous
  release and this one. An entry asserts something about the product; the diff is what makes it true.
- **The git log is not the changelog.** One notable change usually spans several commits, its tests
  and its documentation. Record it once, at the level a user experiences it — never a commit-by-commit
  transcript, and never a `Fixed` bullet for a fix that only ever existed in an unreleased branch.
- **Never invent a change**, a date, an evidence source or a version link. Nothing goes in that the
  repository does not support (rule 2), and nothing already published is silently removed.
- **Separate user-visible from internal.** Internal refactors, CI plumbing, formatting and doc churn
  are not entries (rule 3). Say what changed for someone using the plug-in; keep the mechanism to the
  ADR the entry cross-links.
- **Correct minimally.** When fixing an existing entry, change the wrong fact, the wrong category or
  the wrong structure — not the prose around it. A stylistic rewrite of an accurate entry destroys
  the record's continuity for no gain.

## Entry template

Angle brackets mark placeholders; everything else is literal. Only the categories that have entries
appear, in this order, once each. The template is the shape `check-docs.py` accepts.

```markdown
## [<x.y.z>] — <YYYY-MM-DD>
### Added
- **<What is new, as the user meets it>.** <One or two sentences on what it does.>
  Evidence: PR #<NN> (or commit <sha>). [Verified]
### Changed
- **<What behaves differently>.** <What it did; what it does now; what the user notices.>
  Evidence: PR #<NN>. [Verified]
### Fixed
- **<What went wrong, in the user's terms, and no longer does>.** <Cause in one sentence, if it helps.>
  Evidence: PR #<NN>. [Verified | Partially Verified | Unverified Historical Reconstruction]

[<x.y.z>]: https://github.com/skyRolly/Anamorph/compare/v<previous>...v<x.y.z>
```

Work not yet released goes under `## [Unreleased]` in the same shape, with
`[Unreleased]: https://github.com/skyRolly/Anamorph/compare/v<last tag>...HEAD`; the release commit
renames the heading to `## [<x.y.z>] — <date>` and re-points the definition (rules 7 and 8).

## Source of truth for history

Commit messages + PRs are primary; the README "What's new" sections are corroborating
(Partially Verified) but not authoritative on their own. When reconstructing pre-current
versions, prefer the commit that introduced the change.
