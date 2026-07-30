# PACKAGING.md

Build-artifact structure, installers, code-signing, and install layout. Source:
`.github/workflows/build.yml` and `packaging/` (per-platform assets: `linux/`, `windows/`,
`macos/`). End-user instructions live in `docs/user/INSTALLATION.md`.

## Artifact layout (CI uploads)

| Platform | Artifact | Contents |
|---|---|---|
| Linux | `Anamorph-Linux` | the staged files themselves: stripped `Anamorph.vst3`, `Anamorph` Standalone, `install.sh`/`uninstall.sh`, `INSTALL.txt` — the artifact's downloaded zip contains them **directly** (extract once, no wrapper folder, no nested archive) |
| Linux | `Anamorph-Linux-debug` | split debug info (`.debug` files, `.gnu_debuglink`-referenced) |
| Windows | `Anamorph-Windows` | the staged files themselves: `Anamorph.vst3`, `Anamorph.exe` Standalone, `INSTALL.txt` (no PDBs) — extract once, payload directly |
| Windows | `Anamorph-Windows-installer` | `Anamorph-<version>-Windows-Installer.exe` (Inno Setup, built from the same staged payload) |
| Windows | `Anamorph-Windows-debug` | linker PDBs for both shipped images |
| macOS | `Anamorph-macOS` | the staged files themselves: universal stripped `Anamorph.vst3`, `Anamorph.component` (AU), `Anamorph.app`, `INSTALL.txt` — extract once, payload directly |
| macOS | `Anamorph-macOS-installer` | `Anamorph-<version>-macOS.pkg` (pkgbuild/productbuild, built from the same staged payload) |
| macOS | `Anamorph-macOS-debug` | universal dSYM bundles (best-effort under Release+LTO — may be absent, with a CI warning) |

Each platform ships **one artifact from one validated staging dir**. The
`Anamorph-<OS>` artifact uploads the staged **files**, so the zip GitHub serves for it
contains the payload at its root — extracting shows the packaged files immediately, with
no wrapper folder and no nested archive. The trade-off: the artifact transport does not
preserve Unix file permissions, so on Linux/macOS the executable bits are lost on that
route — `INSTALL.txt` documents the `sh install.sh` / `chmod +x` fallbacks (the install
scripts themselves `chmod 755` what they install). `release.yml` archives that **same**
tree into `Anamorph-<version>-<OS>.zip`: it restores the executable bits on the payload
paths first (`Anamorph`, `install.sh`, `uninstall.sh`, `*.so` on Linux;
`*/Contents/MacOS/*` on macOS — Windows carries no Unix modes), writes the entries at the
archive root, then **fail-closes** unless every expected executable is present in the zip
with its mode. So **release downloads always extract with correct permissions**, while the
published zip is archived from bytes CI already built and validated. On macOS the ad-hoc
signature lives inside each Mach-O and in `Contents/_CodeSignature`, both ordinary files,
so it survives the artifact round-trip. The packages contain only what a user needs to
install — attribution/support files are release-page assets instead (see below).

## Installers (v0.9.0)

Every platform offers an **installer route** and a **manual (zip) route**; both install
into the standard **system-wide** locations. The Windows/macOS installers are built from
the *same* validated staging directory the zip was archived from, in separate, additively
gated steps (`package_windows` / `package_macos_pkg`), each with the version parsed from
`CMakeLists.txt` embedded in a deterministic file name:

- **Linux** — the installer is `packaging/linux/install.sh`, shipped **inside the zip**
  (no separate Linux package archive). It installs system-wide with root (`sudo`):
  VST3 → `/usr/lib/vst3`, Standalone → `/usr/local/bin`; `uninstall.sh` reverses it.
- **Windows** — `Anamorph-<version>-Windows-Installer.exe`: compiled by the preinstalled
  Inno Setup 6 (`ISCC.exe`) from `packaging/windows/Anamorph.iss` (stable `AppId`;
  requires elevation; real uninstall entry). Wizard: a **component page** (*Install VST3*
  / *Install Standalone*, both pre-selected, at least one required — enforced in
  `[Code]`), then **one destination page with both paths** (VST3 folder above the
  Standalone folder; defaults `{commoncf64}\VST3` and Program Files + Start-menu). The
  chosen Standalone folder is written back to `{app}`, so the uninstaller and Start-menu
  icon follow it. No post-install "launch" checkbox. Not yet Authenticode-signed —
  RH-PR-5 signs this same exe.
- **macOS** — `Anamorph-<version>-macOS.pkg`: `packaging/macos/build-pkg.sh` builds three
  component packages (VST3 → `/Library/Audio/Plug-Ins/VST3`, AU → `.../Components`, app →
  `/Applications`) and combines them with `productbuild` over a hand-written distribution
  (**`customize="allow"`, all choices pre-selected** — the default is a full install and
  Installer.app's *Customize* button exposes per-component checkboxes; `<domains
  enable_localSystem>` pins the system-wide destinations). A self-check expands the
  result and asserts all three component identifiers plus the customize/pre-selected
  attributes. Payloads installed by Installer.app carry no quarantine attribute (unlike
  zip-extracted bundles). Not yet signed/notarized — RH-PR-3 signs + notarizes this same
  package.

`release.yml` downloads the two installer artifacts alongside the `Anamorph-<OS>` staging
trees, **fail-closes on a missing or version-skewed file name**, moves them into the
draft release unmodified, and covers them in `SHA256SUMS.txt`. The user manual
(`docs/user/USER_MANUAL.md`) is attached as `Anamorph-<version>-UserManual.md`, and
`SUPPORT.md` as `Anamorph-<version>-SUPPORT.md`.

## Third-party attribution & support files (release-page assets)

Several licences JUCE vendors require their notice to **accompany a binary distribution** —
libjpeg/IJG, FLAC and Ogg Vorbis mandate it outright; HarfBuzz and SheenBidi carry
copyright/attribution terms. The packages themselves are deliberately **lean** (payload +
`INSTALL.txt` only — no attribution or support files inside; owner decision for the
closed-source commercial product, 2026-07-26). The obligations are discharged like this:

| Where | What |
|---|---|
| Release page (every download route) | `Anamorph-<version>-NOTICE.txt`, `Anamorph-<version>-THIRD_PARTY_LICENSES.md` and `Anamorph-<version>-SUPPORT.md` published next to the zips/installers, version-named so `SHA256SUMS.txt` covers them |
| Inside every package | nothing — since 2026-07-26 (owner decision) `INSTALL.txt` carries installation instructions only, so the **mandatory IJG acknowledgement** rests entirely on the release-page `NOTICE` asset that accompanies every download |
| Repository | `NOTICE` and `THIRD_PARTY_LICENSES.md` at the root remain the source the release assets are copied from |

Anyone **redistributing** the binaries outside the release page (e.g. mirroring a zip alone)
must carry the `NOTICE`/`THIRD_PARTY_LICENSES.md` files along with it — the notice-with-binaries
obligations attach to the distribution, wherever it happens.

The inventory itself — what is compiled in, what is only vendored, and how that was verified —
is in [`THIRD_PARTY_LICENSES.md`](../../THIRD_PARTY_LICENSES.md). Re-verify it after any JUCE
bump: two components (FreeType and stb, both inside PlutoVG) are absent from JUCE's own
`LICENSE.md` and are only found by walking the compiled translation units.

Public binaries are **stripped** (RH-PR-2, ADR-0021); the `-debug` artifacts carry the full
symbol/debug information for crash symbolication and must never be redistributed with a release.
Evidence [Verified]: build.yml (stage/upload steps per job).

## Plugin identifiers (for host validation)

| Field | Value | Source |
|---|---|---|
| Company | RollyTech | CMakeLists.txt:151 |
| Bundle ID | `com.rollytech.anamorph` | CMakeLists.txt:152 |
| Manufacturer code | `RTec` | CMakeLists.txt:153 — vendor-wide, shared by every RollyTech plug-in; was `Anmf` before 0.9.1 (ADR-0023) |
| Plugin code | `Anmr` | CMakeLists.txt:154 |
| Product name | Anamorph | CMakeLists.txt:156 |
| VST3 categories | Fx, Spatial, Stereo | CMakeLists.txt:163 |

AU validation (macOS): `auval -v aufx Anmr RTec` (type=`aufx`, subtype=`Anmr`, manufacturer=`RTec`).
Evidence [Verified]: packaging/macos/INSTALL.txt:70-72.

## macOS signing & quarantine

CI **ad-hoc** codesigns the bundles (`codesign --force --deep --sign -`) — they are **NOT
notarized**. Order inside the packaging step (ADR-0021): `dsymutil` (capture dSYMs) → `strip -x`
→ codesign — signing is LAST because stripping afterwards would invalidate the seal. A codesign
failure now fails the job (the former `|| true` swallowing was removed). Gatekeeper quarantines
**zip-extracted** bundles after download, so that route requires removing the quarantine flag;
the `.pkg` route does not (Installer.app-written payloads are not quarantined).
Evidence [Verified]: build.yml (Package macOS plugins step); packaging/macos/INSTALL.txt:4-10.

Install (from `INSTALL.txt`):
```bash
sudo cp -R "Anamorph.vst3"      /Library/Audio/Plug-Ins/VST3/
sudo cp -R "Anamorph.component" /Library/Audio/Plug-Ins/Components/
# REQUIRED — strip the quarantine flag, or the DAW won't load it:
sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/Anamorph.vst3
sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/Components/Anamorph.component
```
Logic Pro / GarageBand load **AU only** (`.component`); VST3 hosts use the `.vst3`.
The `xattr` step is the **zip route only** — payloads written by the `.pkg` installer carry no
quarantine attribute. Evidence [Verified]: packaging/macos/INSTALL.txt:47-65 (manual route),
:27-28 (no quarantine via the installer).

## Universal binary verification (macOS)

The macOS job verifies both slices are present (strict — a missing slice fails the job):
```bash
lipo -archs Anamorph.vst3/Contents/MacOS/Anamorph        # expect: x86_64 arm64
```
Evidence [Verified]: build.yml (Package macOS plugins step).

## Standard install locations (system-wide, both routes)

| What | macOS | Windows | Linux |
|---|---|---|---|
| VST3 | `/Library/Audio/Plug-Ins/VST3/` | `%CommonProgramFiles%\VST3\` | `/usr/lib/vst3/` |
| AU | `/Library/Audio/Plug-Ins/Components/` | — | — |
| Standalone | `/Applications/` | `%ProgramFiles%\Anamorph\` | `/usr/local/bin/` |

Installer and manual routes target the **same system-wide locations** (the Windows
installer additionally lets the user change both paths on its destination page). All
asserted from repo evidence: the per-platform `packaging/<os>/INSTALL.txt` files and the
installer destinations in `packaging/windows/Anamorph.iss` / `packaging/macos/build-pkg.sh`
/ `packaging/linux/install.sh`.

## Not in scope

- **AAX** packaging — **Not Supported** (needs Avid account + PACE/iLok; see
  `docs/policies/COMPATIBILITY_POLICY.md`).
- **Code signing / notarization** of the packages — the Inno Setup exe and the `.pkg` are
  unsigned until RH-PR-3 (macOS) / RH-PR-5 (Windows); the user-facing consequences
  (SmartScreen / Gatekeeper prompts) are documented in the per-platform `INSTALL.txt`
  files and `docs/user/INSTALLATION.md`.
