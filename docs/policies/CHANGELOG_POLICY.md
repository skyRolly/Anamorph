# CHANGELOG_POLICY.md

Repository Governance Policy. How `CHANGELOG.md` is maintained.

**Format authority: [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/).** Read it before
creating or materially editing an entry — the current published spec, not what you remember of it —
and follow it unless a rule below deliberately overrides it (rule 2 does, on Evidence Sources; rule 3
does, on what is notable enough to record). Where the two agree, the spec's wording governs.

## Rules

1. **Format: Keep a Changelog.** Sections per version, in this order: Added / Changed /
   Deprecated / Removed / Fixed / Security — the specification's own order, and the order rule 6
   enforces. (Versions themselves run newest first; that is rule 7.) This line read
   `Added / Changed / Fixed / Removed / Deprecated / Security` until 2026-09-05, which is where
   the file's misordered entries came from. `MAJOR.MINOR.PATCH` per
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
   reads `— Unreleased` or has no date is not a release and is rejected. A heading that only
   **reads** as a release — one that lost a bracket, or names a version at another heading level —
   is held to the same grammar rather than passing as prose (§The structural grammar,
   restriction 3). `check-docs.py` enforces all of this on every push; `release.yml` re-checks the tagged version's heading and date at tag
   time. Two reconstructed headings at the foot predate this policy and are accepted by
   their exact text — `## [0.7.5] – [0.7.0] — 2026-06-21…22` and
   `## [0.6.x] and earlier — 2026-06 (reconstructed)`; no new heading may take either form.
8. **Version headings are linkable.** The bracketed version is a link reference, and from `0.9.7` —
   the first version this line tags — every one has a definition at the foot of the file, into this
   repository (`https://github.com/skyRolly/Anamorph`), naming its own tag:
   `/releases/tag/v0.9.7` for that first tag, which has no predecessor to compare against, and
   `/compare/v<the previous release>...v<x.y.z>` for every version after it — the previous release
   being the next-older entry, which in a newest-first file is the one directly **below** it. An `[Unreleased]`
   section's definition is `/compare/v<last tag>...HEAD`. The definition is written **in the release
   commit**, before the tag exists, because a tag can only point at a commit that already does
   (`RELEASE_PROCESS.md` §Tagging gives the sequence); the name is deterministic, `release.yml`
   refusing any tag other than `v` + the CMake version. Versions older than `0.9.7` were never tagged
   and must have no definition — there is no page to link. `check-docs.py` requires exactly the form
   the version calls for, in both directions.

## The structural grammar

Two programs read `CHANGELOG.md` and must never disagree about it: `scripts/check-docs.py`
gates every push, and `scripts/changelog-section.awk` extracts the release notes that
`release.yml` publishes (it runs the extractor twice — once in `validate`, once in
`draft-release` — so there is one implementation, not two). Each defect this section
names was a place where they disagreed, or where one of them disagreed with the renderer.

**Authority.** [CommonMark](https://spec.commonmark.org/) decides what a line *is* — both
tools implement it and neither invents. This policy decides which of those forms
`CHANGELOG.md` may *use*. `check-docs.py` is the gate: it is the only stage that reads the
whole file on every push, and every restriction below is machine-checked there.

**Where each rule comes from.** Three kinds, and the distinction is not decoration: a
CommonMark-derived rule is not ours to relax, a workflow-specific one changes only if
`release.yml` changes, and a project-restricted one is a choice this policy made and could
unmake. Nothing below is a CommonMark requirement dressed up as a project rule, or the
reverse.

- **CommonMark-derived** — what a line *is*. Heading levels and the closing-`#` run, the
  0–3 column allowance, fence delimiters and their info strings, indented code blocks,
  setext underlines, link reference definitions. Both tools implement these; neither
  invents.
- **Workflow-specific** — required because `release.yml` boundaries a release's notes on
  the literal `^## [`, at column 0, and on nothing else. Restrictions 1, 2, 3 and 5.
- **Project-restricted** — stricter than CommonMark, and stricter than the workflow needs.
  Restrictions 4 and 6, and the container rule below.

**The restricted subset.** Ordinary CommonMark everywhere, with six restrictions:

| # | Restriction | Kind | Why |
|---|---|---|---|
| 1 | An entry heading is level 2, at **column 0**, written `## [` with exactly one space | workflow | the only form the extractor's `^## \[` matches. `##\t[`, `##  [` and an indented `## [` all render as entry headings and none of them can be published |
| 2 | Below the first entry, every level-1 and level-2 heading is an entry heading | workflow | anything else does not terminate the entry above it, so it is published inside that release's notes |
| 3 | A heading that **names a release** — a version number (with or without a leading `v`), or `Unreleased` — must be a valid entry heading, at any level and at any indent. At level 2 the **bracket** is reserved as well | workflow | a bracket lost from `## [0.9.7] — …` used to make it preamble: its categories were charged to the release above it. A release heading the extractor cannot reach is not a release |
| 4 | Category headings are level 3, **at column 0**, one of the six names, in the specification's order, once each per release | project | rule 6. CommonMark allows 0–3 columns and GitHub renders all four identically; requiring column 0 keeps one rule for every heading in the file and stops the drift toward the four-column case, which IS a bypass |
| 5 | Headings are ATX (`##`), never setext (text over `---`) | workflow | the extractor cannot see a setext heading, and neither can `^## \[` |
| 6 | An entry heading is never written inside a fenced code block as a live heading | project | a fence is data; the extractor and the checker both skip it, so a heading there is a sample and nothing more |

**Line endings.** `\n` or `\r\n`. A **lone** carriage return is a line ending to CommonMark
and not to `awk`, so the two tools would split the file differently; both follow `awk`, and
`check-docs.py` reports the character rather than resolving the disagreement silently.

**Container markers: forbidden, not half-supported.** A heading inside a block quote or a
list item (`> ### Fixed`, `- ## [0.9.7] — …`, `> [0.9.7]` over `> -------`) still renders as
a heading, and `release.yml`'s `^## \[` cannot see through the marker. The decision, taken
once and stated here: a release heading inside a container is **forbidden structure**,
reported as such, and recorded as a malformed entry so that what follows it is charged to it
and not to the release above. Supporting it instead would mean changing the extractor's
boundary, which changes what every release publishes; half-supporting it — the checker
seeing one thing and the extractor another — is what this whole section exists to prevent.
A category behind a marker is reported and still counted, so the order and uniqueness rules
see it. Ordinary quoted prose is untouched: only a Keep a Changelog category name or a
release-naming heading is reported.

**Forbidden is not ignored, and this is why the markers are normalised once.**
`strip_containers` removes the container prefix and hands what is left to the SAME functions
that read a top-level line — `atx_heading` for ATX, `SETEXT_UNDERLINE` and
`interrupts_paragraph` for setext. A container does not change what a heading *is*; it
changes the column its content starts at. Two container kinds are counted differently
because CommonMark treats them differently: a `>` takes at most **one** space with it (§5.1),
so `>  ## [0.9.7]` leaves one column of content indent and is still a heading — and the
blockquote **depth**, not a column, is what pairs a setext underline with its subject
(`> [0.9.7]` over `>-------` is one heading; depth 1 under depth 2 is none). A **list**
marker establishes a content column instead, which a continuation line is indented to
(`- [0.9.7]` over `  -------` is a heading; `- foo` over `- ---` is two list items). Both
heading forms, ATX and setext, go through that one normalisation, and the boundaries were
verified against a CommonMark renderer rather than reasoned about.

**Where a heading SITS is half of what it is.** `classify_heading` answers both questions
at once — the level and text, and whether the line is at column 0, indented 1–3 columns,
indented 4 or more, or behind a container marker — and every rule decides from the pair.
There is no second opinion available: three separate paths used to answer these questions
differently, and every gap between them was a bypass (a container-prefixed release heading
was invisible unless an entry already existed above it; a deep release heading was tested
for a leading bracket where the column-0 path tested for a release name; a category
indented one to three columns reached the category list without anyone looking at its
indent).

**`[Unreleased]` and the reconstructed footer.** `## [Unreleased]` appears **at most once**,
and when it appears it is the **first** entry — two distinct invariants, each with its own
diagnostic, because a second section reported as "must be the first entry" tells the author
to move it to the top and make the file worse. The two grandfathered reconstructed headings
appear **at most once each** and in **their own order** (`[0.7.5] – [0.7.0]` above
`[0.6.x] and earlier`): they are release entries in a newest-first file, so rule 7's
ordering governs them as it governs every other entry. Their *presence* is not required —
a file without them is well-formed.

**Fenced code blocks** follow CommonMark §4.5 exactly, in both tools: three backticks or
three tildes minimum, at most **three columns** of indent (one tab is four columns, so a
tab-indented delimiter is an indented code block and opens nothing), a closer of the same
character, at least as long, with nothing but whitespace after it — and a backtick fence's
info string may not contain a backtick, which makes ` ```a`b ` a paragraph rather than a
fence, and a closer may be followed by spaces and tabs only (not by any other Unicode
whitespace — that difference alone made the two tools disagree about where a release ends).
Content inside a fence is never read as changelog structure; a line that is not actually a
fence never hides changelog structure. One exception is stated rather than hidden: a fence
nested inside a list item has its delimiters four or more columns from column 0, where
neither tool can see them without a container stack, so the deep-heading rule is silenced
between two such delimiters — a sample is not a defect.

**Where the two tools deliberately differ from the renderer**, they differ in one
direction only: `check-docs.py` may see structure the renderer treats as an indented code
block (a `### Fixed` indented four columns with no blank line above it), and reports it.
It never sees *less* structure than the renderer — that direction is the bypass the rules
exist to close, and the property is asserted over the grammar matrix in `--self-test`.

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

`<...>` marks a placeholder and `<a|b>` a choice between the spellings inside it; everything else is
literal. Only the categories that have entries appear, and in this order. This is the shape
`check-docs.py` accepts for a release that has a predecessor; the very first tag takes the other
link form, immediately below.

```markdown
## [<x.y.z>] — <YYYY-MM-DD>
### Added
- **<What is new, as the user meets it>.** <One or two sentences on what it does.>
  Evidence: <PR #NN|commit sha>. [Verified]
### Changed
- **<What behaves differently>.** <What it did; what it does now; what the user notices.>
  Evidence: <PR #NN|commit sha>. [<Verified|Partially Verified|Unverified Historical Reconstruction>]
### Fixed
- **<What went wrong, in the user's terms, and no longer does>.** <Cause in one sentence, if it helps.>
  Evidence: <PR #NN|commit sha>. [Verified]

[<x.y.z>]: https://github.com/skyRolly/Anamorph/compare/v<previous version>...v<x.y.z>
```

The definition line belongs with the others at the **foot of the file**, not under the entry — it is
shown here so the template is complete.

The link definition has one exception, and it is the next release: `v0.9.7` is the line's **first**
tag, so it has no predecessor to compare against and its definition is
`[0.9.7]: https://github.com/skyRolly/Anamorph/releases/tag/v0.9.7`. Every version after it uses the
comparison form shown above, against the release before it — the entry directly **below** it in this
newest-first file. `check-docs.py` requires exactly the form the version calls for, and rejects the
other one.

Work not yet released goes under `## [Unreleased]` in the same shape, with
`[Unreleased]: https://github.com/skyRolly/Anamorph/compare/v<last tag>...HEAD`; the release commit
renames the heading to `## [<x.y.z>] — <YYYY-MM-DD>` and re-points the definition (rules 7 and 8).
That definition needs a tag to compare against, so an `## [Unreleased]` section is available from
`v0.9.7` onward — until then unreleased work simply sits in the dated entry it will ship in, which
is how every entry in this file was written.

## Source of truth for history

Commit messages + PRs are primary; the README "What's new" sections are corroborating
(Partially Verified) but not authoritative on their own. When reconstructing pre-current
versions, prefer the commit that introduced the change.
