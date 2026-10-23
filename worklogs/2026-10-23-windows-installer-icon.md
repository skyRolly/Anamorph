# Windows installer icon integration — 2026-10-23

Starting main `098884e7fe12049010f2eaf8379e1514efdad0ab`.
Branch `codex/windows-installer-native-icon`, PR base `codex/namespace-tech-rolly-anamorph`
(#171), after corrected #170. This is an installer-only review diff. ADR-0064 is Proposed;
owner implementation/testing permission is recorded, independent human review remains owed.

The prior UninstallDisplayIcon pointed at optional `{app}\Anamorph.exe`. Exact approved B0 ICO
from source main `35cf982` is now checked in at `resources/icons/windows/Anamorph.ico`
(60131 bytes, SHA-256 `17c2f958e07de4d0e1bcfd8d6e67f1e254368b7ea11764e12eca8da2742fbdf1`).
It is installed unconditionally at `{app}\resources\Anamorph.ico`, without optional Standalone
payload. AppId and destination functions remain unchanged. VST3 directory System attribution
is applied to the selected VST3 destination; no global cleanup/signing/wizard graphics changes.

## Source and liveness

```sh
python3 scripts/check-installer-icon.py --self-test
python3 scripts/check-installer-icon.py
python3 scripts/test-icon-preflight.py
pwsh -NoProfile -File scripts/test-windows-installer.ps1 -SelfTest
```

Local native-tool-independent checks pass: approved ICO/provenance/path/ownership contract,
matching-platform MAINICON positive/negative fixtures (only resource API stubbed), inherited
42 icon fixtures and five shell preflight regressions. PowerShell 7.6.8 (official archive hash
verified) parses the actual native test script and runs three valid snapshots/32 seeded invalid
snapshots through the production assertion function. The real VERSIONINFO reader's fields are
inherited from #171; installer work changes no independent plugin identity.

Inno UpdateIconsAndStyle builds `MAINICON` by copying ICO directory headers/payloads and assigning
RT_ICON IDs from 100; SetupIconFile applies to both setup and uninstaller images. The approved
Pillow container has planes=0, unlike JUCE's planes=1. The dedicated parser preserves this valid
format and compares native primary-group bytes; #170's pinned-JUCE parser is not loosened.

## Native Windows gate

After ISCC, inspect the actual Setup EXE MAINICON. On a fresh GitHub-hosted VM, the behavior
script first rejects any existing Anamorph registration, default payload or VST3 bundle. It
then tests VST3-only, Standalone-only and both, each with an isolated /DIR and /GROUP. Each is
reinstalled after corrupting the installed ICO to prove replacement. Registry DisplayIcon,
uninstaller MAINICON, optional app shortcut, unconditional uninstall shortcut, installed native
payload hashes, VST3 sidecars/System attribute and registered AppId are checked. Uninstall
must remove owned files/registration/menu while preserving an unmanaged resource inside the
app and a different product's sentinel beside the VST3. Only exact test-created sentinel paths
are cleaned; installer logs remain in the isolated temporary directory. No user installation is
used. This is same-version reinstall, not a plugin version bump or a published installer.

The upload of the installer requires these checks. Setup and behavior JSON evidence is retained
as a CI artifact. Native results are pending until the current-head job executes; synthetic
fixtures do not certify a Windows install. Custom VST3 chooser UI, shell icon caches/DPI and real
DAW appearance remain manual. User-selectable destination code is unchanged.
