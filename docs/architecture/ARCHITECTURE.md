# ARCHITECTURE.md

System architecture of the Anamorph stereo-tools plugin. Descriptive reference; binding
constraints live in `docs/policies/`.

## 1. Two-layer decomposition

Anamorph is split into a **format-agnostic DSP core** and a thin **plugin wrapper + GUI**.

```
            ┌────────────────────────── Plugin format wrapper (JUCE) ──────────────────────────┐
            │  AnamorphAudioProcessor : juce::AudioProcessor                                    │
            │    - APVTS parameter tree (host automation, state save/recall)                    │
            │    - InternalState (host-hidden session/view params)                              │
            │    - PresetManager, custom Undo/Redo, A/B compare                                 │
            │    - processBlock(): reads params -> EngineParameters -> engine.process(buffer)   │
            └──────────────────────────────────┬───────────────────────────────────────────────┘
                                                │ EngineParameters (POD snapshot, per block)
            ┌───────────────────────────────────▼──────────────────────────────────────────────┐
            │  anamorph::AnamorphEngine  (src/dsp/, AnamorphDSP INTERFACE lib)                   │
            │    Orchestrates the serial DSP chain; knows nothing about JUCE's APVTS or hosts.   │
            └───────────────────────────────────────────────────────────────────────────────────┘
```

The DSP core depends only on `juce_dsp` / `juce_audio_basics`; it has **no dependency on the
plugin wrapper, APVTS, parameter IDs, or any host**. This is what makes an AU/AAX wrapper a
near-zero-cost addition.

Evidence [Verified]:
- Source: src/dsp/AnamorphEngine.h:38-39 ("Knows nothing about JUCE's plugin wrapper / APVTS")
- Source: src/dsp/EngineParameters.h:3-17 (POD decoupling rationale)
- Source: CMakeLists.txt:124-135 (`AnamorphDSP` INTERFACE library, depends only on juce_dsp)

## 2. The decoupling boundary: `EngineParameters`

The wrapper reads the APVTS atomics once per block and fills an `EngineParameters` POD
(`ParamPointers::toEngine`), then hands it to the engine. The engine never touches a
parameter ID.

Evidence [Verified]:
- Source: src/PluginParameters.cpp:241-300 (`toEngine`)
- Source: src/PluginProcessor.cpp:127-131 (per-block snapshot → `engine.setParameters` → `engine.process`)

## 3. Module inventory

| Layer | Component | File | Role |
|---|---|---|---|
| Wrapper | `AnamorphAudioProcessor` | `src/PluginProcessor.{h,cpp}` | VST3/Standalone wrapper; bus layouts; state; PDC; Undo; A/B |
| Wrapper | `ParamPointers` / layout | `src/PluginParameters.{h,cpp}` | APVTS layout + atomic cache + `toEngine` |
| Wrapper | `InternalState` | `src/InternalState.h` | Host-hidden session/view params (Oversampling, view) |
| Wrapper | `PresetManager` | `src/PresetManager.{h,cpp}` | Factory + user `.anamorph` presets |
| Editor | `AnamorphAudioProcessorEditor` | `src/PluginEditor.{h,cpp}` | Simple/Advanced UI, OpenGL context, timers |
| Editor | `gui/*` | `src/gui/` | LookAndFeel, Vectorscope, SpectrumImager, LevelMeter, CorrelationMeter |
| DSP | `AnamorphEngine` | `src/dsp/AnamorphEngine.{h,cpp}` | Serial chain orchestrator |
| DSP | `MidSide` | `src/dsp/MidSide.h` | MS matrix (1/√2) + width helper |
| DSP | `HaasProcessor` | `src/dsp/HaasProcessor.{h,cpp}` | Precedence delay widening |
| DSP | `VelvetNoise` | `src/dsp/VelvetNoise.{h,cpp}` | Velvet-noise decorrelation |
| DSP | `ChorusEngine` | `src/dsp/ChorusEngine.{h,cpp}` | Chorus + Dimension-D |
| DSP | `MonoMaker` | `src/dsp/MonoMaker.{h,cpp}` | LR4 low-freq mono (post-Mix) |
| DSP | `MultibandWidth` | `src/dsp/MultibandWidth.{h,cpp}` | 1–4 band per-band width |
| DSP | `SoloMonitor` | `src/dsp/SoloMonitor.{h,cpp}` | Post-everything band audition |
| DSP | `LoudnessMatch` | `src/dsp/LoudnessMatch.{h,cpp}` | BS.1770 Level Match |
| DSP | `CorrelationMeter` | `src/dsp/Correlation.h` | Phase correlation |
| DSP | `LevelMeters` | `src/dsp/LevelMeters.h` | L/R Peak+RMS metering |
| DSP | `ScopeBuffer` | `src/dsp/ScopeBuffer.h` | Lock-free SPSC scope ring |

## 4. Thread layers (summary; full detail in `THREAD_MODEL.md`)

- **Audio thread** — `processBlock` → `AnamorphEngine::process`. Real-time, no allocation/lock/IO.
- **Message (GUI) thread** — editor timers (24 Hz), paint, parameter writes, Undo coalesce.
- **OpenGL render thread** — GPU compositing on **macOS/Windows only**; Linux/BSD renders CPU-side.
- **No worker/background threads** (FFT runs on the GUI thread).

Evidence [Verified]:
- Source: src/PluginEditor.cpp:246-256 (OpenGL platform gate)
- Source: src/dsp/ScopeBuffer.h:8-18 (lock-free SPSC)
- Source: src/PluginProcessor.cpp:66 (`juce::ScopedNoDenormals`)

## 5. I/O layouts

- **stereo → stereo** (default).
- **mono → stereo** (the headline "turn Mono into Stereo"; host instantiates on a mono track).
- Output is **always stereo**. **mono → mono is Not Supported** (deliberately rejected).

Evidence [Verified]:
- Source: src/PluginProcessor.cpp:33-43 (`isBusesLayoutSupported`), :77-78 (mono→stereo upmix)

## 6. Cross-references

- Order of stages: `SIGNAL_FLOW.md`
- Reorder constraints: `DSP_GRAPH_REFERENCE.md`
- Threads + lock-free data flow: `THREAD_MODEL.md`
- Parameters: `PARAMETER_REGISTRY.md`, `PARAMETER_REFERENCE.md`
- State: `STATE_SERIALIZATION.md`, `SERIALIZATION_REGISTRY.md`
- Decisions behind this structure: `design-decisions/ADR_INDEX.md`
