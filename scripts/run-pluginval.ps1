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
#
#  KEY: pluginval.exe is a GUI-subsystem app, so it must be launched via
#  System.Diagnostics.Process and explicitly WAITED on (Invoke-Pluginval below) to
#  obtain a trustworthy exit code -- the call operator (`& $pv`) returns immediately
#  with a $null $LASTEXITCODE, which both false-greened the original script and, once
#  the retry loop was added, false-RED-ed it (and spawned concurrent background
#  validators -> garbled output).
#
#  SEED 0 IS NOT A SEED -- see the same note in run-pluginval.sh, which carries the
#  evidence. pluginval treats 0 as "generate a random one" (`Source/PluginTests.h`),
#  so `--random-seed 0` is equivalent to passing nothing and the "deterministic"
#  mode was not deterministic on any platform. $PluginvalSeed below must stay
#  NONZERO and must match PLUGINVAL_SEED in run-pluginval.sh, so all three
#  platforms validate against the same seed.
#
#  Network domain needed: github.com (pluginval download).
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

# Fail closed on ABSENCE and on AMBIGUITY, matching scripts/run-tests.sh,
# scripts/run-pluginval.sh and the "exactly one match" rule the Windows staging
# step in build.yml applies. `Select-Object -First 1` was worst HERE of all
# places: windows-latest uses the multi-config Visual Studio generator, so several
# configurations of Anamorph.vst3 genuinely coexist in one build tree, and the
# release gate could pass on a Debug or leftover bundle while the uploaded
# artifacts come from Release.
#
# `-Directory` is load-bearing and is specific to Windows: the VST3 BUNDLE
# directory is named Anamorph.vst3 and the module inside it is ALSO named
# Anamorph.vst3 (…/Anamorph.vst3/Contents/x86_64-win/Anamorph.vst3), so an
# unfiltered -Filter matches a directory and a file and only enumeration order
# decides which one is validated.
#
# -ErrorAction SilentlyContinue is required because $ErrorActionPreference is
# 'Stop' above: without it a missing build/ makes Get-ChildItem THROW, so the
# intended "build first" message below is never reached and the operator sees a
# .NET stack trace instead of the one-line fix.
$vst3Matches = @(Get-ChildItem -Recurse -Path $build -Filter Anamorph.vst3 -Directory -ErrorAction SilentlyContinue)
if ($vst3Matches.Count -eq 0) {
    Write-Host "Anamorph.vst3 not found under $build -- build first (scripts/build.sh)."
    exit 1
}
if ($vst3Matches.Count -ne 1) {
    Write-Host "Anamorph.vst3 is ambiguous -- found $($vst3Matches.Count) under ${build}:"
    $vst3Matches | ForEach-Object { Write-Host "  $($_.FullName)" }
    Write-Host "Refusing to guess which bundle the release gate should validate. Remove the stale build tree."
    exit 1
}
$vst3 = $vst3Matches[0]

$pv = Join-Path $tools "pluginval.exe"
if (-not (Test-Path $pv)) {
    Write-Host "Fetching pluginval (pluginval_Windows.zip)..."
    Invoke-WebRequest -Uri "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Windows.zip" -OutFile "$tools\pluginval.zip"
    Expand-Archive -Force "$tools\pluginval.zip" -DestinationPath $tools
}

# NONZERO by requirement, and identical to PLUGINVAL_SEED in run-pluginval.sh.
$PluginvalSeed = 1
switch ($Mode) {
    "randomise"     { $modeArgs = @("--randomise");                     $passes = 3 }
    "deterministic" { $modeArgs = @("--random-seed", "$PluginvalSeed"); $passes = 3 }
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

# Verdicts against the REAL exit code (from WaitForExit above): exit 0 is a pass; a small non-zero
# (1..255) is a real validation failure and fails the step immediately; a negative / >=256 code is an
# abnormal termination of a LAUNCHED validator (a Win32 exception such as 0xC0000005) and ALSO fails
# immediately -- Windows has no documented external flake to excuse (KI-007 was an exit-code
# DETECTION problem, fixed by WaitForExit; the only recorded host-side flake is Linux/X11 XEmbed,
# RISK-004, and the 2026-08-18 change deliberately stopped retrying crashes on macOS for the same
# reason). Until 2026-08-31 this loop still gave every abnormal termination up to 3 tries per pass,
# which the WaitForExit fix had left excusing exclusively GENUINE crashes: an intermittently crashing
# plugin (~1-in-3) passed the full 6-pass Windows release gate ~80% of the time (ER-CI-01).
# Only a $null code retries: Invoke-Pluginval yields $null solely when Process.Start itself fails
# under ErrorActionPreference=Continue -- a launch/setup fault of the step, never a verdict about
# the plugin, and the one case the original null-handling rationale actually covers.
$pvArgs = @('--strictness-level', "$Strictness") + $modeArgs + $guiArgs + @('--validate', $vst3.FullName, '--timeout-ms', '600000')
Write-Host "Validating $($vst3.FullName) at strictness $Strictness -- mode=$Mode ($passes consecutive pass(es) required); GUI tests skipped (see KI-007)"
$launchAttempts = 3
for ($p = 1; $p -le $passes; $p++) {
    $passed = $false
    for ($a = 1; $a -le $launchAttempts; $a++) {
        $rc = Invoke-Pluginval -Exe $pv -PvArgs $pvArgs
        if ($rc -eq 0) {
            Write-Host "pluginval: PASSED ($Mode pass $p/$passes) at strictness $Strictness"
            $passed = $true
            break
        }
        # $null MUST be tested first: `$null -lt 0` and `$null -ge 256` are both $false, so a null
        # code would otherwise fall through into the numeric branches.
        if ($null -eq $rc) {
            Write-Host "pluginval: did not launch ($Mode pass $p/$passes, no exit code). Launch retry $a/$launchAttempts."
            continue
        }
        if (($rc -lt 0) -or ($rc -ge 256)) {
            Write-Host "pluginval: CRASHED ($Mode pass $p/$passes, exit $rc -- abnormal termination). A crash is a verdict, not a flake: failing immediately (no documented Windows flake; see RISK-004 scoping)."
            exit 1
        }
        Write-Host "pluginval: FAILED ($Mode pass $p/$passes) at strictness $Strictness (exit $rc) -- real validation failure, not a crash."
        exit $rc
    }
    if (-not $passed) {
        Write-Host "pluginval: could not be launched after $launchAttempts attempts -- setup failure."
        exit 1
    }
}
Write-Host "pluginval: ALL $passes $Mode pass(es) succeeded at strictness $Strictness"
exit 0
