#include "PluginProcessor.h"
#include "PluginEditor.h"

AnamorphAudioProcessor::AnamorphAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ANAMORPH", createAnamorphLayout()) // custom undo, not APVTS's
{
    params.bind (apvts);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (pid::bypass));

    // Parameters that change the reported PDC latency.
    apvts.addParameterListener (pid::oversample, this);
    apvts.addParameterListener (pid::drive,      this);
    apvts.addParameterListener (pid::algorithm,  this);

    syncCommitted(); // establish the undo baseline
}

AnamorphAudioProcessor::~AnamorphAudioProcessor()
{
    apvts.removeParameterListener (pid::oversample, this);
    apvts.removeParameterListener (pid::drive,      this);
    apvts.removeParameterListener (pid::algorithm,  this);
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
    engine.setParameters (params.toEngine());
    updateLatency();
}

void AnamorphAudioProcessor::updateLatency()
{
    setLatencySamples (engine.predictLatency (params.toEngine()));
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

    // Reset the meter peak-hold / clip latches when transport (re)starts (#15).
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            playing = pos->getIsPlaying();
    if (playing && ! prevPlaying)
        engine.getLevels().resetHold();
    prevPlaying = playing;

    engine.setTransportPlaying (playing); // a pause edge kills Velvet's noise tail (#4)
    engine.setParameters (params.toEngine());
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
    committedState = apvts.copyState();
    committedSig = soundSignature();
    lastPolledSig = committedSig;
}

void AnamorphAudioProcessor::applyStatePreservingView (const juce::ValueTree& target)
{
    // Restore a snapshot but keep the CURRENT shared view/Settings params (#10/#13).
    float saved[std::size (pid::viewParams)];
    for (size_t i = 0; i < std::size (pid::viewParams); ++i)
        saved[i] = apvts.getParameter (pid::viewParams[i])->getValue();

    apvts.replaceState (target.createCopy());

    for (size_t i = 0; i < std::size (pid::viewParams); ++i)
        apvts.getParameter (pid::viewParams[i])->setValueNotifyingHost (saved[i]);
}

void AnamorphAudioProcessor::pollUndoCoalesce()
{
    const auto sig = soundSignature();
    // Commit only once a sound edit has SETTLED (signature stable for a tick),
    // folding a whole knob gesture into a single undo step.
    if (sig != committedSig && sig == lastPolledSig)
    {
        abUndo[abActive].undo.push_back (committedState);
        if (abUndo[abActive].undo.size() > 128) abUndo[abActive].undo.erase (abUndo[abActive].undo.begin());
        abUndo[abActive].redo.clear();
        committedState = apvts.copyState();
        committedSig = sig;
    }
    lastPolledSig = sig;
}

void AnamorphAudioProcessor::undo()
{
    auto& st = abUndo[abActive];
    if (st.undo.empty()) return;
    st.redo.push_back (apvts.copyState());
    auto target = st.undo.back(); st.undo.pop_back();
    applyStatePreservingView (target);
    syncCommitted();
}

void AnamorphAudioProcessor::redo()
{
    auto& st = abUndo[abActive];
    if (st.redo.empty()) return;
    st.undo.push_back (apvts.copyState());
    auto target = st.redo.back(); st.redo.pop_back();
    applyStatePreservingView (target);
    syncCommitted();
}

// ----------------------------------------------------------------------------
//  A/B compare
// ----------------------------------------------------------------------------
void AnamorphAudioProcessor::abEnsureInit()
{
    if (! abSlotA.isValid()) abSlotA = apvts.copyState();
    if (! abSlotB.isValid()) abSlotB = abSlotA.createCopy();
}

void AnamorphAudioProcessor::abApplySlot (int slot)
{
    // The "view" + "settings" params live in a SINGLE shared store: they are not
    // part of A/B and never swap (feedback #13 / #15). Same list as undo/presets.
    applyStatePreservingView (slot == 1 ? abSlotB : abSlotA);
}

void AnamorphAudioProcessor::abSwitchTo (int slot)
{
    abEnsureInit();
    if (slot == abActive) return;
    (abActive == 1 ? abSlotB : abSlotA) = apvts.copyState(); // store edits in the old slot
    abMatchGain[abActive] = engine.getMatchGainDb();         // remember this slot's match (#23)
    abActive = slot;
    abApplySlot (slot);
    engine.injectMatchGainDb (abMatchGain[slot]);            // restore the new slot's match (#23)
    syncCommitted();                                         // the switch itself isn't undoable (#11)
}

void AnamorphAudioProcessor::abCopyToOther()
{
    abEnsureInit();
    (abActive == 1 ? abSlotB : abSlotA) = apvts.copyState();
    const int other = abActive == 1 ? 0 : 1;
    // Record the target slot's pre-copy state so undoing on that slot reverts the
    // Copy without disturbing the active slot's history (#12).
    abUndo[other].undo.push_back ((other == 1 ? abSlotB : abSlotA).createCopy());
    abUndo[other].redo.clear();
    (other == 1 ? abSlotB : abSlotA) = apvts.copyState().createCopy();
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
    root.setProperty ("presetName", presets.currentName(), nullptr); // remembered across sessions (F2)
    root.appendChild (apvts.copyState(), nullptr);
    juce::ValueTree ab ("AB");
    ab.setProperty ("active", abActive, nullptr);
    ab.setProperty ("slotA", abSlotA.toXmlString(), nullptr);
    ab.setProperty ("slotB", abSlotB.toXmlString(), nullptr);
    root.appendChild (ab, nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void AnamorphAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;

    auto root = juce::ValueTree::fromXml (*xml);
    if (root.hasType ("AnamorphRoot"))
    {
        auto params = root.getChildWithName (apvts.state.getType());
        if (params.isValid()) apvts.replaceState (params.createCopy());

        auto ab = root.getChildWithName ("AB");
        if (ab.isValid())
        {
            abActive = (int) ab.getProperty ("active", 0);
            if (auto a = juce::parseXML (ab.getProperty ("slotA").toString())) abSlotA = juce::ValueTree::fromXml (*a);
            if (auto b = juce::parseXML (ab.getProperty ("slotB").toString())) abSlotB = juce::ValueTree::fromXml (*b);
        }
    }
    else if (xml->hasTagName (apvts.state.getType())) // backward-compat (v0.2)
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }

    // Fresh session: clear undo history and re-baseline.
    abUndo[0] = {}; abUndo[1] = {};
    syncCommitted();

    // Adopt the remembered preset name (clean baseline = the restored state).
    presets.adoptRestoredState (root.hasType ("AnamorphRoot")
                                    ? root.getProperty ("presetName").toString() : juce::String());
}

// ----------------------------------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AnamorphAudioProcessor();
}
