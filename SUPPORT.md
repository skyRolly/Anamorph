# Getting help with Anamorph

Start here — most questions are already answered in the documentation, and a bug report that
arrives with the right details gets fixed far sooner than one that needs three rounds of
questions.

## 1. Check the documentation first

| Your question | Where to look |
|---|---|
| How do I install it? Where do the files go? | [`docs/user/INSTALLATION.md`](docs/user/INSTALLATION.md) — installer and manual routes for Linux, Windows and macOS |
| How does a control work? What does this panel do? | [`docs/user/USER_MANUAL.md`](docs/user/USER_MANUAL.md) |
| It doesn't show up in my DAW / it won't load / CPU is high | [`docs/user/USER_MANUAL.md`](docs/user/USER_MANUAL.md) §"FAQ & troubleshooting" |
| Is this a known problem? | [`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md) — confirmed limitations, with workarounds where they exist |
| What changed between releases? | [`CHANGELOG.md`](CHANGELOG.md) |
| I want to build from source | [`docs/procedures/BUILD.md`](docs/procedures/BUILD.md); build problems: [`docs/procedures/TROUBLESHOOTING.md`](docs/procedures/TROUBLESHOOTING.md) (contributor-facing) |

## 2. Open an issue

If the docs don't cover it, open a **GitHub issue**:
<https://github.com/skyRolly/Anamorph/issues>

Use the **Bug report** form — it asks for exactly the information needed to reproduce a problem.
Please don't skip fields; every one of them exists because it has been needed before.

### What a good report contains

- **Anamorph version and build number.** Click the **ANAMORPH** title in the plug-in to open the
  About screen — it shows `Version x.y.z   build N`. Both numbers matter: the build number
  identifies the exact CI run the binary came from.
- **Operating system and version** (e.g. Windows 11 24H2, macOS 15.3, Ubuntu 24.04), and on macOS
  whether the machine is Apple Silicon or Intel.
- **DAW and version** (e.g. REAPER 7.22, Ableton Live 12.1, Logic Pro 11.1). Several known issues
  are host-specific.
- **Plug-in format**: VST3, AU (macOS only) or the Standalone application.
- **How you installed it**: the installer (`.exe` / `.pkg` / `install.sh`) or the manual zip route.
- **Exact reproduction steps**, starting from a fresh instance — the shortest sequence that shows
  the problem, plus what you expected instead.
- **Sample rate and buffer size** if the problem is audio-related; **screenshots or a short screen
  recording** if it is visual.

### There is no log file

Anamorph writes no log file and produces no diagnostic output — there is nothing to attach, so
please don't go looking for one. Reproduction steps and the version/build number are what make a
report actionable.

### If it is a crash

Say what you were doing when it happened, and whether it is reproducible. Host crash reports
(macOS Console, Windows Event Viewer) help if you have them. Public builds are **stripped**, so a
stack trace from your machine will not be symbolicated. Debug symbols *are* produced per CI build
as separate artifacts, so symbolication is often possible on our side — with two caveats: CI
artifacts expire, and macOS dSYM capture is best-effort under the release build configuration, so
it can be unavailable for a given build. Reproduction steps remain more valuable than a trace.

## 3. Before reporting: two quick checks

1. **Rescan your plug-ins.** A stale host cache or a blocklist entry from a previous failed scan
   explains a large share of "it doesn't appear" reports. `USER_MANUAL.md` §FAQ has the per-DAW
   steps.
2. **Compare against unprocessed audio.** Press **Bypass** in the top bar (always visible), or
   switch to **Advanced** and set the OUTPUT panel's **Mix** to 0 %, at which point the output is
   bit-exactly the input. If the problem persists there, it is not coming from Anamorph's
   processing.

## Security

Anamorph makes no network connections (`JUCE_USE_CURL=0`, `JUCE_WEB_BROWSER=0`, no telemetry). It
does parse untrusted input in one place: `.anamorph` preset files and host session state are XML,
so a malformed or hostile file is the realistic attack surface.

If you believe you have found a security problem, please **do not open a public issue**. Use
GitHub's private vulnerability reporting on this repository if the *Security* tab offers it; if it
does not, open an issue saying only that you have a security report and asking for a private
contact — no details in the public thread.

## Scope

This repository is the plug-in itself. It cannot help with third-party DAW bugs, operating-system
problems, or audio-interface driver issues — though pointing them out is still useful, since a
workaround may belong in `docs/KNOWN_ISSUES.md`.
