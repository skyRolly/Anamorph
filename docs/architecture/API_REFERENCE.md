# API_REFERENCE.md

Public interfaces of the core classes. Signatures are Verified against the headers cited.
This documents *interfaces and responsibilities*, not implementation — see `DSP_ALGORITHMS.md`
for the maths.

## `AnamorphAudioProcessor` — `src/PluginProcessor.h`

The VST3/Standalone wrapper (`: juce::AudioProcessor, private APVTS::Listener`).

| Member | Signature | Responsibility |
|---|---|---|
| `prepareToPlay` | `void (double sampleRate, int samplesPerBlock)` | Prepares engine; sets initial params; reports latency. |
| `processBlock` | `void (juce::AudioBuffer<float>&, juce::MidiBuffer&)` | Per-block: transport/seek detect → param snapshot → `engine.process`. |
| `isBusesLayoutSupported` | `bool (const BusesLayout&) const` | Accepts stereo→stereo, mono→stereo only. |
| `getStateInformation` / `setStateInformation` | `void (...)` | Full session save/recall (see `STATE_SERIALIZATION.md`). |
| `getBypassParameter` | `juce::AudioProcessorParameter* () const` | Returns the registered host bypass param. |
| `getAPVTS` / `getEngine` / `getPresets` / `getInternal` | accessors | Editor access to subsystems. |
| `undo` / `redo` / `canUndo` / `canRedo` / `pollUndoCoalesce` | Undo API | Custom per-A/B-slot undo (sound params only). |
| `applyAutoGain` | `void ()` | Locks measured Level-Match gain into Output Gain. |
| `setSoloPreview` / `clearSoloPreview` | `void (int) / void ()` noexcept | Momentary solo audition (atomic, non-undoable). |
| `abSwitchTo` / `abCopyToOther` / `abActiveSlot` | A/B API | A/B compare living in the processor (survives editor close). |

Evidence [Verified]: src/PluginProcessor.h:20-80.

## `AnamorphEngine` — `src/dsp/AnamorphEngine.h`

Format-agnostic DSP orchestrator. Driven only by `EngineParameters`.

| Member | Signature | Responsibility |
|---|---|---|
| `prepare` | `void (double sampleRate, int maxBlockSize)` | Allocates all buffers/oversamplers; resets. (Allocation happens here, never in `process`.) |
| `reset` | `void ()` | Settles smoothers/crossfades; clears delay lines; re-latches OS engagement. |
| `setParameters` | `void (const EngineParameters&) noexcept` | Adopts a snapshot; continuous live, discrete ducked. |
| `setTransportPlaying` | `void (bool) noexcept` | Feeds transport edge (Velvet tail kill). |
| `process` | `void (juce::AudioBuffer<float>&) noexcept` | Runs the full serial chain in place. |
| `getLatencySamples` | `int () const noexcept` | Current PDC latency (integer; OS only). |
| `predictLatency` | `int (const EngineParameters&) const noexcept` | Latency for an arbitrary snapshot (message-thread safe). |
| `getScopeBuffer` / `getCorrelation` / `getLevels` / `getMatchGainDb` | accessors | GUI read access to analysis. |
| `injectMatchGainDb` | `void (float) noexcept` | A/B per-slot Level-Match restore (atomic). |
| `requestDuck` | `void () noexcept` | Force a masking duck around a bulk param swap (atomic). |

Evidence [Verified]: src/dsp/AnamorphEngine.h:45-128.

## `ParamPointers` / layout — `src/PluginParameters.h`

| Member | Signature | Responsibility |
|---|---|---|
| `createAnamorphLayout` | `juce::AudioProcessorValueTreeState::ParameterLayout ()` (free fn) | Builds the entire APVTS parameter tree. |
| `ParamPointers::bind` | `void (juce::AudioProcessorValueTreeState&)` | Caches raw atomic pointers (no per-block string lookup). |
| `ParamPointers::toEngine` | `anamorph::EngineParameters (int oversampleIndex) const` | Converts host state → DSP snapshot. |
| `pid::isViewParam` / `pid::isPresetExcluded` | `bool (const juce::String&)` | Shared exclusion lists (A/B, undo, presets). |

Evidence [Verified]: src/PluginParameters.h:90-98, :74-87; .cpp:53-300.

## `InternalState` — `src/InternalState.h`

Host-hidden session/view state (not in APVTS).

| Member | Signature | Responsibility |
|---|---|---|
| `*Value()` (6) | `juce::Value ()` | Two-way GUI binding (oversample, uiScale, scopePersist, meters, tooltips, animations). |
| `oversampleIndex` | `int () const noexcept` | Lock-free audio-thread read (0..3). |
| `copyState` / `restoreState` | `juce::ValueTree () const / void (const juce::ValueTree&)` | Persistence. |
| `migrateFromLegacyApvts` | `void (const juce::ValueTree&)` | One-time pre-0.8.4 migration (legacy APVTS → InternalState). |
| `onOversampleChanged` | `std::function<void()>` | Fires on the message thread so the wrapper re-reports PDC. |

Evidence [Verified]: src/InternalState.h:62-159.

## `PresetManager` — `src/PresetManager.h`

| Member | Signature | Responsibility |
|---|---|---|
| `presetDirectory` / `fileSuffix` | `static juce::File () / static juce::String ()` | `.anamorph` user-preset location. |
| `refresh` / `entries` | rescan / accessor | User-folder listing. |
| `load` / `loadFile` / `step` / `saveUser` | preset ops (message thread) | Load/save sound params only (view params excluded). Each sets `Selection`. |
| `currentName` / `isDirty` / `baseline` / `setMeta` | preset metadata | Name + dirty-star, travels with state sets. |
| `Selection` / `selection` | `struct { Kind kind; juce::String factoryId; juce::File file; }` | Which row produced the current sound: a factory preset's immutable internal id, or a user preset's file. Drives `currentIndex()` so a shared name cannot mis-tick the menu. Travels with a `StateSet` through A/B and undo, **and with the session** since 0.9.2. |
| `SelectionFields` / `encodeSelection` / `decodeSelection` | `struct { juce::String kind, factoryId, userFile; }` + two statics | The one place that knows the wire form of a `Selection` (3 strings, written to `AnamorphRoot` and to each A/B slot). Absent/empty/unrecognised decodes to `Kind::unknown` = the pre-0.9.2 name fallback. A user preset sitting **directly in** the preset folder, whose name cannot itself be mistaken for a path, encodes as its **file name**; everything else (elsewhere on disk, a sub-folder, a `~`-leading name) encodes as its absolute path, so `decode(encode(s)) == s` holds. Nothing here reads or writes a parameter, and nothing touches a user preset **file**. |
| `Entry::factoryId` | `juce::String` | Empty for user presets; the factory preset's internal id otherwise. Immutable — renaming a preset is a display change, renaming an id would re-point live A/B and undo slots. |

Evidence [Verified]: src/PresetManager.h:30-101.

## DSP module public interfaces

See `DSP_ALGORITHMS.md` for per-module method signatures (prepare/reset/process/setters) and
the algorithm behind each. All modules follow the same contract: **allocation only in
`prepare()`; `process()`/`reset()` never allocate, lock, or do IO** (see
`REALTIME_SAFETY_AUDIT.md`).
