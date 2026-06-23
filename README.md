# Anamorph — Stereo Tools Audio Plugin

**Anamorph** (by **RollyTech**) is a stereo-field toolkit: it turns mono into
stereo, controls stereo width (globally and per band), and provides the full set
of stereo tools (MS, mono-maker, channel utilities, monitoring) around a
high-end diamond vectorscope. Built with **CMake + JUCE** only — it configures
and builds entirely from the command line on a headless Linux machine, no IDE.

### What's new in 0.8.1
- **One transition layer, no per-control click patches.** The plugin already smooths
  every continuous level control (`juce::SmoothedValue`) and ducks every discrete switch
  with a raised-cosine fade; this round folds the two outliers into that layer instead of
  patching each knob:
  - **Band Solo is now click-free and ghost-free.** The post-everything monitor keeps its
    band-pass filters *warm* (always running) and crossfades passthrough ↔ soloed bands
    with smoothed per-band gains, so engaging / clearing / changing the solo set is a short
    morph, never a hard switch. It no longer needs an output duck, so toggling Solo while
    the DAW is **stopped / paused / fed a zero buffer** can't emit a transient (the old
    "ghost signal"). Nothing soloed still yields the bit-exact true output.
  - **Drive / Mix gain jumps no longer pop via Level Match** (see below).
- **Level Match reworked into Measure + absolute Predict.**
  - **Measure** is the BS.1770 ground truth; on silence it *waits* (holds the last trusted
    value) instead of drifting.
  - **Predict** is an *absolute* feed-forward estimate of the wet's loudness boost, a pure
    function of **Drive and Mix** — not an accumulator. Cranking Drive up/down/up while
    paused can no longer ratchet the gain toward −24 dB, and raising **Mix** with Drive
    cranked now pre-ducks too (the "Mix=100% slam" is gone). It reads **~0 at unity** (no
    bias), and a **silence→audio edge snap** makes the first audible block compensated even
    if the host never ran the plugin while paused, so Transport-Stop→Play doesn't slam.
- **Band-pass preview is press-and-hold only.** The blue/green band-pass curve under a
  crossover line now lights only when the handle is *held* or *dragged* — never on a click,
  double-click (reset), or repeated clicks, and never on a programmatic / preset / A-B
  change.
- **Mono Maker tooltip** no longer says "(before widening)" — it runs post-Mix.
- New regression tests cover the full matrix: Drive 0→24→0→24, Mix 0→100→0→100, Solo
  on/off, A-B-style bulk swap, Parameter Reset, Transport Stop→Play and Silence→Audio→
  Silence — asserting no clicks, no ghost, no slam, no Level-Match drift/ratchet.

### What's new in 0.8.0
- **Signal flow rebuilt as a clean serial chain.** Previously Mono Maker sat *before*
  the widener and Band Solo was woven into the Multiband DSP, which made the two fight
  with the Dry/Wet Mix (the 0.7.3–0.7.5 round of solo/low-cut bugs). The chain is now
  strictly: **Processing → Dry/Wet Mix → Mono Maker → Output (Balance/Gain/Level Match)
  → Band Solo monitor**.
  - **Mono Maker is now post-Mix**: it collapses the lows of the *mixed* signal, so it
    always acts on whatever the Mix produced, at any Mix amount. Level Match measures
    the post-Mono-Maker output.
  - **Band Solo is now POST-EVERYTHING and monitoring-only**: it band-passes the final
    output to audition the soloed band(s) and never changes any DSP stage. No
    solo-dependent routing, no special low-frequency handling — soloing a high band
    simply doesn't pass the lows, and the Mix is "baked into" the signal before the
    solo filter, so there's no "Solo leaks lows" or "Mix=0 breaks Solo" failure mode.
  - The **Mix dry/wet phase fix (0.7.2)** is preserved and verified across Mix = 25/50/75%.
- New self-tests cover the matrix (Mix × Mono Maker × Solo): mono-sum phase integrity,
  post-Mix Mono Maker, solo band-pass selectivity + transparency, and Level Match
  working and solo-independent.

### What's new in 0.7.5
- **Multiband Solo + Mono Maker, made correct.** Solo is a post-everything *band*
  monitor — you hear exactly the spectral region the soloed band covers. The Mono
  Maker mono lows live in **band 0's region** (the bottom of the spectrum), so they are
  output when **band 0** is soloed and stay out when only higher bands are. This fixes
  both earlier mistakes: 0.7.3 dropped the lows under any solo (band‑0 solo became a
  low‑cut), and 0.7.4 over‑corrected by re‑adding them under *any* solo (a high‑band
  solo leaked the lows). Self‑test checks band‑0 solo plays them and a high‑band solo
  doesn't.

### What's new in 0.7.4
- **Multiband Solo no longer turns Mono Maker into a low-cut.** The mono low band is
  a parallel utility path (it bypasses the widener), not a Multiband band, so it is
  now re-added even while a band is soloed — previously soloing dropped it and the
  whole low end below the Mono Maker frequency disappeared. The lows stay present and
  mono'd at any Mix; the drive on them follows Mix. A self-test covers it.
- **Crossover phase note:** the band-split stages (Mono Maker, Multiband) are
  Linkwitz-Riley applied identically to L and R, so the Mid is always allpass-
  reconstructed (flat magnitude) — no low-frequency cancellation in the mono sum, no
  combing within a path. The only cross-path comb (dry vs. wet) was fixed in 0.7.2.

### What's new in 0.7.3
- **Multiband Solo now obeys the Mix knob.** A soloed band used to be hard-wired to
  full wet, so the Mix control did nothing while soloing. It now follows Mix like the
  rest of the signal: at Mix = 0% you hear the band's **dry** (un-widened) self, at
  100% the wet — using the same phase-matched dry reconstruction as the 0.7.2 fix,
  masked to the soloed band(s). A self-test covers it.
- **Window opens at the right size on Windows.** The user Window-Size (XS…XL) is now
  **composed** with the host's display/DPI scale instead of overwriting it, so the
  window no longer opens too large (and then shrink on the first Adv click / ignore the
  Window-Size combo) on scaled Windows displays. macOS was unaffected.

### What's new in 0.7.2
- **Multiband is now phase-aligned with the dry/wet Mix.** Previously, at Mix < 100%
  with the Multiband engaged, the wet path carried the crossovers' allpass phase while
  the dry path didn't, so the recombination comb-filtered — and because that phase
  sat on the Mid too, it broke the mono sum (L+R). The dry is now reconstructed
  through the **same gliding crossovers** in lockstep (a phase-matched `A(dry)`), so a
  partial Mix no longer combs and **mono compatibility holds at any Mix**. Mix = 0
  stays a bit-exact null (a smoothstep crossfade clean→aligned over the first ~5% of
  Mix keeps the departure click-free). A self-test guards the mono sum at Mix = 50%.

### What's new in 0.7.1
- **No more crackle when dragging a band's width fast.** Each band's MS width now
  glides per sample (one-pole, ~20 ms, matching the global Width smoother) instead of
  stepping at block boundaries. A width is a pure side-gain, so smoothing it can't
  pitch-shift or comb.
- **CI builds the full set of formats on all three desktop OSes** every push (see below).

### What's new in 0.7.0
This release is built around a ground-up **Multiband (spectral) editor** — the old
rotary multiband is replaced by an Ozone-Imager / Pro-Q-style display:

- **1–4 bands, drawn directly on a live FFT analyser.** Drag a band up/down to set
  its stereo width, drag a split to move it, double-click a split to type a Hz value.
  Click empty space to add a split (neighbours spread to make room); drag a split far
  off the box, or click its **✕**, to remove it (the two regions merge).
- **Per-band Solo** with a headphone icon: click to latch any combination of bands,
  press-and-hold to audition one momentarily, hold-and-drag to slide a whole band.
  Solo monitors **after** the output Mix and obeys the Mix knob (dry band at Mix 0%,
  wet at 100% — see 0.7.3). Solo travels with A/B and undo, but a preset load resets it.
- **Phase-coherent crossovers** (Linkwitz-Riley) with a per-sample octave-rate slew,
  so a fast split drag can never chirp; the bands recombine mono-compatibly.
- A **custom non-linear frequency ruler** (monotone-cubic warp), a cubic-smoothed
  analyser, a band-pass response overlay while dragging a split, and a soft neon
  clip band that lights the exact over-0 dBFS frequencies.
- The split / width / Solo elements **animate** on a reset / preset / A-B / undo, and
  every split drag resolves through a reversible projection so neighbours push aside
  and spring back cleanly. Click-free "duck" switching throughout.
- Reworked **Advanced layout**: four full-width tiers (top bar · scope + Widen ·
  Multiband · Input | Output).

The **0.6.x** line that led here added the variable-band Multiband DSP + GUI, the
4-bit per-band Solo, the band-move / push drag model, the asymmetric click-free
switch duck, and a long tail of GUI polish.

### What's new in 0.5.3
- **M/S is now a decoder** (Ch1 = Mid, Ch2 = Side → L/R); Swap swaps Mid/Side.
- **Frequency knobs are logarithmic** (no dead chunk at the bottom). Mono-Maker
  cutoff is octave-rate-limited so a fast drag can't chirp.
- **Velvet** play/pause burst masked by a fixed-time gate ramp.
- **Knobs/sliders**: neon blue→cyan glow (less green), gradient glow, hover/press
  feedback; slider thumb is neutral until you touch it. Combo hover now lights
  the whole control; toggles brighten only the switch+label; Input toggle labels
  line up by construction.
- **Meters**: dim layer is a fast peak that pushes the peak line, bright is the
  slow RMS; the RMS number snaps up then holds and falls slowly; scale tuned for
  the −24..0 mixing range; bars darker at the bottom.
- Vectorscope no longer shaves the image at the rim; Output module dropped lower
  with a Widen/Output divider; bigger Simple-mode Widen text (names + numbers).
- **CI**: Linux job stays paused; macOS build is the deliverable.

### What's new in 0.5.2
- **M/S is now an input encoder**: it encodes the input to Mid/Side so the Input
  controls (Swap, Balance, Phase) act on Mid & Side; Swap swaps Mid↔Side.
- **Enabling Level Match no longer slams loud**: the loudness re-arm only happens
  on real processing/A-B changes, not on the Match or Bypass toggle.
- **Hover/press feedback** across knobs (arc glow on hover, pointer + halo on
  press/number-drag), toggles, A/B, combos (open state) and sliders.
- **Meters**: a non-uniform dB ruler, more distinct & quicker VU/RMS ballistics,
  refined bars. Value boxes mirror the displayed value exactly (no rounding;
  `2k` accepted for the Mid/Hi crossover; Left balance shown negative).
- Vectorscope clip ring no longer shaved at the edges; squarer combo corners,
  narrower pop-up dead-zones, larger Simple-mode Widen text.
- **CI**: the Linux job is paused (macOS prioritised); the source stays
  cross-platform (Win/Linux) and still builds.

### What's new in 0.5.1
- **Undo/Redo reworked**: each **A/B slot keeps its own** undo history; the
  Undo/Redo buttons only affect the current slot, an A/B switch is never an undo
  step, and **Bypass / Settings / Meters** are excluded from history. Copy is
  undoable on the slot it changed. (Replaces JUCE's global undo manager.)
- **Drive + Mono Maker** no longer sounds like a low cut: the mono low band is
  now driven together with the highs.
- **Value box**: dragging the number and the bare-number/`2k` editor now actually
  work — they were being created with the wrong look-and-feel before.
- **Meters** tuned to iZotope Insight 2 Levels (300 ms VU body, fast riser that
  falls with it, 1 s peak hold), richer bar rendering, equal-size Peak/RMS text.
- UI: Input toggle baseline + Balance layout fixed, "Focus"/"Style" captions
  left-aligned, compact Input-combo lists, bigger Simple-mode Widen labels,
  shorter A/B oval, tidied tooltips.

### What's new in 0.5.0
- **Mono Maker is a proper band-split now**: the low band is summed to mono and
  routed *around* both the widener **and** the dry/wet Mix, straight to Output —
  so the lows are never cut and Level Match measures the full recombined signal
  before Output gain/balance. (Verified: mono bass preserved, side bass removed.)
- **Level Match**: freezes during silence so a big boost no longer slams loud on
  the next play; remembers a value per A/B slot so switching glides; stays
  independent of the Output knob.
- **Level meter overhaul**: per-channel L/R **Peak + RMS** numbers (8 total) with
  held max-peak, clip colours (red Peak / amber RMS, click or replay to reset),
  rate-limited hold-then-fall numbers, fast-rise/slow-fall + faster-falling RMS
  bars, a hold-then-drop peak block, and a richer look.
- **Value boxes**: drag the number to change it; double-click types a *bare*
  number (units hidden) and accepts `2k`/`2kHz`/`2000`; balance edits are signed
  (− = Left); double-click on a knob resets (triple-click no longer re-resets).
- Velvet pause/clip-tail gate retuned; Chorus Rate default 0.50 Hz; Settings no
  longer ride A/B; plus many UI fixes (Adv label fits, A/B oval, Dim-D, Haas
  "Lean" / Dim-D "Style" captions, bigger ø, uniform pop-up rows, Persist reveal).

### What's new in 0.4.1
- **Mono Maker actually works again** and is now a true band-split: the low band
  is summed to mono and bypasses the widener, only the highs are widened (no bass
  cancellation *and* the lows really go mono). Verified by a self-test.
- **Level Match** is now independent of the Output knob, and an A/B swap re-arms
  the measurement so it glides instead of jumping in level.
- **Velvet**: per-sample re-weighting kills the Density-drag zipper; an
  input-presence gate fades the decorrelation tail so pausing no longer bursts.
- **Mono Maker Freq** glides per sample (no pitch-wobble while dragging).
- UI: drag the value **number** to change a knob; level-meter glyph instead of
  "Meters"; premium glowing sliders; compact non-clipping Input toggles; square
  pop-up lists; uniform combo/list font; Persist has a tooltip and dims the
  Settings panel while dragging so you see the live scope; re-balanced Simple /
  Advanced layouts; shorter A/B oval; smaller Apply; nudged Undo/Redo glyphs.

### What's new in 0.4
- **Click-free everything:** a short raised-cosine duck swaps *every* discrete
  control (algorithm, M/S, channel, Mono, Swap, Mono-Maker, Oversampling and
  **Bypass**) at silence, so switches never pop. Drive now crossfades from clean,
  so engaging/disengaging it is seamless and 0 dB is truly identity.
- **Mono Maker is now BEFORE the widener** — collapsing the lows first stops the
  decorrelators spreading the bass, so an L+R sum can't comb-cancel it.
- **Mono-Maker frequency glides** (no more "doubled" artefact while dragging).
- **Fixed-size window** for both modes (no resize flicker when toggling Adv/A-B).
- **Simple mode is just the Widen core**; an **Output module** (Mix, Output,
  Balance, Level Match, Mono-Maker) joins Input and Multiband under **Adv** — and
  Advanced-only modules cleanly default-bypass when Advanced is off.
- **Level meter** reworked: non-uniform scale, numeric **Peak / RMS** readouts,
  a slow + fast RMS pair (Ozone-style) and a peak block; it reveals with a small
  animation. Phase meter gets **-1 / +1** labels. Persisted **Meters / Tooltips**.
- UI polish: redesigned toggles (no clipped text/edges), A/B racetrack frame,
  bigger Undo/Redo, rounded pop-ups, **www.rolly.tech** link + fixed copyright,
  Persist moved into Settings, Haas defaults to the **Left** perceived side.

### What's new in 0.3
- **Bug fixes:** Level-Match **Apply** now *overrides* Output (no more drift on
  repeated presses); **M/S Solo** moved to the input stage so soloing Side stays
  correct; no more pops when toggling polarity / Input Channel / the widening
  algorithm (even during silence); **A/B state persists** across editor close and
  session recall, and A/B / presets no longer flip Advanced / Bypass / Oversampling.
- **Drive** is now peak-preserving (driving harder no longer drops the level).
- **Level meters** (Input/Output L/R, peak + RMS) — toggle in the top bar.
- Vectorscope L/R mirroring fixed; Haas side now means the *perceived* side.
- UI polish: rounder controls, subtle glow/glass, single **A / B** indicator,
  bypass goes red and leaves the toolbar lit, tooltips default **off**, "Adv".

### What's new in 0.2
- **Transparent on load**: a fresh instance does nothing to the sound. Widening
  is driven by a single **Amount** control (0% = bypass-clean); **Width 100%**
  is also identity. (Verified by a self-test.)
- **Click-free knobs**: every continuous control is smoothed; Velvet Noise no
  longer regenerates its random taps while you drag (the old crackle).
- **Restructured UI**: grouped INPUT / WIDEN / OUTPUT modules, single-button A/B
  + Copy, in-window About & Settings overlays, styled tooltips (toggle), bypass
  dimming (controls stay live), undo/redo, balance/correlation meters, clip-red
  scope rim, Output Balance, a **Zero-Latency (live)** mode, and oversampling
  (default 1x) in Settings.
- Versioning: `MAJOR.MINOR.PATCH` (pre-1.0) plus a CI build number (About box).

> Primary build target: **VST3** (+ a Standalone target for convenience).
> **AU (Audio Unit) is built on macOS** — the `macos` CI job produces a
> universal VST3 **and** `.component` (AU), so Logic/GarageBand are covered.
> **AAX** is still out of scope (needs an Avid account + PACE/iLok signing).
> The DSP core is fully decoupled from the plugin wrapper, so adding AAX later
> is near-zero work.

## Prebuilt binaries (no toolchain needed)

Every push runs GitHub Actions, which builds the **full set of formats on all
three desktop OSes** and uploads them as ready-to-use build **artifacts**
(Actions run → *Artifacts*):
- **`Anamorph-macOS`** — universal (Apple Silicon + Intel) **VST3 + AU +
  Standalone**, with an `INSTALL.txt`. AU is what **Logic Pro / GarageBand** load.
- **`Anamorph-Windows`** — x86-64 **VST3 + Standalone** (`.exe`).
- **`Anamorph-Linux`** — x86-64 **VST3 + Standalone**.

macOS plugins are ad-hoc signed but not notarized, so after downloading run
`xattr -dr com.apple.quarantine` on the bundles (see the bundled `INSTALL.txt`).

---

## Quick start (headless Linux)

```bash
# 1. Install build dependencies (Ubuntu; safe to re-run)
scripts/setup-linux.sh

# 2. Configure + build (fetches a pinned JUCE tag via CMake FetchContent)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
#    ...or simply:  scripts/build.sh

# 3. Run the DSP self-tests
scripts/run-tests.sh

# 4. Validate the VST3 with pluginval (strictness 10 = the release gate)
scripts/run-pluginval.sh 10
```

The build prints the path to the produced `Anamorph.vst3`. It is typically at:

```
build/Anamorph_artefacts/Release/VST3/Anamorph.vst3
```

### Network domains the build needs (allow-list in a restricted sandbox)
- Ubuntu apt mirrors — `archive.ubuntu.com` / `ports.ubuntu.com` (or your mirror)
- `github.com` — JUCE source (pinned tag, via CMake `FetchContent`)
- `github.com` — pluginval release download (only for `run-pluginval.sh`)

To build **without network** (JUCE already on disk), point CMake at a local
checkout: `cmake -B build -DANAMORPH_JUCE_PATH=/path/to/JUCE ...`

---

## Architecture overview

The project is split into a **format-agnostic DSP core** and a thin **plugin
wrapper + GUI**, so the same DSP could back AU/AAX later unchanged.

```
src/
  dsp/                         Format-agnostic DSP core (no plugin/JUCE-wrapper deps)
    EngineParameters.h         POD snapshot of all DSP params (wrapper -> engine)
    AnamorphEngine.{h,cpp}     Orchestrates the full processing chain
    MidSide.h                  Gain-correct (1/sqrt2) MS matrix + width helper
    Saturation.h               Drive (tanh waveshaper, unity small-signal gain)
    HaasProcessor.{h,cpp}      Haas precedence delay (1..35 ms, L/R side)
    VelvetNoise.{h,cpp}        Sparse velvet-noise decorrelation (mono->stereo)
    ChorusEngine.{h,cpp}       Chorus + Dimension-D (anti-phase = no pitch wobble)
    MonoMaker.{h,cpp}          Linkwitz-Riley low-freq mono (in place, post-Mix)
    MultibandWidth.{h,cpp}     1–4 band phase-coherent per-band width (Advanced, solo-agnostic)
    SoloMonitor.{h,cpp}        Post-everything Band-Solo audition band-pass (never touches DSP)
    LoudnessMatch.{h,cpp}      BS.1770 K-weighted Auto-Gain (Match/Apply)
    Correlation.h              -1..+1 phase-correlation meter (fast + slow)
    ScopeBuffer.h              Lock-free stereo ring buffer for the vectorscope
  PluginParameters.{h,cpp}     APVTS layout + raw-pointer cache + -> EngineParameters
  PluginProcessor.{h,cpp}      VST3/Standalone wrapper, bus layouts, state, PDC
  PluginEditor.{h,cpp}         Simple/Advanced UI, A/B compare, OpenGL context
  gui/
    LookAndFeel.{h,cpp}        Premium dark "digital" look (no skeuomorphism)
    Vectorscope.{h,cpp}        Diamond/Lissajous goniometer (GPU-composited)
    CorrelationMeter.{h,cpp}   Horizontal + vertical correlation meters
    LevelMeter.{h,cpp}         Per-channel L/R Peak + RMS level meters
    SpectrumImager.{h,cpp}     Multiband spectral editor (FFT + drag-to-split bands)
tests/dsp_tests.cpp            Headless acceptance self-tests
scripts/                       setup / build / test / pluginval
```

### Signal chain (order matters — see `AnamorphEngine::process`)
A strictly serial chain: every stage processes the full signal and hands it to the
next. Band Solo is the very last stage and is monitoring-only.
1. **Input conditioning** — channel kill (L/R/Mono), Swap, Input Balance, polarity,
   M/S decode + M/S solo.
2. **Effect engine** — **Drive (in oversampling) → algorithm (Haas / Velvet / Chorus /
   Dimension-D) → global Width → Multiband Width** (1–4 phase-coherent bands, each with
   its own MS width). This stage is *solo-agnostic* — it always sums every band.
3. **Mix (Dry/Wet)** — blends the conditioned input against the processed signal; the
   dry path is **delay-compensated** to the wet latency and **phase-matched** to the
   Multiband crossovers so a partial Mix never combs the mono sum (KI #1).
4. **Mono Maker** — collapses the lows of the **mixed** signal to mono in place, so the
   final low end is mono whatever the Mix amount (Mid stays allpass-flat → no
   low-frequency cancellation).
5. **Output stage** — **Output Balance / Output Gain / Level Match** (Level Match
   measures the post-Mono-Maker signal), plus the click-free switch duck.
6. **Band Solo monitor** *(post-everything)* — band-passes the already-produced output
   to the soloed band(s) for auditioning. It **never** changes any DSP stage; with
   nothing soloed the true output passes through untouched.
7. **Metering tap** — taps the monitored output (scope + correlation + levels).

Every discrete switch (algorithm/routing/bypass/solo) is applied at the silent bottom
of a ~4 ms raised-cosine duck, so toggling is click-free even during playback.

### Key engineering decisions
- **Oversampling wraps only the nonlinear/modulation stages** (Drive, Chorus,
  Dimension-D). Linear stages (Haas, Velvet, Width, MS, Mono-Maker, crossovers)
  stay outside it — no needless CPU or latency. If a chain has nothing nonlinear
  to oversample, the oversampler is bypassed and reports **zero** latency.
- Oversampling uses JUCE's **minimum-phase polyphase IIR** half-band filters:
  low latency and — importantly — **no linear-phase pre-ringing / waveform
  misalignment**. Integer latency is requested so **PDC is exact**. (Trade-off:
  IIR has mild phase response vs. a linear-phase FIR; per the spec we prioritise
  *no high latency* and *no pre-ringing*.)
- **Mono compatibility by construction**: Width and decorrelation only ever
  modify the **Side**; `L + R = 2·Mid` always, so summing to mono never
  collapses the signal.
- **Dimension-D has no audible pitch wobble**: each channel sums two delay taps
  modulated in **anti-phase**, so the Doppler pitch shifts cancel — width and
  spaciousness without the "seasick" vibrato.
- **Auto-Gain is perceptual** (ITU-R BS.1770 K-weighting), not peak/RMS. *Match*
  level-matches in real time for fair A/B; *Apply* locks the measured gain into
  Output Gain as a fixed value so exports stay consistent (Kraftur-style). It is
  deliberately **not** a continuously-adapting AGC.
- **Real-time safe**: no allocation / locking / file IO on the audio thread; the
  scope uses a lock-free ring buffer; the GUI redraws at 60 fps.

### I/O layouts
- **stereo → stereo** (default, for already-stereo material)
- **mono → stereo** (the headline "turn Mono into Stereo" — the host instantiates
  this on a mono track). Output is **always stereo**. Mono → mono is not offered.

---

## Validation (headless gate)

- **DSP self-tests** (`tests/dsp_tests.cpp`): MS round-trip is bit-exact; no
  NaN/Inf/denormals across every algorithm × oversampling × feature combination;
  reported latency matches the actual chain delay; true bypass is a null.
- **pluginval**: every build is gated at **strictness 10** (the release gold
  standard). Editor open/close tests run under `xvfb-run` on headless Linux.

### What cannot be verified headlessly
The **audio sound quality** (how the widening / Dimension-D / chorus actually
sound) and the **visual appearance** of the GUI/vectorscope can't be judged in a
headless sandbox. Load the built `.vst3` in a DAW (e.g. Reaper) on a machine with
audio + display to audition. Treat a green build + pluginval pass as
"ready to audition," not final sign-off.

---

## Simple vs. Advanced Mode
**Simple Mode** (default) is just the **Widen** core around the vectorscope:
algorithm, Drive, Amount, Width. **Advanced Mode** (toggle in the top bar) adds
the **Output** module (Mix, Output, Balance, Level Match, Mono-Maker), the
**Input** module (channel mode, swap, mono, input balance, polarity, M/S, M/S
solo) and the **Multiband** width module. Advanced-only modules default-bypass
when Advanced is off, while their knob positions are remembered. Oversampling,
tooltips and scope persistence live in **Settings**.
