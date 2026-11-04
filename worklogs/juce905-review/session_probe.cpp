#include "PluginProcessor.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdint>

// getValue() below records normalized 0..1 parameter bits. The byte-identical
// state re-save is a separate serialization check; this listing does not
// independently measure restored unnormalized values.
static void describe (AnamorphAudioProcessor& processor)
{
    for (int slot = 0; slot != 2; ++slot)
    {
        processor.abSwitchTo (slot);
        std::printf ("slot=%d name=%s dirty=%d\n", slot,
                     processor.getPresets().currentName().toRawUTF8(), processor.getPresets().isDirty());
        for (auto* parameter : processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                std::printf ("%s=%08x\n", ranged->paramID.toRawUTF8(),
                             std::bit_cast<std::uint32_t> (ranged->getValue()));
        for (const double rate : { 44100.0, 48000.0, 96000.0 })
        {
            processor.setNonRealtime (true);
            processor.setRateAndBufferSizeDetails (rate, 256);
            processor.prepareToPlay (rate, 256);
            juce::AudioBuffer<float> audio (2, 256);
            juce::MidiBuffer midi;
            std::uint64_t hash = 14695981039346656037ULL;
            for (int block = 0; block != 150; ++block)
            {
                for (int channel = 0; channel != 2; ++channel)
                    for (int sample = 0; sample != 256; ++sample)
                        audio.setSample (channel, sample,
                            0.4f * (float) std::sin ((double) (block * 256 + sample)
                                                     * (channel == 0 ? 0.03 : 0.047)));
                processor.processBlock (audio, midi);
                for (int channel = 0; channel != 2; ++channel)
                    for (int sample = 0; sample != 256; ++sample)
                        for (unsigned char byte : std::bit_cast<std::array<unsigned char, 4>> (
                                 audio.getSample (channel, sample)))
                            hash = (hash ^ byte) * 1099511628211ULL;
            }
            std::printf ("render %.0f latency=%d hash=%016llx\n", rate,
                         processor.getLatencySamples(), (unsigned long long) hash);
            processor.releaseResources();
        }
    }
}

int main (int argc, char** argv)
{
    if (argc != 3)
        return 2;
    juce::ScopedJuceInitialiser_GUI initialiser;
    const juce::String mode (argv[1]);
    const juce::File file (argv[2]);
    AnamorphAudioProcessor processor;
    juce::MemoryBlock state;
    if (mode == "write")
    {
        (void) processor.getPresets().load (4);
        processor.abCopyToOther();
        processor.abSwitchTo (1);
        (void) processor.getPresets().load (8);
        int index = 0;
        for (auto* parameter : processor.getParameters())
            parameter->setValueNotifyingHost ((float) (++index % 11) / 11.0f);
        processor.getAPVTS().getParameter (pid::bypass)->setValueNotifyingHost (0.0f);
        auto internal = processor.getInternal().copyState();
        internal.setProperty (anamorph::iid::oversample, 2, nullptr);
        internal.setProperty (anamorph::iid::uiScale, 4, nullptr);
        internal.setProperty (anamorph::iid::scopePersist, 0.73, nullptr);
        internal.setProperty (anamorph::iid::metersOn, true, nullptr);
        internal.setProperty (anamorph::iid::tooltipsOn, true, nullptr);
        internal.setProperty (anamorph::iid::uiAnimations, false, nullptr);
        processor.getInternal().restoreState (internal);
        processor.getStateInformation (state);
        // Exercise XML-special and Unicode metadata through the real reader.
        auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
        auto* slots = xml->getChildByName ("AB");
        const auto label = juce::String::fromUTF8 ("升级 & <Recall> \"A/B\"");
        xml->setAttribute ("presetName", label);
        slots->setAttribute ("slotAName", label + " A");
        slots->setAttribute ("slotBName", label + " B");
        juce::AudioProcessor::copyXmlToBinary (*xml, state);
        processor.setStateInformation (state.getData(), (int) state.getSize());
        processor.getStateInformation (state);
        if (! file.replaceWithData (state.getData(), state.getSize()))
            return 3;
    }
    else if (mode == "read")
    {
        if (! file.loadFileAsData (state))
            return 4;
        processor.setStateInformation (state.getData(), (int) state.getSize());
        juce::MemoryBlock resaved;
        processor.getStateInformation (resaved);
        if (! file.getSiblingFile (file.getFileName() + ".resaved").replaceWithData (
                resaved.getData(), resaved.getSize()))
            return 5;
        std::printf ("resaved-identical=%d size=%zu\n", state == resaved, resaved.getSize());
        if (state != resaved)
            return 6;
    }
    else
        return 7;
    describe (processor);
}
