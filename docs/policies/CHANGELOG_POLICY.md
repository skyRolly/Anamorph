# CHANGELOG_POLICY.md

Repository Governance Policy. How `CHANGELOG.md` is maintained.

## Rules

1. **Format: Keep a Changelog.** Sections per version: Added / Changed / Fixed / Removed /
   Deprecated / Security, newest first. Semantic-ish `MAJOR.MINOR.PATCH` (pre-1.0 line).
2. **No invented history.** Never infer that a past version contained a feature by reasoning
   backward from current code. Each entry cites an **Evidence Source** — a commit SHA, commit
   range, or PR (entries up to `[0.8.12]` predate git tags, so a release tag alone was never
   available as evidence for them; from the first annotated release tag — `v0.9.0` — onward the
   tag is also citable). An entry that cannot be tied to such evidence is marked
   `[Unverified Historical Reconstruction]`.
3. **User-visible changes only.** Refactors, cleanups, formatting, and renames are **not**
   changelog entries **unless** a PR/commit explicitly states a user-visible impact.
4. **Renames are Changed, not Removed.** A display-name change with an unchanged ID (e.g. `Haas
   Side`→`Haas Focus`) is a "Changed" entry; it is **not** a parameter removal.
5. **Compatibility-affecting entries cross-link** the relevant ADR and note any migration.

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
