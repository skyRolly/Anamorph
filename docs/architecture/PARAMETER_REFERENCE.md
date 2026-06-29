# PARAMETER_REFERENCE.md

Purpose, behaviour, and UI mapping of each parameter. IDs, types, defaults, and ranges are in
`PARAMETER_REGISTRY.md` (the compatibility ledger). This document describes *what each control
does*. Evidence: src/PluginParameters.cpp:114-198, src/PluginParameters.cpp:241-300 (`toEngine`),
src/dsp/AnamorphEngine.cpp (application).

## Advanced-mode gating

When **Advanced Mode** is off, the Input module, Multiband module, and Output module
parameters are forced to neutral defaults in `toEngine` (their tree values are preserved so
re-enabling restores them). Only the core widening controls (Drive, Algorithm, Amount,
per-algorithm shaping, Width, Oversampling, Bypass) are always active.
Evidence [Verified]: src/PluginParameters.cpp:246-297.

## Input conditioning (Advanced)

| Param | Behaviour |
|---|---|
| `channelMode` | Kills a channel (Left Only / Right Only) or passes Stereo. Applied in `applyInputConditioning`. |
| `monoSum` | Sums L+R → mono. In L/R domain runs **before** Balance; in M/S domain runs **after** decode. |
| `swap` | Swaps L/R, or Mid↔Side when M/S Mode is on (hence display "Swap L/R (M/S)"). |
| `inputBalance` | −1 (L) .. +1 (R); centre = unity, turning attenuates the far side. Shown as signed % (Left negative). |
| `polarityL` / `polarityR` | Phase invert L/M and R/S respectively (M/S-aware names). Smoothed sign ramp (no click). |
| `msMode` | Treats the chain as M/S: Input is decoded Mid/Side→L/R; Balance/polarity act in the M/S domain. |
| `solo` (M/S Solo) | Off / Mid / Side. Isolates Mid or Side **before** the widener (monitoring aid). |

Evidence [Verified]: src/dsp/AnamorphEngine.cpp:392-440, :614-617.

## Effect / widening engine (always active)

| Param | Behaviour |
|---|---|
| `drive` | 0..24 dB pre-saturation; peak-preserving tanh with makeup `1/tanh(g)`; identity at 0 dB via a 0..2 dB blend. Runs inside oversampling. |
| `algorithm` | Selects Haas / Velvet Noise / Chorus / Dimension-D. |
| `amount` | Unified widening intensity 0..1. **0 = identity** (transparent on load). Each algorithm smooths it internally. |
| `haasDelay` | Haas precedence delay 1..35 ms. |
| `haasSide` (Haas Focus) | The *perceived* side (precedence): the opposite channel is delayed. |
| `velvetDensity` | Velvet-noise diffusion character (active-tap count). |
| `chorusRate` / `chorusDepth` | Chorus LFO rate (Hz) / depth. |
| `dimMode` (Dimension Mode) | Dimension-D voicing: Subtle/Classic/Wide/Lush → engine modes 1..4. |
| `width` | Global MS width: 0 = mono, 1.0 = identity, 2 = wide. |

Evidence [Verified]: src/dsp/AnamorphEngine.cpp:331-389, :442-469, :648-653; src/PluginParameters.cpp:257.

## Multiband (Advanced)

Driven primarily by the drag-to-split `SpectrumImager` display.

| Param | Behaviour |
|---|---|
| `mbEnable` | Enables the multiband band-width stage. **Default true.** Toggle is a click-free output crossfade (not a duck). |
| `mbBands` | Active band count 1..4 (only the first `mbBands−1` splits / `mbBands` widths are used). |
| `mbSolo` | 4-bit mask: bit *b* = band *b* soloed. Post-everything monitor; preset load resets it. |
| `mbFreqLow/Mid/High` | The three crossover splits (band 1\|2, 2\|3, 3\|4). Log range 20..20 kHz; DSP re-orders + Nyquist-clamps. |
| `mbWidthLow/Mid/HiMid/High` | Per-band MS width 0..2 (bands 1..4). |

Evidence [Verified]: src/dsp/EngineParameters.h:67-81; src/dsp/AnamorphEngine.cpp:367-376.

## Mono Maker / Mix / Output (Advanced)

| Param | Behaviour |
|---|---|
| `monoMakerOn` | Collapses lows below `monoMakerFreq` to mono, **post-Mix**, in place. |
| `monoMakerFreq` | Crossover 20..500 Hz (log, centred on 120). |
| `mix` | Dry/wet 0..1. Dry is delay- and phase-aligned; Mix=0 is a bit-exact null. |
| `outputGain` | Manual output trim −24..24 dB. **Replaced** by the matched gain when Level Match is on. |
| `outputBalance` | Whole-plugin output balance (signed %). |
| `autoGainMatch` (Level Match) | Real-time BS.1770 loudness match for fair A/B. "Apply" locks the measured gain into Output Gain. |

Evidence [Verified]: src/dsp/AnamorphEngine.cpp:761-829; src/PluginProcessor.cpp:135-157 (`applyAutoGain`).

## Bypass / view

| Param | Behaviour |
|---|---|
| `bypass` | Registered host bypass. Click-free crossfade to the delay-aligned RAW input; analysis keeps running. |
| `advancedMode` | UI Simple/Advanced toggle. APVTS param; travels with A/B + Undo, excluded from presets. |

## Host-hidden (InternalState — see `PARAMETER_REGISTRY.md`)

Oversampling (drives DSP + PDC), Window Size, Scope Persistence, Tooltips, UI Animations,
Show Meters. Two-way bound to the GUI via `juce::Value`; never automatable, never in
A/B/Undo/presets. Evidence [Verified]: src/InternalState.h:10-39, :60-82.
