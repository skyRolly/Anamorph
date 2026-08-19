#!/usr/bin/env bash
# ============================================================================
#  Anamorph -- pluginval validation (Tracktion's open-source validator)
#
#  Downloads a pluginval release if not present, then validates a built plugin
#  bundle. Works on Linux and macOS (the Windows job uses scripts/run-pluginval.ps1
#  with the SAME structure). Editor open/close tests need a display, so we run
#  under xvfb-run on Linux when available.
#
#  Usage: scripts/run-pluginval.sh [strictness] [mode] [format]
#           strictness : 5 dev / 8 standard / 10 pre-release gold (default 8).
#                        CI passes ANAMORPH_PLUGINVAL_STRICTNESS from build.yml,
#                        which is the single authority for the number.
#           mode       : deterministic (default) | randomise
#           format     : vst3 (default) | au   -- `au` is macOS-only. The AU is
#                        the only format that exists on exactly one platform, and
#                        it is the ONLY format Logic and GarageBand load, so
#                        validating the VST3 alone left the format most of this
#                        product's macOS users actually run entirely ungated. On
#                        a non-Darwin host `au` is an ERROR rather than a silent
#                        skip: a gate that quietly does nothing is the failure
#                        mode the fail-closed `find` below also exists to prevent.
#
#  Both modes run 3 CONSECUTIVE passes; ALL must pass:
#    deterministic -- fixed `--random-seed $PLUGINVAL_SEED` (NONZERO -- see below),
#                     reproducible.
#    randomise     -- `--randomise` (randomised test ORDER) with NO seed, so each
#                     run also draws a fresh seed; a value-/order-dependent defect
#                     surfaces here even when the deterministic pass is green.
#
#  SEED 0 IS NOT A SEED, and this script passed 0 for its whole life -- so the
#  "deterministic" half of the release gate was never deterministic. pluginval
#  treats 0 as "generate a random one": `Source/PluginTests.h` -- "randomSeed = 0;
#  the seed to use for the tests, 0 signifies a randomly generated seed" -- and
#  `Source/CommandLine.cpp` only forwards --random-seed to the validator when it
#  differs from that default. Passing `--random-seed 0` is therefore EXACTLY
#  equivalent to passing nothing, which made the two modes differ only by
#  --randomise (test ORDER) rather than by seed determinism, and made a
#  "deterministic" failure unreproducible from the log. Verified against pluginval
#  1.0.4: `--random-seed 0` printed a different `Random seed:` on every run, while
#  `--random-seed 1` printed `0x1` every time. Any nonzero value works; keep it
#  nonzero and keep it pinned.
#
#  The seed is meaningful WITHOUT --randomise: it seeds the RNG the tests
#  themselves draw from (`Validator.cpp` passes it to `UnitTestRunner::runTests`),
#  whereas --randomise only shuffles test ORDER. The two flags are independent.
#
#  Network domain needed: github.com (pluginval release download).
# ============================================================================
set -euo pipefail

STRICTNESS="${1:-8}"
MODE="${2:-deterministic}"
FORMAT="${3:-vst3}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build"
TOOLS_DIR="$ROOT/.tools"
mkdir -p "$TOOLS_DIR"

case "$FORMAT" in
    vst3) BUNDLE_NAME="Anamorph.vst3" ;;
    au)
        if [ "$(uname -s)" != "Darwin" ]; then
            echo "format 'au' is macOS-only (this host is $(uname -s)) -- refusing to pass silently."
            exit 2
        fi
        BUNDLE_NAME="Anamorph.component"
        ;;
    *) echo "Unknown format '$FORMAT' (expected vst3|au)"; exit 2 ;;
esac

# An explicit bundle path overrides discovery. This exists for ONE case and is
# fail-closed for it: macOS Audio Units are resolved by the system through
# `AudioComponentFindNext`, which only ever finds components the AudioComponent
# registry knows about -- i.e. bundles under a Components directory. A freshly
# built, never-installed .component in the build tree may therefore report ZERO
# plugin types no matter how correct it is, so the macOS job installs it first and
# points here. Set-but-missing is an ERROR rather than a fall back to discovery:
# silently validating a DIFFERENT bundle than the caller named is the failure the
# ambiguity check below exists to prevent.
if [ -n "${ANAMORPH_PLUGINVAL_BUNDLE:-}" ]; then
    if [ ! -e "$ANAMORPH_PLUGINVAL_BUNDLE" ]; then
        echo "ANAMORPH_PLUGINVAL_BUNDLE is set to '$ANAMORPH_PLUGINVAL_BUNDLE' but nothing is there."
        exit 1
    fi
    BUNDLE_MATCHES="$ANAMORPH_PLUGINVAL_BUNDLE"
else
    # Fail closed on ABSENCE and on AMBIGUITY, matching scripts/run-tests.sh. The
    # previous `find ... | head -n1` validated whichever bundle find happened to
    # emit first: with a multi-config layout or a leftover build tree the release
    # gate could pass on a different .vst3 than the one just built. CI always has
    # a single fresh tree, so this only bites locally -- which is exactly where it
    # would go unnoticed.
    BUNDLE_MATCHES="$(find "$BUILD_DIR" -maxdepth 8 -name "$BUNDLE_NAME" 2>/dev/null || true)"
    BUNDLE_COUNT="$(printf '%s' "$BUNDLE_MATCHES" | grep -c . || true)"
    if [ "$BUNDLE_COUNT" -eq 0 ]; then
        echo "$BUNDLE_NAME not found under $BUILD_DIR -- build first (scripts/build.sh)."
        exit 1
    fi
    if [ "$BUNDLE_COUNT" -ne 1 ]; then
        echo "$BUNDLE_NAME is ambiguous -- found $BUNDLE_COUNT under $BUILD_DIR:"
        # Read line by line -- see the same guard in scripts/run-tests.sh: unquoted,
        # printf relies on word splitting (which also splits on spaces inside a
        # path); quoted, printf applies the format once so only the first line gets
        # indented.
        while IFS= read -r m; do echo "  $m"; done <<< "$BUNDLE_MATCHES"
        echo "Refusing to guess which bundle the release gate should validate. Remove the stale build tree."
        exit 1
    fi
fi
BUNDLE_PATH="$BUNDLE_MATCHES"

# Platform-specific pluginval release + binary path (Linux vs macOS).
case "$(uname -s)" in
    Darwin) PV_ZIP="pluginval_macOS.zip"; PLUGINVAL="$TOOLS_DIR/pluginval.app/Contents/MacOS/pluginval" ;;
    *)      PV_ZIP="pluginval_Linux.zip"; PLUGINVAL="$TOOLS_DIR/pluginval" ;;
esac

if [ ! -x "$PLUGINVAL" ]; then
    echo "Fetching pluginval ($PV_ZIP)..."
    curl -L "https://github.com/Tracktion/pluginval/releases/latest/download/$PV_ZIP" -o "$TOOLS_DIR/pluginval.zip"
    (cd "$TOOLS_DIR" && unzip -o pluginval.zip >/dev/null)
    # NOT `|| true`: a failed chmod here resurfaces later as an opaque "cannot
    # execute" from the validation loop, which reads as a plugin problem rather
    # than the setup problem it is. Fail where the fault actually is.
    chmod +x "$PLUGINVAL"
fi

RUN_PREFIX=""
if command -v xvfb-run >/dev/null 2>&1; then
    RUN_PREFIX="xvfb-run -a"
fi

# Extra flags + pass count per mode. Both modes run 3 consecutive passes.
# PLUGINVAL_SEED must stay NONZERO -- 0 is pluginval's "pick a random seed"
# sentinel, not a seed (see the header). The exact value is arbitrary; that it is
# fixed and nonzero is not. Keep it identical to run-pluginval.ps1 so the three
# platforms validate against the same seed.
PLUGINVAL_SEED=1
case "$MODE" in
    randomise)     MODE_ARGS=(--randomise);                     PASSES=3 ;;
    deterministic) MODE_ARGS=(--random-seed "$PLUGINVAL_SEED"); PASSES=3 ;;
    *) echo "Unknown mode '$MODE' (expected deterministic|randomise)"; exit 2 ;;
esac

# ----------------------------------------------------------------------------
#  One validation pass. Retry ONLY on a signal-crash (segfault/abort), never on a
#  real validation failure. The editor/window tests embed the plugin via X11/XEmbed,
#  and JUCE's host-side XEmbedComponent has a use-after-free on rapid open/close;
#  that crash lives in pluginval's own JUCE, not in the plugin, so it can't be fixed
#  from here. A real plugin defect crashes deterministically and still fails after the
#  retries; a real test ASSERTION returns a non-signal exit code and fails immediately.
#
#  THE RETRY IS LINUX-ONLY, and that scoping is the point rather than an
#  incidental detail. The flake above is X11/XEmbed -- a mechanism that does not
#  exist on macOS, where this same script also runs (the `macos` and
#  `macos-intel` jobs, VST3 and AU). Retrying a signal crash there would give a
#  genuine crash three chances to disappear on a platform that has no known
#  flake to excuse it, which is the opposite of what a release gate is for. On
#  macOS a crash is a crash and fails the pass immediately.
#
#  (Windows has its own script and its own, different retry: `run-pluginval.ps1`
#  retries because a GUI-subsystem process can return a null `$LASTEXITCODE`,
#  which is an exit-code DETECTION problem rather than a crash it is excusing.
#  That rationale is unrelated to this one and is left alone.)
# ----------------------------------------------------------------------------
case "$(uname -s)" in
    Linux) CRASH_RETRY_ATTEMPTS=3 ;;   # the XEmbed flake documented above
    *)     CRASH_RETRY_ATTEMPTS=1 ;;   # no known host-side flake: fail on the first crash
esac
run_one_pass() {
    local label="$1"
    local attempts="$CRASH_RETRY_ATTEMPTS" attempt rc
    for attempt in $(seq 1 "$attempts"); do
        set +e
        $RUN_PREFIX "$PLUGINVAL" --strictness-level "$STRICTNESS" "${MODE_ARGS[@]}" \
            --validate "$BUNDLE_PATH" --timeout-ms 600000
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
        if [ "$attempts" -eq 1 ]; then
            echo "pluginval: CRASHED ($label, exit $rc) -- no crash-retry on this platform (the retry exists for the Linux X11/XEmbed flake only)."
            return "$rc"
        fi
        echo "pluginval: crashed ($label, exit $rc -- the known JUCE/X11 host-side XEmbed editor flake). Retry $attempt/$attempts."
    done
    echo "pluginval: still crashing ($label) after $attempts attempts -- treating as a failure."
    return 139
}

echo "Validating $BUNDLE_PATH at strictness $STRICTNESS -- format=$FORMAT mode=$MODE (${PASSES} consecutive pass(es) required)"
for pass in $(seq 1 "$PASSES"); do
    run_one_pass "$FORMAT $MODE pass $pass/$PASSES"
done
echo "pluginval: ALL ${PASSES} ${MODE} pass(es) succeeded for ${FORMAT} at strictness $STRICTNESS"
