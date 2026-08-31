#!/usr/bin/env bash
# ============================================================================
#  Anamorph -- local preflight: the lint gates + the fast release gate, in one
#  command, before a push spends a CI round trip discovering the same thing.
#
#  WHAT THIS RUNS, in CI's own order (docs/procedures/CI_CD.md §Reproducing CI
#  locally): the seven checkers with their --self-tests first (seconds, no
#  build, historically the most-tripped gates), then the built test suites when
#  a built tree exists.
#
#  WHAT THIS CANNOT RUN, said out loud rather than implied. Three of the seven
#  need something a bare checkout does not have, and each says so instead of
#  passing quietly:
#    * the Clang warning gate needs a fresh clang build log to classify;
#    * the GCC warning gate needs a gcc one, from the pinned major;
#    * the Linux ABI floor needs the LINKED, STRIPPED artifacts.
#  Their --self-tests run here, and the ABI floor additionally runs for real
#  when a built VST3 is present, since that is the one of the three whose input
#  an ordinary local Release build already produces. A green preflight is
#  therefore "the checkers and suites pass", not "CI will be green".
#
#  THE CITATION GATE RUNS THREE TIMES, against every base that can disagree:
#  `origin/main` (the local default and the PR merge-base case), the branch's
#  merge base with it, and `HEAD~1` -- the PUSH PREDECESSOR, which is what CI
#  actually compares and which the other two do NOT approximate once a branch
#  has more than one commit. The third was added after it cost a red run: three
#  anchors drifted from a commit earlier in the same branch, both `origin/main`
#  bases already carried the re-aimed spelling, and preflight went green while
#  `source-lint` did not. Several escape hatches open at once is how a stale
#  anchor has shipped before -- CI_CD.md carries the full reasoning.
#
#  NO SILENT SKIPS. If there is no built tree the suites are SKIPPED WITH A
#  NOTE, never silently -- a preflight that quietly did less than the reader
#  assumed is the defect class scripts/build.sh documents (its gate once
#  "passed" by testing a stale binary).
# ============================================================================
set -euo pipefail

cd "$(dirname "$0")/.."

echo "== preflight: documentation + source lints =="
python3 scripts/check-docs.py --self-test
python3 scripts/check-docs.py
python3 scripts/check-portability.py --self-test
python3 scripts/check-portability.py
python3 scripts/check-realtime.py --self-test
python3 scripts/check-realtime.py
python3 scripts/check-clang-warnings.py --self-test
python3 scripts/check-gcc-warnings.py --self-test
# The toolchain installer's release-identity verifier. Its --self-test drives the
# SAME decision function the install path calls, with recorded strings, so it
# needs no apt, no network and no installed compiler -- including the case that
# used to pass silently: a compiler whose version looks right and whose identity
# cannot be established at all.
./scripts/setup-llvm-apt.sh --self-test
echo "note: the FULL warning gates need a build log from the pinned compiler"
echo "      (CI: linux, linux-lto-tests); only their self-tests ran here."

python3 scripts/check-linux-abi.py --self-test
# The ONE of the three that can also run for real locally: an ordinary Release
# build produces the artifact it reads. Skipped WITH A NOTE when absent, never
# silently -- same rule as the suites below.
ABI_SO="build/Anamorph_artefacts/Release/VST3/Anamorph.vst3/Contents/x86_64-linux/Anamorph.so"
ABI_APP="build/Anamorph_artefacts/Release/Standalone/Anamorph"
ABI_TARGETS=()
[ -f "$ABI_SO" ]  && ABI_TARGETS+=("$ABI_SO")
[ -f "$ABI_APP" ] && ABI_TARGETS+=("$ABI_APP")
if [ ${#ABI_TARGETS[@]} -gt 0 ]; then
    python3 scripts/check-linux-abi.py "${ABI_TARGETS[@]}"
else
    echo "note: no built Linux artifact -- the ABI floor check needs one; only its"
    echo "      self-test ran here. CI runs it on the STRIPPED bytes, which a local"
    echo "      build does not produce anyway."
fi

echo "== preflight: citation gate (all three bases) =="
python3 scripts/check-citations.py --self-test
python3 scripts/check-citations.py --check --base origin/main
MERGE_BASE="$(git merge-base origin/main HEAD 2>/dev/null || true)"
if [ -n "$MERGE_BASE" ] && [ "$MERGE_BASE" != "$(git rev-parse origin/main 2>/dev/null)" ]; then
    python3 scripts/check-citations.py --check --base "$MERGE_BASE"
fi

# THE PUSH PREDECESSOR, which is the base CI ACTUALLY uses and which neither of
# the two above approximates on a branch with more than one commit. `HEAD~1` is
# what `github.event.before` will be for the next push -- and on a branch whose
# merge base IS `origin/main`, both checks above compare against the same commit
# and this is the only one that reads the change since the last push.
#
# Added because it cost a red run: three anchors into `CMakeLists.txt` drifted
# from a commit earlier in the same branch, and both `origin/main` bases already
# carried the re-aimed spelling, so preflight was green and `source-lint` was
# not. That is a false green in the one script whose purpose is to prevent one.
PREV="$(git rev-parse HEAD~1 2>/dev/null || true)"
if [ -n "$PREV" ] && [ "$PREV" != "$MERGE_BASE" ]; then
    echo "-- against the push predecessor ($PREV), which is what CI compares"
    python3 scripts/check-citations.py --check --base "$PREV"
fi

echo "== preflight: test suites =="
if [ -d build/AnamorphTests_artefacts ] && [ -d build/AnamorphStateTests_artefacts ]; then
    scripts/run-tests.sh
else
    echo "note: no built tree at ./build -- the test-suite half of preflight DID NOT RUN."
    echo "      Build first (docs/procedures/BUILD.md), then re-run for the full gate."
fi

echo "== preflight: done =="
