# PACKAGING.md

Build-artifact structure, code-signing, and install layout. Source: `.github/workflows/build.yml`
and `packaging/macos/INSTALL.txt`.

## Artifact layout (CI uploads)

| Platform | Artifact | Contents |
|---|---|---|
| Linux | `Anamorph-Linux` | stripped `Anamorph.vst3`, `Anamorph` (Standalone) |
| Linux | `Anamorph-Linux-debug` | split debug info (`.debug` files, `.gnu_debuglink`-referenced) |
| Windows | `Anamorph-Windows` | `Anamorph.vst3`, `Anamorph.exe` (Standalone; no PDBs) |
| Windows | `Anamorph-Windows-debug` | linker PDBs for both shipped images |
| macOS | `Anamorph-macOS` | universal stripped `Anamorph.vst3`, `Anamorph.component` (AU), `Anamorph.app`, `INSTALL.txt` |
| macOS | `Anamorph-macOS-debug` | universal dSYM bundles for all three |

Public binaries are **stripped** (RH-PR-2, ADR-0021); the `-debug` artifacts carry the full
symbol/debug information for crash symbolication and must never be redistributed with a release.
Evidence [Verified]: build.yml (stage/upload steps per job).

## Plugin identifiers (for host validation)

| Field | Value | Source |
|---|---|---|
| Company | RollyTech | CMakeLists.txt:151 |
| Bundle ID | `com.rollytech.anamorph` | CMakeLists.txt:152 |
| Manufacturer code | `Anmf` | CMakeLists.txt:153 |
| Plugin code | `Anmr` | CMakeLists.txt:154 |
| Product name | Anamorph | CMakeLists.txt:156 |
| VST3 categories | Fx, Spatial, Stereo | CMakeLists.txt:163 |

AU validation (macOS): `auval -v aufx Anmr Anmf` (type=`aufx`, subtype=`Anmr`, manufacturer=`Anmf`).
Evidence [Verified]: packaging/macos/INSTALL.txt:38-40.

## macOS signing & quarantine

CI **ad-hoc** codesigns the bundles (`codesign --force --deep --sign -`) — they are **NOT
notarized**. Order inside the packaging step (ADR-0021): `dsymutil` (capture dSYMs) → `strip -x`
→ codesign — signing is LAST because stripping afterwards would invalidate the seal. A codesign
failure now fails the job (the former `|| true` swallowing was removed). Gatekeeper quarantines
the bundles after download, so the user must remove the quarantine flag.
Evidence [Verified]: build.yml (Package macOS plugins step); INSTALL.txt:4-10.

Install (from `INSTALL.txt`):
```bash
sudo cp -R "Anamorph.vst3"      /Library/Audio/Plug-Ins/VST3/
sudo cp -R "Anamorph.component" /Library/Audio/Plug-Ins/Components/
# REQUIRED — strip the quarantine flag, or the DAW won't load it:
sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/Anamorph.vst3
sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/Components/Anamorph.component
```
Logic Pro / GarageBand load **AU only** (`.component`); VST3 hosts use the `.vst3`.
Evidence [Verified]: INSTALL.txt:13-33,46-51.

## Universal binary verification (macOS)

The macOS job verifies both slices are present (strict — a missing slice fails the job):
```bash
lipo -archs Anamorph.vst3/Contents/MacOS/Anamorph        # expect: x86_64 arm64
```
Evidence [Verified]: build.yml (Package macOS plugins step).

## Standard plug-in install locations

| Format | macOS | Windows | Linux |
|---|---|---|---|
| VST3 | `/Library/Audio/Plug-Ins/VST3/` | `%CommonProgramFiles%\VST3\` | `~/.vst3/` |
| AU | `/Library/Audio/Plug-Ins/Components/` | — | — |

(macOS paths Verified from INSTALL.txt; Windows/Linux VST3 paths are the platform standards —
not asserted from repo evidence.)

## Not in scope

- **AAX** packaging — **Not Supported** (needs Avid account + PACE/iLok; see
  `docs/policies/COMPATIBILITY_POLICY.md`).
- A graphical installer / `.pkg` / `.msi` — not present in the repository.
  `TODO: no installer build exists; document one here if/when added.`
