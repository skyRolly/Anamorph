# Anamorph — Privacy statement

**Anamorph** · © 2026 RollyTech · Closed-source commercial audio software.

**Anamorph collects nothing, sends nothing and contacts no server.** It has no telemetry, no
analytics, no usage reporting, no crash reporting, no licence check and no update check. It works
fully offline and behaves identically with the network disconnected.

Everything below is a statement of fact about the build in this repository, verified against the
source, not a policy commitment about future versions. Each claim cites where it can be checked.
This document is not legal advice.

---

## 1. What leaves your machine

**Nothing.** The plug-in and the Standalone application open no network connection of their own.

| Fact | Evidence |
|---|---|
| The embedded web browser is disabled and libcurl is not linked, for every target — the plug-in, both test binaries, and the three off-by-default developer executables (benchmark, DSP dump, fuzz harness) | `CMakeLists.txt:335-336` (`JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`), `:366-367`, `:404-405`, `:443-444`, `:483-484`, `:523-524` |
| Nothing under `src/` opens a network connection, so JUCE's networking code is never referenced and the linker drops it | `CMakeLists.txt:166` (`-Wl,--gc-sections`), `:162` (`-Wl,-dead_strip`, Apple), `:108` (`/OPT:REF`, MSVC); the shipped binary contains **no** `WebInputStream` symbol |
| JUCE's own usage reporting and splash screen are disabled | `CMakeLists.txt:338-339` (`JUCE_DISPLAY_SPLASH_SCREEN=0`, `JUCE_REPORT_APP_USAGE=0`) |
| No analytics, telemetry, crash-reporting or update-check code exists in `src/` | no such symbol appears anywhere under `src/` |

### The one link in the interface

The About screen shows a clickable link to `https://www.rolly.tech`
(`src/PluginEditor.h:382`). It is an ordinary hyperlink: **Anamorph never opens it by itself.** If
you click it, your operating system opens the address in your default web browser, and from that
point the ordinary privacy behaviour of your browser and that website applies — outside Anamorph's
control and outside this document's scope.

## 2. What is written to your disk

Anamorph writes three kinds of file, all local, all containing only plug-in settings:

| What | Where | When |
|---|---|---|
| **User presets** (`.anamorph`, XML) — sound-parameter values and a preset name you choose | your user application-data directory, under `RollyTech/Anamorph/Presets/` (Linux `~/.config/…`, macOS `~/Library/…` — JUCE's `userApplicationDataDirectory` is `~/Library`, without an `Application Support` segment — Windows `%APPDATA%\…`) — `src/PresetManager.cpp:54-55`, written at `:216-220` | the preset *file* only when you save a preset; the containing directory is also created the first time you open the **Load Preset** dialog (`src/PluginEditor.cpp:2016`, `dir.createDirectory()` in `showLoadPreset`) |
| **Session state** — the full parameter set, A/B slots and view settings, serialised as XML, plus the name of the preset you had selected and — since 0.9.2 — a reference to *which* preset that was, so reopening the project restores the checkmark (`src/PluginProcessor.cpp`, `getStateInformation`; encoded by `PresetManager::encodeSelection`) | inside **your host's** project/session file; Anamorph hands the data to the host, which decides where to store it | whenever the host saves its session |
| **Standalone application settings** — audio-device selection, input-mute flag, last plug-in state, the window position, and the full path of the last state file you opened or saved from the Standalone's own Save/Load dialog | `Anamorph.settings`, written by JUCE's standard Standalone wrapper (Linux `~/.config/Anamorph.settings`, macOS `~/Library/Application Support/Anamorph.settings`, Windows in the user application-data folder) — `juce_audio_plugin_client_Standalone.cpp:71-82` in the pinned JUCE tree; the path entry is written at `juce_StandaloneFilterWindow.h:198` and read at `:187`, the window coordinates at `:752-753` | Standalone only; never when running as a plug-in |

Two entries are filesystem paths rather than settings, and both are written only as a direct result
of something you did:

- the Standalone's `lastStateFile`, written only if you use the Standalone's own Save/Load-state
  dialog;
- the session's preset references — **three** of them: one for the preset you have selected and one
  for **each A/B slot**. The rule is one condition with two halves
  (`PresetManager::encodeSelection`, `src/PresetManager.cpp:340-360`): a preset is recorded by its
  **file name** alone when it sits **directly in** your preset folder **and** its name cannot itself
  be mistaken for a path; **otherwise the full filesystem path is stored**. The file-name form is
  deliberate — it keeps the folder's location (and with it your account name) out of the saved
  project, and lets a project opened on another machine still find the preset. Three situations
  produce the path form:
  - a preset opened with **Load Preset…** from **outside** your preset folder;
  - one in a **sub-folder** of your preset folder, which is also only reachable through
    **Load Preset…**;
  - one sitting directly in the folder whose **file name itself looks like a path** — on macOS and
    Linux, a name beginning with `~`. This is the only one of the three that does not involve
    **Load Preset…**: such a preset is an ordinary drop-down entry, and it is the *name* alone that
    forces the path form, because a bare `~name` would not point back at the file on reload.

  An A/B slot keeps its reference until you switch into that slot again or overwrite it, so a path
  can persist in the project after the preset stops being the one you are using.

A filesystem path contains your account name on most systems. Apart from those two entries, none of
these files contains personal data beyond what you type into them (a preset name), and none is
transmitted anywhere.

### There is no log file

Anamorph writes no log and produces no diagnostic output. This is why bug reports depend on
reproduction steps and the version/build number rather than on attachments — see
[`SUPPORT.md`](SUPPORT.md).

## 3. Audio

Audio is processed **in memory, in real time, on your machine**. Anamorph does not record, buffer
to disk, upload or retain your audio. The only audio-derived data that persists anywhere is the
metering and vectorscope display, which lives in memory for as long as the editor is open.

## 4. What you send us, by choosing to

The only information that reaches the project is what **you** put in a test report or bug report —
version and build number, operating system, host and format, reproduction steps, and any
screenshots or recordings you attach. What is asked for and why is set out in
[`SUPPORT.md`](SUPPORT.md) and in the report form
(`.github/ISSUE_TEMPLATE/bug_report.yml`).

Reports filed on the GitHub issue tracker are **public**, and are stored and processed by GitHub
under GitHub's own terms and privacy policy — not under this document. Attach nothing you would
not publish. For a **security** problem, follow [`SUPPORT.md`](SUPPORT.md) §6 — GitHub's private
vulnerability reporting if the repository's *Security* tab offers it, otherwise ask for a private
contact without posting details. For anything else you should not post publicly, use the private
channel **if one was given to you** for this testing round ([`SUPPORT.md`](SUPPORT.md) §3); no
private address is recorded in this repository.

*`[OWNER DECISION]` — how test reports and tester contact details are retained, for how long, and
under which controller, is not settled. No retention or processing commitment is made here.*

## 5. Third-party components

Anamorph statically compiles third-party libraries (JUCE and the components it vendors). They are
inventoried in [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) and attributed in
[`NOTICE`](NOTICE). **None of them is a network, analytics or telemetry component** — the set is
GUI rendering, text shaping, image/audio codecs, the plug-in format SDKs and OpenGL declarations.
No networking component appears in the compiled-in inventory (`THIRD_PARTY_LICENSES.md` §2), and
the `ldd`-verified list of dynamically linked system libraries (§5, lines 174-176) contains no
`libcurl`. `THIRD_PARTY_LICENSES.md` §4 records the components present in the JUCE tree but not
built into Anamorph.

## 6. Children / special categories

Anamorph has no account or sign-in, transmits nothing and contacts no server, so no data of any
kind — including data relating to children — reaches the project through the software. What it
writes locally is listed in §2. No determination about the application of any data-protection
regime is made here; see the `[OWNER/LEGAL DECISION]` note in §7.

## 7. Scope and changes

This statement describes Anamorph **0.9.0** as built from this repository. If a future version ever
adds a network feature — licence activation, update checking or anything else — this document must
be revised in the same change, and the revision is a
[`DOCUMENTATION_LIFECYCLE_POLICY`](docs/policies/DOCUMENTATION_LIFECYCLE_POLICY.md) obligation, not
an optional one.

*`[OWNER/LEGAL DECISION]` — a controller identity, a contact address for data questions, and any
statutory disclosure required in a market of sale (e.g. GDPR Article 13/14 information) are not
recorded in this repository. They must be settled before commercial distribution — tracked with
the other licensing/legal items as `RH-F1` in
[`docs/architecture/RELEASE_HARDENING_PLAN.md`](docs/architecture/RELEASE_HARDENING_PLAN.md).*

---

**Verify it yourself.** Every claim in §1 is checkable without trusting this document: the
compile-time switches are in `CMakeLists.txt`, the shipped binary carries no `WebInputStream`
symbol (`nm -C` on the installed VST3), and a network monitor pointed at the Standalone or at
your host will show Anamorph opening no connection.
