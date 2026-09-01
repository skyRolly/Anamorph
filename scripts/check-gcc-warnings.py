#!/usr/bin/env python3
# ============================================================================
#  check-gcc-warnings.py — the GCC-only first-party warning gate, with a
#  self-test.
#
#  PROVENANCE: the classifier, the baseline format and the compiler-major
#  pinning are the ones `scripts/check-clang-warnings.py` already establishes in
#  this repository. They are restated here rather than imported so that neither
#  gate can be broken by an edit aimed at the other; where the two differ, the
#  difference is deliberate and named in a comment.
#
#  WHY A SECOND WARNING GATE AT ALL. `check-clang-warnings.py` gates first-party
#  diagnostics under Clang, and `juce_recommended_warning_flags` gives Clang the
#  strictly larger default set — which is exactly the argument that makes a GCC
#  gate look redundant. It is not, because "larger" is not "a superset". Measured
#  on this tree, GCC sees three first-party sites Clang does not, and they fall
#  into the two classes GCC is known for:
#
#      src/PluginParameters.cpp:49      parameter shadows a member       -Wshadow
#      src/PluginProcessor.cpp:621      local shadows a member           -Wshadow
#      src/dsp/AnamorphEngine.cpp:1294  'if' does not guard...  -Wmisleading-indentation
#
#  Clang's `-Wshadow-all` structurally does not report a parameter shadowing a
#  member outside a constructor, and Clang has no equivalent of the third at all.
#  (`src/PluginProcessor.cpp` carries a `-Wshadow` entry in the Clang baseline
#  too, but for a DIFFERENT site — the counts are per file, not per line, so the
#  overlap in that one filename is coincidental rather than duplicated coverage.)
#
#  All three sites are benign today, and this gate does NOT ask for them to be
#  fixed: they go in the baseline, exactly as the Clang gate's own accepted set
#  does. The gate exists so the NEXT one is caught on the push that introduces
#  it, on the cheapest runner in the matrix, rather than by a reader.
#
#  THE SET IS DELIBERATELY NARROW, and the two exclusions are the interesting
#  part:
#
#    * `-Wnull-dereference` is NOT gated. It produced four first-party hits, all
#      of the "potential null pointer dereference" kind GCC emits after inlining
#      when it cannot prove a branch unreachable. A four-entry baseline of
#      unprovable warnings is the shape that trains people to regenerate a
#      baseline without reading it, and the baseline diff IS the review.
#    * `-Wmismatched-new-delete` is NOT gated, and this one is structural rather
#      than a matter of taste. `tests/AllocationGuard.h` REPLACES the global
#      `operator new`/`delete`; GCC attributes an allocation to the replaced
#      `operator new[]` and does not follow it through to the allocator call that
#      actually produced the memory, so it reports `free` on "new[] memory"
#      however the deallocators forward among themselves — verified both ways,
#      by funnelling every deallocation through `::operator delete` and by
#      calling `std::free` directly. It is a false positive by construction on
#      the one file whose entire purpose is replacing those operators.
#
#      RE-EXAMINED 2026-08-19, because "put it in the baseline like everything
#      else" is the obvious objection and it is worth answering with numbers
#      rather than with this paragraph. Baselining it does NOT keep the class
#      gated elsewhere, and gating it here does not gate anything at all. Both
#      halves measured on gcc-13.3.0 / Ubuntu 24.04 — the lane's compiler AT
#      THE TIME, not today's: the job has since moved to the floating `gcc:16`
#      container (ANAMORPH_GCC_VERSION in build.yml), so the EMPIRICAL leg below
#      is a measurement on a compiler this lane no longer runs (ER-CI-04,
#      2026-08-31). The exclusion still stands on its STRUCTURAL leg — the flag
#      cannot attribute a first-party site under `-flto`, and AllocationGuard.h
#      is the one file whose purpose is replacing those operators — and the
#      gated set is deliberately unchanged here.
#
#      RE-MEASURED 2026-09-01 (round 3), on every gcc available: BOTH empirical
#      halves reproduce UNCHANGED from 13.3.0 through 15.2.0. Method — a TU that
#      includes this guard, runs selfCheck(), and SEEDS a genuine mismatch
#      (`std::free` on `new double[64]`), compiled `-O2 -Wall -Wextra
#      -Wmismatched-new-delete` with and without `-flto`:
#
#        -flto      : 0 hits on 13.3.0 AND 15.2.0 — the seeded REAL mismatch is
#                     not reported either, so the flag still cannot fail in the
#                     lane that reads the log.
#        no -flto   : 4 hits on both, and the false positive and the seeded real
#                     mismatch are BOTH attributed to AllocationGuard.h:350:69,
#                     so a per-file baseline would still mask a real bug.
#
#      MEASURED ON gcc-16 2026-09-01 (round 4), closing the item round 3 left
#      open. Toolchain: g++-16 (Ubuntu 16-20260315-1ubuntu1~24~ppa1) 16.0.1
#      experimental, trunk r16-8100. Same TU, same flags:
#
#        -flto      : 0 hits -- unchanged from 13.3.0 and 15.2.0, and the seeded
#                     REAL mismatch is still not reported, so the flag STILL
#                     cannot fail in the lane that reads the log.
#        no -flto   : 6 hits (13.3.0 and 15.2.0 both give 4 -- gcc-16 diagnoses
#                     more of the same construct), and AllocationGuard.h:350:69
#                     is STILL among the sites, so the false positive and the
#                     seeded real mismatch still collapse to one path:line:col
#                     and a per-file baseline would still mask a real bug.
#
#      Conclusion unchanged on the lane's actual compiler: BOTH empirical legs
#      hold, the structural leg is compiler-independent, the exclusion stays and
#      no baseline is widened. The rise 4 -> 6 is recorded because it is the kind
#      of drift a future round should re-measure rather than re-argue.
#
#      The reproduction, for re-running it on a later gcc:
#
#        docker run --rm -v "$PWD:/w" -w /w gcc:16 bash -c '
#          printf "%s\n" "#include \"AllocationGuard.h\"" "#include <cstdlib>" \
#            "void u(){auto s=anamorph::testing::selfCheck();(void)s;" \
#            "double*a=new double[64];delete[] a;" \
#            "double*b=new double[64];std::free(b);}" "int main(){u();}" > /tmp/ag.cpp
#          for f in "" "-flto"; do
#            g++ -std=c++23 -O2 $f -Wall -Wextra -Wmismatched-new-delete \
#                -Itests /tmp/ag.cpp -o /dev/null 2>&1 |
#              grep -c -- -Wmismatched-new-delete
#          done'
#
#      Expected on gcc-16: 6 then 0 (4 then 0 on 13-15). A NON-ZERO second number
#      is the only result that would retire the exclusion's first empirical leg —
#      with the flag appended to the gated set and the two gated targets built
#      exactly as the baseline header prescribes:
#
#        1. UNDER `-flto` THE FLAG EMITS NOTHING. The whole two-target build
#           produced ZERO `-Wmismatched-new-delete` lines, first-party and
#           vendored alike — and so did two builds with a genuine `free` on
#           `new[]` memory SEEDED in: once into `tests/dsp_tests.cpp`, and once
#           into `tests/state_tests.cpp` called from `main()` so nothing could
#           elide it (`AnamorphStateTests` relinked, 0 hits, exit 0). The
#           dsp_tests seed in the same translation unit with `-flto` removed
#           produces 3. This job compiles
#           everything `-flto`, which is the whole point of the lane, so adding
#           the flag to GATED_FLAGS adds a flag that cannot fire in the job that
#           reads the log — "a gate that cannot fail is indistinguishable from a
#           gate that passes" (TESTING_POLICY rule 4), which is worse than an
#           exclusion that says so.
#        2. THE ATTRIBUTION MAKES A PER-FILE BASELINE A MASK. Without `-flto`
#           the diagnostic lands on the guard's deallocator —
#           `tests/AllocationGuard.h:351:69`, `operator delete (void*,
#           std::size_t)` — for the false positive AND for the seeded real
#           mismatch alike, because that is where the `free` is. `scan()`
#           deduplicates by `path:line:col`, so both collapse to ONE site and a
#           `1 -Wmismatched-new-delete tests/AllocationGuard.h` line would
#           accept the real one. That is the "a NEW instance fails" property the
#           baseline exists for, absent for this flag specifically.
#
#      So the exclusion stays, and it is an exclusion rather than a baseline
#      entry on purpose. The only shape that would gate this class honestly is a
#      NON-LTO GCC compile of the same sources, which is a lane this pipeline
#      does not have; adding one is a workflow decision, not a flag list edit.
#
#  `-Wduplicated-cond`, `-Wduplicated-branches` and `-Wlogical-op` produce ZERO
#  first-party hits today and are gated precisely because of that: they cost
#  nothing now and guard real classes only GCC can see.
#
#  HOW IT DECIDES WHAT IS FIRST-PARTY. The same STRUCTURAL rule as the Clang
#  gate, and for the same reason: every diagnostic's path is resolved (absolute
#  as-is, relative against the build directory) and asked whether it lands under
#  <root>/{src,tests} without passing through a `_deps` component. That answer
#  does not depend on how the build system spelled the path, so a generator
#  change or a compile wrapper cannot silently turn the gate into a step that
#  passes forever with zero coverage.
#
#  SELF-TEST (`--self-test`), per TESTING_POLICY rule 4. Runs the real parser,
#  the real classifier and the real baseline comparison over synthetic input, in
#  both directions: lines that must be counted, lines that must be ignored
#  (JUCE under `_deps`, notes, flags outside the gated set), the dedup rule, a
#  baseline round-trip including the recorded major, and the two comparison
#  directions.
# ============================================================================

import argparse
import contextlib
import io
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_BASELINE = ROOT / "scripts" / "gcc-warning-baseline.txt"

# The gated set lives in ONE place so the workflow and the baseline cannot drift
# apart: `.github/workflows/build.yml` reads it via `--print-flags` rather than
# restating the flags, which is the same single-authority rule
# ANAMORPH_CLANG_VERSION follows.
GATED_FLAGS = [
    "-Wshadow",
    "-Wmisleading-indentation",
    "-Wduplicated-cond",
    "-Wduplicated-branches",
    "-Wlogical-op",
]

FIRST_PARTY_DIRS = ("src", "tests")

DIAG = re.compile(
    r"^(?P<path>[^:\n]+):(?P<line>\d+):(?P<col>\d+):\s+warning:\s+"
    r"(?P<msg>.*?)\s*\[(?P<flag>-W[A-Za-z0-9=+-]+)\]\s*$")

BASELINE_GCC_MAJOR = re.compile(r"^#\s*gcc-major:\s*(\d+)\s*$")


def classify(path: str, root: Path, build_dir: Path):
    """Return the repo-relative path when `path` is first-party, else None.

    Resolution, not string matching: an absolute path is taken as-is, a relative
    one resolves against the directory the compiler ran in. A `_deps` component
    anywhere in the resolved path is vendored, whatever the spelling.
    """
    p = Path(path)
    if not p.is_absolute():
        p = build_dir / p
    try:
        real = p.resolve()
    except OSError:
        return None
    if "_deps" in real.parts:
        return None
    try:
        rel = real.relative_to(root.resolve())
    except ValueError:
        return None
    if not rel.parts or rel.parts[0] not in FIRST_PARTY_DIRS:
        return None
    return rel.as_posix()


def scan(text: str, root: Path, build_dir: Path):
    """Return (counts, sites, other).

    `counts` is {(relative path, flag): distinct-site count}, `sites` the
    matching `path:line:col` strings so a regression can name its lines, and
    `other` the number of GATED-FLAG diagnostics that landed outside first-party
    paths. `other` is REPORTED, never gated on — a blanket -Werror over JUCE's
    own module sources would be switched off at the first bump — but printing it
    is what tells an operator the log was really parsed.
    """
    seen = set()
    other = 0
    for raw in text.splitlines():
        m = DIAG.match(raw.strip())
        if m is None:
            continue
        if m.group("flag") not in GATED_FLAGS:
            continue
        rel = classify(m.group("path"), root, build_dir)
        if rel is None:
            other += 1
            continue
        seen.add((rel, m.group("flag"), m.group("line"), m.group("col")))
    counts, sites = {}, {}
    for rel, flag, line, col in seen:
        counts[(rel, flag)] = counts.get((rel, flag), 0) + 1
        sites.setdefault((rel, flag), []).append(f"{rel}:{line}:{col}")
    for key in sites:
        sites[key].sort()
    return counts, sites, other


def read_baseline(path: Path):
    """Parse a baseline into ({(relative path, flag): count}, gcc_major).

    A MISSING file is an empty baseline, not an error: that is the strict
    reading (every warning is new), so forgetting the file cannot weaken the
    gate. An UNPARSEABLE file IS an error — treating a corrupt baseline as empty
    would turn one mistake into a wall of false findings, and treating it as
    universal would turn it into no gate at all.
    """
    if not path.exists():
        return {}, None
    out, major = {}, None
    for lineno, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        version = BASELINE_GCC_MAJOR.match(raw.strip())
        if version:
            major = int(version.group(1))
            continue
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        fields = line.split(None, 2)
        if len(fields) != 3 or not fields[0].isdigit():
            raise ValueError(
                f"{path}:{lineno}: expected '<count> <flag> <path>', got {raw.strip()!r}")
        out[(fields[2], fields[1])] = int(fields[0])
    return out, major


BASELINE_HEADER = """\
# check-gcc-warnings baseline -- ACCEPTED first-party GCC-only warnings.
#
# One line per (warning flag, source path):  <count>  <flag>  <path>
# `count` is the number of DISTINCT sites (path:line:col), deduplicated across
# translation units. Line numbers are deliberately NOT recorded: they drift on
# every unrelated edit above a warning, and a baseline that fails on changes
# which introduced nothing gets regenerated blindly, which accepts whatever else
# appeared alongside.
#
# THIS FILE DESCRIBES ONE COMPILER. -Wmisleading-indentation's heuristic and
# -Wshadow's treatment of members have both moved between GCC majors, so these
# counts mean nothing against a different one. The `gcc-major` line below records
# which, and the gate refuses to run against a log from another.
#
# This is a DEBT LIST, not a permission list. The gated flag set lives in
# scripts/check-gcc-warnings.py, which also records why -Wnull-dereference and
# -Wmismatched-new-delete are excluded.
#
# Regenerate with the SAME build the gate runs against -- the two gated targets
# between them compile every first-party translation unit, and a baseline taken
# from a narrower build under-records:
#   cmake -B build-lto -G Ninja -DCMAKE_BUILD_TYPE=Release \\
#       -DCMAKE_C_COMPILER=gcc-<n> -DCMAKE_CXX_COMPILER=g++-<n> \\
#       -DCMAKE_C_FLAGS="-flto" \\
#       -DCMAKE_CXX_FLAGS="-flto $(python3 scripts/check-gcc-warnings.py --print-flags)" \\
#       -DCMAKE_EXE_LINKER_FLAGS="-flto" -DANAMORPH_BUILD_STANDALONE=OFF
#   cmake --build build-lto --target AnamorphTests AnamorphStateTests 2>&1 | tee gcc-build.log
#   python3 scripts/check-gcc-warnings.py --log gcc-build.log \\
#       --root "$PWD" --build-dir "$PWD/build-lto" \\
#       --gcc-major <n> --write-baseline
# and read the diff before committing it -- that diff is the entire review.
"""


def write_baseline(path: Path, counts, gcc_major) -> int:
    if gcc_major is None:
        print("check-gcc-warnings: --write-baseline needs --gcc-major <n> -- a baseline that "
              "does not say which compiler produced it cannot be checked against one.",
              file=sys.stderr)
        return 2
    with path.open("w", encoding="utf-8") as handle:
        handle.write(BASELINE_HEADER)
        handle.write(f"#\n# gcc-major: {gcc_major}\n")
        for (rel, flag), count in sorted(counts.items(), key=lambda kv: (kv[0][1], kv[0][0])):
            handle.write(f"{count}\t{flag}\t{rel}\n")
    print(f"check-gcc-warnings: wrote {len(counts)} baseline entr(ies) for gcc-{gcc_major} "
          f"to {path}")
    return 0


def compare_to_baseline(counts, baseline):
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
def self_test() -> int:
    import tempfile

    root = Path("/repo")
    build = Path("/repo/build-gcc")
    fails, cases = 0, 0

    # `classify` resolves paths, so the synthetic tree has to exist on disk for
    # the expectations to be exact. It is built once, in a temporary directory,
    # and every case is spelled against it.
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        build = root / "build-gcc"
        for rel in ("src/dsp", "tests", "build-gcc/_deps/juce-src/modules/juce_core"):
            (root / rel).mkdir(parents=True, exist_ok=True)
        for rel in ("src/PluginParameters.cpp", "src/PluginProcessor.cpp",
                    "src/dsp/AnamorphEngine.cpp", "src/dsp/ScopeBuffer.h",
                    "tests/dsp_tests.cpp", "tests/AllocationGuard.h",
                    "build-gcc/_deps/juce-src/modules/juce_core/juce_core.cpp"):
            (root / rel).touch()

        def case(label, log, want_keys):
            nonlocal fails, cases
            cases += 1
            counts, _sites, _other = scan(log, root, build)
            got = sorted(counts)
            if got != sorted(want_keys):
                print(f"self-test FAIL: {label}\n  want {sorted(want_keys)}\n  got  {got}",
                      file=sys.stderr)
                fails += 1

        # MUST COUNT.
        case("absolute first-party -Wshadow",
             f"{root}/src/PluginParameters.cpp:49:83: warning: declaration of 'automatable' "
             f"shadows a member of '{{anonymous}}::RawBool' [-Wshadow]",
             [("src/PluginParameters.cpp", "-Wshadow")])
        case("first-party -Wmisleading-indentation",
             f"{root}/src/dsp/AnamorphEngine.cpp:1294:9: warning: this 'if' clause does not "
             f"guard... [-Wmisleading-indentation]",
             [("src/dsp/AnamorphEngine.cpp", "-Wmisleading-indentation")])
        case("first-party tests/ path",
             f"{root}/tests/dsp_tests.cpp:10:1: warning: duplicated cond [-Wduplicated-cond]",
             [("tests/dsp_tests.cpp", "-Wduplicated-cond")])
        # A RELATIVE path, the spelling a relative-path generator emits: it must
        # resolve against the build directory and still land first-party.
        case("relative ../src path resolved against the build dir",
             "../src/PluginProcessor.cpp:621:14: warning: declaration of 'params' shadows a "
             "member of 'AnamorphAudioProcessor' [-Wshadow]",
             [("src/PluginProcessor.cpp", "-Wshadow")])

        # MUST IGNORE.
        case("JUCE under _deps",
             f"{root}/build-gcc/_deps/juce-src/modules/juce_core/juce_core.cpp:5:1: warning: "
             f"shadows [-Wshadow]", [])
        case("a flag outside the gated set",
             f"{root}/src/dsp/AnamorphEngine.cpp:129:42: warning: comparing floating-point "
             f"[-Wfloat-equal]", [])
        case("-Wmismatched-new-delete is deliberately not gated",
             f"{root}/tests/AllocationGuard.h:234:69: warning: 'void free(void*)' called on "
             f"pointer returned from 'operator new []' [-Wmismatched-new-delete]", [])
        case("-Wnull-dereference is deliberately not gated",
             f"{root}/src/PluginProcessor.cpp:1829:23: warning: potential null pointer "
             f"dereference [-Wnull-dereference]", [])
        case("a note line, not a warning",
             f"{root}/src/PluginProcessor.cpp:12:1: note: shadowed declaration is here", [])
        case("a path outside the repository",
             "/usr/include/c++/13/vector:100:1: warning: shadows [-Wshadow]", [])

        # DEDUP: the same site reported from two TUs is ONE site; two sites in
        # one file are TWO.
        cases += 1
        counts, sites, other = scan(
            f"{root}/src/dsp/ScopeBuffer.h:76:59: warning: shadows [-Wshadow]\n"
            f"{root}/src/dsp/ScopeBuffer.h:76:59: warning: shadows [-Wshadow]\n"
            f"{root}/src/dsp/ScopeBuffer.h:88:12: warning: shadows [-Wshadow]\n"
            f"{root}/build-gcc/_deps/juce-src/modules/juce_core/juce_core.cpp:5:1: "
            f"warning: shadows [-Wshadow]\n", root, build)
        if counts.get(("src/dsp/ScopeBuffer.h", "-Wshadow")) != 2 or other != 1:
            print(f"self-test FAIL: dedup/other -> counts={counts} other={other}", file=sys.stderr)
            fails += 1
        elif sites[("src/dsp/ScopeBuffer.h", "-Wshadow")] != \
                ["src/dsp/ScopeBuffer.h:76:59", "src/dsp/ScopeBuffer.h:88:12"]:
            print(f"self-test FAIL: sites not recorded -> {sites}", file=sys.stderr)
            fails += 1

        # BASELINE ROUND-TRIP, including the recorded major. Without this the
        # `# gcc-major:` line could stop being written, or stop being parsed,
        # and the version guard below would silently become a no-op.
        cases += 1
        bl = root / "baseline.txt"
        # Quiet: what these two calls PRINT is the function under test talking
        # about a temporary file, and on a terminal it reads as a report about
        # this repository's real baseline.
        with contextlib.redirect_stdout(io.StringIO()):
            write_baseline(bl, {("src/a.cpp", "-Wshadow"): 2}, 14)
        rt, major = read_baseline(bl)
        if rt != {("src/a.cpp", "-Wshadow"): 2} or major != 14:
            print(f"self-test FAIL: baseline round-trip -> {rt}, major={major}", file=sys.stderr)
            fails += 1

        # An UNPARSEABLE baseline must raise, not read as empty.
        cases += 1
        bad = root / "bad.txt"
        bad.write_text("not a baseline line at all\n", encoding="utf-8")
        try:
            read_baseline(bad)
            print("self-test FAIL: an unparseable baseline was accepted", file=sys.stderr)
            fails += 1
        except ValueError:
            pass

        # --write-baseline must REFUSE without a major.
        cases += 1
        with contextlib.redirect_stderr(io.StringIO()):
            refused = write_baseline(root / "nomajor.txt", {}, None)
        if refused != 2:
            print("self-test FAIL: --write-baseline accepted a missing major", file=sys.stderr)
            fails += 1

        # BOTH comparison directions.
        cases += 1
        reg, imp = compare_to_baseline({("src/a.cpp", "-Wshadow"): 2},
                                       {("src/a.cpp", "-Wshadow"): 1})
        if not reg or imp:
            print(f"self-test FAIL: a rising count did not register -> {reg}, {imp}",
                  file=sys.stderr)
            fails += 1
        cases += 1
        reg, imp = compare_to_baseline({("src/a.cpp", "-Wshadow"): 0},
                                       {("src/a.cpp", "-Wshadow"): 1})
        if reg or not imp:
            print(f"self-test FAIL: a falling count did not register -> {reg}, {imp}",
                  file=sys.stderr)
            fails += 1
        cases += 1
        reg, _ = compare_to_baseline({("src/new.cpp", "-Wlogical-op"): 1}, {})
        if not reg:
            print("self-test FAIL: a file absent from the baseline was not a regression",
                  file=sys.stderr)
            fails += 1

    if fails:
        print(f"check-gcc-warnings: {fails} of {cases} self-test case(s) failed", file=sys.stderr)
        return 1
    print(f"check-gcc-warnings: self-test passed ({cases} cases).")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="Anamorph GCC-only first-party warning gate.")
    ap.add_argument("--log", type=Path, help="GCC output to scan")
    ap.add_argument("--root", type=Path, default=Path(os.getcwd()),
                    help="repository checkout root (default: cwd)")
    ap.add_argument("--build-dir", type=Path, default=None,
                    help="directory the compiler ran in; relative diagnostic paths resolve "
                         "against it (default: --root)")
    ap.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE,
                    help="accepted-warnings baseline (default: scripts/gcc-warning-baseline.txt)")
    ap.add_argument("--write-baseline", action="store_true",
                    help="rewrite the baseline from this log instead of checking against it")
    ap.add_argument("--gcc-major", type=int, default=None,
                    help="the GCC major version that produced the log; must match the "
                         "`gcc-major` line recorded in the baseline")
    ap.add_argument("--self-test", action="store_true",
                    help="prove the classifier and the baseline comparison are live, then exit")
    ap.add_argument("--print-flags", action="store_true",
                    help="print the gated flag set (the workflow reads it from here)")
    args = ap.parse_args()

    if args.self_test:
        return self_test()
    if args.print_flags:
        print(" ".join(GATED_FLAGS))
        return 0
    if args.log is None:
        ap.error("--log is required unless --self-test or --print-flags is given")

    build_dir = args.build_dir or args.root
    counts, sites, other = scan(
        args.log.read_text(encoding="utf-8", errors="replace"), args.root, build_dir)

    if args.write_baseline:
        return write_baseline(args.baseline, counts, args.gcc_major)

    try:
        baseline, baseline_major = read_baseline(args.baseline)
    except ValueError as exc:
        print(f"check-gcc-warnings: {exc}", file=sys.stderr)
        return 2

    # THE COMPILER MUST MATCH THE ONE THE BASELINE DESCRIBES, in both directions,
    # for the reason the Clang gate spells out at length: counts recorded for one
    # major say nothing about another's output, so a mismatch is not a weaker
    # comparison but a meaningless one. Exit 2 — THE CHECK could not run — never
    # the 1 that means the tree regressed.
    if args.gcc_major is not None:
        if baseline_major is None:
            print(f"check-gcc-warnings: {args.baseline} records no `# gcc-major:` line, so it "
                  f"cannot be confirmed to describe gcc-{args.gcc_major}. Regenerate it with "
                  f"--gcc-major {args.gcc_major} --write-baseline and read the diff.",
                  file=sys.stderr)
            return 2
        if baseline_major != args.gcc_major:
            print(f"check-gcc-warnings: baseline describes gcc-{baseline_major} but the log came "
                  f"from gcc-{args.gcc_major}. Diagnostics move between majors, so these counts "
                  f"do not apply. Bump the runner's GCC and re-baseline in the SAME change, and "
                  f"read the diff.", file=sys.stderr)
            return 2

    regressions, improvements = compare_to_baseline(counts, baseline)

    # Improvements FIRST, so they stay visible on a failing run, and as a NOTICE
    # rather than an error: the commit that FIXES a warning must not be the one
    # that goes red. The message names the likelier local explanation too — an
    # incremental build simply did not recompile the file.
    for (rel, flag), allowed, count in improvements:
        print(f"::notice::{flag} in {rel}: {count} site(s), baseline allows {allowed}. "
              f"If this was a FULL build, shrink scripts/gcc-warning-baseline.txt in this "
              f"change; on an incremental build it usually just means the file was not "
              f"recompiled.")

    if regressions:
        for (rel, flag), allowed, count in regressions:
            for site in sites[(rel, flag)]:
                print(f"{site}: warning: [{flag}]")
            print(f"::error::{flag} in {rel}: {count} site(s), baseline allows {allowed}.")
        total = sum(count - allowed for _, allowed, count in regressions)
        print(f"::error::GCC emitted {total} NEW warning(s) in first-party sources "
              f"({', '.join(d + '/' for d in FIRST_PARTY_DIRS)}). Fix them -- do not widen the "
              f"baseline; it is a debt list, not a permission list.", file=sys.stderr)
        return 1

    accepted = sum(counts.values())
    print(f"check-gcc-warnings: no NEW first-party warnings "
          f"({accepted} accepted site(s) in {len(baseline)} baseline entr(ies); "
          f"{other} gated-flag diagnostic(s) in vendored/other paths, not gated).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
