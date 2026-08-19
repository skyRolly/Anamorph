#!/usr/bin/env python3
"""
Anamorph -- static realtime lint: forbidden operations inside audio-path bodies.

WHAT IT IS FOR, and why it exists beside two runtime tools
==========================================================
`REALTIME_AUDIO_POLICY.md` forbids allocation, locking, blocking and IO on the
audio thread.  Two mechanisms already enforce that AT RUNTIME: RealtimeSanitizer
(the `realtime` job, ADR-0029) and the allocation guard compiled into the DSP
suite (`tests/AllocationGuard.h`).  Both are strictly better than a text scan
where they apply -- they see real calls through JUCE, through templates, and
through anything a grep cannot follow.

They share one blind spot, and it is the only reason this lint exists: a runtime
tool sees exactly the code the suite EXECUTES.  Measured on this tree, the DSP
suite covers 93.4 % of lines and 79.9 % of branches in `src/dsp` -- so a `malloc`
added to an unexecuted branch is invisible to both of them and visible here.
This lint is therefore the cheap complement, not a third opinion on the same
question: it costs no build, runs on every platform, and reads the branches the
suite never takes.

WHY IT IS FUNCTION-SCOPED, which is the whole design
====================================================
A file-wide token scan over `src` is unusable, and measurably so: the eight
`setSize` calls in `AnamorphEngine.cpp` are all inside `prepare()`, where
allocation is not merely allowed but REQUIRED (the policy's own wording: "Buffer
sizing must happen in prepare()").  A lint that flags them is a lint that gets
switched off.

So the scan is bounded to the bodies of the functions the policy names, and
`prepare` is out of scope simply by not being one of them -- there is no separate
exemption list to drift.  Body extents come from brace matching after comments and
string literals are removed, so a `//` mentioning "new" or a diagnostic string
containing "malloc" cannot fire -- both are real shapes in this tree.

WHAT IT DOES NOT CLAIM
======================
It is a TEXT scan.  It cannot see an allocation inside a callee, inside JUCE, or
behind a template, and it is not trying to: that is what the runtime tier is for.
A clean run here means "no forbidden construct is written literally in an
audio-path body", which is a narrow claim deliberately.

SELF-TEST (`--self-test`), per TESTING_POLICY rule 4
====================================================
Runs the checker's own functions over synthetic sources in BOTH directions:
every "must fire" case is a violation class the lint exists to catch, every
"must stay silent" case is valid code an over-eager revision would flag --
including the `prepare()` allocations this tree really contains.
"""

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
# The policy binds `processBlock` / `AnamorphEngine::process` and every DSP
# module's `process`/`reset` -- and `AnamorphAudioProcessor::processBlock` lives
# in `src/`, not `src/dsp/`. Scanning `src` rather than `src/dsp` is what makes
# the lint's scope the POLICY's scope; the function filter below is what keeps
# the rest of `src` (editor, presets, parameters) out of the findings.
SCAN_DIRS = ["src"]

# THE POLICY-BOUND FUNCTION NAMES, and this list IS the scope: a function whose
# definition matches one of these has its body scanned, and nothing else does.
#
# `reset` and `softReset` are here because REALTIME_AUDIO_POLICY binds "every DSP
# module's `process`/`reset`" -- the engine calls `reset()` at the silent bottom
# of a switch duck and on the NaN self-heal, and `LoudnessMatch::softReset()` from
# `process` itself (AnamorphEngine.cpp:701), so all of them run on the audio
# thread. REALTIME_SAFETY_AUDIT audits them for the same reason.
#
# `prepare` is NOT here, and that is the whole mechanism by which allocation
# stays legal there -- the policy requires buffer sizing to happen in `prepare()`.
# There is deliberately no separate exemption list: a name that is not in this
# regex is never scanned, so a second list could only drift out of agreement with
# the first. (An earlier revision carried one; every entry in it was unreachable.)
AUDIO_FN = re.compile(r"\b(process|processBlock|processSample|applyInputConditioning|"
                      r"processNonlinearRegion|pushBlock|publish|reset|softReset)\b")

# (regex, human-readable violation class). Kept deliberately small: every entry
# is a construct from the policy's forbidden list that can be written literally.
FORBIDDEN = [
    (re.compile(r"\bnew\s+[A-Za-z_:<]"),           "heap allocation (`new`)"),
    (re.compile(r"\bdelete\b\s*(\[\s*\])?\s*[A-Za-z_(]"), "heap free (`delete`)"),
    (re.compile(r"\b(malloc|calloc|realloc|free)\s*\("), "C heap allocation"),
    # `make_unique`/`make_shared` ARE `new`, spelled so the `new` pattern above
    # cannot see them -- and they are how this engine allocates its oversamplers
    # (`AnamorphEngine.cpp`). A lint that catches `new T` but not
    # `std::make_unique<T>` catches the spelling nobody uses.
    (re.compile(r"\b(std::)?make_(unique|shared)\b"),
                                                   "heap allocation (`make_unique`/`make_shared`)"),
    # `.assign` is THE allocation idiom of this codebase, not a hypothetical
    # one: every DSP module sizes its buffers with it in `prepare()`, and
    # `REALTIME_SAFETY_AUDIT.md` names it as such ("All heap allocation is
    # confined to `prepare()` (via `std::vector::assign` ...)"). The most
    # probable realtime regression here is one of those lines being copied into
    # a `reset()` or `process()` body, which is exactly what this entry sees.
    (re.compile(r"\.(resize|push_back|emplace_back|reserve|assign|insert)\s*\("),
                                                   "container growth"),
    (re.compile(r"\.setSize\s*\("),                "buffer resize (`setSize`)"),
    (re.compile(r"\b(std::)?(mutex|recursive_mutex|lock_guard|unique_lock|scoped_lock)\b"),
                                                   "lock"),
    (re.compile(r"\bjuce::(ScopedLock|CriticalSection|SpinLock)\b"), "JUCE lock"),
    (re.compile(r"\b(std::)?(async|thread|condition_variable|promise|future)\b"),
                                                   "threading / blocking primitive"),
    (re.compile(r"\b(fopen|fread|fwrite|fprintf|printf|puts|system|sleep|usleep)\s*\("),
                                                   "IO / sleep / subprocess"),
    (re.compile(r"\bjuce::(File|FileOutputStream|FileInputStream)\b"), "file IO"),
]


def _is_digit_separator(text: str, i: int) -> bool:
    """Is `text[i]` (an apostrophe) a C++14 digit separator rather than a quote?

    A separator sits between two digits of ONE numeric token, and a numeric
    token STARTS with a digit. Both halves of that sentence are load-bearing.

    TESTING ONLY THE TWO NEIGHBOURS IS NOT ENOUGH, which is what this function
    used to do. An ENCODED character literal -- `L'a'`, `u8'a'`, `u'a'`, `U'a'`
    -- puts an alphanumeric on both sides of its OPENING quote too, so the
    neighbours-only rule emitted that opener as ordinary code, then met the
    CLOSING quote and read it as an opener, blanking the remainder of the line.
    Whatever followed on that line -- including a real violation -- vanished
    from the scan with no diagnostic. That is the same false-negative class the
    raw-string branch above exists to prevent, arriving through the fix for the
    other one.

    Walking LEFT to the start of the token and requiring a DIGIT there
    separates the two cases exactly: `1'000`, `0x1'F` and `1.000'5` begin with
    a digit; an encoding prefix (`L`, `u`, `u8`, `U`) does not, and neither
    does an identifier. `'` and `.` are part of the walk because a separator may
    itself follow an earlier separator (`1'000'000`) or a decimal point
    (`1.000'5`).
    """
    if i == 0 or i + 1 >= len(text):
        return False
    if not text[i + 1].isalnum():
        return False
    j = i - 1
    while j >= 0 and (text[j].isalnum() or text[j] in "'."):
        j -= 1
    return j + 1 < i and text[j + 1].isdigit()


def strip_comments_and_strings(text: str) -> str:
    """Blank out comments and string/char literals, preserving line structure.

    Blanking rather than deleting keeps every line number and brace position
    intact, which is what the body extraction below depends on.
    """
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                out.append(" ")
                i += 1
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            out.append("  ")
            i += 2
            while i < n and not (text[i] == "*" and i + 1 < n and text[i + 1] == "/"):
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            if i < n:
                out.append("  ")
                i += 2
        elif c == "R" and i + 1 < n and text[i + 1] == '"':
            # RAW STRING LITERAL. Without this branch the plain-quote scan below
            # stops at the first `"` inside the raw text -- so `R"(say "hi")"`
            # would leave `hi")"` lexed as code, and a `new` after it read as a
            # violation, or a real one swallowed. Neither shape exists in `src`
            # today; the point is that arriving later must not silently change
            # what the scanner sees.
            j = text.find("(", i + 2)
            delim = text[i + 2:j] if j != -1 else None
            if delim is not None and "\n" not in delim:
                close = ")" + delim + '"'
                k = text.find(close, j + 1)
                if k == -1:
                    k = n
                    end = n
                else:
                    end = k + len(close)
                out.append("  " + " " * (j - (i + 2)) + " ")
                for ch in text[j + 1:end]:
                    out.append("\n" if ch == "\n" else " ")
                i = end
            else:
                out.append(c)
                i += 1
        elif c == "'" and _is_digit_separator(text, i):
            # DIGIT SEPARATOR, not a quote. `1'000` has no closing `'`, so
            # treating it as a char literal blanks everything up to the next
            # apostrophe ANYWHERE in the file -- potentially thousands of lines,
            # silently. Emitting it as an ordinary character is correct: a
            # separator is part of a numeric token, and numeric tokens contain
            # nothing this lint looks for.
            out.append(c)
            i += 1
        elif c in "\"'":
            quote = c
            out.append(" ")
            i += 1
            # BOUNDED TO THE LINE. A non-raw string or character literal cannot
            # contain a bare newline, so an unterminated one is a lexing mistake
            # rather than a long literal -- and blanking to the next quote in the
            # file is how such a mistake turns into hidden violations. Stopping
            # at the newline keeps the damage to one line and keeps the rest of
            # the file readable.
            while i < n and text[i] != quote and text[i] != "\n":
                if text[i] == "\\" and i + 1 < n:
                    out.append("  ")
                    i += 2
                    continue
                out.append(" ")
                i += 1
            if i < n and text[i] == quote:
                out.append(" ")
                i += 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


# Names that are followed by `(` but are not calls. Without this the callee
# scan below would try to resolve `if`, `for`, `switch` and friends.
NOT_A_CALL = frozenset("""
    if else for while do switch case return sizeof alignof new delete throw catch
    try and or not static_cast const_cast dynamic_cast reinterpret_cast
    decltype noexcept typeid operator template using namespace struct class enum
    union public private protected virtual explicit inline constexpr consteval
""".split())

# An identifier immediately followed by `(`, with whatever precedes it captured
# so a member call (`velvet.updateWeights (...)`) can be told from a bare one.
CALLEE = re.compile(r"(?:(\.|->|::)\s*)?\b([A-Za-z_]\w*)\s*\(")

# NEVER FOLLOWED, and this is the mechanism that keeps allocation legal where the
# Policy says it is legal. `prepare()` is REQUIRED to allocate; following a call
# into it would report every one of those allocations. If an audio-path function
# ever calls `prepare()` that is a real defect, but it is a different finding
# from the one this lint makes and it is not silently invented here.
NEVER_FOLLOW = frozenset({"prepare", "prepareToPlay", "releaseResources"})


# Tokens that can legally precede a function NAME in a definition: the end of a
# return type. Anything else -- an opening paren, a comma, an operator, a
# control-flow keyword -- means the name is being USED, not defined.
_IDENT_CHARS = set("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")


def heads_a_definition(clean: str, at: int) -> bool:
    """Is the identifier starting at `at` the NAME of a function definition?

    THE `{`-BEFORE-`;` TEST ALONE IS NOT ENOUGH, and the closure below is what
    made that matter. `if (isModAlgorithm (p.algorithm))` is followed by `{`
    with no intervening `;`, so on that test alone a plain call inside an `if`
    condition reads as a definition -- and the block that followed, which was
    `prepare()`'s body, got attributed to it. Measured: 11 findings, every one of
    them a legitimate `prepare()` allocation reported against a predicate.

    The discriminator is what comes BEFORE the name:

      * `::` -- a qualified definition (`void AnamorphEngine::process (`);
      * `*` or `&` whose own predecessor ends a type (`juce::String& name (`);
      * an identifier that is not a keyword -- the return type (`void f (`).

    Everything else rejects, which covers the shapes a call takes: `(`, `,`,
    `!`, `=`, `&&`, and `return`/`if`/`while` as the preceding word.
    """
    j = at - 1
    while j >= 0 and clean[j] in " \t\n":
        j -= 1
    if j < 0:
        return False
    if clean[j] == ":":
        return j >= 1 and clean[j - 1] == ":"
    if clean[j] in "*&":
        # `Type* name (` and `Type& name (` are definitions; `a && name (` and
        # `a & name (` are not. The difference is what precedes the sigil -- and
        # a doubled sigil is always an operator.
        if j >= 1 and clean[j - 1] in "*&":
            return False
        k = j - 1
        while k >= 0 and clean[k] in " \t\n":
            k -= 1
        return k >= 0 and (clean[k] in _IDENT_CHARS or clean[k] == ">")
    if clean[j] not in _IDENT_CHARS:
        return False
    k = j
    while k >= 0 and clean[k] in _IDENT_CHARS:
        k -= 1
    return clean[k + 1:j + 1] not in NOT_A_CALL


def _bodies(clean: str, name_re):
    """Yield (name, body_start, body_end) for DEFINITIONS whose name matches.

    A definition is a name that HEADS one (see `heads_a_definition`) followed by
    a parameter list and then `{` before any `;`.
    """
    for m in name_re.finditer(clean):
        name = m.group(1)
        # Skip call sites like `engine.process (buf)`, then require the name to
        # be in definition position at all.
        before = clean[max(0, m.start() - 2):m.start()]
        if before.endswith(".") or before.endswith("->"):
            continue
        if not heads_a_definition(clean, m.start()):
            continue
        j = clean.find("(", m.end())
        if j < 0:
            continue
        depth, k = 0, j
        while k < len(clean):
            if clean[k] == "(":
                depth += 1
            elif clean[k] == ")":
                depth -= 1
                if depth == 0:
                    break
            k += 1
        if k >= len(clean):
            continue
        # WHICHEVER COMES FIRST DECIDES: `{` means a definition, `;` means a
        # declaration. The search is deliberately UNBOUNDED. It used to stop
        # 400 characters past the closing parenthesis, and that bound was a
        # silent false negative waiting to happen: `strip_comments_and_strings`
        # blanks a comment but PRESERVES its length, so a long comment or
        # attribute block between `)` and `{` pushed the brace out of the window
        # and the definition left the scanned set with no diagnostic -- in a
        # lint whose entire output on a healthy tree is silence.
        #
        # Removing the bound cannot admit a declaration, because a declaration's
        # `;` is still nearer than any later `{`, and it cannot be fooled by a
        # `;` or `{` inside a comment or string: those are blanked to spaces
        # before this runs. `find` on the whole remainder also avoids copying a
        # slice per candidate.
        brace = clean.find("{", k + 1)
        semi = clean.find(";", k + 1)
        if brace < 0 or (0 <= semi < brace):
            continue                      # a declaration, not a definition
        start = brace
        depth, e = 0, start
        while e < len(clean):
            if clean[e] == "{":
                depth += 1
            elif clean[e] == "}":
                depth -= 1
                if depth == 0:
                    break
            e += 1
        yield name, start, min(e + 1, len(clean))


# Any identifier that heads a definition -- used to build the per-file index the
# callee closure resolves against.
#
# The `(` is a LOOKAHEAD, not part of the match, and that is load-bearing rather
# than stylistic: `_bodies` locates the parameter list with `find("(", m.end())`,
# so a pattern that consumed the paren would send it looking for the NEXT one.
# On a no-argument definition (`void AnamorphEngine::updateDerived()`) the next
# paren is in an unrelated statement, and the body extracted from there is the
# wrong one -- measured, as the reason `updateDerived` was missing from the first
# version of this index while every function WITH parameters resolved fine.
ANY_FN = re.compile(r"\b([A-Za-z_]\w*)\s*(?=\()")


def audio_bodies(clean: str):
    """The SEED set: definitions the Policy names directly."""
    return _bodies(clean, AUDIO_FN)


def definition_index(clean: str):
    """{name: [(start, end), ...]} for every function DEFINED in this file.

    Built once per file so the callee closure can ask "is this name something I
    can actually see the body of?" without re-scanning.
    """
    index = {}
    for name, start, end in _bodies(clean, ANY_FN):
        if name in NOT_A_CALL:
            continue
        index.setdefault(name, []).append((start, end))
    return index


def callees(segment: str):
    """Names called from `segment`, member calls included.

    Deliberately name-only: resolving `velvet.updateWeights()` to a type needs a
    parser, while asking "does THIS FILE define something called
    `updateWeights`?" needs none and answers the question that matters -- the
    helper is in the same file as its caller in every case this covers. A name
    that resolves to nothing in this file is simply not followed.
    """
    out = set()
    for m in CALLEE.finditer(segment):
        qualifier, name = m.group(1), m.group(2)
        if qualifier == "::":
            continue                      # std::/juce:: -- not ours to follow
        if name in NOT_A_CALL or name in NEVER_FOLLOW:
            continue
        out.add(name)
    return out


def reachable_bodies(clean: str):
    """Yield (label, start, end) for every body the audio thread can enter.

    THE SEEDS ARE NOT THE SCOPE. Before this closure existed, only functions
    whose own NAME matched the Policy's list were scanned -- so a helper called
    from `process()` was invisible purely because of what it was called.
    `AnamorphEngine::updateDerived()` (run at the bottom of a switch duck) and
    `VelvetNoise::updateWeights()` (run per block while the density glide moves)
    are both exactly that: audio-thread code, in the same file as their caller,
    never scanned. A hand-maintained list of extra names would have the same
    defect one refactor later, so the reachable set is computed instead.

    The walk is same-file and transitive, which is the honest scope: this lint
    reads text, and a callee whose definition lives in another translation unit
    is not text it has. Those are reached by the runtime tiers (RTSan, the
    allocation guard) instead -- the three tiers of ADR-0029 cover different
    blind spots on purpose.
    """
    index = definition_index(clean)
    seen = set()
    queue = []
    for name, start, end in audio_bodies(clean):
        if (start, end) in seen:
            continue
        seen.add((start, end))
        queue.append((name, name, start, end))

    while queue:
        label, _name, start, end = queue.pop(0)
        yield label, start, end
        for callee in callees(clean[start:end]):
            for cs, ce in index.get(callee, ()):
                if (cs, ce) in seen:
                    continue
                seen.add((cs, ce))
                # The label records HOW the audio thread gets here, so a finding
                # in a helper names the path rather than just the helper.
                queue.append((f"{label} -> {callee}", callee, cs, ce))


def scan_text(text: str, path: str):
    """Return a list of (path, line, function, violation-class, source-line)."""
    clean = strip_comments_and_strings(text)
    raw_lines = text.splitlines()
    findings = []
    for name, start, end in reachable_bodies(clean):
        segment = clean[start:end]
        base_line = clean.count("\n", 0, start) + 1
        for pattern, label in FORBIDDEN:
            for hit in pattern.finditer(segment):
                line_no = base_line + segment.count("\n", 0, hit.start())
                src = raw_lines[line_no - 1].strip() if line_no - 1 < len(raw_lines) else ""
                findings.append((path, line_no, name, label, src))
    return findings


def scan_repo() -> int:
    findings, scanned = [], 0
    for d in SCAN_DIRS:
        for p in sorted((ROOT / d).rglob("*")):
            if p.suffix not in (".cpp", ".h"):
                continue
            scanned += 1
            findings += scan_text(p.read_text(encoding="utf-8"),
                                  str(p.relative_to(ROOT)))
    for path, line, fn, label, src in findings:
        print(f"{path}:{line}: {label} inside audio-path `{fn}`\n    {src}",
              file=sys.stderr)
    if findings:
        print(f"check-realtime: {len(findings)} violation(s) of "
              f"REALTIME_AUDIO_POLICY in {scanned} scanned file(s)", file=sys.stderr)
        return 1
    print(f"check-realtime: {scanned} file(s) scanned, 0 violation(s).")
    return 0


# ---------------------------------------------------------------------------
#  Self-test: both directions.
# ---------------------------------------------------------------------------
MUST_FIRE = [
    ("new in process",
     "void Engine::process (Buf& b) noexcept { float* x = new float[4]; use(x); }"),
    # ---- the allocation forms THIS codebase actually writes -----------------
    # Each of these was invisible to the lint until 2026-08-18, which mattered
    # because they are not exotic alternatives to `new` -- they are the only
    # spellings the DSP modules and the engine use.
    ("assign in process is how every DSP module allocates",
     "void Engine::process (Buf& b) noexcept { scratch.assign (256, 0.0f); }"),
    ("assign in reset -- the likeliest regression, a line copied out of prepare",
     "void Engine::reset() noexcept { delayLine.assign (n, 0.0f); }"),
    ("insert in process",
     "void Engine::process (Buf& b) noexcept { pending.insert (pending.end(), 1); }"),
    ("make_unique in process is `new` the `new` pattern cannot see",
     "void Engine::process (Buf& b) noexcept { os2 = std::make_unique<Over> (2); }"),
    ("make_shared in a module reset",
     "void Mod::reset() noexcept { shared = std::make_shared<State>(); }"),
    # ---- the CLOSURE: a helper is audio-path code even when its name is not --
    ("a same-file helper called from process is scanned too",
     "void Engine::updateDerived() { weights.assign (16, 0.0f); }\n"
     "void Engine::process (Buf& b) noexcept { updateDerived(); }"),
    ("...transitively, through a second hop",
     "void Engine::innermost() { buf.resize (8); }\n"
     "void Engine::middle() { innermost(); }\n"
     "void Engine::process (Buf& b) noexcept { middle(); }"),
    ("...and through a MEMBER call, which is how modules are reached",
     "void Velvet::updateWeights() noexcept { w.assign (32, 0.0f); }\n"
     "void Velvet::processBlock (Buf& b) noexcept { velvet.updateWeights(); }"),
    ("...including a helper that takes no arguments at all",
     "void Engine::noArgs() { v.reserve (64); }\n"
     "void Engine::process (Buf& b) noexcept { noArgs(); }"),
    # ---- the lexer, on the two shapes that would blank the WRONG SPAN -------
    # These are deliberately MUST_FIRE rather than must-stay-silent, and that is
    # the whole point: the danger from a mis-lexed quote is not a false finding,
    # it is a REAL violation swallowed. Both constructs leave an unbalanced
    # quote behind under a line-oriented scanner, which blanks everything to the
    # next quote -- here, to end of input. The allocation after each one is what
    # proves the scanner recovered. (Verified non-vacuous: run through the
    # previous stripper both cases report zero.)
    ("an unbalanced quote inside a raw string must not swallow the code after it",
     'void Engine::process (Buf& b) noexcept { log (R"(")"); v.assign (4, 0.0f); }'),
    ("a digit separator must not open a literal that swallows the code after it",
     "void Engine::process (Buf& b) noexcept { const int k = 1'000; v.assign (k, 0.0f); }"),
    # ...and the four ENCODED character literals, which have the same shape as a
    # digit separator (alphanumeric on both sides of the opening quote) and were
    # mis-read as one until 2026-08-19: the opener was emitted as code, the
    # CLOSER was then taken for an opener, and the rest of the line -- the
    # allocation below -- disappeared. Verified non-vacuous: through the previous
    # `_is_digit_separator` all four report zero violations.
    ("a wide character literal must not swallow the code after it",
     "void Engine::process (Buf& b) noexcept { if (c == L'x') v.assign (4, 0.0f); }"),
    ("...nor a UTF-8 one",
     "void Engine::process (Buf& b) noexcept { if (c == u8'x') v.assign (4, 0.0f); }"),
    ("...nor a UTF-16 one",
     "void Engine::process (Buf& b) noexcept { if (c == u'x') v.assign (4, 0.0f); }"),
    ("...nor a UTF-32 one",
     "void Engine::process (Buf& b) noexcept { if (c == U'x') v.assign (4, 0.0f); }"),
    # ---- the BODY EXTRACTOR, on the distance it used to give up at ----------
    # The brace search stopped 400 characters past the closing parenthesis, and
    # blanked comments keep their length, so a definition with a long comment
    # between `)` and `{` silently left the scanned set. MUST_FIRE for the same
    # reason as the lexer cases above: the symptom is a swallowed violation, not
    # a false one. (Verified non-vacuous: with the 400-character window this
    # case reports zero, and the same fixture with a SHORT comment reports one.)
    ("a definition whose brace is far from its signature is still scanned",
     "void Engine::process (Buf& b) noexcept\n"
     "// " + "z" * 500 + "\n"
     "{ v.assign (4, 0.0f); }"),
    ("malloc in process",
     "void Engine::process (Buf& b) noexcept { void* p = malloc (64); }"),
    ("vector growth in process",
     "void Engine::process (Buf& b) noexcept { v.push_back (1.0f); }"),
    ("setSize in process",
     "void Engine::process (Buf& b) noexcept { scratch.setSize (2, n); }"),
    ("lock in process",
     "void Engine::process (Buf& b) noexcept { std::lock_guard<std::mutex> g (m); }"),
    ("JUCE lock in processBlock",
     "void Mod::processBlock (float* l, float* r, int n) noexcept { juce::ScopedLock sl (cs); }"),
    ("file IO in process",
     "void Engine::process (Buf& b) noexcept { juce::File f (\"/tmp/x\"); }"),
    ("thread in process",
     "void Engine::process (Buf& b) noexcept { std::thread t (work); }"),
    ("resize in a nested block of process",
     "void Engine::process (Buf& b) noexcept { if (x) { for (int i=0;i<n;++i) { v.resize (i); } } }"),
    # The policy binds every DSP module's `reset` as well as its `process`, and
    # an earlier revision of this lint exempted `reset` by name -- these two are
    # the cases that would have caught that.
    ("allocation in a module reset",
     "void HaasProcessor::reset() noexcept { bufL.resize (n); }"),
    ("lock in a module reset",
     "void MonoMaker::reset() noexcept { std::lock_guard<std::mutex> g (m); }"),
    ("allocation in LoudnessMatch::softReset (called from process)",
     "void LoudnessMatch::softReset() noexcept { scratch = new double[8]; }"),
    # `AnamorphAudioProcessor::processBlock` is the FIRST function the policy
    # names and it lives in `src/`, not `src/dsp/`.
    ("allocation in the wrapper processBlock",
     "void AnamorphAudioProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&) { v.push_back (1.0f); }"),
]

MUST_STAY_SILENT = [
    ("allocation in prepare is required by the policy",
     "void Engine::prepare (double sr, int n) { scratch.setSize (2, n); buf.resize (n); }"),
    ("prepare stays excluded even when an audio body calls it -- NEVER_FOLLOW",
     "void Engine::prepare (double sr, int n) { buf.assign (n, 0.0f); }\n"
     "void Engine::process (Buf& b) noexcept { prepare (48000.0, 64); }"),
    ("a helper reached only from prepare is not audio-path code",
     "void Engine::sizeBuffers() { buf.assign (64, 0.0f); }\n"
     "void Engine::prepare (double sr, int n) { sizeBuffers(); }"),
    # THE FALSE-POSITIVE CLASS THE CLOSURE CREATED, and the reason
    # `heads_a_definition` exists. `if (pred (x))` is followed by `{` with no
    # intervening `;`, so on the brace test alone it reads as a definition --
    # and the block that follows gets scanned as that "function's" body.
    # Measured on the real tree before the fix: 11 findings, every one a
    # legitimate prepare() allocation attributed to a predicate.
    ("a call in an if-condition is not a definition, whatever follows it",
     "void Engine::process (Buf& b) noexcept { if (isModAlgorithm (a)) { use(b); } }\n"
     "void Engine::prepare (double sr, int n) { scratch.setSize (2, n); }"),
    ("nor is a call in a return statement",
     "bool Engine::process (Buf& b) noexcept { return isModAlgorithm (a); }\n"
     "void Engine::prepare (double sr, int n) { buf.resize (n); }"),
    ("nor is one behind a logical operator",
     "void Engine::process (Buf& b) noexcept { if (x && isModAlgorithm (a)) { }; }\n"
     "void Engine::prepare (double sr, int n) { buf.resize (n); }"),
    ("the CONTENTS of a raw string are not code",
     'void Engine::process (Buf& b) noexcept { log (R"(new float[4] and v.assign (1,2))"); }'),
    ("a .reset() CALL inside prepare is not a reset definition",
     "void Engine::prepare (double sr, int n) { widthSmooth.reset (sr, 0.05); os2->reset(); buf.resize (n); }"),
    ("a ->reset() call from an audio body is a call, not a scanned definition",
     "void Engine::process (Buf& b) noexcept { if (os2) os2->reset(); }"),
    ("the word new inside a comment in process",
     "void Engine::process (Buf& b) noexcept { /* the new-cutoff bank takes over */ active = 1 - active; }"),
    ("the word malloc inside a string literal in process",
     "void Engine::process (Buf& b) noexcept { const char* s = \"malloc is forbidden here\"; use(s); }"),
    ("a line comment naming setSize in process",
     "void Engine::process (Buf& b) noexcept { // scratch.setSize is done in prepare\n  run(); }"),
    ("a DECLARATION of process, not a definition",
     "struct Engine { void process (Buf& b) noexcept; };\nvoid other() { v.resize (4); }"),
    ("a CALL to process from a non-audio function",
     "void host() { engine.process (buf); v.push_back (1); }"),
    ("plain arithmetic in process",
     "void Engine::process (Buf& b) noexcept { for (int i=0;i<n;++i) l[i] = l[i] * g; }"),
    ("std::fill over a pre-sized buffer is explicitly permitted",
     "void Engine::process (Buf& b) noexcept { std::fill (v.begin(), v.end(), 0.0f); }"),
]


def self_test() -> int:
    failures = 0
    total = 0
    for label, src in MUST_FIRE:
        total += 1
        if not scan_text(src, "synthetic.cpp"):
            print(f"self-test FAIL (must fire): {label}", file=sys.stderr)
            failures += 1
    for label, src in MUST_STAY_SILENT:
        total += 1
        found = scan_text(src, "synthetic.cpp")
        if found:
            print(f"self-test FAIL (must stay silent): {label} -> {found}", file=sys.stderr)
            failures += 1
    # The comment/string stripper is the load-bearing half of the silence cases.
    total += 1
    if strip_comments_and_strings('a // new\nb') .strip().splitlines()[0].strip() != "a":
        print("self-test FAIL: comment stripping", file=sys.stderr)
        failures += 1
    if failures:
        print(f"check-realtime: {failures} of {total} self-test case(s) failed", file=sys.stderr)
        return 1
    print(f"check-realtime: self-test passed ({total} cases).")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="Anamorph static realtime lint.")
    ap.add_argument("--self-test", action="store_true",
                    help="prove the checker still fires and still stays quiet")
    args = ap.parse_args()
    return self_test() if args.self_test else scan_repo()


if __name__ == "__main__":
    sys.exit(main())
