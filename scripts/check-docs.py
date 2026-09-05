#!/usr/bin/env python3
"""Structural lint for the Anamorph documentation set.

PROVENANCE: adopted verbatim from the sibling product Anabasis
(`scripts/check-docs.py`) apart from this paragraph and the product name above.
The first four checks below are structural properties of GitHub-Flavored Markdown,
not of either product, so there was nothing to adapt -- and a diverged copy of a
checker is worse than a shared one. The CHANGELOG rules (5) are this repository's
own, and are shared with `release.yml`'s extractor rather than with the sibling. The defects each check names happened in
Anabasis; they are reproducible in any document set written the same way, which
this one is (same directory layout, same navigation documents, same ADR index).


Five checks, all mechanical and deterministic. Each exists because the defect it
catches shipped at least once in this repository and was invisible in the source
diff that introduced it:

  1. TABLE INTEGRITY -- a blockquote or paragraph inserted in the middle of a
     GitHub-Flavored Markdown table terminates it. The rows after the intrusion
     render as pipe-separated text with no header, outside the table their own
     header governs. This happened to THREADING_POLICY.md's permitted-path table:
     three of the seven binding cross-thread rules stopped being rules. A table
     cannot resume after an intervening block in GFM, so any run of pipe-prefixed
     lines whose second line is not a separator (`|---|`) is either an orphaned
     fragment or a headerless table.

  2. RELATIVE LINKS -- a moved or renamed file silently breaks every pointer to
     it. The docs are a navigation system (SOURCE_OF_TRUTH.md, REPOSITORY_MAP.md,
     the ADR index); a dead link there is a reader who does not reach a binding
     record.

  3. BLOCKQUOTE LAZY CONTINUATION -- an unquoted line directly after a `>` line
     is absorbed into the quote by CommonMark's lazy-continuation rule. This
     happened to ADR-0011: two sentences of binding contract rendered as part of a
     historical correction note, which a reader could reasonably skip.

  5. CHANGELOG STRUCTURE (`CHANGELOG.md` only, four checks that share one parser)
     -- the entry-boundary rule `release.yml`'s note extractor depends on, the
     entry-heading grammar and newest-first order, Keep a Changelog's six category
     names in their specified order once each per release, and the version link
     definitions. See `parse_changelog` and the three `check_changelog_*` rules
     below it; the contract they enforce is `docs/policies/CHANGELOG_POLICY.md`.

  4. UNCLOSED FENCE -- an opening code fence with no closer makes the rest of the
     file render as code on GitHub. It is a real rendering defect on its own, and
     it is also this script's worst failure mode: an unclosed fence exempts every
     line after it from checks 1-3. That happened here -- a prose line in
     DOCUMENTATION_COVERAGE.md began with three backticks, masking 1382 of its
     1401 lines while the run still printed "clean". A checker that reports
     success without having read the file is worse than no checker, so an
     unterminated fence is now a finding rather than a silent exemption.

FALSE POSITIVES ARE THE OTHER FAILURE MODE THAT MATTERS. A lint that invents
findings gets ignored, and the real ones are lost with it -- so each check is
scoped to what it can actually prove:

  * Fenced code blocks, **indented** code blocks (CommonMark's other form: four
    or more columns, preceded by a blank line, outside any list container) and
    inline code spans are all excluded from checks 1-3. A document that shows
    table syntax, a link, or quote syntax as an *example* is not making a claim
    about its own structure, in whichever form it shows it. Indentation is
    measured in **columns**, so one tab counts as four and `"  \\t"` also
    reaches four -- counting characters instead let a tab-indented example
    through as if it were structure.
  * Lazy continuation is only reported where CommonMark applies it: to *paragraph
    continuation text*. A quote ending in a blank `>` has closed its paragraph,
    and a line starting a new block -- heading, fence, list, table row, thematic
    break, HTML -- interrupts rather than continues. `interrupts_paragraph()`
    encodes those cases, including CommonMark's rule that an ordered list
    interrupts a paragraph only when it starts at 1.
  * Link destinations are parsed, not string-sliced: `[t](path "Title")`,
    `[t](<path with spaces>)`, percent-encoded paths, and destinations or titles
    containing parentheses (`[t](a(1).md)`) are all valid and none is truncated.
  * A file that is not valid UTF-8 is reported as a finding in the usual
    `path:line:` form rather than raised as a traceback, so the job's output
    contract holds for every failure it can produce.

KNOWN LIMITS, stated rather than implied (constraint C7):
  * Indentation is stripped before block matching, and a table row is matched at
    any indent. CommonMark measures indent against the *container's* content
    column and a line-based lint has no container stack; anchoring at column 0
    produced 31 false positives inside numbered ADR items, and GFM's own
    three-space rule would silently skip any table nested deeper than that.
    This trades a few false negatives for no false positives.
  * A fence written *inside* a blockquote (`> ` + backticks) does not open the
    mask, because `FENCE` does not look through the quote marker. Harmless while
    the quoted block's own lines stay quoted -- the table and lazy-continuation
    checks both ignore them -- but a fenced *example* inside a prescribed policy
    block whose inner lines are not quoted would be examined as if it were
    structure. No such block exists today.
  * **Tables written without a leading pipe are not checked at all.** GFM accepts
    `A | B` / `---|---`, but `TABLE_ROW` requires the pipe to be the first
    non-whitespace character, so such a table -- including a mid-table intrusion
    in one -- is invisible to check 1. The same applies to a single row that
    omits its leading pipe inside an otherwise piped table (a `---|---` delimiter
    under a `| A | B |` header breaks the run and misreports the header as a
    fragment). Matching the pipeless form would mean treating any prose line
    containing a `|` as a candidate table row, which is the false-positive
    direction this script refuses. No table in this corpus is written that way;
    the repository's convention is leading pipes throughout, on every row. A
    *trailing* pipe, by contrast, is genuinely optional and `SEPARATOR` accepts
    its absence.
  * **Blockquoted tables are not checked at all.** `TABLE_ROW` requires the pipe
    to be the first non-whitespace character, so the table rows the ADRs carry
    inside prescribed policy blocks are invisible to check 1. That is deliberate:
    a prescribed block often quotes a *single* row (ADR-0011's new permitted-path
    row is one), which has no separator and would be reported as a fragment. The
    enacted copy of every such table is checked in the policy file itself, so the
    coverage loss is a duplicate -- but a mid-table intrusion occurring only
    inside a prescribed block would go unseen.
  * An indented code block *inside* a list item is not masked, because at that
    indent a line is far more likely to be a nested table than a code block and
    the two are indistinguishable without a container stack. Erring toward
    checking is only safe here because such a block would have to be indented
    four columns beyond its container's content column, which does not occur in
    this corpus.
  * An indented code block at the very **first** line of a file is not masked
    (the mask requires a preceding blank line). CommonMark does not need one
    there; no file in this corpus opens that way.
  * `FENCE` still measures its own three-column allowance in characters, so a
    tab-indented ` ``` ` is read as a fence opener where CommonMark would call it
    indented code. Both readings mask the block, so no finding differs -- except
    that an *unpaired* tab-indented fence line would be reported as an unclosed
    fence. Left as is deliberately: tightening it would trade this narrow case
    for the risk of a spurious unclosed-fence report, which is the louder failure.
  * Link existence is checked against the filesystem, so on a case-insensitive
    filesystem (macOS) a case-mismatched path passes here and 404s on GitHub.
    Root-relative destinations (`/docs/x.md`) resolve against the **repository**
    root -- the script's own parent's parent -- even when the scan is pointed at
    a subtree, so `check-docs.py docs/policies` still resolves them repo-wide.
  * Reference-style links (`[t][ref]`) and autolinks are not checked.

Deliberately NOT checked: whether each ADR-prescribed policy block matches the
enacted policy text. That comparison is real and is run by hand on every
documentation pass, but it has known cosmetic artefacts -- the policy bolds each
invariant's opening sentence as a headline and stamps a dated attribution that the
prescribing ADR does not carry (see ADR-0003's "scope of verbatim" note). Encoding
those exceptions as an allowlist would make the script assert more than it can
check, which is the failure mode constraint C7 exists to prevent.

Usage:  scripts/check-docs.py [path ...]     (default: the repository root)
        scripts/check-docs.py --self-test
Exit:   0 = clean, 1 = findings printed to stderr.
"""

from __future__ import annotations

import contextlib
import datetime
import io
import re
import sys
import tempfile
from pathlib import Path
from urllib.parse import unquote

# GFM's delimiter row: leading pipe guaranteed here because TABLE_ROW required
# it to open the run, TRAILING pipe optional (`|---|---` is as valid as
# `|---|---|`), and at least one dash required -- a whitespace-only row like
# `|   |` is a data row, not a delimiter, and accepting it as one hid a real
# headerless-table defect behind a false negative. Looser than cmark-gfm in one
# tolerated direction: cells with internal spaces (`|- - -|`) are accepted as
# delimiters here and rejected there -- a false negative, never a false positive.
SEPARATOR = re.compile(r"^\|[\s:|-]*-[\s:|-]*$")
LINK_OPEN = re.compile(r"\[[^\]]*\]\(")
FENCE = re.compile(r"^\s{0,3}(`{3,}|~{3,})(.*)$")
TABLE_ROW = re.compile(r"^\s*\|")
# An entry-boundary heading, spelled to match THIS repository's extractor rather
# than the sibling's. `release.yml` here ends a release's notes at the next
# `^## \[` -- not at any `^## ` -- so the bracket, not a three-part version
# number, is what actually terminates a section. Requiring `\d+\.\d+\.\d+` (the
# sibling's spelling, whose extractor does stop at any h2) would report
# `## [0.6.x] and earlier` as a defect while the pipeline handles it correctly:
# a checker that disagrees with the thing it checks teaches people to ignore it.
# If the extractor is ever tightened to `^## `, tighten this in the same change.
CHANGELOG_VERSION_HEADING = re.compile(r"^## \[")
# `_deps` is FetchContent's cache, and it is listed by name rather than left to
# the build-tree rule below because `.gitignore` allows one at the repository
# ROOT (`_deps/`) as well as the usual `build/_deps/`. Only the second has a
# `build*` ancestor, so the top-level cache was walked and JUCE's own Markdown
# reported as findings. `check-clang-warnings.py` already treats `_deps` as the
# vendored marker; this is the same convention in the other scanner.
SKIP_DIRS = {".git", "node_modules", "JUCE", "_deps"}

# …and every build tree. This set said only the exact name `build`, so a
# checkout that followed the documented CI reproduction — the sanitizers job
# creates `build-san` and `build-vg` — had JUCE's own README and
# BREAKING_CHANGES.md walked into the scan and reported eight findings against
# the fetched dependency. CI never saw it, because the `docs` job runs in its own
# checkout with no build tree; the person reproducing a red job locally is
# exactly who did. A gate that misreports when you follow the instructions is
# the same shape as the citation gate's wrong-base trap.
#
# NAMED, NOT PREFIXED, and the first repair got that wrong in the other
# direction: `startswith("build")` also swallows a directory called `building`,
# and a gate that silently scans FEWER files is the same defect wearing the
# opposite sign. `.gitignore`'s `build*/` really would ignore `docs/building/`
# too — but the ignore file's job is keeping artefacts out of the index, and
# this one's is finding every governed document, so matching it literally is not
# the goal. A build tree here is `build`, `build-<something>` or
# `cmake-build-<something>`; nothing else is one.
def _is_build_dir(name: str) -> bool:
    return name == "build" or name.startswith(("build-", "cmake-build-"))

# Blocks that interrupt a paragraph, and therefore are NOT swallowed by a
# preceding blockquote's lazy continuation (CommonMark 0.31 §4, §5.1, §6.9).
_INTERRUPTERS = (
    re.compile(r"^#{1,6}(\s|$)"),            # ATX heading (space required)
    re.compile(r"^(`{3,}|~{3,})"),           # fenced code block
    re.compile(r"^([-*_])(\s*\1){2,}\s*$"),  # thematic break
    re.compile(r"^>"),                        # another blockquote line
    re.compile(r"^[-*+](\s|$)"),              # bullet list item
    re.compile(r"^1[.)](\s|$)"),              # ordered list — only "1" interrupts
    re.compile(r"^\|"),                       # GFM table row
    re.compile(r"^<"),                        # HTML block
    re.compile(r"^=+\s*$"),                   # setext underline
)


def interrupts_paragraph(line: str) -> bool:
    """Whether `line` starts a block instead of continuing a paragraph.

    Indentation is stripped before matching -- see KNOWN LIMITS in the module
    docstring for why a line-based lint cannot anchor at column 0.
    """
    return any(rx.match(line.lstrip()) for rx in _INTERRUPTERS)


def blank_code_spans(line: str) -> str:
    """Replace inline code spans (backticks included) with spaces.

    Column positions and newlines are preserved, so this is safe to run over a
    whole paragraph and split back into lines. A span is a run of N backticks
    closed by the next run of exactly N; an unclosed run is left alone, which is
    what CommonMark does too.
    """
    chars, i, n = list(line), 0, len(line)
    while i < n:
        if chars[i] != "`":
            i += 1
            continue
        open_end = i
        while open_end < n and line[open_end] == "`":
            open_end += 1
        width, k, closed = open_end - i, open_end, False
        while k < n:
            if line[k] != "`":
                k += 1
                continue
            close_end = k
            while close_end < n and line[close_end] == "`":
                close_end += 1
            if close_end - k == width:
                for x in range(i, close_end):
                    chars[x] = "\n" if line[x] == "\n" else " "
                i, closed = close_end, True
                break
            k = close_end
        if not closed:
            i = open_end
    return "".join(chars)


def fence_mask(lines: list[str]) -> tuple[list[bool], int | None]:
    """(mask, unclosed_opener_line) — True for lines inside or delimiting a fence.

    A closing fence must use the opener's character, be at least as long, and
    carry **nothing but trailing whitespace** (CommonMark §4.5) -- a line with an
    info string is an *opening* fence, never a closer. Both conditions matter:
    without the length rule a shorter run inside a longer block ends it early;
    without the info-string rule a nested ```cpp inside a ```markdown example
    closes the outer block, so the example's contents get scanned as real
    structure and the real closer re-opens a block that then reads as unclosed.

    The second element is the 1-based line of an opener that was never closed,
    or None. Callers must report it: silently masking to EOF is how this script
    once passed a file it had not read.
    """
    mask: list[bool] = [False] * len(lines)
    char: str | None = None
    width = 0
    opened_at: int | None = None
    for i, line in enumerate(lines):
        match = FENCE.match(line)
        if char is None:
            if match:
                run = match.group(1)
                char, width, opened_at, mask[i] = run[0], len(run), i + 1, True
            continue
        mask[i] = True                       # inside the fence, including its closer
        if match:
            run, rest = match.group(1), match.group(2)
            if run[0] == char and len(run) >= width and not rest.strip():
                char, width, opened_at = None, 0, None
    return mask, opened_at


def blanked_lines(lines: list[str], fenced: list[bool]) -> list[str]:
    """Per-line text with inline code spans blanked, matched paragraph-wide.

    A CommonMark code span may wrap across lines within one paragraph -- ADR-0003
    carries `oversample != Off && (driveDb > 0.01 || isModAlgorithm)` split over
    two lines, whose continuation begins `||` and was read as a table row. Spans
    are therefore matched over each run of consecutive non-blank, non-fenced
    lines and the result is split back, which keeps every line's length and so
    every reported column. A blank line ends a paragraph and resets the state.
    """
    out = list(lines)
    start: int | None = None

    def flush(lo: int, hi: int) -> None:
        joined = blank_code_spans("\n".join(lines[lo:hi]))
        out[lo:hi] = joined.split("\n")

    for i, line in enumerate(lines):
        if fenced[i] or not line.strip():
            if start is not None:
                flush(start, i)
                start = None
            continue
        if start is None:
            start = i
    if start is not None:
        flush(start, len(lines))
    return out


LIST_MARKER = re.compile(r"^\s*([-*+]|\d{1,9}[.)])(\s|$)")


def indent_columns(line: str) -> int:
    """Leading indentation in Markdown **columns**, not characters.

    CommonMark advances a tab to the next four-column tab stop, so one tab is
    four columns and `"  \\t"` is also four. Counting characters instead made a
    tab-indented code example fail the `>= 4` test, so it was never masked and
    its contents were inspected as document structure -- GitHub renders it as
    code, so every finding on it was invented.
    """
    col = 0
    for char in line:
        if char == " ":
            col += 1
        elif char == "\t":
            col += 4 - (col % 4)
        else:
            break
    return col


def indented_code_mask(lines: list[str], fenced: list[bool]) -> list[bool]:
    """True for lines inside a CommonMark *indented* code block (4+ spaces).

    Fences are not the only way to show an example. A four-space-indented block is
    code too, and until this existed its contents were examined as if they were
    document structure -- an illustrated table, link or quote in that form failed
    the run. That is the false-positive class this script's docstring calls the
    worst outcome, so it is masked like a fence.

    The detection is deliberately conservative, because a table nested in a list
    item is *also* indented four or more columns and must stay checked. A run
    qualifies as code only when CommonMark's own precondition holds -- it is
    preceded by a blank line, so it cannot be paragraph continuation -- and the
    nearest preceding non-blank line is at column 0 and is not a list marker. In
    any list context the indent belongs to the container, not to a code block, and
    the run stays in scope.
    """
    mask = [False] * len(lines)
    i, n = 0, len(lines)
    while i < n:
        line = lines[i]
        if fenced[i] or not line.strip() or indent_columns(line) < 4:
            i += 1
            continue
        if i == 0 or lines[i - 1].strip():
            i += 1                            # no blank line before: not a code block
            continue
        k = i - 1
        while k >= 0 and not lines[k].strip():
            k -= 1
        prev = lines[k] if k >= 0 else ""
        in_list_context = bool(prev.strip()) and (
            LIST_MARKER.match(prev) or indent_columns(prev) > 0
        )
        if in_list_context:
            i += 1
            continue
        while i < n and (not lines[i].strip() or indent_columns(lines[i]) >= 4):
            if lines[i].strip():
                mask[i] = True
            i += 1
    return mask


def markdown_files(roots: list[Path]) -> list[Path]:
    out: list[Path] = []
    for root in roots:
        if root.is_file() and root.suffix == ".md":
            out.append(root)
            continue
        for path in sorted(root.rglob("*.md")):
            # RELATIVE TO THE ROOT, not absolute, and that is the whole of this
            # function's correctness. `rglob` yields absolute paths when `root`
            # is absolute -- which it is, `main()` resolves it -- so testing
            # `path.parts` tested every ANCESTOR of the checkout too. A clone at
            # `~/build/anamorph`, `/opt/JUCE/anamorph` or anywhere under a
            # `node_modules` matched the skip set on a directory the scan does
            # not own, excluded EVERY file, and printed `0 file(s) clean`: the
            # exact failure this script's docstring names as worse than no
            # checker. Only the components below the scan root can say anything
            # about whether a file is generated, vendored, or ours.
            parts = path.relative_to(root).parts[:-1]   # directories only; a file may be named anything
            if SKIP_DIRS.isdisjoint(parts) \
               and not any(_is_build_dir(p) for p in parts):
                out.append(path)
    return out


def inline_link_targets(line: str) -> list[str]:
    """Raw destination text of every inline link on one line.

    Scanned rather than matched by regex, because a destination or title may
    contain parentheses: `[t](docs/a(1).md)` and `[t](p.md "A (note)")` are both
    valid, and a `[^)]*` pattern truncates them into paths that do not exist --
    a false broken-link report on correct markup. Depth counting handles nested
    parens, quotes suppress counting inside a title, and an angle-bracketed
    destination is skipped whole. An unterminated `](` yields nothing, which is
    also how CommonMark treats it: not a link.
    """
    targets: list[str] = []
    for match in LINK_OPEN.finditer(line):
        start = j = match.end()
        if j < len(line) and line[j] == "<":            # <dest> may hold anything
            close = line.find(">", j)
            if close == -1:
                continue
            j = close + 1
        depth, quote = 1, ""
        while j < len(line):
            char = line[j]
            if quote:
                if char == quote:
                    quote = ""
            elif char in "\"'":
                quote = char
            elif char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
                if depth == 0:
                    targets.append(line[start:j])
                    break
            j += 1
    return targets


def link_destination(raw: str) -> str | None:
    """The path part of a link destination, or None if there is nothing to check.

    Handles `<angle-bracketed>` destinations and the optional title that may
    follow in double quotes, single quotes or parentheses.
    """
    raw = raw.strip()
    if not raw:
        return None
    if raw.startswith("<"):
        end = raw.find(">")
        return raw[1:end] if end != -1 else raw[1:]
    dest = raw.split(None, 1)[0] if re.search(r'\s+["\'(]', raw) else raw
    return dest.strip()


def check_tables(path: Path, lines: list[str], skip: list[bool]) -> list[str]:
    """Every run of pipe-prefixed lines must open with a header + separator pair.

    Rows are matched at **any** indent, not GFM's three columns: a table nested in
    a list item sits at its container's content column, which is often deeper, and
    a check that silently declines to look is the failure this script exists to
    prevent. The separator test runs against the stripped line for the same reason.
    """
    findings: list[str] = []
    run: list[tuple[int, str]] = []

    def close(run: list[tuple[int, str]]) -> None:
        if len(run) < 2 or not SEPARATOR.match(run[1][1].strip()):
            findings.append(
                f"{path}:{run[0][0]}: table fragment with no header/separator "
                f"({len(run)} pipe line(s)) -- a block was inserted mid-table, "
                f"or the separator row is missing"
            )

    for i, line in enumerate(lines):
        if not skip[i] and TABLE_ROW.match(line):
            run.append((i + 1, line))
        elif run:
            close(run)
            run = []
    if run:
        close(run)
    return findings


def check_links(path: Path, lines: list[str], skip: list[bool], root: Path) -> list[str]:
    findings = []
    for i, line in enumerate(lines):
        if skip[i]:
            continue
        for raw in inline_link_targets(line):
            dest = link_destination(raw)
            if dest is None:
                continue
            target = unquote(dest.split("#", 1)[0].strip())
            if not target or target.startswith(("http://", "https://", "mailto:")):
                continue
            base = root if target.startswith("/") else path.parent
            resolved = (base / target.lstrip("/")).resolve()
            if not resolved.exists():
                findings.append(f"{path}:{i + 1}: broken relative link -> {target}")
    return findings


def check_lazy_continuation(path: Path, lines: list[str], skip: list[bool]) -> list[str]:
    findings = []
    for i in range(len(lines) - 1):
        if skip[i] or skip[i + 1]:
            continue
        quote = lines[i].lstrip()
        if not quote.startswith(">"):
            continue
        if not quote.lstrip(">").strip():
            continue                         # blank quote line: the paragraph is closed
        nxt = lines[i + 1]
        if not nxt.strip() or interrupts_paragraph(nxt):
            continue
        findings.append(
            f"{path}:{i + 2}: line is absorbed into the preceding blockquote "
            f"by lazy continuation -- insert a blank line or quote it"
        )
    return findings


def check_changelog_notes_boundary(path: Path, lines: list[str], skip: list[bool]) -> list[str]:
    """Below the first entry heading, every `## ` heading in CHANGELOG.md must be
    an entry heading -- i.e. must start `## [`.

    `release.yml` extracts a release's notes as everything from its own `## [`
    heading to the NEXT `## [` heading, so the `## [` form is reserved for entry
    boundaries. A `## ` heading in any other form does not terminate the scan,
    which breaks the extraction in the direction that is hardest to notice: the
    section runs ON, past where the entry ends, and lands in the published notes
    of a release it does not belong to. Two ways a future edit does that, neither
    visible until a tag is cut:

      * demoting one of an entry's sub-sections from `### ` to `## Fixed` --
        harmless where it sits, but it establishes the habit, and
      * appending an `## Acknowledgements`-style section to the foot of the file,
        which is then published as part of the OLDEST entry's notes (that entry
        has no following `## [` to stop at, so its notes run to end of file).

    Older ENTRY headings are NOT findings: terminating on them is the mechanism
    working, and that includes the non-semver spellings this file uses for the
    reconstructed history (`## [0.6.x] and earlier`, `## [0.7.5] - [0.7.0]`) --
    they carry the bracket, so the extractor stops at them exactly as intended.
    `## [Unreleased]`, were it ever added, sits above the first entry and is
    likewise outside the rule.
    """
    if path.name != "CHANGELOG.md":
        return []
    findings = []
    first: int | None = None
    for i, line in enumerate(lines):
        if skip[i]:
            continue
        h = atx_heading(line)
        if h is None or h[0] != 2:
            continue
        if CHANGELOG_VERSION_HEADING.match(line) or h[1].startswith("["):
            # Any entry-SHAPED heading arms the rule, even one spelled in a way
            # `release.yml` cannot extract: `check_changelog_headings` reports the
            # spelling, and this rule must still see that the entries have begun.
            # Arming only on the publishable spelling meant one bad heading at the
            # top of the file silently disarmed the boundary check for the rest of
            # it -- a second, independent defect hidden behind the first.
            if first is None:
                first = i
            continue
        if first is not None:
            findings.append(
                f"{path}:{i + 1}: `## ` heading that is not an entry heading (`## [`), below "
                f"the first entry (line {first + 1}) -- release.yml ends a release's notes at "
                f"the next `## [`, so this section is published inside whichever entry it "
                f"happens to sit under. Move it ABOVE the first entry heading, or into an "
                f"entry's lead as a bold note (CHANGELOG_POLICY.md rule 6) -- demoting it to "
                f"`### ` only makes it an invented category"
            )
    return findings


# Keep a Changelog 1.1.0's six change types, in the order the specification
# lists them. The order is part of the format, not a preference: a reader who
# knows the spec scans for `Removed` between `Deprecated` and `Fixed`, and a
# release that shuffles them makes every release harder to skim than the one
# above it.
KAC_CATEGORIES = ("Added", "Changed", "Deprecated", "Removed", "Fixed", "Security")

# An ATX heading as CommonMark §4.2 defines it: up to THREE columns of
# indentation, one to six `#`, then a space or the end of the line, then the
# text, then an optional closing run of `#` preceded by a space. Four or more
# columns is an indented code block, not a heading, and `indented_code_mask`
# already handles that case.
#
# WHY THE INDENT IS ACCEPTED HERE and the version-heading rule below still
# rejects it: the first version of the category rule matched `^### ` only, so
# `   ### Fixed` -- a heading to every Markdown renderer -- was invisible to it,
# and a duplicated or invented category could bypass CI by being indented one
# space. A checker that reads Markdown must read the grammar the renderer
# reads. The release extractor in `release.yml`, on the other hand, matches
# `^## \[` at column 0 and nothing else, so a version heading that renders but
# does not extract is a defect in its own right, reported as one.
ATX_HEADING = re.compile(r"^ {0,3}(#{1,6})(?:[ \t]+(.*?))?[ \t]*$")


def atx_heading(line: str) -> tuple[int, str] | None:
    """(level, text) for an ATX heading, else None. The closing `#` run is
    stripped, as the renderer strips it; `###` alone is a heading with empty
    text; `####x` (no space) is not a heading at all."""
    m = ATX_HEADING.match(line)
    if not m:
        return None
    text = m.group(2) or ""
    text = re.sub(r"(?:^|\s+)#+$", "", text).strip()
    return len(m.group(1)), text


# The version-heading grammar this repository writes, and the only forms the
# checks below accept as an ENTRY:
#
#   ## [Unreleased]                     -- at most once, and only as the first entry
#   ## [x.y.z] — YYYY-MM-DD             -- a released version; `-` for the dash is
#                                          also accepted (the spec's own example
#                                          uses it), an optional ` [YANKED]` too
#
# plus, by exact text, the two RECONSTRUCTED headings at the foot of the file
# (`RECONSTRUCTED_HEADINGS`), which predate the policy and are grandfathered
# rather than rewritten. The bracket alone (`^## \[`) is what `release.yml`
# terminates a release's notes at, so that stays the boundary rule; THIS grammar
# is what the checker demands of the text inside the bracket.
VERSION_HEADING_TEXT = re.compile(
    r"^\[(\d+)\.(\d+)\.(\d+)\] (?:—|-) (\d{4}-\d{2}-\d{2})( \[YANKED\])?$"
)
UNRELEASED_HEADING_TEXT = "[Unreleased]"
RECONSTRUCTED_HEADINGS = (
    "[0.7.5] – [0.7.0] — 2026-06-21…22",
    "[0.6.x] and earlier — 2026-06 (reconstructed)",
)
# The first version this line ever tagged. Versions from here on are released
# by `release.yml` from an annotated `v<x.y.z>` tag, so every one of them has a
# tag page or a comparison to point its `[x.y.z]` heading at; nothing older does
# (0.9.0 through 0.9.6 were each written up and superseded before a tag was
# cut), so a definition for one of those would be a link to a page that will
# never exist. A constant, because the fact is: it never changes.
FIRST_TAGGED_VERSION = (0, 9, 7)
# The one repository a version link may point into. Checked because a definition
# is a citation: `https://example.com/x/compare/v0.9.7...v0.9.8` satisfied every
# earlier spelling of the rule and resolves to nothing.
REPO_URL = "https://github.com/skyRolly/Anamorph"
# A CommonMark link reference definition (§4.7): up to three columns of
# indentation, the label, a destination that may be angle-bracketed, and an
# optional title. The first spelling of this required column 0 and a bare
# destination, so `  [0.9.7]: <url> "title"` -- a working link in every renderer --
# was reported as a MISSING definition. A false "add what is already there" is the
# fail-closed direction, which is why it blocked a push rather than shipping a
# broken link, but it is still the script telling the truth about its own regex
# rather than about the document.
LINK_DEFINITION = re.compile(
    r"""^ {0,3}\[([^\]]+)\]:[ \t]*(<[^>\n]*>|\S+)(?:[ \t]+(?:"[^"]*"|'[^']*'|\([^)]*\)))?[ \t\r]*$"""
)
# The one spelling `release.yml` can publish: `## [` at column 0, exactly one
# space. Everything else -- a leading indent, a tab or a second space after the
# `##` -- renders as a heading and is invisible to the extractor's `^## \[`, so a
# release written that way could never be published and an OLDER entry written
# that way does not terminate the entry above it (its whole body is then
# published inside its predecessor's notes). Testing this by counting leading
# spaces missed both whitespace forms; testing the literal prefix cannot.
PUBLISHABLE_ENTRY_PREFIX = "## ["
# A heading indented four columns or more. CommonMark calls that an indented code
# block -- EXCEPT inside a list item, where the indent belongs to the container
# and `### Fixed` still renders as a heading. `indented_code_mask` deliberately
# leaves list-context indentation unmasked (its docstring says so), so such a line
# was neither masked as code nor matched as a heading: a duplicated or invented
# category could hide there.
#
# TWO THINGS THIS DELIBERATELY DOES NOT DO. It does not measure the indent in
# characters -- `indent_columns` exists because one tab is four columns, and a
# tab-indented heading bypassed the first spelling of this rule. And it does not
# report every deep `###`: inside a list item at that depth the line may equally
# be a code sample, which no parser can distinguish without a container stack, so
# only a heading whose text is a Keep a Changelog CATEGORY (or a level-2 entry
# heading) is reported. That is the shape a real category bypass takes, and a
# sample called `### Fixed` is not one anybody writes.
DEEP_HEADING = re.compile(r"^[ \t]+(#{1,6})[ \t]+(\S.*?)[ \t]*$")
# A setext underline (§4.3): `Changed` over `---` renders as a heading too, and
# `release.yml`'s extractor stops at neither it nor a setext version heading. Only
# a PARAGRAPH can carry one, which is what `NOT_A_SETEXT_SUBJECT` excludes: a list
# item, a table row, a quote, an ATX heading, an HTML block, a link reference
# definition. Testing the first character instead (`startswith("-")`) called a
# paragraph beginning `-not a list` a list item, and called the thematic break
# under the file's own link definitions a heading.
SETEXT_UNDERLINE = re.compile(r"^ {0,3}(=+|-+)[ \t\r]*$")


class ChangelogEntry:
    __slots__ = ("line_no", "text", "kind", "version", "date", "categories")

    def __init__(self, line_no: int, text: str, kind: str,
                 version: tuple[int, int, int] | None, date: str | None) -> None:
        self.line_no = line_no
        self.text = text
        self.kind = kind          # "unreleased" | "version" | "reconstructed" | "malformed"
        self.version = version
        self.date = date
        self.categories: list[tuple[str, int]] = []


def parse_changelog(lines: list[str], skip: list[bool]
                    ) -> tuple[list[ChangelogEntry], list[tuple[int, str, str]], list[str]]:
    """(entries, link_definitions, findings) for CHANGELOG.md's structure.

    THE BOUNDARY IS THE ENTRY HEADING, NOT ANY LEVEL-TWO HEADING. The first
    version of the category rule started a new "entry" at every `## ` line, so a
    preamble section such as `## How to read this file` became an entry and its
    `### ` sub-headings were reported as invented categories -- the checker
    rejected a shape the format allows. Now an entry begins only at a
    column-0 `## [` heading, exactly where `release.yml` begins one; every
    heading before the first such line is preamble and is left alone, and a
    stray `## Foo` AFTER the first entry does not start one either (the extractor
    does not stop there, so its sub-headings really do belong to the running
    entry -- `check_changelog_notes_boundary` reports the heading itself).

    Category headings are level-3 ATX headings at 0-3 columns of indentation,
    inside an entry. Link definitions are `[label]: url` lines anywhere in the
    file. Findings raised HERE are the ones about the entry headings themselves
    (grammar, indentation); the ordering, category and link rules read the
    parsed result.
    """
    entries: list[ChangelogEntry] = []
    definitions: list[tuple[int, str, str]] = []
    findings: list[str] = []
    for i, line in enumerate(lines):
        if skip[i]:
            continue
        d = LINK_DEFINITION.match(line)
        if d:
            destination = d.group(2)
            if destination.startswith("<") and destination.endswith(">"):
                destination = destination[1:-1]     # CommonMark §4.7: `<...>` is a wrapper
            definitions.append((i + 1, " ".join(d.group(1).split()), destination))
            continue
        deep = DEEP_HEADING.match(line)
        if deep and entries and indent_columns(line) >= 4:
            level, text = len(deep.group(1)), deep.group(2)
            if level == 3 and text in KAC_CATEGORIES:
                findings.append(
                    f"CHANGELOG.md:{i + 1}: `### {text}` is indented "
                    f"{indent_columns(line)} columns. Inside a list item that still renders "
                    f"as a heading, so a category can hide there; a category heading belongs "
                    f"at column 0"
                )
                entries[-1].categories.append((text, i + 1))
                continue
            if level == 2 and text.startswith("["):
                findings.append(
                    f"CHANGELOG.md:{i + 1}: `{line.strip()}` is indented "
                    f"{indent_columns(line)} columns. Inside a list item it renders as an "
                    f"entry heading, which `release.yml` cannot extract (`^## \\[`) and which "
                    f"does not terminate the entry above it; write it at column 0"
                )
                continue
            # anything else at that depth is a sample, not structure: leave it be,
            # and let the branches below have their turn at the line.
        if SETEXT_UNDERLINE.match(line) and i and lines[i - 1].strip() and not skip[i - 1] \
                and not LIST_MARKER.match(lines[i - 1]) \
                and not interrupts_paragraph(lines[i - 1]) \
                and not LINK_DEFINITION.match(lines[i - 1]) \
                and not lines[i - 1].lstrip().startswith(("|", "<")):
            findings.append(
                f"CHANGELOG.md:{i + 1}: `{lines[i - 1].strip()}` underlined by `{line.strip()}` "
                f"is a setext heading. `release.yml` extracts and terminates release notes on "
                f"`^## \\[` alone and cannot see it -- write headings as `##` / `###`"
            )
            continue
        h = atx_heading(line)
        if h is None:
            continue
        level, text = h
        if level == 2 and text.startswith("["):
            if not line.startswith(PUBLISHABLE_ENTRY_PREFIX):
                findings.append(
                    f"CHANGELOG.md:{i + 1}: `{line.strip()}` renders as an entry heading but "
                    f"is not written `## [` at column 0 with a single space, which is the only "
                    f"form `release.yml` extracts (`^## \\[`) -- this release could not be "
                    f"published, and an older entry written this way does not terminate the "
                    f"entry above it"
                )
            m = VERSION_HEADING_TEXT.match(text)
            if m:
                version = (int(m.group(1)), int(m.group(2)), int(m.group(3)))
                entries.append(ChangelogEntry(i + 1, text, "version", version, m.group(4)))
            elif text == UNRELEASED_HEADING_TEXT:
                entries.append(ChangelogEntry(i + 1, text, "unreleased", None, None))
            elif text in RECONSTRUCTED_HEADINGS:
                entries.append(ChangelogEntry(i + 1, text, "reconstructed", None, None))
            else:
                entries.append(ChangelogEntry(i + 1, text, "malformed", None, None))
                findings.append(
                    f"CHANGELOG.md:{i + 1}: `## {text}` is not a valid entry heading -- write "
                    f"`## [x.y.z] — YYYY-MM-DD` (optionally ` [YANKED]`), or `## [Unreleased]` "
                    f"as the first entry. An undated version heading is not a release "
                    f"(`release.yml` refuses to publish it)"
                )
            continue
        if level == 3 and entries:
            entries[-1].categories.append((text, i + 1))
    return entries, definitions, findings


def check_changelog_headings(path: Path, lines: list[str], skip: list[bool]) -> list[str]:
    """Entry headings: valid grammar, a real calendar date, `[Unreleased]` first
    and only once, versions strictly newest-first, the reconstructed history
    last.

    This is the machine-checkable half of `CHANGELOG_POLICY.md` rule 7. What it
    does NOT decide: whether a date is the RIGHT date, or whether a version
    number is the right bump -- both are facts about the release, not about the
    file, and stay with the maintainer.
    """
    if path.name != "CHANGELOG.md":
        return []
    entries, _, findings = parse_changelog(lines, skip)
    findings = [f.replace("CHANGELOG.md:", f"{path}:", 1) for f in findings]
    previous: ChangelogEntry | None = None
    seen_reconstructed = False
    for pos, e in enumerate(entries):
        if e.kind == "unreleased":
            if pos != 0:
                findings.append(
                    f"{path}:{e.line_no}: `## [Unreleased]` must be the first entry -- it "
                    f"tracks what the NEXT release will contain, so nothing released sits "
                    f"above it"
                )
            continue
        if e.kind == "reconstructed":
            seen_reconstructed = True
            continue
        if e.kind != "version":
            continue
        try:
            datetime.date.fromisoformat(e.date or "")
        except ValueError:
            findings.append(
                f"{path}:{e.line_no}: `{e.date}` is not a calendar date -- the release date "
                f"is ISO 8601, `YYYY-MM-DD`"
            )
        if seen_reconstructed:
            findings.append(
                f"{path}:{e.line_no}: a versioned entry below the reconstructed history -- "
                f"the two reconstructed headings are the foot of the file, nothing goes "
                f"under them"
            )
        if previous is not None and previous.version is not None and e.version is not None:
            if e.version >= previous.version:
                findings.append(
                    f"{path}:{e.line_no}: [{'.'.join(map(str, e.version))}] follows "
                    f"[{'.'.join(map(str, previous.version))}] -- entries run newest first, "
                    f"strictly (Keep a Changelog: the latest version comes first)"
                )
        previous = e
    return findings


def check_changelog_categories(path: Path, lines: list[str], skip: list[bool]) -> list[str]:
    """Inside one CHANGELOG entry, `### ` headings must be Keep a Changelog
    categories, each at most once, in the specification's order.

    WHAT THIS CAUGHT, and why a checker rather than a rule in a document: the
    `[0.9.7]` entry had grown TWO `### Fixed` sections either side of its
    `### Changed` -- each round appended its own heading rather than adding a
    bullet to the one already there -- so the same release told its story as
    Fixed, then Changed, then Fixed again. Six other entries had Fixed above
    Changed, and four sections carried invented names (`Compatibility`,
    `Documentation`, `Build / Release`, `Known issues`) that no reader of the
    spec would look for. None of it is visible while writing ONE entry; all of
    it is obvious to a checker that reads the whole file.

    The rule is deliberately narrow. It says nothing about what belongs in a
    category -- that is a judgement no parser should make, and it stays with
    `CHANGELOG_POLICY.md` and the author. It only asserts the three things the
    format fixes: the NAME is one of the six, it appears ONCE, and the order is
    the spec's. A release-level note that is not a change (a compatibility
    statement, a known issue) is not a category and belongs in the entry's lead,
    which is why an unknown `### ` name is reported rather than tolerated.
    """
    if path.name != "CHANGELOG.md":
        return []
    entries, _, _ = parse_changelog(lines, skip)
    findings: list[str] = []
    for e in entries:
        for pos, (cat, line_no) in enumerate(e.categories):
            if cat not in KAC_CATEGORIES:
                findings.append(
                    f"{path}:{line_no}: `### {cat}` is not a Keep a Changelog category "
                    f"({', '.join(KAC_CATEGORIES)}). Put a release-level note in the entry's "
                    f"lead instead, or file the bullets under the category they belong to"
                )
                continue
            earlier = [c for c, _ in e.categories[:pos] if c in KAC_CATEGORIES]
            if cat in earlier:
                findings.append(
                    f"{path}:{line_no}: second `### {cat}` in ## {e.text} -- one section per "
                    f"category per release; add the bullet to the existing section"
                )
                continue
            out_of_order = [
                c for c in earlier if KAC_CATEGORIES.index(c) > KAC_CATEGORIES.index(cat)
            ]
            if out_of_order:
                findings.append(
                    f"{path}:{line_no}: `### {cat}` comes after `### {out_of_order[0]}` in "
                    f"## {e.text} -- Keep a Changelog orders them "
                    f"{' > '.join(KAC_CATEGORIES)}"
                )
    return findings


def check_changelog_links(path: Path, lines: list[str], skip: list[bool]) -> list[str]:
    """Every `[x.y.z]` heading is a link reference. From `FIRST_TAGGED_VERSION`
    on, each one must have a definition and the definition must name that
    version's own tag; below it, none may (there is no tag to point at). An
    `[Unreleased]` heading needs a `...HEAD` comparison.

    The definition is written in the RELEASE COMMIT, naming the tag that commit
    is about to carry -- `v<x.y.z>`, fixed by `release.yml`'s rule that the tag
    equals the CMake project version -- and the tag is pushed straight after.
    That is the sequence the specification's own example implies (its link
    definitions exist in the tagged tree), and the only one that is satisfiable:
    a tag points at an existing commit, so the definition cannot wait for it.
    What this check therefore asserts is not that the URL resolves today but
    that it is the deterministic one: the right version, the right form
    (`/releases/tag/v<x.y.z>` for a first tag, `/compare/v<a.b.c>...v<x.y.z>`
    after), and no definition for a version this line never tagged.
    """
    if path.name != "CHANGELOG.md":
        return []
    entries, definitions, _ = parse_changelog(lines, skip)
    findings: list[str] = []
    defined: dict[str, tuple[int, str]] = {}
    versions_early = {".".join(map(str, e.version)): e for e in entries if e.kind == "version"}
    ordered = list(versions_early)
    for line_no, label, url in definitions:
        if re.fullmatch(r"\d+\.\d+\.\d+", label) or label.lower() == "unreleased":
            key = label.lower() if label.lower() == "unreleased" else label
            if key in defined:
                findings.append(f"{path}:{line_no}: `[{label}]` is defined twice")
            defined[key] = (line_no, url)
    versions = versions_early
    previous_of = {k: ordered[n + 1] for n, k in enumerate(ordered) if n + 1 < len(ordered)}
    # A label whose heading EXISTS but is malformed (`## [0.9.8] — <YYYY-MM-DD>`)
    # is not an orphaned definition: the entry is there, its heading text is
    # wrong, and `check_changelog_headings` already says so. Reporting the
    # definition too pointed the author at the wrong line.
    claimed = {
        e.text[1:e.text.index("]")]
        for e in entries
        if e.kind == "malformed" and e.text.startswith("[") and "]" in e.text
    }
    has_unreleased = any(e.kind == "unreleased" for e in entries)

    for key, (line_no, url) in defined.items():
        if key == "unreleased":
            if not has_unreleased:
                findings.append(
                    f"{path}:{line_no}: `[Unreleased]` is defined but there is no "
                    f"`## [Unreleased]` entry"
                )
            else:
                newest = next((k for k in ordered), None)
                if newest is None:
                    # No released version below it: nothing to compare from, so the
                    # shape is all that can be asked for.
                    want = url if re.fullmatch(
                        rf"{re.escape(REPO_URL)}/compare/\S+\.\.\.HEAD", url) else (
                        f"{REPO_URL}/compare/v<last tag>...HEAD")
                else:
                    want = f"{REPO_URL}/compare/v{newest}...HEAD"
                if url != want:
                    findings.append(
                        f"{path}:{line_no}: the `[Unreleased]` definition must be `{want}` -- "
                        f"the comparison runs from the newest released version to HEAD"
                    )
            continue
        e = versions.get(key)
        if e is None:
            if key not in claimed:
                findings.append(
                    f"{path}:{line_no}: `[{key}]` is defined but there is no `## [{key}]` entry"
                )
            continue
        if e.version is not None and e.version < FIRST_TAGGED_VERSION:
            findings.append(
                f"{path}:{line_no}: `[{key}]` predates this line's first tag "
                f"(v{'.'.join(map(str, FIRST_TAGGED_VERSION))}) and was never tagged -- "
                f"there is no release page to link, so it must not be defined"
            )
            continue
        tag = f"v{key}"
        # WHICH form, not merely "one of the two". The first version this line
        # tags has no predecessor to compare against, so it points at its own tag
        # page; every later one compares against the version directly above it in
        # the file. Accepting either for any version -- the first spelling of this
        # check -- let `[0.9.7]: .../compare/v0.9.6...v0.9.7` pass, a comparison
        # against a tag that was never cut, which is a dead link in the one place
        # the specification asks to be linkable.
        if e.version == FIRST_TAGGED_VERSION:
            want = f"{REPO_URL}/releases/tag/{tag}"
        elif key in previous_of:
            want = f"{REPO_URL}/compare/v{previous_of[key]}...{tag}"
        else:
            # A tagged-era version with no older entry beneath it in the file. The
            # comparison has no left operand to name, so say that rather than
            # printing a placeholder into the URL the author is told to write.
            findings.append(
                f"{path}:{line_no}: `[{key}]` must compare against the version released "
                f"before it, but no older entry appears below `## [{key}]` -- add the "
                f"predecessor's entry, or use `{REPO_URL}/releases/tag/{tag}` if this is "
                f"the first tag"
            )
            continue
        if url != want:
            why = ("the line's first tag has no predecessor to compare against"
                   if e.version == FIRST_TAGGED_VERSION
                   else "a comparison against the next-older entry, the one directly BELOW it")
            findings.append(
                f"{path}:{line_no}: the `[{key}]` definition must be `{want}` ({why}); "
                f"got `{url}`"
            )

    for key, e in versions.items():
        if e.version is not None and e.version >= FIRST_TAGGED_VERSION and key not in defined:
            findings.append(
                f"{path}:{e.line_no}: `## [{key}]` has no link definition -- add "
                f"`[{key}]: <url>` at the foot of the file in the release commit "
                f"(CHANGELOG_POLICY.md rule 8, RELEASE_PROCESS.md §Tagging)"
            )
    if has_unreleased and "unreleased" not in defined:
        e = next(e for e in entries if e.kind == "unreleased")
        findings.append(
            f"{path}:{e.line_no}: `## [Unreleased]` has no link definition -- add "
            f"`[Unreleased]: .../compare/v<last tag>...HEAD` at the foot of the file"
        )
    return findings


def analyse(path: Path, lines: list[str], root: Path) -> list[str]:
    """Run every check over one document's lines."""
    fenced, unclosed = fence_mask(lines)
    indented = indented_code_mask(lines, fenced)
    skip = [f or c for f, c in zip(fenced, indented)]
    text = blanked_lines(lines, skip)
    findings = []
    if unclosed is not None:
        findings.append(
            f"{path}:{unclosed}: code fence opened here is never closed -- the rest "
            f"of the file renders as code, and every check below it is skipped"
        )
    findings += check_tables(path, text, skip)
    findings += check_links(path, text, skip, root)
    findings += check_lazy_continuation(path, text, skip)
    findings += check_changelog_notes_boundary(path, text, skip)
    findings += check_changelog_headings(path, text, skip)
    findings += check_changelog_categories(path, text, skip)
    findings += check_changelog_links(path, text, skip)
    return findings


def check_file(path: Path, root: Path) -> list[str]:
    try:
        text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError as exc:
        # Report as a finding, not a traceback: the CI job's whole contract is
        # `path:line: message`, and a stray legacy-encoded byte in a corpus this
        # full of typographic characters (— · ⊕ ≥) is a plausible accident.
        line = path.read_bytes()[: exc.start].count(b"\n") + 1
        return [
            f"{path}:{line}: not valid UTF-8 (byte {exc.object[exc.start]:#04x} "
            f"at offset {exc.start}: {exc.reason}) -- the file cannot be checked"
        ]
    return analyse(path, text.split("\n"), root)


def self_test() -> int:
    """Assert the checks fire on real defects and stay silent on valid markup.

    Every case below is one that actually reached review: earlier revisions of
    this script reported the "silent" cases as findings, or missed the "fires"
    ones. A lint whose only evidence is "it returns clean on our tree" proves
    nothing -- and on this repository that evidence was itself false, because an
    unclosed fence had exempted the largest file. Both directions are pinned.
    """
    doc = Path(__file__).resolve()           # a path that exists, for link cases
    root = doc.parent.parent
    cases: list[tuple[str, int, list[str]]] = [
        # --- must stay silent -------------------------------------------------
        ("table syntax inside a fence", 0, ["```", "| not a table", "```"]),
        ("blank quote line ends the paragraph", 0, ["> quote", ">", "Normal paragraph."]),
        ("ordered list at 1 interrupts", 0, ["> quote", "1. item"]),
        ("quote syntax inside a fence", 0, ["```", "> quote", "text", "```"]),
        ("link inside a fence", 0, ["```", "[a](nope.md)", "```"]),
        ("pipe inside an inline code span", 0, ["`| not a table |` prose"]),
        ("quote marker inside an inline code span", 0,
         ["prose with `> quote` inline", "next line"]),
        ("short closer does not end a longer fence", 0,
         ["````", "```", "| not a table", "````"]),
        ("link inside an inline code span", 0, ["prose `[t](path \"Title\")` shown as an example"]),
        ("code span wrapping across lines", 0,     # ADR-0003's `a && (b > 0 || c)` shape
         ["prose `oversample != Off && (drive > 0.01", "|| isMod)` continues here"]),
        ("titled link resolves", 0, ['[a](scripts/check-docs.py "Title")']),
        ("percent-encoded link resolves", 0, ["[a](scripts/check%2Ddocs.py)"]),
        ("title containing parentheses", 0, ['[a](scripts/check-docs.py "A (note)")']),
        ("angle-bracketed destination", 0, ["[a](<scripts/check-docs.py>)"]),
        ("unterminated link is not a link", 0, ["[a](scripts/nope.py"]),
        ("table in an indented code block", 0, ["Example:", "", "    | A | B |", "    | 1 | 2 |"]),
        ("link in an indented code block", 0, ["Example:", "", "    [a](nope.md)"]),
        ("quote in an indented code block", 0, ["Example:", "", "    > quote", "    absorbed"]),
        ("tilde fence indented four columns", 0,
         ["Example:", "", "    ~~~", "    | a | b |", "    ~~~"]),
        ("tab-indented table example", 0, ["Example:", "", "\t| a | b |", "\t| c | d |"]),
        ("tab-indented link example", 0, ["Example:", "", "\t[a](nope.md)"]),
        ("two spaces plus a tab reaches column four", 0, ["Example:", "", "  \t| a | b |"]),
        ("delimiter row without trailing pipe is valid GFM", 0,
         ["| A | B |", "|---|---", "| 1 | 2 |"]),
        ("delimiter row with alignment colons, no trailing pipe", 0,
         ["| A | B |", "|:---|---:", "| 1 | 2 |"]),
        ("info-string fence cannot close an enclosing fence", 0,
         ["```markdown", "```cpp", "| A | B |", "```"]),
        ("nested markdown example stays masked to its real closer", 0,
         ["```markdown", "| A | B |", "> quote", "absorbed?", "```"]),
        # --- must fire --------------------------------------------------------
        ("block inserted mid-table", 1,
         ["| A | B |", "|---|---|", "| 1 | 2 |", "> intruder", "| 3 | 4 |"]),
        ("genuine lazy continuation", 1, ["> quote", "absorbed line"]),
        ("table with no separator row", 1, ["| a | b |", "| c | d |"]),
        ("ordered list not at 1 is absorbed", 1, ["> quote", "2. item"]),
        ("indented table is examined", 1, ["   | a | b |", "   | c | d |"]),
        ("deeply indented table is examined too", 1,      # GFM's 3-space rule would skip this
         ["      | a | b |", "      | c | d |"]),
        ("table nested in a bullet item stays checked", 1,
         ["- item", "", "      | a | b |", "      | c | d |"]),
        ("table nested in a numbered item stays checked", 1,
         ["1. item", "", "      | a | b |", "      | c | d |"]),
        ("indent with no blank line before it is not code", 1,
         ["Example:", "    | a | b |", "    | c | d |"]),
        ("tab-nested table in a list item stays checked", 1,
         ["- item", "", "\t\t| a | b |", "\t\t| c | d |"]),
        ("whitespace-only row is not a delimiter", 1,
         ["| a | b |", "|   |", "| 1 | 2 |"]),
        ("unclosed fence is a finding", 1, ["intro", "```", "rest of the file"]),
        ("broken link is caught", 1, ["[a](scripts/nope.py)"]),
    ]
    failures = checked = 0
    for label, expected, lines in cases:
        # Link cases resolve relative to the document, so place it at the repo
        # root -- the same base a real docs file uses for `scripts/...` targets.
        got = len(analyse(root / "x.md", lines, root))
        checked += 1
        if got != expected:
            failures += 1
            print(f"self-test FAIL: {label}: expected {expected}, got {got}", file=sys.stderr)

    # An unclosed fence must be reported, not silently swallow the file.
    checked += 1
    _, unclosed = fence_mask(["intro", "```", "everything after is masked"])
    if unclosed != 2:
        failures += 1
        print(f"self-test FAIL: unclosed fence reported at {unclosed}, want 2", file=sys.stderr)
    checked += 1
    _, closed = fence_mask(["```", "code", "```"])
    if closed is not None:
        failures += 1
        print(f"self-test FAIL: closed fence reported as unclosed at {closed}", file=sys.stderr)

    for raw, want in [
        ('docs/DESIGN.md "T"', "docs/DESIGN.md"),   # double-quoted title
        ("p.md 'T'", "p.md"),                        # single-quoted title
        ("p.md (T)", "p.md"),                        # parenthesised title
        ("<a b.md>", "a b.md"),                      # angle-bracketed destination
        ("p.md", "p.md"),
    ]:
        got_dest = link_destination(raw)
        checked += 1
        if got_dest != want:
            failures += 1
            print(
                f"self-test FAIL: link_destination({raw!r}) -> {got_dest!r}, want {want!r}",
                file=sys.stderr,
            )

    # The CHANGELOG boundary rule is file-scoped, so it cannot ride the `x.md`
    # loop above. Both directions again: the shape the extractor relies on, and
    # the two ways a future edit breaks it.
    # Fixture conventions: versions BELOW the first tagged version (0.9.7) need no
    # link definition, so structural cases use `[0.5.0]` / `[0.4.0]`; the link
    # cases use `[0.9.7]` (the first tag, a tag page) and `[0.9.8]` (the one after
    # it, a comparison). `V5` / `V4` are valid dated headings.
    V5 = "## [0.5.0] — 2026-01-02"
    V4 = "## [0.4.0] — 2026-01-01"
    V7 = "## [0.9.7] — 2026-09-05"
    V8 = "## [0.9.8] — 2026-09-06"
    D7 = "[0.9.7]: https://github.com/skyRolly/Anamorph/releases/tag/v0.9.7"
    D8 = "[0.9.8]: https://github.com/skyRolly/Anamorph/compare/v0.9.7...v0.9.8"
    for label, expected, lines in [
        # -- the notes-boundary rule, as before --------------------------------
        ("entry sub-sections at ### are fine", 0,
         ["# Changelog", V5, "### Added", "- x"]),
        ("a fenced ## sample is data", 0,
         ["# Changelog", V5, "### Added", "```", "## [x.y.z] - date", "```"]),
        ("a trailing section truncates the oldest entry's notes", 1,
         ["# Changelog", V5, "### Added", "## Acknowledgements"]),
        ("an INDENTED trailing section is the same defect", 1,
         ["# Changelog", V5, "### Added", "  ## Acknowledgements"]),
        ("a demoted sub-section ends them early", 1,
         ["# Changelog", V5, "### Added", "## Fixed", "- y"]),
        ("an older version entry is the mechanism working, not a finding", 0,
         ["# Changelog", V5, "### Added", V4, "### Fixed"]),
        # THE ADAPTATION FROM THE SIBLING, asserted rather than assumed. This
        # file's reconstructed history uses two entry headings that are not a
        # bare semver. They carry the bracket, so this repository's extractor
        # stops at them; they are grandfathered by exact text, so neither the
        # boundary rule nor the grammar rule reports them.
        ("the two reconstructed ENTRY headings are accepted by exact text", 0,
         ["# Changelog", V5, "### Added",
          "## [0.7.5] – [0.7.0] — 2026-06-21…22", "## [0.6.x] and earlier — 2026-06 (reconstructed)"]),
        ("a NEW heading in the reconstructed style is not accepted", 1,
         ["# Changelog", V5, "### Added", "## [0.3.x] and earlier — 2025-12 (reconstructed)"]),
        ("a version below the reconstructed history is a finding", 1,
         ["# Changelog", V5, "## [0.6.x] and earlier — 2026-06 (reconstructed)", V4]),
        # -- the category rule ---------------------------------------------------
        ("the six categories in the spec's order pass", 0,
         ["# Changelog", V5, "### Added", "- x", "### Changed", "- y",
          "### Deprecated", "- z", "### Removed", "- w", "### Fixed", "- v",
          "### Security", "- u"]),
        ("a second section for the same category is a finding", 1,
         ["# Changelog", V5, "### Changed", "- x", "### Fixed", "- y", "### Fixed", "- z"]),
        # `[0.9.7]`'s actual shape: Fixed, Changed, Fixed is BOTH misordered and
        # duplicated, and the two are separate defects with separate remedies.
        ("Fixed / Changed / Fixed reports the order and the duplicate", 2,
         ["# Changelog", V5, "### Fixed", "- x", "### Changed", "- y", "### Fixed", "- z"]),
        ("Fixed above Changed is a finding", 1,
         ["# Changelog", V5, "### Fixed", "- x", "### Changed", "- y"]),
        ("an invented category name is a finding", 1,
         ["# Changelog", V5, "### Changed", "- x", "### Known issues", "- y"]),
        ("each entry is judged on its own", 0,
         ["# Changelog", V5, "### Changed", "- x", "### Fixed", "- y",
          V4, "### Changed", "- z", "### Fixed", "- w"]),
        ("a category heading inside a fence is data", 0,
         ["# Changelog", V5, "### Fixed", "- x", "```", "### Changed", "```"]),
        ("a closing-# run is stripped, so `### Fixed ###` is Fixed", 0,
         ["# Changelog", V5, "### Changed ##", "- x", "### Fixed ###", "- y"]),
        # -- indentation: CommonMark reads 0-3 columns as a heading ---------------
        # The first rule matched `^### ` only, so an indented heading bypassed it.
        ("categories indented 1, 2 and 3 spaces are read as headings (valid order)", 0,
         ["# Changelog", V5, " ### Added", "- x", "  ### Changed", "- y", "   ### Fixed", "- z"]),
        ("a duplicate hidden by one space of indentation is a finding", 1,
         ["# Changelog", V5, "### Fixed", "- x", " ### Fixed", "- y"]),
        ("an invented category hidden by two spaces is a finding", 1,
         ["# Changelog", V5, "### Fixed", "- x", "  ### Known issues", "- y"]),
        ("a misorder hidden by three spaces is a finding", 1,
         ["# Changelog", V5, "### Fixed", "- x", "   ### Changed", "- y"]),
        ("four spaces after a blank line is an indented code block, not a heading", 0,
         ["# Changelog", V5, "### Fixed", "- x", "", "text", "", "    ### Bogus"]),
        ("`####x` and `###x` without a space are not headings", 0,
         ["# Changelog", V5, "### Fixed", "- x", "###Bogus", "####Bogus"]),
        # -- preamble versus entries ----------------------------------------------
        # The first rule started an "entry" at every `## `, so a preamble section
        # became one and its sub-headings were reported as invented categories.
        ("a plain preamble is not an entry", 0,
         ["# Changelog", "", "All notable changes.", "", V5, "### Fixed", "- x"]),
        ("a preamble level-two section with level-three sub-headings is not an entry", 0,
         ["# Changelog", "## How to read this file", "### Conventions", "- a", "### Evidence",
          "- b", V5, "### Fixed", "- x"]),
        ("`## [Unreleased]` above the first release, with its definition", 0,
         ["# Changelog", "## [Unreleased]", "### Added", "- x", V5, "### Fixed", "- y",
          "[Unreleased]: https://github.com/skyRolly/Anamorph/compare/v0.5.0...HEAD"]),
        ("`## [Unreleased]` alone, with its definition", 0,
         ["# Changelog", "## [Unreleased]",
          "[Unreleased]: https://github.com/skyRolly/Anamorph/compare/v0.4.0...HEAD"]),
        ("`## [Unreleased]` without a definition is a finding", 1,
         ["# Changelog", "## [Unreleased]", "### Added", "- x"]),
        # The FORM, not merely the `...HEAD` suffix: a definition pointing at
        # another repository, or at no tag at all, satisfied the suffix test.
        ("an `[Unreleased]` definition on another host is a finding", 1,
         ["# Changelog", "## [Unreleased]", "### Added", "- x",
          "[Unreleased]: https://example.com/x/compare/v0.5.0...HEAD"]),
        ("an `[Unreleased]` definition that names no tag is a finding", 1,
         ["# Changelog", "## [Unreleased]", "### Added", "- x", V7, "### Fixed", "- y",
          "[Unreleased]: https://github.com/skyRolly/Anamorph/compare/main...HEAD", D7]),
        # The comparison runs from the NEWEST released version, not any tag: an
        # `[Unreleased]` pointing at an older one silently misreports what is
        # unreleased.
        ("an `[Unreleased]` comparison from the wrong version is a finding", 1,
         ["# Changelog", "## [Unreleased]", "### Added", "- x", V8, "### Fixed", "- y",
          V7, "### Fixed", "- z",
          "[Unreleased]: https://github.com/skyRolly/Anamorph/compare/v0.9.7...HEAD", D8, D7]),
        ("...and from the newest one it passes", 0,
         ["# Changelog", "## [Unreleased]", "### Added", "- x", V8, "### Fixed", "- y",
          V7, "### Fixed", "- z",
          "[Unreleased]: https://github.com/skyRolly/Anamorph/compare/v0.9.8...HEAD", D8, D7]),
        ("`## [Unreleased]` below a release is a finding", 1,
         ["# Changelog", V5, "### Fixed", "- x", "## [Unreleased]", "### Added", "- y",
          "[Unreleased]: https://github.com/skyRolly/Anamorph/compare/v0.5.0...HEAD"]),
        ("several releases in order pass", 0,
         ["# Changelog", "## [0.6.0] — 2026-01-03", "### Added", "- a", V5, "### Fixed", "- b",
          V4, "### Changed", "- c"]),
        # -- entry-heading grammar -------------------------------------------------
        ("an undated version heading is a finding", 1,
         ["# Changelog", "## [0.5.0]", "### Fixed", "- x"]),
        ("`— Unreleased` on a version heading is a finding (use `## [Unreleased]`)", 1,
         ["# Changelog", "## [0.5.0] — Unreleased", "### Fixed", "- x"]),
        ("a two-part version is a finding", 1,
         ["# Changelog", "## [0.5] — 2026-01-02", "### Fixed", "- x"]),
        ("a non-ISO date is a finding", 1,
         ["# Changelog", "## [0.5.0] — 02/01/2026", "### Fixed", "- x"]),
        ("an ISO-shaped date that is not a calendar date is a finding", 1,
         ["# Changelog", "## [0.5.0] — 2026-13-02", "### Fixed", "- x"]),
        ("the spec's own hyphen separator is accepted", 0,
         ["# Changelog", "## [0.5.0] - 2026-01-02", "### Fixed", "- x"]),
        ("`[YANKED]` is accepted", 0,
         ["# Changelog", "## [0.5.0] — 2026-01-02 [YANKED]", "### Fixed", "- x"]),
        ("an indented version heading renders but cannot be published: a finding", 1,
         ["# Changelog", " ## [0.5.0] — 2026-01-02", "### Fixed", "- x"]),
        # -- newest first ----------------------------------------------------------
        ("an older version above a newer one is a finding", 1,
         ["# Changelog", V4, "### Fixed", "- x", V5, "### Fixed", "- y"]),
        ("the same version twice is a finding", 1,
         ["# Changelog", V5, "### Fixed", "- x", V5, "### Fixed", "- y"]),
        ("a patch bump is compared numerically, not textually", 0,
         ["# Changelog", "## [0.5.10] — 2026-01-03", "### Fixed", "- x",
          "## [0.5.9] — 2026-01-02", "### Fixed", "- y"]),
        # -- link definitions --------------------------------------------------------
        # `V7`/`D7` are the line's FIRST tag, which points at its own tag page;
        # `V8`/`D8` is the release after it, which compares against the entry
        # directly above it. Accepting either form for either version -- the first
        # spelling of the rule -- let a comparison against a never-cut tag pass.
        ("the first tagged version with its tag-page definition passes", 0,
         ["# Changelog", V7, "### Fixed", "- x", D7]),
        ("a later version comparing against the entry above it passes", 0,
         ["# Changelog", V8, "### Fixed", "- x", V7, "### Fixed", "- y", D8, D7]),
        ("the first tag written as a comparison is a finding", 1,
         ["# Changelog", V7, "### Fixed", "- x",
          "[0.9.7]: https://github.com/skyRolly/Anamorph/compare/v0.9.6...v0.9.7"]),
        ("a later version written as a tag page is a finding", 1,
         ["# Changelog", V8, "### Fixed", "- x", V7, "### Fixed", "- y",
          "[0.9.8]: https://github.com/skyRolly/Anamorph/releases/tag/v0.9.8", D7]),
        ("a comparison against the wrong predecessor is a finding", 1,
         ["# Changelog", V8, "### Fixed", "- x", V7, "### Fixed", "- y",
          "[0.9.8]: https://github.com/skyRolly/Anamorph/compare/v0.9.2...v0.9.8", D7]),
        ("a definition on another host is a finding", 1,
         ["# Changelog", V7, "### Fixed", "- x",
          "[0.9.7]: https://example.com/x/releases/tag/v0.9.7"]),
        ("a tagged-era version WITHOUT a definition is a finding", 1,
         ["# Changelog", V7, "### Fixed", "- x"]),
        ("a definition that names another version's tag is a finding", 1,
         ["# Changelog", V7, "### Fixed", "- x",
          "[0.9.7]: https://github.com/skyRolly/Anamorph/releases/tag/v0.9.9"]),
        ("a definition for a version with no entry is a finding", 1,
         ["# Changelog", V7, "### Fixed", "- x", D7,
          "[1.0.0]: https://github.com/skyRolly/Anamorph/releases/tag/v1.0.0"]),
        ("a definition for a never-tagged version is a finding", 1,
         ["# Changelog", V5, "### Fixed", "- x",
          "[0.5.0]: https://github.com/skyRolly/Anamorph/releases/tag/v0.5.0"]),
        ("non-version definitions are not the checker's business", 0,
         ["# Changelog", V5, "### Fixed", "- x",
          "[Keep a Changelog]: https://keepachangelog.com/en/1.1.0/"]),
        # A definition the RENDERER accepts must not be reported as missing:
        # CommonMark allows 0-3 columns of indent, a title, and `<...>`.
        ("an indented definition is still a definition", 0,
         ["# Changelog", V7, "### Fixed", "- x", "  " + D7]),
        ("a definition with a title is still a definition", 0,
         ["# Changelog", V7, "### Fixed", "- x", D7 + ' "the 0.9.7 tag"']),
        ("an angle-bracketed destination is read without the brackets", 0,
         ["# Changelog", V7, "### Fixed", "- x",
          "[0.9.7]: <https://github.com/skyRolly/Anamorph/releases/tag/v0.9.7>"]),
        # -- a malformed heading does not orphan its own definition ----------------
        ("a malformed heading is reported once, not twice", 1,
         ["# Changelog", "## [0.9.7] — <YYYY-MM-DD>", "### Fixed", "- x", D7]),
        # -- headings that render but cannot be extracted --------------------------
        ("a tab after `##` renders but cannot be published: a finding", 1,
         ["# Changelog", "##\t[0.9.7] — 2026-09-05", "### Fixed", "- x", D7]),
        ("two spaces after `##` are the same defect", 1,
         ["# Changelog", "##  [0.9.7] — 2026-09-05", "### Fixed", "- x", D7]),
        ("an older entry in that form is reported where it stands", 1,
         ["# Changelog", V8, "### Fixed", "- x", "##\t[0.9.7] — 2026-09-05", "### Fixed",
          "- y", D8, D7]),
        # -- a heading indented into a list item still renders ---------------------
        ("a category indented four columns under a bullet is reported", 1,
         ["# Changelog", V7, "### Fixed", "- x", "    ### Security", "- y", D7]),
        ("...and its duplicate is caught there too", 2,
         ["# Changelog", V7, "### Fixed", "- x", "    ### Fixed", "- y", D7]),
        # One TAB is four columns (`indent_columns`), so a tab-indented category
        # is the same bypass wearing a different character.
        ("a tab-indented category under a bullet is reported", 1,
         ["# Changelog", V7, "### Changed", "- x", "\t### Fixed", "- y", D7]),
        ("a deeply indented ENTRY heading is reported", 1,
         ["# Changelog", V7, "### Fixed", "- x", "    ## [0.9.6] — 2026-09-01", D7]),
        # ...but a code SAMPLE at that depth is not structure. Only a real category
        # name (or an entry heading) is reported, because nothing else can be told
        # apart from a sample without a container stack.
        ("a deep heading that is not a category name is left alone", 0,
         ["# Changelog", V7, "### Fixed", "- x", "    ### How to read this", "- y", D7]),
        ("a deep `#` sample inside a bullet is left alone", 0,
         ["# Changelog", V7, "### Fixed", "- x", "        # not a category", "- y", D7]),
        # -- setext headings are invisible to the extractor ------------------------
        ("a setext heading inside an entry is a finding", 1,
         ["# Changelog", V7, "### Fixed", "- x", "", "Acknowledgements", "---", "", D7]),
        ("a table delimiter row is not a setext heading", 0,
         ["# Changelog", V7, "### Fixed", "- x", "", "| a | b |", "|---|---|", "| 1 | 2 |",
          D7]),
        # A thematic break is not a setext underline. The first spelling of the
        # guard tested the previous line's first character, so the break under
        # this file's own link definitions was reported as a heading, as was a
        # paragraph that merely began with `-`.
        ("a thematic break under the link definitions is not a heading", 0,
         ["# Changelog", V7, "### Fixed", "- x", "", D7, "---"]),
        ("a thematic break under a list item is not a heading", 0,
         ["# Changelog", V7, "### Fixed", "- x", "---", "", D7]),
        ("a thematic break under an ordered list item is not a heading", 0,
         ["# Changelog", V7, "### Fixed", "- x", "", "1. step", "---", "", D7]),
        ("a thematic break under an HTML block is not a heading", 0,
         ["# Changelog", V7, "### Fixed", "- x", "", "<div>", "---", "", D7]),
        ("a paragraph beginning with `-` still carries a setext underline", 1,
         ["# Changelog", V7, "### Fixed", "- x", "", "-not a list item", "---", "", D7]),
        # -- a first heading the extractor cannot read must not disarm the rest ----
        ("a stray `## ` section is still reported after a badly spelled first entry", 2,
         ["# Changelog", "##  [0.9.7] — 2026-09-05", "### Fixed", "- x", "## Acknowledgements",
          D7]),
        # -- link labels and messages ----------------------------------------------
        ("a label with inner spaces is normalised, as CommonMark normalises it", 0,
         ["# Changelog", V7, "### Fixed", "- x", "[ 0.9.7 ]: " + D7.split(": ", 1)[1]]),
        ("a tagged-era version with no older entry says so, without a placeholder", 1,
         ["# Changelog", V8, "### Fixed", "- x", D8]),
        # ...and the message must not contain the sentinel: a `v?` in the URL an
        # author is told to write is worse than no message at all.
        ("...and the message never prints `v?`", 0,
         ["# Changelog", V8, "### Fixed", "- x", D8, "@@no-placeholder@@"]),
    ]:
        # A fixture carrying the `@@no-placeholder@@` marker asserts the TEXT of
        # the findings instead of their count: the defect it pins is a sentinel
        # (`v?`) leaking into the URL a finding tells the author to write, which
        # no count can see.
        if lines and lines[-1] == "@@no-placeholder@@":
            found = analyse(root / "CHANGELOG.md", lines[:-1], root)
            checked += 1
            if any("v?" in f for f in found):
                failures += 1
                print(f"self-test FAIL: changelog {label}: a finding printed the `v?` "
                      f"placeholder: {found}", file=sys.stderr)
            continue
        got = len(analyse(root / "CHANGELOG.md", lines, root))
        checked += 1
        if got != expected:
            failures += 1
            print(f"self-test FAIL: changelog {label}: expected {expected}, got {got}",
                  file=sys.stderr)

    for raw, want in [("`a`", "   "), ("x `|` y", "x     y"), ("``a`b`` c", "        c")]:
        got_line = blank_code_spans(raw)
        checked += 1
        if got_line != want:
            failures += 1
            print(
                f"self-test FAIL: blank_code_spans({raw!r}) -> {got_line!r}, want {want!r}",
                file=sys.stderr,
            )

    # --- WHICH FILES GET SCANNED AT ALL ---------------------------------------
    # The checks above all answer "is this file well-formed"; none of them
    # notices that no file was handed over. `markdown_files` filtered on the
    # ABSOLUTE path's components, so a checkout under a directory named `build`,
    # `JUCE` or `node_modules` -- a plausible place to put one -- matched the
    # skip set on a directory the scan does not own and excluded everything,
    # while `main()` printed `0 file(s) clean`. Both halves are pinned here: the
    # ancestor must not decide, and an empty result must not pass.
    with tempfile.TemporaryDirectory() as tmp:
        for parent in ("build", "build-san", "cmake-build-debug", "JUCE",
                       "node_modules", ".git", "_deps"):
            root = Path(tmp) / parent / "checkout"
            (root / "docs").mkdir(parents=True)
            (root / "README.md").write_text("# R\n")
            (root / "docs" / "GUIDE.md").write_text("# G\n")
            got = sorted(p.name for p in markdown_files([root]))
            checked += 1
            if got != ["GUIDE.md", "README.md"]:
                failures += 1
                print(f"self-test FAIL: a checkout under {parent!r} scanned {got}, "
                      f"want ['GUIDE.md', 'README.md']", file=sys.stderr)

        # ...and the exclusions that are REAL must survive the repair. These sit
        # inside the scan root, which is the only place a skip rule may speak.
        root = Path(tmp) / "repo"
        for d in ("docs", "build", "build-san", "cmake-build-debug", "JUCE",
                  "node_modules", ".git", "_deps", "building", "rebuild"):
            (root / d).mkdir(parents=True)
            (root / d / f"{d}.md").write_text("# x\n")
        (root / "README.md").write_text("# R\n")
        got = sorted(p.name for p in markdown_files([root]))
        # `building` and `rebuild` are NOT build trees -- named, not prefixed --
        # and dropping them would be the same defect wearing the opposite sign.
        want = ["README.md", "building.md", "docs.md", "rebuild.md"]
        checked += 1
        if got != want:
            failures += 1
            print(f"self-test FAIL: in-repository filtering scanned {got}, want {want}",
                  file=sys.stderr)

        # THE DEPENDENCY CACHE AT THE REPOSITORY ROOT, which is the shape the
        # build-tree rule cannot see: `build/_deps/juce-src` has a `build`
        # ancestor, a top-level `_deps/juce-src` has none, and `.gitignore`
        # allows both. Written as the real layout rather than a bare directory
        # name, because it is the nested file that was being reported.
        (root / "_deps" / "juce-src").mkdir(parents=True)
        (root / "_deps" / "juce-src" / "README.md").write_text("# J\n\n| a | b |\n| c | d |\n")
        checked += 1
        if sorted(p.name for p in markdown_files([root])) != want:
            failures += 1
            print("self-test FAIL: a root-level _deps cache was scanned", file=sys.stderr)

        # A nested build tree is excluded at any depth, not only at the top.
        (root / "docs" / "build" / "gen").mkdir(parents=True)
        (root / "docs" / "build" / "gen" / "API.md").write_text("# A\n")
        checked += 1
        if sorted(p.name for p in markdown_files([root])) != want:
            failures += 1
            print("self-test FAIL: a nested build tree was scanned", file=sys.stderr)

        # AN EMPTY SCAN MUST NOT EXIT 0. This is the backstop: whatever future
        # change empties the set, the run must not call it clean.
        empty = Path(tmp) / "empty"
        empty.mkdir()
        buf = io.StringIO()
        with contextlib.redirect_stderr(buf):
            rc = main(["check-docs.py", str(empty)])
        checked += 1
        if rc == 0:
            failures += 1
            print("self-test FAIL: an empty scan set reported a clean run", file=sys.stderr)

    # Counted as they run, never hand-maintained: the previous literal
    # (`len(cases) + 2 + 5 + 3`) drifted the moment a case was added, and the
    # stale figure reached a navigation document before anyone noticed.
    if failures:
        print(f"\ncheck-docs: {failures} of {checked} self-test case(s) failed.", file=sys.stderr)
        return 1
    print(f"check-docs: self-test passed ({checked} cases).")
    return 0


def main(argv: list[str]) -> int:
    if "--self-test" in argv[1:]:
        return self_test()

    repo = Path(__file__).resolve().parent.parent
    roots = [Path(a) for a in argv[1:]] or [repo]
    for root in roots:
        if not root.exists():
            print(f"check-docs: no such path: {root}", file=sys.stderr)
            return 1
        # An existing file with the wrong suffix would otherwise contribute no
        # files and be reported as "0 file(s) clean" -- a false pass for anyone
        # running this by hand on a typo'd path (`TESTING.md` tells contributors
        # to). Only reachable outside CI, which passes no arguments.
        if root.is_file() and root.suffix != ".md":
            print(f"check-docs: not a Markdown file: {root}", file=sys.stderr)
            return 1

    files = markdown_files(roots)
    # AN EMPTY SCAN IS NEVER A CLEAN RUN. Reporting `0 file(s) clean` is a pass,
    # and every way of reaching it here is a mistake: a directory with no
    # documents in it, a skip rule that swallowed the whole tree, or a path that
    # is not the checkout the caller thought it was. The filtering bug above made
    # that reachable on a correct tree; this is the guard that would have caught
    # it independently, and it stays as the backstop for the next way of getting
    # there. Exit 1, like the other two argument errors -- this file's contract
    # is `0 = clean, 1 = findings`, and "nothing was checked" is not clean.
    if not files:
        print(f"check-docs: no Markdown files found under "
              f"{', '.join(str(r) for r in roots)} -- refusing to report a clean "
              f"run over an empty set", file=sys.stderr)
        return 1

    findings: list[str] = []
    for path in files:
        findings += check_file(path, repo)

    if findings:
        for finding in findings:
            print(finding, file=sys.stderr)
        print(
            f"\ncheck-docs: {len(findings)} finding(s) across {len(files)} file(s).",
            file=sys.stderr,
        )
        return 1

    print(f"check-docs: {len(files)} file(s) clean.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
