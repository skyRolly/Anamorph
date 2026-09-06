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
#  A CRASH IS CALLED A CRASH ON EVERY PLATFORM, which it was not until
#  2026-09-06. An exit code alone does not say what happened -- on Linux a
#  crashing validator dies from the signal and the shell reports 128+N, but on
#  macOS pluginval traps the signal itself and exits 9 -- so the verdict is
#  decided by `classify_pass_exit` below, from pluginval's own handler line
#  wherever that is available rather than from the number. `--self-test` drives
#  that function with recorded strings and needs no build, no pluginval and no
#  network.
#
#  Network domain needed: github.com (pluginval release download).
# ============================================================================
set -euo pipefail

# ---------------------------------------------------------------------------
#  PASS CLASSIFICATION -- what a NON-ZERO pluginval exit actually MEANS.
#
#  Kept as a pure function of (host, exit code, captured output) and separated
#  from the run loop further down so it can be exercised with recorded strings
#  (`--self-test`, below) against no build tree, no pluginval and no display --
#  the same rule, for the same reason, as the `--self-test` the checkers in
#  `scripts/` and `setup-llvm-apt.sh` carry: prove the gate is live before
#  trusting its silence.
#
#  A SIGNAL DEATH DOES NOT LOOK THE SAME ON EVERY PLATFORM, and reading the exit
#  code alone got macOS wrong for this script's whole life. On Linux a crashing
#  validator really is killed by the signal, so the shell reports 128+N and the
#  "under 128 is a real validation failure" rule holds. On macOS it does not:
#  pluginval installs its OWN handler for SIGFPE, SIGILL, SIGSEGV, SIGBUS and
#  SIGABRT -- `kill9WithSomeMercy` in its `Source/CommandLine.cpp`, where BOTH
#  the definition AND the `CommandLineValidator` constructor that installs it
#  are `#if JUCE_MAC` -- which logs
#      pluginval received <strsignal(signal)>, exiting immediately
#  and then calls `std::_Exit (SIGKILL)`. SIGKILL is 9, so a macOS crash arrives
#  as exit 9: small, far below 128, and reported by this script as a "real
#  validation failure, not a crash".
#
#  THAT IS OBSERVED, NOT HYPOTHETICAL. PR #141, run 34019453055, job `macos`, AU
#  randomise pass 2/3: every test passed, pluginval printed `SUCCESS`, and only
#  then came `libc++abi: terminating due to uncaught exception of type
#  std::__1::bad_function_call`, `pluginval received Abort trap: 6, exiting
#  immediately`, and this script's `(exit 9) -- real validation failure, not a
#  crash.` A teardown crash was reported as a plug-in that failed validation --
#  the two most different verdicts this gate can reach.
#
#  THE LOG LINE IS THE EVIDENCE; THE EXIT CODE IS ONLY A PROXY. The handler
#  prints before it exits, so that line is direct proof a signal was trapped,
#  while 9 is merely the number this one handler happens to use. The line is
#  therefore matched on EVERY host rather than on Darwin alone: gating proof on
#  a platform check is exactly the mistake above, and a pluginval that installed
#  the same handler on another platform would reproduce it there. A bare exit 9
#  with no such line is read as a crash on DARWIN ONLY -- there it is that
#  handler's signature, and pluginval's own exits are 0 or 1
#  (`setApplicationReturnValue`) -- while on any other host 9 is just a small
#  code and stays a validation failure.
#
#  THIS RENAMES THE VERDICT; IT DOES NOT MOVE THE GATE. A crashed pass still
#  fails, still with pluginval's own exit code, and the retry policy further
#  down is untouched: macOS still gets exactly one attempt.
#
#  WHAT IT DELIBERATELY DOES NOT SEPARATE, said out loud rather than discovered.
#  Neither is reachable from this script's own fixed invocation, and neither is
#  new: a MALFORMED command line makes pluginval return -1
#  (`ConsoleApplication::fail (msg, -1)`), which the shell reports as 255 and the
#  `>= 128` rule calls a crash; and a pluginval TIMEOUT exits 1 on every platform
#  (`Process::terminate()`), so it reads as a validation failure -- which it is,
#  and its own `*** FAILED: Timeout after` line is the only thing that names it.
# ---------------------------------------------------------------------------

# The two LITERAL halves of pluginval's handler line. What sits between them is
# `strsignal()`'s text, which is each platform's own wording (macOS says "Abort
# trap: 6" where glibc says "Aborted"), so only the halves are matched.
PLUGINVAL_TRAP_PREFIX='pluginval received '
PLUGINVAL_TRAP_SUFFIX=', exiting immediately'
PLUGINVAL_TRAP_EXIT=9                  # std::_Exit (SIGKILL), and SIGKILL is 9

# Echoes the trapped signal's name when the captured output carries the handler
# line, and NOTHING at all otherwise -- so `-n` on its result is the test for
# "pluginval trapped a signal". The LAST match wins, so a pass that trapped more
# than once names the signal it actually died from.
trapped_signal_in_log() {
    local log="${1:-}"
    if [ -z "$log" ] || [ ! -f "$log" ]; then
        return 0
    fi
    sed -n "s/.*${PLUGINVAL_TRAP_PREFIX}\(.*\)${PLUGINVAL_TRAP_SUFFIX}.*/\1/p" "$log" | tail -n 1
}

# pass | crash | fail, from the host, the exit code and the captured output.
# The caller keeps pluginval's own exit code either way; only the NAME of the
# verdict -- and, on Linux, whether the retry loop below is entered -- is decided
# here. A missing or unreadable log is not an error: the code-based rules still
# apply, so classification never DEPENDS on having captured anything.
classify_pass_exit() {
    local host="$1" rc="$2" log="${3:-}"
    if [ "$rc" -eq 0 ]; then
        echo pass
    elif [ "$rc" -ge 128 ]; then
        echo crash                     # killed by the signal; every POSIX host
    elif [ -n "$(trapped_signal_in_log "$log")" ]; then
        echo crash                     # pluginval trapped it and said so
    elif [ "$host" = "Darwin" ] && [ "$rc" -eq "$PLUGINVAL_TRAP_EXIT" ]; then
        echo crash                     # the handler's exit code, without its line
    else
        echo fail
    fi
}

# ----------------------------------------------------------------------------
#  One validation pass. Retry ONLY on a CRASH -- as `classify_pass_exit` above
#  decides one -- never on a real validation failure. The editor/window tests embed
#  the plugin via X11/XEmbed, and JUCE's host-side XEmbedComponent has a
#  use-after-free on rapid open/close; that crash lives in pluginval's own JUCE, not
#  in the plugin, so it can't be fixed from here. A real plugin defect crashes
#  deterministically and still fails after the retries; a real test ASSERTION does not.
#
#  THE RETRY IS LINUX-ONLY, and that scoping is the point rather than an
#  incidental detail. The flake above is X11/XEmbed -- a mechanism that does not
#  exist on macOS, where this same script also runs (the `macos` and
#  `macos-intel` jobs, VST3 and AU). Retrying a signal crash there would give a
#  genuine crash three chances to disappear on a platform that has no known
#  flake to excuse it, which is the opposite of what a release gate is for. On
#  macOS a crash is a crash and fails the pass immediately.
#
#  (Windows has its own script, `run-pluginval.ps1`, which since 2026-08-31
#  applies the same rule as macOS here: a real abnormal exit code of a LAUNCHED
#  validator fails the pass immediately. Its retry loop covers only a $null
#  exit code, which after the WaitForExit fix can mean nothing but "the process
#  never launched" -- a setup fault, not a crash being excused. See ER-CI-01.
#  It needs NO counterpart to the trapped-signal rule above: pluginval's handler
#  is `#if JUCE_MAC` in both halves, so nothing installs it on Windows, and an
#  unhandled access violation there surfaces as its Win32 exception code --
#  large or negative, which that script already calls a crash.)
# ----------------------------------------------------------------------------
case "$(uname -s)" in
    Linux) CRASH_RETRY_ATTEMPTS=3 ;;   # the XEmbed flake documented above
    *)     CRASH_RETRY_ATTEMPTS=1 ;;   # no known host-side flake: fail on the first crash
esac

run_one_pass() {
    local label="$1"
    local attempts="$CRASH_RETRY_ATTEMPTS" attempt rc host verdict sig detail
    host="$(uname -s)"
    for attempt in $(seq 1 "$attempts"); do
        set +e
        $RUN_PREFIX "$PLUGINVAL" --strictness-level "$STRICTNESS" "${MODE_ARGS[@]}" \
            --validate "$BUNDLE_PATH" --timeout-ms 600000 2>&1 | tee "$PASS_LOG"
        # The FIRST element, not `$?`. `$?` is the PIPELINE's status: with
        # `pipefail` (set above) that is the RIGHTMOST non-zero one, so a `tee`
        # that could not write its file would fail an otherwise passing
        # validation; without `pipefail` it would be tee's 0, and a crashed pass
        # would be announced as PASSED. Only element 0 is pluginval's own
        # verdict, and only pluginval's verdict may decide this gate.
        rc=${PIPESTATUS[0]}
        set -e

        verdict="$(classify_pass_exit "$host" "$rc" "$PASS_LOG")"
        if [ "$verdict" = "pass" ]; then
            echo "pluginval: PASSED ($label) at strictness $STRICTNESS (attempt $attempt/$attempts)"
            return 0
        fi
        if [ "$verdict" = "fail" ]; then
            echo "pluginval: FAILED ($label) at strictness $STRICTNESS (exit $rc) -- real validation failure, not a crash."
            return "$rc"
        fi
        # A crash. Name the trapped signal when pluginval reported one, so the
        # step says what died rather than only that something did.
        sig="$(trapped_signal_in_log "$PASS_LOG")"
        if [ -n "$sig" ]; then
            detail="pluginval trapped $sig, exit $rc"
        else
            detail="exit $rc"
        fi
        if [ "$attempts" -eq 1 ]; then
            echo "pluginval: CRASHED ($label, $detail) -- no crash-retry on this platform (the retry exists for the Linux X11/XEmbed flake only)."
            return "$rc"
        fi
        echo "pluginval: crashed ($label, $detail -- the known JUCE/X11 host-side XEmbed editor flake). Retry $attempt/$attempts."
    done
    echo "pluginval: still crashing ($label) after $attempts attempts -- treating as a failure."
    return 139
}

# ---------------------------------------------------------------------------
#  --self-test: PROVE THE CLASSIFIER ABOVE IS LIVE, from recorded strings.
#
#  It drives the SAME function `run_one_pass` calls, over captured output
#  written to a file exactly the way a real pass writes one -- including the
#  verbatim tail of the run that exposed the defect. It needs no build tree, no
#  pluginval, no display and no network, so it runs in a lint job rather than in
#  the five build jobs that validate for real, and in `scripts/preflight.sh`.
#
#  The cases that matter most are the two that must NOT move: a real validation
#  failure stays a failure (a classifier that called everything a crash would
#  hide precisely what this gate exists to catch), and a Linux signal death
#  stays a crash with its retry intact.
# ---------------------------------------------------------------------------
self_test() {
    local dir
    _st_fail=0
    _st_dir="$(mktemp -d "${TMPDIR:-/tmp}/anamorph-pluginval-selftest.XXXXXX")"
    dir="$_st_dir"

    # The verbatim tail of the run this change exists for: PR #141, run
    # 34019453055, job `macos`, AU randomise pass 2/3 -- SUCCESS first, then the
    # teardown crash.
    printf '%s\n' \
        "Completed tests in pluginval / Plugin info" \
        "SUCCESS" \
        "libc++abi: terminating due to uncaught exception of type std::__1::bad_function_call" \
        "pluginval received Abort trap: 6, exiting immediately" > "$dir/trapped"

    # A REAL validation failure: pluginval's own wording, and no handler line.
    printf '%s\n' \
        "Starting tests in: pluginval / Automation..." \
        "!!! Test \"Automation\" failed: Parameter value was not restored" \
        "*** FAILED: 1 TESTS" > "$dir/failed"

    # A clean pass.
    printf '%s\n' \
        "Completed tests in pluginval / Plugin info" \
        "SUCCESS" > "$dir/passed"

    # Two traps in one pass: the LAST is the one it died from.
    printf '%s\n' \
        "pluginval received Segmentation fault: 11, exiting immediately" \
        "pluginval received Abort trap: 6, exiting immediately" > "$dir/trapped_twice"

    # The prefix WITHOUT the suffix must not match: ordinary output that happens
    # to start the same way is not evidence of a trapped signal.
    printf '%s\n' \
        "pluginval received 2 channels on the main input bus" \
        "*** FAILED: 1 TESTS" > "$dir/near_miss"

    : > "$dir/empty"

    echo "run-pluginval --self-test: pluginval exit classification"

    # A pass is a pass, and the log is not consulted for it.
    _st_verdict pass  "clean pass (Darwin)"                        Darwin 0   "$dir/passed"
    _st_verdict pass  "clean pass (Linux)"                         Linux  0   "$dir/passed"

    # THE DEFECT: macOS trapped-signal death, which read as a validation failure.
    _st_verdict crash "macOS trapped signal, handler line + exit 9" Darwin 9   "$dir/trapped"
    _st_verdict crash "macOS exit 9 with no captured line"          Darwin 9   "$dir/empty"
    _st_verdict crash "macOS exit 9, log file absent entirely"      Darwin 9   "$dir/absent"

    # A real validation failure MUST stay one, on both platforms.
    _st_verdict fail  "macOS validation failure (exit 1)"           Darwin 1   "$dir/failed"
    _st_verdict fail  "Linux validation failure (exit 1)"           Linux  1   "$dir/failed"
    _st_verdict fail  "macOS usage error (exit 2)"                  Darwin 2   "$dir/empty"
    _st_verdict fail  "prefix without the suffix is not a trap"     Darwin 1   "$dir/near_miss"

    # Signal deaths: unchanged on Linux, and still crashes on macOS.
    _st_verdict crash "Linux SIGSEGV death (exit 139)"              Linux  139 "$dir/empty"
    _st_verdict crash "macOS SIGABRT death (exit 134)"              Darwin 134 "$dir/trapped"

    # The handler line outranks the host gate; the bare code does not.
    _st_verdict crash "handler line on Linux (evidence, not host)"  Linux  9   "$dir/trapped"
    _st_verdict fail  "bare exit 9 on Linux is not a crash"         Linux  9   "$dir/empty"

    # The message names the signal, so the extraction is a case too.
    _st_signal "Abort trap: 6"      "signal name from the handler line" "$dir/trapped"
    _st_signal "Abort trap: 6"      "two traps: the last one wins"      "$dir/trapped_twice"
    _st_signal ""                   "no handler line, no signal name"   "$dir/passed"
    _st_signal ""                   "absent log yields no signal name"  "$dir/absent"

    # END TO END, through `run_one_pass` itself, against a stand-in pluginval.
    # The unit cases above cannot see the WIRING that feeds them, and that wiring
    # is where this defect could return: `2>&1` (the handler line goes to
    # stderr), the `tee`, and `${PIPESTATUS[0]}` rather than `$?` (the pipeline's
    # status, which is tee's whenever tee is the one that failed). The stand-ins
    # write where the real one writes and exit what it exits.
    printf '%s\n' \
        '#!/usr/bin/env bash' \
        'echo "SUCCESS"' \
        'echo "pluginval received Abort trap: 6, exiting immediately" >&2' \
        'exit 9' > "$dir/pv-trapped"
    printf '%s\n' \
        '#!/usr/bin/env bash' \
        'echo "*** FAILED: 1 TESTS"' \
        'exit 1' > "$dir/pv-failed"
    printf '%s\n' \
        '#!/usr/bin/env bash' \
        'echo "SUCCESS"' \
        'exit 0' > "$dir/pv-passed"
    chmod +x "$dir/pv-trapped" "$dir/pv-failed" "$dir/pv-passed"

    _st_endtoend 9   "CRASHED (e2e trapped, pluginval trapped Abort trap: 6, exit 9)" \
                     "e2e trapped" 1 "$dir/pv-trapped"
    _st_endtoend 1   "FAILED (e2e failed) at strictness 8 (exit 1) -- real validation failure" \
                     "e2e failed"  1 "$dir/pv-failed"
    _st_endtoend 0   "PASSED (e2e passed) at strictness 8 (attempt 1/1)" \
                     "e2e passed"  1 "$dir/pv-passed"
    _st_endtoend 139 "still crashing (e2e retried) after 3 attempts" \
                     "e2e retried" 3 "$dir/pv-trapped"
    # tee cannot write its file: the CAPTURE failed, the validation did not, and
    # only pluginval's verdict may decide this gate. `$?` would be tee's here.
    _st_endtoend 0   "PASSED (e2e uncapturable) at strictness 8 (attempt 1/1)" \
                     "e2e uncapturable" 1 "$dir/pv-passed" "$dir/no-such-dir/pass.log"

    rm -rf "$dir"

    if [ "$_st_fail" -ne 0 ]; then
        echo "run-pluginval --self-test: ${_st_fail} case(s) FAILED" >&2
        return 2
    fi
    echo "run-pluginval --self-test: 22 cases passed"
    return 0
}

_st_verdict() {
    local want="$1" name="$2" host="$3" rc="$4" log="$5" got
    got="$(classify_pass_exit "$host" "$rc" "$log")"
    if [ "$got" = "$want" ]; then
        echo "  ok    ${name} -> ${got}"
    else
        echo "  FAIL  ${name} -> ${got}, want ${want}" >&2
        _st_fail=$((_st_fail + 1))
    fi
}

_st_signal() {
    local want="$1" name="$2" log="$3" got
    got="$(trapped_signal_in_log "$log")"
    if [ "$got" = "$want" ]; then
        echo "  ok    ${name} -> '${got}'"
    else
        echo "  FAIL  ${name} -> '${got}', want '${want}'" >&2
        _st_fail=$((_st_fail + 1))
    fi
}

# Drives the REAL `run_one_pass` with a stand-in validator. The globals it reads
# are declared `local` here: bash scopes those dynamically, so the callee sees
# these and the script's own values are never touched.
_st_endtoend() {
    local want_rc="$1" want_text="$2" name="$3" attempts="$4" exe="$5" out got_rc
    local PLUGINVAL="$exe" BUNDLE_PATH="$_st_dir/Anamorph.vst3" PASS_LOG="${6:-$_st_dir/pass.log}"
    local RUN_PREFIX="" STRICTNESS=8 CRASH_RETRY_ATTEMPTS="$attempts"
    local -a MODE_ARGS=(--random-seed 1)
    set +e
    out="$(run_one_pass "$name" 2>&1)"
    got_rc=$?
    set -e
    if [ "$got_rc" -eq "$want_rc" ] && printf '%s' "$out" | grep -qF -- "$want_text"; then
        echo "  ok    ${name} (end to end) -> exit ${got_rc}, said the right thing"
    else
        echo "  FAIL  ${name} (end to end) -> exit ${got_rc}, want ${want_rc} and \"${want_text}\"" >&2
        printf '        output was: %s\n' "$out" >&2
        _st_fail=$((_st_fail + 1))
    fi
}

# Dispatched BEFORE any setup: the self-test must not need a bundle, pluginval
# or the network, and refusing to run without them would defeat the point.
if [ "${1:-}" = "--self-test" ]; then
    self_test
    exit $?
fi

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

# pluginval's output is TEE'd rather than left to stream straight to the job log,
# because `classify_pass_exit` reads it: the handler line is the only DIRECT
# evidence that a signal was trapped. `2>&1` is load-bearing rather than tidy --
# pluginval never installs a Logger, so `Logger::writeToLog` falls through to
# JUCE's `outputDebugString`, which on POSIX is `std::cerr`. The log still
# streams as it did before (`tee` writes both ways), now as one ordered stream.
PASS_LOG="$(mktemp "${TMPDIR:-/tmp}/anamorph-pluginval-pass.XXXXXX")"
trap 'rm -f "$PASS_LOG"' EXIT

echo "Validating $BUNDLE_PATH at strictness $STRICTNESS -- format=$FORMAT mode=$MODE (${PASSES} consecutive pass(es) required)"
for pass in $(seq 1 "$PASSES"); do
    run_one_pass "$FORMAT $MODE pass $pass/$PASSES"
done
echo "pluginval: ALL ${PASSES} ${MODE} pass(es) succeeded for ${FORMAT} at strictness $STRICTNESS"
