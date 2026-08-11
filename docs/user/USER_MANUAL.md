# Anamorph — User Manual

*The plug-in's exact version and build number are shown on the About screen (click the
**ANAMORPH** title).*

**New here? Go straight to [§2 Quick start](#2-quick-start).**

### Contents

1. [Introduction](#1-introduction) — what Anamorph is and the idea behind it
2. [Quick start](#2-quick-start) — install, first launch, first sound, the standalone app
3. [The interface](#3-the-interface) — every panel and control
4. [The four algorithms](#4-the-four-algorithms)
5. [Signal flow](#5-signal-flow)
6. [Simple mode, Advanced mode, and the Multiband display](#6-simple-mode-advanced-mode-and-the-multiband-display)
7. [Presets and A/B](#7-presets-and-ab)
8. [Workflow examples](#8-workflow-examples)
9. [FAQ & troubleshooting](#9-faq--troubleshooting)

Installation is covered separately in the
**[Installation guide](INSTALLATION.md)** (online:
<https://github.com/skyRolly/Anamorph/blob/main/docs/user/INSTALLATION.md>). If something is
wrong and this manual doesn't answer it, see
the internal testing guide **[SUPPORT.md](../../SUPPORT.md)** (online:
<https://github.com/skyRolly/Anamorph/blob/main/SUPPORT.md> — also published on each release
page as `Anamorph-<version>-SUPPORT.md`) and **[KNOWN_ISSUES.md](../KNOWN_ISSUES.md)** (online:
<https://github.com/skyRolly/Anamorph/blob/main/docs/KNOWN_ISSUES.md>).

---

## 1. Introduction

### What Anamorph is

Anamorph is a **stereo-field toolkit** by RollyTech: it turns mono sources into stereo,
shapes the width of stereo material — overall or per frequency band — and helps you keep
everything **mono-compatible**, all arranged around a precision vectorscope.

It is an audio *effect* (no MIDI): VST3 on all platforms, Audio Unit on macOS, plus a
Standalone application. It accepts mono→stereo or stereo→stereo track layouts; the output
is always stereo.

### Who it is for

Mixing and mastering engineers who need controlled, phase-conscious width; producers and
sound designers who want to place mono synths, guitars or samples into a wide image
without losing the center.

### The signal concept in one paragraph

Anamorph works in **Mid/Side** terms: the *Mid* signal is what mono playback hears, the
*Side* signal is what makes the image wide. The **widening algorithms** create or enhance
Side content from the input; **Width** controls scale Side against Mid (0 % = mono,
100 % = unchanged, 200 % = double); the **Multiband** section applies width per frequency
band through phase-coherent crossovers; and the **vectorscope + correlation meter** show
you at all times how wide — and how mono-safe — the result is.

---

## 2. Quick start

Five minutes from download to first widened sound.

### 2.1 Install

Download the release for your platform from the project's **GitHub Releases** page. Each
platform offers an **installer** (the easy route) and a plain **zip** (copy the files
yourself). Both install to the standard locations your DAW already scans; on Windows and
macOS that is system-wide and needs an administrator step, while the Linux installer asks
and defaults to a per-user install that needs no root at all.

| Platform | Easy route | What it installs |
|---|---|---|
| Windows | run the `…-Windows-Installer.exe` | VST3 into `Common Files\VST3`, Standalone into Program Files |
| macOS | open the `…-macOS.pkg` | VST3, AU and the Standalone app into the standard `/Library` and `/Applications` locations |
| Linux | `./install.sh` from inside the extracted zip | asks: **current user** (default, no root) — VST3 into `~/.vst3`, Standalone into `~/.local/bin` — or system-wide, into `/usr/lib/vst3` and `/usr/local/bin` |

The installers are not code-signed or notarized yet, so Windows SmartScreen and macOS
Gatekeeper will each warn you once — that is expected, and
[the installation guide](INSTALLATION.md) (online:
<https://github.com/skyRolly/Anamorph/blob/main/docs/user/INSTALLATION.md>) shows exactly
which button to click. Full step-by-step instructions, manual-copy paths and uninstall live
there too.

**What you need.** A 64-bit machine — Windows x86-64, macOS (Apple Silicon or Intel; the
build targets macOS 10.13 and later), or x86-64 Linux — and a VST3 or (on macOS) AU host.
There is no 32-bit build. Anamorph adapts to whatever sample rate and buffer size your host
uses; the standard 44.1–192 kHz range is what the DSP suite exercises. The plug-in makes no
network connections and needs no account or activation.

### 2.2 First launch

**Rescan your plug-ins.** Most DAWs only look for new plug-ins on demand:

| DAW | Rescan |
|---|---|
| REAPER | *Options → Preferences → Plug-ins → VST → Re-scan* |
| Ableton Live | *Preferences → Plug-Ins → Rescan* |
| Cubase / Nuendo | *Studio → VST Plug-in Manager → Rescan All* |
| FL Studio | *Plugin Manager → Find installed plugins* |
| Logic Pro / GarageBand | validates AUs automatically on launch |
| Bitwig | *Settings → Locations → Plug-in Locations* |
| Ardour | *Preferences → Plugins → Scan for Plugins* |

Anamorph is **64-bit only**, and it is an audio **effect** — look under effects/audio FX,
not instruments. On macOS, Logic Pro and GarageBand load the **AU** (`.component`); every
other DAW uses the **VST3**.

### 2.3 Load it on a track

Insert Anamorph on a **mono** or **stereo** audio track (or a bus). Mono tracks get the
mono→stereo layout — that is the "turn mono into stereo" case. The output is always
stereo, so a mono→mono slot will not offer the plug-in.

You'll see the **Simple** view: the top bar, the diamond **vectorscope** in the middle,
and the **WIDEN** panel. That is the whole core of the plug-in.

### 2.4 Your first sound

1. Play the track and watch the vectorscope — a mono source draws a **vertical line**.
2. In WIDEN, pick an **Algorithm**. Start with **Velvet Noise**: it creates stereo width
   while leaving the mono sum bit-identical, which makes it the safe default.
3. Raise **Amount** to around 40–60 %. The vectorscope opens out horizontally; the
   correlation meter underneath should stay positive.
4. Press **Bypass** in the top bar to A/B against no processing at all. Bypass is always
   available and crossfades cleanly, so it is your reference point in Simple view.
5. Want a partial blend rather than in/out? **Mix** lives in the OUTPUT panel, which is
   **Advanced-only** — press **Adv** in the top bar to reveal it. At **Mix = 0 %** the
   output is bit-exactly the input.

Then check mono compatibility, which is the whole point of the vectorscope: collapse to
mono (your console's mono button, or the **Mono** toggle in the Advanced INPUT panel) and
listen for anything that disappears.

### 2.5 The Standalone application

Anamorph also installs as a **standalone app** (`Anamorph.exe` on Windows, `Anamorph.app` on
macOS, `Anamorph` on Linux) — the same plug-in with its own audio device, useful for checking
a file or a live input without opening a DAW.

The first thing to do on launch is pick your audio hardware, in the standalone's **audio
settings** dialog: the device, the sample rate, the buffer size and which input to process.
Anamorph's Windows build does **not** include ASIO (the ASIO SDK is not redistributable, so it
is not compiled in) — Windows uses the system audio backends, and on Linux it is
ALSA/JACK/PipeWire. If you need ASIO specifically, use the VST3 in a host that provides it.

Everything else — the interface, presets, A/B, undo — behaves exactly as described in this
manual, and presets are shared with the plug-in versions since they live in the same per-user
folder ([§7.2](#72-saving-and-managing)).

Two differences worth knowing: there is no host, so there is no automation and no session to
save into (settings persist per user instead), and latency compensation is your DAW's job —
irrelevant here because nothing is running alongside it.

### 2.6 Where to go next

- Want more than the basics? Press **Adv** in the top bar for the INPUT, OUTPUT and
  **MULTIBAND** sections — [§6](#6-simple-mode-advanced-mode-and-the-multiband-display).
- Not sure which algorithm to reach for? [§4](#4-the-four-algorithms) compares all four.
- Want a starting point rather than a blank slate? Ten factory presets ship built in —
  [§7](#7-presets-and-ab).
- Concrete recipes for mixing, mastering and sound design: [§8](#8-workflow-examples).
- Something not working? [§9](#9-faq--troubleshooting).

---

## 3. The interface

Anamorph has two views: **Simple** (top bar, vectorscope, WIDEN panel) and **Advanced**
(adds the INPUT, OUTPUT and MULTIBAND sections). Toggle with the **Adv** button in the
top bar. Nothing is lost when you leave Advanced — the extra modules keep their settings
and simply return to neutral processing until you re-enter (see §6).

Controls respond to some universal gestures:

- **Knobs**: drag to change; **double-click or Alt/Option-click to reset** to default
  (one undoable step, with a little sweep animation).
- **Value boxes** (the number under a knob): **drag vertically** to change, double-click
  to type. Typed entry is forgiving: `%` is optional, `2k` or `2kHz` means 2000 Hz, and
  balance fields accept `C`, `L30`, `R30` (or `M`/`S` letters in M/S mode).
- **Tooltips**: every control has one, but they are **off by default** — enable them in
  Settings if you want in-place help (600 ms hover delay).

### 3.1 Top bar

| Control | What it does |
|---|---|
| **ANAMORPH** title | Opens the About overlay (version, build number, credits, link). Click anywhere to close. |
| **‹ Preset name ›** | Steps through presets (wraps around at the ends). Clicking the name opens the preset menu (§7). An `*` after the name means you have edited the sound since loading it. |
| **A / B** | Two independent sound slots for comparing settings; a single click toggles to the other slot (§7.4). |
| **Copy** | Copies the current slot's sound into the other slot (so you can tweak against a fixed reference). |
| **↺ / ↻** | Undo / Redo — sound parameters only, up to 128 steps, kept **per A/B slot**. |
| **Meters icon** | Slides the level-meter panel in and out (§3.2). |
| **Settings (gear)** | Opens the Settings overlay (§3.6). |
| **Adv** | Switches Simple ↔ Advanced view (§6). |
| **Bypass** | True bypass with a short crossfade; the UI below the top bar dims but stays visible, and the analyzers keep running. Hosts also see this as the standard plug-in bypass parameter. |

### 3.2 Vectorscope and meters (center)

- **Vectorscope** — a goniometer rotated 45°: a **vertical** trace means mono (all Mid),
  a **horizontal** spread means Side content, a tilted line means one channel is louder.
  The phosphor-style afterglow length is set by **Persist** in Settings.
- **Correlation meter** (vertical bar, right of the scope) — ranges **+1 … −1**. Values
  near +1: mono-safe; around 0: wide/decorrelated; **negative: parts of the signal will
  cancel in mono** — back off Width or check phase.
- **Balance meter** (horizontal, under the scope) — the average left/right energy balance.
- **Level meters** (hidden until you press the meters icon) — four bars: IN and OUT × L/R,
  with RMS fill, a held peak tick and a clip latch on a dBFS ruler. **Click any readout
  number to reset the holds**; they also reset automatically when the host transport
  starts or jumps.

### 3.3 WIDEN panel (always visible)

The creative heart of the plug-in.

| Control | Range | What it does |
|---|---|---|
| **Drive** | 0 … 24 dB | Gentle saturation/density ahead of the modulated algorithms. 0 dB is bit-clean (the stage is a true identity). Peak-preserving makeup gain is built in. |
| **Algorithm** | Haas / Velvet Noise / Chorus / Dim-D | Selects the widening engine (§4). |
| **Amount** | 0 … 100 % | How much widening the algorithm applies. **0 % is fully transparent** for every algorithm. Default is 0 — Anamorph does nothing until you raise it. |
| **Width** | 0 … 200 % | Global Mid/Side width applied after the algorithm: 0 % collapses to mono, 100 % leaves the image unchanged, 200 % doubles the Side level. |

One knob slot changes with the algorithm:

- **Haas** → **Delay** (1 … 35 ms; host parameter *Haas Delay*) and a **FOCUS** selector
  (Left/Right — the side the image leans toward; the *other* channel gets the delay).
- **Velvet Noise** → **Density** (0 … 100 %; host parameter *Velvet Density*) — how many of
  the sparse noise taps are active.
- **Chorus** → **Rate** (0.05 … 5 Hz) and **Depth** (0 … 100 %); host parameters *Chorus
  Rate* / *Chorus Depth*.
- **Dim-D** → **STYLE** selector (Subtle / Classic / Wide / Lush — progressively wider,
  deeper, slower voicings).

### 3.4 INPUT panel (Advanced only)

Conditions the signal *before* any widening:

| Control | What it does |
|---|---|
| **Input Channel** | Stereo / Left Only / Right Only — audition or feed a single channel. |
| **Mono** | Sums L+R to mono (useful to test mono compatibility, or to start a widening chain from a guaranteed-mono source). |
| **M/S** | Treats the incoming stereo pair as already Mid/Side-encoded and decodes it to L/R. The labels of the other input controls switch between L/R and M/S wording to match. |
| **Swap** | Swaps Left/Right (or Mid/Side when M/S is on). |
| **ø L/M, ø R/S** | Polarity (phase) invert per channel. |
| **Balance** (host: *Input Balance*) | Trims the L/R (or M/S) balance into the processor; the readout shows `L −x %` / `C` / `R x %`. |
| **M/S Solo** | Off / Mid / Side — listen to just the Mid or just the Side of the input, before the widener. Great for hearing what the algorithm adds. |

### 3.5 OUTPUT panel (Advanced only)

| Control | What it does |
|---|---|
| **Mix** | Dry/wet balance. The dry path is **delay- and phase-compensated** through the same crossovers, so intermediate Mix settings don't comb-filter, and **Mix 0 % is a bit-exact null** with the input. |
| **Mono Maker** | Collapses everything **below** the set frequency (20 … 500 Hz, default 120 Hz) to mono — the classic way to keep the low end solid and vinyl/club-safe. Applied after Mix. |
| **Output** (host: *Output Gain*) | ±24 dB final trim. |
| **Balance** (host: *Output Balance*) | Final L/R balance. |
| **Level Match** | Loudness-matches output to input (BS.1770) so widening doesn't fool you with a level change. The readout shows the correction being applied; **Apply Gain** writes that value permanently into Output Gain and switches Match off. |

### 3.6 Settings overlay (gear icon)

These are **per-session UI/engine settings** — they live with the session, never in
presets, and are invisible to host automation:

| Setting | Options | Notes |
|---|---|---|
| **Oversampling** | Off (1×) / 2× / 4× / 8× | For the nonlinear stages (Drive, Chorus, Dim-D). **Off adds no latency.** Higher factors reduce aliasing at higher CPU cost and add a small, host-compensated latency — but only while a nonlinear stage is actually active (§5). |
| **UI Scale** | XS / S / M / L / XL | Scales the whole window (75 % … 150 %); M is the original size. (Labelled *Window Size* before 0.9.2.) |
| **Vectorscope Persist** | 0 … 100 % | Afterglow length. While you drag it, the Settings panel becomes see-through so you can watch the scope behind it. |
| **Tooltips** | on/off | Default off. |
| **UI Animations** | on/off | Default on. |

---

## 4. The four algorithms

All four are **transparent at Amount 0 %**.

1. **Haas** — *precedence-effect widening.* Delays one channel by 1–35 ms; the undelayed
   side arrives first, so the image leans toward the **Focus** side while getting wider.
   Strong and simple, but delay-based width **comb-filters when summed to mono** — always
   check the correlation meter / a mono check when using Haas on important material.
2. **Velvet Noise** — *decorrelation widening (mono-safe by construction).* Synthesises a
   decorrelated Side signal from the Mid using a sparse "velvet noise" filter; the Mid
   itself passes untouched, so **mono playback hears exactly the unprocessed center**.
   Density sets how dense the diffusion is. The default algorithm.
3. **Chorus** — *classic modulated widening.* One modulated delay tap per channel with
   opposite LFO phase, so even a mono source becomes wide, lush and slightly in motion.
   Rate and Depth are yours; audible pitch movement is part of the charm.
4. **Dim-D** — *Roland Dimension-D-style widening.* Two anti-phase-modulated taps per
   channel cancel each other's pitch wobble to first order: spaciousness and width with
   **no seasick vibrato**. Four voicings from Subtle to Lush.

---

## 5. Signal flow

```
input → input conditioning (channel/Mono/Swap/Balance/ø/M-S decode)
      → M/S Solo
      → widening engine  [Drive + Chorus/Dim-D run inside oversampling;
                          Haas/Velvet are linear and run at base rate]
      → global Width (Mid/Side)
      → MULTIBAND per-band Width (phase-coherent crossovers)
      → Mix (dry path delay- AND phase-matched; 0 % = bit-exact input)
      → Mono Maker
      → output stage (Level Match → Output Gain → Output Balance)
      → band-solo monitoring → bypass crossfade → output
```

Points worth knowing:

- **Latency**: only oversampling adds any, and only while a nonlinear stage (Drive > 0,
  or the Chorus/Dim-D algorithms) is active. The plug-in reports its latency to the host
  (full plugin-delay compensation), and engagement is latched at safe moments so the
  latency never jumps mid-note.
- **Multiband** splits into up to 4 bands with cascaded Linkwitz-Riley crossovers plus
  allpass compensation, so with all widths at 100 % the bands recombine flat. Each band's
  width is an independent Mid/Side scale; per-band processing stays mono-compatible.
- **Smooth by design**: preset loads, A/B switches and undo/redo duck the output briefly
  to the dry signal instead of clicking; crossover moves glide; enabling/disabling
  Multiband crossfades. If the engine ever produces a non-finite sample it self-heals
  silently.

---

## 6. Simple mode, Advanced mode, and the Multiband display

### Simple vs Advanced

**Simple** shows the top bar, the vectorscope block and the WIDEN panel — enough for
"make this wider". **Advanced** (the **Adv** button) extends the window downward with
INPUT, OUTPUT and the MULTIBAND editor.

While Advanced is **off**, the Advanced-only modules process at **neutral defaults**
(no input conditioning, no multiband, Mix 100 %, no mono maker, no extra gain) — but
their settings are remembered and come back when you re-enter Advanced. The Adv state
itself travels with the session and with A/B, and is never stored in presets.

### The Multiband editor

The MULTIBAND bar shows a live spectrum with up to four bands separated by up to three
draggable split handles; each band has a horizontal **width line** and a small
**headphone (solo)** glyph, plus a **number chip** showing the split frequency.

**The `On` toggle** at the right of the MULTIBAND header is what actually applies the
per-band widths to the audio. With it off you can still see the spectrum and set up your
bands, splits and widths — nothing you do there reaches the sound until you switch it on.
Turning it on or off crossfades, so it is click-free either way.

| Gesture | Result |
|---|---|
| Drag a split handle | Moves that crossover (relative to where you grabbed; neighbours are pushed and spring back). Hold shows a band-pass preview curve of what that band passes. |
| Click in an empty gap | Adds a new split there (up to 4 bands) and lets you drag it immediately. |
| Drag a split far outside the display | Marks it for removal — release deletes the split and merges the bands. |
| Press the **×** box of a band | Removes that band. |
| Drag a width line up/down | Sets that band's Width (0–200 %). A 3-pixel threshold means a bare click never changes the value. |
| Double-click | On a number chip: type the frequency (accepts `2k`). On a split handle: reset that crossover. On a width line: reset that band's width. |
| Mouse wheel | Over a handle: nudge the split. Over a band: nudge its width. *(Wheel edits don't create undo steps.)* |
| **Solo (headphone) — quick click** | Latches that band's solo on/off. Multiple bands can be soloed together (it's a mask). |
| **Solo — press and hold (>0.2 s)** | Momentary audition of just that band; releasing restores exactly what was soloed before. |
| **Solo — Alt/Option-click** | On an unsoloed band: solo it **exclusively**. On a soloed band: **clear all solos**. |
| **Hold solo + drag sideways** | Moves the whole band rigidly (both its crossovers together). |

Band solo is a **monitoring** function at the end of the chain — it never changes what
the processing itself does, and a momentary audition doesn't even touch the solo
parameter (nothing to undo, nothing recorded to automation).

---

## 7. Presets and A/B

### 7.1 Loading

Click the preset name to open the menu — **FACTORY** and **USER** sections with a
checkmark on the current preset — or step with the **‹ ›** arrows (wrap-around).
"Load Preset…" opens a file chooser for `.anamorph` files anywhere on disk. Loads are
click-free (short duck) and form **one undo step**, so you can undo a preset load.

Ten factory presets ship built in: *Default, Gentle Width, Mono To Stereo, Vocal Air,
Synth Dimension, Drum Spread, Bass Guard, Tape Chorus, Wide Master, Super Wide*.

### 7.2 Saving and managing

"Save Preset…" opens a name dialog (Return saves, Esc cancels). Saving over an existing
name overwrites it silently. User presets are plain XML files with the `.anamorph`
extension, stored per user:

| OS | Folder |
|---|---|
| Windows | `%APPDATA%\RollyTech\Anamorph\Presets` |
| macOS | `~/Library/RollyTech/Anamorph/Presets` |
| Linux | `~/.config/RollyTech/Anamorph/Presets` |

There is no in-plugin rename/delete — manage the files in that folder (the plug-in picks
up changes, sorted alphabetically).

Saving a user preset under a **factory preset's name** is allowed, and the two stay
separate: the checkmark follows whichever one you actually loaded, and ‹ › steps from it.
Both rows show the same label, so tell them apart by the FACTORY / USER section they sit
in. Reopening the project puts the checkmark back where it was, for each A/B slot
independently. If the preset it was on is no longer there — you deleted, renamed or moved
the file — the sound still loads exactly as saved and the menu simply shows no checkmark,
rather than picking a different preset that happens to share the name. (Projects saved
before 0.9.2 have only the name to go on, so a clashing one lands on the factory row.)

### 7.3 What a preset contains — and compatibility

Loading a preset changes **sound parameters only**. Deliberately left alone: Bypass, band
solo (always off after a load), Simple/Advanced state, and everything in Settings
(oversampling, UI scale, etc. stay as they are in your session).

*(The saved `.anamorph` file itself is a dump of the whole parameter tree, so it does
contain whatever Bypass / band-solo / Advanced state you had at save time — those values
are simply ignored on load. Nothing to worry about when sharing presets; it just means the
file is slightly larger than the list above implies. Settings items are genuinely absent:
they are not host parameters at all.)*

Presets are **forward-compatible**: a preset saved by an older Anamorph loads fine in a
newer one — any parameter the old file doesn't mention simply keeps its factory default.
Parameter identities are frozen and regression-tested in CI, so `.anamorph` files and DAW
sessions from older versions load unchanged in newer ones.

### 7.4 A/B compare

The **A/B** pill switches between two complete, independent sound setups; **Copy** pushes
the current one into the other slot. Each slot remembers its own preset name, edit (`*`)
state, undo history and Level-Match gain. Switching is click-free and is *not* an undo
step (undo works within a slot). Both slots and the active side are saved in your DAW
session.

---

## 8. Workflow examples

### Widening a mono source in a mix (guitar, synth, keys)

1. Insert Anamorph on the (mono→stereo) track. Start from the *Mono To Stereo* preset,
   or: Algorithm **Velvet Noise**, raise **Amount** to ~40–60 %.
2. Watch the vectorscope open up horizontally; the correlation meter should stay
   comfortably positive — Velvet keeps the mono sum identical by construction.
3. Want more character instead? Try **Dim-D / Classic** (clean spaciousness) or
   **Chorus** (audible motion). For hard left-right placement, **Haas** with 10–20 ms —
   then *check mono*: use the INPUT panel's **Mono** toggle or your console's mono
   button, and listen for comb-filtering.
4. Trim final level with **Output Gain**, or enable **Level Match** while you dial in,
   then **Apply Gain**.

### Mastering / bus width control

1. Go **Advanced**. Leave Amount at 0 if you only want width *control*, not widening.
2. In MULTIBAND (the *Wide Master* preset is a starting point): keep the lowest band at
   ~100 % width (or less), widen the presence/air bands slightly (105–120 %).
3. Enable **Mono Maker** around 100–150 Hz for a solid, club-safe low end.
4. Use band **solo** (hold for momentary audition) to verify each band, and keep an eye
   on the **correlation meter** — sustained negative readings mean mono trouble.
5. Compare candidate settings with **A/B** (+ **Copy**), with **Level Match** engaged so
   loudness doesn't bias you.

### Creative sound design

1. Start from *Super Wide* or *Tape Chorus*.
2. Push **Drive** for density (raise **Oversampling** to 4×/8× if you hear aliasing
   grit — remember it adds a little latency while engaged).
3. Automate **Width** (0 → 200 %) for collapse/expand moves; try **M/S Solo → Side** to
   hear only the spatial part while you sculpt it.
4. Extreme Multiband: solo one band exclusively (Alt-click its headphone), move it with
   hold+drag, set wildly different widths per band.

---

## 9. FAQ & troubleshooting

### Installing and loading

**The plug-in doesn't appear in my DAW.**
Work through these in order:

1. **Rescan.** Most hosts only look on demand — see the table in
   [§2.2](#22-first-launch). This alone fixes most cases.
2. **Right format?** Logic Pro and GarageBand load **AU only** (`Anamorph.component`);
   everything else uses the **VST3**. Anamorph is 64-bit only and will not appear in a
   32-bit host.
3. **Right place?** Check the install location for your platform in the
   [Installation guide](INSTALLATION.md) (online:
   <https://github.com/skyRolly/Anamorph/blob/main/docs/user/INSTALLATION.md>). If you copied
   by hand, make sure you moved the *whole* `Anamorph.vst3` **folder** — not a single file
   from inside it.
4. **Right kind of track?** It is an audio effect, not an instrument, and the output is
   always stereo — a mono→mono slot will not offer it.
5. **Blocklisted from an earlier failed scan?** Clear that host's plug-in cache or
   blocklist entry for Anamorph and scan again. This is common after a macOS quarantine
   problem: the first scan fails, the host remembers, and it never retries on its own.

**How do I rescan plug-ins?**
See the per-DAW table in [§2.2](#22-first-launch).

**Where exactly does it install on Windows?**
The VST3 goes to `C:\Program Files\Common Files\VST3\Anamorph.vst3` (the folder every
VST3 host scans by default) and the Standalone to `C:\Program Files\Anamorph\`, with a
Start-menu entry. The installer lets you change **both** paths on its destination page,
and its component page lets you install only one of the two. Uninstall from
*Settings → Apps → Installed apps → Anamorph*.

**macOS says it "cannot be opened", or the plug-in won't load after a zip install.**
That is Gatekeeper, because the binaries are not notarized yet.

- Opening the **`.pkg`**: macOS refuses the first double-click. Open
  *System Settings → Privacy & Security*, scroll down, click **Open Anyway** next to the
  blocked-package message, then confirm. (On macOS 14 and earlier you can instead
  right-click the `.pkg` → *Open* → *Open*.)
- After a **zip** install: the extracted bundles carry a quarantine flag and the DAW will
  refuse them. Either use the `.pkg` (its installed files are never quarantined) or run
  the `xattr -dr com.apple.quarantine …` commands from the zip's `INSTALL.txt`, then
  rescan.

**Windows SmartScreen blocks the installer or the app.**
*More info → Run anyway*. Expected until the binaries are code-signed.

### Sound and performance

**How much CPU should I expect, and how do I reduce it?**
**Oversampling** is by far the largest cost — it is a Settings item, and it only changes
the character of the nonlinear stages (Drive, Chorus, Dim-D) at high drive, so Off or 2×
is a reasonable working setting. The **Multiband** section costs more than global width
(more filters per band). On the GUI side, closing the editor window removes essentially
all of it; hiding the meters helps too. The Linux build renders the graphics on the CPU by
design.

**Does Anamorph add latency?**
Almost always **zero**. Reported latency is non-zero *only* when the oversampler is
actually running, and it only runs when there is nonlinear or modulation work for it to
do — that is: Oversampling set to 2×/4×/8× **and** either Drive above zero **or** the
Chorus / Dim-D algorithm selected. Choose Oversampling with Drive at 0 and Haas or Velvet
Noise, and the oversampler stays bypassed at zero latency. When it does engage, the
plug-in **reports** its filter latency to the host, so delay compensation keeps everything
in time automatically. The Haas algorithm's delay is part of the effect, not reported
latency — that is the point of it. So if your DAW shows a plug-in delay, oversampling is
engaged.

**Clicks or level jumps when switching presets or A/B?**
A brief dip to the dry signal is deliberate — it masks the parameter jump. If A and B sit
at noticeably different levels, engage **Level Match** while comparing.

**Something in the sound seems wrong.**
Press **Bypass** in the top bar — it crossfades to the untouched signal while keeping the
analyzers running, so you can watch the meters both ways, and it is available in Simple
view. For a bit-exact reference instead of a crossfade, switch to **Advanced** (the **Adv**
button) and set the OUTPUT panel's **Mix** to 0 %: the output is then bit-exactly the
input. Raise it again and A/B.

### Automation and sessions

**Can I automate the controls?**
Yes — every sound parameter is exposed to the host and can be automated or MIDI-mapped
from your DAW as usual, including the crossover frequencies, the per-band widths, the band
count and the band-solo mask. The Settings overlay items (oversampling, UI scale, scope
persistence, tooltips, animations) and the top bar's meters toggle are deliberately **not**
host parameters at all, so they never clutter your automation list — they are session state
and are saved with your project. Bypass is a host parameter, but it is excluded from A/B, undo
and presets.

**Will my old sessions still work after I update?**
Yes. Parameter identities are frozen and regression-tested in CI, so DAW sessions and
`.anamorph` presets saved by an older Anamorph load unchanged in a newer one — any
parameter an older file doesn't mention simply keeps its default. See
[§7.3](#73-what-a-preset-contains--and-compatibility).

### Presets

**Where are my presets stored?**
User presets are plain XML files with the `.anamorph` extension, in a per-user folder —
the exact path per platform is in [§7.2](#72-saving-and-managing). Factory presets are
built into the plug-in and are not files.

**How do I share a preset, or move presets to another machine?**
Copy the `.anamorph` files out of that folder. They are portable across platforms. Anyone
can load one with *Load Preset…* from anywhere on disk, or drop it into their own preset
folder to have it appear in the menu.

**Preset didn't save / I can't find it.**
The folder in §7.2 is created on demand. In REAPER, if the Save-Preset name field loses
keyboard focus and Space starts the transport instead of typing a space, close and reopen
the save dialog — a known host-focus quirk.

**Can I rename or delete presets from inside the plug-in?**
No — manage the files in the preset folder directly. The plug-in picks up changes and
sorts alphabetically.

### Known quirks

**Undo doesn't undo a typed value or a mouse-wheel band nudge.**
Known limitation: values typed into a value box and Multiband mouse-wheel nudges don't
create undo steps yet. Knob drags, resets and preset loads all do.

**A control looks stuck "pressed" (macOS).**
If the mouse button was released outside the plug-in window, the pressed look can linger
until the cursor re-enters the window. Cosmetic only — the value is not changing.

**Anything else?**
[`docs/KNOWN_ISSUES.md`](../KNOWN_ISSUES.md) (online:
<https://github.com/skyRolly/Anamorph/blob/main/docs/KNOWN_ISSUES.md>) lists every confirmed
limitation with its current status. If your problem isn't there,
the internal testing guide [`SUPPORT.md`](../../SUPPORT.md) (online:
<https://github.com/skyRolly/Anamorph/blob/main/SUPPORT.md>) explains what a test report must
contain.

---

*Anamorph is © 2026 RollyTech — www.rolly.tech. All rights reserved.*
