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

Every platform offers an **installer route** and a **manual (zip) route**, both landing in
the standard locations: **system-wide** on Windows and macOS, while the Linux installer
**asks** and defaults to a **per-user** install (since 0.9.3). The Windows/macOS installers are built from
the *same* validated staging directory the zip was archived from, in separate, additively
gated steps (`package_windows` / `package_macos_pkg`), each with the version parsed from
`CMakeLists.txt` embedded in a deterministic file name:

- **Linux** — the installer is `packaging/linux/install.sh`, shipped **inside the zip**
  (no separate Linux package archive). Since 0.9.3 it **prompts for one of two modes**,
  defaulting to the per-user one:

  | Mode | VST3 | Standalone | Elevation |
  |---|---|---|---|
  | **1) current user** (default: Enter, `1`, or any unrecognised answer) | `~/.vst3` | `~/.local/bin` | none — `sudo` is never called |
  | 2) system-wide | `/usr/lib/vst3` | `/usr/local/bin` | `sudo` per operation, never for the whole script |

  `~/.vst3` is the VST3 standard's per-user Linux folder and is a default scan path in
  REAPER/Bitwig/Ardour, so the no-root route is the recommended one. Three invocations skip
  the prompt: **as root** (`sudo ./install.sh`, the pre-0.9.3 documented form) it installs
  system-wide unchanged — root has no meaningful per-user home — with **no terminal on
  stdin** it takes the default, and **`--user` / `--system`** answer it explicitly, which is
  the only way for a caller with no terminal to select mode 2 without handing the whole script
  to root. The two flags together are an error, as is an unrecognised option, and `--user` under
  root is refused (`$HOME` under `sudo` depends on the sudoers configuration, so the destination
  would not be predictable); repeating one flag is not a conflict. `uninstall.sh` takes the same
  two, plus `--discard-parked`. Mode 2 fail-closes rather than degrading: no `sudo` on
  `PATH` prints the "use a user installation instead" error and exits 1, and a failed elevated
  operation prints the permission-denied message and exits 1.

  **Replacement transaction — what is actually guaranteed.** Each installed artifact is
  replaced atomically; the **two artifacts are not one transaction**, and the guarantee is
  stated per artifact rather than for the install as a whole:

  | Point of failure | Result |
  |---|---|
  | staging (copy fails, payload unreadable, disk full) | nothing has been displaced — the previous install is untouched, complete |
  | commit of the VST3 (rename fails, or the run is interrupted) | the parked previous bundle is restored; the previous install is intact |
  | after the VST3 commit, at the Standalone commit | **new VST3 + previous Standalone** — both valid, both loadable, but a mixed pair until the run is repeated |
  | untrappable kill (`SIGKILL`, power loss) mid-swap | the previous bundle is left parked outside the scan path; the **next run restores it** before doing anything else |

  So "the previous install is never destroyed" holds unconditionally, while "all or nothing"
  does **not** — the last row is a real, if benign, mixed state. Re-running the installer
  resolves it.

  **How.** The replacement is built in a stage directory, the old bundle is moved **aside**
  (`Anamorph.vst3.prev`) rather than deleted, and only then is the finished copy renamed into
  place — so a complete copy exists at every instant. Deleting first would open a window in
  which the destination is empty and the staged copy is the only one. A `reconcile` helper
  restores the parked bundle if the run stops mid-swap, runs again at the *start* of the next
  run (which is what recovers a `SIGKILL`), and drops the parked copy only once the destination
  is populated again. `INT`, `TERM` and `HUP` are trapped explicitly because dash — `/bin/sh` on
  Debian and Ubuntu — does not run `EXIT` traps when the script is signalled.

  **Where it stages.** `.anamorph-install-stage` **next to** the plug-in directory (`$HOME` or
  `/usr/lib`), so an incomplete bundle never appears inside the path DAWs scan. That location is
  only usable on the same filesystem as the destination — every commit must stay a rename — and
  `~/.vst3` or `/usr/lib/vst3` can be a symlink onto another mount, so the script **probes** it
  with a **hard link**, the one operation that cannot cross a filesystem (`mv` is no test: it
  falls back to copy-and-unlink). If the probe fails, staging falls back to the same directory
  name *inside* the plug-in directory, same filesystem by construction. A false negative costs
  the scan-path property, never atomicity. The probe's link target is the only thing written
  into the scan directory, and `ln` refuses an existing target — so it is **removed up front on
  every run**, before any probe. A marker left by
  a run killed mid-probe is therefore litter, never a decision; without that clear it would fail
  the probe on every later run and pin staging to the fallback permanently. The recovery scan
  probes as well: a parked bundle is adopted only from a candidate that passes the same
  filesystem test as a fresh one, rather than on the inductive ground that whatever parked it
  must already have passed. The Standalone stages beside its own destination
  (`.Anamorph.new`) because its directory may be on a different filesystem again, and a bin
  directory is not a scan path. Every commit is a same-filesystem rename, which also replaces a
  **running** Standalone that `cp` refuses with `Text file busy`.

  **Which candidates are trusted.** Both staging locations are accepted only when the directory
  is not a symlink, is owned by the identity whose writes land in the destination (root for a
  system-wide install, the invoking user otherwise) and is writable by nobody else; the mode is
  matched on its last two digits so a set-gid parent's `2700` reads like a plain `700`. One this
  run creates is `chmod 700`ed explicitly, so the next run can adopt what this one made whatever
  the umask was. If neither candidate passes, the run **stops without installing** and names both
  paths — a fresh install included, because staging is where the payload is assembled before the
  rename that publishes it. The recovery scan applies the same two tests and *says so* when a
  parked bundle sits in a directory it can no longer use, since that is the one path on which the
  interrupted-install guarantee does not hold.

  `uninstall.sh` mirrors the same two modes, so a per-user install is removed without root, and
  removes the installer's own scratch (both stage-directory locations, the probe marker and the
  staged Standalone) by exact name, so an interrupted install leaves nothing that survives a
  deliberate uninstall — **with one deliberate exception**: a `Anamorph.vst3.prev` parked by an
  interrupted install is **kept**, named on stdout together with the mode-correct `./install.sh`
  that restores it (nothing else can), and removed only under `--discard-parked`. The `.probe`
  hard link beside it is still swept, because that path is the only one leaving a stage directory
  standing and so the only way that marker can outlive an uninstall; `scripts/check-portability.py`
  holds installer and uninstaller to the same scratch-name set, `.probe` included.

  **Not chased** (the Linux counterpart of the macOS note below): a per-user install does not
  *remove* an existing system-wide one — that would need the elevation the mode exists to avoid
  — but it does **detect** one (a plain `test -e`) and warn, naming what is still installed
  system-wide and the `sudo ./uninstall.sh` that clears it. Registered as **KI-021**; also a
  troubleshooting entry in `INSTALL.txt` and `docs/user/INSTALLATION.md`.
- **Windows** — `Anamorph-<version>-Windows-Installer.exe`: compiled by the preinstalled
  Inno Setup 6 (`ISCC.exe`) from `packaging/windows/Anamorph.iss` (stable `AppId`;
  requires elevation; real uninstall entry). Wizard: a **component page** (*Install VST3*
  / *Install Standalone*, both pre-selected, at least one required — enforced in
  `[Code]`), then **one destination page with both paths** (VST3 folder above the
  Standalone folder; its two field labels read *VST3 Plug-in folder* and *Standalone
  Application folder* since 0.9.2; defaults `{commoncf64}\VST3` and Program Files +
  Start-menu). The chosen Standalone folder is written back to `{app}`, so the uninstaller
  and Start-menu icon follow it. No post-install "launch" checkbox. Not yet
  Authenticode-signed — RH-PR-5 signs this same exe.
- **macOS** — `Anamorph-<version>-macOS.pkg`: `packaging/macos/build-pkg.sh` builds three
  component packages (VST3 → `/Library/Audio/Plug-Ins/VST3`, AU → `.../Components`, app →
  `/Applications`), each **non-relocatable and non-version-checked** (see
  §"macOS reinstall behaviour" below — this is what makes a re-install idempotent), and
  combines them with `productbuild` over a hand-written distribution
  (**`customize="allow"`, all choices pre-selected** — the default is a full install and
  Installer.app's *Customize* button exposes per-component checkboxes, titled *VST3
  Plug-in* / *AU Plug-in* / *Standalone Application* since 0.9.2; `<domains
  enable_localSystem>` pins the system-wide destinations). A self-check expands the
  result and asserts all three component identifiers plus the customize/pre-selected
  attributes — it matches on `<choice id=…>`, not on the titles, so the wording is free to
  change. Payloads installed by Installer.app carry no quarantine attribute (unlike
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
| Company | RollyTech | CMakeLists.txt:271 |
| Bundle ID | `com.rollytech.anamorph` | CMakeLists.txt:272 |
| Manufacturer code | `RTec` | CMakeLists.txt:273 — vendor-wide, shared by every RollyTech plug-in; was `Anmf` before 0.9.1 (ADR-0023) |
| Plugin code | `Anmr` | CMakeLists.txt:274 |
| Product name | Anamorph | CMakeLists.txt:276 |
| VST3 categories | Fx, Spatial, Stereo | CMakeLists.txt:283 |

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

## macOS reinstall behaviour (idempotency)

**Expected destinations** — fixed, and not negotiable at install time:

| Component | Identifier | Destination |
|---|---|---|
| VST3 | `com.rollytech.anamorph.vst3` | `/Library/Audio/Plug-Ins/VST3/Anamorph.vst3` |
| AU | `com.rollytech.anamorph.au` | `/Library/Audio/Plug-Ins/Components/Anamorph.component` |
| Standalone | `com.rollytech.anamorph.app` | `/Applications/Anamorph.app` |

**Guarantee (0.9.3 onward).** Every selected component copies its payload to the
destination above on *every* run, with no reference to what a previous install left
behind. Installed, moved away, deleted — re-running the installer always ends with the
item present at its destination. **Per component, not all-or-nothing:** components install in
sequence, so if one fails its `postinstall` check the installation is reported as failed with the
components already written left in place. Each of those is a complete, valid install of that
format; there is no rollback, and re-running resolves the mix.

**How** — `build_component()` in `build-pkg.sh` patches the component plist that
`pkgbuild --analyze` generates and turns two defaults off for every bundle it lists
(the top-level entries `--analyze` reports; a bundle nested inside another appears under its
parent's `ChildBundles` and is not patched — nor does it need to be, since only top-level bundles
reach `PackageInfo`'s membership lists, and the assertions below would fail the build if one did):

| Key | Value | Why |
|---|---|---|
| `BundleIsRelocatable` | `false` | The default `true` makes Installer.app look the bundle identifier up in the receipt/Spotlight database and write the payload over **whatever copy it finds**, ignoring `--install-location`. That is the INC-012 defect: move `/Applications/Anamorph.app` elsewhere (the Trash counts — still on disk, still indexed) and the next install "succeeds" into the moved copy while `/Applications` stays empty. |
| `BundleIsVersionChecked` | `false` | The default compares the installed bundle's version and **skips** the copy when it is already at or above the package version — the same silent-success failure from the other end, and the reason a repair install of the same version could do nothing. |
| `BundleOverwriteAction` | `upgrade` | pkgbuild's default, pinned explicitly: the write replaces the bundle rather than merging into it, so no file from an older install survives inside the new one. |

**Receipts.** Receipts are still written (`pkgutil --pkgs | grep com.rollytech.anamorph`)
and remain the record of what was installed — but nothing in the install path *reads* them
to decide where or whether to copy, which is exactly the assumption being removed. A
`pkgutil --forget` is therefore never needed to make an install work; it only clears the
record. Files a previous version installed but the current one no longer ships are not
removed from the destination bundle's parent — the payload is authoritative for the
bundle, not for the folder it sits in.

**Backstop.** Each component carries a `postinstall` that checks its item exists at the
destination and exits non-zero otherwise, so an install cannot report success while the
expected item is missing. It runs only for components the user actually selected.

**Not chased.** A copy the user moved elsewhere is left where they put it — the installer
restores the standard destination rather than tracking the moved bundle. A stale copy in
`~/Library/Audio/Plug-Ins/...` from an old relocated install is likewise the user's to
delete; hosts may otherwise see two Anamorphs. Registered as **KI-022** — deliberate, and the
direct trade for INC-012.

**Build-time self-checks** (fail the macOS job, so this cannot silently regress). In `PackageInfo`
each state is a membership list — `<relocate>` for `BundleIsRelocatable`, **`<bundle-version>`** for
`BundleIsVersionChecked` — written self-closing when empty, so a listed bundle shows as the
`<element><bundle` pair. The checks, in order:

1. **The assertion patterns are proved to track their keys.** The app payload is packaged twice,
   differing *only* in the component-plist keys `build_component()` sets: once with pkgbuild's
   defaults, once with the patched plist. `<relocate>` and `<bundle-version>` must be populated in
   the first and empty in the second, and `<upgrade-bundle>` populated in both. This is a controlled
   A/B, not a name check: proving an element is *producible* would not show that its membership list
   follows `BundleIsRelocatable` / `BundleIsVersionChecked` rather than something else. A name
   pkgbuild never writes makes an assertion that always passes — the first cut of the version check
   looked for `<version-check>` and was dead — and a list that ignored its key would be just as
   silent. Either way the build stops here and prints the probe's `PackageInfo`.
2. All **three** component `PackageInfo` files must be found — counted first, because a loop over an
   empty match would pass every assertion without running one.
3. Each must list no relocatable and no version-checked bundle, must carry `<upgrade-bundle>` (the
   overwrite action, previously the one patched key with no assertion of its own), and must declare
   its `postinstall`.
4. `pkgutil --expand-full` must show each component's payload carrying the whole bundle down to
   `Contents/MacOS/Anamorph`.
Evidence [Verified]: `packaging/macos/build-pkg.sh`; INC-012 in `docs/POSTMORTEMS.md`.

## Universal binary verification (macOS)

The macOS job verifies both slices are present (strict — a missing slice fails the job):
```bash
lipo -archs Anamorph.vst3/Contents/MacOS/Anamorph        # expect: x86_64 arm64
```
Evidence [Verified]: build.yml (Package macOS plugins step).

## Standard install locations (both routes)

| What | macOS | Windows | Linux (per-user, default) | Linux (system-wide) |
|---|---|---|---|---|
| VST3 | `/Library/Audio/Plug-Ins/VST3/` | `%CommonProgramFiles%\VST3\` | `~/.vst3/` | `/usr/lib/vst3/` |
| AU | `/Library/Audio/Plug-Ins/Components/` | — | — | — |
| Standalone | `/Applications/` | `%ProgramFiles%\Anamorph\` | `~/.local/bin/` | `/usr/local/bin/` |

Installer and manual routes target the **same locations** — system-wide on macOS and
Windows (whose installer additionally lets the user change both paths on its destination
page), and on Linux whichever of the two columns the user picked. All asserted from repo
evidence: the per-platform `packaging/<os>/INSTALL.txt` files and the installer
destinations in `packaging/windows/Anamorph.iss` / `packaging/macos/build-pkg.sh` /
`packaging/linux/install.sh`.

## Not in scope

- **AAX** packaging — **Not Supported** (needs Avid account + PACE/iLok; see
  `docs/policies/COMPATIBILITY_POLICY.md`).
- **Code signing / notarization** of the packages — the Inno Setup exe and the `.pkg` are
  unsigned until RH-PR-3 (macOS) / RH-PR-5 (Windows); the user-facing consequences
  (SmartScreen / Gatekeeper prompts) are documented in the per-platform `INSTALL.txt`
  files and `docs/user/INSTALLATION.md`.
