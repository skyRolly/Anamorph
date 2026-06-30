# ============================================================================
#  Anamorph -- pluginval validation (Windows). Mirrors scripts/run-pluginval.sh:
#  same strictness + mode arguments, same "3 consecutive passes per mode" gate.
#
#  Usage: pwsh scripts/run-pluginval.ps1 -Strictness 10 -Mode deterministic
#                                        -Mode randomise
#
#  Both modes run 3 CONSECUTIVE passes; ALL must pass. pluginval's exit code is the
#  SOLE pass/fail signal -- a non-zero exit fails the step immediately (no swallowed
#  exit codes). Network domain needed: github.com (pluginval release download).
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

Write-Host "Validating $($vst3.FullName) at strictness $Strictness -- mode=$Mode ($passes consecutive pass(es) required)"
for ($p = 1; $p -le $passes; $p++) {
    & $pv --strictness-level $Strictness @modeArgs --validate $vst3.FullName --timeout-ms 600000
    $rc = $LASTEXITCODE
    if ($rc -ne 0) {
        Write-Host "pluginval: FAILED ($Mode pass $p/$passes) at strictness $Strictness (exit $rc) -- real validation failure."
        exit $rc
    }
    Write-Host "pluginval: PASSED ($Mode pass $p/$passes) at strictness $Strictness"
}
Write-Host "pluginval: ALL $passes $Mode pass(es) succeeded at strictness $Strictness"
exit 0
