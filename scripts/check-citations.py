#!/usr/bin/env python3
# ============================================================================
#  check-citations.py — keep `file:line` evidence citations pointing at the code
#  they were written about.
#
#  PROVENANCE: adopted from the sibling product Anabasis
#  (`scripts/check-citations.py`). The mechanism is unchanged; `TRACKED` is
#  re-derived against this repository's source layout and `DELIBERATE_REAIMS`
#  starts empty, because every entry in such a list names one document and one
#  line number in the tree it was written for.
#
#  WHY THIS EXISTS. The documents of record cite their evidence as
#  `src/PluginProcessor.cpp:695-752` — 184 such anchors across `docs/` when this
#  gate landed. A line anchor is exact and therefore fragile: any edit ABOVE it
#  silently re-aims it at unrelated code, and the document keeps reading as
#  though it were still correct. `DOCUMENTATION_LIFECYCLE_POLICY.md` already
#  requires anchors to be re-anchored in the SAME change set that moves them;
#  this makes that rule checkable instead of remembered. In the sibling, the rule
#  did not survive being remembered — 42 of 71 anchors were stale before anyone
#  noticed — and nothing about this repository makes it likelier to.
#
#  It is worth being precise about what this can and cannot do:
#
#    * It CAN tell you that a citation no longer points at the same TEXT it
#      pointed at in a base revision, and move it back onto that text. That
#      catches the whole class of "an edit above shifted it".
#    * It CANNOT tell you a citation was aimed at the wrong code to begin with.
#      Preserving content identity faithfully preserves a pre-existing mistake —
#      and it does so INVISIBLY, in a DRIFTED line that reads like a repair. The
#      anchors in this tree are therefore ADOPTED, not audited: a clean run does
#      not mean every citation is correct, it means none of them MOVED. Spelling
#      the SYMBOL beside the line number is what makes the other half checkable
#      by a reader, and is the convention to write new citations in.
#
#  WHAT IT WILL NOT TOUCH, and why that list is the important part. This tool
#  rewrites line numbers, so every citation it misclassifies as ours is a
#  citation it CORRUPTS — it replaces a correct anchor with a wrong one and
#  reports success. It has done exactly that once already (see `classify()`), so
#  the ownership test is now deliberately narrow: a citation is ours only when
#  it names its path from THIS repository's root, with no revision or checkout
#  qualifier in front of it, and that path is one of `TRACKED` verbatim.
#  Everything else — a bare file name, a sibling-product path, a `<rev>:`-pinned
#  anchor — is left alone. Under-checking costs coverage; misclassifying costs
#  the truth of the document, which is the thing being protected.
#
#  PROSE EXAMPLES MUST NOT USE A TRACKED PATH. This tool cannot tell an
#  illustration of a citation from a citation — `DOCUMENTATION_COVERAGE.md`'s
#  own worked example of a substitution bug was silently re-anchored, changing
#  the numbers the sentence depended on. Write examples against a path outside
#  `TRACKED` (`some/file.cpp:107`) and they are left alone.
#
#  Usage:  --check (the default) reports drift · --fix re-anchors it.
#  Exit codes follow the sibling scripts: 0 clean · 1 drift found (--check only)
#  · 2 the run could not reach a trustworthy answer (nothing to check against,
#  or --fix left anchors that need a human).
# ============================================================================

import argparse
import os
import re
import subprocess
import sys

# The sources whose line anchors are worth tracking, spelled EXACTLY as a
# citation must spell them to be checked. A file absent here is not checked; a
# path that differs from the spelling here — including the SIBLING product's
# `src/gui/PluginEditor.cpp` against our `src/PluginEditor.cpp`, which is the
# same trap in the other direction — is not ours.
#
# This is EVERY root-spelled source path the documents currently cite with a line
# anchor, and each one was confirmed to exist in the tree when the list was
# written. The sibling ships a deliberately shorter list because most of its
# citations land on a handful of files; here they are spread across the DSP
# stages, so a partial list would leave most of the surface unguarded — which for
# this gate means "silently unchecked", the one outcome worse than a red build.
#
# A file added to `src/` is NOT tracked until it is added here. That is the
# correct default (under-checking costs coverage; misclassifying costs the truth
# of the document) but it does mean the list needs a line when a new source file
# starts being cited.
#
# SCRIPTS ARE DELIBERATELY OUTSIDE THIS LIST, so their citations are NOT gated.
# Several documents cite `scripts/run-pluginval.sh:147-176` and
# `scripts/run-tests.sh:51-73`, and those anchors drift exactly the same way; they
# are excluded only to keep the gate's scope the same as the sibling's on the
# round that introduced it. Adding them is a one-line change and a real
# improvement -- do it in a round that is not itself rewriting those scripts,
# because a wholesale rewrite makes every one of their anchors report as drift and
# each re-anchor then needs a DELIBERATE_REAIMS entry to land.
TRACKED = (
    "src/InternalState.h",
    "src/PluginEditor.cpp",
    "src/PluginEditor.h",
    "src/PluginParameters.cpp",
    "src/PluginParameters.h",
    "src/PluginProcessor.cpp",
    "src/PluginProcessor.h",
    "src/PresetManager.cpp",
    "src/PresetManager.h",
    "src/dsp/AnamorphEngine.cpp",
    "src/dsp/AnamorphEngine.h",
    "src/dsp/ChorusEngine.cpp",
    "src/dsp/Correlation.h",
    "src/dsp/EngineParameters.h",
    "src/dsp/HaasProcessor.cpp",
    "src/dsp/LevelMeters.h",
    "src/dsp/LoudnessMatch.cpp",
    "src/dsp/MonoMaker.h",
    "src/dsp/MultibandWidth.cpp",
    "src/dsp/MultibandWidth.h",
    "src/dsp/ScopeBuffer.h",
    "src/dsp/SoloMonitor.cpp",
    "src/dsp/VelvetNoise.cpp",
    "src/gui/LookAndFeel.cpp",
    "src/gui/Vectorscope.h",
    "tests/state_tests.cpp",
    "tests/dsp_tests.cpp",
)

# A citation is `<path>:<line>`, `<path>:<start>-<end>`, or a COMPOUND list that
# names the path once and then several anchors: `<path>:708-709, 851, 1208`. The
# trailing group is what an early version of this file missed — it re-anchored
# the first anchor and left the bare numbers behind it untouched, which produced
# `:1040, 1039, 1053` in this repository: out of order and internally
# contradictory, from a tool whose whole job is keeping anchors true.
#
# Two parts of this pattern exist ONLY to stop the tool rewriting somebody
# else's line numbers, and both are load-bearing:
#
#   * The leading lookbehind refuses to start a match in the middle of a token.
#     Without it, `<checkout>:src/PluginProcessor.cpp:485-491` simply matched
#     from `src/…` — the qualifier never reached the ownership test — and the
#     anchor was rewritten by THIS tree's code movement onto lines of another
#     product that contain something else entirely. Rejecting the match is not
#     enough on its own either: the scan would just retry one character later
#     and match `rc/PluginProcessor.cpp`. The lookbehind blocks every one of
#     those restarts, because each is preceded by a path or word character.
#   * `<prefix>` therefore CAPTURES the qualifier — a checkout name, or a pinned
#     revision such as `7686204:` — so `classify()` can see it and decline.
#
# The path must contain a directory separator. A bare `PluginProcessor.cpp:7` is
# ambiguous across checkouts by construction — the architecture documents use a
# bare name as shorthand for "the file I have been quoting", which inside a
# paragraph about another product means that product's file — and no amount of
# context-reading fixes that here. Citations in this repository's own documents
# are spelled from the root, which is what makes them checkable.
CITATION = re.compile(
    r"(?<![\w./\\:-])"
    r"(?:(?P<prefix>[\w.@-]+):)?"
    r"(?P<path>[\w.-]+(?:[/\\][\w.-]+)+):"
    r"(?P<anchors>\d+(?:-\d+)?(?:\s*,\s*\d+(?:-\d+)?)*)")
ANCHOR = re.compile(r"(\d+)(?:-(\d+))?")

# Citations this repository RE-AIMED on purpose: the anchor was pointing at the
# wrong code and was moved onto the right code, which is indistinguishable from
# drift by the base-text test and would otherwise fail the gate forever.
# Declaring one here is a reviewable act — it appears in the diff, beside the
# document it exempts, and it is the only way to make this tool accept a
# re-aim. An entry whose citation no longer differs from the base has done its
# job (the base has caught up) and is reported as removable on the next run.
#
# Spelled `set([...])` rather than `{...}` on purpose: emptying a brace literal
# leaves a DICT, and the set difference below then raises `TypeError` instead of
# reporting a clean run. The list going empty is the expected end state of every
# entry here, so it must be the boring case.
DELIBERATE_REAIMS = set([
    # EMPTY, AND THAT IS THE EXPECTED STATE. The sibling's list was NOT carried
    # over: each of its entries excuses one specific `<document>, <path>:<line>`
    # pair in ITS tree, so importing them here would exempt anchors that do not
    # exist in this repository and would silently narrow the gate on the day one
    # of those spellings ever coincided.
    #
    # This repository's anchors are ADOPTED AS-IS rather than audited: the tool
    # detects MOVEMENT, not wrongness, so a citation that was aimed at the wrong
    # code before this gate existed stays wrong and stays green. That limit is
    # stated in the header and is not a reason to delay the gate -- it closes the
    # drift class, which is the one that grows on its own.
])


def is_declared_reaim(doc, whole_base, whole_cur):
    """Does a declaration excuse this citation's mismatch?

    TWO conditions, and the second is what stops the list becoming a set of
    permanent exemptions:

      1. Either spelling is declared. Which side of the change a declaration
         names should not decide whether it works — the two check paths had
         drifted apart on exactly that, one testing the current spelling and the
         other the base one, so a declaration written for one branch was inert in
         the other and nothing said so.
      2. THE SPELLING ACTUALLY CHANGED. A re-aim moves an anchor, so it always
         changes the spelling. If base and current read the same, this diff did
         not re-aim anything and a mismatch is ordinary drift — the exact case a
         spelling-keyed declaration would otherwise silence forever. Without this
         the entry survived its own transition: once the base carried the
         re-aimed spelling, `1766 == 1766` kept matching and the next commit that
         moved that code had its drift swallowed by a declaration written for
         something else.

    So a declaration is good for exactly one transition, and the run after it
    reports the entry as removable.
    """
    if whole_base == whole_cur:
        return False
    return any((doc, w) in DELIBERATE_REAIMS for w in (whole_base, whole_cur) if w)


def classify(prefix, path):
    """The tracked path this citation names, or None if it is not ours.

    A citation qualified by anything — another checkout, or a pinned revision
    such as `7686204:` — is NOT about this working tree's current line numbers,
    and rewriting it is destroying evidence rather than maintaining it. That is
    not a hypothetical: it happened, to 27 anchors across `DESIGN.md`, five ADRs
    and `OPEN_QUESTIONS.md`, including the ADR-0016 table whose heading says in
    as many words that it was read from the pre-change tree.

    Requiring the path to be `TRACKED` verbatim is what makes an explicit
    foreign-root test unnecessary: a path from another checkout does not match
    this repository's own layout, so it is declined for the same reason a typo
    would be — this tool only ever rewrites the nine files it knows.
    """
    if prefix:
        return None
    if path.startswith(("/", "\\")):
        return None
    norm = path.replace("\\", "/")
    return norm if norm in TRACKED else None


def git_show(ref, path):
    r = subprocess.run(["git", "show", f"{ref}:{path}"],
                       capture_output=True, text=True)
    return r.stdout.split("\n") if r.returncode == 0 else None


def read(path):
    with open(path, encoding="utf-8", errors="replace") as fh:
        return fh.read()


def line_of(lines, n):
    return lines[n - 1] if lines is not None and 1 <= n <= len(lines) else None


# A cross-product ATTRIBUTION line, which `ADR-0009` mandates in exactly this
# spelling at the head of every adapted file. It carries an anchor into the
# SIBLING product spelled with a path this repository also owns:
#
#     // Provenance (ADR-0009): adapted from <the sibling product> src/gui/LookAndFeel.cpp:1-912 @ <sha>.
#
# The ownership test cannot see the difference. `<prefix>` catches a `rev:`
# qualifier and the lookbehind catches a path glued to another token, but here
# the product name is a separate word followed by a space, so the citation
# classifies as OURS and a re-anchor would rewrite the sibling's line range using
# THIS tree's code movement — the exact corruption this file's header says is the
# one it must never commit. Harmless while only `docs/` was scanned, and live the
# moment source files joined the scan, which is why the two changes are one
# change.
PROVENANCE_MARKER = "Provenance (ADR-0009)"


def citations(text):
    """(whole matched string, tracked path, span, [(start, end|None), ...])"""
    out = []
    # Byte offsets of every provenance BLOCK, so a match can be tested against
    # the block it sits in without re-splitting the text per match.
    #
    # THE BLOCK, NOT THE LINE, and the difference is a live corruption hazard
    # rather than tidiness. This exclusion was line-scoped, which happens to
    # cover `LookAndFeel.h:1` and `LookAndFeel.cpp:1` — marker and anchor share
    # a line there. It does NOT cover `src/gui/PluginEditor.h`, where the marker
    # sits on one line and the sibling's anchor (`src/PluginEditor.h:36-175 /
    # .cpp:672-800`) wraps onto the line two below. That file is safe today only
    # by ACCIDENT of ownership: `src/PluginEditor.h` is the sibling's spelling
    # and is not in `TRACKED`, this tree's editor being `src/gui/PluginEditor.h`.
    # The day a provenance sentence wraps a path this repo also owns, a re-anchor
    # would rewrite the SIBLING's range using this tree's movement — the one
    # corruption this file's header says it must never commit, reached by a
    # line break.
    #
    # A block is the marker's line plus every following CONTINUATION line: one
    # whose body, after a leading `//`, `#`, `*` or `>` and whitespace, is not
    # empty. That ends the block at the `//` separator below the sentence in
    # `PluginEditor.h`, at a blank line in Markdown, and at the first line of
    # real content anywhere else — without this function needing to know which
    # comment syntax it is reading.
    provenance_spans = []
    lines = text.splitlines(keepends=True)
    offsets, pos = [], 0
    for line in lines:
        offsets.append(pos)
        pos += len(line)

    # A continuation must wear the SAME comment prefix as the marker line, and
    # that requirement is the fix for a boundary that was merely heuristic. The
    # first form stripped `//`, `#`, `*` and `>` interchangeably, so in
    # `src/gui/LookAndFeel.h` — marker on line 1, `#pragma once` on line 2 —
    # `#pragma once` stripped to `pragma once`, read as a continuation, and the
    # block silently swallowed a line of real code. Under-checking rather than
    # corruption, since the effect is to DECLINE citations, but a block that can
    # extend over arbitrary following content is not a boundary.
    def _prefix_of(line):
        s = line.lstrip()
        for p in ("//", "#", "*", ">"):
            if s.startswith(p):
                return p
        return ""                      # Markdown prose: no comment marker

    def _is_continuation(line, prefix):
        s = line.lstrip()
        if prefix == "":
            return s != ""             # a wrapped sentence; a blank line ends it
        if not s.startswith(prefix):
            return False
        return s[len(prefix):].strip() != ""

    i = 0
    while i < len(lines):
        if PROVENANCE_MARKER in lines[i]:
            prefix = _prefix_of(lines[i])
            j = i + 1
            while j < len(lines) and _is_continuation(lines[j], prefix):
                j += 1
            provenance_spans.append((offsets[i], offsets[j - 1] + len(lines[j - 1])))
            i = j
            continue
        i += 1

    for m in CITATION.finditer(text):
        tracked = classify(m.group("prefix"), m.group("path"))
        if tracked is None:
            continue
        s, _e = m.span()
        if any(a <= s < b for a, b in provenance_spans):
            continue        # the sibling product's anchor — never ours to move
        anchors = [(int(a.group(1)), int(a.group(2)) if a.group(2) else None)
                   for a in ANCHOR.finditer(m.group("anchors"))]
        out.append((m.group(0), tracked, m.span(), anchors))
    return out


def build_line_map(base, path):
    """old line -> new line, from the diff hunks. None inside an edited hunk."""
    diff = subprocess.run(["git", "diff", "-U0", base, "--", path],
                          capture_output=True, text=True).stdout
    edits = [(int(h.group(1)), int(h.group(2) or 1), int(h.group(4) or 1))
             for h in re.finditer(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@",
                                  diff, re.M)]

    def m(n):
        off = 0
        for start, old_count, new_count in edits:
            if old_count == 0:
                # A PURE INSERTION (`@@ -9,0 +10,2 @@`) sits AFTER old line
                # `start`; that line does not move. `start + old_count - 1` is
                # `start - 1` here, so the general test below would treat line
                # `start` as shifted — one line too far.
                if start < n:
                    off += new_count
                continue
            if start + old_count - 1 < n:
                off += new_count - old_count
            elif start <= n:
                return None
            else:
                break
        return n + off
    return m


# `worklogs/` is deliberately OUT of scope. Those files are research records
# about the SIBLING product, and they cite it both fully qualified (an absolute
# path into that checkout, `…/src/PluginEditor.cpp:797-800`) and, in the same
# bullet, by bare file name as shorthand for the same file. The ownership test
# above now declines both shapes anyway; the exclusion stays as the cheap,
# explicit statement of intent, so a future loosening of that test cannot reach
# these files by accident.
EXCLUDED_PREFIXES = ("worklogs/",)


def doc_files():
    """Every file whose citations are checked — Markdown AND the tracked SOURCE.

    THE SOURCE HALF WAS ADDED AFTER A COMMENT DRIFTED 24 LINES INSIDE THE ROUND
    THAT BUILT THIS TOOL. `closePresetUndoBracket`'s argument named `:1482` and
    `:1538` as the two sites that seed `presetBaseline`; a comment block inserted
    above them moved both by 24, and because this scan covered only `docs/` and
    the root Markdown, the gate could not see it. A tool that keeps documents
    honest about the code while the code lies about itself is checking the
    smaller half: these anchors are read by whoever is editing the function, at
    the moment they are deciding whether the argument still holds.

    The files in `TRACKED` are exactly the ones already treated as citable
    targets, so this makes the set self-checking rather than widening it. A
    source file citing ITSELF is fine: a re-anchor rewrites digits inside one
    line and never changes a line COUNT, so the numbering the citation is
    measured against survives its own repair.
    """
    # `splitlines`, not `split()`: a tracked path may legally contain a space,
    # and whitespace-splitting would shatter it into fragments that match
    # nothing — silently dropping the file from the scan rather than failing.
    # No such path exists here today, which is exactly why the bug would ship.
    tracked = subprocess.run(["git", "ls-files"],
                             capture_output=True, text=True).stdout.splitlines()
    out = []
    for p in tracked:
        if p.startswith(EXCLUDED_PREFIXES):
            continue
        if p in TRACKED:
            out.append(p)          # the source half — see the docstring
            continue
        if not p.endswith(".md"):
            continue
        if p.startswith("docs/") or "/" not in p:
            out.append(p)
    return out


def apply_edits(text, edits):
    """Substitute by SPAN, right to left.

    Never by string. `str.replace` matches a prefix: with `src/PluginProcessor.cpp:107`
    drifted to `:130` and `src/PluginProcessor.cpp:1076` still correct, replacing
    the first by text turns the second into `src/PluginProcessor.cpp:1306` —
    corrupting a citation that had nothing wrong with it. Spans come from the
    match that produced them, so each rewrite lands on exactly its own citation.

    DEDUPED AND CHECKED FOR OVERLAP, because a span is only safe once. The
    count-mismatch branch pairs a base citation against ALL current spans
    carrying its spelling, so a document that spells one citation twice queued
    that span-list once per base occurrence — and applying an identical
    `(start, end, new)` twice does not repeat a rewrite, it destroys one: the
    first pass changes the text's length, so `end` no longer bounds what it
    bounded and the second splices the replacement into the middle of itself
    (`…cpp:2000` became `…cpp:20000`). A set fixes the duplicate; the overlap
    check is there because any FUTURE way of generating two different rewrites
    for one region fails the same way, and this function must not corrupt a
    governed document in silence when that happens.
    """
    uniq = sorted(set(edits), key=lambda e: -e[0])
    for (a_start, a_end, _), (b_start, b_end, _) in zip(uniq, uniq[1:]):
        if b_end > a_start:
            raise ValueError(
                f"check-citations: refusing to rewrite overlapping spans "
                f"[{b_start},{b_end}) and [{a_start},{a_end}) — this would corrupt the "
                f"document rather than re-anchor it")
    for start, end, new in uniq:
        text = text[:start] + new + text[end:]
    return text


def main():
    ap = argparse.ArgumentParser(description="Verify documentation evidence anchors.")
    ap.add_argument("--base", default="origin/main",
                    help="revision the citations were last verified against")
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true",
                      help="report drifted citations (the default)")
    mode.add_argument("--fix", action="store_true",
                      help="re-anchor drifted citations instead of only reporting")
    args = ap.parse_args()

    base_src, now_src = {}, {}
    for path in TRACKED:
        if not os.path.exists(path):
            continue
        b = git_show(args.base, path)
        if b is None:
            print(f"check-citations: {path} does not exist at {args.base}; skipping",
                  file=sys.stderr)
            continue
        base_src[path] = b
        now_src[path] = read(path).split("\n")

    if not base_src:
        print(f"check-citations: nothing to check against {args.base}", file=sys.stderr)
        return 2

    maps = {p: build_line_map(args.base, p) for p in base_src}

    total = drifted = fixable = unmappable = unchecked = 0
    used_reaims = set()
    base_doc_cites = {}                # doc -> the citation spellings the BASE carries
    for doc in doc_files():
        base_text = subprocess.run(["git", "show", f"{args.base}:{doc}"],
                                   capture_output=True, text=True)
        if base_text.returncode != 0:
            continue                       # new document: nothing to drift from
        old_cites = citations(base_text.stdout)
        base_doc_cites[doc] = {c[0] for c in old_cites}
        text = read(doc)
        cur_cites = citations(text)

        # Paired PER PATH, in order of appearance. Neither global position nor
        # the citation string works on its own: a change set may add or remove
        # citations (position breaks), and once a citation has been re-anchored
        # its base spelling is gone from the file, so looking for that string
        # makes every LATER shift invisible — which is precisely how this round's
        # anchors went stale twice. Pairing the Nth `PluginProcessor.cpp`
        # reference in the base with the Nth in the current file survives both.
        by_path_old, by_path_cur = {}, {}
        for c in old_cites:
            by_path_old.setdefault(c[1], []).append(c)
        for c in cur_cites:
            by_path_cur.setdefault(c[1], []).append(c)

        edits = []
        for tracked, olds in by_path_old.items():
            if tracked not in base_src:
                continue
            curs = by_path_cur.get(tracked, [])
            if len(curs) != len(olds):
                # A change set is allowed to ADD or REMOVE citations, so a count
                # change is not itself a failure. Ordinal pairing is no longer
                # meaningful, though, so fall back to the conservative check:
                # every base citation whose spelling is still present must still
                # be correct. A citation that was re-spelled or removed is beyond
                # what this can judge, and a NEW one has nothing to drift from.
                still = {}
                for (whole_c, _t, span_c, _a) in curs:
                    still.setdefault(whole_c, []).append(span_c)
                # A spelling the BASE carries more than once. `still` is keyed by
                # spelling, so every base occurrence of the same citation resolves
                # to the same span list and the same rewrite — counting each of
                # them would report more citations drifted and fixed than the
                # document contains. `apply_edits` de-duplicates the spans, so
                # this was only ever a reporting defect; it is still the class of
                # imprecision this tool exists to remove from documents.
                counted = set()
                for (whole_o, _t, _span_o, anchors_o) in olds:
                    # `total` counts every base anchor on BOTH paths, and the ones
                    # this path cannot judge are counted again into `unchecked`
                    # and reported. Counting only the judged ones here made the
                    # closing "N of M" mean a different thing per document
                    # depending on which branch it took — the summary of a tool
                    # whose subject is documents saying what they mean.
                    total += len(anchors_o)
                    spans = still.get(whole_o)
                    if not spans:
                        unchecked += len(anchors_o)
                        continue
                    if all(line_of(base_src[tracked], a) == line_of(now_src[tracked], a)
                           and (b is None or line_of(base_src[tracked], b) == line_of(now_src[tracked], b))
                           for (a, b) in anchors_o):
                        continue
                    # Past this point the citation is DRIFTED and will be counted
                    # and queued. A second base occurrence of the same spelling
                    # reaches here with the same spans and the same rebuild, so it
                    # contributes nothing but inflated numbers — its anchors are
                    # already in `total` above, which is the figure that must stay
                    # per-occurrence.
                    if whole_o in counted:
                        continue
                    counted.add(whole_o)
                    # No DELIBERATE_REAIMS check here on purpose: this branch
                    # only reaches citations whose spelling is UNCHANGED (that is
                    # the `still.get(whole_o)` filter above), and a re-aim always
                    # changes the spelling. Anything mismatching here is drift.
                    mapped, movable = [], True
                    for (a, b) in anchors_o:
                        na, nb = maps[tracked](a), (maps[tracked](b) if b else None)
                        if na is None or (b is not None and nb is None):
                            movable = False
                            break
                        mapped.append((na, nb))
                    drifted += 1
                    if not movable:
                        unmappable += 1
                        print(f"  UNMAPPABLE {doc}: {whole_o} "
                              f"(the cited lines were themselves edited — re-aim by hand)")
                        continue
                    rebuilt = f"{tracked}:" + ", ".join(
                        f"{na}-{nb}" if nb else f"{na}" for na, nb in mapped)
                    # One citation, however many times the document spells it.
                    # Identical spellings carry identical anchors, so the same
                    # rewrite is right for each occurrence — but they are ONE
                    # drifted citation, and counting the spans inflated `fixed`
                    # past the number of citations there were to fix.
                    if len(spans) > 1:
                        print(f"    (spelled {len(spans)}× in this document; all move together)")
                    print(f"  DRIFTED {doc}: {whole_o} -> {rebuilt}")
                    edits += [(s, e, rebuilt) for (s, e) in spans]
                    fixable += 1
                print(f"check-citations: note — {doc} now has {len(curs)} `{tracked}` "
                      f"citation(s) where {args.base} had {len(olds)}; the added ones are "
                      f"not checkable against that base.")
                continue

            for (whole_o, _to, _so, anchors_o), (whole_c, _tc, span_c, anchors_c) in zip(olds, curs):
                total += len(anchors_o)
                if len(anchors_o) != len(anchors_c):
                    # COUNTS AS UNMAPPABLE, not merely drifted, and the difference
                    # is the exit code. `--check` returns 1 on any drift; `--fix`
                    # returns `2 if unmappable else 0`. This branch declines to
                    # touch the citation — a `:10` that became `:10-20` is a
                    # judgement about what the prose means, not a line shift — so
                    # counting it as drift alone made `--fix` exit 0 while
                    # promising a clean repair, and the very next `--check` (CI's)
                    # went red on what it left behind. The documented workflow is
                    # "run `--fix` in the SAME change set", so `--fix` saying 0 is
                    # the last word a contributor hears before pushing.
                    print(f"  ANCHOR COUNT {doc}: {whole_o} -> {whole_c}; review by hand")
                    drifted += 1
                    unmappable += 1
                    continue

                # Correct means: the text at the CURRENT anchors is the text the
                # BASE anchors named.
                same = all(line_of(base_src[tracked], a) == line_of(now_src[tracked], a2)
                           and (b is None) == (b2 is None)
                           and (b is None or line_of(base_src[tracked], b) == line_of(now_src[tracked], b2))
                           for (a, b), (a2, b2) in zip(anchors_o, anchors_c))
                if same:
                    continue

                if is_declared_reaim(doc, whole_o, whole_c):
                    # BOTH spellings, because the declaration may have been
                    # written with either (that is what `is_declared_reaim`
                    # accepts). Recording only the current one left an entry
                    # declared with the BASE spelling honoured here and then
                    # ALSO listed in `DELIBERATE_REAIMS - used_reaims`, which
                    # prints "was not needed … delete it" — advice that, taken,
                    # re-breaks the gate. Latent today (every entry happens to be
                    # a current spelling) and cheaper to close than to remember.
                    used_reaims.add((doc, whole_c))
                    used_reaims.add((doc, whole_o))
                    continue

                mapped, movable = [], True
                for (a, b) in anchors_o:
                    na, nb = maps[tracked](a), (maps[tracked](b) if b else None)
                    if na is None or (b is not None and nb is None):
                        movable = False
                        break
                    mapped.append((na, nb))

                drifted += 1
                if not movable:
                    unmappable += 1
                    print(f"  UNMAPPABLE {doc}: {whole_c} "
                          f"(the cited lines were themselves edited — re-aim by hand)")
                    continue
                rebuilt = f"{tracked}:" + ", ".join(
                    f"{na}-{nb}" if nb else f"{na}" for na, nb in mapped)
                print(f"  DRIFTED {doc}: {whole_c} -> {rebuilt}")
                edits.append((span_c[0], span_c[1], rebuilt))
                fixable += 1

        if args.fix and edits:
            with open(doc, "w", encoding="utf-8") as fh:
                fh.write(apply_edits(text, edits))
            # `now_src` IS THE WORKING TREE, and it stopped being so the moment a
            # TRACKED SOURCE joined the scanned set. The snapshot is taken once,
            # before this loop; a `--fix` that rewrites `src/PluginProcessor.cpp`
            # leaves the cached copy holding pre-rewrite lines, and any document
            # processed LATER that cites one of the rewritten lines is then judged
            # against text no longer on disk. Narrow — it needs a citation whose
            # own line is a citation — but it is an invariant, and an invariant
            # this tool holds about a file it just edited is exactly the kind it
            # cannot afford to be casually wrong about.
            if doc in now_src:
                now_src[doc] = read(doc).split("\n")

                # …AND THE REFRESH IS ONLY HALF OF IT. Keeping the cache in step
                # with disk fixes what this run judges NEXT; it does nothing about
                # what the rewrite DID. Re-anchoring a citation that lives inside a
                # tracked source line changes that line's TEXT, and every anchor
                # aimed at that line is verified by text identity — so a document
                # elsewhere pointing there now compares the base's old wording
                # against the new one and reports drift, or gets re-aimed away from
                # a line that never moved. The repair is indistinguishable from the
                # damage, which is this tool's recurring shape.
                #
                # Not silently, then. The lines are named, because a human can
                # settle in seconds what the tool cannot settle at all: whether
                # anything aims at them. Narrow by construction — it needs an
                # anchor whose target line is itself a citation — but that is
                # exactly the shape of the comment in `closePresetUndoBracket`
                # that motivated scanning source in the first place.
                touched = sorted({text.count("\n", 0, a) + 1 for a, _b, _r in edits})
                print(f"check-citations: NOTE — {doc} is a tracked SOURCE file and "
                      f"line(s) {', '.join(str(n) for n in touched)} were rewritten. "
                      f"Any citation elsewhere aimed at those lines is now measured "
                      f"against changed text; re-check them by hand.")

    # A declared re-aim is never silent: it is announced when it is honoured, and
    # announced again when it has stopped being needed, so the list cannot quietly
    # become a set of permanent exemptions.
    # INTERSECTED, because `used_reaims` deliberately holds BOTH spellings of an
    # honoured re-aim (see the `is_declared_reaim` branch) and only one of them is
    # a declaration. Reporting the set raw announced the UNDECLARED spelling too,
    # so a run printed twice as many accepted re-aims as `DELIBERATE_REAIMS` has
    # entries — a tool whose subject is documents saying exactly what they mean,
    # not saying exactly what it means. The intersection is never empty for an
    # honoured re-aim: `is_declared_reaim` returns true only when at least one of
    # the two spellings is literally in the set, so the "never silent" property
    # below survives the narrowing.
    for (doc, whole) in sorted(used_reaims & DELIBERATE_REAIMS):
        print(f"check-citations: ACCEPTED re-aim {doc}: {whole} "
              f"(declared in DELIBERATE_REAIMS — verify the aim by hand, not by this tool)")
    # "DELETE IT" IS ADVICE, so it is only given when it is SAFE, and the two
    # cases below were one message until 2026-08-14. An entry is unused against a
    # given base for two entirely different reasons:
    #
    #   * The base already CARRIES the re-aimed spelling — the transition this
    #     entry excused is behind that base, and `is_declared_reaim`'s "the
    #     spelling actually changed" test can never fire for it again from there.
    #   * The base is simply not the one that needed it, and does not carry the
    #     spelling either. It is a declaration for a DIFFERENT base.
    #
    # Neither is an instruction, and the old single message ("delete it once the
    # base carries the re-aim") read as one on both. CI compares against the
    # previous PUSH (`github.event.before`); on that base ~20 of 23 entries have
    # nothing to excuse, because they were written for the merge base. Acting on
    # that log re-breaks the gate against `origin/main` — the tool destroying what
    # it protects, in a green run. Even the first case is only safe when the base
    # in hand is the branch's MERGE BASE: an entry retired against the previous
    # push is very often still live against `main`, which is exactly how this
    # round's `PluginEditor.h:604` declaration was still doing real work.
    for (doc, whole) in sorted(DELIBERATE_REAIMS - used_reaims):
        if whole in base_doc_cites.get(doc, set()):
            print(f"check-citations: note — DELIBERATE_REAIMS entry {doc}: {whole} was not "
                  f"needed against {args.base}, which already carries the re-aimed "
                  f"spelling. Safe to delete ONLY if that base is the branch's merge "
                  f"base; re-run against that before removing it.")
        else:
            print(f"check-citations: note — DELIBERATE_REAIMS entry {doc}: {whole} was not "
                  f"exercised against {args.base}, which does not carry the re-aimed "
                  f"spelling either — the entry belongs to a different base. Keep it.")

    # Every number below counts base ANCHORS, and `unchecked` is stated rather
    # than netted off, because the difference between "17 verified" and "17
    # verified, 4 beyond what this could judge" is the whole value of the run.
    checked = total - unchecked
    tail = f" ({unchecked} re-spelled or removed, beyond what this run can judge)" if unchecked else ""

    if args.fix:
        print(f"\ncheck-citations: re-anchored {fixable} citation(s) across {checked} "
              f"checked anchor(s){tail}; {unmappable} need a human.")
        return 2 if unmappable else 0

    if drifted:
        print(f"\ncheck-citations: {drifted} citation(s) across {checked} checked "
              f"anchor(s){tail} no longer point at the text they did at {args.base}. "
              f"Re-anchor them in THIS change set (scripts/check-citations.py --fix), "
              f"then re-read the ones marked UNMAPPABLE.", file=sys.stderr)
        return 1

    print(f"check-citations: {checked} anchor(s) still point at the same text as "
          f"{args.base}{tail}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
