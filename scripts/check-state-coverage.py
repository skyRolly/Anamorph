#!/usr/bin/env python3
"""
Anamorph -- static coverage lint: every hand-maintained state list answers for every member.

WHAT IT IS FOR, and the defect class it exists for
==================================================
This repository's dominant recurring defect is not a wrong algorithm.  It is a CORRECTNESS
INVARIANT HELD BY A HAND-MAINTAINED LIST THAT NOTHING COMPARES TO ITS SUBJECT -- a function that
spells out fields or modules one by one, sitting next to a struct or a class that grows without it.
Both of this lint's targets have produced measured defects:

  * `EngineParameters` and its FOUR lists.  `sameParameters`, `discreteDiffers`,
    `processingDiffers` and `copyContinuous` each enumerate fields of one 36-field struct, for four
    different questions.  Round 4 found real defects in two of them at once: `dimMode` was ducking
    when no module could observe the change, and `pendingAlgoReset` was set on one of two paths
    into the variable it describes.  `sameParameters`' own comment states the invariant and then
    relies on the reader: *"this list must name EVERY EngineParameters field -- a field added there
    and forgotten here would have its edits ignored whenever nothing else moves."*

  * `AnamorphEngine::reset` and its MODULES.  Round 5 gave the function a `ResetScope` parameter,
    which turned one list into two answers per module -- and nobody made the second answer for any
    of them.  Round 6 measured two wrong ones in the same function and in opposite directions:
    `loudness` was not reset at all on a host reset (pre-reset energy kept the silence gate shut
    and the published gain drifted, against ADR-0007), and `levels` WAS reset (wiping `peakHoldL/R`,
    a latch LevelMeters.h documents as the user's and THREAD_MODEL.md assigns to the GUI).  Round 9
    measured the cost of Round 6's answer for the display: `correlation` and the LIVE half of
    `levels` were then not reset either, and a host that resets and stops calling `processBlock`
    froze both at the last active frame (State test 122).

The shape is the same both times: a list whose SUBJECT is enumerable, whose answers are judgement
calls, and whose omissions are invisible.  This lint does not make the judgements.  It requires
that one exists for every member, and holds the code to the one that was declared.

WHAT IT CLAIMS, precisely, and what it does not
===============================================
It claims exactly two things and no more:

  1. Every member of the subject appears in the declaration table below with an answer.
  2. The code agrees with that answer -- the field really is compared in the required FORM, the
     module really is reset under the declared SCOPE.

It does NOT claim the answers are right.  A declaration that says `bypass` is deliberately absent
from `discreteDiffers` is true whether or not that is good DSP; what the lint guarantees is that
the absence is a DECISION with a written reason rather than an omission, and that the code has not
since drifted away from it.  The judgement stays where it belongs: with the reviewer, the ADR, and
the tests.

There is a THIRD claim, added in round 8 and deliberately no larger: where the two SELECTION lists
both name a field, they must attach the SAME condition to it.  `discreteDiffers` and
`processingDiffers` ask different questions ("must this be swapped at silence?" and "did the signal
path change?"), but a field's CONDITION in both is the same test -- does the value reach a module --
so a difference between them is a decision rather than a detail.  This is the narrowest rule that
would have caught a measured defect: R4 gave `dimMode` a Dimension-D relevance guard in
`discreteDiffers` and left `processingDiffers` comparing it unconditionally, and the lint stayed
green for four rounds because "the field is named" was true either way.  An inert Dim-D Style move
then threw away a converged Level-Match reading through a forced duck (Test 58).
  * It still does not claim the guard is CORRECT.  Rewriting `Algorithm::DimensionD` to the wrong
    enumerator keeps both lists in agreement and this check silent; Test 58 is what catches that,
    and the four "must still re-arm" legs in it exist for exactly that reason.
  * A divergence CAN be legitimate, since the questions differ, so it is declared in
    `GUARD_DIVERGENCE` with a reason rather than forbidden.  That table is empty today.

WHY A TEXT SCAN IS THE RIGHT TOOL FOR TARGET 1, which is not true of every lint
==============================================================================
All four functions are single-expression enumerations with a FIXED shape -- a `&&` chain of
`a.X == b.X` / `sameF (a.X, b.X)`, a `||` chain of `a.X != b.X`, or a save/restore pair around one
`dst = src`.  The lint does not look for a field NAME anywhere in the body (that would be a weaker
assumption than the invariant, and a name in a comment would satisfy it); it looks for the exact
COMPARISON, with both operands pinned.  `a.mix == b.width` does not satisfy the requirement for
`mix`, because `b.mix` is not there -- and it does not satisfy `width` either.  Comments and string
literals are stripped first, with the same apostrophe predicate `check-dispatch.py` and
`check-realtime.py` carry, so prose naming a field cannot stand in for comparing it.

WHAT TARGET 2 CAN AND CANNOT SEE, stated because the gap is real
================================================================
Target 2's subject is the engine's DSP MODULES -- members whose own type declares a `reset()`.
That set is derived, not declared: the lint reads `AnamorphEngine`'s member declarations and keeps
the ones whose type's header carries `void reset`.  `ScopeBuffer` has none, so it is not in the set
and needs no exemption; it is out by construction rather than by opinion.

It CANNOT see SCALAR state.  `dryDelayWrite`, `prevInputSilent`, `pendingForced`, `switchPhase` and
the rest are cleared by ASSIGNMENT, not by a call, and there is no member set to derive them from
that would not also sweep in `sr`, `maxBlock` and three latency atomics.  That gap is real and is
named here rather than papered over: ER-DSP-07 (`pendingForced` left latched through a re-prepare)
was exactly such a scalar, and it is covered by tests, not by this lint.  Widening the subject to
every data member would turn a 12-row table into a 50-row one whose rows are mostly `never`, and a
table nobody reads is a table nobody maintains.

It sees WHETHER a module is reset under a scope, not WHICH reset.  `reset()` and `softReset()` both
count, so swapping them between the two scopes still reads `both` and passes.  Measured: that
swap's host-reset half fails State tests 118, 120 and 121.  Its re-prepare half ALONE fails nothing,
and correctly -- it changes no behaviour.  `reset (everything)` has one caller, `prepare()`, which
has already zeroed the matcher through `loudness.prepare()` -> `LoudnessMatch::reset()`; the two
flushes are redundant.  Removing BOTH is what would let a re-prepare keep the published gain, and
State test 120 (leg 2) fails on that.  So no behaviour hides behind this blind spot today; telling
the methods apart would mean declaring one per module per scope, and nothing measured asks for it.

A PARTIAL reset is deliberately NOT counted.  `levels.resetLive()` clears the live display and
keeps the user's latches; counting it as a reset would make the Round-6 defect -- a full
`levels.reset()` on the host-reset path -- read `both` against a `both` declaration and pass.  So
`levels` is declared `everything`, a full reset on the host path still fails, and the partial reset
the host path does take is pinned by State test 122 instead.

WHY THE TABLE IS A LIST TOO, and why that is still a strict improvement
=======================================================================
The obvious objection: replacing four hand-maintained lists with a declaration table swaps four
lists for a fifth.  Three things make it an improvement rather than a shuffle.
  * There is ONE table, not four, and it is CROSS-CHECKED against the code on every run -- the four
    lists were cross-checked against nothing.
  * `DISCRETE` is verified against `copyContinuous` EXACTLY, in both directions, so the table
    cannot drift from the code it describes without failing.
  * It is TOTAL over the struct: a field added to `EngineParameters` and not classified fails the
    build.  That is the case every measured defect in this class came through.

SELF-TEST (`--self-test`), per TESTING_POLICY rule 4
====================================================
Runs the real parsers and the real checks over synthetic sources in BOTH directions: each "must
fire" case is a defect this lint exists to catch, re-created field by field and scope by scope
(including the two Round-6 reset defects in their original form), and each "must stay quiet" case
is valid code an over-eager revision would flag.  The tree scan itself is then run for real, so a
parser that reaches no members fails here rather than reporting the tree clean.
"""

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"

PARAMS_H = "src/dsp/EngineParameters.h"
ENGINE_H = "src/dsp/AnamorphEngine.h"
ENGINE_CPP = "src/dsp/AnamorphEngine.cpp"

# ---------------------------------------------------------------------------------------------
#  TARGET 1 -- the declaration table for `EngineParameters`.
# ---------------------------------------------------------------------------------------------
# A field is DISCRETE when a change to it is adopted at the bottom of the switch duck rather than
# ramped by a smoother.  That single classification answers three of the four lists: `copyContinuous`
# preserves exactly this set, and `discreteDiffers` / `processingDiffers` may name only fields from
# it.  `sameParameters` is total over the struct and takes no classification at all.
#
# This set is verified against `copyContinuous` in BOTH directions on every run, so it cannot
# silently drift from the code.  A field added to the struct and not listed here (or explicitly
# left out of it, which is what "continuous" means) fails the lint.
DISCRETE = {
    "channelMode", "monoSum", "swapLR", "msMode", "solo", "algorithm", "haasSide", "dimMode",
    "mbEnable", "mbBands", "monoMakerEnable", "oversample", "bypass", "autoGainMatch",
}

# Fields whose relevance CONDITION is deliberately different between `discreteDiffers` and
# `processingDiffers`, with the reason. Empty today, and that is the finding: `dimMode` carries
# the same Dimension-D guard in both, because both are asking the same thing about it -- does
# the value reach a module. A future field whose two answers really must differ goes here.
GUARD_DIVERGENCE = {}

# DISCRETE fields deliberately NOT named in `discreteDiffers`, with the reason the source gives.
# Being discrete is not the same as needing the duck: a change with its own click-free crossfade
# does not.  The reason is what a reviewer checks; the lint checks only that one exists and that
# the field really is absent.
DISCRETE_DIFFERS_EXCLUDED = {
    "mbEnable": "click-free OUTPUT crossfade (mbEnableBlend) with the bank kept warm, not a duck",
    "bypass": "click-free OUTPUT crossfade (bypassBlend); the chain and analysis run regardless",
}

# DISCRETE fields deliberately NOT named in `processingDiffers`, which asks a narrower question:
# "did the SIGNAL PATH change", the trigger for re-arming the Level-Match measurement.
PROCESSING_DIFFERS_EXCLUDED = {
    "bypass": "the chain runs under bypass too, so the measured path is unchanged (Issues 2/3)",
    "autoGainMatch": "engaging the match changes what is APPLIED, not the path being measured",
}

# ---------------------------------------------------------------------------------------------
#  TARGET 2 -- the declaration table for `AnamorphEngine::reset (ResetScope)`.
# ---------------------------------------------------------------------------------------------
# One answer per DSP module, for the two scopes `AnamorphEngine.h` declares:
#   both       -- cleared on a host reset AND on a re-prepare (the ordinary answer for audio state)
#   everything -- cleared on a re-prepare ONLY; a host reset must leave it alone
#   tails      -- cleared on a host reset ONLY (no module needs this today; the vocabulary carries
#                 it so the checker's classification is complete rather than accidentally total)
#   never      -- not reached from reset() at all
RESET_SCOPE = {
    "haas": ("both", "delay line: audio, and a host reset is a request to stop tails"),
    "velvet": ("both", "decorrelation state: audio"),
    "chorus": ("both", "modulated delay: audio"),
    "multiband": ("both", "crossover bank: audio"),
    "monoMaker": ("both", "crossover: audio"),
    "soloMonitor": ("both", "audition filter bank: audio"),
    "loudness": ("both", "K-weighting filters and energy integrators are audio that has stopped; "
                         "the host-reset path takes softReset() so the PUBLISHED gain survives "
                         "(ADR-0007), which is why this row is `both` and not `everything`"),
    "correlation": ("both", "display-only running averages with NO user latch. R6 declared this "
                            "`everything` on the premise that they decay on silence by themselves; "
                            "they decay only inside process(), so a host that resets and stops "
                            "calling processBlock froze both pointers. reset() now publishes"),
    # `everything` because that is where the WHOLE meter is reset. A host reset calls
    # `levels.resetLive()` -- the live display only -- which this table deliberately does NOT
    # count as a reset (see `resets_in`): if it did, the Round-6 defect (a full
    # `levels.reset()` on the host-reset path, wiping the user's held peak) would read `both`,
    # match, and go uncaught. The partial reset is pinned by State test 122 instead.
    "levels": ("everything", "the WHOLE meter includes peakHoldL/R and the clip latches, the "
                             "USER'S -- LevelMeters.h: \"never falls\"; cleared by the GUI click "
                             "or a playback restart. A host reset takes resetLive() instead"),
    "os2": ("both", "oversampler delay: audio"),
    "os4": ("both", "oversampler delay: audio"),
    "os8": ("both", "oversampler delay: audio"),
}

# The SUBJECT of target 2 is derived, not declared: a member is in it when its type is defined in
# `src/dsp/` and that type declares its own `reset()`.  `ScopeBuffer` has none, so it is out by
# construction rather than by opinion.
#
# Types defined ELSEWHERE cannot be classified that way, so each is classified ONCE, BY TYPE, and
# the lint fails on a type it has never been told about.  That is what keeps the derivation total:
# a new `juce::dsp::` module joining the engine stops the build until somebody says what a host
# reset should do with it, instead of being skipped for having no header in `src/dsp/`.
#
# `RESETTABLE` means "members of this type need a RESET_SCOPE row"; anything else is the reason the
# type carries no reset state this lint can see, and its members are out of the subject.
RESETTABLE = "RESETTABLE"
EXTERNAL_TYPES = {
    "double": "scalar: cleared by assignment, which this lint does not read (see the docstring)",
    "int": "scalar: cleared by assignment",
    "bool": "scalar: cleared by assignment",
    "float": "scalar: cleared by assignment",
    "EngineParameters": "the parameter snapshot, flushed by assignment in reset()",
    "SwitchState": "scalar enum: cleared by assignment",
    "std::atomic": "a cross-thread mailbox, not chain state; assignment if anything",
    "juce::AudioBuffer": "cleared by .clear(), not by a reset() call",
    "juce::SmoothedValue": "settled by setCurrentAndTargetValue()/snapSmoothers(), not reset()",
    "std::unique_ptr": RESETTABLE,   # the oversamplers; the pointee carries juce::dsp reset()
}

SCOPE_GUARD = "ResetScope::everything"


# =============================================================================================
#  Stripping.  Same predicate as check-dispatch.py / check-realtime.py: the three lints strip the
#  same language, so they must agree about what an apostrophe is.
# =============================================================================================
def _closes_on_this_line(text, i):
    quote = text[i]
    k, n = i + 1, len(text)
    while k < n and text[k] != "\n":
        if text[k] == "\\" and k + 1 < n:
            k += 2
            continue
        if text[k] == quote:
            return True
        k += 1
    return False


def _is_digit_separator(text, i):
    """Is `text[i]` (an apostrophe) a C++14 digit separator rather than a quote?

    A separator sits between two digits of ONE numeric token, and a numeric token STARTS with a
    digit (or a leading `.`).  Walking LEFT to the start of the token is what separates it from an
    ENCODED character literal (`u8'a'`), which also has an alphanumeric on both sides of its
    OPENING quote.  Read wrongly, an apostrophe deletes everything up to the next one anywhere in
    the file -- and every comparison in between stops being visible, which is this lint failing
    OPEN at the one job it has.
    """
    if i == 0 or i + 1 >= len(text):
        return False
    if not text[i + 1].isalnum():
        return False
    j = i - 1
    while j >= 0 and (text[j].isalnum() or text[j] in "'."):
        j -= 1
    start = j + 1
    if start >= i:
        return False
    if text[start].isdigit():
        return True
    return text[start] == "." and start + 1 < i and text[start + 1].isdigit()


def strip_comments_and_strings(text):
    """Remove //, /* */ and "..." / '...'; newlines are preserved so line numbers stay real."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if c == "/" and nxt == "/":
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif c == "/" and nxt == "*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("\n" * text.count("\n", i, j))
            i = j
        elif c == "'" and (_is_digit_separator(text, i) or not _closes_on_this_line(text, i)):
            out.append(c)
            i += 1
        elif c in "\"'":
            quote, j = c, i + 1
            while j < n:
                if text[j] == "\\":
                    j += 2
                    continue
                if text[j] == quote:
                    j += 1
                    break
                j += 1
            out.append("\n" * text.count("\n", i, j))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


# =============================================================================================
#  Parsing.
# =============================================================================================
# A data member declaration, with either initialiser form. BOTH forms are required: `latency2 { 0 }`
# is brace-initialised, and a regex that only knew `=` would skip it -- a member this lint never saw
# is exactly the silence it exists to prevent.
_MEMBER_DECL = re.compile(
    r"^\s*(?:mutable\s+|static\s+)*"
    r"(?P<type>[A-Za-z_][\w:]*(?:\s*<[^;]*>)?)\s+"
    r"(?P<names>[A-Za-z_]\w*\s*(?:=[^,;]*|\{[^;]*?\})?"
    r"(?:\s*,\s*[A-Za-z_]\w*\s*(?:=[^,;]*|\{[^;]*?\})?)*)\s*;\s*$"
)

_NOT_A_TYPE = {"return", "const", "using", "friend", "typedef", "enum", "struct", "class",
               "namespace", "public", "private", "protected", "template", "else", "case"}


_NESTED_TYPE_KEYWORD = re.compile(r"^\s*(?:enum|struct|class|union|namespace)\b")


def _is_initialiser_brace(buf):
    """At class scope, does this `{` open a member INITIALISER rather than a nested scope?

    Two shapes reach a `{` at depth 1: an inline function body and a brace initialiser.  A function
    declaration always carries its parameter list, so a pending buffer with a `(` in it is a body;
    one without is `Type name { ... }`.  A nested `enum`/`struct`/`class` is excluded by its own
    keyword, which is what a bare `(`-test would get wrong.
    """
    if "(" in buf:
        return False
    if _NESTED_TYPE_KEYWORD.match(buf):
        return False
    return buf.strip() != ""


def _matching_brace(text, i):
    """Index just past the `}` matching the `{` at `text[i]`."""
    depth, k, n = 0, i, len(text)
    while k < n:
        if text[k] == "{":
            depth += 1
        elif text[k] == "}":
            depth -= 1
            if depth == 0:
                return k + 1
        k += 1
    return n


def parse_members(text, opener):
    """[(line, type, name)] for the data members declared directly in `opener`'s body.

    `opener` is the text that introduces the block -- `struct EngineParameters` or
    `class AnamorphEngine`.  Only depth-1 declarations are returned, so members of a nested
    `enum class` or helper struct are not mistaken for the subject's own.
    """
    stripped = strip_comments_and_strings(text)
    i = stripped.find(opener)
    if i < 0:
        return []
    j = stripped.find("{", i)
    if j < 0:
        return []
    members, depth, k, n = [], 0, j, len(stripped)
    line_start = stripped.count("\n", 0, j)
    buf, buf_line = "", line_start
    while k < n:
        ch = stripped[k]
        if ch == "{" and depth == 1 and _is_initialiser_brace(buf):
            # NOT A SCOPE. `std::atomic<int> latency2 { 0 }, latency4 { 0 };` is one declaration of
            # three members; counted as a scope it would raise the depth, discard the half-read
            # declaration and leave all three INVISIBLE to this lint -- a member it never saw, which
            # is the exact silence the lint exists to prevent. Copied into the buffer instead, so
            # the declaration reaches _MEMBER_DECL whole.
            end = _matching_brace(stripped, k)
            buf += stripped[k:end]
            k = end
            continue
        if ch == "{":
            depth += 1
            buf, buf_line = "", stripped.count("\n", 0, k) + 1
        elif ch == "}":
            depth -= 1
            buf, buf_line = "", stripped.count("\n", 0, k) + 1
            if depth == 0:
                break
        elif ch == ";" and depth == 1:
            m = _MEMBER_DECL.match(buf.strip() + ";")
            if m and m.group("type").split("<")[0].strip() not in _NOT_A_TYPE:
                ty = m.group("type").strip()
                for nm in re.split(r",(?![^{]*\})", m.group("names")):
                    nm = re.split(r"[={]", nm)[0].strip()
                    if nm:
                        members.append((buf_line + 1, ty, nm))
            buf, buf_line = "", stripped.count("\n", 0, k) + 1
        elif ch == ":" and depth == 1 and buf.strip() in ("public", "private", "protected"):
            # An ACCESS SPECIFIER, not part of a declaration. Left in the buffer it prefixes the
            # next member, whose type then reads as `private` and whose declaration is discarded --
            # the first member after every specifier would be invisible.
            buf, buf_line = "", stripped.count("\n", 0, k) + 1
        elif ch == "\n" and not buf.strip():
            buf, buf_line = "", stripped.count("\n", 0, k) + 1
        else:
            buf += ch
        k += 1
    return members


def function_body(text, qualified_name):
    """The stripped body of `qualified_name`, braces included, or None."""
    stripped = strip_comments_and_strings(text)
    i = stripped.find(qualified_name)
    if i < 0:
        return None
    j = stripped.find("{", i)
    if j < 0:
        return None
    depth, k, n = 0, j, len(stripped)
    while k < n:
        if stripped[k] == "{":
            depth += 1
        elif stripped[k] == "}":
            depth -= 1
            if depth == 0:
                return stripped[j:k + 1]
        k += 1
    return None


def split_by_reset_scope(body):
    """(unconditional, everything_only, tails_only) slices of a `reset (ResetScope)` body.

    An `if (... ResetScope::everything ...)` then-branch is `everything`, its `else` branch is
    `tails`, and anything outside such an `if` is unconditional.  A brace-less branch is one
    statement, which is the form the two-line `loudness` case uses.
    """
    uncond, ever, tails = [], [], []
    i, n = 0, len(body)
    while i < n:
        m = re.compile(r"\bif\s*\(([^()]*)\)").search(body, i)
        if m is None or SCOPE_GUARD not in m.group(1):
            if m is None:
                uncond.append(body[i:])
                break
            uncond.append(body[i:m.end()])
            i = m.end()
            continue
        uncond.append(body[i:m.start()])
        then, i = _one_branch(body, m.end())
        ever.append(then)
        rest = body[i:]
        me = re.match(r"\s*else\b", rest)
        if me:
            other, i = _one_branch(body, i + me.end())
            tails.append(other)
    return "".join(uncond), "".join(ever), "".join(tails)


def _one_branch(body, i):
    """(branch text, index after it) for the statement or block starting at `i`."""
    n = len(body)
    while i < n and body[i].isspace():
        i += 1
    if i < n and body[i] == "{":
        depth, k = 0, i
        while k < n:
            if body[k] == "{":
                depth += 1
            elif body[k] == "}":
                depth -= 1
                if depth == 0:
                    return body[i:k + 1], k + 1
            k += 1
        return body[i:], n
    j = body.find(";", i)
    j = n if j < 0 else j + 1
    return body[i:j], j


def resets_in(text, member):
    """Does `text` call a reset on `member`?  `X.reset()`, `X->reset()` and `X.softReset()`."""
    return re.search(r"\b" + re.escape(member) + r"\s*(?:\.|->)\s*(?:soft)?[Rr]eset\s*\(", text) \
        is not None


# =============================================================================================
#  The checks.
# =============================================================================================
_SELECTION_LISTS = ("discreteDiffers", "processingDiffers")


def guard_of(body, field):
    """The CONDITION attached to `field`'s comparison in `body`, normalised, or None.

    The two selection lists are `||` chains of `a.X != b.X`. A field whose relevance is
    conditional is written as one parenthesised term --

        || (a.dimMode != b.dimMode && (a.algorithm == Algorithm::DimensionD
                                    || b.algorithm == Algorithm::DimensionD))

    -- so the guard is everything after the `&&` up to the paren that opened the term.
    Whitespace is collapsed so a re-wrap is not a difference; anything else is.
    """
    m = re.search(r"\(\s*a\." + re.escape(field) + r"\s*!=\s*b\." + re.escape(field)
                  + r"\s*&&", body)
    if m is None:
        return None
    depth, k, n = 1, m.end(), len(body)          # the `(` that opened the term
    while k < n:
        if body[k] == "(":
            depth += 1
        elif body[k] == ")":
            depth -= 1
            if depth == 0:
                return " ".join(body[m.end():k].split())
        k += 1
    return None


def check_engine_params(params_h, engine_cpp):
    """Problems with the four `EngineParameters` lists, as [(file, line, message)]."""
    problems = []
    fields = [n for _, _, n in parse_members(params_h, "struct EngineParameters")]
    if not fields:
        return [(PARAMS_H, 1, "no fields parsed out of `struct EngineParameters` -- this lint "
                              "would report the tree clean without checking anything")]

    unknown = DISCRETE - set(fields)
    if unknown:
        problems.append((PARAMS_H, 1,
                         f"DISCRETE names {sorted(unknown)}, which `EngineParameters` does not "
                         f"declare. Remove them from scripts/check-state-coverage.py."))

    bodies = {}
    for fn in ("sameParameters", "discreteDiffers", "processingDiffers", "copyContinuous"):
        b = function_body(engine_cpp, "AnamorphEngine::" + fn)
        if b is None:
            problems.append((ENGINE_CPP, 1, f"`AnamorphEngine::{fn}` not found -- this lint cannot "
                                            f"check a list it cannot read."))
        bodies[fn] = b or ""

    # --- sameParameters: TOTAL over the struct, in the comparison's own form ------------------
    for f in fields:
        exact = re.search(r"\ba\." + re.escape(f) + r"\s*==\s*b\." + re.escape(f) + r"\b",
                          bodies["sameParameters"])
        memcmp = re.search(r"sameF\s*\(\s*a\." + re.escape(f) + r"\s*,\s*b\." + re.escape(f)
                           + r"\s*\)", bodies["sameParameters"])
        if not (exact or memcmp):
            problems.append((ENGINE_CPP, 1,
                             f"`sameParameters` does not compare `{f}`. Its own comment states the "
                             f"invariant: a field added to EngineParameters and forgotten here has "
                             f"its edits IGNORED whenever nothing else moves. Add "
                             f"`&& a.{f} == b.{f}` (or `&& sameF (a.{f}, b.{f})` for a float)."))

    # --- copyContinuous: preserves EXACTLY the DISCRETE set, in both directions ---------------
    preserved = set()
    for f in fields:
        saved = re.search(r"=\s*dst\." + re.escape(f) + r"\b", bodies["copyContinuous"])
        restored = re.search(r"\bdst\." + re.escape(f) + r"\s*=", bodies["copyContinuous"])
        if saved and restored:
            preserved.add(f)
        elif saved or restored:
            problems.append((ENGINE_CPP, 1,
                             f"`copyContinuous` {'saves' if saved else 'restores'} `{f}` but does "
                             f"not {'restore' if saved else 'save'} it. Half a save/restore pair "
                             f"either loses the field or overwrites it with itself."))
    for f in sorted(DISCRETE - preserved):
        problems.append((ENGINE_CPP, 1,
                         f"`{f}` is declared DISCRETE but `copyContinuous` does not preserve it, so "
                         f"a continuous merge would silently adopt the pending value early."))
    for f in sorted(preserved - DISCRETE):
        problems.append((ENGINE_CPP, 1,
                         f"`copyContinuous` preserves `{f}`, which is not declared DISCRETE. Either "
                         f"add it to DISCRETE in scripts/check-state-coverage.py or stop "
                         f"preserving it -- the two lists must describe the same partition."))

    # --- discreteDiffers / processingDiffers: DISCRETE fields, named or excluded with a reason -
    for fn, table, excluded in (("discreteDiffers", "DISCRETE_DIFFERS_EXCLUDED",
                                DISCRETE_DIFFERS_EXCLUDED),
                               ("processingDiffers", "PROCESSING_DIFFERS_EXCLUDED",
                                PROCESSING_DIFFERS_EXCLUDED)):
        named = {f for f in fields
                 if re.search(r"\ba\." + re.escape(f) + r"\s*!=\s*b\." + re.escape(f) + r"\b",
                              bodies[fn])}
        for f in sorted(named - DISCRETE):
            problems.append((ENGINE_CPP, 1,
                             f"`{fn}` compares `{f}`, which is not declared DISCRETE. A continuous "
                             f"field reaches its module through a smoother, so making it duck (or "
                             f"re-arm the matcher) is a decision that needs a reason."))
        for f in sorted(DISCRETE - named):
            if f not in excluded:
                problems.append((ENGINE_CPP, 1,
                                 f"`{fn}` does not answer for the DISCRETE field `{f}`. Either "
                                 f"compare it (`|| a.{f} != b.{f}`) or record why it is excluded in "
                                 f"{table} in scripts/check-state-coverage.py."))
        for f in sorted(set(excluded) & named):
            problems.append((ENGINE_CPP, 1,
                             f"`{f}` is declared excluded from `{fn}` but the code compares it. The "
                             f"declaration and the code disagree; fix whichever is wrong."))
        for f in sorted(set(excluded) - DISCRETE):
            problems.append((ENGINE_CPP, 1,
                             f"`{f}` is excluded from `{fn}` but is not DISCRETE, so the exclusion "
                             f"describes nothing."))

    # --- GUARD PARITY between the two selection lists ----------------------------------------
    # The narrowest rule this lint could add that would have caught a real, measured defect:
    # R4 gave `dimMode` a Dimension-D relevance guard in `discreteDiffers` and left
    # `processingDiffers` comparing it unconditionally. The lint stayed green for four rounds,
    # because "the field is named" was satisfied either way -- and an inert Dim-D Style move
    # then threw away a converged Level-Match reading through a forced duck (Test 58).
    #
    # WHAT THIS CLAIMS, and it is deliberately not more: the two lists must give the SAME
    # answer where they both answer. It does NOT claim the answer is right -- that is still the
    # reviewer's, the ADR's and the test's job, and Test 58 is what actually pins this one.
    # The lists ask different questions, so a divergence CAN be legitimate; it just has to be
    # declared with a reason rather than appear.
    guards = {f: {fn: guard_of(bodies[fn], f) for fn in _SELECTION_LISTS} for f in fields}
    for f in sorted(fields):
        g = guards[f]
        present = {fn: g[fn] for fn in _SELECTION_LISTS
                   if re.search(r"\ba\." + re.escape(f) + r"\s*!=\s*b\." + re.escape(f) + r"\b",
                                bodies[fn])}
        if len(present) < 2 or len(set(present.values())) == 1:
            continue
        if f in GUARD_DIVERGENCE:
            continue
        shown = "; ".join(f"`{fn}`: " + (v if v else "no guard") for fn, v in present.items())
        problems.append((ENGINE_CPP, 1,
                         f"`{f}` is compared under DIFFERENT conditions in the two selection "
                         f"lists -- {shown}. Both ask whether the field reaches a module, so a "
                         f"difference is a decision: make them agree, or record why they must "
                         f"not in GUARD_DIVERGENCE in scripts/check-state-coverage.py."))
    for f in sorted(set(GUARD_DIVERGENCE) - set(fields)):
        problems.append((ENGINE_CPP, 1,
                         f"GUARD_DIVERGENCE names `{f}`, which `EngineParameters` does not "
                         f"declare. Remove it."))
    return problems


def _depth_one(braces):
    """`braces` with every nested brace block removed, so only its own declarations remain."""
    out, depth = [], 0
    for ch in braces:
        if ch == "{":
            depth += 1
            if depth <= 2:
                out.append(ch)
        elif ch == "}":
            if depth <= 2:
                out.append(ch)
            depth -= 1
        elif depth <= 1 or ch == "\n":
            out.append(ch)
    return "".join(out)


def type_body(text, base):
    """The braces of `class base` / `struct base` in `text`, or None.

    `enum class base` is NOT a match. An enum has no members and no reset(), so reading one as a
    class made `switchState` look like a resettable module -- and a header-wide search for
    `void reset (` then found some OTHER type's, which is how a scalar enum acquired a row it has
    no use for. The declaration must be a real class or struct, and its reset must be ITS OWN.
    """
    for m in re.finditer(r"(?<!\benum\s)\b(?:class|struct)\s+" + re.escape(base) + r"\b", text):
        j = text.find("{", m.end())
        k = text.find(";", m.end())
        if j < 0 or (0 <= k < j):
            continue                       # a forward declaration, not a definition
        return text[j:_matching_brace(text, j)]
    return None


def _dsp_type_body(base):
    """The `src/dsp/` definition of type `base`, or None if the type is not ours."""
    direct = SRC / "dsp" / (base + ".h")
    if direct.is_file():
        body = type_body(strip_comments_and_strings(direct.read_text(encoding="utf-8")), base)
        if body is not None:
            return body
    for path in sorted((SRC / "dsp").glob("*.h")):
        body = type_body(strip_comments_and_strings(path.read_text(encoding="utf-8")), base)
        if body is not None:
            return body
    return None


def reset_subject(members):
    """(subject, problems): the engine members a `reset()` call can reach, derived not declared.

    A member is in the subject when its type is ours and declares `void reset (`, or when its type
    is declared RESETTABLE in EXTERNAL_TYPES.  A type from neither source is a PROBLEM rather than a
    silent skip: that is what keeps the derivation total as the engine grows.
    """
    subject, problems = [], []
    for line, ty, name in members:
        base = ty.split("<")[0].strip()
        own = _dsp_type_body(base.split("::")[-1])
        if own is not None:
            # ITS OWN reset, at ITS OWN depth: a nested helper's reset() is not this member's.
            if re.search(r"\bvoid\s+reset\s*\(", _depth_one(own)):
                subject.append((line, ty, name))
            continue
        verdict = EXTERNAL_TYPES.get(base)
        if verdict is None:
            problems.append((ENGINE_H, line,
                             f"`{name}` has type `{ty}`, which is not defined in src/dsp/ and is "
                             f"not classified in EXTERNAL_TYPES. Say once, by type, whether members "
                             f"of it carry reset state `AnamorphEngine::reset` must answer for "
                             f"(RESETTABLE) or why they do not."))
        elif verdict is RESETTABLE:
            subject.append((line, ty, name))
    return subject, problems


def check_engine_reset(engine_h, engine_cpp):
    """Problems with `AnamorphEngine::reset (ResetScope)`'s module coverage."""
    members = parse_members(engine_h, "class AnamorphEngine")
    if not members:
        return [(ENGINE_H, 1, "no members parsed out of `class AnamorphEngine` -- this lint would "
                              "report the tree clean without checking anything")]

    subject, problems = reset_subject(members)

    body = function_body(engine_cpp, "AnamorphEngine::reset")
    if body is None:
        return problems + [(ENGINE_CPP, 1, "`AnamorphEngine::reset` not found")]
    uncond, ever, tails = split_by_reset_scope(body)

    for line, ty, name in subject:
        actual_uncond = resets_in(uncond, name)
        actual_ever = actual_uncond or resets_in(ever, name)
        actual_tails = actual_uncond or resets_in(tails, name)
        actual = ("both" if actual_ever and actual_tails
                  else "everything" if actual_ever
                  else "tails" if actual_tails
                  else "never")
        if name not in RESET_SCOPE:
            problems.append((ENGINE_H, line,
                             f"`{name}` ({ty}) has its own reset() but no row in RESET_SCOPE. "
                             f"Decide what a HOST reset (ResetScope::audioTailsOnly) should do with "
                             f"it -- clearing audio state is the ordinary answer, but display and "
                             f"user-owned state must survive -- and record it with a reason in "
                             f"scripts/check-state-coverage.py. The code currently does: {actual}."))
            continue
        declared, reason = RESET_SCOPE[name]
        if declared != actual:
            if declared == "everything" and actual in ("both", "tails"):
                why = ("A HOST reset now clears state the declaration says must survive it"
                       + (" -- the Round-6 `levels` defect: LevelMeters::reset() wipes "
                          "peakHoldL/R, the user's held peak." if name == "levels" else "."))
            elif declared == "both" and actual in ("everything", "never"):
                why = ("A HOST reset no longer clears state the declaration calls audio"
                       + (" -- the Round-6 `loudness` defect: stale energy holds the silence gate "
                          "shut and the published Level-Match gain drifts on silence, against "
                          "ADR-0007." if name == "loudness" else "."))
            else:
                why = "The declaration and the code disagree; fix whichever is wrong."
            problems.append((ENGINE_CPP, 1,
                             f"`{name}` is declared `{declared}` ({reason}) but "
                             f"`AnamorphEngine::reset` does `{actual}`. {why}"))

    live = {n for _, _, n in subject}
    for name in sorted(set(RESET_SCOPE) - live):
        problems.append((ENGINE_H, 1,
                         f"RESET_SCOPE has a row for `{name}`, which `AnamorphEngine` no longer "
                         f"declares as a resettable member. Remove the row."))
    return problems


def lint(params_h, engine_h, engine_cpp):
    return check_engine_params(params_h, engine_cpp) + check_engine_reset(engine_h, engine_cpp)


def lint_tree():
    return lint((ROOT / PARAMS_H).read_text(encoding="utf-8"),
                (ROOT / ENGINE_H).read_text(encoding="utf-8"),
                (ROOT / ENGINE_CPP).read_text(encoding="utf-8"))

# =============================================================================================
#  Self-test.
# =============================================================================================
_CONTINUOUS_SAMPLE = ("mix", "width")     # two continuous fields, enough to test the partition


def _params_src(extra=()):
    """A synthetic `EngineParameters` holding every DISCRETE field plus two continuous ones."""
    lines = [f"    int {f} = 0;" for f in sorted(DISCRETE)]
    lines += [f"    float {f} = 0.0f;" for f in _CONTINUOUS_SAMPLE]
    lines += [f"    float {f} = 0.0f;" for f in extra]
    return "struct EngineParameters\n{\n" + "\n".join(lines) + "\n};\n"


def _same_src(fields, skip=(), swap=None, comment_only=()):
    """`sameParameters` comparing `fields`, minus `skip`, with optional deliberate defects."""
    terms = []
    for f in fields:
        if f in skip or f in comment_only:
            continue
        if f == swap:
            terms.append(f"a.{f} == b.width")
        elif f in _CONTINUOUS_SAMPLE:
            terms.append(f"sameF (a.{f}, b.{f})")
        else:
            terms.append(f"a.{f} == b.{f}")
    prose = "".join(f"    // a.{f} == b.{f} is handled elsewhere\n" for f in comment_only)
    return ("bool AnamorphEngine::sameParameters (const EngineParameters& a, "
            "const EngineParameters& b) noexcept\n{\n" + prose
            + "    return " + "\n        && ".join(terms) + ";\n}\n")


def _copy_src(preserve, half=None):
    """`copyContinuous` preserving `preserve`; `half` is saved but never restored."""
    saves = "".join(f"    const auto k_{f} = dst.{f};\n" for f in sorted(set(preserve) | {half} - {None}))
    restores = "".join(f"    dst.{f} = k_{f};\n" for f in sorted(preserve))
    return ("void AnamorphEngine::copyContinuous (EngineParameters& dst, "
            "const EngineParameters& src) noexcept\n{\n" + saves
            + "    dst = src;\n" + restores + "}\n")


def _diff_src(fn, named, guarded=None):
    guarded = guarded or {}
    terms = [(f"(a.{f} != b.{f} && {guarded[f]})" if f in guarded else f"a.{f} != b.{f}")
             for f in sorted(named)] or ["false"]
    return (f"bool AnamorphEngine::{fn} (const EngineParameters& a, "
            f"const EngineParameters& b) noexcept\n{{\n    return "
            + "\n        || ".join(terms) + ";\n}\n")


def _clean_params_cpp(**kw):
    """A tree that must stay quiet: every list answering for every field."""
    fields = sorted(DISCRETE) + list(_CONTINUOUS_SAMPLE)
    return (_same_src(fields, **kw)
            + _copy_src(DISCRETE)
            + _diff_src("discreteDiffers", DISCRETE - set(DISCRETE_DIFFERS_EXCLUDED))
            + _diff_src("processingDiffers", DISCRETE - set(PROCESSING_DIFFERS_EXCLUDED)))


_ENGINE_H_HEAD = "class AnamorphEngine\n{\npublic:\n    enum class ResetScope { everything, " \
                 "audioTailsOnly };\n    void reset (ResetScope s = ResetScope::everything);\n" \
                 "private:\n"
_ENGINE_H_TAIL = "};\n"


def _engine_h(members):
    return _ENGINE_H_HEAD + "".join(f"    {m}\n" for m in members) + _ENGINE_H_TAIL


def _reset_src(unconditional=(), everything=(), tails=()):
    body = "".join(f"    {c}\n" for c in unconditional)
    if everything or tails:
        body += "    if (resetScope == ResetScope::everything)\n    {\n"
        body += "".join(f"        {c}\n" for c in everything) + "    }\n"
        if tails:
            body += "    else\n    {\n" + "".join(f"        {c}\n" for c in tails) + "    }\n"
    return "void AnamorphEngine::reset (ResetScope resetScope)\n{\n" + body + "}\n"


#: The engine's real module members, as the header spells them -- the fixture every reset case
#: varies one line of.
_REAL_MODULES = [
    "HaasProcessor haas;", "VelvetNoise velvet;", "ChorusEngine chorus;",
    "MultibandWidth multiband;", "MonoMaker monoMaker;", "SoloMonitor soloMonitor;",
    "LoudnessMatch loudness;", "CorrelationMeter correlation;", "LevelMeters levels;",
    "ScopeBuffer scope;",
    "std::unique_ptr<juce::dsp::Oversampling<float>> os2, os4, os8;",
]
_ALL_RESET = ["haas.reset();", "velvet.reset();", "chorus.reset();", "multiband.reset();",
              "monoMaker.reset();", "soloMonitor.reset();",
              "if (os2) os2->reset();", "if (os4) os4->reset();", "if (os8) os8->reset();"]


def self_test():
    checked = failures = 0

    def check(what, got, want):
        nonlocal checked, failures
        checked += 1
        if got != want:
            failures += 1
            print(f"self-test FAIL: {what}: got {got!r}, want {want!r}", file=sys.stderr)

    def fires(what, problems, needle, count=1):
        """`problems` must be exactly `count` reports, at least one of them about `needle`."""
        check(f"{what} -- {count} report(s)", len(problems), count)
        check(f"{what} -- names `{needle}`", any(needle in p[2] for p in problems), True)

    # --- 1. PARSERS. A parser that reaches no members reports a clean tree ---------------------
    fields = [n for _, _, n in parse_members(_params_src(), "struct EngineParameters")]
    check("every synthetic field is parsed", len(fields), len(DISCRETE) + len(_CONTINUOUS_SAMPLE))
    check("a brace-initialised member is parsed, and all of its names",
          [n for _, _, n in parse_members(
              "class E\n{\n    std::atomic<int> a { 0 }, b { 0 }, c { 0 };\n};", "class E")],
          ["a", "b", "c"])
    check("an inline function body is not read as an initialiser",
          [n for _, _, n in parse_members(
              "class E\n{\n    float get() const noexcept { return x; }\n    int y;\n};", "class E")],
          ["y"])
    check("a nested enum's values are not members",
          [n for _, _, n in parse_members(
              "class E\n{\n    enum class S { A, B };\n    S s;\n};", "class E")], ["s"])
    check("a member named only in a comment is not parsed",
          [n for _, _, n in parse_members(
              "class E\n{\n    // LoudnessMatch ghost;\n    int y;\n};", "class E")], ["y"])
    check("a digit separator does not swallow the members after it",
          [n for _, _, n in parse_members(
              "class E\n{\n    int k = 1'000;\n    int y;\n};", "class E")], ["k", "y"])
    check("a character literal does not swallow the members after it",
          [n for _, _, n in parse_members(
              "class E\n{\n    char c = 'x';\n    int y;\n};", "class E")], ["c", "y"])

    # --- 2. THE SCOPE SPLIT, which is what target 2's whole verdict rests on -------------------
    body = _reset_src(["haas.reset();"], ["levels.reset();"], ["loudness.softReset();"])
    uncond, ever, tails = split_by_reset_scope(function_body(body, "AnamorphEngine::reset"))
    check("an unconditional call lands outside both guards", resets_in(uncond, "haas"), True)
    check("a then-branch call lands in `everything`", resets_in(ever, "levels"), True)
    check("a then-branch call is NOT unconditional", resets_in(uncond, "levels"), False)
    check("an else-branch call lands in `tails`", resets_in(tails, "loudness"), True)
    check("an else-branch call is NOT `everything`", resets_in(ever, "loudness"), False)
    brace_less = _reset_src([], [], [])
    brace_less = ("void AnamorphEngine::reset (ResetScope resetScope)\n{\n"
                  "    if (resetScope == ResetScope::everything) loudness.reset();\n"
                  "    else                                      loudness.softReset();\n}\n")
    u2, e2, t2 = split_by_reset_scope(function_body(brace_less, "AnamorphEngine::reset"))
    check("a brace-less then-branch is seen", resets_in(e2, "loudness"), True)
    check("a brace-less else-branch is seen", resets_in(t2, "loudness"), True)
    check("a brace-less pair is not read as unconditional", resets_in(u2, "loudness"), False)
    check("an unrelated `if` does not open a scope branch",
          resets_in(split_by_reset_scope(function_body(
              _reset_src(["if (os2) os2->reset();"]), "AnamorphEngine::reset"))[0], "os2"), True)
    check("softReset counts as a reset", resets_in("loudness.softReset();", "loudness"), True)
    # ...but `resetLive` must NOT. It is a partial reset that leaves the user's latches, and
    # counting it would let the Round-6 defect -- a FULL `levels.reset()` on the host-reset
    # path -- read `both`, match its declaration and go uncaught.
    check("resetLive is not a full reset", resets_in("levels.resetLive();", "levels"), False)
    check("resetHold is not a full reset", resets_in("levels.resetHold();", "levels"), False)
    check("a near-miss member name does not count",
          resets_in("loudnessRefScratch.reset();", "loudness"), False)

    # --- 3. TYPE RESOLUTION -------------------------------------------------------------------
    check("an enum class is not a resettable type", type_body("enum class S { A };", "S"), None)
    check("a forward declaration is not a definition", type_body("class S;\n", "S"), None)
    check("a real definition is found", type_body("struct S { void reset(); };", "S") is None, False)
    check("a nested helper's reset is not the outer type's",
          bool(re.search(r"\bvoid\s+reset\s*\(",
                         _depth_one(type_body("struct S { struct I { void reset(); }; I i; };",
                                              "S")))), False)

    # --- 4. TARGET 1 MUST STAY QUIET on a tree that answers for every field --------------------
    check("a complete set of four lists is quiet",
          check_engine_params(_params_src(), _clean_params_cpp()), [])

    # --- 5. TARGET 1 MUST FIRE ----------------------------------------------------------------
    # The defect the invariant is written against: a field added and forgotten.
    fires("a field missing from sameParameters",
          check_engine_params(_params_src(("newKnob",)),
                              _clean_params_cpp() ),
          "newKnob")
    fires("sameParameters naming a field only in prose",
          check_engine_params(_params_src(), _clean_params_cpp(comment_only=("mix",))), "mix")
    fires("sameParameters comparing mismatched operands",
          check_engine_params(_params_src(), _clean_params_cpp(swap="bypass")), "bypass")
    fires("copyContinuous dropping a DISCRETE field",
          check_engine_params(_params_src(),
                              _same_src(sorted(DISCRETE) + list(_CONTINUOUS_SAMPLE))
                              + _copy_src(DISCRETE - {"solo"})
                              + _diff_src("discreteDiffers", DISCRETE - set(DISCRETE_DIFFERS_EXCLUDED))
                              + _diff_src("processingDiffers",
                                          DISCRETE - set(PROCESSING_DIFFERS_EXCLUDED))),
          "solo")
    fires("copyContinuous preserving a continuous field",
          check_engine_params(_params_src(),
                              _same_src(sorted(DISCRETE) + list(_CONTINUOUS_SAMPLE))
                              + _copy_src(DISCRETE | {"mix"})
                              + _diff_src("discreteDiffers", DISCRETE - set(DISCRETE_DIFFERS_EXCLUDED))
                              + _diff_src("processingDiffers",
                                          DISCRETE - set(PROCESSING_DIFFERS_EXCLUDED))),
          "mix")
    fires("copyContinuous saving a field it never restores",
          check_engine_params(_params_src(),
                              _same_src(sorted(DISCRETE) + list(_CONTINUOUS_SAMPLE))
                              + _copy_src(DISCRETE, half="mix")
                              + _diff_src("discreteDiffers", DISCRETE - set(DISCRETE_DIFFERS_EXCLUDED))
                              + _diff_src("processingDiffers",
                                          DISCRETE - set(PROCESSING_DIFFERS_EXCLUDED))),
          "mix")
    for fn, excluded in (("discreteDiffers", DISCRETE_DIFFERS_EXCLUDED),
                         ("processingDiffers", PROCESSING_DIFFERS_EXCLUDED)):
        other = ("processingDiffers" if fn == "discreteDiffers" else "discreteDiffers")
        other_ex = (PROCESSING_DIFFERS_EXCLUDED if fn == "discreteDiffers"
                    else DISCRETE_DIFFERS_EXCLUDED)
        base = (_same_src(sorted(DISCRETE) + list(_CONTINUOUS_SAMPLE)) + _copy_src(DISCRETE)
                + _diff_src(other, DISCRETE - set(other_ex)))
        victim = sorted(DISCRETE - set(excluded))[0]
        fires(f"{fn} dropping an unexcused DISCRETE field",
              check_engine_params(_params_src(),
                                  base + _diff_src(fn, DISCRETE - set(excluded) - {victim})),
              victim)
        fires(f"{fn} comparing a field declared excluded from it",
              check_engine_params(_params_src(), base + _diff_src(fn, DISCRETE)),
              sorted(excluded)[0], count=len(excluded))
        fires(f"{fn} comparing a continuous field",
              check_engine_params(_params_src(),
                                  base + _diff_src(fn, (DISCRETE - set(excluded)) | {"mix"})),
              "mix")

    # --- 5b. GUARD PARITY between the two selection lists -------------------------------------
    # The rule that would have caught the R8 defect: R4 guarded `dimMode` in `discreteDiffers`
    # and left `processingDiffers` comparing it unconditionally, and "the field is named" was
    # satisfied either way. Both directions are exercised, including the escape hatch.
    G1 = "(a.algorithm == X::D || b.algorithm == X::D)"
    G2 = "(a.algorithm == X::D)"

    def _pair(gd=None, gp=None):
        """Both selection lists over the full DISCRETE set, each optionally guarding `solo`."""
        return (_same_src(sorted(DISCRETE) + list(_CONTINUOUS_SAMPLE)) + _copy_src(DISCRETE)
                + _diff_src("discreteDiffers", DISCRETE - set(DISCRETE_DIFFERS_EXCLUDED),
                            {"solo": gd} if gd else None)
                + _diff_src("processingDiffers", DISCRETE - set(PROCESSING_DIFFERS_EXCLUDED),
                            {"solo": gp} if gp else None))

    check("an unguarded term has no guard",
          guard_of("return a.solo != b.solo || a.mix != b.mix;", "solo"), None)
    check("a guard is extracted whole, nested parens included",
          guard_of("return (a.solo != b.solo && (a.x == P::Q || b.x == P::Q)) || z;", "solo"),
          "(a.x == P::Q || b.x == P::Q)")
    check("a re-wrapped guard is the same guard",
          guard_of("return (a.solo != b.solo && (a.x == P::Q\n   || b.x == P::Q));", "solo"),
          guard_of("return (a.solo != b.solo && (a.x == P::Q || b.x == P::Q));", "solo"))

    check("the same guard in both lists is quiet",
          check_engine_params(_params_src(), _pair(G1, G1)), [])
    check("no guard in either list is quiet",
          check_engine_params(_params_src(), _pair()), [])
    fires("a guard in ONE list only -- the R8 defect's own shape",
          check_engine_params(_params_src(), _pair(G1, None)), "solo")
    fires("a guard in the OTHER list only",
          check_engine_params(_params_src(), _pair(None, G1)), "solo")
    fires("two DIFFERENT guards",
          check_engine_params(_params_src(), _pair(G1, G2)), "solo")

    _saved = dict(GUARD_DIVERGENCE)
    try:
        GUARD_DIVERGENCE["solo"] = "a declared, deliberate difference"
        check("a DECLARED divergence is quiet",
              check_engine_params(_params_src(), _pair(G1, None)), [])
        GUARD_DIVERGENCE.clear()
        GUARD_DIVERGENCE["notAField"] = "stale"
        fires("a declaration for a field the struct does not have",
              check_engine_params(_params_src(), _pair(G1, G1)), "notAField")
    finally:
        GUARD_DIVERGENCE.clear()
        GUARD_DIVERGENCE.update(_saved)
    check("the table is restored after the divergence cases", dict(GUARD_DIVERGENCE), _saved)

    # --- 6. TARGET 2 MUST STAY QUIET on the real shape ----------------------------------------
    clean_reset = _reset_src(_ALL_RESET + ["correlation.reset();", "loudnessDone();"],
                             ["levels.reset();"], ["levels.resetLive();"])
    clean_reset = clean_reset.replace("    loudnessDone();\n",
                                      "    if (resetScope == ResetScope::everything) "
                                      "loudness.reset();\n    else loudness.softReset();\n")
    check("the real module shape is quiet", check_engine_reset(_engine_h(_REAL_MODULES),
                                                               clean_reset), [])

    # --- 7. TARGET 2 MUST FIRE: the two Round-6 defects, in their original form ----------------
    # `loudness` untouched on a host reset -- what R5 shipped and R6 measured.
    fires("loudness left out of the host-reset path",
          check_engine_reset(_engine_h(_REAL_MODULES),
                             _reset_src(_ALL_RESET + ["correlation.reset();"],
                                        ["levels.reset();", "loudness.reset();"],
                                        ["levels.resetLive();"])),
          "loudness")
    # `levels` cleared on a host reset -- the user's held peak, wiped by every transport stop.
    levels_out = _reset_src(_ALL_RESET + ["correlation.reset();", "levels.reset();",
                                          "loudnessPair();"], []).replace(
        "    loudnessPair();\n",
        "    if (resetScope == ResetScope::everything) loudness.reset();\n"
        "    else loudness.softReset();\n")
    fires("levels cleared on the host-reset path",
          check_engine_reset(_engine_h(_REAL_MODULES), levels_out), "levels")
    # `correlation` moved back under `everything` -- R6's shape, which froze the phase and
    # balance pointers for any host that resets and then stops calling processBlock.
    corr_back = _reset_src(_ALL_RESET + ["loudnessPair();"],
                           ["correlation.reset();", "levels.reset();"],
                           ["levels.resetLive();"]).replace(
        "    loudnessPair();\n",
        "    if (resetScope == ResetScope::everything) loudness.reset();\n"
        "    else loudness.softReset();\n")
    fires("correlation moved back to a re-prepare only",
          check_engine_reset(_engine_h(_REAL_MODULES), corr_back), "correlation")
    fires("a new resettable module with no declared answer",
          check_engine_reset(_engine_h(_REAL_MODULES + ["MonoMaker monoMaker2;"]),
                             clean_reset), "monoMaker2")
    fires("an external type nobody has classified",
          check_engine_reset(_engine_h(_REAL_MODULES + ["juce::dsp::Limiter<float> limiter;"]),
                             clean_reset), "limiter")
    fires("a declared row for a member that no longer exists",
          check_engine_reset(_engine_h([m for m in _REAL_MODULES if "velvet" not in m]),
                             clean_reset.replace("    velvet.reset();\n", "")), "velvet")

    # --- 8. TARGET 2 MUST STAY QUIET on the cases that look like violations but are not --------
    check("ScopeBuffer needs no row -- it declares no reset of its own",
          any("scope" in p[2] for p in check_engine_reset(_engine_h(_REAL_MODULES), clean_reset)),
          False)
    check("a scalar member needs no row",
          check_engine_reset(_engine_h(_REAL_MODULES + ["bool driveActive;", "int dryDelayWrite;"]),
                             clean_reset), [])
    check("a buffer and a smoother need no row",
          check_engine_reset(_engine_h(_REAL_MODULES + ["juce::AudioBuffer<float> dryScratch;",
                                                        "juce::SmoothedValue<float> mixSmooth;"]),
                             clean_reset), [])

    # --- 9. THE REAL TREE, through the real reader --------------------------------------------
    real = lint_tree()
    check("the real tree is clean", real, [])
    real_fields = parse_members((ROOT / PARAMS_H).read_text(encoding="utf-8"),
                                "struct EngineParameters")
    check("the real EngineParameters is read, not skipped", len(real_fields) >= 30, True)
    real_subject, _ = reset_subject(parse_members((ROOT / ENGINE_H).read_text(encoding="utf-8"),
                                                  "class AnamorphEngine"))
    check("the real engine's module subject is read, not skipped",
          sorted(n for _, _, n in real_subject), sorted(RESET_SCOPE))

    if failures:
        print(f"\ncheck-state-coverage: {failures} of {checked} self-test case(s) failed.",
              file=sys.stderr)
        return 1
    print(f"check-state-coverage: self-test passed ({checked} cases).")
    return 0


def main():
    ap = argparse.ArgumentParser(
        description="Verify every hand-maintained state list answers for every member.")
    ap.add_argument("--self-test", action="store_true",
                    help="verify this tool's own parsers and checks")
    args = ap.parse_args()

    if args.self_test:
        return self_test()

    problems = lint_tree()
    if problems:
        for rel, line, message in problems:
            print(f"::error file={rel},line={line}::{message}", file=sys.stderr)
        print(f"\ncheck-state-coverage: {len(problems)} unanswered member(s). Each is a list that "
              f"has stopped covering its subject -- the defect class that produced the Round-4 "
              f"`dimMode` duck and both Round-6 host-reset findings.", file=sys.stderr)
        return 1

    fields = len(parse_members((ROOT / PARAMS_H).read_text(encoding="utf-8"),
                               "struct EngineParameters"))
    modules = len(RESET_SCOPE)
    print(f"check-state-coverage: {fields} EngineParameters field(s) answered by 4 list(s), "
          f"{modules} engine module(s) answered for both reset scopes.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
