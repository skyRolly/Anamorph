#!/usr/bin/env python3
"""
Anamorph -- static dispatch lint: every parameter write goes through the bracket.

WHAT IT IS FOR, and why the fix it guards needs it
==================================================
ADR-0036 section 30 (Devin R1390) makes one question askable at runtime: *is this thread inside a
parameter listener's dynamic extent?*  `flushDeferredCommands` refuses to run a blocking deferred
command when the answer is yes, because JUCE holds a parameter's `listenerLock` across the WHOLE
dispatch and a host that pumps its message loop from a listener callback runs everything the pump
delivers with that lock held.

The answer is a `thread_local` depth raised by `anamorph::param`'s four wrappers
(`src/ParameterDispatch.h`).  It is only as good as its COVERAGE: one raw
`p->setValueNotifyingHost (v)` anywhere in `src/` is a dispatch the depth does not see, and the
guard reads zero at exactly the moment it must read one.  Coverage is therefore not something to
remember -- it is this lint.

WHY A TEXT SCAN IS THE RIGHT TOOL HERE, which is not true of every lint
=======================================================================
The property is syntactic.  `setValueNotifyingHost`, `beginChangeGesture` and `endChangeGesture`
are NON-VIRTUAL on `juce::AudioProcessorParameter` (juce_AudioProcessorParameter.h:141, :149,
:156), so there is no dynamic dispatch to follow and no subclass that can intercept them: a call
is a call, spelled out, or it does not exist.  A grep can see all of them, and nothing else can
add one behind a template or a pointer.

WHAT IT CANNOT SEE, stated because the gap is real and is covered elsewhere
===========================================================================
JUCE's OWN attachment writes the parameter from inside `SliderParameterAttachment` and friends --
code that is not in `src/` at all.  Nothing static can bracket that.  It is bracketed at runtime by
`AttachmentWitness`, which already straddles the attachment for ADR-0008 round 18's reasons, and
State test 101 leg J is the regression that proves the straddle balances (it was written after the
straddle leaked 14 raises through `sendInitialUpdate`).  So: the lint owns the code this repository
writes, and leg J owns the code JUCE writes.

HOW THE SCAN WORKS
==================
Comments and string literals are removed first, because this tree names these APIs in prose
constantly -- the `//` mentions outnumber the real calls two to one, and an earlier count that
included them is precisely why round 26 rejected this whole approach as too large (it reported 43
imager call sites where there are 15).  What remains is searched for the MEMBER-CALL spellings
`->name (` and `.name (`; the free-function form `anamorph::param::name (` is what the wrappers
are, and is not a member call, so it is invisible to the pattern rather than specially excused.

`src/ParameterDispatch.h` is the one exempt file: the raw calls inside the wrappers ARE the
wrappers.  There is no other exemption list, and adding one would be the wrong shape -- a write
that genuinely must not raise the depth does not exist, because raising it is never unsafe (it
costs one retry) and not raising it is the deadlock.

SELF-TEST (`--self-test`), per TESTING_POLICY rule 4
====================================================
Runs the real stripper and the real pattern over synthetic sources in BOTH directions: every "must
fire" case is a spelling the lint exists to catch (including one inside a string and one after a
line comment, to prove the stripper is not over-eager the other way), and every "must stay quiet"
case is valid wrapped code an over-eager revision would flag.
"""

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"

# The wrappers live here, so the raw calls here are the point.
EXEMPT = {"src/ParameterDispatch.h"}

# Every entry point that starts a parameter dispatch this plug-in can be inside.
# `replaceState` is in the set because JUCE pushes every parameter out through
# `setValueNotifyingHost` from inside it, so the whole call is a dispatch.
GUARDED = ("setValueNotifyingHost", "beginChangeGesture", "endChangeGesture", "replaceState")

# A MEMBER call only: `x->name (` or `x.name (`. The wrapper form is
# `anamorph::param::name (` -- a free function, no `->` and no `.` before the name -- so it does
# not match, which is how the wrappers escape without an exemption of their own.
MEMBER_CALL = re.compile(r"(?:->|\.)\s*(" + "|".join(GUARDED) + r")\s*\(")


def strip_comments_and_strings(text):
    """Remove //, /* */ and "..." / '...' so prose and diagnostics cannot fire the lint.

    Newlines are PRESERVED (each removed span keeps its line breaks) so that line numbers in a
    report still point at the real line.
    """
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


def scan(text):
    """Return [(line, spelling)] for every unbracketed member call in `text`."""
    stripped = strip_comments_and_strings(text)
    hits = []
    for m in MEMBER_CALL.finditer(stripped):
        line = stripped.count("\n", 0, m.start()) + 1
        hits.append((line, m.group(1)))
    return hits


def scan_tree():
    problems = []
    for path in sorted(SRC.rglob("*")):
        if path.suffix not in (".cpp", ".h") or not path.is_file():
            continue
        rel = path.relative_to(ROOT).as_posix()
        if rel in EXEMPT:
            continue
        for line, spelling in scan(path.read_text(encoding="utf-8")):
            problems.append((rel, line, spelling))
    return problems


def self_test():
    checked = failures = 0

    def check(what, got, want):
        nonlocal checked, failures
        checked += 1
        if got != want:
            failures += 1
            print(f"self-test FAIL: {what}: got {got}, want {want}", file=sys.stderr)

    # --- 1. MUST FIRE: every guarded spelling, through both member forms ----------------------
    for name in GUARDED:
        check(f"a raw `->{name}` fires", len(scan(f"void f() {{ p->{name} (x); }}")), 1)
        check(f"a raw `.{name}` fires", len(scan(f"void f() {{ p.{name} (x); }}")), 1)
        check(f"`->{name}` with spaces fires", len(scan(f"void f() {{ p ->  {name}  ( x ); }}")), 1)

    # --- 2. MUST STAY QUIET: the wrapper form, which is what the tree is supposed to contain ---
    for name in GUARDED:
        check(f"the wrapper `anamorph::param::{name}` is quiet",
              len(scan(f"void f() {{ anamorph::param::{name} (p, x); }}")), 0)
    check("the wrappers' own file is exempt", "src/ParameterDispatch.h" in EXEMPT, True)

    # --- 3. MUST STAY QUIET: prose. This is the case that decided the design ------------------
    # An earlier count of these call sites included comment mentions and reported nearly three
    # times the real number, which is why the approach was rejected a round too early.
    check("a line comment naming the API is quiet",
          len(scan("// beginChangeGesture (p) dispatches to every listener\nvoid f() {}")), 0)
    check("a block comment naming the API is quiet",
          len(scan("/* p->setValueNotifyingHost (v) is what the attachment does */\nvoid f() {}")), 0)
    check("a string literal naming the API is quiet",
          len(scan('void f() { log ("p->endChangeGesture () failed"); }')), 0)
    check("an apostrophe in a comment does not eat the file",
          len(scan("// the parameter's own lock\nvoid f() { p->beginChangeGesture (); }")), 1)

    # --- 4. THE LINE NUMBER IS THE REAL ONE, after stripping ----------------------------------
    src = "// one\n/* two\n   three */\nvoid f() { p->endChangeGesture (); }\n"
    hits = scan(src)
    check("the reported line survives stripping", hits and hits[0][0], 4)

    # --- 5. A NEAR MISS IS NOT A HIT ----------------------------------------------------------
    check("a differently-named member is quiet",
          len(scan("void f() { p->setValueNotifyingHostLater (x); }")), 0)
    check("a declaration is not a call",
          len(scan("struct S { void beginChangeGesture(); };")), 0)

    if failures:
        print(f"\ncheck-dispatch: {failures} of {checked} self-test case(s) failed.", file=sys.stderr)
        return 1
    print(f"check-dispatch: self-test passed ({checked} cases).")
    return 0


def main():
    ap = argparse.ArgumentParser(description="Verify every parameter dispatch is bracketed.")
    ap.add_argument("--self-test", action="store_true",
                    help="verify this tool's own stripper and pattern")
    args = ap.parse_args()

    if args.self_test:
        return self_test()

    problems = scan_tree()
    if problems:
        for rel, line, spelling in problems:
            print(f"::error file={rel},line={line}::`{spelling}` is called directly. "
                  f"Use `anamorph::param::{spelling}` (src/ParameterDispatch.h) so the dispatch "
                  f"raises the extent depth ADR-0036 section 30 relies on.", file=sys.stderr)
        print(f"\ncheck-dispatch: {len(problems)} unbracketed parameter dispatch(es). Every one of "
              f"them is a window in which `flushDeferredCommands` would run a blocking command from "
              f"inside a parameter listener's dynamic extent (Devin R1390).", file=sys.stderr)
        return 1

    scanned = sum(1 for p in SRC.rglob("*") if p.suffix in (".cpp", ".h") and p.is_file())
    print(f"check-dispatch: {scanned} file(s) scanned, every parameter dispatch bracketed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
