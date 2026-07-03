# ============================================================================
#  Anamorph -- pluginval validation (Windows). Mirrors scripts/run-pluginval.sh:
#  same strictness + mode arguments, same "3 consecutive passes per mode" gate.
#
#  Usage: pwsh scripts/run-pluginval.ps1 -Strictness 10 -Mode deterministic
#                                        -Mode randomise
#
#  Both modes run 3 CONSECUTIVE passes; ALL must pass. Mirrors run-pluginval.sh's
#  crash-retry policy: a REAL validation assertion (a small, clean exit code) fails
#  the step IMMEDIATELY; an abnormal termination / crash (a large Win32 exception
#  code, a negative code, or NO code at all) is retried, and STILL fails after the
#  retries. Crucially, a crash can leave $LASTEXITCODE $null, and `exit $null` would
#  exit 0 -- a FALSE GREEN. A null/empty code is therefore treated as a crash and
#  NEVER as success. Network domain needed: github.com (pluginval release download).
# ============================================================================
param(
    [int]    $Strictness = 8,
    [string] $Mode       = "deterministic"
)

# --- Setup (download/extract): real errors should stop the script. ----------
$ErrorActionPreference = "Stop"

$root  = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build"
$tools = Join-Path $root ".tools"
New-Item -ItemType Directory -Force -Path $tools | Out-Null

$vst3 = Get-ChildItem -Recurse -Path $build -Filter Anamorph.vst3 -Directory | Select-Object -First 1
if (-not $vst3) { Write-Host "Anamorph.vst3 not found -- build first (scripts/build.sh)."; exit 1 }

$pv = Join-Path $tools "pluginval.exe"
if (-not (Test-Path $pv)) {
    Write-Host "Fetching pluginval (pluginval_Windows.zip)..."
    Invoke-WebRequest -Uri "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Windows.zip" -OutFile "$tools\pluginval.zip"
    Expand-Archive -Force "$tools\pluginval.zip" -DestinationPath $tools
}

switch ($Mode) {
    "randomise"     { $modeArgs = @("--randomise");        $passes = 3 }
    "deterministic" { $modeArgs = @("--random-seed", "0"); $passes = 3 }
    default         { Write-Host "Unknown mode '$Mode' (expected deterministic|randomise)"; exit 2 }
}

# --- pluginval invocation: the EXIT CODE is the only signal. -----------------
# pluginval streams test progress to stderr; under PowerShell 7 `Stop` +
# $PSNativeCommandUseErrorActionPreference that native stderr (or a non-zero exit)
# can throw a terminating error BEFORE our explicit check, which previously failed
# the deterministic step spuriously and made GitHub SKIP the randomise step. Switch
# to Continue and disable native-command error mapping so a clean run (exit 0) is a
# pass and only a real non-zero exit fails the step.
$ErrorActionPreference = "Continue"
$PSNativeCommandUseErrorActionPreference = $false

# The loop is INLINE (not a function) so pluginval's stdout streams straight to the console
# instead of being captured into $rc. Each pass gets up to $attempts tries; a real validation
# failure exits immediately, a crash is retried, and an exhausted pass fails the whole step.
Write-Host "Validating $($vst3.FullName) at strictness $Strictness -- mode=$Mode ($passes consecutive pass(es) required)"
$attempts = 3
for ($p = 1; $p -le $passes; $p++) {
    $passed = $false
    for ($a = 1; $a -le $attempts; $a++) {
        & $pv --strictness-level $Strictness @modeArgs --validate $vst3.FullName --timeout-ms 600000
        $rc = $LASTEXITCODE
        if ($rc -eq 0) {
            Write-Host "pluginval: PASSED ($Mode pass $p/$passes) at strictness $Strictness (attempt $a/$attempts)"
            $passed = $true
            break
        }
        # $null MUST be tested first: `$null -lt 0` and `$null -ge 256` are both $false, so without
        # this a null code would fall through to the "real failure" branch and `exit $null` -> exit 0.
        $crashed = ($null -eq $rc) -or ($rc -lt 0) -or ($rc -ge 256)
        if (-not $crashed) {
            Write-Host "pluginval: FAILED ($Mode pass $p/$passes) at strictness $Strictness (exit $rc) -- real validation failure, not a crash."
            exit $rc
        }
        $shown = if ($null -eq $rc) { 'none (crash/abnormal termination)' } else { $rc }
        Write-Host "pluginval: crashed ($Mode pass $p/$passes, exit $shown -- abnormal termination during the editor tests). Retry $a/$attempts."
    }
    if (-not $passed) {
        Write-Host "pluginval: still crashing ($Mode pass $p/$passes) after $attempts attempts -- treating as a failure."
        exit 1
    }
}
Write-Host "pluginval: ALL $passes $Mode pass(es) succeeded at strictness $Strictness"
exit 0
