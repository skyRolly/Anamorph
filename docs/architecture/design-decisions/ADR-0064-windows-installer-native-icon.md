# ADR-0064 — Approved native installer icon and unconditional uninstall resource

**Status:** Accepted — explicit final owner approval of ADR-0064 and the installer
arrangement merged through PR #172, including the stable uninstall icon for VST3-only
installations. **Architecture Review Gate: CLEARED; owner decision: APPROVED.**
See [ADR_INDEX.md](ADR_INDEX.md#owner-approval-reconciliation--record-updated-2026-10-24)
for provenance/ancestry. The original final-approval date is not recorded; the record update
is 2026-10-24 (Asia/Shanghai). The 2026-10-23 B/C/E implementation/testing permission
remains historical provenance. Release/deployment and future architecture are separate.

## Context

The existing installer points UninstallDisplayIcon at an optional Standalone EXE. VST3-only
installs therefore register an icon path that does not exist. B0 has a separately approved
Windows ICO; PR #170 intentionally deferred installer branding. Namespace PR #171 precedes this.

## Problem

Use the approved icon for Setup/Uninstall and provide a stable installed display-icon file for
all legal component combinations, without changing installer identity, destinations or UI.

## Options

- Keep the optional EXE icon path: broken for VST3-only installs.
- Attach an ICO to the Standalone component: the same liveness defect.
- Use only the generated uninstaller EXE as the registered icon: its filename is installer-managed;
  an explicit stable resource is easier to inspect independently of optional components.
- **Install one unconditional product-owned ICO and reference it: accepted.**
- Generate a new AppId, add fake app payloads, broad delete rules or wizard art: rejected as
  unnecessary identity/safety/UI changes outside the task.

## Decision

- Vendor exact `assets/anamorph/windows/Anamorph.ico` from icon repository commit
  `35cf982f9d2e33bde5c4a1bf84b17205ce1e6b80`, SHA-256
  `17c2f958e07de4d0e1bcfd8d6e67f1e254368b7ea11764e12eca8da2742fbdf1`.
  No asset download during builds. Preserve B0 pixels/geometry and all existing PNG inputs.
- Use SourcePath-anchored SetupIconFile; ISCC resolves the checked-in ICO from the script's
  directory. Missing/wrong source fails the contract before compiling; native MAINICON checks
  compare frame headers and payloads after compilation. No generic fallback is accepted.
- Install `{app}\resources\Anamorph.ico` without a Components/Check restriction and use that
  exact UninstallDisplayIcon. `{app}` already owns the uninstaller; no optional EXE is added.
  ignoreversion overwrites the same product-owned icon on reinstall. Inno's per-file uninstall
  log removes the ICO and empty directories, preserving unmanaged resources.
- Set the installed VST3 bundle directory's System attribute, scoped to the VST3 component,
  using the existing GetVst3Dir destination. Its existing Plugin.ico/desktop.ini remain intact.
- Preserve AppId `{D1E3D8F8-C9CE-415C-AC73-A6AA842987BD}`, namespace, component set, user-selectable
  destinations, product version, signing policy and installer UI. No wildcard cleanup.

## Consequences and ownership

All three component choices have a real registered ICO and branded uninstaller. Inno writes
MAINICON from the supplied ICO bytes; the independent approved ICO has nine PNG frames
(16/20/24/32/40/48/64/128/256), with planes=0. JUCE's distinct generated four-frame ICO uses
planes=1. A separate validator handles the approved container without weakening #170's checker.
No wizard image, Composer artifact, DSP/UI/state/parameter/latency or format change.

## Merged implementation and original review order

The original stack was #170 → #171 → #172. After ordinary prerequisite merges, #172
merged as `1e17f5845a90919949435c60d6ddf2bf2bd3fde1`, the inspected main. The owner
approves that existing arrangement unchanged; architecture/acceptance blockers are closed.
The implementation's exact native evidence and remaining manual limits are recorded below.

## Related code

`packaging/windows/Anamorph.iss`, `resources/icons/windows/PROVENANCE.json`,
`scripts/check-installer-icon.py`, `scripts/test-windows-installer.ps1`, `.github/workflows/build.yml`.

## Evidence and confidence

Source hashes/contract, primary-group good/bad fixtures and snapshot liveness are **Verified**.
The pinned JUCE RC generator and Inno's UpdateIconsAndStyle path were inspected. Actual ISCC,
Setup/uninstaller PE resources and install/reinstall/uninstall results require native Windows CI.
The native script refuses developer/self-hosted machines and pre-existing product installs;
it uses only a disposable GitHub-hosted Windows VM and isolated app/menu destinations.
All three choices are installed, reinstalled over a deliberately obsolete icon, then uninstalled,
checking native payload copies, registry icon, shortcuts, System attribute, owned cleanup and
unmanaged sentinels. GUI destination selection, shell display/DPI and user DAWs remain manual.
Native Windows [run 40011141822](https://github.com/skyRolly/Anamorph/actions/runs/40011141822)
at `cac80161f578dbd570631b3e5c2647f690a0c1ff` passed Setup/uninstaller resources,
six install/reinstall snapshots and three safe uninstalls across all component choices.
Artifact 12526403791 records failure=null; its ZIP SHA-256 is
`520af953866654edf046cf9a2cba7d96c8598758d07a0365b4167b495348f28c`.
This disposable-VM evidence does not verify Explorer/DPI, chooser UI, user DAWs or broader
older-installer upgrades. The [worklog](../../../worklogs/2026-10-23-windows-installer-icon.md)
preserves the earlier results and limits.
