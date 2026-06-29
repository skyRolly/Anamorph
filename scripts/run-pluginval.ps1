# ============================================================================
#  Anamorph -- pluginval validation (Windows). Mirrors scripts/run-pluginval.sh:
#  same strictness + mode arguments, same "3 consecutive passes per mode" gate.
#
#  Usage: pwsh scripts/run-pluginval.ps1 -Strictness 10 -Mode deterministic
#                                        -Mode randomise
#
#  Both modes run 3 CONSECUTIVE passes; ALL must pass (a non-zero pluginval exit
#  fails the step immediately -- no swallowed exit codes).
#  Network domain needed: github.com (pluginval release download).
# ============================================================================
param(
    [int]    $Strictness = 8,
    [string] $Mode       = "deterministic"
)
$ErrorActionPreference = "Stop"

$root  = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build"
$tools = Join-Path $root ".tools"
New-Item -ItemType Directory -Force -Path $tools | Out-Null

$vst3 = Get-ChildItem -Recurse -Path $build -Filter Anamorph.vst3 -Directory | Select-Object -First 1
if (-not $vst3) { Write-Error "Anamorph.vst3 not found -- build first (scripts/build.sh)."; exit 1 }

$pv = Join-Path $tools "pluginval.exe"
if (-not (Test-Path $pv)) {
    Write-Host "Fetching pluginval (pluginval_Windows.zip)..."
    Invoke-WebRequest -Uri "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Windows.zip" -OutFile "$tools\pluginval.zip"
    Expand-Archive -Force "$tools\pluginval.zip" -DestinationPath $tools
}

switch ($Mode) {
    "randomise"     { $modeArgs = @("--randomise");        $passes = 3 }
    "deterministic" { $modeArgs = @("--random-seed", "0"); $passes = 3 }
    default         { Write-Error "Unknown mode '$Mode' (expected deterministic|randomise)"; exit 2 }
}

Write-Host "Validating $($vst3.FullName) at strictness $Strictness -- mode=$Mode ($passes consecutive pass(es) required)"
for ($p = 1; $p -le $passes; $p++) {
    & $pv --strictness-level $Strictness @modeArgs --validate $vst3.FullName --timeout-ms 600000
    if ($LASTEXITCODE -ne 0) {
        Write-Error "pluginval: FAILED ($Mode pass $p/$passes) at strictness $Strictness (exit $LASTEXITCODE) -- real validation failure."
        exit $LASTEXITCODE
    }
    Write-Host "pluginval: PASSED ($Mode pass $p/$passes) at strictness $Strictness"
}
Write-Host "pluginval: ALL $passes $Mode pass(es) succeeded at strictness $Strictness"
