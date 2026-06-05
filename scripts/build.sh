#!/usr/bin/env bash
# ============================================================================
#  Anamorph -- headless command-line build (CMake + Ninja)
#
#  Usage: scripts/build.sh [Release|Debug]
#  Outputs the built .vst3 path on success.
# ============================================================================
set -euo pipefail

BUILD_TYPE="${1:-Release}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build"

cmake -B "$BUILD_DIR" -S "$ROOT" -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" --config "$BUILD_TYPE"

echo
echo "=== Build artefacts ==="
VST3_PATH="$(find "$BUILD_DIR" -name 'Anamorph.vst3' -maxdepth 8 2>/dev/null | head -n1 || true)"
if [ -n "$VST3_PATH" ]; then
    echo "VST3: $VST3_PATH"
else
    echo "WARNING: Anamorph.vst3 not found under $BUILD_DIR"
fi

STANDALONE="$(find "$BUILD_DIR" -name 'Anamorph' -type f -maxdepth 8 2>/dev/null | head -n1 || true)"
[ -n "$STANDALONE" ] && echo "Standalone: $STANDALONE"

TESTS="$(find "$BUILD_DIR" -name 'AnamorphTests' -type f -maxdepth 8 2>/dev/null | head -n1 || true)"
[ -n "$TESTS" ] && echo "Tests: $TESTS"
