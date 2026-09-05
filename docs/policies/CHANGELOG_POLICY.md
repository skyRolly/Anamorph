# CHANGELOG_POLICY.md

Repository Governance Policy. How `CHANGELOG.md` is maintained.

**Format authority: [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/).** Read it before
creating or materially editing an entry — the current published spec, not what you remember of it —
and follow it unless a rule below deliberately overrides it (rule 2 does, on Evidence Sources; rule 3
does, on what is notable enough to record). Where the two agree, the spec's wording governs.

## Rules

1. **Format: Keep a Changelog.** Sections per version: Added / Changed / Fixed / Removed /
   Deprecated / Security, newest first. Semantic-ish `MAJOR.MINOR.PATCH` (pre-1.0 line).
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
7. **Latest version first; every entry carries an ISO `YYYY-MM-DD` release date** in its `## [x.y.z]`
   heading (`release.yml` fails closed on an undated heading at tag time). An `## [Unreleased]`
   section, if used, sits above the first entry — never between two releases.
8. **Version headings are linkable.** The bracketed version is a link reference; add its definition
   at the foot of the file when the tag is cut (`RELEASE_PROCESS.md` §Tagging). Until this line's
   first tag exists there is nothing to point at, so the definitions are absent by design rather than
   broken — do not invent a URL for a tag that has not been pushed.

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

```
## [0.8.7] — Fixed
- <user-visible change>.
  Evidence: commit 6a24b82 (or PR #NN). [Verified | Partially Verified | Unverified Historical Reconstruction]
```

## Source of truth for history

Commit messages + PRs are primary; the README "What's new" sections are corroborating
(Partially Verified) but not authoritative on their own. When reconstructing pre-current
versions, prefer the commit that introduced the change.
