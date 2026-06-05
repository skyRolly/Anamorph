# Anamorph — Stereo Tools Audio Plugin (VST3)

**Anamorph** is a stereo-field toolkit: it turns mono into stereo, controls
stereo width (globally and per band), and provides the full set of stereo tools
(MS, mono-maker, channel utilities, monitoring) around a high-end diamond
vectorscope. Built with **CMake + JUCE** only — it configures and builds
entirely from the command line on a headless Linux machine, with no IDE.

> Primary build target: **VST3** (+ a Standalone target for convenience).
> AU and AAX are intentionally **out of scope for now** (AU needs macOS/Xcode;
> AAX needs an Avid account + PACE/iLok signing). The DSP core is fully
> decoupled from the plugin wrapper, so enabling them later is near-zero work.

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

# 4. Validate the VST3 with pluginval (strictness 8 = the standard gate)
scripts/run-pluginval.sh 8
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
    MonoMaker.{h,cpp}          Linkwitz-Riley low-freq mono
    MultibandWidth.{h,cpp}     3-band phase-coherent per-band width (Advanced)
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
tests/dsp_tests.cpp            Headless acceptance self-tests
scripts/                       setup / build / test / pluginval
```

### Signal chain (order matters — see `AnamorphEngine::process`)
1. **Input conditioning** — channel kill (L/R/Mono), Swap, Input Balance, polarity.
2. **MS encode** *(only if MS mode)* — wraps Drive + the algorithm.
3. **Effect engine** — **Drive → algorithm (Haas / Velvet / Chorus / Dimension-D) → global Width**.
4. **Multiband Width** *(Advanced Mode only)*.
5. **Mono Maker** — lows → mono, deliberately **after** widening.
6. **MS decode** *(only if MS mode)*.
7. **Mix (Dry/Wet)** — the dry path is **delay-compensated** to the wet latency.
8. **Output Gain / Auto Gain**.
9. **Metering tap** — always taps the **final** output (scope + correlation).

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
- **pluginval**: target **strictness 8** as the standard gate; aim for **10**
  before release. Editor open/close tests run under `xvfb-run` on headless Linux.

### What cannot be verified headlessly
The **audio sound quality** (how the widening / Dimension-D / chorus actually
sound) and the **visual appearance** of the GUI/vectorscope can't be judged in a
headless sandbox. Load the built `.vst3` in a DAW (e.g. Reaper) on a machine with
audio + display to audition. Treat a green build + pluginval pass as
"ready to audition," not final sign-off.

---

## Simple vs. Advanced Mode
**Simple Mode** (default) shows only the core controls around the vectorscope:
algorithm, Drive, Width, Mix, Output, Mono-Maker, Auto-Gain. **Advanced Mode**
(toggle in the top bar) reveals MS mode, multiband width, channel mode, swap,
input balance, polarity, solo M/S, oversampling and scope persistence.
