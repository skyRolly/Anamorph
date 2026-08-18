#!/usr/bin/env bash
# ============================================================================
#  Anamorph -- local preflight: the lint gates + the fast release gate, in one
#  command, before a push spends a CI round trip discovering the same thing.
#
#  WHAT THIS RUNS, in CI's own order (docs/procedures/CI_CD.md §Reproducing CI
#  locally): the four lints with their --self-tests first (seconds, no build,
#  historically the most-tripped gates), then the built test suites when a
#  built tree exists.
#
#  WHAT THIS CANNOT RUN, said out loud rather than implied: the FULL Clang
#  warning gate needs a fresh clang build log to classify (see
#  check-clang-warnings.py); only its --self-test runs here. A green preflight
#  is therefore "the lints and suites pass", not "CI will be green".
#
#  THE CITATION GATE RUNS TWICE, against both bases that can disagree:
#  `origin/main` (the local default and the PR merge-base case) and the
#  branch's merge base with it (CI's push-predecessor comparison approximates
#  this locally). Both escape hatches being open at once is how a stale anchor
#  has shipped before -- CI_CD.md carries the full reasoning.
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
python3 scripts/check-clang-warnings.py --self-test
echo "note: the FULL clang-warning gate needs a clang build log (CI: linux-clang);"
echo "      only its self-test ran here."

echo "== preflight: citation gate (both bases) =="
python3 scripts/check-citations.py --self-test
python3 scripts/check-citations.py --check --base origin/main
MERGE_BASE="$(git merge-base origin/main HEAD 2>/dev/null || true)"
if [ -n "$MERGE_BASE" ] && [ "$MERGE_BASE" != "$(git rev-parse origin/main 2>/dev/null)" ]; then
    python3 scripts/check-citations.py --check --base "$MERGE_BASE"
fi

echo "== preflight: test suites =="
if [ -d build/AnamorphTests_artefacts ] && [ -d build/AnamorphStateTests_artefacts ]; then
    scripts/run-tests.sh
else
    echo "note: no built tree at ./build -- the test-suite half of preflight DID NOT RUN."
    echo "      Build first (docs/procedures/BUILD.md), then re-run for the full gate."
fi

echo "== preflight: done =="
