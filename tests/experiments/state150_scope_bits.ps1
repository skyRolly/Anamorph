$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$report = Join-Path $root 'state150-results'
New-Item -Path $report -ItemType Directory | Out-Null

function UtcNow { [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ') }

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$instances = @(& $vswhere -all -prerelease -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
if ($LASTEXITCODE -ne 0 -or !$instances) { throw 'No x64 MSVC installation found' }
$toolsets = @{}
foreach ($instance in $instances) {
    $vcvars = Join-Path $instance 'VC\Auxiliary\Build\vcvars64.bat'
    if (!(Test-Path $vcvars)) { continue }
    foreach ($toolset in Get-ChildItem (Join-Path $instance 'VC\Tools\MSVC') -Directory) {
        if ($toolset.Name -match '^14\.(51|52)\.') {
            $toolsets[$toolset.Name] = $vcvars
        }
    }
}
$defaultVcvars = Join-Path $instances[0] 'VC\Auxiliary\Build\vcvars64.bat'
if (!(Test-Path $defaultVcvars)) { throw 'No default x64 vcvars environment found' }

$versions = @($toolsets.Keys | Sort-Object)
$identity = @(
    "observed=$(UtcNow)",
    "commit=$(git -C $root rev-parse HEAD)",
    "tree=$(git -C $root rev-parse 'HEAD^{tree}')",
    "scope_blob=$(git -C $root rev-parse 'HEAD:src/dsp/ScopeBuffer.h')",
    "probe_blob=$(git -C $root rev-parse 'HEAD:tests/experiments/state150_scope_bits.cpp')",
    "runner_image=$env:ImageOS/$env:ImageVersion",
    "visual_studio=$($instances -join '; ')",
    "matching_toolsets=$($versions -join ', ')",
    "exact_14.51.36231_installed=$($versions -contains '14.51.36231')",
    "exact_14.52.37025_installed=$($versions -contains '14.52.37025')"
)
$identity | Tee-Object -FilePath (Join-Path $report 'source.txt')

$harnessErrors = 0
foreach ($version in @('default') + $versions) {
    $vcvars = $defaultVcvars
    $selection = ''
    if ($version -ne 'default') {
        $vcvars = $toolsets[$version]
        $selection = "-vcvars_ver=$version"
    }
    foreach ($profile in @('optimized', 'minimal')) {
        $optimization = '/Od /Ob0'
        if ($profile -eq 'optimized') { $optimization = '/O2 /Ob2' }
        $out = Join-Path $report "$version-$profile"
        New-Item -Path $out -ItemType Directory | Out-Null
        $flags = "/nologo /Bv /std:c++latest /EHsc /MD /DNDEBUG /arch:AVX2 /W4 $optimization /FAcs"
        $compile = @"
@echo off
call "$vcvars" $selection
if errorlevel 1 exit /b 3
where cl
echo VCToolsVersion=%VCToolsVersion% WindowsSDKVersion=%WindowsSDKVersion%
echo flags=$flags (default /fp:precise)
cl $flags /I"$root\src" /Faprobe.asm /Feprobe.exe /Foprobe.obj "$root\tests\experiments\state150_scope_bits.cpp"
exit /b %errorlevel%
"@
        $compile | Set-Content -LiteralPath (Join-Path $out 'compile.cmd') -Encoding ascii
        Push-Location $out
        try {
            "toolset=$version profile=$profile observed=$(UtcNow) vcvars=$vcvars"
            & cmd.exe /d /c compile.cmd 2>&1 | Tee-Object -FilePath 'compiler.txt'
            $buildExit = $LASTEXITCODE
            "build_exit=$buildExit" | Tee-Object -FilePath 'result.txt'
            if ($buildExit -ne 0) {
                $harnessErrors++
                continue
            }
            & .\probe.exe 2>&1 | Tee-Object -FilePath 'trace.txt'
            $probeExit = $LASTEXITCODE
            "probe_exit=$probeExit" | Tee-Object -FilePath 'result.txt' -Append
            "binary_sha256=$((Get-FileHash 'probe.exe' -Algorithm SHA256).Hash.ToLowerInvariant())" | Tee-Object -FilePath 'result.txt' -Append
            if ($probeExit -notin 0, 1) { $harnessErrors++ }
        }
        finally { Pop-Location }
    }
}
if ($harnessErrors -ne 0) { throw "$harnessErrors builds or diagnostic controls failed" }
