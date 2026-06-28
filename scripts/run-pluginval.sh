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

# ----------------------------------------------------------------------------
#  Retry ONLY on a signal-crash (segfault/abort), never on a real validation
#  failure. The editor/window tests embed the plugin via X11/XEmbed, and JUCE's
#  host-side XEmbedComponent has a use-after-free: on a ConfigureNotify it posts
#  MessageManager::callAsync([this]{...}) capturing a raw pointer that can outlive
#  the editor window when a host rapidly opens/closes it (juce_XEmbedComponent_linux
#  .cpp). That crash lives in pluginval's own JUCE, not in the plugin, so it can't
#  be fixed from here -- the plugin already drops its OpenGL child window on Linux to
#  cut the ConfigureNotify traffic that drives it. A real plugin defect crashes
#  deterministically and still fails after the retries; a real test ASSERTION returns
#  a non-signal exit code and fails immediately (no retry).
ATTEMPTS=3
for attempt in $(seq 1 "$ATTEMPTS"); do
    set +e
    $RUN_PREFIX "$PLUGINVAL" --strictness-level "$STRICTNESS" --validate "$VST3_PATH" --timeout-ms 600000
    rc=$?
    set -e

    if [ "$rc" -eq 0 ]; then
        echo "pluginval: PASSED at strictness $STRICTNESS (attempt $attempt/$ATTEMPTS)"
        exit 0
    fi
    if [ "$rc" -lt 128 ]; then
        echo "pluginval: FAILED at strictness $STRICTNESS (exit $rc) -- real validation failure, not a crash."
        exit "$rc"
    fi
    echo "pluginval: crashed (exit $rc -- the known JUCE/X11 host-side XEmbed editor flake). Retry $attempt/$ATTEMPTS."
done

echo "pluginval: still crashing after $ATTEMPTS attempts -- treating as a failure."
exit 139
