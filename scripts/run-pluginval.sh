#!/usr/bin/env bash
# ============================================================================
#  Anamorph -- pluginval validation (Tracktion's open-source validator)
#
#  Downloads a pluginval release if not present, then validates the built VST3.
#  Works on Linux and macOS (the Windows job uses scripts/run-pluginval.ps1 with
#  the SAME structure). Editor open/close tests need a display, so we run under
#  xvfb-run on Linux when available.
#
#  Usage: scripts/run-pluginval.sh [strictness] [mode]
#           strictness : 5 dev / 8 standard / 10 pre-release gold (default 8)
#           mode       : deterministic (default) | randomise
#
#  Both modes run 3 CONSECUTIVE passes; ALL must pass:
#    deterministic -- fixed `--random-seed 0`, reproducible.
#    randomise     -- `--randomise` (randomised test ORDER + time-seeded fuzzing);
#                     a value-/order-dependent defect surfaces here even when the
#                     deterministic pass is green.
#
#  Network domain needed: github.com (pluginval release download).
# ============================================================================
set -euo pipefail

STRICTNESS="${1:-8}"
MODE="${2:-deterministic}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build"
TOOLS_DIR="$ROOT/.tools"
mkdir -p "$TOOLS_DIR"

VST3_PATH="$(find "$BUILD_DIR" -name 'Anamorph.vst3' 2>/dev/null | head -n1 || true)"
if [ -z "$VST3_PATH" ]; then
    echo "Anamorph.vst3 not found -- build first (scripts/build.sh)."
    exit 1
fi

# Platform-specific pluginval release + binary path (Linux vs macOS).
case "$(uname -s)" in
    Darwin) PV_ZIP="pluginval_macOS.zip"; PLUGINVAL="$TOOLS_DIR/pluginval.app/Contents/MacOS/pluginval" ;;
    *)      PV_ZIP="pluginval_Linux.zip"; PLUGINVAL="$TOOLS_DIR/pluginval" ;;
esac

if [ ! -x "$PLUGINVAL" ]; then
    echo "Fetching pluginval ($PV_ZIP)..."
    curl -L "https://github.com/Tracktion/pluginval/releases/latest/download/$PV_ZIP" -o "$TOOLS_DIR/pluginval.zip"
    (cd "$TOOLS_DIR" && unzip -o pluginval.zip >/dev/null)
    chmod +x "$PLUGINVAL" || true
fi

RUN_PREFIX=""
if command -v xvfb-run >/dev/null 2>&1; then
    RUN_PREFIX="xvfb-run -a"
fi

# Extra flags + pass count per mode. Both modes run 3 consecutive passes.
case "$MODE" in
    randomise)     MODE_ARGS=(--randomise);     PASSES=3 ;;
    deterministic) MODE_ARGS=(--random-seed 0); PASSES=3 ;;
    *) echo "Unknown mode '$MODE' (expected deterministic|randomise)"; exit 2 ;;
esac

# ----------------------------------------------------------------------------
#  One validation pass. Retry ONLY on a signal-crash (segfault/abort), never on a
#  real validation failure. The editor/window tests embed the plugin via X11/XEmbed,
#  and JUCE's host-side XEmbedComponent has a use-after-free on rapid open/close;
#  that crash lives in pluginval's own JUCE, not in the plugin, so it can't be fixed
#  from here. A real plugin defect crashes deterministically and still fails after the
#  retries; a real test ASSERTION returns a non-signal exit code and fails immediately.
# ----------------------------------------------------------------------------
run_one_pass() {
    local label="$1"
    local attempts=3 attempt rc
    for attempt in $(seq 1 "$attempts"); do
        set +e
        $RUN_PREFIX "$PLUGINVAL" --strictness-level "$STRICTNESS" "${MODE_ARGS[@]}" \
            --validate "$VST3_PATH" --timeout-ms 600000
        rc=$?
        set -e

        if [ "$rc" -eq 0 ]; then
            echo "pluginval: PASSED ($label) at strictness $STRICTNESS (attempt $attempt/$attempts)"
            return 0
        fi
        if [ "$rc" -lt 128 ]; then
            echo "pluginval: FAILED ($label) at strictness $STRICTNESS (exit $rc) -- real validation failure, not a crash."
            return "$rc"
        fi
        echo "pluginval: crashed ($label, exit $rc -- the known JUCE/X11 host-side XEmbed editor flake). Retry $attempt/$attempts."
    done
    echo "pluginval: still crashing ($label) after $attempts attempts -- treating as a failure."
    return 139
}

echo "Validating $VST3_PATH at strictness $STRICTNESS -- mode=$MODE (${PASSES} consecutive pass(es) required)"
for pass in $(seq 1 "$PASSES"); do
    run_one_pass "$MODE pass $pass/$PASSES"
done
echo "pluginval: ALL ${PASSES} ${MODE} pass(es) succeeded at strictness $STRICTNESS"
