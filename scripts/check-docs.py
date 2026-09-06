#!/usr/bin/env python3
"""Structural lint for the Anamorph documentation set.

PROVENANCE, stated as it stands rather than as it started. Checks 1-4 were adopted
from the sibling product Anabasis (`scripts/check-docs.py`). Six functions are still
BYTE-FOR-BYTE identical to it: `indent_columns`, `indented_code_mask`, `blanked_lines`,
`interrupts_paragraph`, `check_tables`, `check_links` and `check_lazy_continuation`.
They are structural properties of GitHub-Flavored Markdown, not of either product, so
there was nothing to adapt -- and a diverged copy of a checker is worse than a shared
one. The defects each of them names happened in Anabasis; they are reproducible in any
document set written the same way, which this one is (same directory layout, same
navigation documents, same ADR index).

`fence_mask` and `FENCE` have DIVERGED, deliberately, and the reason is in this
repository rather than in the sibling: only here does a second implementation of the
same grammar exist -- `scripts/changelog-section.awk`, which publishes the release
notes -- and the two were giving opposite answers about the same line. A tab-indented
delimiter (the sibling's `\s{0,3}` matches a tab; four columns, so CommonMark calls it
an indented code block) and a backtick inside a backtick fence's info string (never a
fence at all, CommonMark 4.5) both masked a real `## [x.y.z]` entry heading here while
the extractor still saw it. `fence_delimiter` below states both rules. Anabasis has no
extractor to disagree with, so it carries the same latent defect harmlessly; porting
this back is a product-family decision for that repository, not a change to make from
this one.

Check 5 is where the two files have diverged, deliberately and by about 930 lines. The
sibling carries a single `check_changelog_notes_boundary` that reads raw `## ` prefixes;
this file's version of that rule was rewritten to share `parse_changelog` with three
further rules -- the heading grammar and ordering, the Keep a Changelog categories, and
the version link definitions -- none of which the sibling has. Those four, the parser,
and the self-test that RUNS `scripts/changelog-section.awk` are this repository's own,
and their counterpart is `release.yml`'s extractor, not the sibling.


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

  4. UNCLOSED FENCE -- an opening code fence with no closer makes the rest of the
     file render as code on GitHub. It is a real rendering defect on its own, and
     it is also this script's worst failure mode: an unclosed fence exempts every
     line after it from checks 1-3. That happened here -- a prose line in
     DOCUMENTATION_COVERAGE.md began with three backticks, masking 1382 of its
     1401 lines while the run still printed "clean". A checker that reports
     success without having read the file is worse than no checker, so an
     unterminated fence is now a finding rather than a silent exemption.

  5. CHANGELOG STRUCTURE (`CHANGELOG.md` only, four rules that share one parser)
     -- the entry-boundary rule `release.yml`'s note extractor depends on, the
     entry-heading grammar and newest-first order, Keep a Changelog's six category
     names in their specified order once each per release, and the version link
     definitions. See `parse_changelog` and the four `check_changelog_*` rules
     that read it -- `check_changelog_notes_boundary` sits above the parser, the
     other three below it; the contract they enforce is
     `docs/policies/CHANGELOG_POLICY.md`.
     The extractor those rules protect is `scripts/changelog-section.awk`, and
     `--self-test` RUNS it: the entry-boundary rule's premise -- that a stray
     `## ` heading below an entry lands in the published notes, and that a fenced
     `## [` line does not -- is proved on fixtures rather than asserted in a
     comment, so a regression in the extractor fails here instead of at tag time.

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
  * A `### Fixed` indented four columns or more with no blank line above it and no
    list container is an indented code block to CommonMark, and a hidden category
    heading to `deep_heading`. The mask needs a preceding blank line and cannot
    see containers without a container stack, so the two cannot be told apart
    here. The rule errs toward REPORTING, and only for the six category names:
    over-reporting costs an author one blank line, under-reporting is the bypass
    the rule exists to close. Measured over the 47-fixture grammar matrix against
    a CommonMark renderer, this checker never sees LESS structure than the
    renderer does -- these two fixtures are the only two where it sees more.
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
import shutil
import subprocess
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
# A line that LOOKS like a fence delimiter. Whether it IS one is decided by
# `fence_delimiter` below, which applies the two rules a regex cannot: the
# three-column indent allowance measured in COLUMNS, and CommonMark 4.5's
# ban on a backtick inside a backtick fence's info string.
FENCE = re.compile(r"^[ \t]*(`{3,}|~{3,})(.*)$")
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


def fence_delimiter(line: str) -> tuple[str, int, str] | None:
    """(character, run length, info string) if `line` is a fence delimiter, else None.

    THE TWO RULES A REGEX CANNOT STATE, both CommonMark section 4.5 and both
    live defects rather than pedantry:

      * THE INDENT ALLOWANCE IS THREE **COLUMNS**. One tab is four columns, so a
        tab-indented ``` is an indented code block, not a fence. Measured in
        characters it opened one -- and in CHANGELOG.md that masked a real
        `## [x.y.z]` entry heading from every rule here while
        `changelog-section.awk`, which counts columns, still saw it. Two
        grammars, one document, opposite answers.
      * A BACKTICK FENCE'S INFO STRING MAY NOT CONTAIN A BACKTICK. ```a`b is a
        PARAGRAPH, not a fence, so nothing after it is code. Reading it as a
        fence hid every following line until the next delimiter -- including the
        next release's heading, which then vanished from this checker while the
        published notes ran the two releases together. A TILDE fence has no such
        restriction: ~~~a`b IS a fence.

    Info-string rules apply to an OPENING fence. A closer is a delimiter whose
    info string is blank, so the backtick rule cannot change a closer's verdict
    and is applied by the caller only where it opens.
    """
    if indent_columns(line) > 3:
        return None
    return fence_run(line)


def fence_run(text: str) -> tuple[str, int, str] | None:
    """(character, run length, info string) if `text` opens or closes a fence.

    THE DELIMITER ITSELF, with no opinion about where it sits. Indentation is the
    caller's question, because the allowance is three columns measured from the
    CONTENT COLUMN of whatever contains the line -- column 0 at top level, the
    blockquote's content inside a quote, the marker's width inside a list item.
    One primitive, three callers: `fence_delimiter` for the top-level case,
    `fence_mask` for a fence inside a container, and `parse_changelog`'s deep
    pass. Whatever else they disagree about, they cannot disagree about what a
    fence delimiter IS.
    """
    m = FENCE.match(text)
    if m is None:
        return None
    run = m.group(1)
    return run[0], len(run), m.group(2)


def opens_fence(run: tuple[str, int, str] | None) -> bool:
    """CommonMark 4.5: a BACKTICK fence's info string may not contain a backtick.
    Such a line is a paragraph and opens nothing; a tilde fence has no such
    restriction. Stated once, because every caller needs it and none of them
    should re-derive it."""
    return run is not None and not (run[0] == "`" and "`" in run[2])


def container_chains(lines: list[str]) -> list[tuple[tuple[int, int], ...]]:
    """Per line, the chain of LIST ITEMS it sits in, as `(quote_depth, column)`.

    THE ONE PIECE OF CROSS-LINE CONTAINER STATE THIS FILE KEEPS, and both the
    fence rules and the heading rules read it from here rather than each building
    their own.

    ONE RULE FOR EVERY LINE, and it is TRIM THEN EXTEND:

      * TRIM the carried chain to the longest PREFIX this line still reaches --
        each item read in its own quote frame, a line that does not carry an
        item's depth having dedented past every column in it. Containers nest, so
        what survives is always a prefix: leaving an outer item leaves every
        inner one with it.
      * EXTEND that prefix with the markers this line states for itself, whose
        columns `strip_containers` has already measured in the right frame.
      * A BLANK line changes nothing, because a blank does not end a list item.

    A MARKER LINE IS NOT A FRESH START, and reading it as one was a bypass. It
    used to REPLACE the chain with its own markers, so `- outer` / `  - nested`
    left only the nested item -- and the outer item, which the nested one sits
    inside and which is still open, was gone. The next continuation line then
    fell below the nested column, found nothing left to fall back to, and read as
    top level. A fence opened there was recorded as belonging to NO item, and
    nothing but a closing delimiter could end it: the `## [x.y.z]` at column 0
    that ends the list, the fence and the block in every renderer was masked, and
    the same erasure in `changelog-section.awk` ran the two releases together in
    the published notes. In the other direction the erased column made a
    four-column delimiter inside a two-column item read as an indented code block
    at top level, so a valid fenced sample was scanned as live structure.

    It is what tells a line indented two columns INSIDE an item from one indented
    two columns at top level -- a distinction the renderer makes and no
    single-line rule can. Without it a fence opened on a continuation line had no
    column at all, and a release heading four columns after `> -   item` -- two
    columns inside that item, and a heading to every renderer -- was read as an
    indented code block.
    """
    out: list[tuple[tuple[int, int], ...]] = []
    context: tuple[tuple[int, int], ...] = ()
    for line in lines:
        if not line.strip():
            out.append(context)            # a blank does not end an item
            continue
        keep: list[tuple[int, int]] = []
        for ctx_depth, ctx_col in context:
            ctx_rest = quote_rest(line, ctx_depth)
            if ctx_rest is None or indent_of(ctx_rest) < ctx_col:
                break
            keep.append((ctx_depth, ctx_col))
        # The indentation compared above is the one BEFORE this line's own
        # markers, which is exactly the test CommonMark applies: an outer item
        # matches by indentation first, and only what is left of the line may
        # open a new item inside it. So a marker at an outer item's content
        # column extends the chain, and one at column 0 replaces it -- both fall
        # out of the same two steps rather than needing a case of their own.
        context = tuple(keep) + strip_containers(line)[4]
        out.append(context)
    return out


def chain_column(chain: tuple[tuple[int, int], ...], depth: int) -> int:
    """The innermost item's content column IN THIS QUOTE FRAME, or 0.

    An item recorded at a shallower depth imposes no column inside the quote:
    `- > ` puts one at depth 0 while the line's own content sits at depth 1, and
    subtracting a document column from a quote-relative one stretched every
    allowance measured with it.
    """
    return chain[-1][1] if chain and chain[-1][0] == depth else 0


def fence_mask(lines: list[str]) -> tuple[list[bool], int | None]:
    """(mask, unclosed_opener_line) — True for lines inside or delimiting a fence.

    A closing fence must use the opener's character, be at least as long, and
    carry **nothing but trailing whitespace** (CommonMark §4.5) -- a line with an
    info string is an *opening* fence, never a closer. Both conditions matter:
    without the length rule a shorter run inside a longer block ends it early;
    without the info-string rule a nested ```cpp inside a ```markdown example
    closes the outer block, so the example's contents get scanned as real
    structure and the real closer re-opens a block that then reads as unclosed.

    What counts as a delimiter at all is `fence_delimiter`: three COLUMNS of
    indent at most, and no backtick in a backtick fence's info string. Its
    docstring records why each of those is here.

    The second element is the 1-based line of an opener that was never closed,
    or None. Callers must report it: silently masking to EOF is how this script
    once passed a file it had not read.
    """
    mask: list[bool] = [False] * len(lines)
    char: str | None = None
    width = 0
    opened_at: int | None = None
    depth = 0
    items: tuple[tuple[int, int], ...] = ()  # the list items the fence was opened in
    open_col = 0
    chains = container_chains(lines)
    for i, line in enumerate(lines):
        prefix, content, line_depth, _, line_items = strip_containers(line)
        context = chains[i]
        run = fence_run(content.lstrip(" \t"))
        if char is not None:
            # ---- INSIDE A FENCE: does this line leave the container? ----------
            #
            # HAS THE LINE LEFT THE BLOCKQUOTE THE FENCE WAS OPENED IN? A quote
            # ends at any line below its depth -- a truly blank line included,
            # since one strips to depth 0 while `>` alone does not (that is a
            # quoted blank, and it stays inside). The renderer then shows a
            # heading on the next quoted line, so the fence ends WITHOUT masking
            # this one; not seeing that heading is the bypass direction. No
            # separate blank-line clause: it was subsumed by the depth test, and a
            # redundant condition is what a mutation test cannot tell from a rule.
            #
            # DEPTH ONLY, never the column, for a fence that is NOT in a list
            # item. Whether a fence indented three columns is a top-level fence
            # (its content may sit at column 0 and still be code) or a list item's
            # fence (where column 0 leaves the item) cannot be told apart without
            # knowing the item, and a fence opened on a line with no list marker
            # does not. Ending on the column broke the top-level case; masking on
            # it is what the file has always done. The ambiguity is recorded in
            # KNOWN LIMITS, not resolved here.
            #
            # ...AND, FOR A FENCE OPENED IN A LIST ITEM, ONE MORE TEST AND ONLY
            # ONE: the line's own INDENTATION, read at the depth the item sits in,
            # against the item's content column. A line still indented to that
            # column is the item's content and so is the fence's; a shallower one
            # has left the item, so the renderer ends the item, the fence and
            # anything between, and shows what follows. That single number decides
            # every shape the renderer was asked about: `- ```text` ends at a
            # column-0 `## [x.y.z]` (a real entry heading, which the extractor
            # would have cut at) and at ` - x` one column in, and does NOT end at
            # `  - x`; `> - ```text` ends at `> - x` and not at `>   - x`;
            # `- > ```text` ends at `> x` and not at `  > x`.
            #
            # WHAT IS NOT A TEST: whether the line carries a LIST MARKER. It was
            # one, and it is the defect this replaces -- the old clause fired on
            # any bullet, so `- ```text` followed by its own indented `- item`
            # broke the fence open and the example's `### Fixed` and
            # `## [x.y.z]` were read as live structure. A fenced example holding a
            # Markdown list is the most ordinary thing a changelog preamble can
            # carry, and it failed CI. Content inside a fence is DATA; only the
            # depth, the item's column, or a genuine closer may end it.
            #
            # A blank line is not a dedent -- it stays inside the fence, as it
            # does in the renderer. `quote_rest` returns None where the line does
            # not reach the item's quote depth, which is a dedent past every
            # column.
            ended = line_depth < depth
            if not ended and items:
                # EVERY enclosing item, not just the innermost: a line that leaves
                # an OUTER one takes the inner containers -- and the fence -- with
                # it, and only that item's own frame can see it.
                #
                # BLANKNESS IS READ IN EACH ITEM'S OWN FRAME. A line that is empty
                # inside the frame is not a dedent: `>` alone is a QUOTED BLANK and
                # stays inside a fence opened in a quoted item, exactly as a bare
                # blank line stays inside one opened at top level. Testing
                # `line.strip()` on the raw line called `>` non-blank -- it strips
                # to `>` -- so its zero columns of content read as a dedent and
                # broke the fence open in the middle of a quoted sample.
                for item_depth, item_col in items:
                    rest = quote_rest(line, item_depth)
                    if rest is None:
                        ended = True
                        break
                    if not rest.strip():
                        continue                # blank in this frame: not a dedent
                    if indent_of(rest) < item_col:
                        ended = True
                        break
            if not ended:
                mask[i] = True               # inside the fence, including its closer
                # `strip(" \t")`, never a bare `strip()`. CommonMark 4.5 allows
                # spaces and TABS after a closing run and nothing else; Python's
                # argument-less strip removes every Unicode space, so a closer
                # trailed by a non-breaking space closed the fence HERE and not in
                # `changelog-section.awk`, whose test is `/^[ \t\r]*$/`. One
                # character, and the two tools disagreed about where a release
                # ends -- the checker calling the file clean while the extractor
                # ran two releases together.
                #
                # The closer's allowance is three columns measured inside its own
                # container, exactly as the opener's is -- `>    ``` ` closes and
                # `>     ``` ` does not, which is what the renderer says. Inside a
                # LIST item that container's content column is `open_col`, so the
                # allowance is counted from there: `-    ```text` is closed by a
                # delimiter five columns in, and measuring from column 0 called
                # the block unclosed and every line below it code -- a false
                # positive on a valid document, which is the failure this whole
                # family runs toward. `open_col` is 0 wherever no list marker
                # opened the fence, and the test is then the top-level one it has
                # always been.
                closer_rest = quote_rest(line, depth)
                closer = fence_run(closer_rest) if closer_rest is not None else None
                if closer and indent_of(closer_rest) - open_col <= 3 \
                        and closer[0] == char and closer[1] >= width \
                        and not closer[2].strip(" \t"):
                    char, width, opened_at, items = None, 0, None, ()
                continue
            # The fence ended HERE, and this line is outside it. It is therefore
            # not fence content and not a closer -- so it falls through to be
            # classified from scratch, opener test included. Skipping that step
            # left `- ```text` / `  - item` / `> ``` ` with no fence open where the
            # renderer shows one, and so with no unclosed-fence report either.
            char, width, opened_at, items = None, 0, None, ()
        # ---- NO FENCE ACTIVE: may this line open one? -------------------------
        # A fence opens where its delimiter sits: inside a quote, inside a list
        # item, or at top level. Reading the RAW line instead is what let
        # `> ```text` open nothing at all, so a fenced example in a blockquote
        # reached `classify_heading` as live structure and a valid document was
        # rejected -- the mirror image of every earlier bypass.
        # THE OPENER'S THREE-COLUMN ALLOWANCE IS COUNTED FROM ITS OWN CONTENT
        # COLUMN, which for a line carrying markers is what `strip_containers`
        # already left behind, and for a CONTINUATION line is the item's column
        # read in the fence's quote frame. Counting it from column 0 meant a
        # delimiter indented to a `10. ` item's content column -- four columns, an
        # ordinary nested sample -- opened nothing at all, and the sample's
        # headings were then read as live structure. That is the residual rounds 7
        # and 8 recorded as needing a container stack; there is one now.
        if line_items:
            open_indent = indent_columns(content)
        else:
            open_rest = quote_rest(line, line_depth)
            open_indent = 4 if open_rest is None else \
                indent_of(open_rest) - chain_column(context, line_depth)
        # AND THE CONTAINERS THEMSELVES HAVE TO BE REAL. A `>` more than three
        # columns past the content column of whatever encloses it is literal text
        # inside an INDENTED CODE BLOCK (CommonMark 5.1), not a marker -- so
        # `    > ```text` at top level opened a blockquote that is not there, and
        # the fence inside that phantom quote masked a `> ## [x.y.z]` the renderer
        # shows. `strip_containers` reads one line and cannot know that column;
        # the chain does, and this is the one place that needs it.
        real = not prefix or indent_columns(line) <= chain_column(context, 0) + 3
        if line.strip() and real and opens_fence(run) and open_indent <= 3:
            char, width, depth, items = run[0], run[1], line_depth, context
            # THE CLOSER'S ALLOWANCE IS COUNTED IN THE FENCE'S OWN QUOTE FRAME, so
            # the column it is counted from has to live in that frame too. An item
            # recorded at a SHALLOWER depth -- `- > ```text` puts one at depth 0
            # while the fence itself is at depth 1 -- imposes no column inside the
            # quote, and using its document column stretched the three-column
            # allowance to five: the checker then closed on a line the renderer
            # calls code and read the code as structure.
            open_col = chain_column(items, depth)
            opened_at, mask[i] = i + 1, True
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


def expand_tabs(text: str) -> str:
    """`text` with every tab advanced to the next four-column tab stop.

    ONE COLUMN MODEL, AND THIS IS WHERE IT STARTS. CommonMark measures every
    block-structure decision in COLUMNS, and a tab is not one column: it is
    however many take the line to the next multiple of four. Anything that
    measures a marker in CHARACTERS is therefore wrong the moment a tab appears
    before it, and mixing the two -- some indentation in columns, some in
    characters -- is what let a renderer-visible setext release heading escape
    validation: `-\tname` puts its content at column 4, `strip_containers`
    called it 2, and the underline's 0-3 allowance was then measured from the
    wrong place.

    After this call COLUMN == INDEX, so every later measurement is a character
    count that is also a column count and the two representations cannot
    diverge. Applied once, in `strip_containers`, because that is the only place
    that walks a line column by column; `indent_columns` does its own expansion
    for the callers that only need the leading run.
    """
    if "\t" not in text:
        return text
    out: list[str] = []
    col = 0
    for char in text:
        if char == "\t":
            width = 4 - (col % 4)
            out.append(" " * width)
            col += width
        else:
            out.append(char)
            col += 1
    return "".join(out)


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
        if h is None or h[0] > 2:
            continue
        # LEVEL 1 LEAKS EXACTLY AS LEVEL 2 DOES. The extractor terminates on
        # `^## [` and on nothing else, so a `# Appendix` below the entries is
        # published inside the notes of the entry above it just as `## Appendix`
        # is. The rule looked at level 2 only, so the more eye-catching spelling
        # was the one it missed.
        if release_like(h[1]) or (h[0] == 2 and CHANGELOG_VERSION_HEADING.match(line)):
            # A release-like heading is `parse_changelog`'s to report, at every
            # level: it says WHICH way the heading is wrong (unbracketed, wrong
            # level), where this rule could only say "not an entry heading". Only
            # a level-2 one ARMS the rule -- a `# [0.9.7] ...` is not where the
            # entries begin, it is a defect inside whatever entry it sits in.
            if h[0] != 2:
                continue
            # Any entry-SHAPED heading arms the rule, even one spelled in a way
            # `release.yml` cannot extract: `check_changelog_headings` reports the
            # spelling, and this rule must still see that the entries have begun.
            # Arming only on the publishable spelling meant one bad heading at the
            # top of the file silently disarmed the boundary check for the rest of
            # it -- a second, independent defect hidden behind the first.
            #
            # `release_like` rather than a bare `[`, and for the same reason:
            # `## 0.9.8] - 2026-10-01` is a release somebody wrote, not a preamble
            # section. `parse_changelog` reports the spelling; arming here and
            # saying nothing keeps one defect to one message.
            if first is None:
                first = i
            continue
        if first is not None:
            findings.append(
                f"{path}:{i + 1}: `{'#' * h[0]} ` heading that is not an entry heading (`## [`), below "
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


def awk_code_text(source: str) -> str:
    """`source` with COMMENTS, STRING LITERALS and REGEX LITERALS blanked to spaces.

    THE LEXICAL APPROXIMATION, stated so it can be argued with. `awk_spaced_calls`
    below has to answer a question about awk's GRAMMAR -- is this text a function
    call? -- and the honest way to do that without an awk parser is to remove the
    three places where text is not code, then read what is left. Every character
    removed is replaced by a space rather than deleted, so offsets and line
    numbers still point at the real file.

    Three lexical states and nothing else:

      * `#` to end of line is a comment -- BUT ONLY OUTSIDE A STRING. `"a # b"` is
        a string containing a hash, and treating it as a comment would blank the
        rest of a line that may hold a real call.
      * `"..."` is a string, `\\` escapes the next character, and an awk string
        cannot span a line, so an unterminated one ends at the newline.
      * `/.../` is a REGEX LITERAL only where a `/` cannot be division: after an
        operator, a comma, an opening bracket or at the start of a statement --
        never after an identifier, a number, a `)`, a `]` or a string. This is the
        one heuristic here, and it is the conservative way round: guessing
        "division" where a regex was meant leaves extra text to scan (at worst a
        false positive that a fixture would catch), while guessing "regex" where a
        division was meant would swallow real code and could HIDE a forbidden
        call. `--self-test` pins both directions.

    What it deliberately does not do: parse. It does not know a function from a
    variable, `getline` from a pipe, or a continued line from two statements. It
    does not have to -- the only question asked of the result is whether a name
    the file DEFINES is followed by whitespace and a `(` in code position.
    """
    out = list(source)
    i, n = 0, len(source)
    prev = ""                       # last significant CODE character seen
    while i < n:
        c = source[i]
        if c == "#":                                        # comment to line end
            while i < n and source[i] != "\n":
                out[i] = " "
                i += 1
            continue
        if c == '"' or (c == "/" and not (prev.isalnum() or prev in "_)]")):
            close = c                                       # string, or regex literal
            out[i] = " "
            i += 1
            while i < n and source[i] != close and source[i] != "\n":
                if source[i] == "\\" and i + 1 < n and source[i + 1] != "\n":
                    out[i] = " "
                    i += 1
                out[i] = " "
                i += 1
            if i < n and source[i] == close:
                out[i] = " "
                i += 1
            prev = ")"              # a literal is an operand: a `/` after it divides
            continue
        if not c.isspace():
            prev = c
        i += 1
    return "".join(out)


def awk_spaced_calls(source: str) -> list[tuple[int, str]]:
    """(1-based line, name) for every call of a function `source` DEFINES that is
    written with whitespace before its `(`.

    THE ONE THING ABOUT THE EXTRACTOR THAT NO FIXTURE CAN CATCH: whether every
    `awk` will PARSE it. POSIX gives a user-defined function's name and its `(`
    as a single token, so `qrest (x)` is read as the VARIABLE `qrest` next to a
    parenthesised expression; `mawk` accepts it anyway, while `gawk` and the
    one-true-awk reject THE WHOLE PROGRAM. `scripts/changelog-section.awk` ran
    clean locally and died on the runner in four seconds with all 37 extractor
    fixtures failing at once and the reason in none of them.

    THE GRAMMAR THAT IS FORBIDDEN, and it is narrower than the text:

      * a CALL only. `function qrest (s, k)` -- a DEFINITION with a space -- is
        accepted by gawk, mawk and the one-true-awk alike (measured, not assumed),
        so flagging it would be a false report about a portable file.
      * a name the file itself DEFINES. Built-ins are exempt: `substr (s, 1, 1)`
        is legal, portable, and this file's own house style.
      * in CODE. A comment, a string or a regex holding the characters
        `qrest (` is data -- all three awks run such a file -- and reporting it
        would fail the build over prose. `awk_code_text` removes those first.
      * the whole identifier. `myqrest (` is not a call of `qrest`, which is what
        the look-behind is for.
    """
    code = awk_code_text(source)
    defined = set(re.findall(r"^[ \t]*function[ \t]+(\w+)[ \t]*\(", code, re.M))
    # Blank the DEFINITION headers, keeping the file's length so line numbers
    # survive: a definition may portably carry the space a CALL may not, so the
    # text scanned for calls must not still contain the name that declares one.
    code = re.sub(r"^[ \t]*function[ \t]+\w+",
                  lambda m: " " * len(m.group(0)), code, flags=re.M)
    found: list[tuple[int, str]] = []
    for name in sorted(defined):
        for m in re.finditer(rf"(?<![\w.]){re.escape(name)}[ \t]+\(", code):
            found.append((code.count("\n", 0, m.start()) + 1, name))
    return sorted(found)



def atx_heading(line: str) -> tuple[int, str] | None:
    """(level, text) for an ATX heading, else None. The closing `#` run is
    stripped, as the renderer strips it; `###` alone is a heading with empty
    text; `####x` (no space) is not a heading at all."""
    m = ATX_HEADING.match(line)
    if not m:
        return None
    text = m.group(2) or ""
    # CommonMark 4.2: the closing run must be preceded by spaces or TABS -- not by
    # "whitespace" in Python's Unicode sense. `\s+` here stripped a run after a
    # non-breaking space, so `### Fixed\u00a0###`, which renders as the category
    # `Fixed\u00a0###`, was read as `Fixed` and passed the name check.
    text = re.sub(r"(?:^|[ \t]+)#+$", "", text).strip()
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
# `0|[1-9]\d*` per component, not `\d+`: SemVer forbids a leading zero in a
# numeric identifier, and `\d+` accepted `[0.08.0]`, which `int()` then normalised
# to 0.8.0 -- so every message quoted a version the heading does not carry, and
# the link rule demanded a definition for a tag `release.yml` can never cut (it
# takes the tag from the CMake `project VERSION`, which is written the SemVer way).
VERSION_HEADING_TEXT = re.compile(
    r"^\[(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)\] (?:—|-) "
    r"(\d{4}-\d{2}-\d{2})( \[YANKED\])?$"
)
UNRELEASED_HEADING_TEXT = "[Unreleased]"
# WHICH HEADINGS MUST SATISFY THAT GRAMMAR. The bracket alone used to decide it,
# which meant a heading that LOST one was not an entry attempt at all: it fell
# through to the preamble path, its `### ` sections were charged to the release
# ABOVE it, and if it was the file's first entry nothing here reported it -- the
# `release.yml` grep at tag time was the first thing to notice, by which point
# the tag exists.
#
# A level-2 heading is an ENTRY ATTEMPT, and so must be a valid entry heading, if
# any of these hold. Each catches one way the brackets can be lost, and none of
# them fires on an ordinary preamble section (`## How to read this file`):
#
#   * it starts with `[`                              -- the intended spelling
#   * its text starts with a semantic version, with or without a leading bracket
#     (`## 0.9.8] - 2026-10-01`, `## 0.9.8 - 2026-10-01`)
#   * its text is `Unreleased`, brackets optional, in any case
#   * it carries BOTH a semantic version and an ISO date anywhere in the text
#     (`## Version 0.9.8 - 2026-10-01`)
#
# The same predicate applies at level 1 and level 3, where an entry heading is
# not an entry at all: `release.yml` boundaries on `^## [`, so a release written
# at the wrong level does not terminate anything and is published inside its
# predecessor's notes.
RELEASE_LIKE_VERSION = re.compile(r"^\[?[ \t]*v?\d+\.\d+\.\d+(?![\w.])")
RELEASE_LIKE_UNRELEASED = re.compile(r"^\[?\s*unreleased\s*\]?$", re.I)
SEMVER_ANYWHERE = re.compile(r"(?<![\w.])v?\d+\.\d+\.\d+(?![\w.])")
ISO_DATE_ANYWHERE = re.compile(r"\b\d{4}-\d{2}-\d{2}\b")


def names_a_release(text: str) -> bool:
    """Does this heading NAME a release -- a version number or `Unreleased`?

    Level-independent, and deliberately narrower than `release_like`: it must not
    fire on an ordinary bracketed link in a heading. `# [Anamorph] — changelog`
    is this file's own title and `### [Verified]` is a preamble sub-heading; both
    start with `[` and neither is a release. Reporting them told the author to
    rewrite the document title as a release entry.
    """
    if RELEASE_LIKE_VERSION.match(text) or RELEASE_LIKE_UNRELEASED.match(text):
        return True
    if text.startswith("[") and SEMVER_ANYWHERE.search(text):
        return True                      # `## [0.9.8 — 2026-10-01`: lost its `]`
    return bool(SEMVER_ANYWHERE.search(text) and ISO_DATE_ANYWHERE.search(text))


def release_like(text: str) -> bool:
    """Is this LEVEL-2 heading text trying to be an entry heading?

    At level 2 the bracket is RESERVED (§The structural grammar, restrictions 1
    and 2): `release.yml` boundaries a release's notes on `^## [`, so a `## [`
    heading either is an entry or is a defect, and the reconstructed headings at
    the foot of this file (`[0.6.x] and earlier — …`, whose version is not a
    semantic version at all) are entries only because of that reservation.
    Nowhere else is the bracket reserved -- see `names_a_release`.
    """
    return text.startswith("[") or names_a_release(text)
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
#
# ONE GRAMMAR, APPLIED TWICE. There is no second heading regex here: the line is
# dedented and handed to `atx_heading`, the same function every other heading in
# this file goes through. A private pattern of its own is what let `### Fixed ###`
# past -- it captured the text RAW, closing `#` run included, so the category
# comparison saw `Fixed ###`, matched nothing, and the duplicate/invented/misorder
# rules never saw the heading at all. The closing run is decoration the renderer
# strips (CommonMark 4.2), and stripping it is `atx_heading`'s job.
def deep_heading(line: str) -> tuple[int, str] | None:
    """(level, text) for a heading indented four columns or more, else None."""
    if not line[:1] in (" ", "\t") or indent_columns(line) < 4:
        return None
    return atx_heading(line.lstrip(" \t"))
# A heading written on the same line as its container's marker: `> ### Fixed`,
# `- ### Fixed`, `1. ## [0.9.7] — …`. CommonMark renders every one of them as a
# heading -- the marker opens a block quote or a list item and the heading is that
# block's content -- but neither `atx_heading` (anchored at column 0-3) nor
# `deep_heading` (which needs four columns of leading whitespace) can see through
# the marker, so a category or an entry heading could hide behind two characters.
# `release.yml` cannot see through one either (`^## \[` does not match `> ## [`),
# so an entry heading written this way does not terminate the entry above it.
#
# Only the shapes that MATTER are reported, for the same reason the deep rule is
# narrow: a quoted `### Something` is ordinary quoted prose, and a checker that
# reported it would be wrong far more often than right.
# ONE CONTAINER MARKER, matched repeatedly. `>` takes at most ONE space or tab
# with it (CommonMark 5.1: "followed by an optional space of indentation"); a
# list marker takes at least one. Up to three columns of indentation may precede
# either. What is left after the markers is content, measured from ITS OWN
# column -- which is the whole point, and what the previous spelling got wrong:
# a single regex tried to match marker-and-heading together with `>[ \t]?`
# hard-coded, so `>  ## [0.9.7]` -- two spaces, a heading to every renderer --
# matched nothing and became ordinary prose to this checker while the extractor
# folded the release into the notes above it.
# `^[ \t]*`, not `^ {0,3}`: four columns of indent is an indented code block at
# TOP level and a container's own content INSIDE a list item, and telling those
# apart needs a container stack this file does not keep. `indented_code_mask`
# already draws that line the one way it can -- it masks a four-column line that
# follows a blank line outside list context, and deliberately does not mask one
# in list context -- so a line that reaches here at that depth is the list case,
# where the renderer does show a heading. Erring toward seeing it is the same
# choice `deep_heading` makes, and for the same reason: under-reporting is the
# bypass, over-reporting costs an author one edit.
# The marker HEAD only. Whether the run of spaces after it belongs to the marker
# or to the content is a column question, not a pattern one -- CommonMark 5.2
# gives the marker at most FOUR columns of following whitespace, and a fifth
# means the item's first block is INDENTED CODE whose content column is the
# marker plus one. Baked into a `[ \t]+` here, that greedy run swallowed the code
# indentation as marker padding, so `-     ```text` -- which every renderer shows
# as a code block -- opened a fence in `fence_mask`, and every line after it,
# a real `## [x.y.z]` entry heading included, was masked as its content.
LIST_MARKER_HEAD = re.compile(r"^([-*+]|\d{1,9}[.)])")
# A list marker ANYWHERE in the prefix, not only at its head: `> - ---` opens a
# list inside the quote and so cannot continue the paragraph above it, exactly as
# `- ---` cannot at top level. The renderer agrees on both.
LIST_CONTAINER_MARKER = re.compile(r"(?:^|[ \t])(?:[-*+][ \t]|\d{1,9}[.)][ \t])")


def quote_marker(rest: str) -> int | None:
    """Columns consumed by ONE blockquote marker at the head of `rest`, else None.

    Its optional indentation, the `>`, and AT MOST ONE following space (5.1 -- the
    space belongs to the marker). Factored out because two callers need exactly
    this and no other answer: `strip_containers`, which walks the whole prefix, and
    `strip_containers`, which walks the whole prefix, and `quote_rest`, which walks
    only as far as a fence's own quote depth. One grammar, two consumers -- the
    alternative is a second idea of what a `>` is.
    """
    # NO THREE-COLUMN BOUND HERE, and that is a decision rather than an omission.
    # CommonMark 5.1 allows a `>` at most three columns into its container, but on
    # a CONTINUATION line -- one carrying no marker of its own -- this function
    # cannot know which container that is: `10. > [0.9.7]` over `    > -------` has
    # its `>` at the item's own content column and IS a heading, while
    # `    > ```text` at top level is an indented code block whose `>` is literal.
    # Bounding absolutely fixes the second and breaks the first. Both are recorded
    # as the container-stack residual in CHANGELOG_POLICY.md; `fence_mask` carries
    # the chain it needs for its own decisions, and threading it through this
    # normalisation is a separate change.
    indent = len(rest) - len(rest.lstrip(" "))
    if rest[indent:indent + 1] != ">":
        return None
    take = indent + 1
    return take + (1 if rest[take:take + 1] == " " else 0)


def indent_of(text: str) -> int:
    """Leading SPACES of an already tab-expanded string -- its column, because after
    `expand_tabs` column and index are the same number."""
    return len(text) - len(text.lstrip(" "))


def quote_rest(line: str, quote_depth: int) -> str | None:
    """`line`, tab-expanded, after exactly `quote_depth` blockquote markers -- or None
    if it does not carry that many.

    WHAT A CLOSING FENCE IS ALLOWED TO HAVE IN FRONT OF IT. CommonMark 4.5 lets a
    closer be preceded by up to three columns of SPACES and by nothing else, so a
    list marker disqualifies it: inside a fenced example, `- ``` ` is code text.
    Reading the closer off `strip_containers`'s content -- which removes list
    markers as well as quote markers -- accepted that line as a closer and ended
    the block one line early, at which point the item's real closer opened a fresh
    fence that ran to end of file. Two findings on a valid document, and the same
    for `* `, `+ `, `1. `, `10. `, `- - `, a tab-marked item, and a tilde fence,
    top level included.
    """
    rest = expand_tabs(line)
    for _ in range(quote_depth):
        take = quote_marker(rest)
        if take is None:
            return None
        rest = rest[take:]
    return rest


def strip_containers(line: str) -> tuple[str, str, int, int, tuple[tuple[int, int], ...]]:
    """(prefix, content, quote_depth, list_columns, items) after removing container markers.

    THE NORMALISATION EVERY CONTAINER RULE GOES THROUGH. Strip the markers once,
    then hand what is left to the SAME functions that read a top-level line --
    `atx_heading`, `SETEXT_UNDERLINE`, `interrupts_paragraph`. A container does
    not change what a heading IS; it only changes the column its content starts
    at. Every container defect this file has carried came from a rule that tried
    to answer both questions in one pattern of its own.

    THE TWO CONTAINER KINDS ARE COUNTED DIFFERENTLY, because CommonMark treats
    them differently and a setext underline has to be matched with its subject:

      * `quote_depth` counts `>` markers. The space after a `>` belongs to the
        MARKER, not to the content (5.1), so `>[0.9.7]` over `>-------` is a
        heading and so is `> [0.9.7]` over `>-------` -- the renderer says both.
        Counting columns instead made the depth-1 pair look mismatched.
      * `list_columns` is the content column a LIST marker establishes. A
        continuation line carries no marker and is indented to that column
        instead, which is how `- [0.9.7]` over `  -------` is one heading and
        `- foo` over `- ---` is two list items.

    Two lines are in the same container context when their quote depths are equal
    (exactly -- depth 1 under depth 2 is not a heading, and the renderer agrees).

    EVERYTHING HERE IS MEASURED IN COLUMNS, on a tab-expanded copy of the line, so
    that column == index for the whole walk. Measuring a marker with `len()`
    instead made `-\tname` four columns wide to the renderer and two here, and the
    setext underline's 0-3 allowance was then counted from column 2: an underline
    at six or seven columns is a heading the renderer shows and this file did not.
    The returned `prefix` and `content` are slices of that expanded copy for the
    same reason -- a caller that measured them again would otherwise re-introduce
    the character count this function just removed.

    A LIST MARKER'S WIDTH IS NOT WHATEVER WHITESPACE FOLLOWS IT (CommonMark 5.2).
    One to four columns of following space put the content there; a fifth means
    the item begins with an INDENTED CODE BLOCK, and then the content column is
    the marker plus one and the rest of that whitespace belongs to the content.
    Consuming it as marker padding is what made `-     ```text` -- a code block to
    every renderer -- open a fence here.
    """
    expanded = expand_tabs(line)
    rest, pos, base, depth, columns = expanded, 0, 0, 0, 0
    items: list[tuple[int, int]] = []
    while True:
        take = quote_marker(rest)
        if take is not None:
            # 5.1: the marker is `>` plus AT MOST ONE space, and that space
            # belongs to the marker. Content is measured inside the quote, so the
            # list column restarts from here.
            rest, pos = rest[take:], pos + take
            base, depth, columns = pos, depth + 1, 0
            continue
        indent = len(rest) - len(rest.lstrip(" "))
        body = rest[indent:]
        m = LIST_MARKER_HEAD.match(body)
        if m is None:
            break
        after = m.end()
        tail = body[after:]
        spaces = len(tail) - len(tail.lstrip(" "))
        if spaces == 0:
            break                          # `-foo` and `1.5 foo` are not list items
        # Four columns of following space at most; a fifth, or nothing but
        # whitespace to the end of the line, leaves the content one column past
        # the marker and the remainder as the item's own indentation.
        width = 1 if (spaces >= 5 or not tail[spaces:]) else spaces
        take = indent + after + width
        columns = pos + take - base        # relative to the innermost quote
        # WHICH FRAME EACH ITEM SITS IN, recorded with its column. A fence opened
        # on this line is contained by EVERY item in this chain, and a later line
        # belongs to it only while its indentation -- read at each item's own
        # quote depth -- reaches that item's column. `- > ` puts an item at depth
        # 0 and `> - ` at depth 1, and the two measure their content from
        # different places; `- > - ` has one of each, and keeping only the
        # innermost lost the outer one. A line that leaves the OUTER item takes
        # the quote and the fence with it, and the renderer then shows the heading
        # on it -- masking that was an under-report.
        items.append((depth, columns))
        rest, pos = rest[take:], pos + take
    return expanded[:pos], rest, depth, columns, tuple(items)


def classify_heading(line: str, chain: tuple[tuple[int, int], ...] = ()) -> tuple[int, str, str] | None:
    """(level, text, PLACEMENT) for any line the renderer shows as an ATX heading.

    PLACEMENT is where the heading sits, and it is the whole point of this
    function: `column0`, `indented` (1-3 columns), `deep` (4+), or `container`
    (behind a `>` or a list marker on the same line). Everything downstream
    decides from the pair (what the heading SAYS, where it SITS) -- and there is
    exactly one place, this one, that answers either question.

    THREE INDEPENDENT PATHS ARE WHAT PUT THE DEFECTS HERE. `atx_heading`,
    `deep_heading` and a container pattern of its own each used to be consulted
    separately, each with its own idea of which headings mattered, and each gap
    between them was a bypass: a container-prefixed release heading was invisible
    unless an entry already existed, a deep release heading was tested for a
    leading `[` where the column-0 path tested `release_like`, and a category
    indented one to three columns -- forbidden by the policy -- reached the
    category list through the plain ATX path without anyone looking at its
    indent. Composing the three here, once, is what closes that class rather than
    its instances.
    """
    plain = atx_heading(line)
    if plain is not None:
        level, text = plain
        return level, text, ("column0" if indent_columns(line) == 0 else "indented")
    deep = deep_heading(line)
    if deep is not None:
        return deep[0], deep[1], "deep"
    prefix, content, depth, _, line_items = strip_containers(line)
    if prefix:
        # The remainder is read by the TOP-LEVEL rule, and only by it. A remainder
        # indented four columns or more is an indented code block inside the
        # container, exactly as it would be at top level -- which is why
        # `deep_heading` is deliberately not consulted here: `>` plus five spaces
        # leaves four columns of indent and is code, not a heading, and the
        # renderer agrees.
        #
        # ...MEASURED FROM THE ITEM'S CONTENT COLUMN, when this line sits in one it
        # does not restate. `> -   item` puts its item at column 2 INSIDE the
        # quote, so `>     ## [0.9.7]` is two columns into that item and a heading
        # to every renderer -- read from the quote's own content it looked like
        # four columns of indented code, and a release heading behind a container
        # marker went unreported. The chain says which column to count from; where
        # there is none this is the same test it has always been.
        inner = atx_heading(content)
        if inner is None and not line_items:
            column = chain_column(chain, depth)
            if column and indent_columns(content) >= column:
                inner = atx_heading(content[column:])
        if inner is not None:
            return inner[0], inner[1], "container"
    return None


def placement_phrase(line: str, placement: str) -> str:
    """How to describe where a heading sits, in a finding."""
    if placement == "container":
        marker = " ".join(strip_containers(line)[0].split())  # noqa: E501
        return f"sits behind `{marker}` on the same line"
    columns = indent_columns(line)
    return f"is indented {columns} column{'' if columns == 1 else 's'}"


# A setext underline (§4.3): `Changed` over `---` renders as a heading too, and
# `release.yml`'s extractor stops at neither it nor a setext version heading. Only
# a PARAGRAPH can carry one, which is what `NOT_A_SETEXT_SUBJECT` excludes: a list
# item, a table row, a quote, an ATX heading, an HTML block, a link reference
# definition. Testing the first character instead (`startswith("-")`) called a
# paragraph beginning `-not a list` a list item, and called the thematic break
# under the file's own link definitions a heading.
# THE RUN ONLY -- NO INDENT ALLOWANCE. CommonMark's 0-3 columns are counted from
# the CONTENT column of whatever contains the line, and this pattern cannot know
# that column: it is a property of the SUBJECT line, which the rule reads
# afterwards. Baking `^ {0,3}` in here meant the allowance was measured from the
# container's start instead, so a continuation line under any marker four columns
# wide -- `10. `, `99. `, `100. `, or `1. ` with a second space -- failed this
# pattern before the alignment test could look at it, and the release heading it
# underlined vanished into the notes above. The indent is now judged once, in
# `aligned`, against the column the subject's list marker established.
SETEXT_UNDERLINE = re.compile(r"^(=+|-+)[ \t\r]*$")


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
    # A FENCED BLOCK NESTED IN A LIST ITEM. CommonMark measures a fence's
    # three-column allowance from its CONTAINER's content column, not from column
    # 0, so a perfectly ordinary sample inside a list has its delimiters at four
    # columns or more -- where `fence_delimiter` (which measures from column 0,
    # because nothing here keeps a container stack) does not see them. Its
    # contents were then read as document structure: a sample entry heading and
    # its `### Added` were reported as hidden category headings, and a second
    # `### Added` in the sample as a duplicate. Three findings, no defect.
    #
    # Tracked here rather than in `fence_mask`, and used ONLY to silence the deep
    # rule: it is the deep rule that cannot tell a sample from a container, and a
    # deep delimiter run is the clearest signal there is that what follows is a
    # sample. Nothing else in this file changes its answer because of it.
    deep_fence: tuple[str, int, int] | None = None   # character, run, opener indent
    chains = container_chains(lines)                 # the items each line sits in
    for i, line in enumerate(lines):
        # A LONE CARRIAGE RETURN, reported rather than silently resolved.
        # CommonMark 2.1 calls it a line ending; `awk` does not, and
        # `changelog-section.awk` is `awk`. `check_file` splits the way the
        # extractor does, so the two tools agree about the file -- but the
        # RENDERER then shows a heading neither of them sees, which is the
        # bypass direction. Naming the character is the only answer that leaves
        # nothing hidden: one interpretation downstream, and a finding on the
        # input that would have needed two.
        if "\r" in line:
            findings.append(
                f"CHANGELOG.md:{i + 1}: a bare carriage return inside the line. GitHub "
                f"renders it as a line break and `release.yml`'s extractor does not, so a "
                f"heading after it would be published inside the wrong release -- use `\\n` "
                f"or `\\r\\n` line endings throughout"
            )
        if skip[i]:
            continue
        if indent_columns(line) >= 4:
            # `fence_run` and `opens_fence`, the same primitives `fence_mask`
            # uses: whatever else the two passes disagree about, they cannot
            # disagree about what a fence delimiter IS or which one opens.
            run = fence_run(line.lstrip(" \t"))
            if run:
                if deep_fence is None:
                    if opens_fence(run):
                        deep_fence = (run[0], run[1], indent_columns(line))
                elif run[0] == deep_fence[0] and run[1] >= deep_fence[1] \
                        and not run[2].strip(" \t"):
                    # NO INDENT GUARD ON THE CLOSER, deliberately: a delimiter
                    # shallower than the opener always opens a fence in
                    # `fence_mask` first -- its allowance is measured from the
                    # item's column, which is shallower still -- so every line
                    # after it is masked before this pass sees it. The guard was
                    # written for symmetry and no input distinguishes it, which is
                    # the definition of a clause to remove.
                    deep_fence = None
                continue
            # A LINE LESS INDENTED THAN THE OPENER IS NOT ITS CONTENT. The deep
            # pass silences the deep-heading rule between two deep delimiters, and
            # it used to silence every line between them whatever its indent -- so
            # `- item` over a six-column delimiter swallowed a `### Removed` at
            # FOUR columns, which the renderer shows as a heading because four
            # columns is only two inside the item. Content of a fence is indented
            # at least as far as the fence is; a shallower line has left it.
            if deep_fence is not None and indent_columns(line) >= deep_fence[2]:
                continue
        d = LINK_DEFINITION.match(line)
        if d:
            destination = d.group(2)
            if destination.startswith("<") and destination.endswith(">"):
                destination = destination[1:-1]     # CommonMark §4.7: `<...>` is a wrapper
            definitions.append((i + 1, " ".join(d.group(1).split()), destination))
            continue
        # SETEXT, THROUGH THE SAME NORMALISATION. A setext heading continues a
        # paragraph, so subject and underline must sit in the same container
        # context: the same content column, and the underline may not introduce a
        # LIST marker of its own (`- foo` over `- ---` is two list items and a
        # thematic break, not a heading). Everything else is decided on the
        # CONTENT of both lines, by the same helpers that read a top-level line --
        # which is what makes `> [0.9.7]` over `> -------` visible at last. The
        # old rule matched the raw line, so any container prefix hid the pair, and
        # a quoted release name over a quoted rule became ordinary prose here
        # while the renderer showed a level-2 heading and the extractor missed the
        # boundary.
        under_prefix, under_content, under_depth, _, _ = strip_containers(line)
        if i and not skip[i - 1] and SETEXT_UNDERLINE.match(under_content.lstrip(" \t")) \
                and not LIST_CONTAINER_MARKER.search(under_prefix):
            _, subj_content, subj_depth, subj_cols, subj_items = strip_containers(lines[i - 1])
            # A SUBJECT THAT RESTATES NO MARKER still sits in an item, and the
            # chain is where its column comes from. `- 1. ```text` over an indented
            # `[0.9.7]` and `-----` is two CONTINUATION lines: measured from
            # column 0 the pair fell outside the 0-3 allowance and the release
            # heading the renderer shows went unreported.
            if not subj_items:
                subj_cols = chain_column(chains[i - 1], subj_depth)
            # A continuation line is indented to the list's CONTENT COLUMN, and
            # CommonMark's 0-3 allowance is counted from there -- not from the
            # container's start. The column is the marker's full width including
            # the space after it, so `1. ` puts content at 3 and `100. ` at 5;
            # nothing here is special-cased per marker, `strip_containers` just
            # measures what the marker consumed. With no list above, `subj_cols`
            # is 0 and this is CommonMark's plain top-level rule again.
            aligned = (indent_columns(under_content) >= subj_cols
                       and indent_columns(under_content) <= subj_cols + 3)
            if subj_depth == under_depth and aligned and subj_content.strip() \
                    and not LIST_MARKER.match(subj_content) \
                    and not interrupts_paragraph(subj_content) \
                    and not LINK_DEFINITION.match(subj_content) \
                    and not subj_content.lstrip().startswith(("|", "<")):
                findings.append(
                    f"CHANGELOG.md:{i + 1}: `{lines[i - 1].strip()}` underlined by "
                    f"`{line.strip()}` is a setext heading. `release.yml` extracts and "
                    f"terminates release notes on "
                    f"`^## \\[` alone and cannot see it -- write headings as `##` / `###`"
                )
                continue
        h = classify_heading(line, chains[i])
        if h is None:
            continue
        level, text, placement = h
        # ---- RELEASE HEADINGS ------------------------------------------------
        # A release heading is a structural BOUNDARY: `release.yml` starts and
        # ends a release's notes at `^## [`, at column 0, and at nothing else. So
        # a heading that names a release and does not sit there is not a release
        # the pipeline can publish -- it is hidden release structure, whatever
        # hides it. Recorded as a malformed ENTRY, never skipped: skipping it was
        # how the FIRST such heading in a file escaped entirely (the old rule
        # required `entries` to be non-empty already) and how its `### ` sections
        # were charged to a release above it that may not exist.
        if level == 2 and release_like(text) and placement != "column0":
            findings.append(
                f"CHANGELOG.md:{i + 1}: `{line.strip()}` {placement_phrase(line, placement)}. "
                f"It renders as an entry heading, which `release.yml` cannot extract "
                f"(`^## \\[`) and which does not terminate the entry above it; write it at "
                f"column 0"
            )
            entries.append(ChangelogEntry(i + 1, text, "malformed", None, None))
            continue
        if level != 2 and names_a_release(text):
            # An entry heading at the wrong level. `release.yml` terminates a
            # release's notes at `^## [` and at nothing else, so this heading and
            # everything under it is published inside the entry above it -- and
            # at level 3 it would otherwise be counted as an invented CATEGORY,
            # which reports the wrong defect.
            findings.append(
                f"CHANGELOG.md:{i + 1}: `{line.strip()}` reads as an entry heading but is "
                f"level {level}, not level 2. `release.yml` starts and ends a release's notes "
                f"at `^## [`, so this neither begins a release nor terminates the one above "
                f"it -- write it `## [x.y.z] — YYYY-MM-DD`"
            )
            continue
        if level == 2 and release_like(text):
            if not text.startswith("["):
                # The brackets are the entry syntax AND the extractor's boundary,
                # so a release heading that lost one is not a preamble section: it
                # is a release nobody can publish. Recorded as a malformed ENTRY,
                # not skipped, so its categories are charged to it rather than to
                # the release above it.
                names = ("is an `Unreleased` heading" if RELEASE_LIKE_UNRELEASED.match(text)
                         else "names a version")
                wanted = ("`## [Unreleased]`" if RELEASE_LIKE_UNRELEASED.match(text)
                          else "`## [x.y.z] — YYYY-MM-DD`")
                findings.append(
                    f"CHANGELOG.md:{i + 1}: `{line.strip()}` {names} but is not bracketed, so "
                    f"`release.yml` cannot extract it (`^## \[`) and it does not terminate the "
                    f"entry above it -- write {wanted}"
                )
                entries.append(ChangelogEntry(i + 1, text, "malformed", None, None))
                continue
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
        # ---- CATEGORY HEADINGS ----------------------------------------------
        if level == 3 and entries:
            # At four columns or behind a marker, a `### Something` is as likely a
            # code sample as a heading -- no parser can tell without a container
            # stack -- so only a heading that carries a Keep a Changelog CATEGORY
            # name is treated as structure there. That is the shape a real bypass
            # takes; a sample called `### Fixed` is not one anybody writes.
            if placement in ("deep", "container") and text not in KAC_CATEGORIES:
                continue
            if placement != "column0":
                findings.append(
                    f"CHANGELOG.md:{i + 1}: `### {text}` "
                    f"{placement_phrase(line, placement)}. It still renders as a heading, so "
                    f"a category can hide there; a category heading belongs at column 0 "
                    f"(CHANGELOG_POLICY.md §The structural grammar, restriction 4)"
                )
            entries[-1].categories.append((text, i + 1))
    return entries, definitions, findings


def check_changelog_headings(path: Path, lines: list[str], skip: list[bool]) -> list[str]:
    """Entry headings: valid grammar, a real calendar date, `[Unreleased]` first
    and only once, versions strictly newest-first, the reconstructed history
    last -- each of those two headings once, and in their own newest-first order.

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
    # `[Unreleased]` AND the two reconstructed headings are each at most-once,
    # and the reconstructed pair is ORDERED. Both facts were promised by this
    # function's docstring and by `VERSION_HEADING_TEXT`'s comment and enforced by
    # neither: a second `## [Unreleased]` was reported only as "must be the first
    # entry", which is not the violated invariant and whose remedy (move it to the
    # top) makes the file worse; and the reconstructed pair could be reversed or
    # duplicated in silence.
    unreleased_seen = 0
    reconstructed_seen: list[int] = []
    for pos, e in enumerate(entries):
        if e.kind == "unreleased":
            unreleased_seen += 1
            if unreleased_seen > 1:
                findings.append(
                    f"{path}:{e.line_no}: a second `## [Unreleased]` entry -- there is one "
                    f"set of unreleased work, so there is one section for it; merge this "
                    f"one into the first"
                )
            elif pos != 0:
                findings.append(
                    f"{path}:{e.line_no}: `## [Unreleased]` must be the first entry -- it "
                    f"tracks what the NEXT release will contain, so nothing released sits "
                    f"above it"
                )
            continue
        if e.kind == "reconstructed":
            seen_reconstructed = True
            rank = RECONSTRUCTED_HEADINGS.index(e.text)
            if rank in reconstructed_seen:
                findings.append(
                    f"{path}:{e.line_no}: `## {e.text}` appears twice -- each reconstructed "
                    f"heading covers its own span of history and stands once"
                )
            elif reconstructed_seen and rank < max(reconstructed_seen):
                findings.append(
                    f"{path}:{e.line_no}: `## {e.text}` sits BELOW "
                    f"`## {RECONSTRUCTED_HEADINGS[max(reconstructed_seen)]}`, which covers "
                    f"older history -- the reconstructed headings run newest first like "
                    f"every other entry (rule 7)"
                )
            reconstructed_seen.append(rank)
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
    # Every version a MALFORMED entry names, however it is spelled. A malformed
    # heading's own definition is not an orphan -- the entry is there, its text is
    # wrong, and `check_changelog_headings` already says so. Reading only the
    # bracketed shape covered the one defect that predates `release_like` and none
    # of the shapes it added, so `## 0.9.8] — …` collected a second, false finding
    # ("`[0.9.8]` is defined but there is no `## [0.9.8]` entry") on top of the
    # true one.
    claimed = {
        m.group(0).lstrip("v")
        for e in entries if e.kind == "malformed"
        for m in [SEMVER_ANYWHERE.search(e.text)] if m
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
        # page; every later one compares against its PREDECESSOR, which in a
        # newest-first file is the entry directly BELOW it (`previous_of`, built
        # from the entry order). Accepting either for any version -- the first spelling of this
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
    # THE CHANGELOG RULES READ THE RAW LINES, not `text`. `blanked_lines` blanks
    # inline code spans for the prose checks, and a code span may run across a
    # line boundary -- so a backtick opened in one bullet blanked the `## `
    # heading two lines below it, and the one rule whose whole job is to see that
    # heading did not. `release.yml`'s extractor has no notion of a code span
    # either; it sees fences and nothing else, which is exactly what `skip`
    # already carries. Feeding these four rules the same view the extractor has
    # is what makes them speak about the same document.
    findings += check_changelog_notes_boundary(path, lines, skip)
    findings += check_changelog_headings(path, lines, skip)
    findings += check_changelog_categories(path, lines, skip)
    findings += check_changelog_links(path, lines, skip)
    return findings


def check_file(path: Path, root: Path) -> list[str]:
    try:
        # `newline=""` and a `\n` split, NOT universal newlines. CommonMark 2.1
        # counts a LONE carriage return as a line ending; `awk` counts only `\n`,
        # and `scripts/changelog-section.awk` is `awk`. Universal-newline reading
        # made this checker agree with CommonMark and disagree with the extractor:
        # a lone CR before an entry heading split the file into two entries here
        # and into one record there, so the checker called the file clean while
        # the published notes ran two releases together. The two tools now split
        # the same file the same way; the trailing `\r` of a CRLF line is dropped
        # so every other rule sees what universal newlines used to give it, and a
        # lone CR stays inside its line, exactly as `awk` leaves it.
        with path.open(encoding="utf-8", newline="") as handle:
            text = handle.read()
    except UnicodeDecodeError as exc:
        # Report as a finding, not a traceback: the CI job's whole contract is
        # `path:line: message`, and a stray legacy-encoded byte in a corpus this
        # full of typographic characters (— · ⊕ ≥) is a plausible accident.
        line = path.read_bytes()[: exc.start].count(b"\n") + 1
        return [
            f"{path}:{line}: not valid UTF-8 (byte {exc.object[exc.start]:#04x} "
            f"at offset {exc.start}: {exc.reason}) -- the file cannot be checked"
        ]
    lines = [ln[:-1] if ln.endswith("\r") else ln for ln in text.split("\n")]
    return analyse(path, lines, root)


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
    RECON1, RECON2 = ("## " + t for t in RECONSTRUCTED_HEADINGS)
    V7CONTENT = "## [0.9.7] — 2026-09-05"
    # The five shapes a fenced changelog EXAMPLE contains, and that the parser
    # must not react to: a category, a release heading, a setext release pair,
    # and a second category.
    SAMPLE = ["### Fixed", "## [0.9.7] — 2026-09-05", "[0.9.7]", "-------", "### Added"]
    UDEF = "[Unreleased]: https://github.com/skyRolly/Anamorph/compare/v0.5.0...HEAD"
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
        # A LEVEL-1 heading leaks identically -- the extractor terminates on
        # `^## [` and on nothing else -- and the rule used to look at level 2
        # only, so the more eye-catching spelling was the one it missed.
        ("a trailing `# ` section is the same defect at level 1", 1,
         ["# Changelog", V5, "### Added", "# Appendix", "- y"]),
        ("a `### ` sub-section is still not one", 0,
         ["# Changelog", V5, "### Added", "### Fixed", "- y"]),
        # These rules read the RAW lines: a code span opened in one bullet used to
        # blank the heading two lines below it, and the one rule whose job is to
        # see that heading did not.
        ("a code span opened above does not hide a stray heading", 1,
         ["# Changelog", V5, "### Added", "- a `span opens here", "## Appendix",
          "and closes here` done"]),
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
        # These four were written when 1-3 columns of indent were ACCEPTED, which
        # `CHANGELOG_POLICY.md` restriction 4 has never allowed ("at column 0").
        # The policy and the parser disagreed and the parser was the permissive
        # one, so the indent is now a finding IN ITS OWN RIGHT -- and the heading
        # is still counted, so the category rules go on seeing what they saw. The
        # counts below are the sum of the two, which is what makes them evidence
        # of both halves at once.
        ("categories indented 1, 2 and 3 columns are three findings, not zero", 3,
         ["# Changelog", V5, " ### Added", "- x", "  ### Changed", "- y", "   ### Fixed", "- z"]),
        ("a duplicate hidden by one space of indentation is still a duplicate", 2,
         ["# Changelog", V5, "### Fixed", "- x", " ### Fixed", "- y"]),
        ("an invented category hidden by two spaces is still invented", 2,
         ["# Changelog", V5, "### Fixed", "- x", "  ### Known issues", "- y"]),
        ("a misorder hidden by three spaces is still a misorder", 2,
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
        # `V8`/`D8` is the release after it, which compares against its
        # predecessor -- the entry directly BELOW it in this newest-first file,
        # which is `V7`. Accepting either form for either version -- the first
        # spelling of the rule -- let a comparison against a never-cut tag pass.
        ("the first tagged version with its tag-page definition passes", 0,
         ["# Changelog", V7, "### Fixed", "- x", D7]),
        ("a later version comparing against the entry below it passes", 0,
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

        # ===================================================================
        # THE GRAMMAR BOUNDARIES, one fixture per rule the parser and the
        # extractor have to agree on. Every expectation below was derived from
        # `markdown-it-py` in CommonMark mode -- an actual renderer, asked what
        # each line IS -- and then written down as a literal, because this
        # self-test must keep running on a bare `python3` with no dependency to
        # install. Where a fixture and CommonMark deliberately disagree, the
        # comment says so and why.
        # ===================================================================

        # -- ATX closing hashes are DECORATION (CommonMark 4.2) ---------------
        # `### Fixed ###` is the heading `Fixed`. Every rule here goes through
        # `atx_heading`, which strips the run -- and `deep_heading` now does too,
        # which is the defect these six pin: it had a regex of its own, captured
        # the text raw, compared `Fixed ###` against the category names, matched
        # nothing, and let a duplicate, an invented or a misordered category
        # through untouched.
        ("a closing `#` run is not part of the category name", 0,
         ["# Changelog", V5, "### Added ###", "- a"]),
        ("...nor when trailing spaces follow it", 0,
         ["# Changelog", V5, "### Added ###   ", "- a"]),
        ("...and a run with no space before it IS part of the name", 1,
         ["# Changelog", V5, "### Added###", "- a"]),
        ("...as is anything after the run, which is then not a closer", 1,
         ["# Changelog", V5, "### Added ### x", "- a"]),
        ("a duplicate category hidden by a closing run is still duplicate", 1,
         ["# Changelog", V5, "### Added", "- a", "### Added ###", "- b"]),
        ("a misordered category hidden by a closing run is still misordered", 1,
         ["# Changelog", V5, "### Fixed ###", "- a", "### Added ###", "- b"]),
        ("an invented category hidden by a closing run is still invented", 1,
         ["# Changelog", V5, "### Documentation ###", "- a"]),
        # The four-column case is the one the private regex was written for, so
        # it is the one that must still fire -- now through the shared grammar.
        ("a deep category with a closing run is reported, not skipped", 1,
         ["# Changelog", V5, "- b", "", "    ### Added ###", "", "- c"]),

        # -- release-heading grammar (CHANGELOG_POLICY.md rule 7) -------------
        # A heading that is TRYING to be an entry must BE one. Losing a bracket
        # used to make it preamble: its categories were charged to the release
        # above it, and if it was the file's first entry nothing here said a
        # word -- `release.yml`'s grep at tag time was the first to notice.
        ("an entry heading that lost its `[` is a finding", 1,
         ["# Changelog", "## 0.5.0] — 2026-01-02", "### Added", "- a"]),
        ("an entry heading with no brackets at all is a finding", 1,
         ["# Changelog", "## 0.5.0 — 2026-01-02", "### Added", "- a"]),
        ("an entry heading that lost its `]` is a finding", 1,
         ["# Changelog", "## [0.5.0 — 2026-01-02", "### Added", "- a"]),
        ("`## Unreleased` without brackets is a finding", 1,
         ["# Changelog", "## Unreleased", "### Added", "- a"]),
        ("a heading carrying a version AND a date is an entry attempt", 1,
         ["# Changelog", "## Version 0.5.0 on 2026-01-02", "Text.", "", V5,
          "### Added", "- a"]),
        ("an entry heading at level 1 is a finding", 1,
         ["# Changelog", "# [0.5.0] — 2026-01-02", "### Added", "- a"]),
        ("an entry heading at level 3 is a finding, and not a category one", 1,
         ["# Changelog", V5, "### [0.4.0] — 2026-01-01", "- a"]),
        # The malformed entry is still an ENTRY: its categories belong to it, or
        # they silently become the previous release's and the counts go wrong in
        # both entries at once.
        ("a malformed entry's categories are charged to IT", 2,
         ["# Changelog", "## 0.5.0] — 2026-01-02", "### Fixed", "- a",
          "### Added", "- b"]),
        # ...and the other half: an ordinary preamble section must stay ordinary.
        # A rule that catches the cases above by banning `## ` headings would
        # pass every fixture here and make the file unwritable.
        ("an ordinary preamble heading is not an entry attempt", 0,
         ["# Changelog", "## How to read this file", "Text.", "", V5,
          "### Added", "- a"]),
        ("...nor is one that merely has a number in it", 0,
         ["# Changelog", "## The 6 categories", "Text.", "", V5,
          "### Added", "- a"]),

        # -- fence grammar (CommonMark 4.5), shared with the extractor --------
        # A backtick fence's info string MAY NOT contain a backtick: ```a`b is a
        # paragraph, so what follows is document structure, not code. Reading it
        # as a fence hid every line to the next delimiter -- here a duplicate
        # category, in CHANGELOG.md a whole release heading.
        # Two duplicates, not one: with the rule dropped, the would-be fence
        # masks both AND reports itself unclosed, so a single duplicate leaves
        # the COUNT unchanged and the case proves nothing. This is the shape that
        # actually separates the two implementations.
        ("a backtick in a backtick fence's info string: not a fence", 2,
         ["# Changelog", V5, "### Added", "```a`b", "### Added", "- b",
          "### Added", "- c"]),
        # A TILDE fence has no such restriction, and the pair is what makes the
        # rule above discriminating rather than merely present.
        ("...but a tilde fence may carry one, and hides the sample", 0,
         ["# Changelog", V5, "### Added", "~~~a`b", "### Added", "~~~"]),
        # Three columns of indent at most, counted in COLUMNS: one tab is four,
        # so a tab-indented delimiter is an indented code block and opens
        # nothing. `changelog-section.awk` has counted columns since the round
        # before this one; `FENCE` still counted characters, so the two tools
        # gave opposite answers about the same line.
        ("a tab-indented delimiter opens no fence", 1,
         ["# Changelog", V5, "### Added", "\t```", "### Added", "\t```"]),
        ("a three-space-indented delimiter still opens one", 0,
         ["# Changelog", V5, "### Added", "   ```", "### Added", "   ```"]),
        ("a closer carrying an info string does not close", 0,
         ["# Changelog", V5, "### Added", "```", "### Added", "```x", "- y", "```"]),
        ("a closer shorter than its opener does not close", 0,
         ["# Changelog", V5, "### Added", "````", "### Added", "```", "- y", "````"]),
        # ===================================================================
        # ROUND 4: one classifier, and the four gaps between the three paths it
        # replaced. Each fixture names WHERE the heading sits as well as what it
        # says -- the pair is what the parser now decides from.
        # ===================================================================

        # -- hidden release headings (forbidden structure, Option A) ----------
        # `release.yml` boundaries on `^## [` at column 0. A release heading
        # anywhere else is not a release the pipeline can publish, so it is
        # reported AND recorded as a malformed entry -- the FIRST one included,
        # which the old rule skipped because no entry existed yet to hang it on.
        ("a first release heading behind a `>` is a finding", 1,
         ["# Changelog", "> " + V5, "### Added", "- a"]),
        ("a first release heading behind a list marker is a finding", 1,
         ["# Changelog", "- " + V5, "### Added", "- a"]),
        ("a first release heading behind an ordered marker is a finding", 1,
         ["# Changelog", "1. " + V5, "### Added", "- a"]),
        ("a release heading indented two columns is a finding", 1,
         ["# Changelog", "  " + V5, "### Added", "- a"]),
        ("a DEEP UNBRACKETED version heading is a finding", 1,
         ["# Changelog", V5, "- b", "", "    ## 0.4.0 — 2026-01-01", "", "- c"]),
        # THE MESSAGE, not the count. Drop the placement test and the heading
        # falls through to the column-0 entry path, which reports its own defect
        # ("not written `## [` at column 0 with a single space") -- one finding
        # either way, naming the wrong thing. Only the text separates a rule that
        # sees hidden release structure from one that does not.
        ("...and the container is named in the finding", 0,
         ["# Changelog", "> " + V5, "### Added", "- a",
          "@@says:sits behind `>` on the same line@@"]),
        ("...as is the indent, for a deep unbracketed version", 0,
         ["# Changelog", V5, "- b", "", "    ## 0.4.0 — 2026-01-01", "", "- c",
          "@@says:is indented 4 columns@@"]),
        ("...and for a release indented two columns", 0,
         ["# Changelog", "  " + V5, "### Added", "- a",
          "@@says:is indented 2 columns@@"]),
        # ...and it is an ENTRY, so what follows is charged to it and not to the
        # release above it: `### Added` after `### Fixed` here is the malformed
        # entry's own misorder, which is only visible if the entry exists.
        ("a hidden release heading takes its own categories", 2,
         ["# Changelog", "> " + V5, "### Fixed", "- a", "### Added", "- b"]),

        # ===================================================================
        # ROUND 5: containers, normalised once. The two bugs here were two
        # heading FORMS -- ATX and setext -- behind the same container prefix,
        # and each had a pattern of its own that tried to match marker and
        # heading together. Both now strip the prefix and hand the remainder to
        # the top-level rule, so the boundaries below are CommonMark's, verified
        # against `markdown-it-py`, not this file's guesses.
        # ===================================================================

        # A `>` takes at most ONE space with it (CommonMark 5.1), so the content
        # keeps the rest: 0-4 spaces still leave 0-3 columns and a heading; five
        # leave four columns and an indented code block. The old pattern
        # hard-coded `>[ \t]?` and saw only the first two.
        ("a release heading behind `>` with no space is a finding", 1,
         ["# Changelog", V5, "### Added", "- a", ">" + V7CONTENT, "- b"]),
        ("...with two spaces (the reported bypass) is a finding", 1,
         ["# Changelog", V5, "### Added", "- a", ">  " + V7CONTENT, "- b"]),
        ("...with four spaces is a finding", 1,
         ["# Changelog", V5, "### Added", "- a", ">    " + V7CONTENT, "- b"]),
        ("...with five spaces is indented code, and is not", 0,
         ["# Changelog", V5, "### Added", "- a", ">     " + V7CONTENT, "- b"]),
        ("...and the diagnostic names the container, not the spelling", 0,
         ["# Changelog", V5, "### Added", "- a", ">  " + V7CONTENT, "- b",
          "@@says:sits behind `>` on the same line@@"]),
        ("a category behind `>` with two spaces is a finding", 2,
         ["# Changelog", V5, "### Added", "- a", ">  ### Added", "- b"]),
        ("nested `>>` hides a release heading no better", 1,
         ["# Changelog", V5, "### Added", "- a", ">> " + V7CONTENT, "- b"]),
        ("nor does `> -`", 1,
         ["# Changelog", V5, "### Added", "- a", "> - " + V7CONTENT, "- b"]),
        ("nor does `- >`", 1,
         ["# Changelog", V5, "### Added", "- a", "- > " + V7CONTENT, "- b"]),
        ("nor a tab after `>`", 1,
         ["# Changelog", V5, "### Added", "- a", ">\t" + V7CONTENT, "- b"]),

        # SETEXT, the second form. A quoted release name over a quoted rule is a
        # level-2 heading to every renderer; the old rule matched the raw line, so
        # any container prefix hid the pair completely.
        ("a quoted setext release heading is a finding", 1,
         ["# Changelog", V5, "### Added", "- a", "> [0.9.7]", "> -------", "- b"]),
        ("...with no space after the `>` on the underline", 1,
         ["# Changelog", V5, "### Added", "- a", "> [0.9.7]", ">-------", "- b"]),
        ("...and the diagnostic names it a setext heading", 0,
         ["# Changelog", V5, "### Added", "- a", "> [0.9.7]", "> -------", "- b",
          "@@says:is a setext heading@@"]),
        ("a quoted setext CATEGORY is a finding too", 1,
         ["# Changelog", V5, "### Added", "- a", "> Added", "> -----", "- b"]),
        ("a nested `>>` setext pair is a finding", 1,
         ["# Changelog", V5, "### Added", "- a", ">> [0.9.7]", ">> -------", "- b"]),
        # ...and the shapes that are NOT headings, which is what stops the rule
        # from firing on ordinary quoted prose and ordinary lists.
        ("quote depths that differ are not a setext pair", 0,
         ["# Changelog", V5, "### Added", "- a", "> [0.9.7]", ">> -------", "- b"]),
        ("a blank quoted line breaks the pair", 0,
         ["# Changelog", V5, "### Added", "- a", "> [0.9.7]", ">", "> -------", "- b"]),
        ("`- foo` over `- ---` is two list items, not a heading", 0,
         ["# Changelog", V5, "### Added", "- foo", "- ---", "- b"]),
        # ...and the same inside a quote, where the alignment test cannot see it:
        # `> - ---` opens a list in the quote rather than continuing the
        # paragraph, so the marker has to be looked for anywhere in the prefix.
        ("`> foo` over `> - ---` is a list in a quote, not a heading", 0,
         ["# Changelog", V5, "### Added", "- a", "> foo", "> - ---", "- b"]),
        ("a quoted `---` on its own is a thematic break", 0,
         ["# Changelog", V5, "### Added", "- a", "", "> ---", "", "- b"]),
        ("ordinary quoted prose is untouched", 0,
         ["# Changelog", V5, "### Added", "- a", "> a note", "- b"]),
        # A list continuation line IS the underline's home when the subject is a
        # list item: the column the marker established, not column 0.
        ("a setext pair inside a list item is a finding", 1,
         ["# Changelog", V5, "### Added", "- a", "", "- [0.9.7]", "  -------", "", "- b"]),
        ("...but not when the underline misses that column", 0,
         ["# Changelog", V5, "### Added", "- a", "", "- [0.9.7]", "-------", "", "- b"]),

        # ===================================================================
        # ROUND 6: the list marker's WIDTH. A continuation line is indented to
        # the list's content column, and CommonMark's 0-3 allowance is counted
        # from THERE. The allowance used to live in `SETEXT_UNDERLINE`, which is
        # matched before the subject line is read and so measured it from the
        # container's start -- so every marker four columns wide or more (`10. `,
        # `99. `, `100. `, `10) `, or `1. ` with a second space) rejected its own
        # continuation and hid the release heading it underlined. Nothing here is
        # special-cased per marker: `strip_containers` measures what the marker
        # actually consumed.
        # ===================================================================
        ("a setext release under `1.` is a finding", 1,
         ["# Changelog", V5, "### Added", "- a", "", "1. [0.9.7]", "   -------"]),
        ("...under `10.` (the reported bypass) is a finding", 1,
         ["# Changelog", V5, "### Added", "- a", "", "10. [0.9.7]", "    -------"]),
        ("...under `100.` is a finding", 1,
         ["# Changelog", V5, "### Added", "- a", "", "100. [0.9.7]", "     -------"]),
        ("...under `1000.` is a finding", 1,
         ["# Changelog", V5, "### Added", "- a", "", "1000. [0.9.7]", "      -------"]),
        ("...under `10)` is a finding", 1,
         ["# Changelog", V5, "### Added", "- a", "", "10) [0.9.7]", "    -------"]),
        ("...under `1.` with a second space is a finding", 1,
         ["# Changelog", V5, "### Added", "- a", "", "1.  [0.9.7]", "    -------"]),
        ("...in a NESTED ordered list is a finding", 1,
         ["# Changelog", V5, "### Added", "- a", "", "1. x", "", "   1. [0.9.7]",
          "      -------"]),
        ("...inside a blockquote is a finding", 1,
         ["# Changelog", V5, "### Added", "- a", "", "> 10. [0.9.7]", ">     -------"]),
        ("...with a blockquote inside the item is a finding", 1,
         ["# Changelog", V5, "### Added", "- a", "", "10. > [0.9.7]", "    > -------"]),
        ("...after a `-` item, in a mixed list, is a finding", 1,
         ["# Changelog", V5, "### Added", "- a", "", "- x", "", "  10. [0.9.7]",
          "      -------"]),
        ("a setext CATEGORY under `10.` is a finding", 1,
         ["# Changelog", V5, "### Added", "- a", "", "10. Added", "    -----"]),
        # ...and the alignment really is measured, in both directions: short of
        # the content column is not a heading, and the 0-3 allowance beyond it is.
        ("an underline short of the content column is not a heading", 0,
         ["# Changelog", V5, "### Added", "- a", "", "10. [0.9.7]", "   -------"]),
        ("...nor one at column 0", 0,
         ["# Changelog", V5, "### Added", "- a", "", "10. [0.9.7]", "-------"]),
        ("...but three columns past it still is", 1,
         ["# Changelog", V5, "### Added", "- a", "", "10. [0.9.7]", "       -------"]),
        ("...and four past it is not", 0,
         ["# Changelog", V5, "### Added", "- a", "", "10. [0.9.7]", "        -------"]),
        ("`10. foo` over `10. ---` is two list items, not a heading", 0,
         ["# Changelog", V5, "### Added", "- a", "", "10. foo", "10. ---"]),
        # The ATX half of the same family was already right, and stays right.
        ("an ATX release heading under `100.` is a finding", 1,
         ["# Changelog", V5, "### Added", "- a", "", "100. " + V7CONTENT]),
        ("...and the diagnostic names the container", 0,
         ["# Changelog", V5, "### Added", "- a", "", "100. " + V7CONTENT,
          "@@says:sits behind `100.` on the same line@@"]),
        ("...and the setext diagnostic names a setext heading", 0,
         ["# Changelog", V5, "### Added", "- a", "", "10. [0.9.7]", "    -------",
          "@@says:is a setext heading@@"]),

        # ===================================================================
        # ROUND 7: a fence inside a container. The opposite failure mode to every
        # bypass before it -- valid EXAMPLE content read as live structure, so a
        # correct document was rejected. `fence_mask` read the RAW line, so a
        # `> ```text` opened nothing and the sample inside it reached
        # `classify_heading`. It now strips the container prefix first, through
        # the same `strip_containers` every heading rule uses, and a fence opened
        # inside a quote is closed only inside that quote.
        #
        # Each fenced sample below carries all five shapes the parser reacts to:
        # a category, a release heading, a setext release pair, and a second
        # category. None of them may be seen.
        # ===================================================================
        ("a fenced sample in a blockquote is data", 0,
         ["# Changelog", V5, "### Added", "- a", "", "> ```text"]
         + ["> " + c for c in SAMPLE] + ["> ```"]),
        ("...with no space after the `>`", 0,
         ["# Changelog", V5, "### Added", "- a", "", ">```text"]
         + [">" + c for c in SAMPLE] + [">```"]),
        ("...with four spaces after the `>`", 0,
         ["# Changelog", V5, "### Added", "- a", "", ">    ```text"]
         + [">    " + c for c in SAMPLE] + [">    ```"]),
        ("...nested in `>>`", 0,
         ["# Changelog", V5, "### Added", "- a", "", ">> ```text"]
         + [">> " + c for c in SAMPLE] + [">> ```"]),
        ("...nested in `> >`", 0,
         ["# Changelog", V5, "### Added", "- a", "", "> > ```text"]
         + ["> > " + c for c in SAMPLE] + ["> > ```"]),
        ("...as a tilde fence in a blockquote", 0,
         ["# Changelog", V5, "### Added", "- a", "", "> ~~~text"]
         + ["> " + c for c in SAMPLE] + ["> ~~~"]),
        ("a fenced sample in a `-` list item is data", 0,
         ["# Changelog", V5, "### Added", "- The template is:", "", "  ```text"]
         + ["  " + c for c in SAMPLE] + ["  ```"]),
        ("...in a `*` item", 0,
         ["# Changelog", V5, "### Added", "* The template is:", "", "  ```text"]
         + ["  " + c for c in SAMPLE] + ["  ```"]),
        ("...in a `1.` item", 0,
         ["# Changelog", V5, "### Added", "1. The template is:", "", "   ```text"]
         + ["   " + c for c in SAMPLE] + ["   ```"]),
        ("...in a `10.` item", 0,
         ["# Changelog", V5, "### Added", "10. The template is:", "", "    ```text"]
         + ["    " + c for c in SAMPLE] + ["    ```"]),
        ("...in a `100.` item", 0,
         ["# Changelog", V5, "### Added", "100. The template is:", "", "     ```text"]
         + ["     " + c for c in SAMPLE] + ["     ```"]),
        ("...in a nested list item", 0,
         ["# Changelog", V5, "### Added", "- x", "", "  - y", "", "    ```text"]
         + ["    " + c for c in SAMPLE] + ["    ```"]),
        ("...in a list inside a blockquote", 0,
         ["# Changelog", V5, "### Added", "- a", "", "> - x", "", ">   ```text"]
         + [">   " + c for c in SAMPLE] + [">   ```"]),
        ("...and a blank line inside it does not end a list fence", 0,
         ["# Changelog", V5, "### Added", "- The template is:", "", "  ```text",
          "  ### Fixed", "", "  ### Added", "  ```"]),

        # Three, and the third is the trailing `    ``` `: it sits at the `10. `
        # item's own content column, so once the opener's allowance is counted from
        # there it OPENS a fence, and that fence is never closed. The renderer says
        # the same -- a heading on the `### Added` line and a fence on the one
        # after it.
        ("...and a bad info string opens no DEEP fence either", 3,
         ["# Changelog", V5, "### Added", "10. x", "", "    ```a`b", "    ### Added",
          "    ```"]),

        # -- NEGATIVE: a line that only LOOKS like a fence masks nothing --------
        # Over-masking is the same defect wearing the other face: it would hide
        # real structure, which is the direction every earlier round closed.
        ("a bad backtick info string opens no fence in a quote", 2,
         ["# Changelog", V5, "### Added", "- a", "", "> ```a`b", "> ### Added", "> ```",
          "> ```"]),
        ("two backticks open no fence in a quote", 2,
         ["# Changelog", V5, "### Added", "- a", "", "> ``x", "> ### Added", "> ``"]),
        ("five spaces after a `>` is indented code, not a fence", 0,
         ["# Changelog", V5, "### Added", "- a", "", ">     ```text", ">     ### Added",
          ">     ```"]),
        ("a blank line ends the quote, so the fence ends with it", 3,
         ["# Changelog", V5, "### Added", "- a", "", "> ```text", "", "> ### Added",
          "> ```"]),
        ("a line that leaves the quote ends the fence", 2,
         ["# Changelog", V5, "### Added", "- a", "", "> ```text", "### Added", "> ```"]),
        ("after the fence closes, a quoted category is seen again", 2,
         ["# Changelog", V5, "### Added", "- a", "", "> ```text", "> x", "> ```", "",
          "> ### Added"]),
        ("a closer three columns in still closes", 2,
         ["# Changelog", V5, "### Added", "- a", "", "> ```text", "> x", ">    ```", "",
          "> ### Added"]),
        ("a top-level fence is not closed by a quoted delimiter", 0,
         ["# Changelog", V5, "### Added", "- a", "", "```text", "### Added", "> ```",
          "```"]),
        # Three findings, and the third is the point of the round-9 restructure: the
        # renderer shows THREE fences here, one per list item, and the last is
        # never closed. A line that ends a fence by leaving its item is outside
        # that fence, so it is classified from scratch -- and when it is itself a
        # delimiter it OPENS one. Ending the fence and then skipping the line left
        # the last opener unseen and its unclosed block unreported.
        ("a new list item ends a fence opened on a list line", 3,
         ["# Changelog", V5, "### Added", "- a", "", "- > ```text", "- > ### Added",
          "- > ```", "- > ```"]),
        ("...but a bullet INSIDE a quoted fence is still data", 0,
         ["# Changelog", V5, "### Added", "- a", "", "> ```text", "> - a bullet",
          "> ### Added", "> ```"]),
        ("an inline code span is not a fence", 2,
         ["# Changelog", V5, "### Added", "- a", "", "> a `### Added` b", "> ### Added"]),

        # -- categories live at column 0 (restriction 4) ----------------------
        ("a category indented one column is a finding", 1,
         ["# Changelog", V5, " ### Added", "- a"]),
        ("a category indented three columns is a finding", 1,
         ["# Changelog", V5, "   ### Added", "- a"]),
        ("a category at column 0 is not", 0,
         ["# Changelog", V5, "### Added", "- a"]),

        # -- the reconstructed footer is ordered, and each heading stands once -
        ("the reconstructed headings in their own order pass", 0,
         ["# Changelog", V5, "### Added", "- a", RECON1, "- b", RECON2, "- c"]),
        ("...reversed is a finding", 1,
         ["# Changelog", V5, "### Added", "- a", RECON2, "- b", RECON1, "- c"]),
        ("...the first one twice is a finding", 1,
         ["# Changelog", V5, "### Added", "- a", RECON1, "- b", RECON1, "- c"]),
        ("...the second one twice is a finding", 1,
         ["# Changelog", V5, "### Added", "- a", RECON2, "- b", RECON2, "- c"]),
        ("...only one of them present is fine", 0,
         ["# Changelog", V5, "### Added", "- a", RECON2, "- b"]),
        ("...neither present is fine", 0,
         ["# Changelog", V5, "### Added", "- a"]),

        # -- `[Unreleased]`: at most one, and first ---------------------------
        # Two distinct invariants, and the diagnostic must name the one that was
        # broken: a second section reported as "must be the first entry" tells
        # the author to move it to the top, which makes the file worse.
        ("a second `## [Unreleased]` is reported as a duplicate", 1,
         ["# Changelog", "## [Unreleased]", "### Added", "- a", "## [Unreleased]",
          "### Fixed", "- b", V5, "### Added", "- c", UDEF]),
        # ...and the MESSAGE is the point: the old rule reported the same COUNT
        # while naming the wrong invariant, so only the text can tell them apart.
        ("...and the message names duplication, not placement", 0,
         ["# Changelog", "## [Unreleased]", "### Added", "- a", "## [Unreleased]",
          "### Fixed", "- b", V5, "### Added", "- c", UDEF,
          "@@says:a second `## [Unreleased]` entry@@"]),
        ("one `## [Unreleased]` below a release is a placement finding", 1,
         ["# Changelog", V5, "### Added", "- a", "## [Unreleased]", "### Fixed", "- b",
          UDEF]),
        ("one `## [Unreleased]` first is fine", 0,
         ["# Changelog", "## [Unreleased]", "### Added", "- a", V5, "### Fixed", "- b",
          UDEF]),
        ("no `## [Unreleased]` at all is fine", 0,
         ["# Changelog", V5, "### Added", "- a"]),

        ("a fenced entry heading is data, not a boundary", 0,
         ["# Changelog", V5, "### Added", "```", "## [9.9.9] — 2026-01-03", "```"]),
        # A closer may be followed by SPACES AND TABS and by nothing else
        # (CommonMark 4.5). The two directions were both unpinned: a rule that
        # refused to close on a trailing space would break ordinary files, and one
        # that closed on a non-breaking space -- Python's bare `.strip()` -- closed
        # the fence HERE and not in the extractor, which is the divergence four
        # separate audits of this file found first.
        # TWO duplicates after the closer, not one: whether the fence closes or
        # not, the alternative produces exactly one finding (a duplicate, or an
        # unclosed fence), so a single one cannot tell the two apart.
        ("a closer followed by spaces still closes", 2,
         ["# Changelog", V5, "### Added", "```", "### Added", "```   ",
          "### Added", "### Added"]),
        ("a closer followed by a tab still closes", 2,
         ["# Changelog", V5, "### Added", "```", "### Added", "```\t",
          "### Added", "### Added"]),
        ("a closer followed by a NON-BREAKING space does not close", 1,
         ["# Changelog", V5, "### Added", "```", "### Added", "```\u00a0",
          "### Added", "### Added"]),
        # ...and the same Unicode trap one line up, in the ATX closing run: the
        # run must be preceded by a space or a tab, so this heading's name really
        # is `Fixed\u00a0###` and really is not a category.
        ("a closing run after a non-breaking space is part of the name", 1,
         ["# Changelog", V5, "### Fixed\u00a0###", "- a"]),

        # -- headings hidden behind a container marker on the same line -------
        ("a category behind a `>` is reported and still counted", 2,
         ["# Changelog", V5, "### Added", "- a", "> ### Added", "- b"]),
        ("a category behind a list marker is the same defect", 2,
         ["# Changelog", V5, "### Added", "- a", "- ### Added", "- b"]),
        ("an entry heading behind a `>` cannot be extracted", 1,
         ["# Changelog", V5, "### Added", "- a", "> ## [0.4.0] — 2026-01-01", "- b"]),
        ("ordinary quoted prose with a heading in it is not a category", 0,
         ["# Changelog", V5, "### Added", "- a", "> ### Some note", "- b"]),

        # -- a fenced sample nested in a list item ----------------------------
        # CommonMark measures the fence's three-column allowance from the
        # CONTAINER's content column. Nothing here keeps a container stack, so the
        # deep rule is silenced between deep delimiters instead: a sample is not a
        # defect, and three findings on one were what the alternative produced.
        ("a fenced sample inside a list item is data", 0,
         ["# Changelog", V5, "### Added", "- The template is:",
          "    ```markdown", "    ## [1.2.3] — 2026-01-01", "    ### Added", "    - x",
          "    ```", "- b"]),
        ("...and a deep category OUTSIDE one is still reported", 1,
         ["# Changelog", V5, "- b", "", "    ### Added", "", "- c"]),

        # -- release-likeness is level-dependent ------------------------------
        # At level 2 the bracket is reserved; nowhere else is. Reporting an
        # ordinary bracketed link in a heading told the author to rewrite this
        # file's own title as a release entry.
        ("the document title may carry a bracketed link", 0,
         ["# [Anamorph] — changelog", "", V5, "### Added", "- a"]),
        ("a preamble `### [Verified]` sub-heading is not a release", 0,
         ["# Changelog", "", "### [Verified]", "Text.", "", V5, "### Added", "- a"]),
        ("a `v`-prefixed version is still a release heading", 1,
         ["# Changelog", "## v0.5.0 — 2026-01-02", "### Added", "- a"]),
        # ...and one where the version is not at the start, so only the
        # anywhere-in-the-text pattern can see it.
        ("a `v`-prefixed version mid-heading is still a release heading", 1,
         ["# Changelog", "## Release v0.5.0 on 2026-01-02", "### Added", "- a"]),
        # ...and one with a `v` version and NO date, which only the
        # start-of-heading pattern can see.
        ("a `v`-prefixed version with no date is still a release heading", 1,
         ["# Changelog", "## v0.5.0", "### Added", "- a"]),
        # The entry grammar is anchored at BOTH ends: trailing text after the date
        # is not a release heading, however well the front of it reads.
        ("trailing text after the date is not a valid entry heading", 1,
         ["# Changelog", "## [0.5.0] — 2026-01-02 (final)", "### Added", "- a"]),
        ("a misspelled YANKED marker is not a valid entry heading", 1,
         ["# Changelog", "## [0.5.0] — 2026-01-02 [Yanked]", "### Added", "- a"]),
        # SemVer forbids a leading zero, and `\d+` accepted one and then quoted a
        # version the heading does not carry back at the author.
        ("a leading-zero version is not a valid entry heading", 1,
         ["# Changelog", "## [0.08.0] — 2026-01-02", "### Added", "- a"]),
        # A malformed entry's own definition is not an orphan: one defect, one
        # finding. Reading only the bracketed shape gave this input two.
        ("a malformed entry does not orphan its own definition", 1,
         ["# Changelog", "## 0.9.7] — 2026-09-05", "### Added", "- a", D7]),

        # -- ROUND 8: THE COLUMN MODEL, and the two ways it was wrong ---------
        # A LIST MARKER'S PADDING IS NOT THE ITEM'S CONTENT (CommonMark 5.2).
        # Five columns after the marker means the item begins with an INDENTED
        # CODE BLOCK; consuming them as marker padding made a code line that only
        # LOOKS like a fence open one, and everything below it -- the next real
        # entry heading included -- was masked as that fence's content. The two
        # entries here are deliberately out of order, so the file is only clean
        # if the second one VANISHES: 1 finding proves it was read as an entry.
        ("list code five columns in does not open a fence", 1,
         ["# Changelog", V4, "### Added", "- x", "-     ```text",
          V5, "### Added", "- y"]),
        ("ordered list code does not open a fence", 1,
         ["# Changelog", V4, "### Added", "- x", "1.     ```text",
          V5, "### Added", "- y"]),
        ("multi-digit list code does not open a fence", 1,
         ["# Changelog", V4, "### Added", "- x", "10.     ```text",
          V5, "### Added", "- y"]),
        ("nested list code does not open a fence", 1,
         ["# Changelog", V4, "### Added", "- x", "- -     ```text",
          V5, "### Added", "- y"]),
        # Three spaces and a TAB reach column 8: five columns of padding measured
        # the way the renderer measures it, and a character count cannot see it.
        ("tab-padded list code does not open a fence", 1,
         ["# Changelog", V4, "### Added", "- x", "-   \t```text",
          V5, "### Added", "- y"]),
        ("list code carrying a release heading is data", 1,
         ["# Changelog", V4, "### Added", "- x", "-     ### Fixed",
          V5, "### Added", "- y"]),
        # ...AND THE OTHER DIRECTION, which is the same column: a GENUINE fence in
        # a list item must still mask its sample, and must still CLOSE. Its
        # delimiters sit at the item's content column, so the three-column
        # allowance is counted from there; counted from column 0 a four-column
        # closer was not a closer and a valid document was reported as one long
        # unclosed code block.
        ("a fence in a list item masks its sample", 0,
         ["# Changelog", V5, "### Added", "- ```text"]
         + ["  " + s for s in SAMPLE] + ["  ```"]),
        ("a fence four columns into its item still closes", 0,
         ["# Changelog", V5, "### Added", "-    ```text"]
         + ["     " + s for s in SAMPLE] + ["     ```"]),
        ("a fence in a tab-marked item still closes", 0,
         ["# Changelog", V5, "### Added", "-\t```text"]
         + ["    " + s for s in SAMPLE] + ["    ```"]),
        ("a fence in a `10. ` item still closes", 0,
         ["# Changelog", V5, "### Added", "10. ```text"]
         + ["    " + s for s in SAMPLE] + ["    ```"]),
        ("a fence in a nested item still closes", 0,
         ["# Changelog", V5, "### Added", "- - ```text"]
         + ["    " + s for s in SAMPLE] + ["    ```"]),
        # A line that falls BELOW the item's content column leaves the item, so it
        # leaves the fence: the renderer shows the entry heading and the extractor
        # would cut there. Masking it was the under-report the column now closes.
        ("a list-item fence ends where the content dedents", 1,
         ["# Changelog", V4, "### Added", "- ```text", V5, "### Added", "- y",
          "  ```", "  ```"]),
        # ...but a BLANK line is not a dedent. Both `### Fixed` lines stay masked;
        # unmasked they are a duplicate category and the count moves.
        ("a blank line does not end a list-item fence", 0,
         ["# Changelog", V5, "### Added", "- ```text", "", "  ### Fixed",
          "  ### Fixed", "  ```"]),

        # A QUOTED line can dedent out of the item too, and its own content indent
        # is not the column that decides it: `>` at column 0 leaves an item whose
        # content starts at column 2, and the renderer then shows the heading
        # inside the quote. Reading `indent_columns` of the quote's content
        # instead measured three columns, kept the fence open and hid it.
        # A COUNT CANNOT SEE THIS ONE. Keeping the fence open reports the opener
        # as never closed -- one finding, just like naming the heading -- so the
        # fixture asserts WHICH invariant is named.
        ("a quoted line below the item's column ends the fence", 1,
         ["# Changelog", V5, "### Added", "- ```text",
          ">    ## [0.4.0] — 2026-01-01",
          "@@says:sits behind `>` on the same line@@"]),

        # -- ROUND 8: TABS IN A MARKER PUT CONTENT AT A TAB STOP ---------------
        # `-\tname` starts its content at column 4, not 2. Measured with `len()`
        # the setext underline's 0-3 allowance was counted from column 2, so an
        # underline at six or seven columns -- a heading to every renderer -- was
        # invisible here while `release.yml` published the release into the notes
        # above it.
        ("a tab-marked setext release at its content column", 1,
         ["# Changelog", V5, "### Added", "-\t[0.9.7]", "    -------"]),
        ("a tab-marked setext release three columns past it", 1,
         ["# Changelog", V5, "### Added", "-\t[0.9.7]", "       -------"]),
        ("four columns past it is not a heading", 0,
         ["# Changelog", V5, "### Added", "-\t[0.9.7]", "        -------"]),
        ("below the content column it is not a heading", 0,
         ["# Changelog", V5, "### Added", "-\t[0.9.7]", "   -------"]),
        ("a tab-marked ordered setext release", 1,
         ["# Changelog", V5, "### Added", "1.\t[0.9.7]", "    -------"]),
        # Nested tabs compound: `-\t-\t` reaches column 8, and each tab stop
        # depends on the column the one before it left off at.
        ("nested tab markers put content at column 8", 1,
         ["# Changelog", V5, "### Added", "-\t-\t[0.9.7]", "        -------"]),
        ("a space-then-tab marker reaches the same stop", 1,
         ["# Changelog", V5, "### Added", "- \t[0.9.7]", "    -------"]),
        ("a tab-marked setext CATEGORY is a heading too", 1,
         ["# Changelog", V5, "### Added", "-\tChanged", "    -------"]),
        ("a space-marked setext release is unchanged", 1,
         ["# Changelog", V5, "### Added", "- [0.9.7]", "  -------"]),

        # -- ROUND 9: A BULLET INSIDE A FENCE IS DATA -------------------------
        # Once a fence is open, its contents are DATA. The old rule ended a
        # list-opened fence at any line carrying a list marker, so the most
        # ordinary thing a changelog preamble can hold -- a fenced example
        # containing a Markdown list -- broke the fence open and its `### Fixed`
        # and `## [x.y.z]` were read as live structure. Every fixture here renders
        # as ONE fenced code block and nothing else, so the answer is 0.
        ("bullets inside an unordered-list fence are data", 0,
         ["# Changelog", V5, "### Added", "- ```text", "  - item", "  * item",
          "  + item", "  ```"]),
        ("a bulleted sample inside a list fence is data", 0,
         ["# Changelog", V5, "### Added", "- ```text"]
         + ["  - " + s for s in SAMPLE] + ["  ```"]),
        ("an ordered sample inside an ordered-list fence is data", 0,
         ["# Changelog", V5, "### Added", "1. ```markdown", "   1. item", "   2. item",
          "   10. item", "", "   ### Fixed", "   ### Fixed", "   ```"]),
        ("list content inside a `10. ` fence is data", 0,
         ["# Changelog", V5, "### Added", "10. ```text", "    - item", "    ### Fixed",
          "    ### Fixed", "    ```"]),
        ("list content inside a nested-list fence is data", 0,
         ["# Changelog", V5, "### Added", "- - ```text", "    - item", "    ### Fixed",
          "    ### Fixed", "    ```"]),
        # The two frames a list marker can sit in, and they measure from different
        # places: `> - ` puts its item INSIDE the quote, `- > ` outside it.
        ("list content inside a quote+list fence is data", 0,
         ["# Changelog", V5, "### Added", "> - ```text", ">   - item", ">   ### Fixed",
          ">   ### Fixed", ">   ```"]),
        ("list content inside a list+quote fence is data", 0,
         ["# Changelog", V5, "### Added", "- > ```text", "  > - item", "  > ### Fixed",
          "  > ### Fixed", "  > ```"]),
        ("list content inside a tab-marked fence is data", 0,
         ["# Changelog", V5, "### Added", "-\t```text", "    - item", "    ### Fixed",
          "    ### Fixed", "    ```"]),
        ("nested bullets and a blank line inside a fence are data", 0,
         ["# Changelog", V5, "### Added", "- ```text", "  - item", "", "    - nested",
          "  - ### Fixed", "  1. ## [0.9.7] — 2026-09-05", "  ```"]),

        # ...AND THE OTHER DIRECTION, which is why the fix is a column and not a
        # blanket "ignore list-like lines". A line that LEAVES the item still ends
        # the fence, and the structure after it is still seen.
        ("a genuine sibling item still ends the fence", 1,
         ["# Changelog", V5, "### Added", "- ```text", "- ### Fixed"]),
        ("a one-column dedent still ends the fence", 1,
         ["# Changelog", V5, "### Added", "- ```text", " - ### Fixed"]),
        ("a column-0 release heading still ends the fence", 1,
         ["# Changelog", V4, "### Added", "- x", "- ```text", V5, "### Added", "- y",
          "  ```", "  ```"]),
        ("structure after a correct close is still seen", 1,
         ["# Changelog", V5, "### Added", "- ```text", "  - item", "  ```", "### Added"]),
        # A fence whose only "closer" is a bullet is NOT closed. Before the fix the
        # bullet ended it and the file looked clean; the renderer says otherwise.
        ("an unclosed list fence is reported", 1,
         ["# Changelog", V5, "### Added", "- ```text", "  - item", "  - more"]),
        ("a closer four columns past the item does not close", 1,
         ["# Changelog", V5, "### Added", "- ```text", "  - item", "      ```"]),
        ("a quoted closer does not close a list fence", 1,
         ["# Changelog", V5, "### Added", "- ```text", "  - item", "> ```"]),
        # A LINE THAT NEVER REACHES THE ITEM'S OWN QUOTE FRAME has left it past
        # every column, and `quote_rest` says so by returning None. The frame
        # here is one quote deep (`- > - ` puts its innermost item inside the
        # quote), and the next line's markers do not line up with it: the renderer
        # shows the heading, and reading None as "still inside" hid it.
        ("a line that leaves the item's quote frame ends the fence", 2,
         ["# Changelog", V5, "### Added", "- > - ```text", "- > - ### Fixed",
          "  > - ```"]),
        # A CLOSER MAY BE PRECEDED BY SPACES AND BY NOTHING ELSE (4.5), so a list
        # marker disqualifies it: `- ``` ` inside a fence is code text. Reading the
        # closer off the container-stripped content accepted it, ended the block a
        # line early, and the item's real closer then opened a fence that ran to
        # end of file -- two findings on a document the renderer calls valid.
        ("a bare delimiter behind a bullet is fence content", 0,
         ["# Changelog", V5, "### Added", "```text", "- ```",
          "## [1.2.3] — 2026-01-01", "```"]),
        ("...inside a list item too", 0,
         ["# Changelog", V5, "### Added", "- ```text", "  - ```", "  ### Fixed",
          "  ```"]),
        ("...behind `*`, `1.`, `10.` and a tilde fence", 0,
         ["# Changelog", V5, "### Added", "```text", "* ```", "1. ```", "10. ```",
          "### Fixed", "```", "", "~~~text", "- ~~~", "### Fixed", "~~~"]),
        # EVERY enclosing item, not just the innermost. `- > - ` nests an item at
        # depth 0 and another at depth 1; a line that leaves the OUTER one takes
        # the quote and the fence with it, and the renderer shows the heading on
        # it. Keeping only the innermost item measured the wrong frame and masked
        # a renderer-visible release heading -- an under-report.
        ("a line that leaves an outer item ends the fence", 2,
         ["# Changelog", V5, "### Added", "- > - ```text",
          ">   ## [0.9.7] — 2026-09-05", "  >   ```"]),
        ("...but a line that stays inside both is still data", 0,
         ["# Changelog", V5, "### Added", "- > - ```text", "  >   ### Fixed",
          "  >   ### Fixed", "  >   ```"]),
        # An item's frame is not reachable from a line whose quote marker is not at
        # its head: `  - > x` carries a quote, so its DEPTH matches, but the fence's
        # inner item lives one quote in and this line's first container is a list.
        # `quote_rest` says so by returning None, and that is a dedent past
        # every column -- the renderer shows the heading.
        ("a line whose quote is not at its head leaves the frame", 2,
         ["# Changelog", V5, "### Added", "- > - ```text", "  - > ### Fixed",
          "  > - ```"]),
        # ...and the INNER item is checked as well as the outer one: `- - ```text`
        # nests items at columns 2 and 4, and a new item at column 2 satisfies the
        # outer while leaving the inner. The renderer ends the fence there.
        # Two: the heading, and the closer that is no longer one. The new item at
        # column 2 ends the fence, so the `    ``` ` below it starts a fresh item's
        # fence at that item's content column -- unclosed, exactly as the renderer
        # shows it.
        ("a new inner item at the outer column ends the fence", 2,
         ["# Changelog", V5, "### Added", "- - ```text", "  - ### Fixed", "    ```"]),
        ("...and content at the inner column stays data", 0,
         ["# Changelog", V5, "### Added", "- - ```text", "    - ### Fixed", "    ```"]),
        # A QUOTED BLANK IS A BLANK. `>` alone strips to `>`, so testing the RAW
        # line called it non-blank and its zero columns of content read as a dedent
        # -- breaking a fence open in the middle of a quoted sample. Blankness is
        # read in each item's own frame, as the dedent is.
        ("a quoted blank does not end a fence in a quoted item", 0,
         ["# Changelog", V5, "### Added", "> - ```text", ">   ### Fixed", ">",
          ">   ## [0.9.7] — 2026-09-05", ">   ```"]),
        # A FENCE MAY BE OPENED ON A CONTINUATION LINE, and then the item chain is
        # not on that line at all. Reading it from the opener alone left such a
        # fence with no column: no dedent could end it, its own closer four or five
        # columns in was rejected, and a real release heading below it was masked
        # with no finding at all -- 98 such documents in the generated corpus.
        # `fence_mask` now carries the item chain across lines: a line with markers
        # restates it, one without keeps as much as its indentation still reaches.
        ("a fence on a continuation line does not hide the entry below it", 1,
         ["# Changelog", V4, "### Added", "- an item", "  ```text", V5, "### Added",
          "- y"]),
        ("...and still masks its own sample", 0,
         ["# Changelog", V5, "### Added", "- an item", "  ```text", "  ### Fixed",
          "  ### Fixed", "  ```"]),
        # ...at the item's OWN column, which is what the opener's three-column
        # allowance is counted from: four columns inside a `10. ` item is an
        # ordinary nested sample, and counting from column 0 opened nothing there.
        ("...four columns into a `10. ` item", 0,
         ["# Changelog", V5, "### Added", "10. an item", "    ```text", "    ### Fixed",
          "    ### Fixed", "    ```"]),
        ("...and inside a quoted item", 0,
         ["# Changelog", V5, "### Added", "> - an item", ">   ```text", ">   ### Fixed",
          ">   ### Fixed", ">   ```"]),
        # THE CONTROL. With no list above it, a delimiter indented two columns is a
        # TOP-LEVEL fence whose content may sit at column 0, and the renderer keeps
        # masking to the closer. That is the distinction the carried chain makes.
        ("a top-level indented fence still masks a column-0 heading", 0,
         ["# Changelog", V5, "### Added", "  ```text", "## [0.9.7] — 2026-09-05",
          "  ```"]),
        # The closer's allowance is counted in the FENCE's quote frame, so an item
        # recorded at a shallower depth imposes no column there: `- > ```text` is
        # not closed by a delimiter four columns into the quote, and using the
        # item's document column stretched the allowance to five.
        ("a shallower item imposes no column inside the quote", 1,
         ["# Changelog", V5, "### Added", "- > ```text", "  >     ### Fixed",
          "  >     ```"]),
        # THE DEEP PASS SILENCES ITS OWN CONTENT AND NOT WHAT IS ABOVE IT. A
        # six-column delimiter inside a `- ` item used to silence a `### Removed`
        # at FOUR columns, which the renderer shows as a heading -- four columns is
        # only two inside the item. Content of a fence is indented at least as far
        # as the fence is; a shallower line has left it.
        ("a line shallower than a deep opener is not its content", 1,
         ["# Changelog", V5, "### Added", "- item", "      ```text", "    ### Removed",
          "      ```"]),
        ("...and a line at the opener's own indent still is", 0,
         ["# Changelog", V5, "### Added", "- item", "      ```text", "      ### Removed",
          "      ```"]),
        # ...and a SHALLOWER DELIMITER does not close it either: it belongs to a
        # different container, and the renderer shows it opening a fence of its own
        # that swallows what follows. Closing on it would have unmasked that.
        ("a shallower delimiter does not close a deep fence", 2,
         ["# Changelog", V5, "### Added", "- item", "      ```text", "    ### Removed",
          "    ```", "      ### Security"]),
        ("...but one at the opener's indent does", 1,
         ["# Changelog", V5, "### Added", "- item", "      ```text", "      ### Removed",
          "      ```", "      ### Security"]),
        ("...and a bare blank does not end one at top level", 0,
         ["# Changelog", V5, "### Added", "- ```text", "  ### Fixed", "",
          "  ## [0.9.7] — 2026-09-05", "  ```"]),

        # -- ROUND 10: THE HEADING RULES READ THE CONTAINER CHAIN TOO ---------
        # Three under-reports round 9 named and did not close, all one question:
        # `classify_heading` and the setext alignment measured a container-prefixed
        # line from column 0, while `fence_mask` had the chain. They read it from
        # `container_chains` now, and the boundaries below were taken from the
        # renderer: `> -   item` puts its item's content at a column where three
        # to EIGHT spaces after the `>` are a heading and nine are not.
        ("a release heading inside a quoted item is reported", 1,
         ["# Changelog", V5, "### Added", "> -   item",
          ">     ## [0.9.7] — 2026-09-01"]),
        ("...at the far end of the allowance too", 1,
         ["# Changelog", V5, "### Added", "> -   item",
          ">        ## [0.9.7] — 2026-09-01"]),
        ("...but one column past it is indented code", 0,
         ["# Changelog", V5, "### Added", "> -   item",
          ">         ## [0.9.7] — 2026-09-01"]),
        # A setext pair written as two CONTINUATION lines inside a nested item:
        # neither line restates a marker, so measured from column 0 the pair fell
        # outside the 0-3 allowance and the release heading went unreported.
        ("a setext release pair on continuation lines is reported", 1,
         ["# Changelog", V5, "### Added", "- 1. item", "",
          "    [0.9.7] — 2026-09-05", "    -----"]),
        ("...and four columns past the item's column is not a heading", 0,
         ["# Changelog", V5, "### Added", "- 1. item", "",
          "    [0.9.7] — 2026-09-05", "        -----"]),
        # A `>` more than three columns past its container's content column is
        # literal text inside an indented code block, so the fence it seemed to
        # open is not there -- and the heading the phantom fence masked is real.
        # A COUNT CANNOT SEE THIS ONE: opening the phantom fence reports it as
        # never closed -- one finding, just like naming the heading it masks -- so
        # the fixture asserts WHICH invariant is named.
        ("a quote four columns in opens no fence", 1,
         ["# Changelog", V5, "### Added", "    > ```text",
          "> ## [0.9.9] — 2026-10-02",
          "@@says:sits behind `>` on the same line@@"]),
        ("...but three columns in is a real quote", 0,
         ["# Changelog", V5, "### Added", "   > ```text", "   > ### Fixed",
          "   > ```"]),
        ("...and four columns inside a `10. ` item is too", 0,
         ["# Changelog", V5, "### Added", "10. item", "    > ```text",
          "    > ### Fixed", "    > ```"]),

        # -- ROUND 11: A NESTED ITEM EXTENDS THE CHAIN, IT DOES NOT REPLACE IT --
        # `container_chains` used to throw the carried chain away on any line that
        # stated a marker, so `- outer` over `  - nested` left only the nested
        # item and the still-open outer one vanished. The next continuation line
        # fell below the nested column with nothing to fall back to and read as
        # top level, which broke BOTH ways at once:
        #
        #   * a fence opened there belonged to NO item, so nothing but a closing
        #     delimiter could end it and the column-0 release heading that ends
        #     the list, the fence and the block in every renderer was masked --
        #     and `changelog-section.awk`, which carried the same erasure,
        #     published the two releases as one note;
        #   * where the item's column was wider than three, the delimiter itself
        #     read as an indented code block, no fence opened at all, and the
        #     sample's contents were scanned as live structure -- the broken link
        #     inside these samples is what that direction reports.
        #
        # Every fixture here is a document the RENDERER shows as one fenced block
        # inside a list, with `## [0.4.0]` a real heading below it.
        ("an outer bullet survives a nested bullet", 0,
         ["# Changelog", V5, "### Added", "- item", "- outer", "  - nested", "  text",
          "  ```text", "  see [x](nope.md)", V4, "### Added", "- a"]),
        ("...an outer ordered item survives a nested one", 0,
         ["# Changelog", V5, "### Added", "- item", "1. outer", "   1. nested", "   text",
          "   ```text", "   see [x](nope.md)", V4, "### Added", "- a"]),
        ("...and the two kinds mix", 0,
         ["# Changelog", V5, "### Added", "- item", "1. outer", "   - nested", "   text",
          "   ```text", "   see [x](nope.md)", V4, "### Added", "- a"]),
        ("...tab-marked items are items too", 0,
         ["# Changelog", V5, "### Added", "- item", "-\touter", "\t-\tnested", "\ttext",
          "\t```text", "\tsee [x](nope.md)", V4, "### Added", "- a"]),
        ("...a multi-digit marker's column is its own", 0,
         ["# Changelog", V5, "### Added", "- item", "10. outer", "    100. nested",
          "    text", "    ```text", "    see [x](nope.md)", V4, "### Added", "- a"]),
        ("...and the chain is a chain, not a pair", 0,
         ["# Changelog", V5, "### Added", "- item", "- a", "  - b", "    - c", "    text",
          "    ```text", "    see [x](nope.md)", V4, "### Added", "- a"]),
        # The quote shapes were never broken -- a fence inside a blockquote is
        # ended by the DEPTH test, whatever happened to the item chain -- and they
        # are here so a future change to the chain cannot quietly take them out.
        ("a quoted nested item still ends its fence at the quote", 0,
         ["# Changelog", V5, "### Added", "- item", "> - outer", ">   - nested", ">   text",
          ">   ```text", ">   see [x](nope.md)", V4, "### Added", "- a"]),
        ("...and so does a quote inside an item", 0,
         ["# Changelog", V5, "### Added", "- item", "- > outer", "  > - nested",
          "  >   text", "  >   ```text", "  >   see [x](nope.md)", V4, "### Added", "- a"]),
        # A COUNT CANNOT SEE THE UNDER-REPORT: masking the release reports the
        # fence as never closed, which is also one finding. The fixture therefore
        # asserts WHICH invariant is named -- and an ordering complaint can only
        # be made about a release the checker actually parsed.
        ("a release below a nested container is parsed, not masked",
         1, ["# Changelog", V4, "### Added", "- item", "- outer", "  - nested", "  text",
             "  ```text", V5, "### Added", "- a",
             "@@says:entries run newest first@@"]),
        # ...and the other direction: while the nested item is still ACTIVE, a
        # release-like heading inside the fence stays data, and the closer in the
        # outer item's frame still closes.
        ("a heading inside the fence stays inside it", 0,
         ["# Changelog", V5, "### Added", "- outer", "  - nested", "  text", "  ```text",
          "  ## [0.3.0] — 2026-01-03", "  ```", "- after", V4, "### Added", "- a"]),
        ("...markers inside a fence do not outlive its closer", 1,
         ["# Changelog", V5, "### Added", "- outer", "  ```text", "  - a", "    - b",
          "      - c", "  ```", "  see [x](nope.md)", V4, "### Added", "- a"]),

        # -- a bare CR is named, not silently resolved ------------------------
        ("a bare carriage return inside a line is a finding", 1,
         ["# Changelog", V5, "### Added", "- a. Evidence: PR #2.\r### Fixed", "- b"]),
    ]:
        # A fixture carrying the `@@no-placeholder@@` marker asserts the TEXT of
        # the findings instead of their count: the defect it pins is a sentinel
        # (`v?`) leaking into the URL a finding tells the author to write, which
        # no count can see.
        # `@@says:<text>@@` asserts that some finding CONTAINS that text. Where a
        # broken rule and a working one produce the same NUMBER of findings and
        # differ only in which invariant they name, a count cannot see the defect
        # -- and naming the wrong invariant is itself the defect (a duplicate
        # `## [Unreleased]` reported as "must be the first entry" tells the author
        # to move it to the top, which makes the file worse).
        if lines and lines[-1].startswith("@@says:"):
            want_text = lines[-1][len("@@says:"):-2]
            found = analyse(root / "CHANGELOG.md", lines[:-1], root)
            checked += 1
            if not any(want_text in f for f in found):
                failures += 1
                print(f"self-test FAIL: changelog {label}: no finding said "
                      f"{want_text!r}: {found}", file=sys.stderr)
            continue
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

    # --- THE CONTAINER CHAIN ITSELF ------------------------------------------
    # The invariant `container_chains` exists to keep, asserted directly rather
    # than through whatever a fence or a heading happens to do with it:
    #
    #     A continuation line may EXTEND the active chain, and may TRIM it back to
    #     an enclosing container that is still open. It may never ERASE an
    #     enclosing container that the line still reaches.
    #
    # Every expectation was read off `markdown-it-py`: the chain for a line lists
    # the list items the renderer shows it inside, innermost last, each with the
    # content column its marker establishes and the quote depth it sits at. The
    # last two documents pin the two directions a chain can be wrong -- one item
    # too few (the round-11 defect) and one too many (fence content leaking out
    # past the closer).
    for label, doc, want in [
        ("a nested item extends its outer one, and a dedent returns to it",
         ["- outer", "  - nested", "    body", "", "  back", "text"],
         [((0, 2),), ((0, 2), (0, 4)), ((0, 2), (0, 4)), ((0, 2), (0, 4)),
          ((0, 2),), ()]),
        ("...inside a blockquote, each item read at its own depth",
         ["> - q", ">   - qn", ">   qback", "> qtop"],
         [((1, 2),), ((1, 2), (1, 4)), ((1, 2),), ()]),
        ("...with ordered markers of different widths",
         ["10. outer", "    100. nested", "    back", "text"],
         [((0, 4),), ((0, 4), (0, 9)), ((0, 4),), ()]),
        ("...and with tabs, where one marker is four columns",
         ["-\touter", "\t-\tnested", "\tback", "text"],
         [((0, 4),), ((0, 4), (0, 8)), ((0, 4),), ()]),
        ("markers inside a fence extend the chain and the closer trims it back",
         ["- outer", "  ```t", "  - a", "    - b", "  ```", "  after", "top"],
         [((0, 2),), ((0, 2),), ((0, 2), (0, 4)), ((0, 2), (0, 4), (0, 6)),
          ((0, 2),), ((0, 2),), ()]),
    ]:
        checked += 1
        got = container_chains(doc)
        if got != want:
            failures += 1
            print(f"self-test FAIL: container chain -- {label}: expected {want}, got {got}",
                  file=sys.stderr)

    # --- THE COLUMN MODEL ITSELF ---------------------------------------------
    # Every expectation here was read off `markdown-it-py` in CommonMark mode by
    # asking, for each marker, at which underline indents a setext heading
    # appears: the lowest is the item's CONTENT COLUMN. They are written down as
    # literals so the self-test needs nothing but a stdlib `python3`. The two
    # defects this table pins are a tab measured as one character (`-\tx` is four
    # columns wide, not two) and a fifth column of padding taken as marker width
    # (`-     x` leaves the content one column past the marker, with the rest an
    # indented code block).
    for raw, cols, depth in [
        ("- x", 2, 0), ("-  x", 3, 0), ("-   x", 4, 0), ("-    x", 5, 0),
        ("-     x", 2, 0), ("-      x", 2, 0),
        # An item that carries nothing but its marker starts its content one
        # column past it, however much whitespace trails (5.2, and the renderer
        # puts the last openable fence at column 5 for `-`, `-  ` and `-    `
        # alike). Taking the trailing run as marker width made it 5.
        ("-  ", 2, 0), ("-    ", 2, 0),
        ("-\tx", 4, 0), ("- \tx", 4, 0), ("-  \tx", 4, 0), ("-   \tx", 2, 0),
        ("1. x", 3, 0), ("1.\tx", 4, 0), ("9. x", 3, 0),
        ("10. x", 4, 0), ("10.\tx", 4, 0), ("100. x", 5, 0), ("1000. x", 6, 0),
        ("10) x", 4, 0),
        ("- - x", 4, 0), ("-\t-\tx", 8, 0), ("- \t- x", 6, 0), ("1. 1. x", 6, 0),
        ("> x", 0, 1), (">  x", 0, 1), (">> x", 0, 2), ("> > x", 0, 2),
        ("> - x", 2, 1), (">\t- x", 4, 1), ("- > x", 0, 1),
        ("x", 0, 0), ("-x", 0, 0), ("1.5 x", 0, 0), ("-", 0, 0),
    ]:
        got_cols, got_depth = strip_containers(raw)[3], strip_containers(raw)[2]
        checked += 1
        if (got_cols, got_depth) != (cols, depth):
            failures += 1
            print(f"self-test FAIL: strip_containers({raw!r}) -> columns {got_cols}, "
                  f"depth {got_depth}; want {cols}, {depth}", file=sys.stderr)

    # A tab advances to the next four-column stop, from wherever the line is.
    for raw, want in [("\tx", "    x"), (" \tx", "    x"), ("   \tx", "    x"),
                      ("    \tx", "        x"), ("a\tb", "a   b"), ("no tabs", "no tabs")]:
        checked += 1
        if expand_tabs(raw) != want:
            failures += 1
            print(f"self-test FAIL: expand_tabs({raw!r}) -> {expand_tabs(raw)!r}, "
                  f"want {want!r}", file=sys.stderr)

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


    # ------------------------------------------------------------------
    # THE OTHER HALF OF THE CONTRACT, EXECUTED RATHER THAN ASSERTED.
    #
    # `check_changelog_notes_boundary` exists to protect `release.yml`'s notes
    # extractor, and until now this script only DESCRIBED what that extractor
    # does. A description is not a proof: if the extractor's fence handling
    # regressed, every rule above would keep passing while the published notes
    # went wrong. The extractor is one file -- `scripts/changelog-section.awk`,
    # which `release.yml` runs twice, in `validate` and in `draft-release` --
    # so it can simply be RUN here, on inputs whose correct extraction is known.
    #
    # Skipped WITH A NOTE, never silently, where there is no `awk` (Windows
    # developer machines); the `docs` job and `preflight.sh` both have one.
    # ------------------------------------------------------------------
    extractor = Path(__file__).resolve().parent / "changelog-section.awk"

    # ---- THE PORTABILITY GATE, AND THE PROOF THAT IT CAN FIRE ----------------
    #
    # `awk_spaced_calls` answers the one question about the extractor that no
    # extraction fixture can: will every `awk` PARSE it? Its docstring carries the
    # grammar; what is executed here is the pair of properties a lint has to have,
    # because a lint that cannot fire passes forever:
    #
    #   * IT MUST FIRE on the forbidden syntax. Running the matcher only over a
    #     file that is already clean is VACUOUS -- every result is the empty list,
    #     and a matcher that had stopped recognising `qrest (` would look exactly
    #     the same. That is how the gate could have died silently, so the firing
    #     half is now synthetic input whose answer is known and non-empty.
    #   * IT MUST STAY SILENT on text that merely CONTAINS those characters. A
    #     comment, a string, a regex or a documentation line saying `qrest (` runs
    #     under gawk, mawk and the one-true-awk alike, and failing the build over
    #     prose is a false report, not caution. A raw substring search passes the
    #     firing half and fails every case below it.
    #
    # Each case is (name, awk source, expected (line, function) hits).
    portability_cases: list[tuple[str, str, list[tuple[int, str]]]] = [
        # ---- MUST DETECT: this is really what gawk and nawk refuse to parse.
        ("a spaced call of a defined function",
         'function qrest(s, k) { return s }\nBEGIN { x = qrest (s, 1) }\n', [(2, "qrest")]),
        ("a tab between the name and its `(`",
         'function qrest(s, k) { return s }\nBEGIN { x = qrest\t(s, 1) }\n', [(2, "qrest")]),
        ("several spaces are no better than one",
         'function qrest(s, k) { return s }\nBEGIN { x = qrest   (s, 1) }\n', [(2, "qrest")]),
        ("every defined function is covered, each at its own line",
         'function expand(s) { return s }\nfunction lead(s) { return s }\n'
         'BEGIN { x = expand (1); }\nBEGIN { y = lead (2) }\n',
         [(3, "expand"), (4, "lead")]),
        ("a `#` inside a string does not hide the call after it",
         'function qrest(s, k) { return s }\nBEGIN { x = "a # b"; y = qrest (1) }\n',
         [(2, "qrest")]),
        ("a division earlier on the line is not a regex that swallows the call",
         'function qrest(s, k) { return s }\nBEGIN { y = a / b; x = qrest (1) }\n',
         [(2, "qrest")]),
        ("an unterminated string ends at the newline, not at end of file",
         'function qrest(s, k) { return s }\nBEGIN { print "oops\nx = qrest (1) }\n',
         [(3, "qrest")]),
        # ---- MUST NOT DETECT: all three awks run every one of these.
        ("a comment naming the call",
         'function qrest(s, k) { return s }\n# qrest (s, k) is called below\n'
         'BEGIN { x = qrest(s, 1) }\n', []),
        ("a string containing the call",
         'function qrest(s, k) { return s }\nBEGIN { print "qrest (s, 1)" }\n', []),
        ("a trailing comment after a correct call",
         'function qrest(s, k) { return s }\nBEGIN { x = qrest(1) } # qrest (2)\n', []),
        # The `(` is UNESCAPED here on purpose: an escaped one would not match the
        # rule's own pattern, and the fixture would pass without the regex ever
        # being skipped. `/qrest (a|b)/` is an ordinary ERE with a group, and it
        # carries the exact characters the rule looks for.
        ("a regex literal containing the call",
         'function qrest(s, k) { return s }\nBEGIN { if ($0 ~ /qrest (a|b)/) print }\n', []),
        ("a DEFINITION written with a space, which every awk accepts",
         'function qrest (s, k) { return s }\nBEGIN { x = qrest(1) }\n', []),
        ("a built-in, which is exempt and is this file's own style",
         'function qrest(s, k) { return s }\nBEGIN { x = substr (s, 1, 1) }\n', []),
        ("a longer identifier that merely ends in the name",
         'function qrest(s, k) { return s }\nBEGIN { x = myqrest (1) }\n', []),
        ("the correct call, which is the whole point",
         'function qrest(s, k) { return s }\nBEGIN { x = qrest(1) }\n', []),
    ]
    for name, src, want in portability_cases:
        checked += 1
        got = awk_spaced_calls(src)
        if got != want:
            failures += 1
            print(f"self-test FAIL: awk portability -- {name}: expected {want}, got {got}",
                  file=sys.stderr)

    # ...and only then the real file, whose answer must be the empty list.
    if extractor.is_file():
        checked += 1
        spaced = awk_spaced_calls(extractor.read_text(encoding="utf-8"))
        if spaced:
            sites = ", ".join(f"line {n} (`{fn}`)" for n, fn in spaced)
            failures += 1
            print(f"self-test FAIL: {extractor.name} calls its own function with a "
                  f"space before `(` -- {sites}. POSIX awk reads that as a variable, "
                  f"and `gawk` and the one-true-awk refuse to parse the program at all",
                  file=sys.stderr)

    awk = shutil.which("awk")
    if awk is None:
        print("check-docs: NOTE -- no `awk` on PATH, so the release-notes "
              "extractor's cases were not run (they run in CI and in preflight.sh).",
              file=sys.stderr)
    elif not extractor.is_file():
        failures += 1
        checked += 1
        print(f"self-test FAIL: {extractor.name} is missing -- `release.yml` runs it "
              f"in two steps and would fail at tag time", file=sys.stderr)
    else:
        F = "```"
        F4 = "````"
        entry = ["## [0.9.8] — 2026-10-01", "### Fixed", "- b"]
        older = ["## [0.9.7] — 2026-09-05", "### Added", "- a"]
        extractor_cases: list[tuple[str, str, list[str], list[str]]] = [
            # (name, version, document, expected extraction)
            ("the entry is extracted verbatim, heading included",
             "0.9.8", ["# Changelog", *entry, *older], entry),
            ("extraction stops at the next entry heading",
             "0.9.8", ["# Changelog", *entry, *older, "## [0.9.6] — 2026-08-01"], entry),
            ("the oldest entry extracts to EOF",
             "0.9.7", ["# Changelog", *entry, *older], older),
            ("a version with no entry extracts to nothing",
             "0.9.9", ["# Changelog", *entry, *older], []),
            # Clause: a fenced block is DATA. The preamble template is the case
            # that actually exists in this repository's policy document.
            ("a heading inside a fenced example is not an entry",
             "0.9.8", ["# Changelog", F + "markdown", "## [0.9.8] — 2026-10-01", F, *older],
             []),
            ("a fenced heading inside a real entry does not cut it short",
             "0.9.8", ["# Changelog", *entry, F, "## [0.9.7] — 2026-09-05", F, "- c", *older],
             [*entry, F, "## [0.9.7] — 2026-09-05", F, "- c"]),
            # Clause 1: the SAME character closes a fence.
            ("a `~~~` line inside a ``` block is data",
             "0.9.8", ["# Changelog", *entry, F, "~~~", "## [0.9.7] — 2026-09-05", F, *older],
             [*entry, F, "~~~", "## [0.9.7] — 2026-09-05", F]),
            # Clause 2 + 3: length, and an info string can never close.
            ("a nested ```cpp inside a ```` example does not close it",
             "0.9.8",
             ["# Changelog", *entry, F4 + "markdown", F + "cpp", "int x;", F,
              "## [0.9.7] — 2026-09-05", F4, *older],
             [*entry, F4 + "markdown", F + "cpp", "int x;", F,
              "## [0.9.7] — 2026-09-05", F4]),
            # Clause 3 ALONE: a nested fence of the SAME character and the SAME
            # length as its opener, carrying an info string. Clause 2 cannot
            # decide this one -- only "an opening fence can never be a closer"
            # can -- and without it the example's body is scanned as structure
            # and the real closer re-opens the block, inverting the mask.
            ("a same-length ```cpp inside a ```markdown example does not close it",
             "0.9.8",
             ["# Changelog", *entry, F + "markdown", F + "cpp", "int x;", F, *older],
             [*entry, F + "markdown", F + "cpp", "int x;", F]),
            # A CRLF file. `check-docs.py` reads with universal newlines and
            # never sees the `\r`, so it called such a file clean while no fence
            # in the extractor could ever close -- the first fenced block ran to
            # EOF and every older entry was published inside the newest one's
            # notes. Nothing else in the pipeline would have caught it.
            ("a CRLF file still closes its fences", "0.9.8",
             ["# Changelog\r", "## [0.9.8] — 2026-10-01\r", "```\r",
              "## [0.9.7] — 2026-09-05\r", "```\r", "- z\r",
              "## [0.9.6] — 2026-08-01\r", "- old\r"],
             ["## [0.9.8] — 2026-10-01", "```", "## [0.9.7] — 2026-09-05", "```", "- z"]),
            # The closing `]` in the entry test is load-bearing: without it,
            # `## [0.9.80]` answers to `ver=0.9.8` and a real release's notes are
            # taken from the wrong entry.
            ("a longer version is not a prefix match",
             "0.9.8",
             ["# Changelog", "## [0.9.80] — 2026-12-01", "### Fixed", "- wrong",
              *entry],
             entry),
            # The `^` anchor on the termination rule: a bullet that quotes an
            # entry heading mid-line must not end the extraction.
            ("a mid-line `## [` in a bullet does not terminate the entry",
             "0.9.8",
             ["# Changelog", *entry, "- see ## [0.9.7] for the original", *older],
             [*entry, "- see ## [0.9.7] for the original"]),
            # CommonMark 4.5: a BACKTICK fence's info string may not contain a
            # backtick, so ```a`b opens nothing and the entry heading below it
            # still terminates this release's notes. Opening a fence there ran
            # the two releases together in the published notes.
            ("a backtick in a backtick info string opens no fence", "0.9.8",
             ["# Changelog", *entry, F + "a`b", *older],
             [*entry, F + "a`b"]),
            # ...and the discriminating other half: a TILDE fence may carry one,
            # so the same heading IS data and is published with the entry.
            ("...but a tilde fence may carry one", "0.9.8",
             ["# Changelog", *entry, "~~~a`b", *older, "~~~"],
             [*entry, "~~~a`b", *older, "~~~"]),
            # ONE TAB IS FOUR COLUMNS, so a tab-indented ``` is an indented code
            # block and must not open a fence. Measured in characters it did, and
            # the mask then ran on until the next delimiter -- merging the
            # following entry into the release notes above it.
            ("a tab-indented ``` does not open a fence", "0.9.8",
             ["# Changelog", *entry, "\t" + F, *older],
             [*entry, "\t" + F]),
            # Up to three leading blanks is still a fence (CommonMark 4.5); the
            # strip is what makes the delimiter comparable.
            # ...and it is a fence INSIDE `- b`'s list item, three columns being one
            # column past that item's content column. The column-0 heading below it
            # leaves the item, so the fence ends there and the heading is a real
            # boundary -- which is what the renderer shows and what `fence_mask`
            # masks. The extractor used to keep the fence open and publish the
            # older release inside the newer one's notes.
            ("a three-space-indented fence is still a fence",
             "0.9.8",
             ["# Changelog", *entry, "   " + F, "## [0.9.7] — 2026-09-05", "   " + F,
              *older],
             [*entry, "   " + F]),
            # `index(...) == 1` is a PREFIX test, not a substring search: a
            # sentence that merely mentions the heading is prose.
            ("a mid-line mention of the heading does not start an extraction",
             "0.9.8",
             ["# Changelog", "## [0.9.9] — 2026-11-01", "### Fixed",
              "- see ## [0.9.8] — 2026-10-01 for the original", *entry],
             entry),
            # Four columns is an indented code block, not a fence: it must not
            # open one and swallow the rest of the file.
            ("a four-space-indented ``` does not open a fence",
             "0.9.8", ["# Changelog", *entry, "    " + F, *older],
             [*entry, "    " + F]),
            # WHY check_changelog_notes_boundary EXISTS, demonstrated: a `## `
            # heading below the first entry that is not an entry heading does
            # not terminate anything, so it and everything under it land in the
            # published notes of the release above it.
            # ROUND 9: A FENCE MAY BE OPENED BEHIND CONTAINER MARKERS. Not seeing
            # that opener made the sample's own CLOSER look like an opener, and the
            # mask then ran to end of file: NOTHING extracted, for any version, on
            # a document `check-docs.py` calls clean. The asymmetry is CommonMark's
            # (4.5) and is the same one the checker applies: an OPENER may sit
            # behind markers, a CLOSER may be preceded by spaces and nothing else.
            ("a fence opened in a list item is still a fence",
             "0.9.8", ["# Changelog", "- " + F + "text", "  ### Fixed", "  " + F,
                       *entry, *older],
             entry),
            ("...and its closer sits at the item's own column",
             "0.9.8", ["# Changelog", "10. " + F + "text", "    ## [1.2.3] — 2026-01-01",
                       "    " + F, *entry, *older],
             entry),
            ("...and a quoted item's closer inside the quote",
             "0.9.8", ["# Changelog", "> - " + F + "text", ">   ## [1.2.3] — 2026-01-01",
                       ">   " + F, *entry, *older],
             entry),
            ("a bare delimiter behind a bullet is fence content, not a closer",
             "0.9.8", ["# Changelog", F + "text", "- " + F, "## [1.2.3] — 2026-01-01", F,
                       *entry, *older],
             entry),
            # The allowance is measured from the item's own column: four columns
            # into the item is an indented code block, not a closer, so the fence
            # stays open and NOTHING extracts -- which is what the checker says too
            # ("code fence opened here is never closed"). Both tools fail closed on
            # the same document, which is the property that matters.
            # The delimiter four columns into the item is not a closer, so the fence
            # is unclosed -- and the line after it leaves the blockquote, which ends
            # the fence anyway. Both tools agree: `fence_mask` masks the two quoted
            # lines and nothing else, and the entry below extracts normally. Before
            # the container-exit rule the extractor masked to EOF and published
            # nothing for any version.
            ("a delimiter four columns into its item is not a closer",
             "0.9.8", ["# Changelog", "> - " + F + "text", ">       " + F, *entry, *older],
             entry),
            ("a bullet inside a list-item fence does not close it",
             "0.9.8", ["# Changelog", "- " + F + "text", "  - item",
                       "  ## [1.2.3] — 2026-01-01", "  " + F, *entry, *older],
             entry),
            # ROUND 10: A FENCE ENDS WHERE ITS CONTAINER DOES. Each of these
            # documents is one the checker ACCEPTS, and before the container-exit
            # rule the extractor swallowed every release below the fence: `0.9.8`
            # published both entries as one section and `0.9.7` extracted NOTHING,
            # so a release tag could not be cut. One transition per fixture.
            ("a list-item fence ends at a dedent, not at EOF",
             "0.9.8", ["# Changelog", "- " + F + "text", "  ## [9.9.9] — 2999-01-01",
                       *entry, *older],
             entry),
            ("a blockquote fence ends where the quote does",
             "0.9.8", ["# Changelog", "> " + F + "text", "> ## [9.9.9] — 2999-01-01",
                       *entry, *older],
             entry),
            ("...and a nested quote ends at top level too",
             "0.9.8", ["# Changelog", ">> " + F + "text", ">> ## [9.9.9] — 2999-01-01",
                       *entry, *older],
             entry),
            ("a nested list item's fence ends at a dedent",
             "0.9.8", ["# Changelog", "- - " + F + "text", "    ## [9.9.9] — 2999-01-01",
                       *entry, *older],
             entry),
            ("a quote+list fence ends at top level",
             "0.9.8", ["# Changelog", "> - " + F + "text", ">   ## [9.9.9] — 2999-01-01",
                       *entry, *older],
             entry),
            ("a list+quote fence ends at top level",
             "0.9.8", ["# Changelog", "- > " + F + "text", "  > ## [9.9.9] — 2999-01-01",
                       *entry, *older],
             entry),
            ("a fence opened on a continuation line ends at a dedent",
             "0.9.8", ["# Changelog", "- an item", "  " + F + "text",
                       "  ## [9.9.9] — 2999-01-01", *entry, *older],
             entry),
            ("a tab-marked item's fence ends at a dedent",
             "0.9.8", ["# Changelog", "-\t" + F + "text", "    ## [9.9.9] — 2999-01-01",
                       *entry, *older],
             entry),
            # ROUND 11: A NESTED MARKER EXTENDS THE CHAIN. The item chain used to
            # be REPLACED by whatever markers a line stated, so `- outer` over
            # `  - nested` dropped the outer item; the continuation line below
            # then fell below the nested column with nothing left to belong to,
            # and the fence it opened was recorded as being in NO item. Only a
            # closing delimiter could end it after that -- so the column-0 entry
            # heading below, which ends the list, the fence and the block in every
            # renderer, was swallowed: the newer entry's notes ran on through the
            # older one, and asking for the older version printed NOTHING.
            ("a nested item does not detach a fence from its outer item",
             "0.9.8", ["# Changelog", *entry, "- outer", "  - nested", "  text",
                       "  " + F + "text", *older],
             [*entry, "- outer", "  - nested", "  text", "  " + F + "text"]),
            ("...and the entry below it can still be cut",
             "0.9.7", ["# Changelog", *entry, "- outer", "  - nested", "  text",
                       "  " + F + "text", *older],
             older),
            ("...for ordered markers of different widths",
             "0.9.7", ["# Changelog", *entry, "10. outer", "    100. nested", "    text",
                       "    " + F + "text", *older],
             older),
            ("...and for tab-marked ones",
             "0.9.7", ["# Changelog", *entry, "-\touter", "\t-\tnested", "\ttext",
                       "\t" + F + "text", *older],
             older),
            ("...three items deep as well",
             "0.9.7", ["# Changelog", *entry, "- a", "  - b", "    - c", "    text",
                       "    " + F + "text", *older],
             older),
            # ...while the fence is still INSIDE the nested item, an entry heading
            # at its indentation is data, and markers inside it do not outlive the
            # closer that ends it.
            ("a heading inside a nested item's fence is still data",
             "0.9.8", ["# Changelog", *entry, "- outer", "  - nested", "  " + F + "text",
                       "  ## [0.9.7] — 2026-09-05", "  " + F, *older],
             [*entry, "- outer", "  - nested", "  " + F + "text",
              "  ## [0.9.7] — 2026-09-05", "  " + F]),
            ("markers inside a fence do not outlive its closer",
             "0.9.8", ["# Changelog", *entry, "- outer", "  " + F + "text", "  - a",
                       "    - b", "  " + F, "  after", *older],
             [*entry, "- outer", "  " + F + "text", "  - a", "    - b", "  " + F,
              "  after"]),
            # ...AND THE PADDING RULE (5.2), which decides whether there is a fence
            # to end at all: four columns after the marker is content, a fifth is
            # an indented code block whose delimiter opens nothing.
            ("five columns of marker padding open no fence",
             "0.9.8", ["# Changelog", "-     " + F + "text", *entry, *older],
             entry),
            ("...for a multi-digit marker as well",
             "0.9.8", ["# Changelog", "10.     " + F + "text", *entry, *older],
             entry),
            # THE PADDING RULE IS OBSERVABLE AT A BOUNDARY, and this is the shape
            # that shows it. Read greedily, the marker takes all five columns and
            # a fence opens at the item's column 6; the `  ``` ` below it is a
            # dedent, so that fence ends and the delimiter OPENS A SECOND fence --
            # this one at top level, with no container to end it -- which then
            # masks to end of file and publishes nothing for any version. Read the
            # way CommonMark 5.2 says, the first line opens nothing (it is an
            # indented code block) and the `  ``` ` is the item's own fence,
            # which the entry heading below ends.
            ("five columns of padding, then a delimiter at the item's column",
             "0.9.8", ["# Changelog", "-     " + F + "text", "  " + F, *entry, *older],
             entry),
            ("four columns of padding still open one",
             "0.9.8", ["# Changelog", "-    " + F + "text",
                       "     ## [9.9.9] — 2999-01-01", "     " + F, *entry, *older],
             entry),

            ("a stray `## ` heading below an entry lands in that entry's notes",
             "0.9.8", ["# Changelog", *entry, "## Appendix", "- not part of 0.9.8", *older],
             [*entry, "## Appendix", "- not part of 0.9.8"]),
        ]
        for name, version, document, expected in extractor_cases:
            checked += 1
            with tempfile.TemporaryDirectory() as tmp:
                doc = Path(tmp) / "CHANGELOG.md"
                doc.write_text("\n".join(document) + "\n", encoding="utf-8")
                run = subprocess.run(
                    [awk, "-v", f"ver={version}", "-f", str(extractor), str(doc)],
                    capture_output=True, text=True, encoding="utf-8",
                )
            if run.returncode != 0:
                failures += 1
                print(f"self-test FAIL [extractor]: {name} -- awk exited "
                      f"{run.returncode}: {run.stderr.strip()}", file=sys.stderr)
                continue
            got = run.stdout.splitlines()
            if got != expected:
                failures += 1
                print(f"self-test FAIL [extractor]: {name}\n  expected: {expected}\n"
                      f"  got:      {got}", file=sys.stderr)

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
