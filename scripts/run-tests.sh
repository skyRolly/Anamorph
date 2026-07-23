#!/usr/bin/env bash
# Run the Anamorph headless self-tests built by build.sh:
#   1. AnamorphTests      -- DSP acceptance suite
#   2. AnamorphStateTests -- state-serialization / parameter-compatibility suite
# Both are required (fail-closed): a missing binary fails the gate.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build"

TESTS="$(find "$BUILD_DIR" -name 'AnamorphTests' -type f 2>/dev/null | head -n1 || true)"
if [ -z "$TESTS" ]; then
    echo "AnamorphTests not found -- build first (scripts/build.sh)."
    exit 1
fi

STATE_TESTS="$(find "$BUILD_DIR" -name 'AnamorphStateTests' -type f 2>/dev/null | head -n1 || true)"
if [ -z "$STATE_TESTS" ]; then
    echo "AnamorphStateTests not found -- build first (scripts/build.sh)."
    exit 1
fi

echo "Running $TESTS"
"$TESTS"

echo
echo "Running $STATE_TESTS"
"$STATE_TESTS"
