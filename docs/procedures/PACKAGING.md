# PACKAGING.md

Build-artifact structure, code-signing, and install layout. Source: `.github/workflows/build.yml`
and `packaging/macos/INSTALL.txt`.

## Artifact layout (CI uploads)

| Platform | Artifact | Contents |
|---|---|---|
| Linux | `Anamorph-Linux` | `Anamorph.vst3`, `Anamorph` (Standalone) |
| Windows | `Anamorph-Windows` | `Anamorph.vst3`, `Anamorph.exe` (Standalone) |
| macOS | `Anamorph-macOS` | universal `Anamorph.vst3`, `Anamorph.component` (AU), `Anamorph.app`, `INSTALL.txt` |

Evidence [Verified]: build.yml:47-61,97-114,150-178.

## Plugin identifiers (for host validation)

| Field | Value | Source |
|---|---|---|
| Company | RollyTech | CMakeLists.txt:89 |
| Bundle ID | `com.rollytech.anamorph` | CMakeLists.txt:90 |
| Manufacturer code | `Anmf` | CMakeLists.txt:91 |
| Plugin code | `Anmr` | CMakeLists.txt:92 |
| Product name | Anamorph | CMakeLists.txt:94 |
| VST3 categories | Fx, Spatial, Stereo | CMakeLists.txt:101 |

AU validation (macOS): `auval -v aufx Anmr Anmf` (type=`aufx`, subtype=`Anmr`, manufacturer=`Anmf`).
Evidence [Verified]: packaging/macos/INSTALL.txt:38-40.

## macOS signing & quarantine

CI **ad-hoc** codesigns the bundles (`codesign --force --deep --sign -`) — they are **NOT
notarized**. Gatekeeper quarantines them after download, so the user must remove the quarantine
flag. Evidence [Verified]: build.yml:159-162; INSTALL.txt:4-10.

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

The macOS job verifies both slices are present:
```bash
lipo -archs Anamorph.vst3/Contents/MacOS/Anamorph        # expect: x86_64 arm64
```
Evidence [Verified]: build.yml:168-170.

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
