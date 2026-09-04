# API_REFERENCE.md

Public interfaces of the core classes. Signatures are Verified against the headers cited.
This documents *interfaces and responsibilities*, not implementation — see `DSP_ALGORITHMS.md`
for the maths.

## `AnamorphAudioProcessor` — `src/PluginProcessor.h`

The VST3/Standalone wrapper (`: juce::AudioProcessor, private APVTS::Listener`).

| Member | Signature | Responsibility |
|---|---|---|
| `prepareToPlay` | `void (double sampleRate, int samplesPerBlock)` | Prepares engine; sets initial params; requests the latency report (synchronous on the message thread, one 20 Hz tick otherwise — D-1). |
| `processBlock` | `void (juce::AudioBuffer<float>&, juce::MidiBuffer&)` | Per-block: transport/seek detect → param snapshot → `engine.process`. |
| `isBusesLayoutSupported` | `bool (const BusesLayout&) const` | Accepts stereo→stereo, mono→stereo only. |
| `getStateInformation` / `setStateInformation` | `void (...)` | Full session save/recall (see `STATE_SERIALIZATION.md`). |
| `getBypassParameter` | `juce::AudioProcessorParameter* () const` | Returns the registered host bypass param. |
| `getAPVTS` / `getEngine` / `getPresets` / `getInternal` | accessors | Editor access to subsystems. |
| `undo` / `redo` / `canUndo` / `canRedo` / `pollUndoCoalesce` | Undo API | Custom per-A/B-slot undo (sound params only). |
| `adoptPendingHostState` | `void ()` | Message thread only: adopts every restore an off-message-thread `setStateInformation` has handed over (D-2, ADR-0036). Called by the processor's timer, by `pollUndoCoalesce` and at every state-mutating entry point; a no-op when nothing is pending. It **drains to a fixed point** (§15): a restore arriving during an adoption is adopted in the same pass, so a caller that drains before it acts is acting on the state of every restore that has arrived. It is a drain, not a wait — it stops the moment the cell is empty and never blocks. |
| `applyAutoGain` | `void ()` | Locks measured Level-Match gain into Output Gain. |
| `setSoloPreview` / `clearSoloPreview` | `void (int) / void ()` noexcept | Momentary solo audition (atomic, non-undoable). |
| `abSwitchTo` / `abCopyToOther` / `abActiveSlot` | A/B API | A/B compare living in the processor (survives editor close). |
| `abToggle` | `void ()` | The A/B toggle as its own operation (ADR-0036 §18). Drains any pending host restore, then derives the destination as the other slot of the **post-drain** active slot. The editor's toggle calls this; it used to compute `abSwitchTo (abActiveSlot() == 0 ? 1 : 0)` from a read taken before the drain, which a restore flipping the active slot turned into a no-op. `abSwitchTo (int)` stays the primitive for an explicit target, which is intent rather than a stale derivation. |

Evidence [Verified]: src/PluginProcessor.h:23-113.

## `AnamorphEngine` — `src/dsp/AnamorphEngine.h`

Format-agnostic DSP orchestrator. Driven only by `EngineParameters`.

| Member | Signature | Responsibility |
|---|---|---|
| `prepare` | `void (double sampleRate, int maxBlockSize)` | Allocates all buffers/oversamplers; resets; **then snaps every smoothed value onto the current snapshot's targets** — the engine's own via `snapSmoothers()`, and each module's via its `snapToTargets()`, which the modules' own `prepare()` cannot do because it runs before the snapshot is pushed in (ER-DSP-09). A restored session therefore opens IN its sound instead of gliding into it. (Allocation happens here, never in `process`.) |
| `reset` | `void ()` | Settles smoothers/crossfades; clears delay lines; re-latches OS engagement. |
| `setParameters` | `void (const EngineParameters&) noexcept` | Adopts a snapshot; continuous live, discrete ducked. |
| `setTransportPlaying` | `void (bool) noexcept` | Feeds transport edge (Velvet tail kill). |
| `process` | `void (juce::AudioBuffer<float>&) noexcept` | Runs the full serial chain in place. |
| `getLatencySamples` | `int () const noexcept` | Current PDC latency (integer). A function of `p.oversample` alone since ADR-0034 — not of whether the wrap is engaged. |
| `predictLatency` | `int (const EngineParameters&) const noexcept` | Latency for an arbitrary snapshot (message-thread safe). Reads `e.oversample` and nothing else, so it agrees with `getLatencySamples()` for any snapshot carrying the same factor. |
| `getScopeBuffer` / `getCorrelation` / `getLevels` / `getMatchGainDb` | accessors | GUI read access to analysis. |
| `injectMatchGainDb` | `void (float) noexcept` | A/B per-slot Level-Match restore (atomic). |
| `requestDuck` | `void () noexcept` | Force a masking duck around a bulk param swap (atomic). |

Evidence [Verified]: src/dsp/AnamorphEngine.h:46-142.

## `ParamPointers` / layout — `src/PluginParameters.h`

| Member | Signature | Responsibility |
|---|---|---|
| `createAnamorphLayout` | `juce::AudioProcessorValueTreeState::ParameterLayout ()` (free fn) | Builds the entire APVTS parameter tree. |
| `normalisedAsRendered` | `float (const juce::RangedAudioParameter&, float)` / `float (const juce::AudioProcessorParameter&)` (free fns) | The normalised value mapped onto the grid the plug-in actually renders and stores: `convertTo0to1(convertFrom0to1(v))`. The DSP reads `getRawParameterValue()` (the denormalised, interval-snapped value) and both the preset and session formats store that same number, so this is what "the sound" means for every *has it changed?* comparison — the preset modified-marker and the undo / A-B coalescer both build their signature from it (ADR-0036 §17). A no-op for stock `AudioParameterFloat`; it snaps the `RawChoice` / `RawBool` values, which deliberately keep the exact normalised value in `getValue()` for pluginval. **Apply exactly once per value**: for the custom log/exp frequency ranges it is the identity in real arithmetic but not idempotent in float. |
| `ParamPointers::bind` | `void (juce::AudioProcessorValueTreeState&)` | Caches raw atomic pointers (no per-block string lookup). |
| `ParamPointers::toEngine` | `anamorph::EngineParameters (int oversampleIndex) const` | Converts host state → DSP snapshot. |
| `pid::isViewParam` / `pid::isPresetExcluded` | `bool (const juce::String&)` | Shared exclusion lists (A/B, undo, presets). |

Evidence [Verified]: src/PluginParameters.h:131-139, :74-87; .cpp:53-300.

## `InternalState` — `src/InternalState.h`

Host-hidden session/view state (not in APVTS).

| Member | Signature | Responsibility |
|---|---|---|
| `*Value()` (6) | `juce::Value ()` | Two-way GUI binding (oversample, uiScale, scopePersist, meters, tooltips, animations). |
| `oversampleIndex` | `int () const noexcept` | Lock-free audio-thread read (0..3). |
| `publishField` (private) | `void (const juce::Identifier&, juce::uint32 generation)` | Publishes ONE property's engine-facing mirror — the animation flag for `uiAnimations`, the tagged engine-config word for `oversample`, nothing for a GUI-only field (ADR-0036 §18). Every single property change publishes through this; the whole tree (`publishFromTree`) is published only where the whole tree is coherent for its generation — the end of `writeResolved` and the constructor. This is what stops an unrelated Settings edit republishing a stale Oversampling under a pending restore's generation. |
| `copyState` / `restoreState` | `juce::ValueTree () const / void (const juce::ValueTree&)` | Persistence. |
| `migrateFromLegacyApvts` | `void (const juce::ValueTree&)` | One-time pre-0.8.4 migration (legacy APVTS → InternalState). |
| `resolveRestore` / `resolveLegacy` | `static juce::ValueTree (const juce::ValueTree&)` | The pure half of the two above: the six typed values a session resolves to. Any thread (D-2, ADR-0036). |
| `applyResolved` | `void (const juce::ValueTree&)` | Writes a resolved set into the GUI-bound tree — the inline restore, the newest arrival by definition, every field. **Message thread only.** |
| `adoptResolved` | `void (const juce::ValueTree&, juce::uint32 generation)` | Writes an ADOPTED host-thread restore into the tree, keeping every field the user edited after that restore arrived (its recorded generation ≥ `generation`): precedence by arrival (ADR-0036 §9). Republishes the word from the whole tree afterwards. **Message thread only.** |
| `publishEngineConfig` | `bool (const juce::ValueTree&, juce::uint32 generation) noexcept` | Publishes the oversampling index from a resolved set into the generation-tagged engine-config word — the part of an off-message-thread restore that must be synchronous. Lands (returns true) only if no higher generation stands: the latest restore wins (ADR-0036 §8). Any thread but the audio thread. |
| `noteAdoptedGeneration` | `void (juce::uint32) noexcept` | Message thread: the generation of the last host restore it adopted, which tags every publication the tree makes from then on. Set before an adoption writes the tree. |
| `engineConfigGeneration` | `juce::uint32 () const noexcept` | The generation that published the current engine config. Read by the restore adoption to decide whether this restore is still the latest arrival, and so whether to re-install its own sound (ADR-0036 §10); also State test 43. The second half of that decision is the processor's `soundSetGen`, which counts wholesale sound replacements only, so an ordinary sound edit made while a restore is pending is not mistaken for one (§12). `noteWholeSoundReplaced()` allocates and returns one replacement's token and `applySoundTree` passes it on, so a restore identifies its own sound by the value it was handed rather than by reading the counter back (§13). Every path allocates AFTER its last sound write, so the counter orders replacements by completion; `soundReplacementToken (begin)` brackets the writes and returns 0 — "no owner provable", which never compares equal — when another replacement ran inside them (§14). |
| `onChanged` | `std::function<void()>` | Fires on the message thread after any property change; the processor republishes its program snapshot from it. |
| `onOversampleChanged` | `std::function<void()>` | Fires on the message thread so the wrapper re-reports PDC. |

Evidence [Verified]: src/InternalState.h:165-376.

## `PresetManager` — `src/PresetManager.h`

| Member | Signature | Responsibility |
|---|---|---|
| `presetDirectory` / `fileSuffix` | `static juce::File () / static juce::String ()` | `.anamorph` user-preset location. |
| `refresh` / `entries` | rescan / accessor | User-folder listing. |
| `load` / `loadFile` / `step` / `saveUser` | preset ops (message thread) | Load/save sound params only (view params excluded). Each sets `Selection`. |
| `currentName` / `isDirty` / `baseline` / `setMeta` | preset metadata | Name + dirty-star, travels with state sets. |
| `Selection` / `selection` | `struct { Kind kind; juce::String factoryId; juce::File file; }` | Which row produced the current sound: a factory preset's immutable internal id, or a user preset's file. Drives `currentIndex()` so a shared name cannot mis-tick the menu. Travels with a `StateSet` through A/B and undo, **and with the session** since 0.9.2. |
| `SelectionFields` / `encodeSelection` / `decodeSelection` | `struct { juce::String kind, factoryId, userFile; }` + two statics | The one place that knows the wire form of a `Selection` (3 strings, written to `AnamorphRoot` and to each A/B slot). Absent/empty/unrecognised decodes to `Kind::unknown` = the pre-0.9.2 name fallback. A user preset sitting **directly in** the preset folder, whose name cannot itself be mistaken for a path, encodes as its **file name**; everything else (elsewhere on disk, a sub-folder, a `~`-leading name) encodes as its absolute path, so `decode(encode(s)) == s` holds. Nothing here reads or writes a parameter, and nothing touches a user preset **file**. |
| `soundSignatureAfterLoading` | `static juce::String (const APVTS&, const juce::ValueTree& savedSound)` | The **pure prediction** of the signature `soundSignatureFor` will return once `savedSound` has been loaded, from the tree alone (ADR-0036 §18, closes KI-029). Differs from `soundSignatureForSavedTree` by modelling the parameter's own store/report pass (`normalisedAsRendered` applied twice), one range mapping deeper than a save's capture and not idempotent in float for the log-mapped frequency ranges. It is exact on the reference Release build (0 of 3000 sweep points) but only within float tail on others (the sanitizer build disagrees), so the load paths do not trust it bare: `load`/`loadFile` use the file-local `loadBaselineFromTree`, which reconciles this prediction against one read-back per parameter at signature resolution — the live form where they agree within ~10 ulps, the predicted value where a foreign write moved the parameter. Public so State test 55 can measure it. |
| `soundSignatureForSavedTree` | `static juce::String (const APVTS&, const juce::ValueTree& savedSound)` | The signature `soundSignatureFor` will return once `savedSound` has been loaded — i.e. the clean baseline of a preset whose file holds that tree (ADR-0036 §17). Derived from the tree alone, so a saved preset's baseline describes its own **bytes** rather than a second read of the live parameters. Resolves every parameter through the same helper `applySoundTree` uses, so the apply path and the baseline cannot disagree about what a file means. It does **not** re-apply `normalisedAsRendered`: the tree holds the denormalised value, so resolving it already *is* that mapping, and applying it twice is not idempotent in float for the custom-mapped frequency ranges. |
| `beforeStateCapture` | `std::function<void()>` | Test seam, empty in production (one null check, non-audio path). Fired immediately before a clean baseline is fixed: by `saveUser` before its one state capture, and by `load`/`loadFile` after the preset's sound is applied — the instant a mutation would have to land to split a baseline from what it describes. State tests 52 and 55 use it. |
| `adoptPending` | `std::function<void()>` | Fired by `step()` before it reads the current row, so the processor adopts a pending host restore first and the prev/next step is relative to the **authoritative** selection (ADR-0036 §18). Found by the round-10 entry-point audit as the fourth instance of the rule the A/B toggle broke: a relative target derived from a selection a pending restore was about to replace. `onAboutToSave` is the save path's instance of the same hook; both are wired to `adoptPendingHostState`. |
| `onAboutToSave` / `soundParamGeneration` | `std::function<void()>` / `std::function<juce::uint32()>` | The two hooks `saveUser` uses to keep the file and the clean baseline one coherent pair (ADR-0036 §16). `onAboutToSave` drains any pending host restore **before** the sound is captured, so the bytes are the session the plug-in is on. `soundParamGeneration` is no longer read by `saveUser` at all — §17 replaced its two reads and their retry with ONE capture, so there is nothing to re-check; it remains `isDirty()`'s memo generation. Both optional. |
| `Entry::factoryId` | `juce::String` | Empty for user presets; the factory preset's internal id otherwise. Immutable — renaming a preset is a display change, renaming an id would re-point live A/B and undo slots. |

Evidence [Verified]: src/PresetManager.h:30-101.

## DSP module public interfaces

See `DSP_ALGORITHMS.md` for per-module method signatures (prepare/reset/process/setters) and
the algorithm behind each. All modules follow the same contract: **allocation only in
`prepare()`; `process()`/`reset()` never allocate, lock, or do IO** (see
`REALTIME_SAFETY_AUDIT.md`).
