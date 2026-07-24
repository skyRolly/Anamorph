; Anamorph Windows installer (Inno Setup 6 — preinstalled on windows-latest).
; Compiled by CI from the validated staging directory:
;   ISCC.exe /DAppVersion=<x.y.z> /DStagingDir=<abs path to dist\Anamorph-Windows> /O<outdir> packaging\windows\Anamorph.iss
; StagingDir is the CI-validated customer payload (Anamorph.vst3\ bundle +
; Anamorph.exe, PDB-purged) — the installer repacks those exact files.
; The installer is NOT Authenticode-signed yet; RH-PR-5 signs this same exe.

#ifndef AppVersion
  #error AppVersion must be passed with /DAppVersion=x.y.z
#endif
#ifndef StagingDir
  #error StagingDir must be passed with /DStagingDir=<staged customer dir>
#endif

[Setup]
; Stable AppId: upgrades and uninstalls must always target the same product.
AppId={{D1E3D8F8-C9CE-415C-AC73-A6AA842987BD}
AppName=Anamorph
AppVersion={#AppVersion}
AppPublisher=RollyTech
AppPublisherURL=https://www.rolly.tech
DefaultDirName={autopf}\Anamorph
DefaultGroupName=Anamorph
DisableProgramGroupPage=yes
OutputBaseFilename=Anamorph-{#AppVersion}-Windows-Installer
Compression=lzma2
SolidCompression=yes
; The CI build is x64-only; install the VST3 into the 64-bit Common Files tree.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; {commoncf64}\VST3 requires elevation.
PrivilegesRequired=admin
UninstallDisplayIcon={app}\Anamorph.exe
WizardStyle=modern

[Files]
Source: "{#StagingDir}\Anamorph.vst3\*"; DestDir: "{commoncf64}\VST3\Anamorph.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#StagingDir}\Anamorph.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Anamorph"; Filename: "{app}\Anamorph.exe"
Name: "{group}\Uninstall Anamorph"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\Anamorph.exe"; Description: "Launch Anamorph (Standalone)"; Flags: nowait postinstall skipifsilent unchecked
