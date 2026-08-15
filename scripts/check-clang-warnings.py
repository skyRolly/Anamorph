#!/usr/bin/env python3
# ============================================================================
#  check-clang-warnings.py — the first-party warning gate, with a self-test.
#
#  PROVENANCE: adopted from the sibling product Anabasis
#  (`scripts/check-clang-warnings.py`). Adapted here in exactly two places, both
#  named below: `FIRST_PARTY_DIRS` drops `tools` (this repository has no such
#  directory) and the self-test cases are respelled against this tree's files.
#  The classifier itself is unchanged — it is a question about paths, not about
#  either product.
#
#  WHAT IT IS FOR, in this repository. `juce_recommended_warning_flags` picks its
#  set by COMPILER ID, and Clang's set is strictly larger than GCC's:
#  -Wshorten-64-to-32, -Wconditional-uninitialized, -Wsign-conversion,
#  -Wcast-align, -Wshift-sign-overflow, -Wzero-as-null-pointer-constant and
#  -Wimplicit-int-float-conversion are Clang-only here. Every one of those
#  therefore reached this project only from the macOS runner, several minutes
#  into a universal build. The 2026-08-15 image move is the worked example: four
#  pre-existing `-Wimplicit-int-float-conversion` sites in `src/PluginEditor.cpp`,
#  `src/gui/LookAndFeel.cpp` and `src/dsp/VelvetNoise.cpp` had been in the tree
#  for months and surfaced only when AppleClang went 15 → 21 (CI_CD.md §Build
#  matrix). A Linux+Clang job would have shown them on the push that introduced
#  them, on the cheapest runner in the matrix.
#
#  WHY A SCRIPT AND NOT A `grep` IN THE WORKFLOW. The obvious form of this gate
#  is a single anchored expression:
#
#      grep -E "^${GITHUB_WORKSPACE}/(src|tests)/[^:]*:[0-9]+:[0-9]+: warning:"
#
#  That expression WORKS in the configuration this repository builds — CMake's
#  Ninja generator hands Clang absolute source paths here. The defect is not that
#  it never matches. The defect is its SHAPE: it can only match one spelling of a
#  path the build system is free to change, and if that spelling ever changes — a
#  different generator, a compile wrapper, ccache with `base_dir`, a build tree
#  moved outside the checkout — the step keeps printing "no warnings in
#  first-party sources" forever with zero coverage and no signal that it stopped
#  working. A gate that cannot fail is indistinguishable from a gate that passes.
#
#  So the classification here is STRUCTURAL rather than textual. Every
#  diagnostic's path is resolved — absolute taken as-is, relative resolved
#  against the directory the compiler ran in — and then asked one question:
#  does the resulting real path live under <root>/{src,tests} without passing
#  through a `_deps` component? That answer does not depend on how the path was
#  spelled, which is the property the anchored expression lacked.
#
#  WHY THERE IS A BASELINE, and why it is not a way of being lax. The obvious
#  gate — "fail on any first-party warning" — cannot be adopted by this tree,
#  because the tree already HAS first-party Clang warnings: 14 distinct sites when
#  this gate landed, across `-Wshadow-field`, `-Wshadow`, `-Wswitch-enum`,
#  `-Wsign-conversion`, `-Wfloat-equal`, `-Wunused-but-set-variable` and
#  `-Wmissing-prototypes`. They are not new and they are not this change's to fix:
#  clearing them means renaming a member across the editor, adding cases to engine
#  switches and changing float comparisons in DSP code — source work that belongs
#  in its own review, under DSP_POLICY, not in a CI change.
#
#  The two ways out of that are both worse than a baseline. Landing the job
#  RED teaches everyone to ignore it, which is how a gate dies. Landing it
#  NON-BLOCKING makes it a gate that cannot fail, which is the one thing this
#  file's whole rationale is written against. So the assertion is narrowed to
#  the one this repository can actually hold today and that catches the class the
#  job exists for: NO NEW first-party warnings. A warning introduced by a push
#  fails that push, which is the signal that was missing.
#
#  Deliberately keyed on (path, flag) and a COUNT — never on line numbers. A
#  line-keyed baseline goes stale on every unrelated edit above a warning site, so
#  it would fail on changes that introduced nothing, and the reflex fix for that
#  is to regenerate it blindly, which silently accepts whatever else appeared. The
#  count is what stops a file already carrying one `-Wsign-conversion` from
#  absorbing a second for free.
#
#  A pair whose count FALLS, or vanishes, is reported as a ::notice:: and never as
#  a failure. Failing there would turn the commit that FIXED a warning red — the
#  same trap `check-citations.py` documents for a deliberate re-anchor — and the
#  baseline is meant to shrink.
#
#  `--self-test` is the repository's existing answer to "prove the checker is
#  live before trusting its silence" (`check-docs.py --self-test`, and
#  `check-portability.py --compile-canary` for the same reason). It feeds the
#  classifier every path spelling this build could plausibly produce, plus the
#  vendored forms that must NOT count, and exercises the baseline comparison in
#  both directions — a new warning must fail, a fixed one must not.
#
#  Exit codes follow the sibling scripts: 0 clean · 1 NEW first-party warnings
#  found (a real gate failure) · 2 the check itself could not run (self-test
#  failure, missing log, unreadable baseline) — deliberately not the 1 that means
#  "the tree regressed".
# ============================================================================

import argparse
import collections
import os
import re
import sys

# `path:line:col: [warning|error]: text`. The path is non-greedy and forbidden
# from containing a colon-digit run, so a Windows drive letter or a colon inside
# a message cannot be mistaken for the line number.
DIAGNOSTIC = re.compile(r"^(?P<path>.+?):(?P<line>\d+):(?P<col>\d+):\s+warning:(?P<msg>.*)$")

# Clang closes a diagnostic with the flag that produced it: `... [-Wshadow]`.
# Warnings with no flag (a few are unconditional) group under the placeholder
# below rather than being dropped — dropping them would make them invisible to
# both the baseline and the gate.
WARNING_FLAG = re.compile(r"\[(-W[^\]]+)\]\s*$")
NO_FLAG = "(unflagged)"

DEFAULT_BASELINE = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "clang-warning-baseline.txt")

# No `tools` entry: this repository has no such directory (the sibling's list
# carries one because it keeps host-side probes there). Listing a directory that
# does not exist would be harmless at runtime but would make the self-test create
# it, so the gate's declared scope would stop matching the tree it guards.
FIRST_PARTY_DIRS = ("src", "tests")
VENDORED_COMPONENT = "_deps"


def first_party_path(path: str, root: str, base: str):
    """The repo-relative path when `path` names a first-party source file, else None.

    `root` is the repository checkout; `base` is the directory the compiler ran
    in, which is what a relative diagnostic path is relative to (for Ninja that
    is the build directory). Both are resolved so that `..` segments, symlinks
    and a build tree nested inside the checkout all collapse to one answer --
    which is also what folds `src/gui/../dsp/ScopeBuffer.h` and
    `src/dsp/ScopeBuffer.h` (both of which this build emits, from different
    translation units) into ONE baseline key.

    Returns the relative path with forward slashes, so a baseline written on one
    platform reads the same on another.
    """
    resolved = path if os.path.isabs(path) else os.path.join(base, path)
    resolved = os.path.normpath(os.path.realpath(resolved))
    root = os.path.normpath(os.path.realpath(root))

    try:
        rel = os.path.relpath(resolved, root)
    except ValueError:          # different drive on Windows — cannot be ours
        return None

    parts = rel.split(os.sep)
    if parts and parts[0] == os.pardir:
        return None             # outside the checkout entirely
    # A dependency's own `src/` directory is the trap a textual filter needs a
    # second `grep -v` for: `build/_deps/juce-src/…/src/…` starts with `_deps`
    # only after the build directory, so the test is "does ANY component say
    # _deps", not "does it start with one".
    if VENDORED_COMPONENT in parts:
        return None
    if not parts or parts[0] not in FIRST_PARTY_DIRS:
        return None
    return "/".join(parts)


def classify(path: str, root: str, base: str) -> bool:
    """True when `path` names a first-party source file. Kept as the named
    predicate the self-test exercises; `first_party_path` carries the work."""
    return first_party_path(path, root, base) is not None


def scan(log_path: str, root: str, base: str):
    """Return (counts, sites, other_total).

    `counts` maps (relative path, warning flag) -> number of DISTINCT sites, and
    `sites` maps the same key to the sorted `path:line:col` list behind it.
    Distinct means deduplicated on (path, line, col, flag): a header included by
    twelve translation units reports the same warning twelve times, and counting
    those as twelve would make the baseline depend on the target set rather than
    on the source.
    """
    try:
        with open(log_path, "r", errors="replace") as handle:
            lines = handle.read().splitlines()
    except OSError as exc:
        print(f"check-clang-warnings: cannot read {log_path}: {exc}", file=sys.stderr)
        return None, None, None

    seen = set()
    counts = collections.Counter()
    sites = collections.defaultdict(list)
    other = 0
    for line in lines:
        match = DIAGNOSTIC.match(line.strip())
        if match is None:
            continue
        rel = first_party_path(match.group("path"), root, base)
        if rel is None:
            other += 1
            continue
        flag_match = WARNING_FLAG.search(match.group("msg"))
        flag = flag_match.group(1) if flag_match else NO_FLAG
        key = (rel, int(match.group("line")), int(match.group("col")), flag)
        if key in seen:
            continue
        seen.add(key)
        counts[(rel, flag)] += 1
        sites[(rel, flag)].append(f"{rel}:{match.group('line')}:{match.group('col')}")
    for key in sites:
        sites[key].sort()
    return counts, sites, other


# --- baseline ----------------------------------------------------------------

def read_baseline(path: str):
    """Parse a baseline file into {(relative path, flag): count}.

    A MISSING file is an empty baseline, not an error: that is the strict
    reading (every warning is new), so forgetting the file cannot weaken the
    gate. An UNPARSEABLE file IS an error -- silently treating a corrupt
    baseline as empty would turn one mistake into a wall of false findings, and
    silently treating it as universal would turn it into no gate at all.
    """
    if not os.path.exists(path):
        return {}
    out = {}
    with open(path, "r", encoding="utf-8") as handle:
        for lineno, raw in enumerate(handle, 1):
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            fields = line.split(None, 2)
            if len(fields) != 3 or not fields[0].isdigit():
                raise ValueError(
                    f"{path}:{lineno}: expected '<count> <flag> <path>', got {raw.strip()!r}")
            out[(fields[2], fields[1])] = int(fields[0])
    return out


BASELINE_HEADER = """\
# check-clang-warnings baseline -- ACCEPTED first-party Clang warnings.
#
# One line per (warning flag, source path):  <count>  <flag>  <path>
# `count` is the number of DISTINCT sites (path:line:col), deduplicated across
# translation units. Line numbers are deliberately NOT recorded: they drift on
# every unrelated edit above a warning, and a baseline that fails on changes
# which introduced nothing gets regenerated blindly, which accepts whatever else
# appeared alongside.
#
# THIS FILE IS A DEBT LIST, NOT A PERMISSION LIST. Every entry is a warning this
# repository has not fixed yet. The gate fails on anything ABOVE these counts, so
# new warnings cannot be added to a file that already has one; a count that FALLS
# is reported as a notice asking for this file to shrink, never as a failure --
# the commit that fixes a warning must not be the commit that goes red.
#
# Regenerate with:
#   cmake --build build-clang --target ... 2>&1 | tee clang-build.log
#   python3 scripts/check-clang-warnings.py --log clang-build.log \\
#       --root "$PWD" --build-dir "$PWD/build-clang" --write-baseline
# and read the diff before committing it -- that diff is the entire review.
"""


def write_baseline(path: str, counts) -> int:
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(BASELINE_HEADER)
        for (rel, flag), count in sorted(counts.items(), key=lambda kv: (kv[0][1], kv[0][0])):
            handle.write(f"{count}\t{flag}\t{rel}\n")
    print(f"check-clang-warnings: wrote {len(counts)} baseline entr(ies) to {path}")
    return 0


def compare_to_baseline(counts, sites, baseline):
    """Return (regressions, improvements).

    A regression is a (path, flag) whose distinct-site count EXCEEDS the
    baseline -- including every pair the baseline does not mention at all.
    """
    regressions, improvements = [], []
    for key, count in sorted(counts.items()):
        allowed = baseline.get(key, 0)
        if count > allowed:
            regressions.append((key, allowed, count))
    for key, allowed in sorted(baseline.items()):
        count = counts.get(key, 0)
        if count < allowed:
            improvements.append((key, allowed, count))
    return regressions, improvements


# --- self-test ---------------------------------------------------------------
# The cases are written against a synthetic tree so the expectations are exact.
# Every one of them is a spelling this build could produce, or one a plausible
# change to it would: the absolute form CMake emits today, the `../src/…` form a
# relative-path generator emits, the `./` form, and the two vendored shapes that
# must never count no matter how they are spelled.
#
# `tools/` is deliberately absent from the first-party cases and present as a
# NEGATIVE one: this repository has no such directory, so a diagnostic naming it
# would be somebody else's tree. That is the adaptation from the sibling, and it
# is asserted rather than merely implied by the shorter FIRST_PARTY_DIRS.
SELF_TEST_CASES = [
    # (path as the compiler printed it, first-party?, label)
    ("/repo/src/dsp/AnamorphEngine.cpp",            True,  "absolute first-party (what this build emits today)"),
    ("/repo/tests/state_tests.cpp",                 True,  "absolute first-party under tests/"),
    ("/repo/src/gui/LookAndFeel.cpp",               True,  "absolute first-party in a src/ subdirectory"),
    ("/repo/tools/probe.cpp",                       False, "tools/ is not a directory of this repository"),
    ("../src/PluginEditor.cpp",                     True,  "relative first-party from the build dir"),
    ("../../src/PluginEditor.cpp",                  False, "relative escaping the checkout"),
    ("src/dsp/MidSide.h",                           False, "relative to the BUILD dir, so not our src/"),
    ("/repo/build/_deps/juce-src/modules/x.cpp",    False, "vendored, absolute"),
    ("_deps/juce-src/modules/juce_core/src/y.cpp",  False, "vendored whose OWN path contains src/"),
    ("../build/_deps/juce-src/modules/z.cpp",       False, "vendored, relative"),
    ("/elsewhere/src/other.cpp",                    False, "another checkout's src/"),
]


def self_test() -> int:
    import tempfile

    failures = 0
    with tempfile.TemporaryDirectory() as tmp:
        root = os.path.join(tmp, "repo")
        base = os.path.join(root, "build")
        os.makedirs(base)
        for name in FIRST_PARTY_DIRS:
            os.makedirs(os.path.join(root, name), exist_ok=True)

        for raw, expected, label in SELF_TEST_CASES:
            path = raw.replace("/repo", root, 1) if raw.startswith("/repo") else raw
            got = classify(path, root, base)
            if got != expected:
                failures += 1
                print(f"self-test FAIL: {label}: {path!r} -> {got}, want {expected}",
                      file=sys.stderr)

        # The line matcher itself, not just the path classifier: a diagnostic
        # must be recognised and a NON-diagnostic must not be, or the gate would
        # either miss real warnings or fail on prose that mentions "warning:".
        line_cases = [
            ("/repo/src/a.cpp:12:3: warning: unused variable 'x'", True,  "a real warning line"),
            ("/repo/src/a.cpp:12:3: error: no member named 'y'",   False, "an error is not this gate's business"),
            ("/repo/src/a.cpp:12:3: note: expanded from macro",    False, "a note"),
            ("cmake: -- warning: something happened",              False, "prose containing 'warning:'"),
            ("[52/91] Building CXX object src/a.cpp.o",            False, "a progress line"),
        ]
        for raw, expected, label in line_cases:
            got = DIAGNOSTIC.match(raw.replace("/repo", root, 1).strip()) is not None
            if got != expected:
                failures += 1
                print(f"self-test FAIL: {label}: {raw!r} matched={got}, want {expected}",
                      file=sys.stderr)

        # THE BASELINE COMPARISON, IN BOTH DIRECTIONS. This is the half that
        # decides whether the job can fail at all, so asserting the classifier
        # alone would leave the load-bearing part unproven. The flag is extracted
        # from the message here too: a baseline keyed on the wrong flag matches
        # nothing, which reads exactly like a clean tree.
        log = os.path.join(tmp, "build.log")
        with open(log, "w", encoding="utf-8") as handle:
            handle.write(
                f"{root}/src/a.cpp:10:5: warning: declaration shadows a field [-Wshadow]\n"
                f"{root}/src/a.cpp:10:5: warning: declaration shadows a field [-Wshadow]\n"   # same site, 2nd TU
                f"{root}/src/gui/../a.cpp:10:5: warning: declaration shadows a field [-Wshadow]\n"  # same site, other spelling
                f"{root}/src/b.cpp:20:1: warning: unused variable 'q' [-Wunused-variable]\n"
                f"{root}/build/_deps/juce-src/m.cpp:1:1: warning: vendored [-Wshadow]\n")
        counts, sites, other_total = scan(log, root, base)

        baseline_cases = [
            ("a matching baseline is clean",
             {("src/a.cpp", "-Wshadow"): 1, ("src/b.cpp", "-Wunused-variable"): 1}, 0, 0),
            ("an unlisted warning is a regression",
             {("src/a.cpp", "-Wshadow"): 1}, 1, 0),
            ("an empty baseline makes every warning new",
             {}, 2, 0),
            ("a baseline that shrank reports an improvement, not a failure",
             {("src/a.cpp", "-Wshadow"): 1, ("src/b.cpp", "-Wunused-variable"): 1,
              ("src/c.cpp", "-Wshadow"): 3}, 0, 1),
            ("the WRONG FLAG for the right file does not excuse it",
             {("src/a.cpp", "-Wunused-variable"): 1, ("src/b.cpp", "-Wunused-variable"): 1},
             1, 1),
        ]
        for label, baseline, want_reg, want_imp in baseline_cases:
            regressions, improvements = compare_to_baseline(counts, sites, baseline)
            if len(regressions) != want_reg or len(improvements) != want_imp:
                failures += 1
                print(f"self-test FAIL: {label}: {len(regressions)} regression(s) / "
                      f"{len(improvements)} improvement(s), want {want_reg}/{want_imp}",
                      file=sys.stderr)

        # Deduplication is what makes the counts a property of the SOURCE rather
        # than of the target set: three log lines, one site.
        dedup_checks = [
            (counts.get(("src/a.cpp", "-Wshadow")), 1,
             "three spellings of one site count once"),
            (other_total, 1, "the vendored diagnostic is counted as other, not first-party"),
            (len(sites.get(("src/a.cpp", "-Wshadow"), [])), 1, "one recorded site"),
        ]
        for got, want, label in dedup_checks:
            if got != want:
                failures += 1
                print(f"self-test FAIL: {label}: got {got}, want {want}", file=sys.stderr)

    total = len(SELF_TEST_CASES) + len(line_cases) + len(baseline_cases) + len(dedup_checks)
    if failures:
        print(f"\ncheck-clang-warnings: {failures} of {total} self-test case(s) failed.",
              file=sys.stderr)
        return 2
    print(f"check-clang-warnings: self-test passed ({total} cases).")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", help="compiler output to scan")
    parser.add_argument("--root", default=os.getcwd(),
                        help="repository checkout root (default: cwd)")
    parser.add_argument("--build-dir", default=None,
                        help="directory the compiler ran in; relative diagnostic "
                             "paths resolve against it (default: --root)")
    parser.add_argument("--self-test", action="store_true",
                        help="prove the classifier and the baseline comparison are live, then exit")
    parser.add_argument("--baseline", default=DEFAULT_BASELINE,
                        help="accepted-warnings baseline (default: scripts/clang-warning-baseline.txt)")
    parser.add_argument("--write-baseline", action="store_true",
                        help="rewrite the baseline from this log instead of checking against it")
    args = parser.parse_args()

    if args.self_test:
        return self_test()

    if not args.log:
        parser.error("--log is required unless --self-test is given")

    base = args.build_dir or args.root
    counts, sites, other = scan(args.log, args.root, base)
    if counts is None:
        return 2

    if args.write_baseline:
        return write_baseline(args.baseline, counts)

    try:
        baseline = read_baseline(args.baseline)
    except ValueError as exc:
        print(f"check-clang-warnings: {exc}", file=sys.stderr)
        return 2

    regressions, improvements = compare_to_baseline(counts, sites, baseline)

    # Improvements FIRST, so they are visible even on a failing run: they are the
    # direction the baseline is supposed to move, and burying them under an error
    # is how a debt list stops shrinking.
    #
    # THE MESSAGE NAMES THE OTHER EXPLANATION, and it is the likelier one locally.
    # A count can fall for two reasons: the warning was fixed, or the log simply
    # does not contain the translation unit that carries it -- which is what an
    # INCREMENTAL rebuild produces, since ninja only recompiles what changed. CI
    # always builds from a fresh checkout so its logs are complete, but the first
    # thing a developer does is rebuild locally, and a notice that says only
    # "shrink the baseline" would talk them into deleting entries for warnings
    # that are still there. It stays a notice either way: failing here would turn
    # the commit that FIXED a warning red.
    for (rel, flag), allowed, count in improvements:
        print(f"::notice::{flag} in {rel}: {count} site(s), baseline allows {allowed}. "
              f"If this was a FULL build, shrink scripts/clang-warning-baseline.txt in this "
              f"change; on an incremental build it usually just means the file was not "
              f"recompiled.")

    if regressions:
        for (rel, flag), allowed, count in regressions:
            for site in sites[(rel, flag)]:
                print(f"{site}: warning: [{flag}]")
            print(f"::error::{flag} in {rel}: {count} site(s), baseline allows {allowed}.")
        total = sum(count - allowed for _, allowed, count in regressions)
        print(f"::error::Clang emitted {total} NEW warning(s) in first-party sources "
              f"({', '.join(d + '/' for d in FIRST_PARTY_DIRS)}). Fix them -- do not widen the "
              f"baseline; it is a debt list, not a permission list.", file=sys.stderr)
        return 1

    # The dependency count is REPORTED, never gated on: a blanket -Werror over
    # JUCE's own module sources would be switched off at the first bump. Printing
    # it is what tells an operator the log was actually parsed — a zero here
    # alongside a large build is the shape that says "look at the log", which a
    # silent `grep` could not say. The accepted count is printed for the same
    # reason: a baseline nobody is reminded of is a baseline nobody shrinks.
    accepted = sum(counts.values())
    print(f"check-clang-warnings: no NEW first-party warnings "
          f"({accepted} accepted site(s) in {len(baseline)} baseline entr(ies); "
          f"{other} diagnostic(s) in vendored/other paths, not gated).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
