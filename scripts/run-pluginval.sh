#!/usr/bin/env bash
# ============================================================================
#  Anamorph -- pluginval validation (Tracktion's open-source validator)
#
#  Downloads a pluginval release if not present, then validates the built VST3.
#  Editor open/close tests need a display, so we run under xvfb-run on Linux.
#
#  Usage: scripts/run-pluginval.sh [strictness]   (default strictness = 8)
#
#  Strictness targets (spec 11.3):
#    5  = during development (loads & runs)
#    8  = standard gate (the working bar)
#    10 = pre-release gold standard
#
#  Network domain needed: github.com (pluginval release download).
# ============================================================================
set -euo pipefail

STRICTNESS="${1:-8}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build"
TOOLS_DIR="$ROOT/.tools"
mkdir -p "$TOOLS_DIR"

VST3_PATH="$(find "$BUILD_DIR" -name 'Anamorph.vst3' 2>/dev/null | head -n1 || true)"
if [ -z "$VST3_PATH" ]; then
    echo "Anamorph.vst3 not found -- build first (scripts/build.sh)."
    exit 1
fi

PLUGINVAL="$TOOLS_DIR/pluginval"
if [ ! -x "$PLUGINVAL" ]; then
    echo "Fetching pluginval..."
    URL="https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Linux.zip"
    curl -L "$URL" -o "$TOOLS_DIR/pluginval.zip"
    (cd "$TOOLS_DIR" && unzip -o pluginval.zip >/dev/null)
    chmod +x "$PLUGINVAL" || true
fi

echo "Validating $VST3_PATH at strictness level $STRICTNESS"
RUN_PREFIX=""
if command -v xvfb-run >/dev/null 2>&1; then
    RUN_PREFIX="xvfb-run -a"
fi

$RUN_PREFIX "$PLUGINVAL" --strictness-level "$STRICTNESS" --validate "$VST3_PATH" --timeout-ms 600000
echo "pluginval: PASSED at strictness $STRICTNESS"
