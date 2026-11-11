# PARAMETER_REFERENCE.md

Purpose, behaviour, and UI mapping of each parameter. IDs, types, defaults, and ranges are in
`PARAMETER_REGISTRY.md` (the compatibility ledger). This document describes *what each control
does*. Evidence: src/PluginParameters.cpp:192-336 (`createAnamorphLayout`), :350-437 (`toEngine`),
src/dsp/AnamorphEngine.cpp (application).

## Advanced-mode gating

When **Advanced Mode** is off, the Input module, Multiband module, and Output module
parameters are forced to neutral defaults in `toEngine` (their tree values are preserved so
re-enabling restores them). Only the core widening controls (Drive, Algorithm, Amount,
per-algorithm shaping, Width, Oversampling, Bypass) are always active.
Evidence [Verified]: src/PluginParameters.cpp:388-475.

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

Evidence [Verified]: src/dsp/AnamorphEngine.cpp:1050-1128, :614-617.

## Effect / widening engine (always active)

| Param | Behaviour |
|---|---|
| `drive` | 0..24 dB pre-saturation; peak-preserving tanh with makeup `1/tanh(g)`; identity at 0 dB via a 0..2 dB blend. Runs inside oversampling. |
| `algorithm` | Selects Haas / Velvet Noise / Chorus / Dimensional. |
| `amount` | Unified widening intensity 0..1. **0 = identity** (transparent on load). Each algorithm smooths it internally. |
| `haasDelay` | Haas precedence delay 1..35 ms. |
| `haasSide` (Haas Focus) | The *perceived* side (precedence): the opposite channel is delayed. |
| `velvetDensity` | Velvet-noise diffusion character (active-tap count). |
| `chorusRate` / `chorusDepth` | Chorus LFO rate (Hz) / depth. |
| `dimMode` (Dimensional Style) | Dimensional voicing: Subtle/Classic/Wide/Lush → engine modes 1..4. Read only by the Dimensional algorithm, so a move made under any other algorithm is adopted without a transition duck (ADR-0004, Correction of 2026-09-21). |
| `width` | Global MS width: 0 = mono, 1.0 = identity, 2 = wide. |

Evidence [Verified]: src/dsp/AnamorphEngine.cpp:950-1047, :442-469, :648-653; src/PluginParameters.cpp:309.

## Multiband (Advanced)

Driven primarily by the drag-to-split `SpectrumImager` display.

| Param | Behaviour |
|---|---|
| `mbEnable` | Enables the multiband band-width stage. **Default true.** Toggle is a click-free output crossfade (not a duck). |
| `mbBands` | Active band count 1..4 (only the first `mbBands−1` splits / `mbBands` widths are used). |
| `mbSolo` | 4-bit mask: bit *b* = band *b* soloed. Post-everything monitor; preset load resets it. |
| `mbFreqLow/Mid/High` | The three crossover splits (band 1\|2, 2\|3, 3\|4). Log range 20..20 kHz; DSP re-orders + Nyquist-clamps. |
| `mbWidthLow/Mid/HiMid/High` | Per-band MS width 0..2 (bands 1..4). |

Evidence [Verified]: src/dsp/EngineParameters.h:67-81; src/dsp/AnamorphEngine.cpp:988-997.

Multiband split text entry (inline and host converters): positive bare values below
20 mean kHz; `20` and larger values mean Hz. Explicit `Hz`, `k` or `kHz` units take
precedence, case-insensitively. Whitespace and punctuation are ignored except decimal
points and a leading minus; commas concatenate digits (`1,5k` = 15 kHz). Valid numbers
are limited to 20–20,000 Hz; malformed/empty text preserves the current value.
Typed placement uses the full axis while keeping neighboring splits 46 px apart, so
an interior split can be constrained away from a parsed endpoint. Changed typed edits
use the existing display easing when animations are on; sound updates immediately.
These text rules do not apply to Mono Maker or Chorus.

Evidence [Verified]: src/PluginParameters.cpp (`parseMultibandFrequency`, Multiband
parameter text conversion); src/gui/SpectrumImager.cpp (`commitFreqEditor`, `paint`,
`advanceFrame`); State test 158.

## Mono Maker / Mix / Output (Advanced)

| Param | Behaviour |
|---|---|
| `monoMakerOn` | Collapses lows below `monoMakerFreq` to mono, **post-Mix**, in place. |
| `monoMakerFreq` | Crossover 20..500 Hz (log, centred on 120). |
| `mix` | Dry/wet 0..1. Dry is delay- and phase-aligned; Mix=0 is a bit-exact null. |
| `outputGain` | Manual output trim −24..24 dB. **Replaced** by the matched gain when Level Match is on. |
| `outputBalance` | Whole-plugin output balance (signed %). |
| `autoGainMatch` (Level Match) | Real-time BS.1770 loudness match for fair A/B. "Apply" locks the measured gain into Output Gain. |

Evidence [Verified]: src/dsp/AnamorphEngine.cpp:1589-1667; src/PluginProcessor.cpp:507-573 (`applyAutoGain`).

## Bypass / view

| Param | Behaviour |
|---|---|
| `bypass` | Registered host bypass. Click-free crossfade to the delay-aligned RAW input; analysis keeps running. |
| `advancedMode` | UI Simple/Advanced toggle. APVTS param; travels with A/B + Undo, excluded from presets. |

## Host-hidden (InternalState — see `PARAMETER_REGISTRY.md`)

Oversampling (drives DSP + PDC), UI Scale (`int_uiScale`; labelled *Window Size* before 0.9.2 —
display name only, the identifier is immutable), Vectorscope Persistence (`int_scopePersist`;
labelled *Vectorscope Persist* before 0.9.7 — display name only, the identifier is immutable),
Tooltips, UI Animations, Show Meters. Two-way bound to the GUI via `juce::Value`; never
automatable, never in A/B/Undo/presets. Evidence [Verified]: src/InternalState.h:13-42, :60-82.
