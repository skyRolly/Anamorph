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
    // Oversampling, Persistence) and A/B switches themselves are never recorded
    // (feedback #10 / #11 / #12). The editor calls pollUndoCoalesce() on its timer
    // to fold a knob gesture into a single step.
    void undo();
    void redo();
    bool canUndo() const noexcept { return ! abUndo[abActive].undo.empty(); }
    bool canRedo() const noexcept { return ! abUndo[abActive].redo.empty(); }
    void pollUndoCoalesce();

    // ADR-0036, ROUND 21. THE TIMER'S POLL NEVER BLOCKS ON A WHOLE-SOUND REPLACEMENT, and that is
    // a deadlock fix rather than a performance one. `soundReplacement` is taken by a HOST thread
    // inside `applySoundTree` (an off-message-thread `setStateInformation`), which then calls
    // `apvts.replaceState` and, inside it, waits for a parameter's `listenerLock`. The message
    // thread reaches this poll from a TIMER -- and a timer runs from any loop that drains the
    // message queue, including one a host pumps from its gesture-end callback, which JUCE
    // dispatches while that same `listenerLock` is HELD. A poll that blocks there closes the
    // cycle: host thread holds the replacement lock and wants the listener lock; message thread
    // holds the listener lock and wants the replacement lock.
    //
    // TWO acquisitions are reachable from here and BOTH are handled, which is why this is not one
    // try-lock around the whole thing:
    //   * the DRAIN's (`adoptRestoreTail` -> `applySoundTree`), taken through
    //     `adoptPendingHostState (false)`. It cannot be covered by a lock held out here, because
    //     the adoption calls OUT to the host from inside itself -- a restored Oversampling
    //     delivers the reported latency synchronously, and `AudioProcessorListener`s run on this
    //     thread while it does. Holding a replacement lock across a host callback is the same
    //     inversion pointing the other way, and State test 27 (ER-STATE-14) hangs on it: measured,
    //     round 21.
    //   * the POLL BODY's (`currentStateSet`), covered by the try-lock below. That body calls out
    //     to nothing -- it reads parameters and copies the tree -- so the lock is safe to hold
    //     across it, and every acquisition inside it is then a free recursive re-entry.
    //
    // Nothing is consumed on the way out -- `pendingRestore`, `pendingGestureCommit`, the batch
    // vectors and `polledGen` are all untouched -- so the next tick does the work. The USER-ACTION
    // callers of `pollUndoCoalesce` (undo, redo, a preset save) keep the blocking acquisition they
    // have always had: they cannot be re-entered from inside a parameter dispatch, and their flush
    // must not be skipped.
    void pollUndoCoalesceFromTimer();

    // ------------------------------------------------------------------------
    //  A SCROLL IS ONE UNDO STEP (ADR-0053).
    //
    //  An undo entry holds the state from BEFORE the step it undoes, so "keep the value the whole
    //  scroll started from and replace only where it ended" is exactly "do not push another entry,
    //  and move the committed baseline on". That is the whole mechanism: a wheel edit NAMES the
    //  control it belongs to for as long as its change gesture is open, the name travels with the
    //  commit that gesture requests, and a commit whose name matches the one the most recently
    //  recorded step carries EXTENDS that step instead of pushing a second one.
    //
    //  0 means "not a wheel edit", and that is what ends a chain: a drag, a typed value, an
    //  Alt-click reset -- every other edit closes its gesture unnamed, so the next scroll starts a
    //  fresh step, which is what "switching to another modification method creates a new step"
    //  means here. A change that arrives with no gesture at all (host automation) ends it too.
    //
    //  WHY THERE IS NO INACTIVITY TIMER. The step exists from the first notch and is extended by
    //  every notch after it, so "one Undo returns the parameter to the value it had before the
    //  scroll" is true at EVERY instant rather than only after a dwell -- and nothing has to be
    //  polled, timed or held open to make it so. A held-open gesture would also be the one failure
    //  this class already knows to fear: `pollUndoCoalesceAdopted` records nothing while
    //  `openGestures > 0`, so a gesture that is never closed stops undo recording silently.
    //
    //  Message-thread state, like every other member of this section.
    void setWheelStepKey (int key) noexcept { wheelStepKey = key; }

    // ADR-0008 as amended (round 14). A STORE THE PLUG-IN'S OWN UI MAKES OUTSIDE A GESTURE OF ITS
    // OWN IS STILL THE USER'S EDIT. Nearly every write here is bracketed by a change gesture on the
    // parameter it writes, which is the declaration an undo entry needs; the exceptions are the
    // multiband display's coupled stores -- `SpectrumImager::storeOwned` and `setParam`, which push
    // neighbouring splits and shifted widths inside a gesture held on ONE parameter -- and they say
    // so by calling this instead. Without it a pushed neighbour would fall outside the very step
    // that moved it and one Undo would leave the split row half restored. Message thread only, like
    // every other member of this section; a no-op when no batch is pending.
    // ROUND 17: it takes BOTH ends. `wasNorm` is what the parameter held immediately before this
    // store -- its `before` endpoint if this store is what first takes it into the batch -- and
    // `nowNorm` is what the store installed, which is its `after`. A store is the only place that
    // knows either: it runs with nothing bracketed, so no gesture edge can read them for it.
    void noteOwnedParamWrite (const juce::AudioProcessorParameter* p,
                              float wasNorm, float nowNorm) noexcept;


    // ADR-0008, ROUND 18. THE CONTROL THAT WROTE THE PARAMETER SAYS WHAT IT WROTE, and this is the
    // narrow half of `noteOwnedParamWrite` for the controls that cannot bring a `before`. A JUCE
    // parameter attachment declares ownership when its gesture OPENS and declares no value at all,
    // so the close had nothing but a live read -- and a host write landing after the user's last
    // attachment write and before that gesture closed became the value Redo restored (State test
    // 91). The editor's per-control witness calls this with the value the control actually asked
    // the parameter to take, which is exactly what `noteOwnedParamWrite` does for the imager's
    // stores. It REFUSES to create ownership: a write the batch does not already own is not a user
    // step's endpoint, and declaring one here would hand an undo step to the gesture-less writes
    // ADR-0052 deliberately leaves alone.
    void noteOwnedParamEndpoint (const juce::AudioProcessorParameter* p, float nowNorm) noexcept;

    // ADR-0008, ROUND 19. A REFUSED STORE IS A POSITIVE FACT, AND THIS IS WHERE IT IS RECORDED.
    // `SpectrumImager::storeOwned` reads its own write back and refuses the declaration when what is
    // there is not what it installed -- the slot holds somebody else's value, so the store has
    // produced nothing the user can be said to have made. Until round 19 the refusal said that by
    // saying NOTHING, which is indistinguishable from a control that has not written yet: the close
    // fell through to its live read and the replacement became the user's `after` (State test 92
    // legs B, C and E). Recorded, the close leaves the endpoint exactly where the last thing that
    // actually stood left it -- the value `noteFirstOwnership` seeded, or the last store that stood
    // -- so a refusal costs the step the parameter rather than inventing an endpoint for it.
    //
    // It states no value, deliberately: a refused store has none to state.
    void noteOwnedParamRefused (const juce::AudioProcessorParameter* p) noexcept;

    // ADR-0008, ROUND 20. WHAT THE CONTROL ASKED FOR, KNOWN BEFORE THE GESTURE CAN BE POLLED.
    // `noteOwnedParamEndpoint` above states the endpoint AFTER the write, which is early enough for
    // a slider -- its attachment writes in one callback and closes the gesture in a later one, so
    // the endpoint is standing before the close. It is NOT early enough for a ComboBox or a Button:
    // JUCE's attachment does begin/write/end for those inside a SINGLE listener callback
    // (`setValueAsCompleteGesture`), and the editor's witness is the next listener in that same
    // pass -- so the batch becomes pollable, and its endpoint is read, while the witness is still
    // pending. A host that pumps the message loop from its gesture-end callback (it is entered
    // last, as the parameter's `finalListener`) gets a nested `pollUndoCoalesce` in that gap, and
    // the entry it commits is final: `UndoEntry` stores the value, so a later declaration cannot
    // reach it.
    //
    // So the control says what it is about to ask for BEFORE handing over to the attachment, and
    // the close prefers that over its live read. It is a REQUEST, not a declaration: it states no
    // ownership and creates no step -- a host push arms and disarms it with no gesture in between
    // and nothing consumes it. The previous request is returned so the caller can restore it,
    // which keeps one control's notification nested inside another's from stranding the outer one.
    struct AttachmentRequest { int index = -1; float norm = 0.0f; };
    AttachmentRequest noteAttachmentRequest (const juce::AudioProcessorParameter* p,
                                             float norm) noexcept;
    void restoreAttachmentRequest (AttachmentRequest prev) noexcept;
    // The name a parameter-backed control answers to. A parameter's index is stable for the life of
    // the processor and unique to it, so two controls driving the SAME parameter -- a knob and the
    // numeric box under it -- are correctly one control for this purpose. +1 keeps 0 meaning "none".
    static int wheelStepKeyFor (const juce::AudioProcessorParameter* p) noexcept
    { return p != nullptr ? p->getParameterIndex() + 1 : 0; }

    // Names a wheel edit for the duration of its change gesture and un-names it on every exit path.
    //
    // WHY THE DESTRUCTOR WRITES 0 RATHER THAN RESTORING WHAT IT FOUND (round 11 asked, and the
    // answer is that 0 IS what it found). The key is non-zero only inside one of these scopes, and
    // no call chain in this plug-in enters one from inside another. There are three construction
    // sites -- the knob's standalone scroll (`PluginEditor.h`), and the imager's split and width
    // bursts, which name the same key through `SpectrumImager`'s own `ScopedWheelName` -- and no
    // chain joins any two:
    //   * the imager's two are mutually exclusive branches of one handler, each wrapping a single
    //     `endGesture` call and reaching no other component;
    //   * `juce::Slider::mouseWheelMove` hands the event to its Pimpl, which returns true whether
    //     or not it acted, so an enabled slider with the wheel on never forwards to an ancestor;
    //     and `juce::Component::mouseWheelMove` walks UP to the nearest enabled ancestor, never
    //     down. Every `Knob` is a direct child of the editor or of the Settings backdrop -- never
    //     of another `Knob`, and never of the imager, whose only children are its own overlays.
    // So a saved-and-restored key would restore 0 at every exit this code can reach, and a
    // restoring destructor would be a mechanism with no second caller to justify it.
    //
    // THE ONE INTERLEAVING THAT IS NOT STRUCTURAL, and for a notch over a DIFFERENT control
    // restoring the key would make it WORSE rather than better: a host that pumps the OS message
    // loop from inside `beginChangeGesture` or `setValueNotifyingHost` -- both of which the knob's
    // scope spans -- could deliver a queued notch over another control inside this one. Follow it
    // through. The inner notch opens its own gesture while this one is still open, so
    // `parameterGestureChanged` counts 1 -> 2, and its close counts 2 -> 1: it latches NOTHING,
    // because the latch runs only where the count returns to ZERO. The zero-crossing close is
    // therefore the outer one (or, when a foreign gesture was already open, that gesture's own
    // release), and it reads the key the inner scope's exit has just cleared -- so the batch is
    // unnamed and the next notch starts its own step.
    //
    // TWO extra undo steps mid-scroll, not one, and the count is worth stating because both
    // clauses above cost one each: the unnamed batch cannot extend (`stepKey != 0` fails), and the
    // poll then records `lastStepWheelKey = 0`, so the notch AFTER it cannot extend either. One
    // extra step if the interruption lands on the scroll's first notch, and one if both batches
    // fall inside a single 24 Hz poll period and collapse.
    //
    // AND THE SUB-CASE THE PARAGRAPH ABOVE DOES NOT COVER: a queued notch over the SAME control.
    // There, restoring would be strictly better -- the scroll would stay one step -- and clearing
    // splits it. Neither policy dominates; clearing errs toward extra undo steps and restoring
    // errs toward swallowing another control's edit, and the rule below picks the former
    // deliberately.
    //
    // Restore the key and that same latch names the batch after the OUTER control -- a batch that
    // also contains the INNER control's edit, because the inner gesture closed inside it. The next
    // notch of the outer control would then extend a step holding somebody else's value: since
    // round 14 an entry owns the parameters the BATCH moved, and the inner control's is one of
    // them, so the other control's edit still travels with it. The
    // cleared key is not a shortcut that happens to be safe; it is the answer that keeps a batch
    // this scope cannot account for from being claimed. Recorded rather than hardened, and the
    // reasoning written out because the first version of this paragraph got it wrong -- it argued
    // from the disagreement rule, which never fires here, since the inner close never latches.
    struct ScopedWheelStep
    {
        ScopedWheelStep (AnamorphAudioProcessor& p, int key) noexcept : proc (p) { proc.setWheelStepKey (key); }
        ~ScopedWheelStep() noexcept { proc.setWheelStepKey (0); }
        ScopedWheelStep (const ScopedWheelStep&) = delete;
        ScopedWheelStep& operator= (const ScopedWheelStep&) = delete;
        AnamorphAudioProcessor& proc;
    };

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
    // `mayBlock` is false for the TIMER doors only (round 21), and it changes exactly one thing:
    // the tail's re-install of the restored sound tries `soundReplacement` instead of waiting on
    // it. The drain itself is unchanged -- it still runs to a fixed point and still applies every
    // tail. See `pollUndoCoalesceFromTimer` for the cycle that forbids the wait, and
    // `adoptRestoreTail` for the proof that the skip and the wait end in the same state.
    void adoptPendingHostState (bool mayBlock = true);

    // THE RULE FOR RELATIVE NAVIGATION (D-2 round 16, ADR-0036 §23).
    //
    // "The other slot", "the next preset", "the previous undo step" are decisions ABOUT a
    // session, and the session they are about is the one the operation observed at its drain.
    // Between deriving such a target and applying it NOTHING MAY BE ADOPTED: an adoption there
    // replaces the session the target was derived from, and the target then names a slot or a
    // row of a session that is no longer live.
    //
    // The primitives drain on ENTRY, which is right for their absolute callers ("switch to B",
    // "load row 7") and fatal for a relative one, whose target is already in its hand. So each
    // primitive is a draining shell over an ALREADY-ADOPTED core, and a relative operation
    // drains once itself and then calls the core. Nothing is ever skipped -- the drains that
    // must always run still always run -- and no window exists in which an adoption is
    // suppressed, which is what makes this safe against a host that pumps the message loop from
    // inside a parameter notification.
    void abSwitchToAdopted (int slot);        // the switch, with the drain already done
    void pollUndoCoalesceAdopted();           // the poll, with the drain already done

    // Test seams (D-2): EMPTY in production, so each costs one null check on a
    // non-audio path. A harness installs one to run code at an ownership boundary
    // -- after the host side's mailbox take inside an off-thread save, after the
    // message thread's take inside an adoption -- which is the only way to
    // reproduce a reviewed interleaving deterministically rather than by timing.
    // Installed and cleared on the main thread while no other thread can reach them.
    //
    // TWO OF THEM HAVE OPPOSITE CONTRACTS, and mixing them up produces a test that hangs or one
    // that proves nothing. `beforeSoundReplacementWrites` fires at a replacement's last instant
    // BEFORE it takes the §24 lock, so a harness may hold the replacement open there and let a
    // competing one run to completion. `insideSoundReplacement` fires part-way through the write
    // loop, WITH THE LOCK HELD: a harness may sample state or arm another thread from there, but
    // must never join or wait on a thread that itself performs a whole-sound replacement, because
    // that thread is blocked on this one.
    //
    // `insidePollBody` fires inside `pollUndoCoalesceAdopted`, after the signature has been built
    // and before `committed` is captured -- the window in which a host write is absorbed by the
    // capture while the generation the poll started from does not name it. It is the only place a
    // deterministic harness can put a write, because nothing the poll body calls re-enters a
    // parameter write, so the race is otherwise cross-thread only (State test 86 leg U).
    struct Seams { std::function<void()> afterHostSaveTake, afterRestoreTake, beforeRestorePut,
                                        afterRestoreSoundApplied, beforeSoundReplacementWrites,
                                        atRelativeDecision, insideSoundReplacement,
                                        betweenStateSetApplyAndMeta, insidePollBody; };   // ADR-0037: proves no live read
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
    // THE TOGGLE, as its own operation (D-2 round 10, ADR-0036 §18). "Go to the other
    // slot" is a decision about the session the plug-in is ON, and only the processor
    // knows that at the moment the action commits: a pending host restore is adopted at
    // the top of every state-mutating entry point, and it can move the active slot. The
    // editor used to compute the destination itself -- `abSwitchTo (abActiveSlot() == 0
    // ? 1 : 0)` -- from a read taken BEFORE that adoption, so with a restore pending that
    // flipped the active slot the computed target was the slot the restore had just made
    // active and the switch was a no-op. The destination is now derived here, after the
    // drain, from the authoritative slot. abSwitchTo(int) remains the primitive for an
    // EXPLICIT target ("switch to B"), which is intent rather than a stale derivation.
    void abToggle();
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
    void resetBatchOwnership();
    // ROUND 17. THE ONE PLACE A `before` ENDPOINT IS WRITTEN, and it writes each one exactly once:
    // the instant the pending batch first takes that parameter. A second declaration of an
    // already-owned parameter is a no-op, which is what makes the endpoint stable against every
    // later gesture, snapshot and automation write in the same batch.
    void noteFirstOwnership (int index, float beforeNorm) noexcept;

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

    // ------------------------------------------------------------------------
    //  ADR-0008 AS AMENDED (round 14, approved). WHAT AN UNDO ENTRY IS.
    //
    //  An entry used to be a whole `StateSet` snapshot, and that is what made a host automation
    //  value part of a user's step: the snapshot behind the user's edit predates every write that
    //  landed in the commit window, so one Undo took the automation back with the edit -- and the
    //  REDO destination was worse, because it was manufactured from the LIVE parameters at the
    //  moment Undo was pressed, so any automation between the step and the Undo silently became
    //  the value the user's Redo restored (RISK-012, R983).
    //
    //  An entry now records WHAT THE USER'S OWN BATCH MOVED, and both ends of it. `owned` carries
    //  one `ParamEdit` per parameter the batch declared as its own -- a parameter with a change
    //  gesture of its own, or one the imager's coupled stores declared through
    //  `noteOwnedParamWrite` -- with the value it held when the batch FIRST TOOK it and the latest
    //  value its own gesture or store produced (PER PARAMETER SINCE ROUND 17). Undo writes the
    //  `before` ends, Redo writes the `after` ends, and the SAME entry moves between the two
    //  stacks, so the two directions cannot disagree. Everything else the live sound holds is left
    //  exactly where it is, which is what makes a later host write survive both.
    //
    //  `before.params` VALID MEANS A WHOLE-STATE ENTRY, and two push sites still make them: a
    //  preset load (`commitPresetSwitchUndoStep`) and an A/B Copy (`abCopyToOther`). Neither opens
    //  a gesture, both wholesale-replace a sound, and there is no per-parameter attribution to be
    //  had for either; they keep exactly the semantics they have always had. Undo history is never
    //  serialized, so none of this reaches the Serialization Registry.
    struct ParamEdit { int index = 0; float before = 0.0f, after = 0.0f; };  // normalised (raw) values
    struct UndoEntry
    {
        std::vector<ParamEdit> owned;   // what the user's batch moved; empty on a whole-state entry
        StateSet before, after;         // `params` valid ONLY when whole; the preset metadata always
        bool isWhole() const noexcept { return before.isValid(); }
    };
    struct UndoStacks { std::vector<UndoEntry> undo, redo; };
    UndoStacks abUndo[anamorph::kNumAbSlots];
    // ADR-0008 as amended (round 14). Install one end of an undo entry. A whole-state entry is
    // applied exactly as it always was. A scoped one is applied by handing `applyStateSet` the LIVE
    // state with only the entry's own parameters overwritten -- deliberately, rather than by writing
    // those parameters directly, so the §24 replacement lock, the view-param preservation,
    // `reassertParameters`' exact-value assert, `seams.beforeSoundReplacementWrites` and
    // `noteWholeSoundReplaced()` all still happen on the undo path (State test 49's `undoStep` leg
    // is the one that would stop measuring its interleaving if they did not).
    void applyUndoEntry (const UndoEntry& e, bool toAfter);
    // ADR-0008's history bound, in the ONE place that enforces it. The ADR's Consequences call it
    // "a hand-rolled history with a 128-entry cap per slot", and until round 16 that cap lived as
    // two hand-copied `push_back` / `size() > 128` / `erase (begin())` triples -- the poll's step
    // push and the preset-switch push -- while `abCopyToOther` had neither, so repeated A/B Copies
    // grew a slot's history without limit, and each of those entries is the expensive kind: two
    // whole `ValueTree`s rather than a handful of `{index, before, after}` triples. `undo()` and
    // `redo()` MOVE an entry between the two stacks rather than growing either, so they need no
    // cap and do not call this: the undo stack they push to has just had an entry popped from it.
    static constexpr size_t kUndoDepth = 128;
    static void pushCapped (std::vector<UndoEntry>& stack, UndoEntry&& e);
    StateSet committed;
    // ROUND 21 (ADR-0036 §26). SET WHEN A TIMER'S ADOPTION COULD NOT TAKE THE BASELINE, and
    // cleared by the first door that can. `syncCommitted` copies the parameter tree under
    // `soundReplacement`; a timer may not wait for that lock (see `pollUndoCoalesceFromTimer`),
    // so when a host thread is mid-replacement the snapshot is skipped and this says so. Every
    // path that can PUSH an undo entry runs through `pollUndoCoalesceAdopted`, which repairs it
    // at its first line, so no entry is ever built on a `committed` this flag still names.
    bool committedNeedsResync = false;
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
    // replacement (a restore's install runs on the host's), read on the message thread;
    // relaxed, because every bump and every deciding read happen under the whole-sound
    // replacement lock (§24), which provides the ordering -- and the value that matters
    // travels inside the RestoreDecode, whose cell orders it too.
    std::atomic<juce::uint32> soundSetGen { 1 };
    // Allocates the token for ONE replacement and returns it. Two rules make the token
    // mean what the adoption needs it to mean (ADR-0036 §13, §14):
    //
    //  * IDENTITY. `fetch_add` hands each caller a value no other caller can be handed,
    //    so an operation that keeps its own return value holds an identity rather than a
    //    reading of shared state. Reading the counter back after a replacement instead
    //    returns whatever the LAST replacement was -- another operation's token whenever
    //    one overlapped (the round-6 defect).
    //  * COMPLETION. Every caller allocates AFTER its last sound write, never before, so
    //    the counter orders replacements by when they finished rather than by when they
    //    started. Since each wholesale replacement writes every sound parameter, the one
    //    that finished last is the one the live sound belongs to; allocating at the start
    //    ordered them by begin time, which is a different order (the round-7 defect).
    //
    // Callers that need to prove the live sound is still theirs bracket their writes with
    // `soundReplacementToken` rather than calling this directly.
    juce::uint32 noteWholeSoundReplaced() noexcept
    {
        return soundSetGen.fetch_add (1, std::memory_order_relaxed) + 1;
    }

    // The completion token for a replacement whose writes began when the counter read
    // `begin`, or 0 when another replacement ran inside ours. `begin` is sampled before
    // the first write and the token allocated after the last, so the pair BRACKETS this
    // replacement: exactly one bump in between (`token == begin + 1`) is proof that no
    // other wholesale replacement began-and-finished while ours was in flight, and so
    // that ours is the one the live sound belongs to. Anything else means the two
    // interleaved -- their per-parameter writes are not mutually excluded, so the live
    // sound may hold values from both -- and 0 is returned to say "no owner provable".
    // The counter starts at 1 and only rises, so 0 is never a real token, and a decode
    // holding it can never compare equal: the adoption re-installs, which is the
    // conservative answer that restores one coherent session (ADR-0036 §14).
    juce::uint32 soundReplacementToken (juce::uint32 begin) noexcept
    {
        const auto token = noteWholeSoundReplaced();
        return token == begin + 1 ? token : 0;
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
    // ADR-0053, the three halves of "a scroll is one undo step". `wheelStepKey` is the control a
    // wheel edit currently in flight names; `pendingStepWheelKey` is that name LATCHED at the
    // instant the gesture closed, because the poll that acts on it runs up to a timer period later,
    // by which time the edit has long un-named itself and another may be in flight;
    // `lastStepWheelKey` is the name the most recently RECORDED undo step carries, and comparing the
    // two is the whole extend-or-push decision. 0 everywhere means "not a wheel edit".
    int  wheelStepKey = 0;
    int  pendingStepWheelKey = 0;
    int  lastStepWheelKey = 0;
    // Whether any gesture in the batch now pending has CONTRIBUTED that name -- only a gesture that
    // actually changed a sound parameter does, which is what makes an empty press transparent to a
    // scroll instead of ending it (ADR-0053). With `gestureOpenGen`, the sound generation sampled
    // when the batch's first gesture opened, it is a two-word comparison per gesture rather than a
    // signature rebuild: `soundParamGen` is already bumped by every value change.
    bool pendingStepNamed = false;
    juce::uint32 gestureOpenGen = 0;
    // ...and the sound generation as of the last GESTURE EDGE -- the open of a batch, the close of
    // a batch, or a poll. Anything that moves a sound parameter between two edges moved it outside
    // every gesture, which is host automation by construction: it is the only writer that opens
    // none. Both windows matter and they are different windows: the poll runs up to a timer period
    // after the close, and the next batch can open a whole gesture before the poll ever runs. A
    // step that carries a foreign write is nobody's scroll -- it neither extends the scroll before
    // it nor lets the next notch extend it (ADR-0053, round 11).
    juce::uint32 gestureEdgeGen = 0;
    // ...latched at the OPEN side, because by the time the poll reads the counter the batch's own
    // writes have moved it past the evidence. Message thread only, like everything else here.
    bool foreignSinceEdge = false;

    // ADR-0008 as amended (round 14), IMPLEMENTED PER PARAMETER SINCE ROUND 17. The parameters the
    // pending batch owns, and for each of them the value it held when the batch first took it and
    // the latest value the user's own action produced for it. `batchOwnedParam[i]` is set where the
    // batch declared parameter i its own -- a change gesture opened on it, or `noteOwnedParamWrite`
    // said so.
    //
    // THESE ARE NOT SNAPSHOTS ANY MORE, and that is the whole of the round-17 correction. They were
    // whole-parameter-list reads taken when the batch opened and re-taken at EVERY zero-crossing
    // close, which made three things wrong at once, all measured (State test 90):
    //   * a second gesture's close re-read the WHOLE list, so a host write that landed between two
    //     gestures of one batch replaced the first gesture's `after` -- Redo then restored the
    //     automation value as though the user had produced it, and the value the user actually
    //     produced was not recoverable from either end;
    //   * `before` came from the batch's open rather than from the parameter's own first
    //     ownership, so automation that moved a parameter BEFORE the user first touched it became
    //     the value Undo restored;
    //   * an empty press -- a click that starts no drag -- declared a parameter and the close
    //     handed it whatever the host had written, turning pure automation into a user Undo step.
    // Per parameter, `before` is written once at first ownership and `after` only from a value the
    // owning gesture or store actually produced, so no later read of anything can redefine either.
    //
    // ROUND 18 completes the sentence above for the controls that declare NOTHING. A parameter
    // written through a JUCE attachment used to reach the close with only bit 0 set, so the close
    // live-read it -- and a host write landing after the user's last attachment write and before
    // that gesture closed became the recorded `after`. The editor's `AttachmentWitness` now states
    // the value the control asked for, through `noteOwnedParamEndpoint`, which is the same bit-1
    // declaration a declaring store makes. State test 91.
    //
    // Sized ONCE in the constructor and never resized, so a gesture callback allocates nothing.
    // SIX functions touch them, and this list is exhaustive because a fix that adds per-parameter
    // state and misses `resetBatchOwnership` would leak it across a program-state jump: the
    // constructor (sizing, on the host's construction thread, before anything can observe the
    // object), `parameterGestureChanged`, `noteFirstOwnership`, `noteOwnedParamWrite`,
    // `noteOwnedParamEndpoint`, `resetBatchOwnership` and `pollUndoCoalesceAdopted`. Every one of
    // those but the constructor is message-thread, which is why this is NOT the
    // `parameterValueChanged` design RISK-012 flagged as a new cross-thread path.
    std::vector<float> batchOpenValue, batchCloseValue;
    std::vector<char>  batchOwnedParam;
    // ...and which parameters the CURRENT gesture episode is about. Bit 0 is "a gesture opened on
    // it since the last zero-crossing close"; bit 1 is "a store declared its endpoint in this
    // episode, so the close must not second-guess it with a live read"; bit 2 (round 19) is "a
    // store on it was REFUSED this episode, so the live value is known not to be the user's and
    // the endpoint must stay where the last thing that stood left it". Cleared at every
    // zero-crossing close and whenever the batch is re-based. This is what keeps a closing gesture
    // from retaking an endpoint that belongs to an earlier gesture of the same batch.
    std::vector<char>  batchEpisodeParam;
    // ROUND 20: the one piece of endpoint bookkeeping that is NOT per-batch, and must not be --
    // it is armed before the gesture opens, and opening a fresh batch re-bases every vector above.
    // At most one control notification is in flight at a time; nesting is handled by save/restore
    // at the witness rather than by a stack here.
    AttachmentRequest  attachRequest;

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
    //            restore. H publishes it after announcing its generation in the
    //            engine-config word and THEN installing the sound (APVTS), both
    //            synchronously and in that order (§25); M adopts it in
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
        // Per Settings field, the generation of the latest restore that had ARRIVED when the
        // message thread last edited it (D-2 round 12, ADR-0036 §21). Published WITH the tree
        // it describes, in the same immutable object, so a host thread reading both reads one
        // consistent pair. It is what lets a save inside the pending window apply §9's
        // per-field precedence -- the object-wide `generation` above answers only the
        // whole-session question, and a Settings edit can never move it.
        anamorph::InternalState::EditGenerations settingsEditGen {};
        int abActive = 0;
        StateSet abSlot[anamorph::kNumAbSlots];
    };

    // What a restore DECODES from the blob before anything is applied: the sound tree
    // to install and the exact inputs of the adoption tail, thread-neutral. Built on the
    // caller's thread; the sound installed there by `installRestoredSound`; the rest
    // adopted on M (inline when the caller IS M, else through the cell).
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

        // THIS RESTORE'S OWN CLEAN BASELINE (D-2 round 15, ADR-0036 §22), for the sessions
        // that carry no `presetBaseline` of their own: the signature the parameters will
        // report once `soundParams` has been installed, derived from that tree ALONE by
        // `soundSignatureAfterLoading` -- the primitive round 10 built for the preset-load
        // baseline (§18, KI-029). Decided at DECODE time, by the thread that decoded it, so
        // there is no live read and therefore no window an edit or an automation write can
        // land in. It is what both the adoption and `viewOfRestore` resolve an absent or
        // empty stored baseline to, through one shared helper, so the prediction and the
        // adoption cannot disagree.
        juce::String    restoredSoundSig;
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
    // AND NOTHING ON THIS SIDE CALLS THEM EITHER. The other half of the question is
    // whether the plug-in can re-enter its own state functions concurrently: it cannot,
    // because it never calls them at all. `getStateInformation` / `setStateInformation`
    // appear in this repository only as these definitions -- no timer, no editor action,
    // no preset path, no engine callback invokes either one -- so every activation comes
    // from a host entry point, and JUCE itself adds none (no wrapper timer, async
    // callback or background thread reaches them on any format built here).
    //
    // So the support boundary, stated rather than implied: concurrent host state calls
    // are OUTSIDE supported operation, Anamorph assumes nothing stronger than JUCE
    // itself, and rather than paying for a broken host on every save the two off-thread
    // branches count themselves in and a debug build asserts if a second one ever
    // overlaps. The assertion is diagnostic only -- nothing reads the counter in a
    // release build, and it neither changes state nor imposes ordering -- so it is not
    // the synchronisation mechanism and is not standing in for one. Never blocks, never
    // affects the result, and same-thread nesting cannot occur (no state call re-enters
    // another off the message thread).
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
    // ONE WHOLE-SOUND REPLACEMENT AT A TIME (D-2 round 17, ADR-0036 §24).
    //
    // A replacement is `apvts.replaceState` (locked by JUCE) followed by a LOOP of
    // per-parameter writes (`reassertParameters`, or PresetManager's own loop) that runs
    // outside that lock -- so it is not atomic, and two of them running at once leave the live
    // parameter set holding values from BOTH. `soundReplacementToken` has always DETECTED that
    // ("no owner provable", token 0) and the adoption repairs it (§14), but the mixed sound is
    // live until the repair runs, which is up to one 20 Hz timer period later: long enough for
    // the engine to play it and for an off-message-thread save to write it into a session.
    //
    // Only one replacement can ever be off the message thread -- the sound half of a host
    // thread's decode -- and every other replacement (adoption re-install, A/B apply, undo,
    // redo, preset load) is message-thread work, so this lock is uncontended in ordinary
    // operation and the pairs it excludes are exactly {a decode's install} x {anything M does}.
    // The AUDIO THREAD NEVER TAKES IT: `processBlock` reads parameter atomics and is untouched,
    // so no realtime path can block. It is recursive (juce::CriticalSection), which lets the
    // adoption hold it across its guard check AND the re-install so the decision cannot go
    // stale between the two.
    juce::CriticalSection soundReplacement;

    // The APVTS root type, captured once at construction so no thread reads the live
    // `apvts.state` handle to learn it (JUCE guards the tree's contents with its own
    // lock; the handle itself is assigned under that lock by replaceState).
    juce::Identifier apvtsStateType;

    // True on the message thread, and when no MessageManager exists at all (a
    // harness): the one predicate that decides "inline" versus "hand off", shared
    // by the latency request (D-1) and the program-state handoff (D-2).
    static bool onMessageThreadOrNoMessageManager() noexcept;

    // Decode a blob into a RestoreDecode. PURE since round 18 (§25): it touches no
    // parameter -- the sound half is `installRestoredSound`, run by the caller once the
    // restore has announced itself, and it is still synchronous on the caller's thread
    // for the ordinary setState-then-prepareToPlay order. Returns false for input that
    // is not a restore (nothing was touched).
    // The sound half of a restore as its own step: installs `d.soundParams` and records the
    // token that install was handed (§13). Called by the message thread right after the
    // decode, and by a host thread only AFTER it has announced its generation (§25).
    void installRestoredSound (RestoreDecode& d);
    bool decodeRestore (const void* data, int sizeInBytes, RestoreDecode& out);
    // The adoption tail, message thread only: today's restore tail, verbatim.
    void adoptRestoreTail (const RestoreDecode&, bool mayBlock = true);
    // `mayBlock` as above, and it reaches exactly one line: the baseline snapshot, which
    // copies the parameter tree under `soundReplacement`. A timer that may not wait leaves
    // `committed` alone and raises `committedNeedsResync`; every door into the poll body
    // repairs it before that body can push anything.
    void syncCommitted (bool mayBlock = true);
    // Serialize a program snapshot plus the live parameters. Any thread: the APVTS
    // copy is JUCE-locked and the snapshot is immutable.
    // `settings` is the Settings tree to write, passed separately because a save inside the
    // pending window writes the restore's program with the message thread's post-arrival
    // edits overlaid, which is neither snapshot's own tree (ADR-0036 §21).
    void writeState (const ProgramSnapshot&, const juce::ValueTree& settings, juce::MemoryBlock& destData);
    // The message thread's own program state as a snapshot value (M only).
    ProgramSnapshot ownedProgram() const;
    // Republish `ownedProgram()` into the mailbox (M only; skipped inside an adoption).
    void publishProgram();
    // The H-side view of a restore H just decoded: what M will own once it adopts it.
    // The one resolver both the prediction and the adoption use for a restore's clean
    // baseline (ADR-0036 §22): the session's own `presetBaseline` when it recorded a
    // non-empty one, and otherwise the sound THIS restore installed, from its own bytes.
    static juce::String baselineOfRestore (const RestoreDecode&);
    static std::unique_ptr<const ProgramSnapshot> viewOfRestore (const RestoreDecode&);
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
