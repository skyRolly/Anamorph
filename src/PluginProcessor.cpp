#include "PluginProcessor.h"
#include "PluginEditor.h"

AnamorphAudioProcessor::AnamorphAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ANAMORPH", createAnamorphLayout())
{
    params.bind (apvts);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (pid::bypass));

    // Parameters that change the reported PDC latency.
    apvts.addParameterListener (pid::oversample, this);
    apvts.addParameterListener (pid::drive,      this);
    apvts.addParameterListener (pid::algorithm,  this);
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

    engine.setParameters (params.toEngine());
    engine.process (buffer);
}

// ----------------------------------------------------------------------------
void AnamorphAudioProcessor::applyAutoGain()
{
    // "Apply": lock the measured loudness-match gain into Output Gain as a fixed
    // value, then disengage Match so exports/playback stay consistent.
    const float matchDb = engine.getMatchGainDb();

    if (auto* og = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (pid::outputGain)))
    {
        const float current = og->get();
        const float target  = juce::jlimit (-24.0f, 24.0f, current + matchDb);
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
juce::AudioProcessorEditor* AnamorphAudioProcessor::createEditor()
{
    return new AnamorphAudioProcessorEditor (*this);
}

void AnamorphAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AnamorphAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// ----------------------------------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AnamorphAudioProcessor();
}
