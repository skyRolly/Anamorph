#include "PresetManager.h"

namespace anamorph
{

// ----------------------------------------------------------------------------
//  Factory presets: overrides applied on top of the default sound. Values are
//  PLAIN (dB / fraction / Hz / choice index). Elegant, useful starting points:
//  a few all-purpose ones first, then character presets per algorithm.
//
//  `id` is the preset's INTERNAL identity (#4) and is never shown: the menu, the
//  top bar and the Save Preset field all display `name`. It exists so a factory
//  preset is selected by something a user preset can never collide with -- a user
//  preset is identified by its FILE, so the two namespaces are disjoint and a user
//  preset saved as e.g. "Wide Master" no longer steals the factory row's tick.
//  Treat the ids as immutable: renaming a preset is a display change, renaming an
//  id would silently re-point A/B and undo slots that still hold the old one.
// ----------------------------------------------------------------------------
namespace
{
    struct Override { const char* id; float value; };
    struct Factory  { const char* id; const char* name; std::vector<Override> set; };

    static const std::vector<Factory>& factoryPresets()
    {
        static const std::vector<Factory> presets = {
            { "default",        "Default", {} },
            { "gentleWidth",    "Gentle Width",   { { pid::algorithm, 1 }, { pid::amount, 0.30f }, { pid::width, 1.10f } } },
            { "monoToStereo",   "Mono To Stereo", { { pid::algorithm, 1 }, { pid::amount, 0.80f }, { pid::velvetDensity, 0.60f },
                                  { pid::width, 1.15f }, { pid::monoMakerOn, 1 }, { pid::monoMakerFreq, 120.0f } } },
            { "vocalAir",       "Vocal Air",      { { pid::algorithm, 2 }, { pid::amount, 0.40f }, { pid::chorusRate, 0.35f },
                                  { pid::chorusDepth, 0.28f }, { pid::width, 1.05f }, { pid::mix, 0.90f } } },
            { "synthDimension", "Synth Dimension",{ { pid::algorithm, 3 }, { pid::dimMode, 2 }, { pid::amount, 0.65f },
                                  { pid::width, 1.10f } } },
            { "drumSpread",     "Drum Spread",    { { pid::algorithm, 0 }, { pid::haasDelay, 9.0f }, { pid::amount, 0.50f },
                                  { pid::monoMakerOn, 1 }, { pid::monoMakerFreq, 150.0f } } },
            { "bassGuard",      "Bass Guard",     { { pid::algorithm, 1 }, { pid::amount, 0.45f }, { pid::width, 1.05f },
                                  { pid::monoMakerOn, 1 }, { pid::monoMakerFreq, 200.0f } } },
            { "tapeChorus",     "Tape Chorus",    { { pid::algorithm, 2 }, { pid::amount, 0.60f }, { pid::chorusRate, 0.80f },
                                  { pid::chorusDepth, 0.50f }, { pid::drive, 2.5f } } },
            { "wideMaster",     "Wide Master",    { { pid::algorithm, 1 }, { pid::amount, 0.28f }, { pid::width, 1.12f },
                                  { pid::mbEnable, 1 }, { pid::mbWidthLow, 0.90f }, { pid::mbWidthHigh, 1.25f },
                                  { pid::monoMakerOn, 1 }, { pid::monoMakerFreq, 90.0f } } },
            { "superWide",      "Super Wide",     { { pid::algorithm, 1 }, { pid::amount, 1.00f }, { pid::velvetDensity, 0.65f },
                                  { pid::width, 1.40f }, { pid::monoMakerOn, 1 }, { pid::monoMakerFreq, 130.0f } } },
        };
        return presets;
    }

    const Factory* findFactory (const juce::String& id)
    {
        for (const auto& f : factoryPresets())
            if (id == f.id) return &f;
        return nullptr;
    }

    const juce::String kPresetExt = PresetManager::fileSuffix();
}

// ----------------------------------------------------------------------------
PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s) : apvts (s)
{
    refresh();
    // The freshly-constructed state IS the "Default" FACTORY preset -- seed the
    // identity too, so the tick starts on the factory row even if a user preset
    // called "Default" exists on disk (#4).
    sel = { Selection::Kind::factory, factoryPresets().front().id, {} };
    sigAtLoad = soundSig();
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
        list.add ({ f.name, true, {}, f.id });

    auto dir = presetDirectory();
    if (dir.isDirectory())
    {
        juce::Array<juce::File> files = dir.findChildFiles (juce::File::findFiles, false,
                                                            juce::String ("*") + kPresetExt);
        struct ByName { static int compareElements (const juce::File& a, const juce::File& b)
                        { return a.getFileNameWithoutExtension().compareIgnoreCase (b.getFileNameWithoutExtension()); } };
        ByName cmp; files.sort (cmp);
        for (auto& f : files)
            list.add ({ f.getFileNameWithoutExtension(), false, f, {} });
    }
}

// Which row the drop-down ticks. IDENTITY first (#4): a factory preset is matched by
// its immutable id and a user preset by its file, so a user preset that happens to
// share a factory preset's name resolves to the row that was actually loaded rather
// than to whichever one the name hit first (always the factory row, since the factory
// block is list-front).
//
// The NAME fallback below is still needed and still correct: it covers everything that
// carries a name but no live identity -- a session restored from disk (the identity is
// runtime-only), a `.anamorph` file loaded from OUTSIDE the preset folder, and a user
// preset that has since been renamed or deleted on disk. In the duplicate-name case the
// fallback keeps the pre-0.9.2 answer (the factory row), which is the documented
// tie-break, not an accident.
int PresetManager::currentIndex() const noexcept
{
    if (sel.kind != Selection::Kind::unknown)
    {
        for (int i = 0; i < list.size(); ++i)
        {
            const auto& e = list.getReference (i);
            if (sel.kind == Selection::Kind::factory && e.isFactory && e.factoryId == sel.factoryId)
                return i;
            if (sel.kind == Selection::Kind::userFile && ! e.isFactory && e.file == sel.file)
                return i;
        }
        // A KNOWN identity that is not in the list means the thing that produced this
        // sound is not on the menu -- a file loaded from outside the preset folder, or a
        // user preset deleted/renamed on disk since. It must show NO tick. Falling through
        // to the name scan would instead tick whatever shares the name, i.e. exactly the
        // factory row this change exists to stop mis-ticking.
        return -1;
    }

    for (int i = 0; i < list.size(); ++i)
        if (list.getReference (i).name == current)
            return i;
    return -1;
}

bool PresetManager::isDirty() const
{
    // S10: soundSig() is a pure function of the sound parameters, and the
    // processor bumps a generation counter on every sound-param change -- an
    // unchanged generation means the last built signature is still exact, so
    // the ~34 per-call String formats/allocations are skipped. The comparison
    // below stays live, which is why sigAtLoad updates (load/save/undo/A-B)
    // need no cache invalidation. Generation is sampled before building, so a
    // concurrent change just rebuilds on the next call.
    if (soundParamGeneration != nullptr)
    {
        const auto gen = soundParamGeneration();
        if (gen != cachedSigGen)
        {
            cachedSig    = soundSig();
            cachedSigGen = gen;
        }
        return cachedSig != sigAtLoad;
    }
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

    // Parse a user preset BEFORE opening the undo bracket: a corrupt file must fail cleanly, without
    // an onAboutToLoad() with no matching onLoaded() (which would flush undo coalescing yet record no
    // step, leaving the undo timeline half-open). Factory presets can't fail. Mirrors loadFile().
    std::unique_ptr<juce::XmlElement> userXml;
    if (! e.isFactory)
    {
        userXml = juce::parseXML (e.file);
        if (userXml == nullptr) return;
    }

    if (onAboutToLoad) onAboutToLoad(); // flush any settled edit so the pre-load state is the undo baseline

    if (e.isFactory)
    {
        applyDefaults();
        // Resolved through the ID, not the list position: the entry already carries the
        // identity the selection is about to adopt, so the two can never disagree (#4).
        if (const auto* f = findFactory (e.factoryId))
            for (const auto& o : f->set)
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (o.id)))
                    rp->setValueNotifyingHost (rp->convertTo0to1 (o.value));
    }
    else
    {
        applySoundTree (juce::ValueTree::fromXml (*userXml));
    }

    current = e.name;
    sel = e.isFactory ? Selection { Selection::Kind::factory,  e.factoryId, {} }
                      : Selection { Selection::Kind::userFile, {},          e.file };
    sigAtLoad = soundSig();
    if (onLoaded) onLoaded(); // record the switch as ONE undo step (name/baseline now reflect the new preset)
}

bool PresetManager::loadFile (const juce::File& f)
{
    auto xml = juce::parseXML (f);
    if (xml == nullptr) return false;
    if (onAboutToLoad) onAboutToLoad(); // flush any settled edit so the pre-load state is the undo baseline
    applySoundTree (juce::ValueTree::fromXml (*xml));
    current = f.getFileNameWithoutExtension();
    // The chooser can point ANYWHERE, so the file is the identity whether or not it
    // lives in the preset folder; a file from outside simply matches no list row and
    // leaves the menu unticked, which is what it should show (#4).
    sel = { Selection::Kind::userFile, {}, f };
    sigAtLoad = soundSig();
    if (onLoaded) onLoaded(); // record the switch as ONE undo step (name/baseline now reflect the new preset)
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
    // Saving SELECTS what was just written, by file. This is the case the ID split
    // exists for: saving a user preset under a factory preset's name now moves the
    // tick to the USER row instead of leaving it on the factory one (#4).
    sel = { Selection::Kind::userFile, {}, file };
    sigAtLoad = soundSig();
    if (onSaved) onSaved(); // re-baseline the processor's undo snapshot onto the saved preset
    return true;
}

void PresetManager::adoptRestoredState (const juce::String& name)
{
    if (name.isNotEmpty()) current = name;
    sel = {};               // a restored session carries a name, never an identity (#4)
    sigAtLoad = soundSig(); // restored state counts as the clean baseline
}

} // namespace anamorph
