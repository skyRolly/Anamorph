#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginParameters.h"
#include "PresetManager.h"
#include "InternalState.h"
#include "AbSlotIndex.h"          // anamorph::kNumAbSlots (single source of truth for A/B sizing)
#include "dsp/AnamorphEngine.h"

#include <memory>
#include <functional>

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
                               private juce::AudioProcessorParameter::Listener, // sound-param gestures (undo)
                               private juce::Timer // D-1: off-thread latency delivery (KI-027)
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

    // D-2 (RISK-007), 2026-09-03. Every piece of PROGRAM state this class owns -- the
    // preset name / identity / dirty baseline, the two A/B slots and the active index,
    // the per-slot Level-Match memory, the undo history, the committed baseline and
    // gesture bookkeeping, and InternalState's Settings tree -- is MESSAGE-THREAD
    // state: only the message thread ever writes it, and the editor reads it there.
    // A host that calls setStateInformation from some other thread (the macOS AU
    // autosave shape, pluginval's AU background-thread state test, an out-of-spec
    // VST3 host) therefore no longer writes any of it: the sound half of the restore
    // (the APVTS, JUCE-owned and thread-aware) and the engine-config word (the
    // oversampling) are applied synchronously on the caller's thread, and the DECODED metadata tail
    // is handed to the message thread as one immutable object that this method adopts.
    // It is served by the processor's own 20 Hz timer (so it runs with no editor
    // open) and drained at the top of every message-thread entry point that reads or
    // mutates program state, so a user action after a restore always sees the
    // restore. Public because the editor's tick is one of those entry points (it goes
    // through pollUndoCoalesce) and because the state suite drains deterministically
    // instead of waiting a timer period. Message thread only; a no-op when nothing
    // is pending (one relaxed atomic load).
    void adoptPendingHostState();

    // Test seams (D-2): EMPTY in production, so each costs one null check on a
    // non-audio path. A harness installs one to run code at an ownership boundary
    // -- after the host side's mailbox take inside an off-thread save, after the
    // message thread's take inside an adoption -- which is the only way to
    // reproduce a reviewed interleaving deterministically rather than by timing.
    // Installed and cleared on the main thread while no other thread can reach them.
    struct Seams { std::function<void()> afterHostSaveTake, afterRestoreTake, beforeRestorePut,
                                        afterRestoreSoundApplied; };
    Seams seams;

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

    // The delivery half of updateLatency(), WITHOUT touching latencyUpdateRequest.
    // Separated because clearing the flag twice for one delivery is what loses a
    // concurrent request -- see timerCallback().
    void deliverLatency();

    // D-1 (KI-027), approved 2026-09-01. Route EVERY latency re-report through
    // here rather than calling updateLatency() directly from a listener: under
    // VST3 host automation of drive/algorithm, `parameterChanged` runs on the
    // AUDIO thread, and setLatencySamples' notification chain takes locks and --
    // on a real change -- allocates and write()s in the wrapper. On the message
    // thread the update stays synchronous, so nothing about the common path
    // changes; anywhere else it becomes a request the timer below consumes.
    // prepareToPlay goes through here too (round 15, ER-STATE-19): a host that
    // prepares off the message thread must not deliver from that thread either.
    void requestLatencyUpdate();

    // Consumes a deferred request at ~20 Hz on the message thread. The host can
    // therefore learn about a latency change up to one interval (50 ms) after the
    // parameter moved -- documented in LATENCY_MODEL.md, and acceptable because
    // the alternative is a lock and an allocation on the audio thread.
    void timerCallback() override;

    // Set by requestLatencyUpdate() from a non-message thread; cleared by the
    // timer and by updateLatency() itself (so a message-thread prepareToPlay,
    // which supersedes any pending request, does not leave a stale one behind).
    // Written with RELEASE off the message thread and consumed with ACQUIRE, so a
    // consumed request also publishes the parameter write that raised it. Relaxed on
    // both sides was measurably lossy -- see requestLatencyUpdate().
    std::atomic<int> latencyUpdateRequest { 0 };

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
        // Which preset row produced this state (#4). Travels with A/B and undo like the
        // rest of this struct, and since 0.9.2 it is SERIALIZED too -- once on the root for
        // the live selection and once per A/B slot -- so reopening a project ticks the row
        // that produced the sound (ADR-0024 as amended; SERIALIZATION_REGISTRY.md).
        // `readSlot` ASSIGNS it rather than merging, so an absent field means the default
        // (unknown) and a repeat restore into one live instance cannot inherit the previous
        // session's slot identity.
        anamorph::PresetManager::Selection selection;
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
    // D-2 round 5 (ADR-0036 §12). Bumped once every time the live parameters are REPLACED
    // WHOLESALE by another state set -- an A/B apply, an undo/redo, a preset load, a
    // restore's own sound install -- and NOT by an individual parameter edit, which is
    // what `soundParamGen` counts. That is exactly the distinction the restore adoption
    // needs: a state set installed after the restore's sound means the live sound is
    // some other session's and the adoption must re-install its own, while a knob the
    // user turned means the restored session is still live with a newer edit in it,
    // which the adoption must not erase. Written by whichever thread performs the
    // replacement (a decode runs on the host's), read on the message thread; relaxed,
    // because it carries no payload -- the value that matters travels inside the
    // RestoreDecode, whose cell provides the ordering.
    std::atomic<juce::uint32> soundSetGen { 1 };
    // Allocates the token for ONE replacement and returns it. The token identifies the
    // operation, not the moment: `fetch_add` hands each caller a value no other caller
    // can be handed, so an operation that keeps its own return value can later ask "is
    // the live sound still the one I installed?" by comparing the counter against it.
    // Reading the counter back AFTER performing a replacement answers a different and
    // wrong question -- it returns whatever the LAST replacement was, which is another
    // operation's token whenever one overlapped (D-2 round 6, ADR-0036 §13).
    juce::uint32 noteWholeSoundReplaced() noexcept
    {
        return soundSetGen.fetch_add (1, std::memory_order_relaxed) + 1;
    }
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
    // Remembered Level-Match per A/B slot (#23). A runtime cache, never serialized --
    // and therefore reset by every restore along with the slots themselves, or a
    // restore with no A/B data would leak the previous project's gains into the first
    // switch (ER-STATE-20). 0 dB is both the initialiser and the fresh-instance value.
    float abMatchGain[anamorph::kNumAbSlots] = { 0.0f, 0.0f };

    // ------------------------------------------------------------------------
    //  D-2 (RISK-007): the program-state ownership boundary. ADR-0036.
    //
    //  ARCHITECTURE REVIEW GATE: APPROVED (human architecture review, 2026-09-03).
    //  This section is a Thread Model change -- new cross-thread paths and new atomic
    //  ordering -- which `docs/policies/ARCHITECTURE_REVIEW_GATE.md` gates and
    //  `AI_AGENT_POLICY.md` makes an agent Hard Stop that only human review clears.
    //  The architecture a reviewer approved is the one ADR-0036 records: message-thread
    //  ownership of the program metadata, the two single-object exchange cells, the
    //  generation-tagged engine-config word, and the precedence rules for a user action
    //  overlapping a restore. Work that stays inside those decisions is covered; a
    //  change that adds a thread, a cross-thread path or an ordering-critical atomic
    //  beyond them is a new gated change and must say so.
    //
    //  THREADS. `M` is the JUCE message thread (the editor, this processor's timer,
    //  every in-spec VST3 host call). `H` is any other thread a host uses for
    //  getStateInformation / setStateInformation. The audio thread touches nothing
    //  in this section: its inputs are the APVTS parameter atomics, InternalState's
    //  engine-config word (the oversampling index, one relaxed load) and
    //  `soloPreviewMask`, exactly as before.
    //
    //  OWNERSHIP. Everything above this comment that is not an atomic is owned by
    //  M. Two immutable value types cross the M/H boundary, each through its own
    //  single-object exchange cell whose ownership rule is: whichever side's
    //  `exchange` returns the pointer owns it. Each carries its own generation. No hazard pointers, no reader-side
    //  lock, no reference-count race -- the same request/consume shape D-1 uses
    //  for the latency flag, with a payload. The host contract that its state calls
    //  are serialized (never two at once) is what makes H "one side"; it is the
    //  same contract JUCE's AudioProcessor already relies on.
    //
    //    H -> M  `pendingRestore`: the DECODED tail of an off-message-thread
    //            restore. H publishes it after applying the sound (APVTS) and the
    //            engine-config word synchronously; M adopts it in
    //            adoptPendingHostState() with the code that runs inline on M. A
    //            restore superseded before adoption is freed by the H side that
    //            supersedes it, so at most one object ever exists.
    //    M -> H  `programMailbox`: an immutable snapshot of the program state M
    //            owns, republished after every mutation of it. An off-message-thread
    //            getStateInformation takes the latest into its own H-side view
    //            (`hostProgramView`) and serializes from that plus the JUCE-locked
    //            APVTS copy. M frees a snapshot H never took; H frees the view it
    //            replaces.
    //
    //  THE PENDING WINDOW. Between H publishing a restore and M adopting it, an
    //  H-side save must describe the sound H just applied, not the previous
    //  program: H keeps the view it built from that restore (`hostRestoreView`) and
    //  uses it whenever the newest snapshot it holds carries a generation OLDER than
    //  its own last restore. The generation travels inside the snapshot, so the
    //  decision and the object it is about are one thing: a snapshot published
    //  after the adoption says so itself, whichever moment H took it.
    //
    //  THE ENGINE-CONFIG WORD. The one thing a restore publishes for the AUDIO side
    //  -- the oversampling index -- is stored synchronously by whichever thread
    //  restores, as one word tagged with the restore's generation, and lands only if
    //  no newer restore has published (InternalState::publishEngineConfig, a CAS).
    //  M's later adoption of that restore republishes with the same generation
    //  (idempotent), and the adoption of a restore a newer one has superseded yields
    //  to the newer one's value: an older restore never overwrites a newer one.
    //
    //  LIFETIME, in full: `pendingRestore` and `programMailbox` hold at most one
    //  object each and free it on replacement or in the destructor; the two H-side
    //  views are unique_ptrs replaced on H and destroyed with the processor. Nothing
    //  is ever freed while another thread can still reach it, because a pointer is
    //  reachable from exactly one side at a time.
    // ------------------------------------------------------------------------

    // The program metadata a save needs, as ONE immutable value. A slot whose
    // params tree is INVALID means "lazily initialised from current"
    // (SERIALIZATION_REGISTRY.md, `AB` child) and is resolved at serialization time
    // from the live parameters plus this snapshot's own preset metadata -- which is
    // what abEnsureInit() does on the message thread.
    struct ProgramSnapshot
    {
        // The generation of the last host restore the message thread had adopted when
        // it published this snapshot: PART of the immutable object, so the host side
        // can decide "does this describe my restore?" from the snapshot in hand alone.
        juce::uint32 generation = 0;
        juce::String presetName, presetBaseline;
        anamorph::PresetManager::Selection presetSelection;
        juce::ValueTree internalState;                 // a private copy of the Settings tree
        int abActive = 0;
        StateSet abSlot[anamorph::kNumAbSlots];
    };

    // What a restore DECODES from the blob before anything but the sound is applied:
    // the exact inputs of the adoption tail, thread-neutral. Built on the caller's
    // thread; adopted on M (inline when the caller IS M, else through the cell).
    struct RestoreDecode
    {
        juce::uint32 generation = 0;                   // set only for the H -> M handoff
        juce::String restoredName, restoredBaseline;
        bool haveName = false, haveBaseline = false;   // property PRESENT, as opposed to non-empty
        anamorph::PresetManager::Selection restoredSelection;
        juce::ValueTree internalResolved;              // the six typed Settings values to write
        int abActive = 0;
        StateSet abSlot[anamorph::kNumAbSlots];        // invalid params = the documented default
        // The SOUND this restore installed, kept so the adoption can re-install it
        // (D-2 round 4, ADR-0036 §10): a message-thread action that REPLACED the live
        // parameters between the decode and the adoption would otherwise leave this
        // restore's metadata over that action's sound. `soundSetGen` is the whole-sound
        // replacement counter as it stood immediately after the decode installed its
        // sound, so the adoption can tell "another state set has been installed since"
        // (re-install) from "the restored sound is still the one live, whatever the
        // user has since edited in it" (leave it alone -- ADR-0036 §12). The token is
        // the one THIS restore's own sound install was handed (§13), never a later read
        // of the shared counter, which would name an overlapping replacement instead.
        juce::ValueTree soundParams;
        juce::uint32    soundSetGen = 0;
    };

    // A single-object handoff cell. `put` publishes and frees whatever the other side
    // never took; `take` claims ownership. Both are one acq_rel exchange.
    template <typename T>
    struct ExchangeCell
    {
        ~ExchangeCell() { delete slot.load (std::memory_order_acquire); }
        void put (T* fresh) noexcept { delete slot.exchange (fresh, std::memory_order_acq_rel); }
        T*   take() noexcept         { return slot.exchange (nullptr, std::memory_order_acq_rel); }
        bool empty() const noexcept  { return slot.load (std::memory_order_relaxed) == nullptr; }
        std::atomic<T*> slot { nullptr };
    };

    ExchangeCell<RestoreDecode>   pendingRestore;    // H -> M
    ExchangeCell<ProgramSnapshot> programMailbox;    // M -> H
    std::unique_ptr<const ProgramSnapshot> hostProgramView, hostRestoreView; // H-side only
    // The two generation counters are each ONE side's plain state: H counts the
    // restores it hands over (monotonic; 0 = none yet) and M records the last one it
    // adopted. They cross the boundary only INSIDE the immutable objects -- a
    // RestoreDecode carries the generation H gave it, a ProgramSnapshot the generation
    // M had adopted when it published -- so neither side ever pairs a decision with
    // a generation read at a different moment than the object it decides about.
    juce::uint32 hostRestoreGen    = 0;              // H only
    juce::uint32 adoptedGeneration = 0;              // M only
    // The host-serialization contract, made detectable instead of merely assumed
    // (D-2 rounds 4-5, ADR-0036 §11). The three members above this line that H owns --
    // `hostRestoreGen` and the two views -- are plain, which is correct exactly as long
    // as the host never runs two OFF-MESSAGE-THREAD state calls at once.
    //
    // THE PRIMARY EVIDENCE, so the assumption is not re-litigated from memory. The
    // pinned VST3 SDK annotates BOTH halves of the pair on the host's UI thread --
    // `IComponent::setState`: "\note [UI-thread & (Initialized | Connected | Setup Done
    // | Activated | Processing)]", and `IComponent::getState` identically
    // (`format_types/VST3_SDK/pluginterfaces/vst/ivstcomponent.h`) -- so on VST3 the
    // two cannot overlap without the host violating the spec, and JUCE asserts the
    // thread for `setState`. On AU nothing pins them: the wrapper's `SaveState` /
    // `RestoreState` pass straight through on the caller's thread, taking neither
    // `getCallbackLock()` nor a `MessageManagerLock`, so serialization there is the
    // host's practice rather than a citable clause. Standalone uses the message thread
    // for both. No wrapper serializes save against restore FOR the plug-in and none
    // can: the guarantee is the host's, and JUCE's whole AudioProcessor state API
    // already rests on it.
    //
    // So the support boundary, stated rather than implied: concurrent host state calls
    // are OUTSIDE supported operation, Anamorph assumes nothing stronger than JUCE
    // itself, and rather than paying for a broken host on every save the two off-thread
    // branches count themselves in and a debug build asserts if a second one ever
    // overlaps. Never blocks, never affects the result, and same-thread nesting cannot
    // occur (no state call re-enters another off the message thread).
    std::atomic<int> offThreadStateCalls { 0 };
    struct OffThreadStateCall
    {
        explicit OffThreadStateCall (AnamorphAudioProcessor& p) : owner (p)
        {
            // A second concurrent off-message-thread state call would race this side's
            // `hostRestoreGen` and its two views. No format this plug-in ships permits it
            // (ADR-0036 §11); a host that does it is broken, and this is where it shows.
            [[maybe_unused]] const auto inFlight = owner.offThreadStateCalls.fetch_add (1, std::memory_order_acq_rel);
            jassert (inFlight == 0); // host issued overlapping off-thread getState/setState
        }
        ~OffThreadStateCall() { owner.offThreadStateCalls.fetch_sub (1, std::memory_order_acq_rel); }
        AnamorphAudioProcessor& owner;
        JUCE_DECLARE_NON_COPYABLE (OffThreadStateCall)
    };
    bool adoptingRestore = false;                    // M: suppress per-field publishes inside an adoption

    // The APVTS root type, captured once at construction so no thread reads the live
    // `apvts.state` handle to learn it (JUCE guards the tree's contents with its own
    // lock; the handle itself is assigned under that lock by replaceState).
    juce::Identifier apvtsStateType;

    // True on the message thread, and when no MessageManager exists at all (a
    // harness): the one predicate that decides "inline" versus "hand off", shared
    // by the latency request (D-1) and the program-state handoff (D-2).
    static bool onMessageThreadOrNoMessageManager() noexcept;

    // Decode a blob into a RestoreDecode, applying the SOUND (the APVTS) on the
    // caller's thread as a side effect -- that half is JUCE-owned and thread-aware
    // and must be synchronous for the ordinary setState-then-prepareToPlay order.
    // Returns false for input that is not a restore (nothing was touched).
    bool decodeRestore (const void* data, int sizeInBytes, RestoreDecode& out);
    // The adoption tail, message thread only: today's restore tail, verbatim.
    void adoptRestoreTail (const RestoreDecode&);
    // Serialize a program snapshot plus the live parameters. Any thread: the APVTS
    // copy is JUCE-locked and the snapshot is immutable.
    void writeState (const ProgramSnapshot&, juce::MemoryBlock& destData);
    // The message thread's own program state as a snapshot value (M only).
    ProgramSnapshot ownedProgram() const;
    // Republish `ownedProgram()` into the mailbox (M only; skipped inside an adoption).
    void publishProgram();
    // The H-side view of a restore H just decoded: what M will own once it adopts it.
    static std::unique_ptr<const ProgramSnapshot> viewOfRestore (const RestoreDecode&, const juce::String& liveSoundSig);
    // The sound half of a restore on the caller's thread: repair on our copy, one
    // locked replaceState, then reassert. Returns the token of the replacement it
    // performed, which is how a restore identifies ITS OWN sound (ADR-0036 §13).
    juce::uint32 applySoundTree (const juce::ValueTree& soundTree);
    // The serialized-text half of the malformed-value repair, on a tree WE own and
    // are about to hand to replaceState (see the .cpp for why it moved here).
    void repairSerializedValues (juce::ValueTree& tree) const;

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
