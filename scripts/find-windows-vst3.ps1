# Shared build-artifact discovery for pluginval and Windows staging.
# JUCE writes desktop.ini for every VS configuration at configure time. An
# icon-only directory is not a built plug-in; real multi-build ambiguity fails.
param([switch]$SelfTest)

function Get-AnamorphVst3Bundle {
    param([Parameter(Mandatory)][string]$BuildRoot)
    $builtBundles = @(Get-ChildItem -Force -Recurse -Path $BuildRoot -Filter Anamorph.vst3 -Directory -ErrorAction SilentlyContinue |
        Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'Contents/x86_64-win/Anamorph.vst3') -PathType Leaf })
    if ($builtBundles.Count -eq 0) {
        throw "Built Anamorph.vst3 module not found under $BuildRoot -- build first."
    }
    if ($builtBundles.Count -ne 1) {
        throw ("Anamorph.vst3 is ambiguous -- found $($builtBundles.Count) built bundles under ${BuildRoot}: " +
               ($builtBundles.FullName -join ', ') + '. Refusing to guess which plug-in to validate or stage.')
    }
    return $builtBundles[0]
}

if ($SelfTest) {
    $ErrorActionPreference = 'Stop'
    $fixtureRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('anamorph-vst3-discovery-' + [guid]::NewGuid())
    function Assert-Refused {
        param([string]$Path, [string]$Expected)
        try { Get-AnamorphVst3Bundle -BuildRoot $Path | Out-Null }
        catch {
            if ($_.Exception.Message -notlike "*$Expected*") { throw }
            return
        }
        throw 'Discovery accepted a seeded missing/ambiguous build.'
    }
    function Add-FixtureModule {
        param([string]$Configuration)
        $moduleDir = Join-Path $fixtureRoot "$Configuration/VST3/Anamorph.vst3/Contents/x86_64-win"
        New-Item -ItemType Directory -Force -Path $moduleDir | Out-Null
        # Discovery tests presence only. Native PE/image validity has its own gate.
        [System.IO.File]::WriteAllBytes((Join-Path $moduleDir 'Anamorph.vst3'), [byte[]](0x4d, 0x5a))
    }
    try {
        Assert-Refused -Path $fixtureRoot -Expected 'not found'
        foreach ($configuration in 'Debug', 'MinSizeRel', 'Release', 'RelWithDebInfo') {
            $bundle = Join-Path $fixtureRoot "$configuration/VST3/Anamorph.vst3"
            New-Item -ItemType Directory -Force -Path $bundle | Out-Null
            Set-Content -LiteralPath (Join-Path $bundle 'desktop.ini') -Value '[.ShellClassInfo]'
        }
        Assert-Refused -Path $fixtureRoot -Expected 'not found'
        Add-FixtureModule -Configuration Release
        $unfiltered = @(Get-ChildItem -Recurse -Path $fixtureRoot -Filter Anamorph.vst3 -Directory)
        if ($unfiltered.Count -ne 4) { throw 'Fixture does not reproduce the configure-only bundle ambiguity.' }
        $release = Join-Path $fixtureRoot 'Release/VST3/Anamorph.vst3'
        if ($IsWindows) {
            (Get-Item -Force -LiteralPath $release).Attributes = [System.IO.FileAttributes]::Directory -bor [System.IO.FileAttributes]::System
        }
        $found = Get-AnamorphVst3Bundle -BuildRoot $fixtureRoot
        if ($found.FullName -ne (Get-Item -Force -LiteralPath $release).FullName) { throw 'Discovery selected the wrong configuration.' }
        Add-FixtureModule -Configuration Debug
        Assert-Refused -Path $fixtureRoot -Expected 'ambiguous'
        Write-Host 'Windows VST3 discovery self-test passed: empty/configure-only, one built, and multiple built bundles.'
    }
    finally {
        if (Test-Path -LiteralPath $fixtureRoot) { Remove-Item -Force -Recurse -LiteralPath $fixtureRoot }
    }
}
