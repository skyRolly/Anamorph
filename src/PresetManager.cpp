#include "PresetManager.h"

#include "SerializedNumber.h"   // the shared malformed-value predicate (both restore paths)
#include <cmath>   // std::isfinite -- the non-finite guards on the restore paths

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
// The NAME fallback below is still needed and still correct, but its remit narrowed in
// 0.9.2 when the identity started travelling with the session: it now covers only state
// that carries a name and NO identity -- a pre-0.9.2 session, or one whose identity
// properties were dropped or hand-edited. There it keeps the pre-0.9.2 answer (the
// factory row on a duplicate name), which is the documented tie-break, not an accident.
// Everything that HAS an identity is answered above, including with -1.
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
namespace
{
    // Shared with AnamorphAudioProcessor::reassertParameters (session state) via
    // anamorph::looksLikePlainNumber -- one predicate, two restore paths, so a
    // malformed value cannot mean different things depending on where it came from.
    // Returns false for "no usable value here", which every caller answers with the
    // parameter default -- the same answer an absent node already gets, and the one
    // SERIALIZATION_REGISTRY.md records.
    bool readSerializedValue (const juce::var& prop, float& out)
    {
        if (prop.isVoid()) return false;
        if (prop.isString())
        {
            const auto text = prop.toString().trim();   // tolerate a hand edit's spaces
            if (! anamorph::looksLikePlainNumber (text.toRawUTF8())) return false;
        }
        const float v = (float) (double) prop;
        if (! anamorph::isUsableSerializedValue (v)) return false;   // nan, +/-inf, 1e39
        out = v;
        return true;
    }
}

void PresetManager::applySoundTree (const juce::ValueTree& state)
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            if (! pid::isPresetExcluded (wid->paramID))
            {
                auto child = state.getChildWithProperty ("id", wid->paramID);
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                {
                    // A preset file is user-editable text and `nan` parses, so the
                    // value is only adopted when it is finite -- otherwise the
                    // parameter default applies, exactly as it does for a missing
                    // child. A non-finite parameter silences the plug-in for the
                    // rest of the session (see reassertParameters for the full
                    // mechanism); the session path is guarded there.
                    //
                    // The presence test is on the PROPERTY, not just the node: a
                    // <PARAM id="width"/> that lost its value to a truncated write or
                    // a hand edit reads back as var() -> 0.0 -> the range MINIMUM,
                    // which for width (0..2, default 1) is a silent mono collapse.
                    // "Absent means default" has to hold for a value-less node too --
                    // the writer is apvts.copyState().createXml(), which always emits
                    // `value`, so nothing this plug-in saves takes the new branch.
                    // The guard runs on the INPUT, before convertTo0to1, because the
                    // conversion clamps: an infinity arrives at a finiteness test
                    // already laundered into a finite range ENDPOINT. anamorph::
                    // SerializedNumber.h carries the measured table and the rule; the
                    // session path applies the same predicate so the two cannot drift.
                    float plain = 0.0f;
                    const bool usable = readSerializedValue (child.getProperty ("value"), plain);
                    const float fromFile = usable ? rp->convertTo0to1 (plain)
                                                  : rp->getDefaultValue();
                    rp->setValueNotifyingHost (fromFile);
                }
            }
    resetSolo();
}

void PresetManager::load (int index)
{
    if (index < 0 || index >= list.size()) return;
    const auto& e = list.getReference (index);

    // Resolve EVERYTHING that can fail BEFORE opening the undo bracket: a failure must be a
    // clean no-op, never an onAboutToLoad() with no matching onLoaded() (which would flush undo
    // coalescing yet record no step, leaving the undo timeline half-open). Mirrors loadFile().
    std::unique_ptr<juce::XmlElement> userXml;
    const Factory* factory = nullptr;
    if (e.isFactory)
    {
        // The entry's id was copied straight out of the table by refresh(), so a miss is a
        // programming error (a duplicated or edited id), not a user condition -- assert it.
        // Failing as a no-op is what the non-debug build must do: applying defaults while
        // ADOPTING a factory identity that resolves to nothing would leave the tick pointing
        // at a preset whose sound was never applied.
        factory = findFactory (e.factoryId);
        jassert (factory != nullptr);
        if (factory == nullptr) return;
    }
    else
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
        for (const auto& o : factory->set)
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

    // NOT a path escape, though it reads like one and has been reported as one more than once.
    // `getChildFile` does short-circuit to the raw File constructor for anything isAbsolutePath
    // accepts, and a leading `~` survives createLegalFileName on POSIX -- so `~foo.anamorph` really
    // does yield the unresolved relative path rather than a file in `dir`. The WRITE then cannot
    // succeed: replaceWithText never opens the target, it writes a hidden sibling built from
    // getParentDirectory(), and for a separator-less path getPathUpToLastSlash() returns the path
    // itself -- not a directory. createLegalFileName strips `/` and `\`, so a name reaching here can
    // never contain a separator and every tilde-leading name hits that same degenerate parent.
    // saveUser therefore returns FALSE below, nothing is written anywhere, and the editor's
    // `if (saveUser(...))` leaves the Save dialog open with the text intact -- the save fails
    // VISIBLY, which is what a sanitisation guard here would have produced anyway. Verified against
    // the pinned juce_core for `~foo`, `~/foo`, `~` and `~root`. Do not "fix" this; see
    // worklogs/PRESET_MENU_AND_IDENTITY_v0.9.2.md §7. (The ENCODE side of the same character was a
    // real defect and is fixed in encodeSelection -- a `~`-named file a user copies into the folder
    // by hand. Different function, different question: §9.)
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

void PresetManager::adoptRestoredState (const juce::String& name, const Selection& restoredSel)
{
    // `name` is adopted VERBATIM, empty included. An empty preset name is a real state since
    // 0.9.2 -- a session saved while sitting on a nameless A/B slot stores exactly that -- and
    // keeping the old name instead would leave the PREVIOUS project's label on this instance,
    // because hosts reuse one processor across setStateInformation calls. Resolving "the field
    // was absent" (a pre-0.6 session) into defaultName() is the caller's job: only the caller
    // can tell an absent property from an empty one, exactly as it already does for the baseline.
    current = name;
    sel = restoredSel;      // unknown for a pre-0.9.2 session -> the name fallback, as before (#4)
    sigAtLoad = soundSig(); // restored state counts as the clean baseline
}

// ----------------------------------------------------------------------------
//  Indicator identity <-> session state. Metadata only: nothing here reads or writes
//  a parameter, and nothing here touches a user preset FILE.
// ----------------------------------------------------------------------------
PresetManager::SelectionFields PresetManager::encodeSelection (const Selection& s)
{
    switch (s.kind)
    {
        case Selection::Kind::factory:
            return { "factory", s.factoryId, {} };

        case Selection::Kind::userFile:
        {
            // DIRECT child, not descendant. juce::File::isAChildOf recurses (juce_File.cpp), so it
            // is also true for a file nested in a SUB-folder of the preset folder -- and that file
            // would then be stored as its bare name and decode back to a DIFFERENT file of the same
            // name sitting directly in the folder. `refresh()` scans non-recursively, so a direct
            // child is the only thing that can ever be a menu row anyway; everything else takes the
            // absolute-path branch and round-trips exactly.
            //
            // ...and the bare name has to be one the decoder cannot mistake for a path. Nothing
            // stops a user dropping `~foo.anamorph` into the preset folder by hand (the manual
            // tells them to manage presets as files), and `juce::File::isAbsolutePath` accepts a
            // leading `~` on POSIX -- so a bare `~foo.anamorph` would come back as the literal
            // relative string rather than the file in the folder, and the row would lose its tick.
            // Such a name simply takes the absolute-path branch instead: less portable for that one
            // preset, but `decode(encode(s)) == s` holds, which is the invariant that matters.
            const auto name = s.file.getFileName();
            const bool nameIsUnambiguous = ! juce::File::isAbsolutePath (name);
            return { "user", {}, (s.file.getParentDirectory() == presetDirectory() && nameIsUnambiguous)
                                     ? name
                                     : s.file.getFullPathName() };
        }

        case Selection::Kind::unknown:
        default:
            return {};
    }
}

PresetManager::Selection PresetManager::decodeSelection (const juce::String& kind,
                                                        const juce::String& factoryId,
                                                        const juce::String& userFile)
{
    // Anything unrecognised, empty or half-written decodes to `unknown`, which is the
    // pre-0.9.2 behaviour (resolve by name). A wrong-but-well-formed value cannot select
    // the wrong row either: currentIndex() answers -1 for an identity it cannot find,
    // rather than falling back to a same-named preset.
    if (kind == "factory" && factoryId.isNotEmpty())
        return { Selection::Kind::factory, factoryId, {} };

    if (kind == "user" && userFile.isNotEmpty())
        return { Selection::Kind::userFile, {},
                 juce::File::isAbsolutePath (userFile) ? juce::File (userFile)
                                                       : presetDirectory().getChildFile (userFile) };

    return {};
}

} // namespace anamorph
