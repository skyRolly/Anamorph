# THREAD_MODEL.md

Threads, their responsibilities, and the only legal data paths between them. Binding rules
are in `docs/policies/THREADING_POLICY.md` and `docs/policies/REALTIME_AUDIO_POLICY.md`.

## Threads

| Thread | Established | Responsibilities |
|---|---|---|
| **Audio** | host → `AnamorphAudioProcessor::processBlock` | DSP, parameter snapshot read, meter/scope/correlation *production*. Real-time: no allocation, lock, or IO. |
| **Message / GUI** | JUCE message loop; editor `juce::Timer` (24 Hz) + editor `meterVBlank` + per-visualizer `FrameClock` (display-rate vblank, capped ~120 Hz) | Painting, parameter writes (APVTS + InternalState), Undo coalesce, FFT, meter/scope *consumption*. |
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
- Partially Verified (history): CHANGELOG.md [0.8.5]; commit c924ff8
- See `design-decisions/ADR-0011-linux-x11-cpu-render.md` for the decision record.

## Timers / animation cadence

| Mechanism | Rate | Work | Source |
|---|---|---|---|
| `VBlankAttachment meterVBlank` | per display frame (dt clamped ≤ 0.05 s) | meter-reveal + micro-anims easing | PluginEditor.cpp:616-622 |
| Editor `juce::Timer` | 24 Hz | view-state sync, preset display, `pollUndoCoalesce()`, undo/redo enable, match-gain readout | PluginEditor.cpp:613,917-1003 |
| `Vectorscope` `FrameClock` | display-rate, capped ~120 Hz | `repaint()` | Vectorscope.cpp; FrameClock.h |
| `LevelMeter` `FrameClock` | display-rate, capped ~120 Hz (shown only) | `repaint()` | LevelMeter.cpp; FrameClock.h |
| `StereoMeter` `FrameClock` | display-rate, capped ~120 Hz (shown only) | dt-corrected smooth + `repaint()` | CorrelationMeter.cpp; FrameClock.h |
| `SpectrumImager` `FrameClock` | display-rate, capped ~120 Hz | FFT, dt-corrected eased positions, `repaint()` | SpectrumImager.cpp; FrameClock.h |

The four visualizers were fixed 60 Hz `juce::Timer`s; since the adaptive-refresh
change they ride the display's vertical blank via `gui::FrameClock` (a
`juce::VBlankAttachment` wrapper), executing at every `ceil(rate/126)`-th vblank
so the rate tracks the panel but is capped near 120 Hz (60→60, 120→120, 144→72,
240→120), with a 60 Hz wall-clock fallback when the cadence can't be measured.
All still run on the **message thread**, and every temporal ease/decay was
re-expressed in `dt` form so its time constant is display-independent. The idle
gates (S1/S2/S3, H15) and the once-per-block audio-side ballistics are unchanged.

Editor destructor order (matters): release VBlank → `stopTimer()` → `openGLContext.detach()`
(the VBlank lambda captures `this`). Source: src/PluginEditor.cpp:627-632.

## Legal cross-thread data paths (lock-free)

### Audio → GUI (production → consumption)
| Data | Mechanism | Writer | Reader | Source |
|---|---|---|---|---|
| Scope samples | `ScopeBuffer` SPSC ring (release/acquire on one `atomic<uint64_t> write`; the index is published once per block, so readers see whole blocks atomically) | audio `pushBlock()` | GUI `readLatest()` / `writeCount()` | ScopeBuffer.h:28-80 |
| Level meters | `std::atomic<float/int>` (relaxed), published per block | audio `publish()` | GUI getters | LevelMeters.h:125-198 |
| Correlation / balance / energy | `std::atomic<float>` (relaxed) | audio `publish()` | GUI getters | Correlation.h:50-95 |
| Level-Match gain (dB) | `std::atomic<float>` (relaxed) | audio `process()` | GUI `getMatchGainDb()` | LoudnessMatch.h:112 |
| Sound-param change generation | `std::atomic<uint32> soundParamGen` (relaxed) — a monotonic **generation / staleness hint, NOT payload sync**: it only tells the GUI "a sound-parameter value has changed since you last looked" so the 24 Hz signature caches rebuild; the parameter *values* themselves travel via the APVTS-atomics path in the GUI→Audio table. Relaxed is sufficient (nothing is published *through* it). | `parameterValueChanged` (whichever thread changes a value — audio/host under automation, or the message thread) + `reassertParameters` on host restore | GUI `pollUndoCoalesce()` / `PresetManager::isDirty()`; micro-anim re-arm gate (Wave 2 / H15) | PluginProcessor.h `soundParamGen`; .cpp `parameterValueChanged` / `reassertParameters` |
| View-param + InternalState change generations (Wave 2 / H15) | `std::atomic<uint32> viewParamGen` and `InternalState::gen` (both relaxed) — the **same generation-hint pattern as `soundParamGen`**: no payload, no ordering role. Together with `soundParamGen` they cover every path that can move an animated widget while the cursor is outside the editor, so the 60 Hz micro-anim poll re-arms on three counter loads instead of hashing every tracked widget value per frame. | `ViewGenWatcher::parameterValueChanged` (Bypass — the one view param; whichever thread automates it); `InternalState::valueTreePropertyChanged` (message thread, incl. session restore) | Editor `stepMicroAnims()` pre-gate | PluginProcessor.h `viewParamGen`/`ViewGenWatcher`; InternalState.h `gen` |

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
- `ScopeBuffer` is single-producer / single-reader-thread: exactly one audio writer; all reads
  happen on the message thread as stateless peeks (`readLatest` copies, `writeCount` staleness
  probe) — the Vectorscope and the SpectrumImager are two such read sites, never concurrent.

Evidence [Verified]: src/dsp/ScopeBuffer.h:8-18; src/dsp/LevelMeters.h; src/dsp/Correlation.h.
