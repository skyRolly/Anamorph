#!/usr/bin/env bash
# Run the Anamorph DSP self-tests built by build.sh.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build"

TESTS="$(find "$BUILD_DIR" -name 'AnamorphTests' -type f 2>/dev/null | head -n1 || true)"
if [ -z "$TESTS" ]; then
    echo "AnamorphTests not found -- build first (scripts/build.sh)."
    exit 1
fi
echo "Running $TESTS"
"$TESTS"
