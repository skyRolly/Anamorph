#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "AbSlotIndex.h"
#include "SerializedNumber.h"   // the shared malformed-value predicate (both restore paths)

#include <cmath>   // std::isfinite -- the non-finite guards on the restore paths

AnamorphAudioProcessor::AnamorphAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ANAMORPH", createAnamorphLayout()) // custom undo, not APVTS's
{
    params.bind (apvts);
    // The APVTS root type ("ANAMORPH"), read ONCE here, single-threaded. Every later
    // reader -- the restore decode, which may run on a host thread -- uses this copy
    // rather than the live `apvts.state` handle, which replaceState reassigns under
    // JUCE's own lock and which nothing outside that lock may read (D-2, ADR-0036).
    apvtsStateType = apvts.state.getType();
    // Bypass is a custom RangedAudioParameter subclass (raw-value round-trip), so it is no longer
    // an AudioParameterBool -- take the base pointer directly for getBypassParameter().
    bypassParam = apvts.getParameter (pid::bypass);

    // Parameters that used to change the reported PDC latency. Since ADR-0034 the
    // reported number is a function of the Oversampling SELECTION alone, so neither
    // of these can move it any more; they are kept as the defensive re-derivation
    // path -- `deliverLatency()` recomputes from the live state and JUCE's
    // setLatencySamples notifies the host only when the value actually differs, so
    // a Drive or Algorithm move now costs one predicate and returns. Removing them
    // would be a further (real) simplification and is deliberately NOT part of the
    // latency change. Oversampling is not an APVTS parameter (it lives in
    // InternalState), so its PDC update is driven by a callback instead.
    apvts.addParameterListener (pid::drive,      this);
    apvts.addParameterListener (pid::algorithm,  this);
    // Same route as parameterChanged, for the same reason: this fires from
    // InternalState property changes, and one of those paths is
    // setStateInformation, which a macOS AU host may call off the main thread
    // (RISK-007). On the message thread it stays synchronous.
    internal.onOversampleChanged = [this] { requestLatencyUpdate(); };

    // Observe begin/end GESTURES on the SOUND params so a whole drag folds into ONE undo step and
    // host automation (which never opens a gesture) is excluded from undo. View params are skipped.
    for (auto* p : getParameters())
        if (auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
        {
            if (! pid::isViewParam (wid->paramID))
                p->addListener (this);
            else
                p->addListener (&viewGenWatcher); // H15 re-arm only; no gesture/undo effect
        }

    // A preset load opens NO gesture, so the gesture-gated coalescer would fold it into the baseline
    // without an undo step (host-automation path). Bracket every load: flush any settled edit first,
    // then record exactly ONE undo step for the switch, so a preset change is undoable (ADR-0008).
    // ...and RAISE THE MASKING DUCK HERE, not at the call site (ER-GUI-06). This hook is the
    // load path's own "it is definitely happening" boundary: PresetManager fires it after every
    // check that can refuse -- the missing factory id, the unparsable file, and since ER-STATE-24
    // the foreign root -- and BEFORE the first parameter moves. Both properties matter. The
    // editor used to call requestDuck() before asking the manager to load, so a load the manager
    // then REFUSED still left a request pending, and the next audio block dry-filled ~32 ms to
    // mask a swap that never happened (measured: an engaged widener's side energy at 0.4549 of
    // the control's, State test 35) -- the same "duck whose swap already happened" fault
    // AnamorphEngine::primeParameters documents for the activation route. Moving the request in
    // here rather than adding a success flag to the callers fixes all three call sites at once
    // (the menu, Load Preset..., and the prev/next buttons via step()), cannot drift between the
    // two loaders, and keeps the ordering the duck depends on: still raised before any
    // setValueNotifyingHost, so the swap is still heard only at the silent bottom.
    // A preset load installs another session's sound one parameter at a time
    // (PresetManager::applySoundTree) rather than through replaceState, so its
    // whole-sound-replacement bump is made here rather than at a replacement site -- it
    // is a wholesale replacement like the A/B and undo paths, and a restore adopted
    // after it must re-install its own sound rather than wear the preset's (D-2 §12).
    // It belongs in onLoaded, not onAboutToLoad: the bump marks COMPLETION of the sound
    // writes, the same point every other replacement bumps at (§14), and the two hooks
    // are paired on every path that can succeed (see the failure discipline in
    // PresetManager::load).
    // NO DRAIN HERE since round 16 (§23): `load`/`loadFile` drain at their own top, before the
    // refusal checks and before `step` derives its row, so a drain at this point could only
    // adopt something AFTER a relative target had been chosen. The flush and the duck stay --
    // the duck deliberately after every check that can refuse (round 26).
    presets.onAboutToLoad = [this] { pollUndoCoalesceAdopted(); engine.requestDuck(); };
    // The completion bump used to live here, AFTER the load's write loop had released the §24
    // lock and after the signature and setMeta work. A host thread released from the lock into
    // that gap sampled a `begin` the preset load had already invalidated, read a CLEAN token,
    // and the adoption then re-installed its restore over the preset. It is now published from
    // inside the write loop's own scope (`presets.noteReplaced`), where the two processor sites
    // have always published theirs.
    presets.onLoaded      = [this] { commitPresetSwitchUndoStep(); };
    // A save changes no parameter, so nothing else would ever refresh `committed` off the
    // pre-save preset -- and the next undo would restore that stale name/identity/baseline.
    // Flush FIRST, exactly like the other program-state jumps (onAboutToLoad above, and
    // undo()/redo()): syncCommitted() clears pendingGestureCommit, so a knob gesture that
    // closed but has not been polled yet would otherwise be folded into the new baseline
    // with NO undo step -- the edit silently stops being undoable. Flushing here is safe
    // even though it runs AFTER the save's own mutations, because a save touches no
    // parameter and leaves `committed` alone: the step it records is still the exact
    // pre-edit state set, with the pre-save preset metadata, which is what undo wants.
    presets.onSaved       = [this] { pollUndoCoalesce(); syncCommitted(); };
    presets.soundParamGeneration = [this] { return soundParamGen.load (std::memory_order_relaxed); }; // S10

    // D-2 (RISK-007, ADR-0036): the program-state publication. Every mutation of the
    // metadata this processor owns republishes the immutable snapshot an
    // off-message-thread save reads -- the preset manager and InternalState report
    // theirs through these hooks, the A/B paths publish directly -- and a save that
    // is about to rewrite the preset metadata adopts a pending host restore first, so
    // the two land in the order they happened.
    presets.onMetaChanged = [this] { publishProgram(); };
    presets.onAboutToSave = [this] { adoptPendingHostState(); };
    presets.adoptPending  = [this] { adoptPendingHostState(); }; // load/loadFile/step drain through this (§18, §23)
    presets.soundReplacementLock = &soundReplacement;   // a preset load is a whole-sound replacement too (§24)
    presets.insideReplacement     = [this] { if (seams.insideSoundReplacement) seams.insideSoundReplacement(); };
    presets.noteReplaced          = [this] { noteWholeSoundReplaced(); };   // published under the §24 lock
    internal.onChanged    = [this] { publishProgram(); };

    syncCommitted(); // establish the undo baseline

    // Snapshot BOTH A/B slots to the open (Default) state up front. The slots are otherwise filled
    // LAZILY on the first A/B switch (abEnsureInit): editing A before ever visiting B would then make
    // B born as a copy of A's ALREADY-edited state -- the edit leaks into B, so B never shows the
    // open state. Eager init makes the two slots independent from open, deterministically (the lazy
    // path made "B == open state" depend on whether the host called getStateInformation early). The
    // switch/apply logic is unchanged; this only fixes WHEN the initial snapshot is taken.
    abEnsureInit();

    // The first published snapshot, so an off-message-thread save issued before any
    // message-thread mutation still has a program to describe (D-2).
    publishProgram();

    // D-1: the consumer for deferred latency requests. Owned by the PROCESSOR so
    // it runs with no editor open -- the reason the editor-polling candidate was
    // refuted. Guarded because a harness may construct the processor with no
    // MessageManager; there, requestLatencyUpdate() delivers synchronously instead,
    // because with no timer a stored request would never be served.
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr)
        startTimerHz (20);
}

AnamorphAudioProcessor::~AnamorphAudioProcessor()
{
    stopTimer(); // D-1 and D-2: before any member the callback touches -- or adopts -- goes away
    apvts.removeParameterListener (pid::drive,      this);
    apvts.removeParameterListener (pid::algorithm,  this);
    // Symmetric with the constructor: every parameter that got a listener there loses
    // it here, on the SAME view/non-view split. viewGenWatcher (H15) is a member, so
    // the parameters -- owned by the AudioProcessor base subobject, destroyed AFTER
    // this derived object's members -- would otherwise outlive it holding a dangling
    // listener pointer. Removing here, in the destructor BODY (before any member/base
    // teardown), makes that impossible regardless of member declaration order.
    for (auto* p : getParameters())
        if (auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
        {
            if (! pid::isViewParam (wid->paramID))
                p->removeListener (this);
            else
                p->removeListener (&viewGenWatcher);
        }
}

// ----------------------------------------------------------------------------
//  I/O: output is ALWAYS stereo; accept stereo->stereo (default) OR mono->stereo
//  (the headline "turn Mono into Stereo" layout the host instantiates on mono
//  tracks). Mono->mono is intentionally NOT supported.
// ----------------------------------------------------------------------------
bool AnamorphAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in  = layouts.getMainInputChannelSet();

    if (out != juce::AudioChannelSet::stereo())
        return false;

    return in == juce::AudioChannelSet::stereo()
        || in == juce::AudioChannelSet::mono();
}

void AnamorphAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // ORDER IS LOAD-BEARING. engine.prepare() settles the whole engine from the
    // engine's OWN snapshot, so that snapshot must be current before it runs --
    // priming first is what makes a session the host restored BEFORE activation
    // (the ordinary VST3/AU order: setState, then setActive/prepareToPlay) come
    // up correct from the first sample instead of ramping to it. The trailing
    // setParameters is the ordinary steady-state entry and now finds nothing to
    // do -- its bitwise no-change gate skips it -- but it stays as the single
    // path by which live parameters ever reach the engine.
    const auto e = params.toEngine (internal.oversampleIndex());
    engine.primeParameters (e);
    engine.prepare (sampleRate, samplesPerBlock);
    engine.setParameters (e);

    // Through D-1's request path, NOT updateLatency() directly (round 15,
    // ER-STATE-19). On the message thread -- every in-spec VST3 activation, the
    // standalone, pluginval -- this is the same synchronous delivery as before.
    // But a host may prepare on some other thread: JUCE's own Linux VST3 wrapper
    // services the plug-in's messages (this timer included) from a background
    // thread until the host registers an IRunLoop, and never hands over if it
    // never does; FL Studio's Patcher is known to ignore setActive's [UI-thread]
    // annotation; nothing pins an AU Initialize to main. A direct call from
    // such a thread wrote AudioProcessor::latencySamples and walked the listener
    // chain concurrently with timerCallback() doing the same on the message
    // thread, and let that tick read latency2/4/8 while engine.prepare() above
    // was rewriting them -- two data races ThreadSanitizer reports on the
    // pre-round-15 code (--reprepare-race-probe), with a reachable ending in
    // which the timer's older number lands last and nothing is pending to
    // correct it. Requesting instead makes the message thread the ONLY writer,
    // and the release store below orders this prepare's latencies before the
    // tick that reports them. State test 30.
    requestLatencyUpdate();
}

void AnamorphAudioProcessor::deliverLatency()
{
    // Message thread only, by construction: every caller either IS the message
    // thread or reached here through requestLatencyUpdate()/timerCallback().
    //
    // Deliberately does NOT touch latencyUpdateRequest. The flag must be cleared
    // exactly once per delivery, and BEFORE the state this reads is read -- so a
    // request that lands while setLatencySamples is running (which is the slow
    // part: three CriticalSections, and on a real change a heap append and a pipe
    // write) stays set and is served by the next tick. A second clear after the
    // first would swallow exactly those requests.
    setLatencySamples (engine.predictLatency (params.toEngine (internal.oversampleIndex())));
}

void AnamorphAudioProcessor::updateLatency()
{
    // Clear FIRST, then deliver: this ordering is what makes a concurrent request
    // survive rather than be lost. A message-thread prepareToPlay reaches here
    // through requestLatencyUpdate(), and the clear is right for it too -- a full
    // re-prepare supersedes any pending request.
    // ACQUIRE for the same reason timerCallback uses it: consuming a request must
    // also make the parameter write that raised it visible.
    latencyUpdateRequest.exchange (0, std::memory_order_acquire);
    deliverLatency();
}

// D-1 (KI-027) -- approved 2026-09-01, implemented here.
//
// `predictLatency` is const and race-free, so COMPUTING the number was never the
// problem. DELIVERING it was: setLatencySamples' notification chain takes at
// least three CriticalSections and, when the reported value actually changes,
// appends to a heap container and write()s a pipe in the Linux wrapper. Under
// VST3 host automation of drive/algorithm the caller is the AUDIO thread, so that
// is a lock, an allocation and a syscall in a realtime context -- with a
// priority-inversion variant when the host's restartComponent is synchronous.
//
// Two candidate fixes were refuted in round 2 and must not come back: polling
// from the editor does not exist when the editor is closed, and an AsyncUpdater
// reproduces the same message-posting syscall from the audio thread.
//
// What survives is a request flag plus a timer the PROCESSOR owns, so it runs
// whether or not an editor exists. On the message thread nothing is deferred at
// all, which keeps every UI edit, preset load and undo instantaneous; off it, the
// audio thread does one relaxed atomic store and returns.
bool AnamorphAudioProcessor::onMessageThreadOrNoMessageManager() noexcept
{
    // The message thread -- and, when no MessageManager exists at all (a harness;
    // see the constructor's timer guard), whichever thread is running, since then
    // there is no timer to serve anything deferred and a stored request or restore
    // would never be delivered. Shared by D-1 (the latency request) and D-2 (the
    // program-state handoff) so the two cannot answer the question differently.
    return juce::MessageManager::existsAndIsCurrentThread()
        || juce::MessageManager::getInstanceWithoutCreating() == nullptr;
}

void AnamorphAudioProcessor::requestLatencyUpdate()
{
    // Synchronous on the message thread (and in a harness with no MessageManager).
    if (onMessageThreadOrNoMessageManager())
        updateLatency();
    else
        // RELEASE, not relaxed. The flag is not the payload -- the payload is the
        // parameter (and oversampling) write that happened before this call, which
        // deliverLatency() reads on the message thread. Under relaxed ordering there
        // is no happens-before edge between those two writes, so a consumer could
        // legitimately observe the flag WITHOUT observing the value that raised it,
        // deliver the OLD latency, and clear the request -- leaving the host
        // permanently stale with nothing pending to correct it. Measured: with
        // relaxed/relaxed, a 400-move stress loop left reported 0 against a state
        // predicting 4 in roughly half of runs, even with the double-clear closed.
        // On x86-64 and AArch64 a release store is the same instruction as a relaxed
        // one plus a compiler barrier, so the audio thread still pays nothing.
        latencyUpdateRequest.store (1, std::memory_order_release);
}

void AnamorphAudioProcessor::timerCallback()
{
    // D-2 first: a restore handed over from a host thread is adopted here, on the
    // message thread, whether or not an editor exists -- this timer is the one
    // consumer that runs with the editor closed. The adoption may itself deliver a
    // latency synchronously (a changed Oversampling fires onOversampleChanged on
    // this thread), which clears the flag below; the exchange then finds nothing,
    // so nothing is delivered twice.
    adoptPendingHostState();

    // exchange() IS the clear for this delivery, so call deliverLatency() rather
    // than updateLatency() -- the latter would clear a SECOND time, and anything
    // stored in the window between the exchange above and that second clear would
    // be silently dropped. The audio thread's store is a bare relaxed write with
    // no acknowledgement, so a dropped request is a permanently stale reported
    // latency: nothing re-raises it until the next unrelated parameter move or a
    // re-prepare. Requests landing during deliverLatency() below are served by the
    // next tick, which is the whole point of clearing before reading.
    if (latencyUpdateRequest.exchange (0, std::memory_order_acquire) != 0)
        deliverLatency();
}

void AnamorphAudioProcessor::parameterChanged (const juce::String&, float)
{
    // Drive / Algorithm used to move the reported PDC; since ADR-0034 they cannot,
    // so this is a re-derivation that finds the same number and delivers nothing.
    // It is the call that used to run setLatencySamples on whatever thread moved
    // the parameter -- see requestLatencyUpdate for why that mattered and what
    // replaced it. Kept: it is the single re-derivation point, and its cost is now
    // one predicate on a value that no longer changes.
    requestLatencyUpdate();
}

void AnamorphAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();

    // Clear any output channels with no corresponding input.
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // mono -> stereo: duplicate the mono input into the second channel so the
    // widening engine always sees a stereo source to turn into stereo.
    if (getMainBusNumInputChannels() == 1 && buffer.getNumChannels() >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());

    if (buffer.getNumChannels() < 2)
        return; // safety: engine requires a stereo working buffer

    // Reset the meter peak-hold / clip latches when transport (re)starts (#15) OR when
    // the playback position is repositioned mid-flight -- a seek / timeline click / loop
    // wrap -- so the held numbers track the new location, matching a play restart (Issue 3).
    bool playing = false;
    bool seeked  = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();

            // Position in samples: prefer the host's sample clock, else derive it from
            // the musical (ppq) position so seek detection works on either kind of host.
            std::optional<juce::int64> nowOpt;
            if (auto t = pos->getTimeInSamples())
                nowOpt = *t;
            else if (auto ppq = pos->getPpqPosition())
            {
                const double bpm = pos->getBpm().orFallback (120.0);
                nowOpt = (juce::int64) (*ppq * 60.0 / juce::jmax (1.0, bpm) * getSampleRate());
            }

            if (nowOpt)
            {
                const juce::int64 now = *nowOpt;
                if (prevPosValid)
                {
                    // Playing continuously, the transport advances by exactly one block;
                    // anything else (forward jump, rewind, loop wrap) is a reposition.
                    const juce::int64 expected = prevPosSamples + (prevPlaying ? (juce::int64) prevPosBlock : 0);
                    if (std::abs (now - expected) > (juce::int64) buffer.getNumSamples())
                        seeked = true;
                }
                prevPosSamples = now;
                prevPosBlock   = buffer.getNumSamples();
                prevPosValid   = true;
            }
            else prevPosValid = false;
        }

    if ((playing && ! prevPlaying) || (playing && seeked))
        engine.getLevels().resetHold();
    prevPlaying = playing;

    engine.setTransportPlaying (playing); // a pause edge kills Velvet's noise tail (#4)
    auto e = params.toEngine (internal.oversampleIndex());
    if (const int sp = soloPreviewMask.load (std::memory_order_relaxed); sp >= 0)
        e.mbSolo = sp; // momentary hold audition overrides the latched solo (#8)
    engine.setParameters (e);
    engine.process (buffer);
}

// ----------------------------------------------------------------------------
void AnamorphAudioProcessor::applyAutoGain()
{
    adoptPendingHostState(); // message thread: a pending host restore lands BEFORE this edit (D-2)

    // "Apply": OVERRIDE Output Gain with the measured loudness compensation as a
    // fixed value (feedback #18). The match gain is measured pre-output-gain, so
    // setting Output Gain = matchDb makes the output sit at the dry loudness.
    // (Override, not add -- otherwise repeated Apply presses keep dropping it.)
    const float matchDb = engine.getMatchGainDb();

    if (auto* og = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (pid::outputGain)))
    {
        const float target = juce::jlimit (-24.0f, 24.0f, matchDb);
        og->beginChangeGesture();
        og->setValueNotifyingHost (og->convertTo0to1 (target));
        og->endChangeGesture();
    }

    // Level Match is a custom RangedAudioParameter subclass now; the gesture/notify calls below are
    // AudioProcessorParameter methods, so take the base pointer instead of a concrete-type cast.
    if (auto* match = apvts.getParameter (pid::autoGainMatch))
    {
        match->beginChangeGesture();
        match->setValueNotifyingHost (0.0f);
        match->endChangeGesture();
    }
}

// ----------------------------------------------------------------------------
//  Custom Undo/Redo (per A/B slot, sound params only) -- #10 / #11 / #12
// ----------------------------------------------------------------------------
bool AnamorphAudioProcessor::isViewParam (const juce::String& id) noexcept
{
    return pid::isViewParam (id); // single shared list (presets/A-B/undo agree)
}

juce::String AnamorphAudioProcessor::soundSignature() const
{
    juce::String sig;
    for (auto* p : getParameters())
        if (auto* wid = dynamic_cast<const juce::AudioProcessorParameterWithID*> (p))
            if (! isViewParam (wid->paramID))
                // As the plug-in RENDERS it, the same question the preset modified-marker
                // asks (D-2 round 9, ADR-0036 §17). These two signatures answer "has the
                // sound changed?" for different purposes and over different parameter sets,
                // but they must not answer it differently for the same movement: signing the
                // raw normalised value here while the marker signs the rendered one would
                // record an undo step for a sub-step move on a discrete parameter -- 0.66 to
                // 0.67 on a 4-choice, both index 2 -- that the marker simultaneously
                // declares to be no change, and that undoing would neither hear nor show.
                sig << juce::String (normalisedAsRendered (*p), 5) << ',';
    return sig;
}

void AnamorphAudioProcessor::syncCommitted()
{
    committed = currentStateSet();
    committedSig = soundSignature();
    lastPolledSig = committedSig;
    openGestures = 0;             // A/B switch / preset / session load is not a user gesture
    pendingGestureCommit = false;
}

// A full snapshot: parameters PLUS the live preset name + clean baseline (#6). The params carry the
// additive `raw` attribute so A/B slots + undo round-trip discrete params exactly (no snap drift).
AnamorphAudioProcessor::StateSet AnamorphAudioProcessor::currentStateSet()
{
    return { copyStateWithRawValues(), presets.currentName(), presets.baseline(), presets.selection() };
}

// Restore a state set: parameters (keeping the shared view params) AND the
// preset metadata, so the name + dirty-star reappear exactly as stored (#6).
void AnamorphAudioProcessor::applyStateSet (const StateSet& s)
{
    // ORDER IS LOAD-BEARING: parameters first, metadata second. setMeta resolves an EMPTY
    // baseline by calling soundSig(), which reads the LIVE apvts -- so it means "the state
    // just applied is its own clean baseline" only while these two lines are in this order.
    // Swapping them would baseline a pre-0.6.4 A/B slot against the OUTGOING sound and leave
    // its dirty-star wrong from then on, with nothing to catch it. See PresetManager.h.
    applyStatePreservingView (s.params);
    presets.setMeta (s.name, s.baseline, s.selection);
}

void AnamorphAudioProcessor::applyStatePreservingView (const juce::ValueTree& target)
{
    // Restore a snapshot but keep the CURRENT shared view/Settings params (#10/#13).
    float saved[std::size (pid::viewParams)];

    // The same three steps as a host restore's sound half (applySoundTree), on the
    // editor's own thread: repair the serialized text on OUR copy, hand that copy to
    // JUCE in ONE locked replaceState, then re-assert the exact raw values. A slot
    // tree adopted verbatim from a restored session can carry a malformed value just
    // as a session can, and the repaired text has to reach the LIVE tree through
    // JUCE's lock here too -- an unlocked write into `apvts.state` would race an
    // off-message-thread save's locked copyState (D-2, ADR-0036).
    // OUTSIDE the §24 scope, deliberately. This seam exists so a test can hold a replacement open
    // at its LAST INSTANT BEFORE IT BEGINS and let a host restore run to completion there; the
    // replacement then finishes and is the last writer. That is a legal ordering under §24 -- two
    // replacements one after the other -- and it is what State tests 45 and 50 are about. Firing it
    // inside the scope would make the restore they wait for block on this thread, so the seam would
    // spin out its bound and the tests would pass on a timeout rather than on the ordering. The
    // interleave §24 forbids is reached through `insideSoundReplacement` instead, which fires
    // part-way through the write loop where the mixture is actually made.
    if (seams.beforeSoundReplacementWrites) seams.beforeSoundReplacementWrites();

    // §24: the same mutual exclusion as applySoundTree. This path is message-thread only, but the
    // replacement it can interleave with -- a host thread's decode installing a restored sound --
    // is not, and an A/B apply or an undo torn against that decode mixes two sessions exactly as
    // two restores do. The view-param restore below is inside the scope for the same reason.
    const juce::ScopedLock oneAtATime (soundReplacement);

    // INSIDE the scope, with the write-back at the end of this function. The view params are the
    // one part of this replacement that is READ from the live sound rather than taken from the
    // snapshot, so capturing them before the exclusion would let a host thread's decode-install
    // move Bypass between the read and the write and settle a sound that is the slot's everywhere
    // else and the restore's there -- the mixture this rule forbids, in the one place State test
    // 62's oracle cannot see it (it classifies the preset-carried set, and Bypass is excluded).
    for (size_t i = 0; i < std::size (pid::viewParams); ++i)
        saved[i] = apvts.getParameter (pid::viewParams[i])->getValue();

    auto copy = target.createCopy();
    repairSerializedValues (copy);
    apvts.replaceState (copy);
    // Synchronously force every parameter to its exact (raw) value from the snapshot, so undo /
    // redo / A-B apply propagate exactly like host state restore -- replaceState alone can leave a
    // param at a stale/snapped value (see reassertParameters). View params are re-overridden below.
    reassertParameters (copy, /*notifyHost*/ true); // undo/redo/A-B is editor-initiated: notify host+editor

    for (size_t i = 0; i < std::size (pid::viewParams); ++i)
        apvts.getParameter (pid::viewParams[i])->setValueNotifyingHost (saved[i]);

    // A state set replaced the live sound, and it is finished replacing it: the counter
    // is bumped HERE, after the last write, so replacements are ordered by completion
    // (D-2 §14). Bumping at the top ordered them by start, which let a replacement that
    // began earlier and finished later leave its sound live under a higher token.
    noteWholeSoundReplaced();
}

// apvts.copyState() with each PARAM node additively stamped with its exact raw getValue(): pluginval
// sets RAW normalised values and expects them back within 0.1, but APVTS serialises the DENORMALISED
// (snapped) value, which for discrete params (Bool/Choice/Int) can be >0.1 from the raw value. The
// `raw` attribute carries the exact value so the round-trip is bit-faithful. Additive + backward-
// compatible: the APVTS `value` is unchanged, old sessions/plugins ignore `raw` (no removal/rename).
juce::ValueTree AnamorphAudioProcessor::copyStateWithRawValues()
{
    auto tree = apvts.copyState();
    for (auto param : tree)
        if (param.hasType ("PARAM"))
            if (auto* p = apvts.getParameter (param.getProperty ("id").toString()))
                param.setProperty ("raw", p->getValue(), nullptr);
    return tree;
}

namespace
{
    // The same predicate the preset path uses (PresetManager.cpp), so a malformed
    // serialized value cannot mean one thing in a session and another in a preset.
    // False means "no usable number here"; every caller answers that with the
    // parameter default, which is what SERIALIZATION_REGISTRY.md already records
    // for an absent node.
    bool readSerializedValue (const juce::var& prop, float& out)
    {
        if (prop.isVoid()) return false;
        if (prop.isString())
        {
            const auto text = prop.toString().trim();
            if (! anamorph::looksLikePlainNumber (text.toRawUTF8())) return false;
        }
        const float v = (float) (double) prop;
        if (! anamorph::isUsableSerializedValue (v)) return false;
        out = v;
        return true;
    }
}

// The SERIALIZED-TEXT half of a restore's repair, on a tree WE own (D-2, ADR-0036).
//
// A serialized value is text, and text can be anything: `nan` and `inf` (which
// JUCE's number parser accepts), `abc`, `""`, `0x10`, `1e39`, a usable number
// outside the parameter's range (`raw="-7"`). Every one of those must resolve to
// something the parameter can hold -- the default for unusable text or a
// non-finite value, the nearest range endpoint for a finite out-of-range one --
// and the REPAIRED value is what has to reach the file, or the damage survives
// every later save (ER-STATE-25, State test 36). Until D-2 that write went into
// the LIVE `apvts.state` after replaceState, directly and outside JUCE's own
// lock. That is a data race against any other thread's locked `copyState()` --
// an off-message-thread save, precisely the caller RISK-007 is about -- and
// JUCE exposes no way to take its lock. So the repair now happens BEFORE the
// tree is handed to JUCE, on the private copy the caller is about to hand over:
// the live tree is then only ever written by `replaceState`, under JUCE's lock,
// and the repaired text is in it from the first instant rather than patched in
// afterwards. One consequence is deliberate and better: JUCE pushes @value
// through setValueNotifyingHost during replaceState, so the host is now told the
// REPAIRED value, where it used to be told the clamped garbage first (a poisoned
// width read as 1.000000, the maximum; `abc` as 0.000000, the minimum -- measured
// on the pre-D-2 build) and then silently corrected.
//
// The classification is decided on the INPUT, before any clamp hides the
// evidence: text that is not a usable number at all, or a usable one outside the
// field's range. Snapping is NOT repair -- a stepped parameter moving "0.4" to its
// nearest step is the parameter doing its job on a legitimate value, and
// rewriting for that would be the "always rewrite" behaviour this deliberately
// avoids. BOTH attributes are rewritten when the node carries them: `value` is
// what an older build (which has no `raw` path) reads, and `raw` is what this
// build prefers and what A/B slots and undo snapshots copy. The rule is the same
// one reassertParameters applies to the VALUE below, so the two cannot disagree
// about what a malformed value means; it is deliberately separate from "the live
// value moved", because a file can be corrupt AND resolve to the value already
// loaded (ER-STATE-25), and the text is repaired either way.
//
// Both branches validate the INPUT before any clamp or conversion, for the
// reason anamorph::SerializedNumber.h records with the measured table: jlimit and
// convertTo0to1 both CLAMP, so an infinity reaching either one comes out as a
// finite range ENDPOINT and every later finiteness test passes.
void AnamorphAudioProcessor::repairSerializedValues (juce::ValueTree& tree) const
{
    if (! tree.isValid()) return;

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (auto node = tree.getChildWithProperty ("id", rp->paramID); node.isValid())
            {
                float serialized = 0.0f;
                const bool haveRaw = node.hasProperty ("raw")
                                     && readSerializedValue (node.getProperty ("raw"), serialized);
                float norm;
                bool repaired;
                if (haveRaw)
                {
                    norm = juce::jlimit (0.0f, 1.0f, serialized);   // `raw` is normalised: 0..1
                    repaired = serialized < 0.0f || serialized > 1.0f;
                }
                else if (readSerializedValue (node.getProperty ("value"), serialized))
                {
                    norm = juce::jlimit (0.0f, 1.0f, rp->convertTo0to1 (serialized));
                    const auto& r = rp->getNormalisableRange();
                    repaired = serialized < r.start || serialized > r.end;
                }
                else
                {
                    norm = rp->getDefaultValue();
                    repaired = true;                                 // unusable text
                }

                if (repaired)
                {
                    node.setProperty ("value", rp->convertFrom0to1 (norm), nullptr);
                    if (node.hasProperty ("raw"))
                        node.setProperty ("raw", norm, nullptr);
                }
            }
}

// Synchronously force every parameter to its restored value, from the just-restored tree.
//
// CORRECTED 2026-08-31 (ER-STATE-04) after checking the pinned JUCE 9.0.1 rather than
// re-asserting the original reasoning. What replaceState actually does, and what it leaves:
//  1. `apvts.replaceState()` DOES propagate: assigning `state` fires valueTreeRedirected ->
//     updateParameterConnectionsToChildTrees -> setDenormalisedValue -> setValueNotifyingHost,
//     so for every PARAM node PRESENT in the new tree the parameter, the DSP atomic, the
//     editor's attachments and the host all move. The older claim here -- that it "swaps only
//     the tree" -- was wrong. What it does NOT cover is the residual this function exists for:
//     it reads only @value.
//  1b. CORRECTED AGAIN 2026-08-31 (ER-STATE-07), by running it rather than reasoning about it:
//     ABSENT nodes are covered by replaceState too, which round 1 (ER-STATE-01) got wrong in the
//     opposite direction. updateParameterConnectionsToChildTrees clears every adapter's tree,
//     re-points the ones the new state carries, then APPENDS a fresh empty PARAM node for each
//     adapter left over -- and that appendChild fires the APVTS's own valueTreeChildAdded ->
//     setNewState -> setDenormalisedValue(getProperty("value", denormalisedDefault)). The node
//     has no @value, so the parameter is set to its DEFAULT, through setValueNotifyingHost:
//     host, editor and the drive/algorithm latency listeners all included. So the default branch
//     below is a redundant, idempotent backstop, not the thing that makes rule 2 hold; it is
//     kept because it costs one comparison per absent parameter and does not depend on that
//     JUCE internal staying as it is. Measured with --latency-restore-probe step 0b.
//  2. @value is the DENORMALISED (snapped) value; for discrete params the saved "raw" attribute
//     (see getStateInformation) carries the EXACT normalised getValue() pluginval set, so the
//     round-trip is bit-faithful and passes its 0.1 raw-value tolerance. replaceState cannot
//     use it, so restoring exactly is this function's job.
// Prefer "raw" (exact); fall back to the denormalised "value" for legacy sessions that lack it.
// Idempotent: parameters already at the target value are left untouched.
//
// SINCE D-2 (ADR-0036) THE TREE IS ALREADY REPAIRED when this runs: every caller passes the
// copy repairSerializedValues() rewrote and replaceState adopted, so the classification below
// finds nothing malformed on an ordinary path and this function writes NO tree property. The
// classification stays, as the VALUE-side backstop it always also was: a malformed value that
// somehow reached a parameter is still driven to the same answer the text repair gives.
//
// notifyHost: TRUE for editor-initiated restores (undo / redo / A-B via applyStatePreservingView) --
// setValueNotifyingHost updates value + DSP atomic + editor + host. FALSE for host state restore
// (setStateInformation): a parameter-change callback during the HOST's own state load can be treated
// by some DAWs as an automation write, so we must NOT notify the host. setValueNotifyingHost is
// setValue + sendValueChangedMessageToListeners, and the latter reaches the host via the parameter's
// owner-listener, so it can't be used. Instead update getValue() with setValue() and write the raw
// atomic the audio thread reads (getRawParameterValue) DIRECTLY.
// NOTE what that does and does not buy (also corrected 2026-08-31): replaceState has ALREADY
// notified the host for every PARAM node whose denormalised value moved, so this flag suppresses
// notification only for THIS pass's residual corrections -- it cannot make the whole restore
// silent, and it never could. By the same token the earlier "trade-off" recorded here was wrong:
// an open editor DOES track the restore, through the same attachments replaceState drives; only
// these residual corrections are invisible to it, and they resolve on the next editor sync.
// undo/redo/A-B keep the full notifyHost=true path.
void AnamorphAudioProcessor::reassertParameters (const juce::ValueTree& restoredApvtsTree, bool notifyHost)
{
    if (! restoredApvtsTree.isValid()) return;

    bool silentSoundChange = false;

    // Idempotent single-parameter apply, shared by both branches below.
    auto applyNorm = [&] (juce::RangedAudioParameter* rp, float norm)
    {
        // A serialized value is text, and `nan` is text JUCE's number parser
        // accepts, so a hand-edited or corrupted session can carry
        // <PARAM value="nan"/>. It must never become parameter state: a
        // non-finite continuous parameter latches its smoother target, every
        // output sample goes non-finite, and ADR-0009's sample-level self-heal
        // then zeroes the block and resets the engine on EVERY block --
        // permanent silence, which `getStateInformation` writes straight back
        // out so the next project load reproduces it. The parameter default is
        // the same answer an ABSENT node already gets just below. Since D-2 the
        // text repair upstream already resolved it; this is the value-side
        // backstop, and it is the one place that sees every parameter on every
        // restore path.
        if (! std::isfinite (norm))
            norm = rp->getDefaultValue();

        // EXACTLY EQUAL, OR IT IS WRITTEN (D-2 round 11, ADR-0036 section 19). Written as
        // a negated == so that a NaN on EITHER side still counts as "differs" and gets
        // repaired -- the plain `!=` reads the same but a plain `>` comparison does not:
        // it is false when either operand is NaN, which is exactly how a poisoned
        // parameter used to survive the pass meant to fix it.
        //
        // This gate carried a 1e-6 tolerance until round 11. It is the same shape as the
        // one deleted from the preset baseline in the same round and it sat on the same
        // coherence path: the value it declined to write is the value the SESSION stored,
        // while the baseline travelling with that session is adopted verbatim
        // (baselineOfRestore). Keeping the live value and the restored baseline is the
        // combination that makes an untouched preset show the modified star -- and, as
        // there, a tolerance cannot tell a float tail from a real difference of the same
        // size, so the only defensible answer is "restore what was stored".
        //
        // The tolerance was NOT load-bearing, which is the part that had to be measured
        // rather than assumed, because a restore must be a FIXED POINT: a host may apply
        // one chunk any number of times, and if each application nudged a value by a float
        // tail the sound would walk. Measured on the round-11 tree with this comparison
        // exact: 2000 random sounds x 20 re-applications of their own chunk move no
        // parameter at all (worst delta 0.0) and move no signature; 3000 project
        // save/reopen round trips leave the preset marker clean on both a fresh instance
        // and the same one. The tolerance had been declining 23 writes per 198000 -- all
        // of them float tails on the four gridless log-mapped frequency ranges, none of
        // them needed to reach the fixed point.
        const bool valueMoves = ! juce::exactlyEqual (norm, rp->getValue());

        if (valueMoves)
        {
            if (notifyHost)
                rp->setValueNotifyingHost (norm);
            else
            {
                rp->setValue (norm); // getValue() only -- no host / listener notification
                if (auto* atom = apvts.getRawParameterValue (rp->paramID))
                    atom->store (rp->convertFrom0to1 (norm)); // DSP value (snapped denormalised)
            }
            if (! notifyHost && ! pid::isViewParam (rp->paramID))
                silentSoundChange = true;
        }
    };

    // Test seam: fires ONCE, part-way through the write loop -- the only place from which the
    // interleave §24 forbids can be constructed deterministically. A harness lands a competing
    // whole-sound replacement here and then requires the settled live sound to be one session's,
    // not a mixture. Empty in every shipping path.
    int written = 0;
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (++written == 4 && seams.insideSoundReplacement) seams.insideSoundReplacement();
            if (auto node = restoredApvtsTree.getChildWithProperty ("id", rp->paramID); node.isValid())
            {
                // The same two-branch reading as repairSerializedValues, on the same
                // predicate, so a value that classified as usable there is applied
                // here exactly. A property that is not a usable number means the
                // parameter default -- the same answer the absent-node branch below
                // gives, and the same one the text repair already wrote.
                float serialized = 0.0f;
                const bool haveRaw = node.hasProperty ("raw")
                                     && readSerializedValue (node.getProperty ("raw"), serialized);
                float norm;
                if (haveRaw)
                    norm = juce::jlimit (0.0f, 1.0f, serialized);   // `raw` is normalised: 0..1
                else if (readSerializedValue (node.getProperty ("value"), serialized))
                    norm = juce::jlimit (0.0f, 1.0f, rp->convertTo0to1 (serialized));
                else
                    norm = rp->getDefaultValue();
                applyNorm (rp, norm);
            }
            else
            {
                // No PARAM node for this parameter in the restored blob (an older
                // session predating it, or a partial host chunk): apply the
                // parameter DEFAULT, exactly as the preset path already does for a
                // missing child (PresetManager::applySoundTree) and as
                // SESSION_COMPATIBILITY_POLICY rule 2 / SERIALIZATION_REGISTRY
                // ("Default: per-parameter defaults") record.
                //
                // A BACKSTOP, not the mechanism -- see 1b above. replaceState has
                // already applied this same default via its appended-node path, so
                // by the time this runs the parameter is at it and applyNorm's gate
                // is false. Round 1 claimed a reused live instance kept the previous
                // project's value here; measurement (--latency-restore-probe step 0b)
                // refuted that. Kept anyway: it is one comparison, it is the same
                // answer, and it does not rely on a JUCE internal. View params are
                // unaffected where rule 5 applies: applyStatePreservingView
                // re-overrides them after this call.
                applyNorm (rp, rp->getDefaultValue());
            }
        }

    // The notifyHost=false path (host session restore) applies values via
    // setValue(), which does NOT fire parameterValueChanged -- so the listener
    // never bumps soundParamGen. The notify path bumps it per changed sound
    // param via the listener; mirror that here with a single bump when a
    // listener-tracked (non-view) sound param actually changed silently, so the
    // S10 signature caches (pollUndoCoalesce and PresetManager::isDirty) rebuild
    // against the restored values instead of reusing a signature cached from the
    // pre-restore state (which left a false dirty-star until the next real edit).
    // Bumped only on an actual sound change, so no needless cache invalidation.
    if (silentSoundChange)
        soundParamGen.fetch_add (1, std::memory_order_relaxed);
}

// The sound half of a host restore, on the CALLER's thread (D-2, ADR-0036): this is
// the part that must be synchronous whoever calls -- the ordinary VST3/AU order is
// setState, then setActive/prepareToPlay, and prepareToPlay primes the engine from
// the parameters -- and it is the part JUCE owns and makes thread-aware (the APVTS
// locks its tree; ParameterAttachment hops to the message thread on its own).
// Repair on our copy, one locked replaceState, then the exact raw values.
juce::uint32 AnamorphAudioProcessor::applySoundTree (const juce::ValueTree& soundTree)
{
    // A state set replaces the live sound (D-2 §12). `begin` before the first write and
    // the token after the last bracket this replacement, so the value returned is a
    // token only when no other replacement ran inside ours -- see soundReplacementToken.
    // ONE AT A TIME (§24). Held across the WHOLE replacement -- the locked replaceState and the
    // unlocked per-parameter loop after it -- because it is the loop that made two replacements
    // able to interleave into a live sound holding values from both. This is the sound half of a
    // host thread's decode when it runs there, so this is the one site where the lock is ever
    // actually contended; every other replacement is message-thread work.
    const juce::ScopedLock oneAtATime (soundReplacement);
    const auto begin = soundSetGen.load (std::memory_order_relaxed);
    auto copy = soundTree.createCopy();
    repairSerializedValues (copy);
    apvts.replaceState (copy);
    reassertParameters (copy, /*notifyHost*/ false); // host restore: no host-notify (see above)
    return soundReplacementToken (begin);
}

// Message thread. Count nested / overlapping gestures (e.g. the two-parameter Multiband band move
// opens gestures on both split params); request a single undo commit only after the LAST closes.
void AnamorphAudioProcessor::parameterGestureChanged (int, bool gestureIsStarting)
{
    // D-2: deliberately NO adoptPendingHostState() here. This is an
    // AudioProcessorParameter::Listener callback and JUCE delivers it holding the
    // parameter's listenerLock; adopting a restore inside it would take the APVTS
    // lock (syncCommitted() -> copyState()), the reverse of the order a host-thread
    // replaceState() takes the two (APVTS lock first, then listenerLock through
    // setValueNotifyingHost). `--d2-stress-probe` reported exactly that cycle as a
    // lock-order inversion when a drain sat here. A restore that lands mid-gesture
    // is adopted by the next poll instead, where its syncCommitted() zeroes the
    // gesture count exactly as an inline restore mid-gesture always has.
    if (gestureIsStarting)                       ++openGestures;
    else if (openGestures > 0 && --openGestures == 0) pendingGestureCommit = true;
}

void AnamorphAudioProcessor::pollUndoCoalesce()
{
    // D-2: the editor's tick comes through here, so a host restore handed over
    // from another thread is adopted before this poll reads or writes anything the
    // adoption replaces (the undo history, the committed baseline, the gesture
    // bookkeeping). One relaxed load when nothing is pending.
    adoptPendingHostState();
    pollUndoCoalesceAdopted();
}

// The poll itself, with the drain already done. A preset load reaches this through
// `onAboutToLoad` AFTER the load path has drained and (for `step`) after the target row has
// been derived, so draining again here would move the selection under a chosen row (§23).
void AnamorphAudioProcessor::pollUndoCoalesceAdopted()
{

    // S10: the signature is a pure function of the listened sound parameters,
    // so an unchanged generation means it is character-identical to the one
    // built on the previous poll -- and with no gesture commit pending, every
    // branch below is then provably a no-op (sig == committedSig holds as an
    // invariant after each full run). Skip the ~36 String formats. Polling
    // cadence and all coalescing semantics are untouched; the generation is
    // sampled BEFORE building, so a concurrent change simply rebuilds next tick.
    //
    // Why skipping is behavior-preserving -- the post-run invariant
    // sig == committedSig:
    //  * The non-gesture branch sets committed = current, committedSig = sig.
    //  * The gesture-commit branch, when it changes anything, also sets
    //    committedSig = sig; when sig == committedSig it is already a no-op.
    //  * syncCommitted() (ctor / A-B / preset / session load) rebuilds
    //    committedSig from the live params, re-establishing it there too.
    // So whenever this early-return fires, gen is unchanged since the last full
    // run => sig is byte-identical to that run's sig => sig == committedSig, and
    // with pendingGestureCommit false the skipped body would do nothing. A
    // parameter change bumps gen before the next tick, so an edit is still
    // reflected on the very next poll -- the cadence and outcome are identical.
    const auto gen = soundParamGen.load (std::memory_order_relaxed);
    if (gen == polledGen && ! pendingGestureCommit)
        return;
    polledGen = gen;

    const auto sig = soundSignature();

    if (openGestures > 0)          // a user gesture is in progress -> never commit mid-gesture
    {
        lastPolledSig = sig;
        return;
    }

    if (pendingGestureCommit)      // exactly ONE undo step per finished gesture (knob or band move)
    {
        pendingGestureCommit = false;
        if (sig != committedSig)
        {
            abUndo[abActive].undo.push_back (committed);   // the PREVIOUS state set (name + baseline, #6)
            if (abUndo[abActive].undo.size() > 128) abUndo[abActive].undo.erase (abUndo[abActive].undo.begin());
            abUndo[abActive].redo.clear();
            committed = currentStateSet();
            committedSig = sig;
        }
    }
    else if (sig != committedSig)  // NON-gesture change (host automation / programmatic): fold into
    {                              // the baseline WITHOUT creating an undo step (automation is not undoable)
        committed = currentStateSet();
        committedSig = sig;
    }

    lastPolledSig = sig;
}

// Record ONE undo step for a preset load. Called by the PresetManager::onLoaded hook AFTER the new
// preset's params + name/baseline are in place; onAboutToLoad has already flushed any settled edit,
// so `committed` holds the exact pre-load state set (previous preset name + params). Push it and adopt
// the freshly-loaded state as the new committed baseline. Distinct from the non-gesture fold in
// pollUndoCoalesce: a preset switch IS a discrete, undoable user action (unlike host automation).
void AnamorphAudioProcessor::commitPresetSwitchUndoStep()
{
    const auto sig = soundSignature();
    if (sig != committedSig)
    {
        abUndo[abActive].undo.push_back (committed);   // the PREVIOUS state set (name + baseline, #6)
        if (abUndo[abActive].undo.size() > 128) abUndo[abActive].undo.erase (abUndo[abActive].undo.begin());
        abUndo[abActive].redo.clear();                 // a new user action invalidates the redo stack
        committed = currentStateSet();                 // now carries the NEW preset name + clean baseline
        committedSig = sig;
    }
    else
    {
        // Same SOUND -- e.g. a user preset saved from the factory preset next to it, or the
        // user simply re-picking the row that is already ticked. There is nothing sonic to
        // undo, so no step is recorded, but the baseline must still adopt the new name /
        // identity / clean baseline: leaving the previous preset's metadata on it means the
        // next undo restores THAT name and tick onto this sound (#4).
        //
        // Redo is invalidated ONLY when the identity actually MOVED. A surviving redo entry
        // carries the PREVIOUS preset's identity, so redoing it after a switch would drag the
        // tick off the row the user just picked -- that is the case worth destroying redo for.
        // Re-adopting the identity that is already committed changes nothing the redo entry
        // could contradict, and clearing it there would silently discard an edit the user had
        // undone and was about to redo, for no benefit.
        if (presets.selection() != committed.selection)
            abUndo[abActive].redo.clear();
        committed = currentStateSet();
    }
    lastPolledSig = sig;
    openGestures = 0;             // a preset load is a program state jump, not a user gesture -- drop any
    pendingGestureCommit = false; // in-flight gesture bookkeeping so nothing re-commits afterwards
}

void AnamorphAudioProcessor::undo()
{
    // Flush any settled-but-unpolled gesture into its own undo step first (the
    // editor timer polls at 24 Hz, so a commit can be pending for up to ~42 ms),
    // exactly like PresetManager::onAboutToLoad does before a preset switch.
    // Otherwise an edit finished just before the click would be silently folded
    // by the openGestures/pendingGestureCommit reset below and this undo would
    // jump PAST it.
    pollUndoCoalesce();
    auto& st = abUndo[abActive];
    if (st.undo.empty()) return;
    engine.requestDuck(); // mask the level jump (#1, 0.6.4)
    st.redo.push_back (currentStateSet());
    committed = st.undo.back(); st.undo.pop_back();
    applyStateSet (committed);
    committedSig = soundSignature();
    lastPolledSig = committedSig;
    openGestures = 0;             // undo is a program state jump, not a user gesture -- drop any
    pendingGestureCommit = false; // in-flight gesture bookkeeping so nothing re-commits afterwards
}

void AnamorphAudioProcessor::redo()
{
    pollUndoCoalesce(); // same settled-gesture flush as undo()
    auto& st = abUndo[abActive];
    if (st.redo.empty()) return;
    engine.requestDuck(); // mask the level jump (#1, 0.6.4)
    st.undo.push_back (currentStateSet());
    committed = st.redo.back(); st.redo.pop_back();
    applyStateSet (committed);
    committedSig = soundSignature();
    lastPolledSig = committedSig;
    openGestures = 0;             // redo is a program state jump, not a user gesture -- drop any
    pendingGestureCommit = false; // in-flight gesture bookkeeping so nothing re-commits afterwards
}

// ----------------------------------------------------------------------------
//  A/B compare
// ----------------------------------------------------------------------------
void AnamorphAudioProcessor::abEnsureInit()
{
    // An INVALID slot means "no stored state for this slot" -- at construction, or after a
    // restore whose AB node carried no usable params payload for it (readSlot resets the whole
    // slot, so an unreadable payload arrives here rather than keeping the previous project's).
    // BOTH slots get the same answer, the current live state, which is what
    // SERIALIZATION_REGISTRY.md means by "lazily initialised from current".
    //
    // Slot B used to be seeded from a copy of slot A instead. At construction the two are
    // indistinguishable -- slot A has just been seeded from the same live state -- so this
    // changes nothing on the path that runs every time. They diverged only when slot A was
    // valid and slot B was not, i.e. an AB node whose `slotBParams` alone was missing or
    // unparsable: slot B came back as a DUPLICATE of slot A rather than as the state just
    // restored, and a later save wrote that duplicate out. currentStateSet() builds a fresh
    // tree per call, so the slots stay independent with no explicit copy.
    bool seeded = false;
    for (auto& slot : abSlot)
        if (! slot.isValid())
        {
            slot = currentStateSet();
            seeded = true;
        }
    // The published snapshot carries an unseeded slot as INVALID and the
    // off-message-thread save resolves it from the live parameters at save time,
    // exactly as this function does here -- so the two agree until this seeding
    // pins the slot to a particular moment, at which point the snapshot must say so.
    if (seeded)
        publishProgram();
}

void AnamorphAudioProcessor::abApplySlot (int slot)
{
    // Read the WHOLE target state set: params (keeping the shared view params) AND
    // its preset name + dirty baseline, so switching shows that slot's own name,
    // never the previous slot's (#6). View/Settings params never swap (#13/#15).
    applyStateSet (abSlot[slot]);
}

void AnamorphAudioProcessor::abSwitchTo (int slot)
{
    adoptPendingHostState(); // message thread: a pending host restore lands BEFORE this switch (D-2)
    abSwitchToAdopted (slot);
}

// The switch itself, with the drain already done. `abToggle` calls this directly because its
// target is derived from the state that drain established, and adopting again here would
// replace that state underneath a target already in hand (§23).
void AnamorphAudioProcessor::abSwitchToAdopted (int slot)
{
    slot = juce::jlimit (0, anamorph::kNumAbSlots - 1, slot); // defensive: never index out of bounds
    abEnsureInit();
    if (slot == abActive) return;
    engine.requestDuck();                              // mask the level jump (#1, 0.6.4)
    abSlot[abActive] = currentStateSet();              // store the whole state set in the old slot
    abMatchGain[abActive] = engine.getMatchGainDb();   // remember this slot's match (#23)
    abActive = slot;
    abApplySlot (slot);                                // ...whose setMeta republishes the snapshot (D-2)
    engine.injectMatchGainDb (abMatchGain[slot]);      // restore the new slot's match (#23)
    syncCommitted();                                   // the switch itself isn't undoable (#11)
}

void AnamorphAudioProcessor::abToggle()
{
    // Drain FIRST, then decide: the target is the other slot of the session that is
    // authoritative once every arrived restore has been adopted (§15), never of the one a
    // caller happened to observe earlier.
    //
    // ROUND 16 CLOSED THE REST OF IT (§23). Round 10 moved the derivation off the editor and
    // in here, but `abSwitchTo` drains AGAIN on entry, so the window merely got smaller: a
    // restore landing between the two drains was adopted by the second one, AFTER the target
    // had been derived from the outgoing session. When that restore made the other slot
    // active -- which is exactly what a session saved on slot B does -- the derived target
    // equalled the new `abActive` and `abSwitchTo`'s own "already there" guard returned
    // without switching. The user pressed A/B and NOTHING HAPPENED. The decision window makes
    // the second drain a no-op, so the target and the state it is applied to are the same
    // session by construction.
    adoptPendingHostState();
    if (seams.atRelativeDecision) seams.atRelativeDecision();   // test seam: land a restore HERE
    abSwitchToAdopted (abActive == 0 ? 1 : 0);                  // ...and NOT abSwitchTo: no second drain
}

void AnamorphAudioProcessor::abCopyToOther()
{
    adoptPendingHostState(); // message thread: a pending host restore lands BEFORE this copy (D-2)
    abEnsureInit();
    abSlot[abActive] = currentStateSet();
    const int other = abActive == 1 ? 0 : 1;
    // Record the target slot's pre-copy state so undoing on that slot reverts the
    // Copy without disturbing the active slot's history (#12).
    abUndo[other].undo.push_back (abSlot[other]);
    abUndo[other].redo.clear();
    abSlot[other] = currentStateSet(); // overwrite the other slot with the FULL state set (#6)
    publishProgram();                  // both slots moved and no preset metadata did (D-2)
}

// ----------------------------------------------------------------------------
juce::AudioProcessorEditor* AnamorphAudioProcessor::createEditor()
{
    adoptPendingHostState(); // the editor's constructor reads the program state this thread owns (D-2)
    return new AnamorphAudioProcessorEditor (*this);
}

namespace
{
    // One place that knows a Selection is three properties, shared by the root node and both
    // A/B slots. The encoding itself lives on PresetManager (`encodeSelection`).
    void writeSelection (juce::ValueTree& t, const anamorph::PresetManager::Selection& s,
                         const char* kindKey, const char* factoryKey, const char* fileKey)
    {
        const auto f = anamorph::PresetManager::encodeSelection (s);
        t.setProperty (kindKey,    f.kind,      nullptr);
        t.setProperty (factoryKey, f.factoryId, nullptr);
        t.setProperty (fileKey,    f.userFile,  nullptr);
    }

    anamorph::PresetManager::Selection readSelection (const juce::ValueTree& t,
                                                      const char* kindKey, const char* factoryKey,
                                                      const char* fileKey)
    {
        return anamorph::PresetManager::decodeSelection (t.getProperty (kindKey).toString(),
                                                         t.getProperty (factoryKey).toString(),
                                                         t.getProperty (fileKey).toString());
    }
}

// ----------------------------------------------------------------------------
//  Host state, both directions, with D-2's thread split (ADR-0036).
//
//  The shape is the same for save and restore: the SOUND (the APVTS) is JUCE's
//  and thread-aware, so it is read or written on whatever thread the host used;
//  the PROGRAM METADATA is this class's and message-thread-owned, so on the
//  message thread it is used directly and on any other thread it goes through an
//  immutable value -- a snapshot to read from, a decoded restore to hand over.
//  What the audio thread sees is unchanged either way: the parameter atomics,
//  InternalState's engine-config word (published synchronously on a restore, below)
//  and nothing in this section.
// ----------------------------------------------------------------------------

AnamorphAudioProcessor::ProgramSnapshot AnamorphAudioProcessor::ownedProgram() const
{
    // Message thread: every field is owned here and read here.
    ProgramSnapshot s;
    s.generation      = adoptedGeneration;
    s.settingsEditGen = internal.editGenerations();   // WITH the tree below, in one object (§21)
    s.presetName      = presets.currentName();
    s.presetBaseline  = presets.baseline();
    s.presetSelection = presets.selection();
    s.internalState   = internal.copyState();
    s.abActive        = abActive;
    for (int i = 0; i < anamorph::kNumAbSlots; ++i)
        s.abSlot[i] = abSlot[i];
    return s;
}

void AnamorphAudioProcessor::publishProgram()
{
    // Inside an adoption the individual hooks (setMeta, the Settings tree) would each
    // republish a half-adopted program; the adoption publishes once when it is whole.
    if (adoptingRestore) return;
    jassert (onMessageThreadOrNoMessageManager()); // the program state is message-thread-owned
    programMailbox.put (new ProgramSnapshot (ownedProgram()));
}

void AnamorphAudioProcessor::adoptPendingHostState()
{
    // Drains to a FIXED POINT (D-2 round 8, ADR-0036 §15). Every caller is a message-
    // thread entry point about to read or mutate program state, and the guarantee it
    // needs is not "one restore was adopted" but "nothing is pending any more": only
    // then is the state it goes on to edit the state of every restore that has arrived,
    // and only then does the precedence rule §10 states -- an action after a restore's
    // arrival lands on top of it -- actually hold for this caller. Adopting a single
    // restore left a newer one that arrived DURING the adoption still in the cell, and
    // the caller then edited a session that was already superseded; the later adoption
    // wholesale-overwrote the edit, even though the edit came after that restore's
    // arrival.
    //
    // This is a drain, not a wait: it stops as soon as the cell is empty and never
    // blocks. It cannot spin indefinitely in supported operation, because a host
    // serializes its own state calls (ADR-0036 §11), so a further restore can only
    // appear after a whole setStateInformation has returned.
    //
    // The fast path is still one relaxed load: nothing pending is the overwhelmingly
    // common case (every in-spec VST3 host restores on this very thread and never uses
    // the cell at all). A restore that lands after the last check is adopted at the next
    // entry point, or by the 20 Hz timer.
    while (! pendingRestore.empty())
    {
        std::unique_ptr<const RestoreDecode> r (pendingRestore.take());
        if (r == nullptr) break;   // only this thread takes, but the answer is exact either way
        if (seams.afterRestoreTake) seams.afterRestoreTake();

        // The generation FIRST. It tags every tree write the tail makes, so the
        // oversampling this adoption republishes lands only if no newer host restore has
        // published since (InternalState::publishEngineConfig); and it stamps the
        // snapshot published below, which is how a host-side save learns that the
        // program it describes is this restore's or later.
        adoptedGeneration = r->generation;
        internal.noteAdoptedGeneration (adoptedGeneration);
        {
            const juce::ScopedValueSetter<bool> adopting (adoptingRestore, true);
            adoptRestoreTail (*r);
        }
        publishProgram();
    }
}

// The restore TAIL: everything a restore changes that is not the sound. Message
// thread only, by construction -- inline when setStateInformation ran here, else
// from adoptPendingHostState(). Its order is the order the pre-D-2 restore ran:
// the Settings tree, the A/B slot set, the undo history, the preset metadata, the
// committed baseline.
void AnamorphAudioProcessor::adoptRestoreTail (const RestoreDecode& d)
{
    // SESSION COHERENCE (D-2 round 4, ADR-0036 §10). A restore handed over from a host
    // thread applied its sound there, at decode time, and its metadata lands HERE. In
    // between, this thread may have run an A/B switch, a Copy, an undo or a preset load
    // that replaced the live parameters -- the message thread cannot see a restore whose
    // decode has applied its sound but whose handoff is not in the cell yet, so it acts
    // on the sound of a session it has not adopted. Without this, the tail would then
    // stamp THIS restore's metadata over THAT action's sound: one saved session made of
    // two. Re-installing the decode's own sound makes the adoption commit sound and
    // metadata together, which is what "adopting a restore" has to mean.
    //
    // Both guards matter, and the first is the one round 5 sharpened (ADR-0036 §12).
    // What must trigger a re-install is another STATE SET having been installed since
    // the decode -- an A/B apply, an undo/redo, a preset load -- because then the live
    // sound is some other session's and this restore's metadata would sit over it.
    // What must NOT trigger one is the user having EDITED the restored sound: the
    // parameters are the restored session's, with a newer mutation in them, and
    // re-installing would erase that edit. `soundSetGen` counts only the wholesale
    // replacements, so it separates the two; keying this on `soundParamGen`, which
    // every knob turn bumps, is what erased pending sound edits in round 4. When
    // nothing has been replaced the re-install is skipped, so an ordinary restore also
    // costs no redundant burst of setValueNotifyingHost, as before.
    // And the engine-config word's generation identifies the LATEST restore that has
    // arrived: when a newer one has already published its sound, this older restore's
    // adoption must not resurrect its own (ADR-0036 §8's rule, applied to the sound).
    // An inline restore (generation 0) applied its sound on this thread with nothing
    // able to run in between, so it never needs this.
    // THE GUARD AND THE WRITE UNDER ONE LOCK (§24, round 17). Both terms it tests are moved by a
    // host thread's decode -- `engineConfigGeneration()` when that restore announces itself, and
    // `soundSetGen` when it installs its sound -- so checking them and then writing outside the
    // lock decides on a state that can be gone before the first parameter moves. The lock is
    // recursive, so `applySoundTree` re-entering it below is free.
    {
        const juce::ScopedLock oneAtATime (soundReplacement);
        if (d.generation != 0
            && internal.engineConfigGeneration() == d.generation
            && soundSetGen.load (std::memory_order_relaxed) != d.soundSetGen   // 0 (no owner provable) never matches
            && d.soundParams.isValid())
        {
            applySoundTree (d.soundParams);
        }
    }

    // The host-hidden Settings. A changed Oversampling fires InternalState's callback
    // -> requestLatencyUpdate(), synchronous on this thread; prepareToPlay re-asserts
    // it anyway. (The engine-config word was already published on the restoring
    // thread when that was not this one; the tree write republishes it with this
    // restore's generation, which is idempotent -- or yields, if a newer restore has
    // published since. The tree's value then trails that newer restore's by at most
    // one timer period, until its own tail lands.) A restore that came through the
    // cell carries its generation, and a Setting the user edited AFTER it arrived is
    // the newer arrival and stands (D-2 round 3); an inline restore is the newest
    // arrival by definition and writes every field.
    if (d.generation != 0) internal.adoptResolved (d.internalResolved, d.generation);
    else                   internal.applyResolved (d.internalResolved);

    // The slot set as a WHOLE -- both slots, the active index and the per-slot
    // Level-Match memory -- from the one decode, so no half of it can come from a
    // different project than the other half (the rule readSlot states per slot,
    // applied to the set). `abMatchGain` is the one member of the set that is never
    // serialized -- a runtime cache of what the matcher had settled on when each slot
    // was last left -- so there is nothing to overlay it with and every restore
    // resets it (ER-STATE-20, round 16; State test 31): leaving it alone let the
    // PREVIOUS project's figure survive into this session's first switch, which
    // ends with `engine.injectMatchGainDb (abMatchGain[slot])`. 0.0f is the member's
    // own initialiser, which is what makes this exactly the fresh-instance path.
    abActive = d.abActive;
    for (int i = 0; i < anamorph::kNumAbSlots; ++i)
    {
        abSlot[i]      = d.abSlot[i];
        abMatchGain[i] = 0.0f;
    }

    // Fresh session: clear undo history.
    abUndo[0] = {}; abUndo[1] = {};

    // Adopt the remembered preset name + baseline so the dirty-star is reproduced
    // (#6); fall back to a clean baseline at the restored state when absent.
    //
    // ABSENT and EMPTY are different answers, and only the decode can tell them apart -- the
    // same distinction `haveBaseline` already draws for the sibling field. `presetName` is
    // absent only in a session that predates it (< 0.6): there the label is the one a fresh
    // manager carries, and it has to be that CONSTANT rather than presets.currentName(), which
    // is whatever the PREVIOUS project left on this instance (hosts reuse one processor across
    // setStateInformation calls -- the same rule readSlot follows for the A/B slots above).
    // A present-but-EMPTY presetName is a real value meaning "this state has no preset"; since
    // 0.9.2 a session saved while sitting on a nameless A/B slot stores exactly that, so it is
    // adopted verbatim and must never be turned back into a name.
    const auto adoptedName = d.haveName ? d.restoredName : anamorph::PresetManager::defaultName();
    presets.setMeta (adoptedName, baselineOfRestore (d), d.restoredSelection);

    syncCommitted();
}

// THE CLEAN BASELINE A RESTORE ADOPTS (D-2 round 15, ADR-0036 §22). One function, so the
// prediction `viewOfRestore` publishes and the value `adoptRestoreTail` writes into the
// manager are the same answer by construction rather than by two sites agreeing.
//
// A session that recorded a baseline gets that baseline, verbatim. ABSENT and EMPTY are not
// the same thing -- only the decode can tell them apart, the same distinction `haveName`
// draws for the sibling field -- but they resolve alike here: neither means "modified" (an
// empty string is unequal to every possible sound, so it would pin the star on for ever) and
// neither may mean "whatever happens to be live when the message thread gets round to
// adopting". Both mean "this session recorded no baseline", and the answer is the sound THIS
// RESTORE INSTALLED, taken from the restore's own bytes at decode time.
//
// That last clause is the round-15 fix. It used to be a live read of the parameter atomics --
// `adoptRestoredState`'s `sigAtLoad = soundSig()`, and `setMeta`'s empty-baseline fallback --
// performed at ADOPTION time, which is an unbounded window later than the restore for a host
// thread's restore. Every sound edit made in that window was absorbed into the baseline, so
// the preset indicator reported the user's own edits as clean. It is the same defect round 9
// removed from the save baseline (§17) and round 10 from the preset-load baseline (§18,
// KI-029), in the third and last place the pattern occurs, and `soundSignatureAfterLoading`
// is the primitive round 10 built for exactly this.
juce::String AnamorphAudioProcessor::baselineOfRestore (const RestoreDecode& d)
{
    return (d.haveBaseline && d.restoredBaseline.isNotEmpty()) ? d.restoredBaseline
                                                               : d.restoredSoundSig;
}

// What the message thread WILL own once it adopts `d`, computed on the restoring
// thread so a save issued there before the adoption describes the sound it just
// applied. Each field follows the rule adoptRestoreTail applies: the name's
// absent-vs-empty distinction, and the baseline through the one resolver above. A
// slot with no usable payload stays INVALID and is resolved at save time, as
// abEnsureInit resolves it on the message thread.
std::unique_ptr<const AnamorphAudioProcessor::ProgramSnapshot>
AnamorphAudioProcessor::viewOfRestore (const RestoreDecode& d)
{
    auto v = std::make_unique<ProgramSnapshot>();
    v->presetName      = d.haveName ? d.restoredName : anamorph::PresetManager::defaultName();
    v->presetBaseline  = baselineOfRestore (d);
    v->presetSelection = d.restoredSelection;
    // The restore's own resolved Settings. Since round 12 this is NOT the whole answer for a
    // save taken before the adoption: a field the message thread edits after this restore
    // ARRIVES is the newer arrival and the adoption will keep it, so `getStateInformation`
    // overlays those fields from the published snapshot (ADR-0036 §21). `settingsEditGen` is
    // left at zero here and is never read from this view -- the generations that matter are
    // the message thread's, and they travel in the snapshot, not in this prediction.
    v->internalState   = d.internalResolved;   // built by the resolver, never edited again
    v->abActive        = d.abActive;
    for (int i = 0; i < anamorph::kNumAbSlots; ++i)
        v->abSlot[i] = d.abSlot[i];
    return v;
}

void AnamorphAudioProcessor::writeState (const ProgramSnapshot& s, const juce::ValueTree& settings, juce::MemoryBlock& destData)
{
    juce::ValueTree root ("AnamorphRoot");
    root.setProperty ("presetName", s.presetName, nullptr);       // remembered across sessions (F2)
    root.setProperty ("presetBaseline", s.presetBaseline, nullptr); // so the dirty-star survives reload (#6)
    // Which preset row the indicator points at, so reopening a project puts the tick back
    // where it was even when a user preset shares a factory preset's NAME (ADR-0024
    // amendment). Three additive strings, always written; all-empty means "no identity",
    // which is exactly what a pre-0.9.2 session restores as. METADATA ONLY: the sound comes
    // from the APVTS child below and is restored identically whether or not these resolve.
    // Nothing here is written into a user preset FILE -- that format is untouched.
    writeSelection (root, s.presetSelection, "presetSource", "presetFactoryId", "presetUserFile");
    root.appendChild (copyStateWithRawValues(), nullptr); // APVTS state + exact "raw" values per PARAM
    // A fresh copy, because appending re-parents a tree and the snapshot's must stay
    // free for the next save.
    root.appendChild (settings.createCopy(), nullptr); // host-hidden Settings / view state

    // A slot the snapshot carries as INVALID is "lazily initialised from current"
    // (SERIALIZATION_REGISTRY.md, `AB` child): the live parameters plus the
    // snapshot's own preset metadata -- what abEnsureInit() seeds on the message
    // thread, resolved here at save time exactly as it resolves there.
    auto resolved = [&] (int i) -> StateSet
    {
        if (s.abSlot[i].isValid()) return s.abSlot[i];
        return { copyStateWithRawValues(), s.presetName, s.presetBaseline, s.presetSelection };
    };
    const StateSet slotA = resolved (0), slotB = resolved (1);

    juce::ValueTree ab ("AB");
    ab.setProperty ("active", s.abActive, nullptr);
    // Each slot carries its params AND its preset name + baseline (#6) + its indicator identity.
    ab.setProperty ("slotAParams", slotA.params.toXmlString(), nullptr);
    ab.setProperty ("slotAName",   slotA.name, nullptr);
    ab.setProperty ("slotABase",   slotA.baseline, nullptr);
    writeSelection (ab, slotA.selection, "slotASource", "slotAFactoryId", "slotAUserFile");
    ab.setProperty ("slotBParams", slotB.params.toXmlString(), nullptr);
    ab.setProperty ("slotBName",   slotB.name, nullptr);
    ab.setProperty ("slotBBase",   slotB.baseline, nullptr);
    writeSelection (ab, slotB.selection, "slotBSource", "slotBFactoryId", "slotBUserFile");
    root.appendChild (ab, nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void AnamorphAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (onMessageThreadOrNoMessageManager())
    {
        // The owner's own state: a pending host restore lands first, the slots are
        // seeded as they always were, and the bytes are the ones this thread has
        // always written.
        adoptPendingHostState();
        abEnsureInit();
        const auto owned = ownedProgram();
        writeState (owned, owned.internalState, destData);
        return;
    }

    // See `offThreadStateCalls`: the host must not have another off-message-thread
    // state call in flight. Counted, asserted in debug, never blocking.
    const OffThreadStateCall serialized { *this };

    // Any other thread: the latest snapshot the message thread published, taken
    // into this side's own view -- unless a restore THIS side handed over is not
    // described by it yet, in which case the program to describe is that restore's,
    // from the view built when it was decoded, not the one the message thread still
    // owns. ONE comparison, between two values this side alone holds: the generation
    // the snapshot in hand carries (part of the immutable object, so it cannot be
    // about a different snapshot than the one taken) and this side's own last restore
    // generation. Reading generations separately from the take, as this once did,
    // let an adoption that completed between the two pair the OLD snapshot with a
    // "nothing pending" answer -- the restored sound with the previous program.
    if (auto* fresh = programMailbox.take())
        hostProgramView.reset (fresh);
    if (seams.afterHostSaveTake) seams.afterHostSaveTake();

    const bool covered = hostProgramView != nullptr
                      && (juce::int32) (hostProgramView->generation - hostRestoreGen) >= 0;
    const ProgramSnapshot* view = covered ? hostProgramView.get() : hostRestoreView.get();
    if (view == nullptr)
    {
        // Unreachable: the constructor publishes before any host call can arrive and
        // only this side ever takes. Refusing to write a block at all would hand the
        // host nothing to restore, so describe the fresh-instance program instead.
        jassertfalse;
        ProgramSnapshot fresh;
        fresh.presetName      = anamorph::PresetManager::defaultName();
        fresh.presetBaseline  = anamorph::PresetManager::soundSignatureFor (apvts);
        fresh.presetSelection = { anamorph::PresetManager::Selection::Kind::factory, "default", {} };
        fresh.internalState   = anamorph::InternalState::resolveRestore (juce::ValueTree ("ANAMORPH_INTERNAL"));
        writeState (fresh, fresh.internalState, destData);
        return;
    }

    // THE SETTINGS ARE DECIDED PER FIELD, NOT WITH THE SESSION (D-2 round 12, ADR-0036 §21).
    // `covered` above answers ONE whole-object question -- has the message thread adopted my
    // restore yet -- and rejecting the snapshot is right for every field that IS the session:
    // in this window the message thread still holds the OUTGOING project's preset name,
    // baseline, selection and A/B slots while the live parameters already hold the restored
    // sound, so writing them would rebuild exactly the mixed state §5 and State test 42 forbid.
    //
    // The Settings do not follow that rule. Since round 3 they are per-field, arrival-ordered
    // state (§9): a field edited at or after this restore ARRIVED is the newer arrival and the
    // adoption will KEEP it. `hostRestoreView` cannot know that -- it is built at decode time,
    // before the edit exists, and its Settings model `applyResolved` (write every field) while
    // the adoption that will actually run is `adoptResolved`. So a save here used to describe
    // the restore's session with Settings the plug-in was never going to hold, and a Setting
    // the user had already changed vanished from the project.
    //
    // The missing information crosses the way every other message-thread fact crosses: inside
    // the immutable snapshot, as the per-field generations published WITH the tree they
    // describe. Both are read from the one object in hand, so a publication landing between
    // two reads cannot pair a tree with someone else's generations -- the same rule round 2
    // established for the object-wide generation. The merge itself is `resolvedWithEdits`,
    // which applies `adoptResolved`'s own predicate to values instead of in place: the two
    // walk the same table in the same order and share `editIsNewerThan`, so they cannot
    // disagree about WHICH value wins. They are still two loops rather than one, so what
    // pins them together is a test, not the type system -- State test 59's two-restore legs
    // adopt after the save and require the tree to equal what the save described.
    //
    // A snapshot published BEFORE the restore arrived carries only generations below it, so
    // nothing is overlaid and the restore's own Settings stand -- which is the right answer
    // for a save that never observed the edit, and is what keeps §18's stated ordering
    // boundary intact rather than adding a second one.
    const juce::ValueTree settings = covered || hostProgramView == nullptr
                                       ? view->internalState
                                       : anamorph::InternalState::resolvedWithEdits (
                                             view->internalState, hostProgramView->internalState,
                                             hostProgramView->settingsEditGen, hostRestoreGen);
    writeState (*view, settings, destData);
}

// Read the blob and apply its SOUND; everything else it carries lands in `out`.
// Returns false when the input is not a restore at all, in which case nothing --
// not one parameter, not the Settings, not the A/B slots -- was touched.
bool AnamorphAudioProcessor::decodeRestore (const void* data, int sizeInBytes, RestoreDecode& d)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return false;

    auto root = juce::ValueTree::fromXml (*xml);
    if (root.hasType ("AnamorphRoot"))
    {
        auto params = root.getChildWithName (apvtsStateType);
        if (! params.isValid())
        {
            // An `AnamorphRoot` with no `ANAMORPH` child restores NO SOUND -- and
            // everything below this point is metadata that describes a sound. Adopting
            // it would relabel the sound the user currently has with the incoming
            // session's preset name, indicator tick and dirty baseline, and hand it the
            // incoming Settings, while not one parameter moved. That is the same
            // "metadata describing a session that was never loaded" the foreign-root
            // branch at the bottom of this function returns to avoid, and the registry
            // states the rule for both: input we do not recognise never becomes state
            // (SERIALIZATION_REGISTRY.md, "A chunk of neither recognised shape is not a
            // restore at all"). A root missing its only sound-bearing child is that
            // case wearing a recognised tag.
            //
            // Every session this plug-in has ever written carries the child --
            // getStateInformation appends it unconditionally -- so no valid session
            // reaches here and none changes behaviour. What reaches here is a
            // truncated, hand-edited or forward-version blob.
            return false;
        }

        // The token this restore's OWN sound install was handed (§13). Taken from the
        // call rather than read back from the counter afterwards: a wholesale
        // replacement that lands between the two -- an A/B apply, a preset load, an
        // undo on the message thread while this decode runs on a host thread -- would
        // otherwise be recorded as this restore's own, and the adoption would then take
        // that replacement's sound for the restored session's and pair it with the
        // restored metadata. The seam below is where a test lands exactly that.
        d.soundSetGen  = applySoundTree (params);
        if (seams.afterRestoreSoundApplied) seams.afterRestoreSoundApplied();
        d.soundParams  = params;
        // ...and THIS restore's clean baseline, for a session that carries none of its own
        // (§22). From the tree, not from the parameters: a live read here would describe
        // whatever the seam above -- or a real replacement landing in the same window -- had
        // just installed, and on the message thread it would also absorb an automation write.
        d.restoredSoundSig = anamorph::PresetManager::soundSignatureAfterLoading (apvts, params);

        // The host-hidden Settings / view state (Oversampling, UI Scale, Persistence,
        // Tooltips, Animations, Show Meters), RESOLVED here and written by the message
        // thread. Pre-0.8.4 sessions have no ANAMORPH_INTERNAL child (these were APVTS
        // params back then) -- resolve from the saved APVTS state so the user's choices
        // survive upgrade.
        if (auto internalState = root.getChildWithName ("ANAMORPH_INTERNAL"); internalState.isValid())
            d.internalResolved = anamorph::InternalState::resolveRestore (internalState);
        else
            d.internalResolved = anamorph::InternalState::resolveLegacy (params);

        d.restoredName = root.getProperty ("presetName").toString();
        d.haveName     = root.hasProperty ("presetName");
        // Absent (pre-0.9.2), empty or unrecognised all decode to `unknown`, i.e. the name
        // fallback this build already used. Metadata only -- the parameters above are already
        // restored at this point and are not touched by anything below.
        d.restoredSelection = readSelection (root, "presetSource", "presetFactoryId", "presetUserFile");
        if (root.hasProperty ("presetBaseline"))
        {
            d.restoredBaseline = root.getProperty ("presetBaseline").toString();
            d.haveBaseline = true;
        }

        auto ab = root.getChildWithName ("AB");
        if (ab.isValid())
        {
            // Clamp on restore: a hand-edited / corrupted / forward-version blob can carry an
            // out-of-range "active"; abSlot[]/abUndo[] are size-2, so an unclamped index would be
            // an out-of-bounds access (anamorph::kNumAbSlots). Valid states (0/1) are unchanged.
            d.abActive = anamorph::clampAbSlotIndex ((int) ab.getProperty ("active", 0));
            auto readSlot = [&ab, expectedType = apvtsStateType]
                            (StateSet& dst,
                                   const char* pk, const char* nk, const char* bk,
                                   const char* sk, const char* fk, const char* uk,
                                   const char* legacyKey)
            {
                // Start from the DEFAULT slot and overlay only what this AB node actually
                // carries. abSlot[] are processor members that a host may restore into
                // repeatedly on ONE live instance, so every absent field has to mean its
                // default rather than "whatever the previous session left here" -- and that
                // has to hold for the slot as a WHOLE, not field by field. Resetting first is
                // what keeps the two halves of a slot from coming out of two different
                // projects: a blob whose AB node exists but carries no params payload for this
                // slot (hand-edited, truncated, or the payload present but unparsable) would
                // otherwise keep the previous restore's SOUND while its name, baseline and
                // identity were reset around it.
                //
                // The default for the params is not an empty tree but "lazily initialised from
                // current" (SERIALIZATION_REGISTRY.md, `AB` child), and an INVALID tree is
                // already how this processor spells that: StateSet::isValid() is
                // params.isValid(), and abEnsureInit() re-seeds an invalid slot from
                // currentStateSet() before anything can read it. So the slot comes back seeded
                // from the state that was just restored -- sound and metadata from one project.
                //
                dst = {};
                // (The slot's remembered Level-Match gain resets with it, in adoptRestoreTail,
                // for the whole set at once -- round 16, ER-STATE-20.)
                //
                // Accept a parsed payload only when it is an APVTS tree of the
                // live type ("ANAMORPH"). A parsable-but-foreign-typed payload
                // is corrupt state exactly like an unparsable one -- and worse
                // if admitted: applying that slot would replaceState() the
                // foreign type into the live APVTS (JUCE has no type check
                // either), after which every later save writes a foreign-typed
                // params child that a fresh instance's restore (which looks up
                // getChildWithName(apvtsStateType)) silently skips --
                // delayed, silent loss of all 36 parameters. Wrong type ->
                // the slot stays invalid and abEnsureInit re-seeds it, the
                // same recovery the unparsable case already gets.
                auto adoptIfAnamorph = [&] (const juce::String& slotPayload)
                {
                    if (auto x = juce::parseXML (slotPayload))
                        if (auto t = juce::ValueTree::fromXml (*x); t.hasType (expectedType))
                            dst.params = t;
                };
                if (ab.hasProperty (pk))
                    adoptIfAnamorph (ab.getProperty (pk).toString());
                else if (ab.hasProperty (legacyKey)) // pre-0.6.4 slots: params only
                    adoptIfAnamorph (ab.getProperty (legacyKey).toString());

                // The metadata reads sit OUTSIDE the params branch on purpose: that is
                // what makes the rule hold for the pre-0.6.4 shape too, which carries
                // params ALONE -- otherwise the previous session's preset name and dirty
                // baseline stay attached to freshly restored parameters. All of these
                // defaults are the ones SERIALIZATION_REGISTRY.md already records.
                //
                // A round-11 review asked whether a REJECTED payload leaves this metadata
                // attached to the reseeded sound. It does not, and the reason is worth
                // stating so the question is not reopened: StateSet::isValid() is
                // params.isValid(), so a rejected payload leaves the slot invalid, and
                // abEnsureInit() then assigns `slot = currentStateSet()` -- the WHOLE
                // struct, metadata included, not just the params. Every reader of
                // abSlot[] (abSwitchTo, abCopyToOther, getStateInformation) calls
                // abEnsureInit first, so the values written here are unreachable in that
                // case. Measured: gating these three reads on dst.params.isValid()
                // changes no test outcome. State test 27 pins the contract.
                dst.selection = readSelection (ab, sk, fk, uk);
                dst.name      = ab.getProperty (nk).toString();
                dst.baseline  = ab.getProperty (bk).toString();
            };
            readSlot (d.abSlot[0], "slotAParams", "slotAName", "slotABase",
                      "slotASource", "slotAFactoryId", "slotAUserFile", "slotA");
            readSlot (d.abSlot[1], "slotBParams", "slotBName", "slotBBase",
                      "slotBSource", "slotBFactoryId", "slotBUserFile", "slotB");
        }
        else
        {
            // No `AB` node: the whole block above is skipped, so nothing had reset
            // the slots or the active index. `AB` is optional (registry: every field
            // in it is "Required: No"), and a root without one is exactly the
            // "absent" case readSlot's rule is written for -- it just cannot reach
            // it from in there. Same answer, applied to the slot set as a whole: the
            // decode's own defaults are the documented ones (`active` -> 0, both slots
            // -> invalid, i.e. "lazily initialised from current" -- SERIALIZATION_
            // REGISTRY.md's `AB` table), and adoptRestoreTail resets the Level-Match
            // memory with them. On a REUSED instance (hosts restore into one live
            // processor repeatedly) the pre-round-12 code left `abSlot[]` and
            // `abActive` holding the PREVIOUS project's values here, so the next A/B
            // switch recalled the previous project's sound underneath the restored
            // one. Measured before the reset existed: after a v0.2 restore, switching
            // to B played the previous project's B (raw width 0.10 against a restored
            // 0.75), and with the previous project left active on B the first switch
            // read its A (0.90) and its `active` index survived too --
            // `--legacy-ab-probe`. A FRESH instance was not exempt: the constructor
            // seeds both slots eagerly, so without the reset they kept the open/Default
            // snapshot instead of the restored session (State test 26 leg 3).
            // Invalidating rather than seeding is what makes this correct at this
            // point in the restore: abEnsureInit() re-seeds from currentStateSet() at
            // first use, which is after the restore has finished.
        }
    }
    else if (xml->hasTagName (apvtsStateType)) // backward-compat (v0.2)
    {
        auto legacy = juce::ValueTree::fromXml (*xml);
        d.soundSetGen  = applySoundTree (legacy);   // this restore's own token (§13)
        if (seams.afterRestoreSoundApplied) seams.afterRestoreSoundApplied();
        d.soundParams  = legacy;
        d.restoredSoundSig = anamorph::PresetManager::soundSignatureAfterLoading (apvts, legacy);   // §22

        // A v0.2 session is older than 0.8.4, so it can only carry the host-hidden Settings the
        // way pre-0.8.4 sessions do: as APVTS params, or not at all. Same resolver the AnamorphRoot
        // branch uses for that vintage -- and needed for the same reason readSlot resets the A/B
        // slots first: `internal` is a processor member a host restores into repeatedly on ONE
        // live instance, so without this the previous project's Oversampling, UI Scale,
        // Persistence, Meters, Tooltips and Animations stay in force underneath a v0.2 sound.
        // resolveLegacy resolves all six unconditionally, so absent ones reset to default
        // rather than being inherited.
        d.internalResolved = anamorph::InternalState::resolveLegacy (legacy);

        // ...and the A/B slots need the same treatment for the same reason, which
        // fixing `internal` does NOT also fix: they are a separate pair of processor
        // members, and a v0.2 session predates the A/B feature, so it can carry no
        // slot data to overwrite them with. Without this the previous project's A
        // and B sounds stayed loaded underneath a v0.2 restore and came back on the
        // next slot switch. The decode's defaults ARE that reset (see the no-`AB`
        // branch above).
    }
    else
    {
        // Neither shape: a foreign or forward-version chunk. NOTHING above ran -- not one
        // parameter, not the Settings, not the A/B slots -- so the sound in force is still the
        // one the user had, and nothing below may claim otherwise. Everything after this point
        // ADOPTS: it clears the undo history for the "new session" and writes the restored
        // preset name, identity and baseline into the manager. Running that with nothing
        // restored relabels the live sound (with no `presetName` to read it would resolve to
        // defaultName()), drops the identity to `unknown` so the drop-down ticks whatever
        // shares the label, and re-baselines the dirty-star -- metadata describing a session
        // that was never loaded.
        //
        // Returning is the same answer the guard at the top of this function already gives an
        // unparsable blob, which is the same situation one layer down: input we do not
        // recognise is not a restore.
        return false;
    }

    return true;
}

void AnamorphAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (onMessageThreadOrNoMessageManager())
    {
        // The owner's thread. The pending restore is adopted BEFORE this one is decoded
        // (D-2 round 4): the decode applies the new session's SOUND as it runs, and an
        // adoption that followed it would publish the older session's metadata while the
        // newer session's parameters were already live -- one snapshot describing two
        // sessions. Draining first, the older restore is adopted whole against its own
        // sound, and the new session's sound and metadata then land together. A blob
        // that turns out not to be a restore still changes nothing of its own; the drain
        // it triggered is an adoption this thread owed anyway and the 20 Hz timer would
        // have run a moment later.
        adoptPendingHostState();

        RestoreDecode d;
        if (! decodeRestore (data, sizeInBytes, d))
            return;

        {
            const juce::ScopedValueSetter<bool> adopting (adoptingRestore, true);
            adoptRestoreTail (d);
        }
        publishProgram();
    }
    else
    {
        // See `offThreadStateCalls`: the host must not have another off-message-thread
        // state call in flight. Counted, asserted in debug, never blocking.
        const OffThreadStateCall serialized { *this };

        RestoreDecode d;
        if (! decodeRestore (data, sizeInBytes, d))
            return;

        // Another thread (the macOS AU autosave shape, pluginval's AU background-
        // thread state test, an out-of-spec VST3 host): the sound is already applied
        // above. Two things happen HERE, synchronously, because a prepareToPlay that
        // follows on this same thread -- the ordinary setState-then-activate order --
        // reads them: the engine-config word (the oversampling), and the latency request at
        // the bottom. Everything else is handed to the message thread as one immutable
        // value, and this side keeps its own view of it for a save that arrives
        // before the adoption.
        // This restore's generation: this side's own count, monotonic. It tags the
        // engine-config publication (so no older restore's completion can overwrite
        // it), rides inside the decode to the message thread, and is what this
        // side's next save compares the published snapshot's generation against.
        const auto generation = ++hostRestoreGen;
        internal.publishEngineConfig (d.internalResolved, generation);
        hostRestoreView = viewOfRestore (d);

        auto* handoff = new RestoreDecode (d);
        handoff->generation = generation;
        if (seams.beforeRestorePut) seams.beforeRestorePut();
        pendingRestore.put (handoff);   // frees an older restore the message thread never took
    }

    // Re-derive the reported latency from the state that ACTUALLY ended up live.
    //
    // Unconditional on purpose: a restore is exactly the moment the reported
    // latency has to match the live state, and correctness here must not depend
    // on which of the paths above happened to notify. Before D-2 this also covered
    // a poisoned latency-bearing value that `apvts.replaceState` had adopted
    // clamped and `reassertParameters` then repaired without notifying anyone
    // (measured: restoring drive value="inf" left the host told 4 samples while the
    // repaired state predicts 0, on the SECOND restore -- the first was correct by
    // the var-type coincidence recorded for ER-STATE-07); the text repair now runs
    // before replaceState, so that path no longer exists, and this stays as the
    // restore's own re-report. Off the message thread it is a request the 20 Hz
    // timer serves (D-1), reading the engine-config word published above.
    requestLatencyUpdate();
}

// ----------------------------------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AnamorphAudioProcessor();
}
