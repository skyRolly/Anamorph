# ADR-0001 — Format-agnostic DSP core via `EngineParameters` POD

**Status:** Accepted

## Context
The plugin must build as VST3 + Standalone today and potentially AU/AAX later. JUCE couples
parameters to `AudioProcessorValueTreeState` (APVTS), which is wrapper-specific.

## Problem
If the DSP reads APVTS / parameter IDs directly, the core is welded to one plugin format and
cannot be reused or unit-tested without the full wrapper.

## Options
- **A. DSP reads the APVTS directly.** Simpler, but couples DSP to JUCE's wrapper and host.
- **B. DSP driven by a plain-old-data snapshot filled by the wrapper.** Chosen.

## Decision
The DSP lives in `anamorph::AnamorphEngine`, driven solely by an `EngineParameters` POD that the
wrapper fills once per block (`ParamPointers::toEngine`). The DSP core is compiled as the
`AnamorphDSP` **INTERFACE** library depending only on `juce_dsp` / `juce_audio_basics` — never on
the plugin wrapper. The engine "knows nothing about parameter IDs or hosts."

## Consequences
- The headless `AnamorphTests` console app links the same DSP core directly (no wrapper).
- An AU/AAX wrapper is near-zero work (the README's stated payoff).
- Cost: every parameter must be marshalled through `EngineParameters` and `toEngine`.

## Related code
- `src/dsp/EngineParameters.h:3-17`, `src/dsp/AnamorphEngine.h:38-39`
- `src/PluginParameters.cpp:241-300` (`toEngine`)
- `CMakeLists.txt:62-73` (INTERFACE lib), `:149-166` (tests link the core)

Evidence [Verified]:
- Source: src/dsp/AnamorphEngine.h:38-39; src/dsp/EngineParameters.h:3-17; CMakeLists.txt:62-73
