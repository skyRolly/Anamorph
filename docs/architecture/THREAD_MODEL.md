# THREAD_MODEL.md

Threads, their responsibilities, and the only legal data paths between them. Binding rules
are in `docs/policies/THREADING_POLICY.md` and `docs/policies/REALTIME_AUDIO_POLICY.md`.

## Threads

| Thread | Established | Responsibilities |
|---|---|---|
| **Audio** | host → `AnamorphAudioProcessor::processBlock` | DSP, parameter snapshot read, meter/scope/correlation *production*. Real-time: no allocation, lock, or IO. |
| **Message / GUI** | JUCE message loop; editor `juce::Timer` (24 Hz) + per-component 60 Hz timers | Painting, parameter writes (APVTS + InternalState), Undo coalesce, FFT, meter/scope *consumption*. |
| **OpenGL render** | `openGLContext.attachTo(*this)` — **macOS/Windows only** | GPU compositing of the editor's paint. Absent on Linux/BSD. |
| **Worker / background** | none | No `std::thread`/`Thread`/`ThreadPool`. FFT runs on the GUI thread. |

Evidence [Verified]:
- Source: src/PluginProcessor.cpp:64-131 (audio thread), :66 `ScopedNoDenormals`
- Source: src/PluginEditor.cpp:613 (24 Hz timer), :616-622 (VBlank), :246-256 (OpenGL gate)
- Source: src/gui/Vectorscope.h:18-20 ("Nothing is ever drawn on the audio thread")

## OpenGL platform gate (0.8.5)

```cpp
openGLContext.setContinuousRepainting (false);
#if ! (JUCE_LINUX || JUCE_BSD)
    openGLContext.attachTo (*this);
#endif
```

On Linux/X11, attaching a GL context adds an embedded child X11 window whose
`ConfigureNotify` events make the host's `XEmbedComponent` post async lambdas capturing a raw
`this`; rapid editor open/close (pluginval "Editor Automation", real Linux DAWs) can fire
that lambda after the editor is destroyed → use-after-free in JUCE's X11 embedding. Linux/BSD
therefore render the editor (including the vectorscope) **CPU-side** via the normal `paint()`
path — visually identical. macOS/Windows keep GPU compositing.

Evidence [Verified]:
- Source: src/PluginEditor.cpp:246-256 (gate + rationale comment)
- Source: src/PluginEditor.cpp:1151-1152 (`triggerRepaint` guarded by `isAttached()`)
- Partially Verified (history): README:39-47 (0.8.5); commit c924ff8
- See `design-decisions/ADR-0011-linux-x11-cpu-render.md` for the decision record.

## Timers / animation cadence

| Mechanism | Rate | Work | Source |
|---|---|---|---|
| `VBlankAttachment meterVBlank` | per display frame (dt clamped ≤ 0.05 s) | meter-reveal + micro-anims easing | PluginEditor.cpp:616-622 |
| Editor `juce::Timer` | 24 Hz | view-state sync, preset display, `pollUndoCoalesce()`, undo/redo enable, match-gain readout | PluginEditor.cpp:613,917-1003 |
| `Vectorscope` timer | 60 Hz | `repaint()` | Vectorscope.cpp:15 |
| `LevelMeter` timer | 60 Hz | `repaint()` | LevelMeter.cpp:13 |
| `StereoMeter` timer | 60 Hz | smooth + `repaint()` | CorrelationMeter.cpp:10 |
| `SpectrumImager` timer | 60 Hz | FFT, eased positions, `repaint()` | SpectrumImager.cpp:114,601-699 |

Editor destructor order (matters): release VBlank → `stopTimer()` → `openGLContext.detach()`
(the VBlank lambda captures `this`). Source: src/PluginEditor.cpp:627-632.

## Legal cross-thread data paths (lock-free)

### Audio → GUI (production → consumption)
| Data | Mechanism | Writer | Reader | Source |
|---|---|---|---|---|
| Scope samples | `ScopeBuffer` SPSC ring (release/acquire on one `atomic<uint64_t> write`) | audio `push()` | GUI `readLatest()` | ScopeBuffer.h:28-57 |
| Level meters | `std::atomic<float/int>` (relaxed), published per block | audio `publish()` | GUI getters | LevelMeters.h:125-198 |
| Correlation / balance / energy | `std::atomic<float>` (relaxed) | audio `publish()` | GUI getters | Correlation.h:50-95 |
| Level-Match gain (dB) | `std::atomic<float>` (relaxed) | audio `process()` | GUI `getMatchGainDb()` | LoudnessMatch.h:112 |

### GUI → Audio
| Data | Mechanism | Writer | Reader | Source |
|---|---|---|---|---|
| Automatable params | APVTS atomics (`std::atomic<float>*`, read once/block) | GUI attachments / host | audio `toEngine` | PluginParameters.cpp:201-300 |
| Host-hidden params (Oversampling, view) | `InternalState` `juce::ValueTree` + `int`/`float` atomics | GUI `juce::Value` binding | audio (oversample only) | InternalState.h:60-138 |
| Momentary solo audition | `std::atomic<int> soloPreviewMask` (relaxed, −1 = use param) | GUI `setSoloPreview` | audio processBlock | PluginProcessor.h:72-73,130; .cpp:128 |
| Meter hold reset | `std::atomic<int> resetReq` (exchange) | GUI `resetHold()` | audio `process()` | LevelMeters.h:58,62 |
| UI-animation flag → imager | `const std::atomic<float>*` (relaxed) | InternalState | GUI imager timer | InternalState.h:72; SpectrumImager.cpp:626 |

## Forbidden

- No painting, allocation, locking, or file IO on the audio thread.
- No direct cross-thread access to non-atomic shared state. The only synchronisers are the
  `ScopeBuffer` release/acquire index and relaxed published atomics.
- `ScopeBuffer` is single-producer/single-consumer: exactly one audio writer, one GUI reader.

Evidence [Verified]: src/dsp/ScopeBuffer.h:8-18; src/dsp/LevelMeters.h; src/dsp/Correlation.h.
