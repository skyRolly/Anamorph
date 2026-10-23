[CmdletBinding()]
param(
    [switch]$SelfTest,
    [string]$Installer,
    [string]$StagingDir,
    [string]$Report = 'build/windows-installer-behavior.json'
)
$ErrorActionPreference = 'Stop'
$ApprovedHash = '17C2F958E07DE4D0E1BCFD8D6E67F1E254368B7EA11764E12ECA8DA2742FBDF1'
$AppId = '{D1E3D8F8-C9CE-415C-AC73-A6AA842987BD}_is1'

function Assert-InstalledSnapshot($Snapshot, [string[]]$Components) {
    if (-not $Snapshot.Registered) { throw 'missing uninstall registration' }
    if (-not $Snapshot.IconExists -or $Snapshot.IconHash -ne $ApprovedHash) { throw 'missing or altered unconditional installed icon' }
    if (-not $Snapshot.DisplayIconCorrect) { throw 'uninstall DisplayIcon does not resolve to installed ICO' }
    if (-not $Snapshot.UninstallerExists) { throw 'registered uninstaller missing' }
    $app = $Components -contains 'standalone'
    $vst = $Components -contains 'vst3'
    if ($Snapshot.AppExists -ne $app -or $Snapshot.AppShortcut -ne $app) { throw 'Standalone payload/shortcut does not match component selection' }
    if ($Snapshot.VstExists -ne $vst) { throw 'VST3 payload does not match component selection' }
    if (-not $Snapshot.UninstallShortcut) { throw 'uninstall shortcut missing or incorrect' }
    if (-not $Snapshot.NativeCopiesMatch) { throw 'installed native payload/resource bytes differ from staging' }
    if ($vst -and -not $Snapshot.VstSystemAttribute) { throw 'VST3 System attribute missing' }
}

function Registered-UninstallerPath([string]$Command, [string]$AppDirectory) {
    # Inno AddQuotes quotes only paths containing spaces.
    if ($Command -match '^"([^"\r\n]+\.exe)"(?:\s.*)?$') { $path = $Matches[1] }
    elseif ($Command -match '^([^"\r\n]+?\.exe)(?:\s.*)?$') { $path = $Matches[1] }
    else { throw 'unexpected uninstall command syntax' }
    if ([IO.Path]::GetDirectoryName($path) -ne $AppDirectory -or [IO.Path]::GetFileName($path) -notmatch '^unins[0-9]+\.exe$') {
        throw 'uninstaller is outside the isolated app directory or has an unexpected name'
    }
    return $path
}

function Test-SnapshotLiveness {
    $fixtureApp = Join-Path ([IO.Path]::GetTempPath()) 'Anamorph-parser-fixture'
    $fixtureExe = Join-Path $fixtureApp 'unins000.exe'
    foreach ($command in @($fixtureExe, ('"' + $fixtureExe + '"'), ('"' + $fixtureExe + '" /SILENT'))) {
        if ((Registered-UninstallerPath $command $fixtureApp) -ne $fixtureExe) { throw 'registered uninstaller parser rejected valid path' }
    }
    foreach ($command in @('malformed', (Join-Path ([IO.Path]::GetTempPath()) 'unins000.exe'), (Join-Path $fixtureApp 'unrelated.exe'))) {
        $rejected = $false
        try { Registered-UninstallerPath $command $fixtureApp | Out-Null } catch { $rejected = $true }
        if (-not $rejected) { throw 'registered uninstaller parser accepted unsafe path' }
    }
    $count = 0
    foreach ($components in @(@('vst3'), @('standalone'), @('vst3', 'standalone'))) {
        $valid = @{
            Registered = $true; IconExists = $true; IconHash = $ApprovedHash
            DisplayIconCorrect = $true; UninstallerExists = $true
            AppExists = ($components -contains 'standalone'); AppShortcut = ($components -contains 'standalone')
            VstExists = ($components -contains 'vst3'); UninstallShortcut = $true
            NativeCopiesMatch = $true; VstSystemAttribute = $true
        }
        Assert-InstalledSnapshot $valid $components
        $fields = @('Registered', 'IconExists', 'IconHash', 'DisplayIconCorrect', 'UninstallerExists', 'AppExists', 'AppShortcut', 'VstExists', 'UninstallShortcut', 'NativeCopiesMatch')
        if ($components -contains 'vst3') { $fields += 'VstSystemAttribute' }
        foreach ($field in $fields) {
            $bad = $valid.Clone()
            if ($field -eq 'IconHash') { $bad[$field] = 'altered' } else { $bad[$field] = -not $bad[$field] }
            $rejected = $false
            try { Assert-InstalledSnapshot $bad $components } catch { $rejected = $true }
            if (-not $rejected) { throw "installer snapshot gate accepted mutant $field ($components)" }
            $count++
        }
    }
    Write-Host "Windows installer snapshot self-test: 3 valid component choices, $count seeded failures rejected"
}

Test-SnapshotLiveness
if ($SelfTest) { return }
# Native installation is allowed only on a disposable GitHub-hosted Windows VM.
# A developer machine/self-hosted runner must never be used as the test target.
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted' -or $env:RUNNER_OS -ne 'Windows') {
    throw 'native installer tests require a disposable GitHub-hosted Windows runner'
}
if (-not $Installer -or -not $StagingDir) { throw '-Installer and -StagingDir are required' }
$Installer = (Resolve-Path -LiteralPath $Installer).Path
$StagingDir = (Resolve-Path -LiteralPath $StagingDir).Path
$repo = Split-Path -Parent $PSScriptRoot
$reg = "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\$AppId"
$reg32 = "HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\$AppId"
$vstParent = Join-Path ${env:CommonProgramW6432} 'VST3'
$vst = Join-Path $vstParent 'Anamorph.vst3'
if ((Test-Path -LiteralPath $reg) -or (Test-Path -LiteralPath $reg32) -or (Test-Path -LiteralPath $vst) -or (Test-Path -LiteralPath (Join-Path $env:ProgramW6432 'Anamorph'))) {
    throw 'existing Anamorph registration/payload detected; refusing to modify an existing installation'
}
& python (Join-Path $repo 'scripts/check-installer-icon.py') --self-test
if ($LASTEXITCODE -ne 0) { throw 'installer icon self-test failed' }
& python (Join-Path $repo 'scripts/check-installer-icon.py') --setup-exe $Installer
if ($LASTEXITCODE -ne 0) { throw 'Setup MAINICON validation failed' }
$runId = [Guid]::NewGuid().ToString('N')
$testRoot = Join-Path $env:RUNNER_TEMP "Anamorph-installer-$runId"
New-Item -ItemType Directory -Path $testRoot | Out-Null
$outside = Join-Path $testRoot 'outside-owned-paths.keep'
Set-Content -LiteralPath $outside -Value 'unrelated resource'
$otherVst = Join-Path $vstParent "OtherProduct-$runId.vst3"
New-Item -ItemType Directory -Path $otherVst | Out-Null
$otherFile = Join-Path $otherVst 'unmanaged.keep'
Set-Content -LiteralPath $otherFile -Value 'other product sentinel'
$results = [System.Collections.Generic.List[object]]::new()
$failure = $null

function Invoke-CheckedProcess([string]$Executable, [string[]]$Arguments) {
    $process = Start-Process -FilePath $Executable -ArgumentList $Arguments -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "process failed ($($process.ExitCode)): $Executable" }
}

function Shortcut-Target([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    $shell = New-Object -ComObject WScript.Shell
    return $shell.CreateShortcut($Path).TargetPath
}

function Equal-File([string]$A, [string]$B) {
    return (Test-Path -LiteralPath $A -PathType Leaf) -and (Test-Path -LiteralPath $B -PathType Leaf) -and
        ((Get-FileHash -LiteralPath $A -Algorithm SHA256).Hash -eq (Get-FileHash -LiteralPath $B -Algorithm SHA256).Hash)
}

try {
    foreach ($components in @(@('vst3'), @('standalone'), @('vst3', 'standalone'))) {
        $label = $components -join '-'
        $app = Join-Path $testRoot $label
        $icon = Join-Path $app 'resources\Anamorph.ico'
        $exe = Join-Path $app 'Anamorph.exe'
        $groupName = "Anamorph-CI-$runId-$label"
        $group = Join-Path ([Environment]::GetFolderPath('CommonPrograms')) $groupName
        if (Test-Path -LiteralPath $group) { throw 'test menu group already exists' }
        $arguments = @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', '/TYPE=custom',
                       "/COMPONENTS=`"$($components -join ',')`"", "/DIR=`"$app`"", "/GROUP=`"$groupName`"")
        for ($lap = 1; $lap -le 2; $lap++) {
            if ($lap -eq 2) {
                # Prove ignoreversion overwrites an obsolete icon on reinstall.
                Set-Content -LiteralPath $icon -Value 'obsolete development icon'
            }
            $log = Join-Path $testRoot "$label-install-$lap.log"
            Invoke-CheckedProcess $Installer ($arguments + "/LOG=`"$log`"")
            $registration = Get-ItemProperty -LiteralPath $reg
            $uninstaller = Registered-UninstallerPath $registration.UninstallString $app
            $displayIcon = ($registration.DisplayIcon -replace ',0$', '').Trim('"')
            $copies = $true
            if ($components -contains 'standalone') { $copies = $copies -and (Equal-File $exe (Join-Path $StagingDir 'Anamorph.exe')) }
            if ($components -contains 'vst3') {
                foreach ($relative in @('Plugin.ico', 'desktop.ini', 'Contents\x86_64-win\Anamorph.vst3')) {
                    $copies = $copies -and (Equal-File (Join-Path $vst $relative) (Join-Path (Join-Path $StagingDir 'Anamorph.vst3') $relative))
                }
            }
            $snapshot = @{
                Registered = (Test-Path -LiteralPath $reg); IconExists = (Test-Path -LiteralPath $icon -PathType Leaf)
                IconHash = (Get-FileHash -LiteralPath $icon -Algorithm SHA256).Hash
                DisplayIconCorrect = ($displayIcon -eq $icon); UninstallerExists = (Test-Path -LiteralPath $uninstaller -PathType Leaf)
                AppExists = (Test-Path -LiteralPath $exe -PathType Leaf)
                AppShortcut = ((Shortcut-Target (Join-Path $group 'Anamorph.lnk')) -eq $exe)
                VstExists = (Test-Path -LiteralPath (Join-Path $vst 'Contents\x86_64-win\Anamorph.vst3') -PathType Leaf)
                UninstallShortcut = ((Shortcut-Target (Join-Path $group 'Uninstall Anamorph.lnk')) -eq $uninstaller)
                NativeCopiesMatch = $copies
                VstSystemAttribute = ((Test-Path -LiteralPath $vst) -and (((Get-Item -LiteralPath $vst -Force).Attributes -band [IO.FileAttributes]::System) -ne 0))
            }
            $snapshot.MenuGroup = $group
            $snapshot.ExpectedUninstaller = $uninstaller
            $snapshot.AppShortcutTarget = Shortcut-Target (Join-Path $group 'Anamorph.lnk')
            $snapshot.UninstallShortcutTarget = Shortcut-Target (Join-Path $group 'Uninstall Anamorph.lnk')
            $snapshot.MenuEntries = @(Get-ChildItem -LiteralPath $group -Force -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
            Write-Host ($snapshot | ConvertTo-Json -Depth 4)
            Assert-InstalledSnapshot $snapshot $components
            & python (Join-Path $repo 'scripts/check-installer-icon.py') --uninstall-exe $uninstaller
            if ($LASTEXITCODE -ne 0) { throw 'installed uninstaller MAINICON differs from approved B0' }
            $results.Add(@{ components = $components; phase = $(if ($lap -eq 1) { 'install' } else { 'reinstall-over-obsolete-icon' }); snapshot = $snapshot })
        }
        # Uninstall must remove owned resources while preserving unmanaged files.
        $unmanaged = Join-Path $app 'resources\unmanaged.keep'
        Set-Content -LiteralPath $unmanaged -Value 'unmanaged resource must survive'
        Invoke-CheckedProcess $uninstaller @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/LOG=`"$(Join-Path $testRoot "$label-uninstall.log")`"")
        $deadline = [DateTime]::UtcNow.AddSeconds(30)
        while ((Test-Path -LiteralPath $uninstaller) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 100 }
        if ((Test-Path -LiteralPath $reg) -or (Test-Path -LiteralPath $reg32) -or (Test-Path -LiteralPath $icon) -or (Test-Path -LiteralPath $exe) -or (Test-Path -LiteralPath $vst) -or (Test-Path -LiteralPath $group)) { throw "owned registration/payload/menu remains after $label uninstall" }
        foreach ($sentinel in @($outside, $otherFile, $unmanaged)) {
            if (-not (Test-Path -LiteralPath $sentinel -PathType Leaf)) { throw "uninstall removed an unmanaged resource: $sentinel" }
        }
        $residual = @(Get-ChildItem -LiteralPath $app -Recurse -Force -File)
        if ($residual.Count -ne 1 -or $residual[0].FullName -ne $unmanaged) { throw 'unexpected residual product files after uninstall' }
        $results.Add(@{ components = $components; phase = 'uninstall'; remaining_owned_files = @(); unmanaged_resources_preserved = $true; expected_residual = $unmanaged })
        # Exact files/directories created by this test only; no recursive/global cleanup.
        Remove-Item -LiteralPath $unmanaged
        Remove-Item -LiteralPath (Join-Path $app 'resources')
        Remove-Item -LiteralPath $app
    }
} catch {
    $failure = $_.Exception.Message
    if ($log -and (Test-Path -LiteralPath $log -PathType Leaf)) {
        Write-Host 'Installer log (last 100 lines):'
        Get-Content -LiteralPath $log -Tail 100 | ForEach-Object { Write-Host $_ }
    }
    throw
} finally {
    $reportPath = [IO.Path]::GetFullPath($Report)
    New-Item -ItemType Directory -Path (Split-Path -Parent $reportPath) -Force | Out-Null
    @{ installer = $Installer; AppId = $AppId; source_ICO_sha256 = $ApprovedHash; tests = @($results.ToArray()); failure = $failure; last_snapshot = $snapshot; isolated_logs = $testRoot } |
        ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $reportPath -Encoding utf8
    if (Test-Path -LiteralPath $otherFile -PathType Leaf) { Remove-Item -LiteralPath $otherFile }
    if ((Test-Path -LiteralPath $otherVst) -and @(Get-ChildItem -LiteralPath $otherVst -Force).Count -eq 0) { Remove-Item -LiteralPath $otherVst }
}
Write-Host 'Windows installer: all 3 component choices, reinstall, icons/shortcuts/registration and safe uninstall verified'
