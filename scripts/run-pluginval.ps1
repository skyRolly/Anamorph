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
#  code, a negative code, or no code) is retried, and STILL fails after the retries.
#  KEY: pluginval.exe is a GUI-subsystem app, so it must be launched via
#  System.Diagnostics.Process and explicitly WAITED on (Invoke-Pluginval below) to
#  obtain a trustworthy exit code -- the call operator (`& $pv`) returns immediately
#  with a $null $LASTEXITCODE, which both false-greened the original script and, once
#  the retry loop was added, false-RED-ed it (and spawned concurrent background
#  validators -> garbled output). Network domain needed: github.com (pluginval download).
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
# CRITICAL: pluginval.exe is a GUI-SUBSYSTEM app. PowerShell's call operator (`& $pv`) does NOT wait
# for a GUI-subsystem process -- it returns immediately, leaving $LASTEXITCODE $null. The old loop
# then misread that null as a "crash", retried, and each retry launched ANOTHER pluginval that kept
# validating in the background -> interleaved "garbled" console output AND a false failure (the
# validation actually succeeds in the detached processes). It is also why the original `exit
# $LASTEXITCODE` false-greened (null -> exit 0). Fix: launch pluginval via System.Diagnostics.Process
# with UseShellExecute=$false (inherits THIS console, so output still streams to the CI log), then
# WaitForExit() and read the REAL .ExitCode. Only then is the exit code a trustworthy signal.
$ErrorActionPreference = "Continue"
$PSNativeCommandUseErrorActionPreference = $false

function Invoke-Pluginval {
    param([string] $Exe, [string[]] $PvArgs)
    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $Exe
    foreach ($a in $PvArgs) { [void] $psi.ArgumentList.Add($a) }
    $psi.UseShellExecute = $false   # inherit console (stream output) AND enable a real .ExitCode
    $proc = [System.Diagnostics.Process]::Start($psi)
    $proc.WaitForExit()             # actually WAIT for the validation to finish (the missing piece)
    return $proc.ExitCode
}

# WINDOWS-ONLY: skip the editor GUI tests. The GitHub `windows-latest` runner is GPU-less/headless
# and cannot host this plugin's editor "Editor Automation" test -- it fails there in BOTH GL mode
# (the GDI-generic OpenGL 1.1 renderer has no GL2 shader/VBO entry points) and CPU mode. This is an
# ENVIRONMENTAL limit of the runner, not a plugin defect: the editor validates cleanly on Linux (xvfb,
# CPU) and macOS (GPU/GL), and a core dump of the analogous reproduced crash on Linux lands in JUCE's
# own XEmbedComponent (host-side), never in plugin code. See KI-007. Real Windows machines have a GPU
# and render on GL as designed; only this runner needs the GUI tests skipped. All non-GUI tests
# (audio/state/parameter/bus/automation) still run and still block. pluginval flag: --skip-gui-tests.
$guiArgs = @("--skip-gui-tests")

# Each pass gets up to $attempts tries against the REAL exit code (from WaitForExit above): exit 0 is
# a pass; a small non-zero (1..255) is a real validation failure and fails the step immediately; a
# null / negative / >=256 code is an abnormal termination (Win32 exception) and is retried, then still
# fails after the retries. Because Invoke-Pluginval now WAITS, exactly one pluginval runs at a time --
# no concurrent background instances, so the console output is no longer interleaved.
$pvArgs = @('--strictness-level', "$Strictness") + $modeArgs + $guiArgs + @('--validate', $vst3.FullName, '--timeout-ms', '600000')
Write-Host "Validating $($vst3.FullName) at strictness $Strictness -- mode=$Mode ($passes consecutive pass(es) required); GUI tests skipped (see KI-007)"
$attempts = 3
for ($p = 1; $p -le $passes; $p++) {
    $passed = $false
    for ($a = 1; $a -le $attempts; $a++) {
        $rc = Invoke-Pluginval -Exe $pv -PvArgs $pvArgs
        if ($rc -eq 0) {
            Write-Host "pluginval: PASSED ($Mode pass $p/$passes) at strictness $Strictness (attempt $a/$attempts)"
            $passed = $true
            break
        }
        # $null MUST be tested first: `$null -lt 0` and `$null -ge 256` are both $false, so without
        # this a null code would fall through to the "real failure" branch and exit non-zero anyway,
        # but treating null as a crash keeps the retry semantics symmetric with an abnormal exit.
        $crashed = ($null -eq $rc) -or ($rc -lt 0) -or ($rc -ge 256)
        if (-not $crashed) {
            Write-Host "pluginval: FAILED ($Mode pass $p/$passes) at strictness $Strictness (exit $rc) -- real validation failure, not a crash."
            exit $rc
        }
        $shown = if ($null -eq $rc) { 'none (abnormal termination)' } else { $rc }
        Write-Host "pluginval: crashed ($Mode pass $p/$passes, exit $shown -- abnormal termination). Retry $a/$attempts."
    }
    if (-not $passed) {
        Write-Host "pluginval: still crashing ($Mode pass $p/$passes) after $attempts attempts -- treating as a failure."
        exit 1
    }
}
Write-Host "pluginval: ALL $passes $Mode pass(es) succeeded at strictness $Strictness"
exit 0
