; Anamorph Windows installer (Inno Setup 6 — preinstalled on windows-latest).
; Compiled by CI from the validated staging directory:
;   ISCC.exe /DAppVersion=<x.y.z> /DStagingDir=<abs path to dist\Anamorph-Windows> /O<outdir> packaging\windows\Anamorph.iss
; StagingDir is the CI-validated customer payload (Anamorph.vst3\ bundle +
; Anamorph.exe, PDB-purged) — the installer repacks those exact files.
; The installer is NOT Authenticode-signed yet; RH-PR-5 signs this same exe.
;
; Wizard flow: component selection (Install VST3 / Install Standalone, both
; pre-selected, at least one required) → one destination page carrying BOTH
; install paths (VST3 folder above the Standalone folder) → install. The
; standard single-directory page is disabled; the custom destination page
; below replaces it, and the chosen Standalone folder is written back to
; {app} so the uninstaller, Start-menu icon and registry entry stay coherent.

#ifndef AppVersion
  #error AppVersion must be passed with /DAppVersion=x.y.z
#endif
#ifndef StagingDir
  #error StagingDir must be passed with /DStagingDir=<staged customer dir>
#endif

[Setup]
; ArchitecturesAllowed/InstallIn64BitMode use the `x64compatible` identifier,
; which requires Inno Setup >= 6.3 (2024). Validated: windows-latest ships the
; 6.7.1 compiler engine and compiles this script successfully in the
; "Package Windows installer (Inno Setup)" step of build.yml's windows job.
; Stable AppId: upgrades and uninstalls must always target the same product.
AppId={{D1E3D8F8-C9CE-415C-AC73-A6AA842987BD}
AppName=Anamorph
AppVersion={#AppVersion}
AppPublisher=RollyTech
AppPublisherURL=https://www.rolly.tech
DefaultDirName={autopf}\Anamorph
DefaultGroupName=Anamorph
DisableProgramGroupPage=yes
; The standard directory page only knows ONE path ({app}) and never says what
; it is for; the custom destination page in [Code] shows both clearly-labelled
; paths (VST3 first) instead.
DisableDirPage=yes
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

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "Install VST3"; Types: full custom
Name: "standalone"; Description: "Install Standalone"; Types: full custom

[Files]
Source: "{#StagingDir}\Anamorph.vst3\*"; DestDir: "{code:GetVst3Dir}\Anamorph.vst3"; Components: vst3; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#StagingDir}\Anamorph.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
; The installer payload is deliberately lean: only what the user needs to run
; the product -- it carries no attribution file, and INSTALL.txt (which this
; installer does not install either) is installation-only since 2026-07-26.
; Third-party attribution (NOTICE, THIRD_PARTY_LICENSES.md) and SUPPORT.md
; accompany every download as release-page assets instead, and those assets are
; the sole carrier of the mandatory IJG acknowledgement.

[Icons]
Name: "{group}\Anamorph"; Filename: "{app}\Anamorph.exe"; Components: standalone
Name: "{group}\Uninstall Anamorph"; Filename: "{uninstallexe}"

[Code]
var
  DestPage: TInputDirWizardPage;

procedure InitializeWizard;
begin
  // One destination page for both components, shown right after the component
  // page. Index 0 (top) = VST3 folder, index 1 = Standalone folder.
  // NOTE: comments in this section are //-style throughout — Pascal { } comments
  // do not nest, so a literal constant name like the ones expanded below would
  // terminate a brace comment early and break the ISCC compile.
  DestPage := CreateInputDirPage(wpSelectComponents,
    'Select Destination Locations',
    'Where should the selected components be installed?',
    'Setup will install each selected component into its folder below.' + #13#10 +
    'To continue, click Next. To pick different folders, click Browse.',
    False, 'Anamorph');
  DestPage.Add('VST3 Plug-in folder (the plug-in installs as Anamorph.vst3 inside it):');
  DestPage.Add('Standalone Application folder:');
  DestPage.Values[0] := ExpandConstant('{commoncf64}\VST3');
  DestPage.Values[1] := WizardDirValue;   // previous install dir, else the DefaultDirName default
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  // Only the paths of selected components are editable.
  if (DestPage <> nil) and (CurPageID = DestPage.ID) then
  begin
    DestPage.PromptLabels[0].Enabled := WizardIsComponentSelected('vst3');
    DestPage.Edits[0].Enabled        := WizardIsComponentSelected('vst3');
    DestPage.Buttons[0].Enabled      := WizardIsComponentSelected('vst3');
    DestPage.PromptLabels[1].Enabled := WizardIsComponentSelected('standalone');
    DestPage.Edits[1].Enabled        := WizardIsComponentSelected('standalone');
    DestPage.Buttons[1].Enabled      := WizardIsComponentSelected('standalone');
  end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpSelectComponents then
  begin
    // At least one component must be selected before continuing.
    if WizardSelectedComponents(False) = '' then
    begin
      MsgBox('Select at least one component to install.', mbError, MB_OK);
      Result := False;
    end;
  end
  else if (DestPage <> nil) and (CurPageID = DestPage.ID) then
  begin
    if WizardIsComponentSelected('vst3') and (Trim(DestPage.Values[0]) = '') then
    begin
      MsgBox('Enter a folder for the VST3 Plug-in.', mbError, MB_OK);
      Result := False;
      exit;
    end;
    if WizardIsComponentSelected('standalone') and (Trim(DestPage.Values[1]) = '') then
    begin
      MsgBox('Enter a folder for the Standalone Application.', mbError, MB_OK);
      Result := False;
      exit;
    end;
    // Feed the chosen Standalone folder back into the app directory constant
    // (the dir page is disabled, so this edit is the only writer). The
    // uninstaller and the Start-menu icon resolve against it.
    WizardForm.DirEdit.Text := DestPage.Values[1];
  end;
end;

function GetVst3Dir(Param: string): string;
begin
  Result := DestPage.Values[0];
end;
