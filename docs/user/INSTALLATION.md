# Installing Anamorph

This guide is for **users** of the plug-in. If you want to build Anamorph from source,
see [`docs/procedures/BUILD.md`](../procedures/BUILD.md) instead.

Anamorph is distributed from the project's **GitHub Releases** page. Every release offers,
per platform, an **installer/package** (the easy way) and a **plain archive** (the manual
way), plus `SHA256SUMS.txt` if you want to verify your download.

| You use… | Download this | Formats you get |
|---|---|---|
| Linux | `Anamorph-<version>-Linux.tar.gz` (or the `.zip`) | VST3 + Standalone |
| Windows | `Anamorph-<version>-Windows-Installer.exe` (or the `.zip`) | VST3 + Standalone |
| macOS | `Anamorph-<version>-macOS.pkg` (or the `.zip`) | VST3 + AU + Standalone |

The plug-in is 64-bit only. macOS builds are universal (Apple Silicon + Intel).

> **Heads-up about security warnings.** Anamorph is not yet code-signed (Windows) or
> notarized (macOS). Your OS will warn you once at install time. The workarounds below are
> normal for unsigned software; signing/notarization is planned.

---

## Linux

### Easy install (tar.gz)

1. Download and extract `Anamorph-<version>-Linux.tar.gz`.
2. In the extracted folder, run:

   ```sh
   ./install.sh
   ```

   This installs, **for your user only** (no root needed):

   | What | Where |
   |---|---|
   | VST3 plug-in | `~/.vst3/Anamorph.vst3` |
   | Standalone app | `~/.local/bin/Anamorph` |

3. Rescan plug-ins in your DAW (REAPER: *Options → Preferences → Plug-ins → VST →
   Re-scan*; Bitwig: *Settings → Locations*; Ardour: *Preferences → Plugins*).

To remove it later, run `./uninstall.sh` from the same folder.

### Manual install (zip or tar.gz)

Copy `Anamorph.vst3` (the whole folder) into `~/.vst3/`, or system-wide into
`/usr/lib/vst3/` (needs root). Put the `Anamorph` standalone executable anywhere on your
`PATH`, e.g. `~/.local/bin/`.

### Linux troubleshooting

- **"Permission denied" launching the Standalone** — some archive tools drop the
  executable bit. Fix: `chmod +x ~/.local/bin/Anamorph` (and, if the DAW can't load the
  plug-in, `chmod +x ~/.vst3/Anamorph.vst3/Contents/x86_64-linux/Anamorph.so`).
- **DAW doesn't find the plug-in** — check `~/.vst3` is in the DAW's VST3 search path
  (it is by default in REAPER/Bitwig/Ardour), then rescan.
- **Standalone needs audio** — a working ALSA/JACK/PipeWire setup; pick the device in the
  app's audio settings.

---

## Windows

### Easy install (installer)

1. Download `Anamorph-<version>-Windows-Installer.exe` and run it.
2. If **SmartScreen** shows "Windows protected your PC": click **More info → Run anyway**
   (the installer is not code-signed yet).
3. Approve the administrator prompt (required to write into `Common Files\VST3`) and
   follow the wizard. It installs:

   | What | Where |
   |---|---|
   | VST3 plug-in | `C:\Program Files\Common Files\VST3\Anamorph.vst3` |
   | Standalone app | `C:\Program Files\Anamorph\Anamorph.exe` (+ Start-menu entry) |

4. Rescan plug-ins in your DAW (REAPER: *Preferences → Plug-ins → VST → Re-scan*;
   Ableton: *Options → Preferences → Plug-Ins*; FL Studio: *Plugin Manager*;
   Cubase: *Studio → VST Plug-in Manager*).

**Uninstall:** *Settings → Apps → Installed apps → Anamorph → Uninstall* (or the
"Uninstall Anamorph" Start-menu entry).

### Manual install (zip)

Extract the zip and copy the **whole `Anamorph.vst3` folder** into
`C:\Program Files\Common Files\VST3\`. `Anamorph.exe` (Standalone) can live anywhere.

### Windows troubleshooting

- **Plug-in doesn't appear after a manual install** — make sure you copied the entire
  `Anamorph.vst3` *folder*, not a file from inside it, then rescan.
- **SmartScreen blocks the Standalone** — *More info → Run anyway*.
- **32-bit host** — Anamorph is 64-bit only and won't show up in 32-bit DAWs.

---

## macOS

### Easy install (.pkg)

1. Download `Anamorph-<version>-macOS.pkg`.
2. Because the package is not notarized yet, the first double-click may be refused.
   **Right-click (Ctrl-click) the .pkg → Open → Open.**
3. Follow the installer. It installs everything to the standard locations:

   | What | Where |
   |---|---|
   | VST3 plug-in | `/Library/Audio/Plug-Ins/VST3/Anamorph.vst3` |
   | AU (Audio Unit) | `/Library/Audio/Plug-Ins/Components/Anamorph.component` |
   | Standalone app | `/Applications/Anamorph.app` |

   Files installed by the package carry **no quarantine flag**, so no Terminal steps are
   needed afterwards.
4. Rescan in your DAW. Logic Pro / GarageBand use the AU and validate it automatically on
   launch (you can check from Terminal with `auval -v aufx Anmr Anmf` — "PASS" means Logic
   will see it).

**Uninstall:** delete the three items in the table above (Finder will ask for your
password for the two `/Library/...` items).

### Manual install (zip)

The zip route requires removing macOS's quarantine flag by hand — follow the
`INSTALL.txt` inside the zip. Short version:

```sh
sudo cp -R "Anamorph.vst3"      /Library/Audio/Plug-Ins/VST3/
sudo cp -R "Anamorph.component" /Library/Audio/Plug-Ins/Components/
sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/Anamorph.vst3
sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/Components/Anamorph.component
```

### macOS troubleshooting

- **"Cannot be opened because it is from an unidentified developer"** — right-click →
  Open (once), or *System Settings → Privacy & Security → Open Anyway*.
- **Plug-in doesn't load from the zip install** — you skipped the `xattr` quarantine
  step above; run it and rescan.
- **Logic/GarageBand don't see it** — they only use the AU (`.component`); check
  `auval -v aufx Anmr Anmf`.

---

## Verifying a download (optional, all platforms)

Each release includes `SHA256SUMS.txt`. Download it next to the file you want to check,
then run the command for your platform from that folder:

**Linux** — checks every release file present in the folder, skips the rest:

```sh
sha256sum -c --ignore-missing SHA256SUMS.txt
```

**macOS** — check exactly the file you downloaded (replace the filename with yours;
repeat per file if you downloaded more than one). This form works on every macOS
version — older systems' `shasum` doesn't have the `--ignore-missing` option:

```sh
grep "Anamorph-0.9.0-macOS.pkg" SHA256SUMS.txt | shasum -a 256 -c -
```

A good file prints `Anamorph-0.9.0-macOS.pkg: OK`.

**Windows** — print the hash and compare it by eye to the matching line in
`SHA256SUMS.txt`:

```bat
CertUtil -hashfile Anamorph-0.9.0-Windows-Installer.exe SHA256
```

`RELEASE_MANIFEST.txt` on the release lists the exact version, git tag, commit and CI
build number the binaries were produced from — the same version and build number the
plug-in's About screen shows (click the ANAMORPH title in the plug-in).
