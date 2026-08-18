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
    (re.compile(r"\.(resize|push_back|emplace_back|reserve)\s*\("), "container growth"),
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
        elif c in "\"'":
            quote = c
            out.append(" ")
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\" and i + 1 < n:
                    out.append("  ")
                    i += 2
                    continue
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            if i < n:
                out.append(" ")
                i += 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def audio_bodies(clean: str):
    """Yield (function_name, body_start_index, body_end_index) for audio-path
    function DEFINITIONS in the cleaned text.

    A definition is a name followed by a parameter list and then `{` before any
    `;` -- which distinguishes it from a declaration without needing a parser.
    """
    for m in AUDIO_FN.finditer(clean):
        name = m.group(1)
        # Qualified name check: skip call sites like `engine.process (buf)` by
        # requiring the match to be preceded by a type/qualifier, not a `.`/`->`.
        before = clean[max(0, m.start() - 2):m.start()]
        if before.endswith(".") or before.endswith("->"):
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
        rest = clean[k + 1:k + 400]
        brace = rest.find("{")
        semi = rest.find(";")
        if brace < 0 or (0 <= semi < brace):
            continue                      # a declaration, not a definition
        start = k + 1 + brace
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


def scan_text(text: str, path: str):
    """Return a list of (path, line, function, violation-class, source-line)."""
    clean = strip_comments_and_strings(text)
    raw_lines = text.splitlines()
    findings = []
    for name, start, end in audio_bodies(clean):
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
