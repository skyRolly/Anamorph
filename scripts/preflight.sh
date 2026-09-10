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
#
#  THE EXIT STATUS IS THE RESULT, AND THIS SCRIPT FAILS FAST. `set -e` above
#  means the FIRST failing checker ends the run, so a non-zero exit also means
#  every stage after it did not run at all -- an early failure is not "one
#  finding in an otherwise green preflight", it is an UNKNOWN result for
#  everything below it. Read the exit status, not a filtered view of the
#  output: piping this script through `grep` replaces its status with grep's
#  and can hide a finding whose wording the pattern did not anticipate. That
#  is not hypothetical -- it is exactly how round 21 reported a green preflight
#  while `check-docs` (the SECOND command here) was failing on a line that
#  began with a `|`, and the docs job went red on the push.
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
# The release gate's own verdict. `classify_pass_exit` decides whether a non-zero
# pluginval exit is a crash or a real validation failure, from recorded strings
# and a stand-in validator, so it needs no bundle, no pluginval and no display --
# and a green pluginval run proves nothing about it, because a pass takes none of
# its branches.
./scripts/run-pluginval.sh --self-test
echo "note: the FULL warning gates need a build log from the pinned compiler"
echo "      (CI: linux, linux-lto-tests); only their self-tests ran here."

# ...WHICH IS HOW A -Wunused-variable IN tests/state_tests.cpp REACHED CI TWICE.
# The two gates above compare against a baseline stamped with a compiler major and
# REFUSE to run against a different one -- correctly, because diagnostic counts move
# between majors. Locally that means they never run at all (gcc-13 / clang-18 against
# a pinned gcc-16 / clang-22), so a brand-new warning in first-party code is invisible
# until the push builds. A whole class of those is version-INDEPENDENT, though, and
# this is the cheap half: syntax-only the first-party translation units with whatever
# compiler is installed and report anything it says about a file under src/ or tests/.
# Advisory, never fatal -- the authoritative gate is still CI's pinned major, and this
# must not become a second baseline to argue with. It is a smoke alarm, not a gate.
LOCAL_CXX="$(command -v clang++ || command -v g++ || true)"
if [ -n "$LOCAL_CXX" ] && [ -d build/_deps/juce-src/modules ]; then
    echo "== preflight: local first-party warning sweep ($(basename "$LOCAL_CXX")) =="
    SWEEP_LOG="$(mktemp)"
    for TU in src/gui/SpectrumImager.cpp tests/state_tests.cpp; do
        # -Wshadow is NOT in -Wall -Wextra, and its absence is why a `-Wshadow` on a loop
        # variable shadowing a function parameter reached CI on 2026-09-10 with this sweep green.
        # The pinned gates carry it; this advisory one now does too.
        "$LOCAL_CXX" -std=c++23 -fsyntax-only -Wall -Wextra -Wshadow \
            -I src -I src/dsp -I src/gui -I build/_deps/juce-src/modules \
            -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJUCE_STANDALONE_APPLICATION=1 \
            -DJUCE_WEB_BROWSER=0 -DJUCE_USE_CURL=0 \
            "$TU" 2>>"$SWEEP_LOG" || true
    done
    if grep -E '^(src|tests)/[^:]+:[0-9]+:[0-9]+: warning:' "$SWEEP_LOG" > /dev/null 2>&1; then
        echo "warning: the local compiler reports first-party warnings. CI gates on the"
        echo "         PINNED major and may disagree, but these are worth reading before"
        echo "         pushing -- a new one here is usually a new one there:"
        grep -E '^(src|tests)/[^:]+:[0-9]+:[0-9]+: warning:' "$SWEEP_LOG" | sort -u | head -40
    else
        echo "local sweep: no first-party warnings from $(basename "$LOCAL_CXX")."
    fi
    rm -f "$SWEEP_LOG"
else
    echo "note: no local compiler or no fetched JUCE -- the local warning sweep was skipped."
fi

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
# WHICH COMMIT IS THE PUSH PREDECESSOR DEPENDS ON WHETHER THE CHANGE SET IS
# COMMITTED YET, and getting that wrong is the same false green from the other
# side. Run after committing, CI will compare the new HEAD against HEAD~1. Run on
# a DIRTY tree -- which is when preflight is most useful, before the commit -- the
# work in hand becomes the next commit and CI will compare it against HEAD. Using
# HEAD~1 there checks one commit too far back: it reports drift the last commit
# already re-anchored, and can pass a tree whose anchors drifted only within it.
# Measured 2026-09-09: on the dirty tree of the ADR-0048 round, HEAD~1 reported 9
# stale anchors that HEAD reported as clean, and CI (comparing against HEAD) was
# the one that was right.
if git diff --quiet HEAD 2>/dev/null; then
    PREV="$(git rev-parse HEAD~1 2>/dev/null || true)"
    PREV_WHY="the push predecessor"
else
    PREV="$(git rev-parse HEAD 2>/dev/null || true)"
    PREV_WHY="the push predecessor of this UNCOMMITTED change set"
fi
if [ -n "$PREV" ] && [ "$PREV" != "$MERGE_BASE" ]; then
    echo "-- against $PREV_WHY ($PREV), which is what CI compares"
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
