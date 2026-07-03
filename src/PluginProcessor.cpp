#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "AbSlotIndex.h"

AnamorphAudioProcessor::AnamorphAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ANAMORPH", createAnamorphLayout()) // custom undo, not APVTS's
{
    params.bind (apvts);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (pid::bypass));

    // Parameters that change the reported PDC latency. Oversampling is no longer an APVTS
    // parameter (it lives in InternalState), so its PDC update is driven by a callback.
    apvts.addParameterListener (pid::drive,      this);
    apvts.addParameterListener (pid::algorithm,  this);
    internal.onOversampleChanged = [this] { updateLatency(); };

    // Observe begin/end GESTURES on the SOUND params so a whole drag folds into ONE undo step and
    // host automation (which never opens a gesture) is excluded from undo. View params are skipped.
    for (auto* p : getParameters())
        if (auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            if (! pid::isViewParam (wid->paramID))
                p->addListener (this);

    syncCommitted(); // establish the undo baseline
}

AnamorphAudioProcessor::~AnamorphAudioProcessor()
{
    apvts.removeParameterListener (pid::drive,      this);
    apvts.removeParameterListener (pid::algorithm,  this);
    for (auto* p : getParameters())
        p->removeListener (this);
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
    engine.prepare (sampleRate, samplesPerBlock);
    engine.setParameters (params.toEngine (internal.oversampleIndex()));
    updateLatency();
}

void AnamorphAudioProcessor::updateLatency()
{
    setLatencySamples (engine.predictLatency (params.toEngine (internal.oversampleIndex())));
}

void AnamorphAudioProcessor::parameterChanged (const juce::String&, float)
{
    // Recompute PDC on the message thread without touching the audio-thread
    // engine state (predictLatency is const and race-free).
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

    if (auto* match = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (pid::autoGainMatch)))
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
    return { copyStateWithRawValues(), presets.currentName(), presets.baseline() };
}

// Restore a state set: parameters (keeping the shared view params) AND the
// preset metadata, so the name + dirty-star reappear exactly as stored (#6).
void AnamorphAudioProcessor::applyStateSet (const StateSet& s)
{
    applyStatePreservingView (s.params);
    presets.setMeta (s.name, s.baseline);
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
    reassertParameters (target);

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

// Synchronously force every parameter to its restored value, from the just-restored tree:
//  1. A wholesale apvts.replaceState() does not reliably push every parameter's value through to
//     its cached/atomic getValue() synchronously (some params kept their PRE-restore value).
//  2. APVTS stores the DENORMALISED (snapped) value; for discrete params the saved "raw" attribute
//     (see getStateInformation) carries the EXACT normalised getValue() pluginval set, so the
//     round-trip is bit-faithful and passes its 0.1 raw-value tolerance.
// Prefer "raw" (exact); fall back to the denormalised "value" for legacy sessions that lack it.
// Idempotent: parameters already at the target value are left untouched (no spurious host notify).
void AnamorphAudioProcessor::reassertParameters (const juce::ValueTree& restoredApvtsTree)
{
    if (! restoredApvtsTree.isValid()) return;

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (auto node = restoredApvtsTree.getChildWithProperty ("id", rp->paramID); node.isValid())
            {
                const float norm = node.hasProperty ("raw")
                    ? juce::jlimit (0.0f, 1.0f, (float) node.getProperty ("raw"))
                    : juce::jlimit (0.0f, 1.0f, rp->convertTo0to1 ((float) node.getProperty ("value",
                                                                   rp->convertFrom0to1 (rp->getValue()))));
                if (std::abs (norm - rp->getValue()) > 1.0e-6f)
                    rp->setValueNotifyingHost (norm);
            }
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

void AnamorphAudioProcessor::undo()
{
    auto& st = abUndo[abActive];
    if (st.undo.empty()) return;
    engine.requestDuck(); // mask the level jump (#1, 0.6.4)
    st.redo.push_back (currentStateSet());
    committed = st.undo.back(); st.undo.pop_back();
    applyStateSet (committed);
    committedSig = soundSignature();
    lastPolledSig = committedSig;
}

void AnamorphAudioProcessor::redo()
{
    auto& st = abUndo[abActive];
    if (st.redo.empty()) return;
    engine.requestDuck(); // mask the level jump (#1, 0.6.4)
    st.undo.push_back (currentStateSet());
    committed = st.redo.back(); st.redo.pop_back();
    applyStateSet (committed);
    committedSig = soundSignature();
    lastPolledSig = committedSig;
}

// ----------------------------------------------------------------------------
//  A/B compare
// ----------------------------------------------------------------------------
void AnamorphAudioProcessor::abEnsureInit()
{
    if (! abSlot[0].isValid()) abSlot[0] = currentStateSet();
    if (! abSlot[1].isValid())
    {
        abSlot[1] = abSlot[0];
        abSlot[1].params = abSlot[0].params.createCopy(); // independent tree
    }
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

void AnamorphAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    abEnsureInit();
    juce::ValueTree root ("AnamorphRoot");
    root.setProperty ("presetName", presets.currentName(), nullptr);     // remembered across sessions (F2)
    root.setProperty ("presetBaseline", presets.baseline(), nullptr);    // so the dirty-star survives reload (#6)
    root.appendChild (copyStateWithRawValues(), nullptr); // APVTS state + exact "raw" values per PARAM
    root.appendChild (internal.copyState(), nullptr); // host-hidden Settings / view state
    juce::ValueTree ab ("AB");
    ab.setProperty ("active", abActive, nullptr);
    // Each slot carries its params AND its preset name + baseline (#6).
    ab.setProperty ("slotAParams", abSlot[0].params.toXmlString(), nullptr);
    ab.setProperty ("slotAName",   abSlot[0].name, nullptr);
    ab.setProperty ("slotABase",   abSlot[0].baseline, nullptr);
    ab.setProperty ("slotBParams", abSlot[1].params.toXmlString(), nullptr);
    ab.setProperty ("slotBName",   abSlot[1].name, nullptr);
    ab.setProperty ("slotBBase",   abSlot[1].baseline, nullptr);
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
    bool haveBaseline = false;
    if (root.hasType ("AnamorphRoot"))
    {
        auto params = root.getChildWithName (apvts.state.getType());
        if (params.isValid())
        {
            apvts.replaceState (params.createCopy());
            reassertParameters (params); // force every parameter through synchronously (see below)
        }

        // Restore the host-hidden Settings / view state (Oversampling, Window Size,
        // Persistence, Tooltips, Animations, Show Meters). A changed Oversampling fires
        // InternalState's callback -> updateLatency(); prepareToPlay re-asserts it anyway.
        // Pre-0.8.4 sessions have no ANAMORPH_INTERNAL child (these were APVTS params back
        // then) -- migrate from the saved APVTS state so the user's choices survive upgrade.
        if (auto internalState = root.getChildWithName ("ANAMORPH_INTERNAL"); internalState.isValid())
            internal.restoreState (internalState);
        else
            internal.migrateFromLegacyApvts (params);

        restoredName = root.getProperty ("presetName").toString();
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
            auto readSlot = [&ab] (StateSet& dst, const char* pk, const char* nk, const char* bk,
                                   const char* legacyKey)
            {
                if (ab.hasProperty (pk))
                {
                    if (auto x = juce::parseXML (ab.getProperty (pk).toString()))
                        dst.params = juce::ValueTree::fromXml (*x);
                    dst.name     = ab.getProperty (nk).toString();
                    dst.baseline = ab.getProperty (bk).toString();
                }
                else if (ab.hasProperty (legacyKey)) // pre-0.6.4 slots: params only
                {
                    if (auto x = juce::parseXML (ab.getProperty (legacyKey).toString()))
                        dst.params = juce::ValueTree::fromXml (*x);
                }
            };
            readSlot (abSlot[0], "slotAParams", "slotAName", "slotABase", "slotA");
            readSlot (abSlot[1], "slotBParams", "slotBName", "slotBBase", "slotB");
        }
    }
    else if (xml->hasTagName (apvts.state.getType())) // backward-compat (v0.2)
    {
        auto legacy = juce::ValueTree::fromXml (*xml);
        apvts.replaceState (legacy);
        reassertParameters (legacy);
    }

    // Fresh session: clear undo history.
    abUndo[0] = {}; abUndo[1] = {};

    // Adopt the remembered preset name + baseline so the dirty-star is reproduced
    // (#6); fall back to a clean baseline at the restored state when absent.
    if (haveBaseline) presets.setMeta (restoredName.isNotEmpty() ? restoredName : presets.currentName(),
                                       restoredBaseline);
    else              presets.adoptRestoredState (restoredName);

    syncCommitted();
}

// ----------------------------------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AnamorphAudioProcessor();
}
