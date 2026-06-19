#include "PresetManager.h"

namespace anamorph
{

// ----------------------------------------------------------------------------
//  Factory presets: overrides applied on top of the default sound. Values are
//  PLAIN (dB / fraction / Hz / choice index). Elegant, useful starting points:
//  a few all-purpose ones first, then character presets per algorithm.
// ----------------------------------------------------------------------------
namespace
{
    struct Override { const char* id; float value; };
    struct Factory  { const char* name; std::vector<Override> set; };

    static const std::vector<Factory>& factoryPresets()
    {
        static const std::vector<Factory> presets = {
            { "Default", {} },
            { "Gentle Width",   { { pid::algorithm, 1 }, { pid::amount, 0.30f }, { pid::width, 1.10f } } },
            { "Mono To Stereo", { { pid::algorithm, 1 }, { pid::amount, 0.80f }, { pid::velvetDensity, 0.60f },
                                  { pid::width, 1.15f }, { pid::monoMakerOn, 1 }, { pid::monoMakerFreq, 120.0f } } },
            { "Vocal Air",      { { pid::algorithm, 2 }, { pid::amount, 0.40f }, { pid::chorusRate, 0.35f },
                                  { pid::chorusDepth, 0.28f }, { pid::width, 1.05f }, { pid::mix, 0.90f } } },
            { "Synth Dimension",{ { pid::algorithm, 3 }, { pid::dimMode, 2 }, { pid::amount, 0.65f },
                                  { pid::width, 1.10f } } },
            { "Drum Spread",    { { pid::algorithm, 0 }, { pid::haasDelay, 9.0f }, { pid::amount, 0.50f },
                                  { pid::monoMakerOn, 1 }, { pid::monoMakerFreq, 150.0f } } },
            { "Bass Guard",     { { pid::algorithm, 1 }, { pid::amount, 0.45f }, { pid::width, 1.05f },
                                  { pid::monoMakerOn, 1 }, { pid::monoMakerFreq, 200.0f } } },
            { "Tape Chorus",    { { pid::algorithm, 2 }, { pid::amount, 0.60f }, { pid::chorusRate, 0.80f },
                                  { pid::chorusDepth, 0.50f }, { pid::drive, 2.5f } } },
            { "Wide Master",    { { pid::algorithm, 1 }, { pid::amount, 0.28f }, { pid::width, 1.12f },
                                  { pid::mbEnable, 1 }, { pid::mbWidthLow, 0.90f }, { pid::mbWidthHigh, 1.25f },
                                  { pid::monoMakerOn, 1 }, { pid::monoMakerFreq, 90.0f } } },
            { "Super Wide",     { { pid::algorithm, 1 }, { pid::amount, 1.00f }, { pid::velvetDensity, 0.65f },
                                  { pid::width, 1.40f }, { pid::monoMakerOn, 1 }, { pid::monoMakerFreq, 130.0f } } },
        };
        return presets;
    }

    const juce::String kPresetExt = PresetManager::fileSuffix();
}

// ----------------------------------------------------------------------------
PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s) : apvts (s)
{
    refresh();
    sigAtLoad = soundSig(); // the freshly-constructed state IS "Default"
}

juce::File PresetManager::presetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
              .getChildFile ("RollyTech").getChildFile ("Anamorph").getChildFile ("Presets");
}

void PresetManager::refresh()
{
    list.clearQuick();
    for (auto& f : factoryPresets())
        list.add ({ f.name, true, {} });

    auto dir = presetDirectory();
    if (dir.isDirectory())
    {
        juce::Array<juce::File> files = dir.findChildFiles (juce::File::findFiles, false,
                                                            juce::String ("*") + kPresetExt);
        struct ByName { static int compareElements (const juce::File& a, const juce::File& b)
                        { return a.getFileNameWithoutExtension().compareIgnoreCase (b.getFileNameWithoutExtension()); } };
        ByName cmp; files.sort (cmp);
        for (auto& f : files)
            list.add ({ f.getFileNameWithoutExtension(), false, f });
    }
}

int PresetManager::currentIndex() const noexcept
{
    for (int i = 0; i < list.size(); ++i)
        if (list.getReference (i).name == current)
            return i;
    return -1;
}

bool PresetManager::isDirty() const
{
    return soundSig() != sigAtLoad;
}

// One signature of every SOUND parameter (same idea as the processor's undo
// signature): cheap to compare, no float-tolerance surprises.
juce::String PresetManager::soundSig() const
{
    juce::String sig;
    for (auto* p : apvts.processor.getParameters())
        if (auto* wid = dynamic_cast<const juce::AudioProcessorParameterWithID*> (p))
            if (! pid::isPresetExcluded (wid->paramID))
                sig << juce::String (p->getValue(), 5) << ',';
    return sig;
}

// Presets always start with the per-band solo off (0.6.10 #9); undo still restores
// the pre-load solo because mbSolo is part of the undo signature.
void PresetManager::resetSolo()
{
    if (auto* sp = apvts.getParameter (pid::mbSolo))
        sp->setValueNotifyingHost (sp->getDefaultValue());
}

void PresetManager::applyDefaults()
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            if (! pid::isPresetExcluded (wid->paramID))
                p->setValueNotifyingHost (p->getDefaultValue());
    resetSolo();
}

// Apply the sound params stored in an APVTS-style tree (missing ones fall back
// to their defaults, so older preset files stay loadable).
void PresetManager::applySoundTree (const juce::ValueTree& state)
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            if (! pid::isPresetExcluded (wid->paramID))
            {
                auto child = state.getChildWithProperty ("id", wid->paramID);
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                    rp->setValueNotifyingHost (child.isValid()
                        ? rp->convertTo0to1 ((float) (double) child.getProperty ("value"))
                        : rp->getDefaultValue());
            }
    resetSolo();
}

void PresetManager::load (int index)
{
    if (index < 0 || index >= list.size()) return;
    const auto& e = list.getReference (index);

    if (e.isFactory)
    {
        applyDefaults();
        const auto& f = factoryPresets()[(size_t) index]; // factory block is list-front
        for (const auto& o : f.set)
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (o.id)))
                rp->setValueNotifyingHost (rp->convertTo0to1 (o.value));
    }
    else
    {
        auto xml = juce::parseXML (e.file);
        if (xml == nullptr) return;
        applySoundTree (juce::ValueTree::fromXml (*xml));
    }

    current = e.name;
    sigAtLoad = soundSig();
}

bool PresetManager::loadFile (const juce::File& f)
{
    auto xml = juce::parseXML (f);
    if (xml == nullptr) return false;
    applySoundTree (juce::ValueTree::fromXml (*xml));
    current = f.getFileNameWithoutExtension();
    sigAtLoad = soundSig();
    return true;
}

void PresetManager::step (int delta)
{
    if (list.isEmpty()) return;
    const int cur = currentIndex();
    const int n   = list.size();
    // Unknown current name steps from "Default"; otherwise wrap around the list.
    const int from = cur >= 0 ? cur : 0;
    load (((from + delta) % n + n) % n);
}

bool PresetManager::saveUser (const juce::String& rawName)
{
    const juce::String name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) return false;

    auto dir = presetDirectory();
    if (! dir.createDirectory()) return false;

    auto file = dir.getChildFile (name + kPresetExt);
    auto xml  = apvts.copyState().createXml();
    if (xml == nullptr || ! file.replaceWithText (xml->toString())) return false;

    refresh();
    current = name;
    sigAtLoad = soundSig();
    return true;
}

void PresetManager::adoptRestoredState (const juce::String& name)
{
    if (name.isNotEmpty()) current = name;
    sigAtLoad = soundSig(); // restored state counts as the clean baseline
}

} // namespace anamorph
