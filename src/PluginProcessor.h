#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginParameters.h"
#include "PresetManager.h"
#include "InternalState.h"
#include "AbSlotIndex.h"          // anamorph::kNumAbSlots (single source of truth for A/B sizing)
#include "dsp/AnamorphEngine.h"

// ============================================================================
//  AnamorphAudioProcessor
//
//  The VST3 / Standalone format wrapper. Owns the APVTS (parameter tree, state
//  save/recall, host automation) and the format-agnostic AnamorphEngine.
//  Declares the two supported I/O layouts: stereo->stereo and mono->stereo
//  (the "turn Mono into Stereo" headline feature). Output is always stereo.
// ============================================================================
class AnamorphAudioProcessor : public juce::AudioProcessor,
                               private juce::AudioProcessorValueTreeState::Listener,
                               private juce::AudioProcessorParameter::Listener // sound-param gestures (undo)
{
public:
    AnamorphAudioProcessor();
    ~AnamorphAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Anamorph"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    // --- editor access ---
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    anamorph::AnamorphEngine& getEngine() noexcept          { return engine; }
    anamorph::PresetManager&  getPresets() noexcept         { return presets; }
    anamorph::InternalState&  getInternal() noexcept        { return internal; } // host-hidden Settings/view state

    // Custom Undo/Redo: each A/B slot keeps its OWN stack of SOUND-param
    // snapshots; the "view"/Settings params (Bypass, Advanced, Meters, Tooltips,
    // Oversampling, Persist) and A/B switches themselves are never recorded
    // (feedback #10 / #11 / #12). The editor calls pollUndoCoalesce() on its timer
    // to fold a knob gesture into a single step.
    void undo();
    void redo();
    bool canUndo() const noexcept { return ! abUndo[abActive].undo.empty(); }
    bool canRedo() const noexcept { return ! abUndo[abActive].redo.empty(); }
    void pollUndoCoalesce();

    // Auto-Gain "Apply": locks the measured loudness-match gain into Output Gain.
    void applyAutoGain();

    // Momentary solo audition (press-and-hold a Multiband headphone): overrides the
    // engine's solo mask WITHOUT touching the mbSolo parameter, so a hold never lands
    // in undo / A-B history and the previous latched solo returns on release (#8).
    void setSoloPreview (int mask) noexcept   { soloPreviewMask.store (mask & 0x0F, std::memory_order_relaxed); }
    void clearSoloPreview() noexcept          { soloPreviewMask.store (-1, std::memory_order_relaxed); }

    // A/B compare lives in the processor so it survives editor close / session
    // recall. Switching A/B never touches the shared view/Settings params (#13).
    int  abActiveSlot() const noexcept { return abActive; }
    void abSwitchTo (int slot);
    void abCopyToOther();

    // H15 (Wave 2): change generations for the editor's micro-anim re-arm gate.
    // Together with InternalState::generation() they cover every path that can
    // move an animated widget's value while the cursor is outside the editor.
    juce::uint32 soundGeneration() const noexcept { return soundParamGen.load (std::memory_order_relaxed); }
    juce::uint32 viewGeneration()  const noexcept { return viewParamGen.load (std::memory_order_relaxed); }

private:
    void parameterChanged (const juce::String& id, float newValue) override;
    // AudioProcessorParameter::Listener: coalesce a whole user GESTURE into one undo step, and
    // exclude host automation (which never opens a gesture) from undo entirely.
    // The value callback bumps the sound-param generation (S10): the 24 Hz polls
    // rebuild their signature strings only when this counter moved, since the
    // signature is a pure function of the listened (sound) parameter values.
    // Atomic: value changes can arrive from the audio thread (host automation) --
    // the same relaxed published-counter pattern as the meter atomics.
    void parameterValueChanged (int, float) override
    {
        soundParamGen.fetch_add (1, std::memory_order_relaxed);
    }
    void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override;
    void updateLatency();

    // A/B helpers (preserve the shared view/Settings params across a slot apply)
    void abEnsureInit();
    void abApplySlot (int slot);

    // A complete "state set" (#6): the sound parameters PLUS the preset metadata
    // (base name + clean baseline signature) that determines the displayed name
    // and dirty-star. Every undo entry and every A/B slot stores one of these, so
    // undo / A-B / Copy carry the name + dirty state, not just the parameters.
    struct StateSet
    {
        juce::ValueTree params;
        juce::String     name, baseline;
        bool isValid() const noexcept { return params.isValid(); }
    };
    StateSet currentStateSet();                  // current params + live preset meta
    void applyStateSet (const StateSet&);        // restore params (keeping view) + meta

    // Undo helpers
    static bool isViewParam (const juce::String& id) noexcept;
    // Record ONE undo step spanning a preset load (a gesture-less setValueNotifyingHost burst the
    // coalescer would otherwise fold silently into the baseline). Bracketed by the PresetManager hooks.
    void commitPresetSwitchUndoStep();
    juce::String soundSignature() const;
    void applyStatePreservingView (const juce::ValueTree& target);
    // Force every APVTS parameter to its value in a just-restored tree (see the .cpp): a wholesale
    // replaceState does not reliably propagate to every parameter's cached value synchronously.
    // notifyHost=false (host state restore) updates value + DSP atomic WITHOUT notifying the host;
    // notifyHost=true (editor-initiated undo/redo/A-B) notifies host + editor as before.
    void reassertParameters (const juce::ValueTree& restoredApvtsTree, bool notifyHost);
    // apvts.copyState() with each PARAM node additively stamped with its exact raw getValue()
    // ("raw" attribute), so every saved snapshot (host state, A/B slots, undo) round-trips exactly.
    juce::ValueTree copyStateWithRawValues();
    void syncCommitted();

    struct UndoStacks { std::vector<StateSet> undo, redo; };
    UndoStacks abUndo[anamorph::kNumAbSlots];
    StateSet committed;
    juce::String committedSig, lastPolledSig;
    std::atomic<juce::uint32> soundParamGen { 1 }; // bumped by parameterValueChanged (S10)
    juce::uint32 polledGen = 0;                    // generation the poll last built a signature for

    // H15: the view params (only Bypass now) are deliberately NOT listened to by
    // the processor itself -- their gestures must stay out of the undo coalescer --
    // but the editor still needs a re-arm signal when the host automates Bypass
    // with the cursor outside (the bypass toggle is an animated widget). A tiny
    // separate listener bumps a separate generation; gestures are a no-op.
    struct ViewGenWatcher final : juce::AudioProcessorParameter::Listener
    {
        explicit ViewGenWatcher (std::atomic<juce::uint32>& g) noexcept : gen (g) {}
        void parameterValueChanged (int, float) override { gen.fetch_add (1, std::memory_order_relaxed); }
        void parameterGestureChanged (int, bool) override {}
        std::atomic<juce::uint32>& gen;
    };
    std::atomic<juce::uint32> viewParamGen { 1 };
    ViewGenWatcher viewGenWatcher { viewParamGen };
    // Undo coalescing is GESTURE-gated (message thread only, matches the editor-timer poll): count
    // open user gestures; commit exactly one undo step after the LAST gesture-end. Host automation
    // never opens a gesture, so it is never recorded.
    int  openGestures = 0;
    bool pendingGestureCommit = false;

    StateSet abSlot[anamorph::kNumAbSlots]; // A = [0], B = [1]
    int abActive = 0;
    float abMatchGain[anamorph::kNumAbSlots] = { 0.0f, 0.0f }; // remembered Level-Match per A/B slot (#23)

    juce::AudioProcessorValueTreeState apvts;
    ParamPointers params;
    anamorph::PresetManager presets { apvts }; // top-bar preset browser backing (F2)
    anamorph::InternalState internal;          // Settings + Show Meters: host-hidden state
    anamorph::AnamorphEngine engine;

    juce::AudioProcessorParameter* bypassParam = nullptr;
    bool prevPlaying = false; // transport edge-detect for meter reset (#15)
    // Transport reposition (seek) detection so the meter holds also reset on a timeline
    // jump while playing, not only on a stop->play restart (Issue 3).
    juce::int64 prevPosSamples = 0;
    int         prevPosBlock   = 0;
    bool        prevPosValid   = false;
    std::atomic<int> soloPreviewMask { -1 }; // -1 = use the mbSolo param (momentary audition, #8)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnamorphAudioProcessor)
};
