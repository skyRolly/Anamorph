# PACKAGING.md

## macOS component identity validation

`packaging/macos/build-pkg.sh` runs the identity self-test before inspecting the real
`pkgutil --expand` output. The named `AnamorphVST3.pkg`, `AnamorphAU.pkg` and
`AnamorphApp.pkg` must own `.vst3`, `.au` and `.app` receipts respectively under
`tech.rolly.anamorph`; Distribution choices and payload filenames must agree with those
owners. Unknown/nested/extra component mappings fail closed. Six-reference input and genuine
nine-reference productbuild output remain valid. Recheck retained metadata with
`python3 scripts/check-product-identity.py --expanded-package <expanded-dir>` after its
`--self-test`. This query does not install or alter any host receipts.

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

## Installers (0.9.0)

Installer product titles use the bare **Anamorph** name: Windows sets
`AppVerName` and `UninstallDisplayName`, and the macOS Distribution sets `<title>`.
Localized Setup/Uninstall wording may surround that name. `AppVersion` still
supplies the installed Windows version; component/receipt and bundle versions,
package identities and versioned download filenames retain their real values.
Evidence: `packaging/windows/Anamorph.iss`, `packaging/macos/build-pkg.sh`.

Every platform offers an **installer route** and a **manual (zip) route**, both landing in
the standard locations: **system-wide** on Windows and macOS, while the Linux installer
**asks** and defaults to a **per-user** install (since 0.9.3). The Windows/macOS installers are built from
the *same* validated staging directory the zip was archived from, in separate, additively
gated steps (`package_windows` / `package_macos_pkg`), each with the version parsed from
`CMakeLists.txt` embedded in a deterministic file name:

- **Linux** — `install.sh`/`uninstall.sh` inside the ZIP install only VST3.
  The corrected 0.10.0 candidate keeps the existing prompt/default and explicit
  `--user` / `--system` flags:

  | Mode | Installed VST3 | Elevation |
  |---|---|---|
  | Current user (default) | `~/.vst3/Anamorph.vst3` | none; no sudo writes to HOME |
  | System-wide | `/usr/lib/vst3/Anamorph.vst3` | sudo per operation, or an already-root entry |

  Root `--user` remains refused. Both scripts accept repeated matching flags and
  refuse conflicting flags. Standalone remains in the ZIP for portable execution;
  it is not copied to `~/.local/bin` or `/usr/local/bin`.

  Replacement retains the previous workflow's same-filesystem staging and parked
  `Anamorph.vst3.prev`. Copy/chmod failures leave the previous bundle intact;
  failure during publication restores it. SIGKILL can leave the old copy parked;
  the next install reconciles it before retry. Installed module chmod is mandatory,
  before publication. Stage directories are private to the writing identity.
  Uninstall preserves parked copies unless `--discard-parked` is explicit, and
  preserves unknown scratch entries. Linked/foreign plugin destinations are
  rejected before writes/removals. Serialize same-mode operations; this is
  process-interruption recovery, not power-loss durability.

  User/system plugin copies remain independent. The scripts report a second
  scanned copy without removing it. Legacy desktop cleanup is described below;
  unrelated icon-directory modes no longer block plugin installation/removal.

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
| Company | RollyTech | CMakeLists.txt:422 |
| Bundle ID | `tech.rolly.anamorph` | CMakeLists.txt:423 |
| Manufacturer code | `RTec` | CMakeLists.txt:424 — vendor-wide, shared by every RollyTech plug-in; was `Anmf` before 0.9.1 (ADR-0023) |
| Plugin code | `Anmr` | CMakeLists.txt:425 |
| Product name | Anamorph | CMakeLists.txt:427 |
| VST3 categories | Fx, Spatial, Stereo | CMakeLists.txt:436 |

AU validation (macOS): `auval -v aufx Anmr RTec` (type=`aufx`, subtype=`Anmr`, manufacturer=`RTec`).
Evidence [Verified]: packaging/macos/INSTALL.txt:70-72.

## macOS signing & quarantine

The macOS icon fallback (ADR-0062, 2026-10-29 amendment) compiles the complete approved
iconset in the normal CMake resource graph before bundle copying/signing. No signed bundle
is patched to repair its icon. `check-icons.py --platform macos` checks generated/built
resource equality and native application rendering; repeat with `--staged-dir` after the
existing strip/sign/stage sequence. Render evidence belongs outside the customer payload.

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
| VST3 | `tech.rolly.anamorph.vst3` | `/Library/Audio/Plug-Ins/VST3/Anamorph.vst3` |
| AU | `tech.rolly.anamorph.au` | `/Library/Audio/Plug-Ins/Components/Anamorph.component` |
| Standalone | `tech.rolly.anamorph.app` | `/Applications/Anamorph.app` |

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

**Receipts.** Receipts are still written (`pkgutil --pkgs | grep tech.rolly.anamorph`)
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
| Standalone | `/Applications/` | `%ProgramFiles%\Anamorph\` | portable ZIP only | portable ZIP only |

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

## Windows installer icon ownership (ADR-0064)

SetupIconFile uses the vendored approved ICO through the script's SourcePath, independent of
the build working directory. The installer always owns `{app}\resources\Anamorph.ico`;
UninstallDisplayIcon references it for VST3-only, Standalone-only and combined installs. No
optional EXE is required. Reinstall overwrites this icon; Inno removes only owned files and
empty directories on uninstall. AppId/destinations remain unchanged. The VST3 component's
bundle directory receives System attribution for its existing Plugin.ico/desktop.ini.
No wildcard deletion, extra wizard graphics, signing or release change is introduced.

## Linux plugin-only package and legacy cleanup

The corrected 0.10.0 tree contains only `Anamorph.vst3/`, portable `Anamorph`,
`install.sh`, `uninstall.sh` and `INSTALL.txt`. It ships no desktop helper, desktop
entries, hicolor assets or AppStream metadata. Shared PNGs and Windows/macOS assets
remain unchanged. `scripts/check-linux-package.py` checks exact top-level members,
source/staged script equality, ELF/module modes and rejection of desktop/AppStream
payloads; its self-test precedes use.

Both entry scripts embed the same migration-only cleanup. User cleanup uses absolute
XDG_DATA_HOME or HOME/.local/share; system cleanup uses fixed /usr/local/share,
independent of root HOME/XDG. It interprets no old helper. A real owned 0700 state
directory and exact v1 ten-path receipt authorize removing only matching regular,
single-link, owner/hash-verified leaves. User cleanup does not chmod/chown shared
icon parents, including mode 0775/0777. Privileged cleanup keeps strict ancestor
ownership/write checks. Shared directories, caches, unrelated files and presets stay.

Missing owned leaves allow retry. Modified/linked/unowned/malformed resources and
unknown/interrupted journals are preserved with explicit warnings; they do not block
VST3 operations. A receipt is retired only after its resources are gone and the state
contains just the verified empty format marker. Previously installed/staged Standalone
is removed only if it exactly matches the earlier verified 0.10.0 release executable
digest recorded in ADR-0069. Other binaries at that name remain untouched.

`tests/linux_install_tests.py` exercises actual scripts in disposable namespaces.
`tests/linux_install_native_tests.py` requires real UID/root/sudo execution in
disposable hosted-VM mount/PID/network roots, with no host security-setting changes.
Its full successful result is required for the Linux customer artifact. Recorder/plan
tests cannot replace that native result. See proposed ADR-0069 and TESTING for limits.

## Native installed-receipt smoke test

`scripts/test-macos-receipts.py --self-test` exercises the installed-metadata parser,
payload comparison, receipt ownership, separation and environment guards. The macOS
packaging job runs it before `--package <actual.pkg> --staged-dir <validated tree>`.
Only this repository's disposable GitHub-hosted macOS VM may install. Existing product
receipts or system bundles cause refusal; no receipt is forgotten and no plug-in folder
is deleted. The real installer targets `/`; VM disposal is cleanup.

The gate reads real `pkgutil --pkgs`, `--pkg-info`, `--pkg-info-plist` and `--files`, checks
the three canonical `.vst3`/`.au`/`.app` IDs, versions, volume, installation locations,
component-owned file paths, installed bundle IDs/AU codes, and exact installed versus
staged payload bytes. Pre-existing receipts and any sibling receipts/payloads must be
unchanged. An empty sibling snapshot does not prove both products were co-installed.
The installed-receipts artifact retains JSON, installer log and raw receipt metadata.
PKG upload requires this gate. The smoke test covers default full installation; choice
combinations, broader upgrades, Finder/Dock and real DAW recall remain separate tests.
Linux self-tests are parser evidence, never a native installation pass.


## Customer instruction wording (2026-10-26)

Linux INSTALL.txt describes application icons without internal artwork/approval
codes and names the support files by their purpose. The macOS introduction
states its universal architecture without build chronology. Existing languages,
signing/quarantine instructions, recovery, ownership/refusal and distribution
authenticity cautions remain. Windows wording and both entry scripts are already
concise enough and are unchanged. The helper's necessary security/recovery
comments are retained: its bytes and all four active pin/observer references
remain SHA-256 3ea641f74a7a04abd2cffeac4298e54022c078655593ad45d226d622ece3c45e.
No approved icon manifest or artwork hash is regenerated. Newly staged archive
bytes still require the real native-to-uploaded hash/mode gate.
