#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "AbSlotIndex.h"

#include <cmath>   // std::isfinite -- the non-finite guards on the restore paths

AnamorphAudioProcessor::AnamorphAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ANAMORPH", createAnamorphLayout()) // custom undo, not APVTS's
{
    params.bind (apvts);
    // Bypass is a custom RangedAudioParameter subclass (raw-value round-trip), so it is no longer
    // an AudioParameterBool -- take the base pointer directly for getBypassParameter().
    bypassParam = apvts.getParameter (pid::bypass);

    // Parameters that change the reported PDC latency. Oversampling is no longer an APVTS
    // parameter (it lives in InternalState), so its PDC update is driven by a callback.
    apvts.addParameterListener (pid::drive,      this);
    apvts.addParameterListener (pid::algorithm,  this);
    internal.onOversampleChanged = [this] { updateLatency(); };

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
    presets.onAboutToLoad = [this] { pollUndoCoalesce(); };
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

    syncCommitted(); // establish the undo baseline

    // Snapshot BOTH A/B slots to the open (Default) state up front. The slots are otherwise filled
    // LAZILY on the first A/B switch (abEnsureInit): editing A before ever visiting B would then make
    // B born as a copy of A's ALREADY-edited state -- the edit leaks into B, so B never shows the
    // open state. Eager init makes the two slots independent from open, deterministically (the lazy
    // path made "B == open state" depend on whether the host called getStateInformation early). The
    // switch/apply logic is unchanged; this only fixes WHEN the initial snapshot is taken.
    abEnsureInit();
}

AnamorphAudioProcessor::~AnamorphAudioProcessor()
{
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
    updateLatency();
}

void AnamorphAudioProcessor::updateLatency()
{
    setLatencySamples (engine.predictLatency (params.toEngine (internal.oversampleIndex())));
}

void AnamorphAudioProcessor::parameterChanged (const juce::String&, float)
{
    // Recompute PDC without touching the audio-thread engine state
    // (predictLatency is const and race-free). NOTE: this runs on whatever
    // thread changed the parameter -- under VST3 host AUTOMATION of
    // drive/algorithm that is the AUDIO thread, and when the reported latency
    // actually changes, setLatencySamples' notification chain takes locks and
    // (on a real change) allocates + write()s in the wrapper. Recorded as
    // KI-027; the fix (deferring off-thread delivery) is a threading-model
    // change gated on Architecture Review, so the comment states the fact
    // rather than the earlier "on the message thread" assumption.
    updateLatency();
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
                sig << juce::String (p->getValue(), 5) << ',';
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
    for (size_t i = 0; i < std::size (pid::viewParams); ++i)
        saved[i] = apvts.getParameter (pid::viewParams[i])->getValue();

    apvts.replaceState (target.createCopy());
    // Synchronously force every parameter to its exact (raw) value from the snapshot, so undo /
    // redo / A-B apply propagate exactly like host state restore -- replaceState alone can leave a
    // param at a stale/snapped value (see reassertParameters). View params are re-overridden below.
    reassertParameters (target, /*notifyHost*/ true); // undo/redo/A-B is editor-initiated: notify host+editor

    for (size_t i = 0; i < std::size (pid::viewParams); ++i)
        apvts.getParameter (pid::viewParams[i])->setValueNotifyingHost (saved[i]);
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
        // the same answer an ABSENT node already gets just below.
        //
        // This runs AFTER apvts.replaceState(), which is the actual ingress:
        // JUCE pushes @value through setDenormalisedValue -> setValueNotifyingHost
        // and its own approximatelyEqual guard is false for NaN. So this is a
        // REPAIR, not just a filter, and it is the one place that sees every
        // parameter on every restore path.
        if (! std::isfinite (norm))
            norm = rp->getDefaultValue();

        // Written as a negated <= so that a NaN on EITHER side counts as
        // "differs" and gets repaired. The plain `> 1e-6` this replaced is
        // false when either operand is NaN, which is exactly how a poisoned
        // parameter used to survive the pass meant to fix it.
        if (! (std::abs (norm - rp->getValue()) <= 1.0e-6f))
        {
            if (notifyHost)
                rp->setValueNotifyingHost (norm);
            else
            {
                rp->setValue (norm); // getValue() only -- no host / listener notification
                if (auto* atom = apvts.getRawParameterValue (rp->paramID))
                    atom->store (rp->convertFrom0to1 (norm)); // DSP value (snapped denormalised)
                if (! pid::isViewParam (rp->paramID))
                    silentSoundChange = true;
            }
        }
    };

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (auto node = restoredApvtsTree.getChildWithProperty ("id", rp->paramID); node.isValid())
            {
                const float norm = node.hasProperty ("raw")
                    ? juce::jlimit (0.0f, 1.0f, (float) node.getProperty ("raw"))
                    : juce::jlimit (0.0f, 1.0f, rp->convertTo0to1 ((float) node.getProperty ("value",
                                                                   rp->convertFrom0to1 (rp->getValue()))));
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

// Message thread. Count nested / overlapping gestures (e.g. the two-parameter Multiband band move
// opens gestures on both split params); request a single undo commit only after the LAST closes.
void AnamorphAudioProcessor::parameterGestureChanged (int, bool gestureIsStarting)
{
    if (gestureIsStarting)                       ++openGestures;
    else if (openGestures > 0 && --openGestures == 0) pendingGestureCommit = true;
}

void AnamorphAudioProcessor::pollUndoCoalesce()
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
    for (auto& slot : abSlot)
        if (! slot.isValid())
            slot = currentStateSet();
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
    slot = juce::jlimit (0, anamorph::kNumAbSlots - 1, slot); // defensive: never index out of bounds
    abEnsureInit();
    if (slot == abActive) return;
    engine.requestDuck();                              // mask the level jump (#1, 0.6.4)
    abSlot[abActive] = currentStateSet();              // store the whole state set in the old slot
    abMatchGain[abActive] = engine.getMatchGainDb();   // remember this slot's match (#23)
    abActive = slot;
    abApplySlot (slot);
    engine.injectMatchGainDb (abMatchGain[slot]);      // restore the new slot's match (#23)
    syncCommitted();                                   // the switch itself isn't undoable (#11)
}

void AnamorphAudioProcessor::abCopyToOther()
{
    abEnsureInit();
    abSlot[abActive] = currentStateSet();
    const int other = abActive == 1 ? 0 : 1;
    // Record the target slot's pre-copy state so undoing on that slot reverts the
    // Copy without disturbing the active slot's history (#12).
    abUndo[other].undo.push_back (abSlot[other]);
    abUndo[other].redo.clear();
    abSlot[other] = currentStateSet(); // overwrite the other slot with the FULL state set (#6)
}

// ----------------------------------------------------------------------------
juce::AudioProcessorEditor* AnamorphAudioProcessor::createEditor()
{
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

void AnamorphAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    abEnsureInit();
    juce::ValueTree root ("AnamorphRoot");
    root.setProperty ("presetName", presets.currentName(), nullptr);     // remembered across sessions (F2)
    root.setProperty ("presetBaseline", presets.baseline(), nullptr);    // so the dirty-star survives reload (#6)
    // Which preset row the indicator points at, so reopening a project puts the tick back
    // where it was even when a user preset shares a factory preset's NAME (ADR-0024
    // amendment). Three additive strings, always written; all-empty means "no identity",
    // which is exactly what a pre-0.9.2 session restores as. METADATA ONLY: the sound comes
    // from the APVTS child below and is restored identically whether or not these resolve.
    // Nothing here is written into a user preset FILE -- that format is untouched.
    writeSelection (root, presets.selection(), "presetSource", "presetFactoryId", "presetUserFile");
    root.appendChild (copyStateWithRawValues(), nullptr); // APVTS state + exact "raw" values per PARAM
    root.appendChild (internal.copyState(), nullptr); // host-hidden Settings / view state
    juce::ValueTree ab ("AB");
    ab.setProperty ("active", abActive, nullptr);
    // Each slot carries its params AND its preset name + baseline (#6) + its indicator identity.
    ab.setProperty ("slotAParams", abSlot[0].params.toXmlString(), nullptr);
    ab.setProperty ("slotAName",   abSlot[0].name, nullptr);
    ab.setProperty ("slotABase",   abSlot[0].baseline, nullptr);
    writeSelection (ab, abSlot[0].selection, "slotASource", "slotAFactoryId", "slotAUserFile");
    ab.setProperty ("slotBParams", abSlot[1].params.toXmlString(), nullptr);
    ab.setProperty ("slotBName",   abSlot[1].name, nullptr);
    ab.setProperty ("slotBBase",   abSlot[1].baseline, nullptr);
    writeSelection (ab, abSlot[1].selection, "slotBSource", "slotBFactoryId", "slotBUserFile");
    root.appendChild (ab, nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void AnamorphAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;

    auto root = juce::ValueTree::fromXml (*xml);
    juce::String restoredName, restoredBaseline;
    anamorph::PresetManager::Selection restoredSelection; // unknown unless the session carried one
    bool haveName = false, haveBaseline = false;          // property PRESENT, as opposed to non-empty
    if (root.hasType ("AnamorphRoot"))
    {
        auto params = root.getChildWithName (apvts.state.getType());
        if (params.isValid())
        {
            apvts.replaceState (params.createCopy());
            reassertParameters (params, /*notifyHost*/ false); // host restore: no host-notify (see below)
        }

        // Restore the host-hidden Settings / view state (Oversampling, UI Scale,
        // Persistence, Tooltips, Animations, Show Meters). A changed Oversampling fires
        // InternalState's callback -> updateLatency(); prepareToPlay re-asserts it anyway.
        // Pre-0.8.4 sessions have no ANAMORPH_INTERNAL child (these were APVTS params back
        // then) -- migrate from the saved APVTS state so the user's choices survive upgrade.
        if (auto internalState = root.getChildWithName ("ANAMORPH_INTERNAL"); internalState.isValid())
            internal.restoreState (internalState);
        else
            internal.migrateFromLegacyApvts (params);

        restoredName = root.getProperty ("presetName").toString();
        haveName     = root.hasProperty ("presetName");
        // Absent (pre-0.9.2), empty or unrecognised all decode to `unknown`, i.e. the name
        // fallback this build already used. Metadata only -- the parameters above are already
        // restored at this point and are not touched by anything below.
        restoredSelection = readSelection (root, "presetSource", "presetFactoryId", "presetUserFile");
        if (root.hasProperty ("presetBaseline"))
        {
            restoredBaseline = root.getProperty ("presetBaseline").toString();
            haveBaseline = true;
        }

        auto ab = root.getChildWithName ("AB");
        if (ab.isValid())
        {
            // Clamp on restore: a hand-edited / corrupted / forward-version blob can carry an
            // out-of-range "active"; abSlot[]/abUndo[] are size-2, so an unclamped index would be
            // an out-of-bounds access (anamorph::kNumAbSlots). Valid states (0/1) are unchanged.
            abActive = anamorph::clampAbSlotIndex ((int) ab.getProperty ("active", 0));
            auto readSlot = [&ab, expectedType = apvts.state.getType()]
                            (StateSet& dst, const char* pk, const char* nk, const char* bk,
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
                // The metadata reads below sit outside the params branch on purpose: that is
                // what makes the rule hold for the pre-0.6.4 shape too, which carries params
                // ALONE -- otherwise the previous session's preset name and dirty baseline stay
                // attached to freshly restored parameters. All of these defaults are the ones
                // SERIALIZATION_REGISTRY.md already records for these fields.
                dst = {};
                dst.selection = readSelection (ab, sk, fk, uk);
                dst.name      = ab.getProperty (nk).toString();
                dst.baseline  = ab.getProperty (bk).toString();
                // Accept a parsed payload only when it is an APVTS tree of the
                // live type ("ANAMORPH"). A parsable-but-foreign-typed payload
                // is corrupt state exactly like an unparsable one -- and worse
                // if admitted: applying that slot would replaceState() the
                // foreign type into the live APVTS (JUCE has no type check
                // either), after which every later save writes a foreign-typed
                // params child that a fresh instance's restore (which looks up
                // getChildWithName(apvts.state.getType())) silently skips --
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
            };
            readSlot (abSlot[0], "slotAParams", "slotAName", "slotABase",
                      "slotASource", "slotAFactoryId", "slotAUserFile", "slotA");
            readSlot (abSlot[1], "slotBParams", "slotBName", "slotBBase",
                      "slotBSource", "slotBFactoryId", "slotBUserFile", "slotB");
        }
    }
    else if (xml->hasTagName (apvts.state.getType())) // backward-compat (v0.2)
    {
        auto legacy = juce::ValueTree::fromXml (*xml);
        apvts.replaceState (legacy);
        reassertParameters (legacy, /*notifyHost*/ false); // legacy host restore: no host-notify

        // A v0.2 session is older than 0.8.4, so it can only carry the host-hidden Settings the
        // way pre-0.8.4 sessions do: as APVTS params, or not at all. Same call the AnamorphRoot
        // branch makes for that vintage -- and needed for the same reason readSlot resets the A/B
        // slots first: `internal` is a processor member a host restores into repeatedly on ONE
        // live instance, so without this the previous project's Oversampling, UI Scale,
        // Persistence, Meters, Tooltips and Animations stay in force underneath a v0.2 sound.
        // migrateFromLegacyApvts writes all six unconditionally, so absent ones reset to default
        // rather than being inherited.
        internal.migrateFromLegacyApvts (legacy);
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
        return;
    }

    // Fresh session: clear undo history.
    abUndo[0] = {}; abUndo[1] = {};

    // Adopt the remembered preset name + baseline so the dirty-star is reproduced
    // (#6); fall back to a clean baseline at the restored state when absent.
    //
    // ABSENT and EMPTY are different answers, and only this scope can tell them apart -- the
    // same distinction `haveBaseline` already draws for the sibling field. `presetName` is
    // absent only in a session that predates it (< 0.6): there the label is the one a fresh
    // manager carries, and it has to be that CONSTANT rather than presets.currentName(), which
    // is whatever the PREVIOUS project left on this instance (hosts reuse one processor across
    // setStateInformation calls -- the same rule readSlot follows for the A/B slots above).
    // A present-but-EMPTY presetName is a real value meaning "this state has no preset"; since
    // 0.9.2 a session saved while sitting on a nameless A/B slot stores exactly that, so it is
    // adopted verbatim and must never be turned back into a name.
    const auto adoptedName = haveName ? restoredName : anamorph::PresetManager::defaultName();
    if (haveBaseline) presets.setMeta (adoptedName, restoredBaseline, restoredSelection);
    else              presets.adoptRestoredState (adoptedName, restoredSelection);

    syncCommitted();
}

// ----------------------------------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AnamorphAudioProcessor();
}
