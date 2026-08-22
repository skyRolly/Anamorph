# Installing Anamorph

This guide is for **users and testers** — how to install a build you were given.
Maintainers building the project: [`docs/procedures/BUILD.md`](../procedures/BUILD.md).

Anamorph is distributed from the project's **GitHub Releases** page. Every release offers,
per platform, an **installer** (the easy way) and a **plain ZIP archive** (the manual
way), plus `SHA256SUMS.txt` if you want to verify your download. Extracting a ZIP shows
the packaged files directly — there is no extra wrapper folder inside.

| You use… | Installer | Manual archive |
|---|---|---|
| Linux | `install.sh` inside the `.zip` | `Anamorph-<version>-Linux.zip` |
| Windows | `Anamorph-<version>-Windows-Installer.exe` | `Anamorph-<version>-Windows.zip` |
| macOS | `Anamorph-<version>-macOS.pkg` | `Anamorph-<version>-macOS.zip` |

Formats per platform: Linux and Windows ship VST3 + Standalone; macOS ships
VST3 + AU + Standalone. The plug-in is 64-bit only. macOS builds are universal
(Apple Silicon + Intel).

**Processor requirement (Intel/AMD, from 0.9.5).** The **Linux** build and the **Intel half of the
macOS build** need an Intel **Haswell** (2013) or AMD **Excavator** (2015) processor or newer. On
anything older the plug-in will not load and the failure looks like a DAW crash rather than a
message. **Apple Silicon Macs and the Windows build carry no such requirement.** If you are on an
Intel Mac old enough to be affected, it will be one running macOS 10.13–10.15.

**On Windows and macOS both routes install system-wide** (for all users of the machine),
into the standard locations DAWs scan by default, so both need an administrator step.
**On Linux the installer asks you** — the default is a **per-user** install into `~/.vst3`,
which needs no root at all; a system-wide install is offered as the second choice.

> **Heads-up about security warnings.** Anamorph is not yet code-signed (Windows) or
> notarized (macOS). Your OS will warn you once at install time. The workarounds below are
> normal for unsigned software; signing/notarization is planned.

---

## Linux

### Installer installation (install.sh)

1. Download and extract `Anamorph-<version>-Linux.zip`.
2. In the extracted folder, run it **without** `sudo`:

   ```sh
   ./install.sh
   ```

   It asks where to install:

   ```
   Anamorph Linux Installer

   Choose installation location:

   1) Install for current user (recommended)
      ~/.vst3

   2) Install system-wide
      /usr/lib/vst3

   Select [1/2]:
   ```

   | Choice | VST3 plug-in | Standalone app | Needs root? |
   |---|---|---|---|
   | **1) current user** (press Enter — the default) | `~/.vst3/Anamorph.vst3` | `~/.local/bin/Anamorph` | no |
   | 2) system-wide, all users | `/usr/lib/vst3/Anamorph.vst3` | `/usr/local/bin/Anamorph` | yes (`sudo`, for the copy only) |

   **The per-user install is recommended**: it needs no root access and no password, it
   leaves system directories untouched, and `~/.vst3` is the VST3 standard's per-user
   Linux folder — REAPER, Bitwig, Ardour and other Linux DAWs scan it by default, so the
   plug-in shows up exactly as a system-wide one would. Anything typed that is not `2` is
   treated as the default. Choosing 2 prints *"System-wide installation requires
   administrator privileges"* and lets `sudo` ask for your password; only the copy runs
   elevated, never the whole script. Running `sudo ./install.sh` installs system-wide
   without asking, as before.

3. Rescan plug-ins in your DAW (REAPER: *Options → Preferences → Plug-ins → VST →
   Re-scan*; Bitwig: *Settings → Locations → Plug-in Locations*, then rescan; Ardour:
   *Preferences → Plugins → Scan for Plugins*). A DAW that is already running will not
   notice the new plug-in until you rescan or restart it.

To remove it later, run `./uninstall.sh` from the same folder — it offers the same two
choices, and removing a per-user install needs no root either (`sudo ./uninstall.sh`
removes the system-wide one).

### Manual installation (zip)

Per-user, no root — copy `Anamorph.vst3` (the whole folder) into `~/.vst3/` and the
`Anamorph` standalone executable into `~/.local/bin/`:

```sh
mkdir -p ~/.vst3 ~/.local/bin
cp -R Anamorph.vst3 ~/.vst3/
cp Anamorph ~/.local/bin/
```

System-wide instead (both need root):

```sh
sudo mkdir -p /usr/lib/vst3
sudo cp -R Anamorph.vst3 /usr/lib/vst3/
sudo cp Anamorph /usr/local/bin/
```

### Linux troubleshooting

- **"Permission denied" launching the Standalone** — some archive tools drop the
  executable bit (`install.sh` sets it itself, so this only follows a manual copy). Fix:
  `chmod +x ~/.local/bin/Anamorph` (and, if the DAW can't load the plug-in,
  `chmod +x ~/.vst3/Anamorph.vst3/Contents/x86_64-linux/Anamorph.so`) — with `sudo` and
  the `/usr/...` paths for a system-wide install.
- **`./install.sh: Permission denied`** — you downloaded the package as a per-push CI
  artifact, which drops the executable bit (release zips keep it). Run `sh ./install.sh`
  instead (or `sudo sh ./install.sh` to go straight to a system-wide install); the script
  behaves identically either way.
- **`Anamorph: command not found` after a per-user install** — `~/.local/bin` is not on
  your `PATH`; start it with the full path `~/.local/bin/Anamorph`, or add that folder to
  `PATH`. `install.sh` prints a note when it spots this.
- **DAW doesn't find the plug-in** — check `~/.vst3` (per-user) or `/usr/lib/vst3`
  (system-wide) is in the DAW's VST3 search path (both are by default in
  REAPER/Bitwig/Ardour), then rescan.
- **Anamorph appears twice, or an update doesn't seem to take** — you have both installs. If
  you previously ran `sudo ./install.sh` and now take the per-user default, the older
  system-wide copy stays where it is: both paths are scanned, so the DAW may list two, and
  which one it loads depends on its scan order. **The installer warns you when it finds one**
  and names what is still installed system-wide. Remove the one you don't want —
  `sudo ./uninstall.sh` for the system-wide copy, `./uninstall.sh` for the per-user one —
  then rescan.
- **Standalone needs audio** — a working ALSA/JACK/PipeWire setup; pick the device in the
  app's audio settings.

---

## Windows

### Installer installation

1. Download `Anamorph-<version>-Windows-Installer.exe` and run it.
2. If **SmartScreen** shows "Windows protected your PC": click **More info → Run anyway**
   (the installer is not code-signed yet).
3. Approve the administrator prompt (required to write into `Program Files`).
4. **Choose the components** — *Install VST3* and *Install Standalone* are both selected
   by default; deselect what you don't need (at least one must stay selected).
5. **Confirm the install locations** — one page sets both paths, VST3 on top:

   | What | Default location |
   |---|---|
   | VST3 plug-in | `C:\Program Files\Common Files\VST3` (the plug-in installs as `Anamorph.vst3` inside it) |
   | Standalone app | `C:\Program Files\Anamorph` (+ Start-menu entry) |

6. Rescan plug-ins in your DAW (REAPER: *Preferences → Plug-ins → VST → Re-scan*;
   Ableton: *Options → Preferences → Plug-Ins*; FL Studio: *Plugin Manager*;
   Cubase: *Studio → VST Plug-in Manager*).

**Uninstall:** *Settings → Apps → Installed apps → Anamorph → Uninstall* (or the
"Uninstall Anamorph" Start-menu entry).

### Manual installation (zip)

Extract the zip, then (administrator approval needed for both):

1. Copy the **whole `Anamorph.vst3` folder** into `C:\Program Files\Common Files\VST3\`.
2. Create `C:\Program Files\Anamorph\` and copy `Anamorph.exe` (Standalone) into it.

### Windows troubleshooting

- **Plug-in doesn't appear after a manual install** — make sure you copied the entire
  `Anamorph.vst3` *folder*, not a file from inside it, then rescan.
- **SmartScreen blocks the Standalone** — *More info → Run anyway*.
- **32-bit host** — Anamorph is 64-bit only and won't show up in 32-bit DAWs.

---

## macOS

### Installer installation (.pkg)

1. Download `Anamorph-<version>-macOS.pkg`.
2. Because the package is not notarized yet, the first double-click will be refused.
   Open ***System Settings → Privacy & Security***, scroll down and click **Open Anyway**
   next to the blocked-package message, then confirm. (On macOS 14 and earlier you can
   instead right-click / Ctrl-click the .pkg → *Open* → *Open*.)
3. Follow the installer. The default is a **full installation**; click **Customize** on
   the *Installation Type* step to choose which components to install:

   | Component | Where |
   |---|---|
   | VST3 Plug-in | `/Library/Audio/Plug-Ins/VST3/Anamorph.vst3` |
   | AU Plug-in (Audio Unit) | `/Library/Audio/Plug-Ins/Components/Anamorph.component` |
   | Standalone Application | `/Applications/Anamorph.app` |

   Files installed by the package carry **no quarantine flag**, so no Terminal steps are
   needed afterwards.
4. Rescan in your DAW. Logic Pro / GarageBand use the AU and validate it automatically on
   launch (you can check from Terminal with `auval -v aufx Anmr RTec` — "PASS" means Logic
   will see it).

**Uninstall:** delete the installed items in the table above (Finder will ask for your
password for the two `/Library/...` items).

### Manual installation (zip)

The zip route requires removing macOS's quarantine flag by hand — follow the
`INSTALL.txt` inside the zip. Short version (copy what you need, then de-quarantine it):

```sh
sudo mkdir -p /Library/Audio/Plug-Ins/VST3 /Library/Audio/Plug-Ins/Components
sudo cp -R "Anamorph.vst3"      /Library/Audio/Plug-Ins/VST3/
sudo cp -R "Anamorph.component" /Library/Audio/Plug-Ins/Components/
sudo cp -R "Anamorph.app"       /Applications/
sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/Anamorph.vst3
sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/Components/Anamorph.component
sudo xattr -dr com.apple.quarantine /Applications/Anamorph.app
```

### macOS troubleshooting

- **"Cannot be opened because it is from an unidentified developer"** — right-click →
  Open (once), or *System Settings → Privacy & Security → Open Anyway*.
- **Plug-in doesn't load from the zip install** — you skipped the `xattr` quarantine
  step above; run it and rescan.
- **Logic/GarageBand don't see it** — they only use the AU (`.component`); check
  `auval -v aufx Anmr RTec`.

---

## Verifying a download (optional, all platforms)

Each release includes `SHA256SUMS.txt`. Download it next to the file you want to check,
then run the command for your platform from that folder:

**Linux** — checks every release file present in the folder, skips the rest:

```sh
sha256sum -c --ignore-missing SHA256SUMS.txt
```

**macOS** — check exactly the file you downloaded (replace the filename with the one you
downloaded; repeat per file if you downloaded more than one). This form works on every
macOS version — older systems' `shasum` doesn't have the `--ignore-missing` option:

```sh
grep "Anamorph-<version>-macOS.pkg" SHA256SUMS.txt | shasum -a 256 -c -
```

A good file prints `Anamorph-<version>-macOS.pkg: OK`.

**Windows** — print the hash and compare it by eye to the matching line in
`SHA256SUMS.txt` (use the filename you downloaded):

```bat
CertUtil -hashfile Anamorph-<version>-Windows-Installer.exe SHA256
```

`RELEASE_MANIFEST.txt` on the release lists the exact version, git tag, commit and CI
build number the binaries were produced from — the same version and build number the
plug-in's About screen shows (click the ANAMORPH title in the plug-in).

---

## Next steps

Installed? The user manual's **[Quick start](USER_MANUAL.md#2-quick-start)** covers the first
launch, rescanning in your DAW and your first widened sound. If something isn't working, the
manual's **[FAQ & troubleshooting](USER_MANUAL.md#9-faq--troubleshooting)** is the fastest route;
after that, [`KNOWN_ISSUES.md`](../KNOWN_ISSUES.md) and the internal testing guide
[`SUPPORT.md`](../../SUPPORT.md).

---

*Anamorph is © 2026 RollyTech — www.rolly.tech. All rights reserved.*
