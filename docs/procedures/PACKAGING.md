# PACKAGING.md

Build-artifact structure, installers, code-signing, and install layout. Source:
`.github/workflows/build.yml` and `packaging/` (per-platform assets: `linux/`, `windows/`,
`macos/`). End-user instructions live in `docs/user/INSTALLATION.md`.

## Artifact layout (CI uploads)

| Platform | Artifact | Contents |
|---|---|---|
| Linux | `Anamorph-Linux` | `Anamorph-Linux.zip` (single archive: stripped `Anamorph.vst3`, `Anamorph` Standalone, `INSTALL.txt`) |
| Linux | `Anamorph-Linux-package` | `Anamorph-<version>-Linux.tar.gz` (the zip contents + `install.sh`/`uninstall.sh`, under a versioned top-level dir) |
| Linux | `Anamorph-Linux-debug` | split debug info (`.debug` files, `.gnu_debuglink`-referenced) |
| Windows | `Anamorph-Windows` | `Anamorph-Windows.zip` (single archive: `Anamorph.vst3`, `Anamorph.exe` Standalone, `INSTALL.txt`; no PDBs) |
| Windows | `Anamorph-Windows-installer` | `Anamorph-<version>-Windows-Installer.exe` (Inno Setup, built from the same staged payload) |
| Windows | `Anamorph-Windows-debug` | linker PDBs for both shipped images |
| macOS | `Anamorph-macOS` | `Anamorph-macOS.zip` (single `ditto` archive: universal stripped `Anamorph.vst3`, `Anamorph.component` (AU), `Anamorph.app`, `INSTALL.txt`) |
| macOS | `Anamorph-macOS-installer` | `Anamorph-<version>-macOS.pkg` (pkgbuild/productbuild, built from the same staged payload) |
| macOS | `Anamorph-macOS-debug` | universal dSYM bundles (best-effort under Release+LTO — may be absent, with a CI warning) |

Customer artifacts are **archived at the source** (Info-ZIP `-ry` on Linux, `Compress-Archive`
on Windows, `ditto -c -k` on macOS) because the artifact transport itself does not preserve
Unix file permissions or symlinks — the archive bytes do. Extract with `unzip` (Linux) /
Explorer (Windows) / double-click or `ditto -x -k` (macOS); executable bits and the signed
macOS bundle layout are intact inside. `release.yml` publishes these exact archive bytes
untouched (renamed to `Anamorph-<version>-<OS>.zip`).

## Installable packages (v0.9.0)

Each platform job additionally builds a **user-installable package** from the *same*
validated staging directory the zip was archived from, in a separate, additively gated step
(`package_linux` / `package_windows` / `package_macos_pkg`), each with the version parsed
from `CMakeLists.txt` embedded in a deterministic file name:

- **Linux** — `Anamorph-<version>-Linux.tar.gz`: the staged payload plus
  `packaging/linux/install.sh` / `uninstall.sh` / `INSTALL.txt` under a versioned top-level
  directory. `install.sh` installs user-locally (`~/.vst3`, `~/.local/bin`) with no root;
  a CI self-check asserts the executable bit survived into the tarball.
- **Windows** — `Anamorph-<version>-Windows-Installer.exe`: compiled by the preinstalled
  Inno Setup 6 (`ISCC.exe`) from `packaging/windows/Anamorph.iss` (stable `AppId`; VST3 →
  `{commoncf64}\VST3`, Standalone → Program Files + Start-menu, real uninstall entry;
  requires elevation). Not yet Authenticode-signed — RH-PR-5 signs this same exe.
- **macOS** — `Anamorph-<version>-macOS.pkg`: `packaging/macos/build-pkg.sh` builds three
  component packages (VST3 → `/Library/Audio/Plug-Ins/VST3`, AU → `.../Components`, app →
  `/Applications`) and combines them with `productbuild`; a self-check expands the result
  and asserts all three component identifiers. Payloads installed by Installer.app carry no
  quarantine attribute (unlike zip-extracted bundles). Not yet signed/notarized — RH-PR-3
  signs + notarizes this same package.

`release.yml` downloads the three package artifacts alongside the zips, **fail-closes on a
missing or version-skewed file name**, moves them into the draft release unmodified, and
covers them in `SHA256SUMS.txt`. The user manual (`docs/user/USER_MANUAL.md`) is attached
as `Anamorph-<version>-UserManual.md`.

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

(All three now asserted from repo evidence: the per-platform `packaging/<os>/INSTALL.txt`
files and the installer destinations in `packaging/windows/Anamorph.iss` /
`packaging/macos/build-pkg.sh` / `packaging/linux/install.sh`. The Windows installer uses
the per-machine `{commoncf64}\VST3`; the Linux install script uses the per-user `~/.vst3`.)

## Not in scope

- **AAX** packaging — **Not Supported** (needs Avid account + PACE/iLok; see
  `docs/policies/COMPATIBILITY_POLICY.md`).
- **Code signing / notarization** of the packages — the Inno Setup exe and the `.pkg` are
  unsigned until RH-PR-3 (macOS) / RH-PR-5 (Windows); the user-facing consequences
  (SmartScreen / Gatekeeper prompts) are documented in the per-platform `INSTALL.txt`
  files and `docs/user/INSTALLATION.md`.
