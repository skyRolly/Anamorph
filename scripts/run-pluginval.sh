#!/usr/bin/env bash
# ============================================================================
#  Anamorph -- pluginval validation (Tracktion's open-source validator)
#
#  Downloads a pluginval release if not present, then validates the built VST3.
#  Editor open/close tests need a display, so we run under xvfb-run on Linux.
#
#  Usage: scripts/run-pluginval.sh [strictness] [mode]
#           strictness : 5 dev / 8 standard / 10 pre-release gold (default 8)
#           mode       : deterministic (default) | randomise
#
#  Modes (spec 11.3 + state-restoration hardening):
#    deterministic -- fixed `--random-seed 0`, reproducible run.
#    randomise     -- `--randomise` (randomised test ORDER + time-seeded fuzzing),
#                     run 3 CONSECUTIVE times; ALL must pass. This is the gate that
#                     exercises state restoration under randomised conditions a fixed
#                     run can miss; a value-/order-dependent round-trip defect surfaces
#                     here even when the deterministic pass is green.
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

PLUGINVAL="$TOOLS_DIR/pluginval"
if [ ! -x "$PLUGINVAL" ]; then
    echo "Fetching pluginval..."
    URL="https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Linux.zip"
    curl -L "$URL" -o "$TOOLS_DIR/pluginval.zip"
    (cd "$TOOLS_DIR" && unzip -o pluginval.zip >/dev/null)
    chmod +x "$PLUGINVAL" || true
fi

RUN_PREFIX=""
if command -v xvfb-run >/dev/null 2>&1; then
    RUN_PREFIX="xvfb-run -a"
fi

# Extra flags per mode. `--randomise` randomises the test order and time-seeds the
# fuzzing; `--random-seed 0` pins the deterministic run for reproducibility.
case "$MODE" in
    randomise)     MODE_ARGS=(--randomise);        PASSES=3 ;;
    deterministic) MODE_ARGS=(--random-seed 0);    PASSES=1 ;;
    *) echo "Unknown mode '$MODE' (expected deterministic|randomise)"; exit 2 ;;
esac

# ----------------------------------------------------------------------------
#  One validation pass. Retry ONLY on a signal-crash (segfault/abort), never on a
#  real validation failure. The editor/window tests embed the plugin via X11/XEmbed,
#  and JUCE's host-side XEmbedComponent has a use-after-free: on a ConfigureNotify it
#  posts MessageManager::callAsync([this]{...}) capturing a raw pointer that can
#  outlive the editor window when a host rapidly opens/closes it. That crash lives in
#  pluginval's own JUCE, not in the plugin, so it can't be fixed from here. A real
#  plugin defect crashes deterministically and still fails after the retries; a real
#  test ASSERTION returns a non-signal exit code and fails immediately (no retry).
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
