# Anamorph — User Manual

*Applies to Anamorph 0.9.0. The plug-in's exact version and build number are shown on the
About screen (click the **ANAMORPH** title).*

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

## 2. Installation

See the step-by-step [Installation guide](INSTALLATION.md) for all three platforms
(installer/package **and** manual zip routes, file locations, security-warning notes,
uninstall). Short version: run the platform installer from the GitHub release, then
rescan plug-ins in your DAW.

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

- **Haas** → **Haas Delay** (1 … 35 ms) and a **FOCUS** selector (Left/Right — the side
  the image leans toward; the *other* channel gets the delay).
- **Velvet Noise** → **Velvet Density** (0 … 100 % — how many of the sparse noise taps
  are active).
- **Chorus** → **Chorus Rate** (0.05 … 5 Hz) and **Chorus Depth** (0 … 100 %).
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
| **Input Balance** | Trims the L/R (or M/S) balance into the processor; the readout shows `L −x %` / `C` / `R x %`. |
| **M/S Solo** | Off / Mid / Side — listen to just the Mid or just the Side of the input, before the widener. Great for hearing what the algorithm adds. |

### 3.5 OUTPUT panel (Advanced only)

| Control | What it does |
|---|---|
| **Mix** | Dry/wet balance. The dry path is **delay- and phase-compensated** through the same crossovers, so intermediate Mix settings don't comb-filter, and **Mix 0 % is a bit-exact null** with the input. |
| **Mono Maker** | Collapses everything **below** the set frequency (20 … 500 Hz, default 120 Hz) to mono — the classic way to keep the low end solid and vinyl/club-safe. Applied after Mix. |
| **Output Gain** | ±24 dB final trim. |
| **Output Balance** | Final L/R balance. |
| **Level Match** | Loudness-matches output to input (BS.1770) so widening doesn't fool you with a level change. The readout shows the correction being applied; **Apply Gain** writes that value permanently into Output Gain and switches Match off. |

### 3.6 Settings overlay (gear icon)

These are **per-session UI/engine settings** — they live with the session, never in
presets, and are invisible to host automation:

| Setting | Options | Notes |
|---|---|---|
| **Oversampling** | Off (1×) / 2× / 4× / 8× | For the nonlinear stages (Drive, Chorus, Dim-D). **Off adds no latency.** Higher factors reduce aliasing at higher CPU cost and add a small, host-compensated latency — but only while a nonlinear stage is actually active (§5). |
| **Window Size** | XS / S / M / L / XL | UI scale steps (75 % … 150 %); M is the original size. |
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
| macOS | `~/Library/Application Support/RollyTech/Anamorph/Presets` |
| Linux | `~/.config/RollyTech/Anamorph/Presets` |

There is no in-plugin rename/delete — manage the files in that folder (the plug-in picks
up changes, sorted alphabetically).

### 7.3 What a preset contains — and compatibility

A preset stores **sound parameters only**. Deliberately excluded: Bypass, band solo
(always off after a load), Simple/Advanced state, and everything in Settings
(oversampling, window size, etc. stay as they are in your session).

Presets are **forward-compatible**: a preset saved by an older Anamorph loads fine in a
newer one — any parameter the old file doesn't mention simply keeps its factory default.
Parameter identities are frozen and regression-tested in CI, so `.anamorph` files and DAW
sessions from 0.8.x load unchanged in 0.9.0.

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

## 9. Troubleshooting

**The plug-in doesn't appear in my DAW.**
Check the install location for your platform (see the [Installation guide](INSTALLATION.md)),
make sure you copied/installed the *whole* `Anamorph.vst3` bundle folder, then force a
plug-in rescan. On macOS, Logic/GarageBand only see the AU (`.component`); other DAWs use
the VST3. Anamorph is 64-bit only.

**macOS says the plug-in/app "cannot be opened" or it fails to load after a zip install.**
That is Gatekeeper quarantine on un-notarized software. Either use the `.pkg` installer
(no quarantine on installed files) or run the `xattr -dr com.apple.quarantine …` commands
from the zip's `INSTALL.txt`, then rescan.

**Windows SmartScreen blocks the installer/app.**
"More info → Run anyway" — expected until the binaries are code-signed.

**The DAW scan hangs or the plug-in fails a scan once.**
Rescan; if a host cached a failed scan (common after a quarantine issue on macOS), clear
that host's plug-in cache/blocklist entry and scan again.

**High CPU.**
Oversampling is the main cost — set it to Off/2× (it only affects Drive/Chorus/Dim-D
character at high drive). Closing the editor or hiding meters reduces GUI load; the
Linux build intentionally renders on the CPU.

**Clicks or level jumps when switching presets/A-B?**
A brief dip to the dry signal is by design (it masks parameter jumps). If levels differ
between A and B, engage **Level Match** while comparing.

**Preset didn't save / can't find my preset.**
Presets go to the per-user folder in §7.2 (created automatically). In REAPER, if the
Save-Preset name field loses keyboard focus and Space toggles the transport, close and
reopen the save dialog — a known host-focus quirk.

**Undo doesn't undo a typed value / a mouse-wheel band nudge.**
Known limitation: values typed into a value box and Multiband mouse-wheel nudges don't
create undo steps yet. Knob/drag gestures, resets, and preset loads all do.

**A control looks stuck "pressed" (macOS).**
If the mouse button was released outside the plug-in window, the pressed look can linger
until the cursor re-enters the window — cosmetic only; the value isn't changing.

**Something in the sound seems wrong.**
Set **Mix** to 0 % — the output is then bit-exactly the input; raise it again and A/B.
Bypass (top bar) crossfades to the untouched signal while keeping the analyzers alive.

---

*Anamorph is © 2026 RollyTech — www.rolly.tech.*
