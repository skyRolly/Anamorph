#include "PresetManager.h"

#include "SerializedNumber.h"   // the shared malformed-value predicate (both restore paths)
#include "ParameterDispatch.h"  // ADR-0036 round 27 (R1390): every host-notifying write is bracketed
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
juce::String PresetManager::soundSignatureFor (const juce::AudioProcessorValueTreeState& s)
{
    juce::String sig;
    for (auto* p : s.processor.getParameters())
        if (auto* wid = dynamic_cast<const juce::AudioProcessorParameterWithID*> (p))
            if (! pid::isPresetExcluded (wid->paramID))
            {
                // Signed as the plug-in renders and stores it, so that this signature and
                // the one soundSignatureForSavedTree builds from a FILE are the same
                // quantity (§17). A no-op for every stock parameter; see
                // normalisedAsRendered in PluginParameters.h, which the undo / A-B
                // signature uses too so all three answer one question.
                sig << juce::String (normalisedAsRendered (*p), 5) << ',';
            }
    return sig;
}

juce::String PresetManager::soundSig() const
{
    return soundSignatureFor (apvts);
}

// Presets always start with the per-band solo off (0.6.10 #9); undo still restores
// the pre-load solo because mbSolo is part of the undo signature.
void PresetManager::resetSolo()
{
    if (auto* sp = apvts.getParameter (pid::mbSolo))
        anamorph::param::setValueNotifyingHost (sp, sp->getDefaultValue());
}

// ONE AT A TIME (§24). This is HALF of the factory apply -- the overrides in loadAdopted are
// the other half -- so its caller holds the same (recursive) lock across both; taking it here
// as well costs nothing and keeps the function safe for any future caller of its own.
void PresetManager::applyDefaults()
{
    const juce::ScopedLock oneAtATime (replacementLock());
    int written = 0;
    for (auto* p : apvts.processor.getParameters())
        if (auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            if (! pid::isPresetExcluded (wid->paramID))
            {
                if (++written == 4 && insideReplacement) insideReplacement();
                anamorph::param::setValueNotifyingHost (p, p->getDefaultValue());
            }
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

    // THE ONE RULE FOR "WHAT SOUND DOES THIS SAVED TREE MEAN", shared by the two paths
    // that must never disagree about it (D-2 round 9, ADR-0036 §17): the path that
    // APPLIES a preset (applySoundTree) and the path that computes the CLEAN BASELINE a
    // saved preset is judged against (soundSignatureForSavedTree). They used to be
    // different computations over different sources -- the apply read the tree, the
    // baseline read the LIVE parameters -- which is exactly how a preset came to be
    // marked clean against a sound its own file does not contain. One function now
    // answers the question for both, so "clean" can only ever mean "the live sound is
    // what this file restores".
    //
    // The resolution rule itself is unchanged and is the one SERIALIZATION_REGISTRY.md
    // records: a value is adopted only when the node, the property and the number are
    // all usable; absent, value-less, malformed or non-finite all resolve to the
    // parameter DEFAULT, which is the same answer a missing PARAM child already got.
    // The presence test is on the PROPERTY, not just the node: a <PARAM id="width"/>
    // that lost its value to a truncated write or a hand edit reads back as var() ->
    // 0.0 -> the range MINIMUM, which for width (0..2, default 1) is a silent mono
    // collapse. The guard runs on the INPUT, before convertTo0to1, because the
    // conversion clamps: an infinity arrives at a finiteness test already laundered
    // into a finite range ENDPOINT. anamorph::SerializedNumber.h carries the measured
    // table; the session path applies the same predicate so the two cannot drift.
    float normalisedFromSavedTree (const juce::RangedAudioParameter& rp,
                                   const juce::ValueTree& savedSound,
                                   const juce::String& paramID)
    {
        const auto child = savedSound.getChildWithProperty ("id", paramID);
        float plain = 0.0f;
        return readSerializedValue (child.getProperty ("value"), plain)
                 ? rp.convertTo0to1 (plain)
                 : rp.getDefaultValue();
    }
}

namespace
{
    // ADR-0055. THE BYTE-LEVEL HALF OF THE PRESET BOUNDARY.
    //
    // It steps over every XML shape that may legally contain a `<` or a `>` -- a comment, a CDATA
    // section, a processing instruction (which is also how the `<?xml ... ?>` declaration arrives),
    // and a quoted attribute value -- because miscounting any of them would refuse a legitimate
    // preset, and a guard that rejects real files is worse than the hole it closes.

    // Advance past the next occurrence of `terminator`. False means the construct never ended,
    // which is not a document worth handing to a parser.
    bool skipPast (juce::String::CharPointerType& p, const char* terminator, int length)
    {
        for (; ! p.isEmpty(); ++p)
            if (juce::CharacterFunctions::compareUpTo (p, juce::CharPointer_ASCII (terminator), length) == 0)
            {
                p += length;
                return true;
            }
        return false;
    }

    bool startsWith (juce::String::CharPointerType p, const char* lit, int length)
    {
        return juce::CharacterFunctions::compareUpTo (p, juce::CharPointer_ASCII (lit), length) == 0;
    }

    // ADR-0055 (0.9.9). THE FILE IS ITS BYTES, AND EVERY ONE OF THEM MUST REACH THE SCAN BELOW.
    //
    // A `juce::String` is NUL-TERMINATED, and every reader downstream walks it with a CharPointer
    // that STOPS at the first NUL -- `presetTextIsAdmissible` and `juce::XmlDocument` alike. So a
    // file holding a valid preset, then one 0x00, then anything at all decodes to a String whose
    // visible content is just the preset: the scan agrees it is ONE document, and the tail is
    // examined by nobody. Measured on `f03aa06` through the real `loadFile`: a 263-byte file
    // holding preset A, a NUL and a COMPLETE preset B decoded to 131 bytes and LOADED, applying
    // A -- while the same file without the NUL was refused. Trailing prose and a trailing
    // invalid-UTF-8 tail loaded the same way. One byte of disguise defeated the whole boundary.
    //
    // The refusal is therefore on the bytes, BEFORE the decode, because after it the evidence is
    // gone. It reads them the way `String::createStringFromData` is about to (juce_String.cpp
    // :1987-2035): a UTF-16 byte-order mark selects 16-bit code units, a UTF-8 one is skipped,
    // anything else is bytes. Scanning for a zero BYTE regardless of encoding would be simpler and
    // wrong -- a UTF-16 preset is ASCII with a zero byte in every other position, and that file
    // decodes and loads today, so refusing it would break a real file rather than a corrupt one.
    bool presetBytesAreAdmissible (const void* data, size_t size)
    {
        const auto* bytes = static_cast<const juce::uint8*> (data);

        if (size >= 2 && (juce::CharPointer_UTF16::isByteOrderMarkBigEndian (bytes)
                           || juce::CharPointer_UTF16::isByteOrderMarkLittleEndian (bytes)))
        {
            // The decoder takes `size / 2 - 1` whole code units, so a trailing HALF unit is
            // dropped without ever being read -- another byte the scan would never see.
            if (size % 2 != 0) return false;

            for (size_t i = 2; i < size; i += 2)
                if (bytes[i] == 0 && bytes[i + 1] == 0) return false;

            return true;
        }

        for (size_t i = (size >= 3 && juce::CharPointer_UTF8::isByteOrderMark (bytes)) ? 3 : 0;
             i < size; ++i)
            if (bytes[i] == 0) return false;

        return true;
    }

    // True when `p` stands at the `<?` of a processing instruction whose TARGET is exactly `xml`
    // in any case -- which XML reserves for the DECLARATION and for nothing else. The comparison
    // stops at a terminator (`compareIgnoreCaseUpTo` breaks on a zero character), so a truncated
    // `<?xm` cannot be read past the end of the text; and because a match proves five non-zero
    // characters, `p[5]` is at worst the terminator itself. `<?xmlfoo?>` is a DIFFERENT target and
    // an ordinary instruction, which is why the character after the name is examined at all.
    bool isXmlDeclaration (juce::String::CharPointerType p)
    {
        if (juce::CharacterFunctions::compareIgnoreCaseUpTo (p, juce::CharPointer_ASCII ("<?xml"), 5) != 0)
            return false;

        const auto after = p[5];
        return after == 0 || after == '?' || juce::CharacterFunctions::isWhitespace (after);
    }

    // True when `text` is ONE well-delimited XML document, nested no deeper than `maxDepth`,
    // carrying no DOCTYPE, and followed by nothing but whitespace, comments and processing
    // instructions. It answers ONLY those questions: what the document says is
    // `presetDocumentIsWellFormed`'s job, and what its values mean is `normalisedFromSavedTree`'s.
    bool presetTextIsAdmissible (const juce::String& text, int maxDepth)
    {
        const auto begin = text.getCharPointer();
        auto p = begin;
        int  depth = 0;
        bool sawRoot = false, rootClosed = false;

        while (! p.isEmpty())
        {
            const auto c = *p;

            if (c != '<')
            {
                // Character data. Inside an element it is merely content this format does not use
                // (and which `presetDocumentIsWellFormed` refuses by name); OUTSIDE every element
                // only whitespace may appear, and that single rule is what refuses a second
                // document's prose, a trailing sentence and a trailing run of binary alike.
                if (depth == 0 && ! juce::CharacterFunctions::isWhitespace (c)) return false;
                ++p;
                continue;
            }

            if (startsWith (p, "<!--", 4))
            {
                p += 4;
                if (! skipPast (p, "-->", 3)) return false;
                continue;
            }
            if (startsWith (p, "<![CDATA[", 9))
            {
                if (depth == 0) return false;          // a CDATA section outside every element
                p += 9;
                if (! skipPast (p, "]]>", 3)) return false;
                continue;
            }
            if (p[1] == '?')                           // a processing instruction -- or the declaration
            {
                // THE XML DECLARATION IS NOT AN ORDINARY INSTRUCTION, and treating it as one was a
                // hole: `<ANAMORPH/>` followed by `<?xml version="1.0"?>` was accepted, because the
                // skip below steps over any `<?...?>` wherever it sits and the `rootClosed` refusal
                // guards only an opening TAG. `juce::parseXML` then returns the first element and
                // ignores the tail, so a malformed document loaded as a preset.
                //
                // XML allows the declaration in exactly one place: the very first thing in the
                // document, once. So it is admitted only at offset zero and refused everywhere
                // else -- after the root, after a comment, after another instruction, after
                // whitespace, or after another declaration. NOTHING may precede it, not even
                // whitespace, which is the spec's own rule and is what the writer produces:
                // `XmlElement::toString` emits the header first, at offset zero, and a byte-order
                // mark is removed by the decode before this scan ever sees the text.
                //
                // Ordinary instructions are untouched, before the root and after it alike, which is
                // the tolerance ADR-0055 states and State test 114 leg E pins.
                if (p.getAddress() != begin.getAddress() && isXmlDeclaration (p)) return false;

                p += 2;
                if (! skipPast (p, "?>", 2)) return false;
                continue;
            }
            if (p[1] == '!')                           // `<!DOCTYPE ...`: the hang and the file read
                return false;

            if (p[1] == '/')                           // a closing tag
            {
                if (depth == 0) return false;
                p += 2;
                if (! skipPast (p, ">", 1)) return false;
                if (--depth == 0) rootClosed = true;
                continue;
            }

            // An opening tag. It is walked to its own `>` with quotes honoured, so a `>` inside an
            // attribute value cannot end it early and a `<` inside one cannot open a phantom
            // element.
            if (rootClosed) return false;              // a SECOND top-level element
            ++p;
            juce::juce_wchar quote = 0;
            bool selfClosing = false, closed = false;
            for (; ! p.isEmpty(); ++p)
            {
                const auto t = *p;
                if (quote != 0)              { if (t == quote) quote = 0; continue; }
                if (t == '"' || t == '\'')   { quote = t; continue; }
                if (t == '/' && p[1] == '>') { selfClosing = closed = true; p += 2; break; }
                if (t == '>')                { closed = true; ++p; break; }
            }
            if (! closed) return false;

            if (depth == 0) sawRoot = true;
            if (! selfClosing)
            {
                if (++depth > maxDepth) return false;
            }
            else if (depth == 0)
            {
                rootClosed = true;                     // `<ANAMORPH/>`: opened and closed at once
            }
        }

        return sawRoot && rootClosed && depth == 0;
    }

    // ADR-0055. THE DOCUMENT-SHAPE HALF, checked on the parsed ELEMENT rather than on the
    // ValueTree, for two reasons a ValueTree cannot answer: `ValueTree::fromXml` DROPS a text
    // child (tripping `jassertfalse` on the way in a debug build), and a ValueTree's property set
    // collapses a duplicated attribute to one. Both are shapes a preset must not have, so the
    // question is asked where they are still visible.
    //
    // The three literals mirror JUCE's own `AudioProcessorValueTreeState::valueType`,
    // `idPropertyID` and `valuePropertyID`, which are private to that class; State test 1 pins the
    // spelling against the real serialized tree, so they cannot drift silently.
    bool presetDocumentIsWellFormed (const juce::XmlElement& root)
    {
        juce::StringArray ids;
        for (auto* e = root.getFirstChildElement(); e != nullptr; e = e->getNextElement())
        {
            if (e->isTextElement())            return false;   // text or CDATA inside the root
            if (! e->hasTagName ("PARAM"))     return false;   // any other element
            if (e->getNumChildElements() != 0) return false;   // a PARAM node is a leaf

            // THE ATTRIBUTE SET, COUNTED RATHER THAN SIZED, so one rule covers three failures at
            // once: an attribute this format does not define, a missing `id` or `value`, and the
            // SAME attribute written twice -- which a ValueTree would silently collapse to one and
            // a sized check would miss whenever a duplicate replaced an absentee.
            //
            // `raw` IS ONE OF OURS, and this round measured that rather than assuming it. The
            // comment at `soundSignatureAfterRestoring` said a preset file never carries `raw`;
            // it does, whenever the preset is saved after an undo, a redo or an A/B apply, because
            // those install a state-set tree that carries `raw` through `replaceState` and
            // `saveUser`'s `apvts.copyState()` then writes the live tree as it stands. Measured on
            // `0e32e65`: State tests 10, 18 and 35 all save such a file. The LOADER still resolves
            // a preset through `value` alone (`normalisedFromSavedTree`), so nothing about the
            // sound depends on it -- but refusing the attribute would have refused files this
            // plug-in itself writes, which is the one thing this boundary must never do.
            int idCount = 0, valueCount = 0, rawCount = 0;
            for (int a = 0; a < e->getNumAttributes(); ++a)
            {
                const auto& attribute = e->getAttributeName (a);
                if      (attribute == "id")    ++idCount;
                else if (attribute == "value") ++valueCount;
                else if (attribute == "raw")   ++rawCount;
                else return false;
            }
            if (idCount != 1 || valueCount != 1 || rawCount > 1) return false;

            const auto id = e->getStringAttribute ("id");
            if (id.isEmpty() || ids.contains (id)) return false;   // nameless, or a second node
            ids.add (id);                                          // claiming the same parameter
        }
        return true;
    }
}

// A preset file is accepted only when it is a well-formed document AND its root
// is the type this plug-in writes (`apvts.state.getType()`, "ANAMORPH"). Both
// conditions fail the same way -- an invalid tree -- because to a loader they are
// the same event: this file is not one of ours.
//
// WHY THE CHECK CANNOT LIVE IN applySoundTree, which is where it looks like it
// belongs (ER-STATE-24). That function resolves each parameter with
// `getChildWithProperty ("id", ...)`, which searches by PROPERTY and does not
// care what the root is called. Under a foreign root it therefore does two wrong
// things at once, and only the second was reported: every parameter the document
// lacks takes the "absent means default" branch written for a genuinely missing
// PARAM node -- and every parameter it happens to NAME is ADOPTED. Measured on a
// two-child `<SomeOtherPluginPreset>` against a non-default sound: `drive` and
// `width` took the foreign file's values (0.95 and 0.05 plain), while
// `algorithm`, `monoMakerFreq` and `chorusRate` were reset to their defaults --
// and `loadFile` returned TRUE. So the distinction has to be made on the ROOT,
// before any per-parameter fallback can reinterpret the document; making the
// fallback "keep the current value" would have left a foreign preset ACCEPTED
// and merely inert, which is a different and weaker contract.
//
// The rule is not invented here. ER-STATE-02 settled exactly this question for
// A/B slot payloads -- `readSlot`'s `adoptIfAnamorph` accepts only
// `apvts.state.getType()` and refuses a foreign-typed tree precisely as it
// refuses an unparsable one -- and a preset is the same kind of payload asking
// the same question, so it gets the same answer rather than a second one.
//
// ADR-0055 (0.9.9) ADDED A BOUNDARY IN FRONT OF THAT RULE AND LEFT THE RULE ITSELF ALONE.
// The root test above answers "is this one of ours"; it never answered "is this ONE document,
// and is it safe to hand to a parser at all". Measured on 0e32e65 against the pinned JUCE 9.0.2,
// three things went wrong before this function ever held a tree, and one after it:
//
//   * TWO COMPLETE DOCUMENTS IN ONE FILE LOADED, AND THE FIRST WON. `parseDocumentElement` reads
//     ONE element and returns it; nothing looks at what follows. Preset A followed by Preset B
//     applied A, and so did a valid preset followed by prose, by a stray `<JUNK/>`, or by raw
//     binary. The file is corrupt and it was being applied.
//   * NESTING CRASHED THE PARSER. `readNextElement` and `readChildElements` are mutually
//     recursive with no bound: SIGSEGV between 25 000 and 30 000 levels on an 8 MB stack, and
//     between 2 500 and 3 000 -- a 21 KB file -- on a 1 MB one.
//   * A DOCTYPE HUNG THE MESSAGE THREAD, AND READ ARBITRARY FILES. A 220-byte file whose DOCTYPE
//     defines recursive entities left `XmlDocument::expandEntity` running after 60 s; and
//     `<!DOCTYPE x SYSTEM "...">` resolves through `FileInputSource::createInputStreamFor` ->
//     `File::getSiblingFile`, which honours an absolute path and `../` traversal alike.
//   * A FOREIGN CHILD, A DUPLICATED `id` OR A TEXT NODE WAS SILENTLY IGNORED -- so a document
//     that means two different things at once was accepted and half-applied.
//
// The first three are properties of the BYTES and are refused before `juce::parseXML` sees them,
// because a parser that has already crashed or hung cannot be corrected afterwards. The fourth is
// a property of the parsed document and is refused after. What is NOT refused is every tolerance
// this format documents: a missing `PARAM` still means that parameter's default, a malformed
// `value` still resolves through `SerializedNumber.h`, an `id` this build does not know still
// loads, `<ANAMORPH/>` with no children at all is still a valid preset that means "all defaults",
// and the root's own attributes are still free. Those are forward and backward compatibility, and
// narrowing them would break real files rather than corrupt ones.
juce::ValueTree PresetManager::parseSoundFile (const juce::File& f) const
{
    // Size first: it is the only question answerable without reading anything, and it is what
    // bounds the read below. `getDocumentElement` takes the WHOLE file into memory before parsing
    // -- measured peak RSS tracked file size linearly to 264 MB for a 256 MB file, with no cap --
    // and a preset this plug-in writes is 1525 bytes.
    if (! f.existsAsFile() || f.getSize() > maxPresetBytes) return {};

    // TEST SEAM. Between the size check and the read is where "another process replaced this file"
    // happens, and it is the only point at which that is reproducible. Empty in production.
    if (beforePresetRead) beforePresetRead (f);

    // THE SIZE CHECK ABOVE DOES NOT BOUND THE READ, AND THAT IS THE POINT OF THIS ONE (0.9.9).
    // `getSize()` describes the file at the instant it is asked; the read happens afterwards, and
    // anything may have replaced the file in between -- a sync client finishing a download, an
    // editor's save-over, another process writing the same name. `File::loadFileAsData`, which
    // this used to call, re-stats and reads the WHOLE file (`juce_File.cpp:559-566`), so the
    // replacement's entire content entered memory and then went on to the scans below with the cap
    // never re-applied: the one check that exists to keep an enormous file out of memory was
    // decided on a file that was no longer there.
    //
    // So the read is BOUNDED -- at most one byte past the cap, which is all that is needed to tell
    // "at the cap" from "over it" -- and the cap is then re-applied to what actually arrived. The
    // early rejection above is kept: it is free, it is right in the overwhelming majority of
    // cases, and it is what stops a genuinely oversized file from being opened at all.
    //
    // What is NOT re-checked is `loadFileAsData`'s other rule, that the file's length was the same
    // before and after. A file that shrank under the read now yields its new content, and a torn
    // write is refused by the document scans below (an incomplete `<ANAMORPH>` is not one
    // well-formed document) rather than by a length comparison -- which is the same answer, reached
    // by the rule this boundary actually states.
    juce::FileInputStream in (f);
    if (! in.openedOk()) return {};

    // THE BOUND IS SPELLED `int`, AND THAT IS PORTABILITY RATHER THAN LAZINESS. The parameter's
    // type is `ssize_t`, which JUCE declares ITSELF only under `#if JUCE_WINDOWS`
    // (`juce_MathsFunctions.h:97-99`) and takes from the system headers everywhere else -- so
    // naming the type unqualified compiles on Linux and macOS, where POSIX puts it in the global
    // namespace, and fails on MSVC with `C2065: 'ssize_t': undeclared identifier`, because
    // `juce::ssize_t` is not visible from this namespace. 256 KB + 1 fits an `int` on every
    // platform this ships to, and an `int` converts to whichever `ssize_t` is in play.
    juce::MemoryBlock raw;
    in.readIntoMemoryBlock (raw, (int) (maxPresetBytes + 1));
    if ((juce::int64) raw.getSize() > maxPresetBytes) return {};

    if (! presetBytesAreAdmissible (raw.getData(), raw.getSize())) return {};

    // DECODED EXACTLY AS `loadFileAsString` DECODED IT, which is not a tidying but the point: that
    // function is `readEntireStreamAsString` -> `MemoryOutputStream::toString` -> this same call
    // (juce_File.cpp:568-576, juce_InputStream.cpp:241-246, juce_MemoryOutputStream.cpp:207-210),
    // so a byte-order mark and a UTF-16 preset still read precisely as they did before this guard.
    // `MemoryBlock::toString` would NOT do: it is `String::fromUTF8`, which keeps a UTF-8 mark as a
    // character and cannot read UTF-16 at all. The cast is bounded by the size check above.
    const auto text = juce::String::createStringFromData (raw.getData(), (int) raw.getSize());
    if (! presetTextIsAdmissible (text, maxPresetDepth)) return {};

    // PARSED FROM THE TEXT, NOT FROM THE FILE, and that is a second guard rather than a tidying:
    // an `XmlDocument` built from a string carries no `InputSource`, so `getFileContents` -- the
    // external-entity door measured above -- has nothing to open even if a DOCTYPE ever reached
    // it. The pre-scan refuses one anyway; this makes the refusal structural as well as textual.
    // `loadFileAsString` resolves a UTF-8 or UTF-16 byte-order mark exactly as the file-based
    // parse did, so nothing a legitimate preset can carry is read differently.
    auto xml = juce::parseXML (text);
    if (xml == nullptr) return {};
    if (! xml->hasTagName (apvts.state.getType().toString())) return {};
    if (! presetDocumentIsWellFormed (*xml)) return {};
    return juce::ValueTree::fromXml (*xml);
}

// A preset file is user-editable text and `nan` parses, so a value is adopted only
// when it is usable -- otherwise the parameter default applies, exactly as it does
// for a missing child. A non-finite parameter silences the plug-in for the rest of
// the session (see reassertParameters for the full mechanism); the session path is
// guarded there. "Absent means default" holds for a value-less node too -- the writer
// is apvts.copyState().createXml(), which always emits `value`, so nothing this
// plug-in saves takes that branch. All of it now lives in normalisedFromSavedTree,
// which is also what soundSignatureForSavedTree resolves through: this function and
// the baseline it will be judged against read the file through ONE rule (§17).
void PresetManager::applySoundTree (const juce::ValueTree& state)
{
    // §24: one whole-sound replacement at a time. Without it this loop and a host thread's
    // decode-install interleave, and the sound that settles is neither the preset's nor the
    // restore's but a mixture of the two.
    const juce::ScopedLock oneAtATime (replacementLock());
    int written = 0;
    for (auto* p : apvts.processor.getParameters())
        if (auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            if (! pid::isPresetExcluded (wid->paramID))
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                {
                    if (++written == 4 && insideReplacement) insideReplacement();
                    anamorph::param::setValueNotifyingHost (rp, normalisedFromSavedTree (*rp, state, wid->paramID));
                }
    resetSolo();
    if (noteReplaced) noteReplaced();   // completion, published before the scope closes (§24)
}

// THE SIGNATURE OF THE LIVE STATE A SAVED TREE WAS CAPTURED FROM (D-2 round 9,
// ADR-0036 §17). This is the clean baseline of a preset that was just written, and it
// is derived from the BYTES rather than from a second read of the live parameters --
// which is the whole point: no mutation, however busy, can separate a baseline from
// the file it describes when the baseline is computed FROM that file.
//
// It resolves each parameter through the same normalisedFromSavedTree the loader uses,
// and equals soundSignatureFor of the captured live state bit for bit (State test 52
// measures it over the whole parameter set). It is deliberately NOT the signature the
// parameters will report once the tree is LOADED -- a load stores and reports through
// the parameter's own pass, one range mapping deeper; that is
// soundSignatureAfterLoading, and State test 55 measures why the two must differ.
//
// THE NON-RANGED BRANCH IS UNREACHABLE IN THIS PLUG-IN and is a fallback, not a claim.
// Every parameter `createAnamorphLayout` builds is a RangedAudioParameter (stock
// AudioParameterFloat, or the RawChoice / RawBool / RawInt subclasses), which State test
// 52's sweep leg asserts rather than assumes. If one ever were not, a preset could carry
// no value for it and a load would leave it untouched, so its live value is the only
// defensible contribution -- but it is then the live value AT SAVE TIME, which is the one
// place this function's "what the file will restore" contract is approximate. Nothing in
// the tree can make it exact, because the tree holds nothing for such a parameter.
// THE SIGNATURE THE LIVE PARAMETERS WILL PRODUCE AFTER A LOAD, from the values the
// load is about to write (D-2 round 10, ADR-0036 §18). `resolve` answers "what
// normalised value will this parameter be SET to" -- from a saved tree for a user
// preset, from the override table for a factory one -- and the signature models
// what `soundSignatureFor` then reads back: the parameter stores convertFrom0to1(x)
// and reports convertTo0to1 of it (one `normalisedAsRendered`), and the signature
// itself renders that once more. Same arithmetic, same inputs, so the post-load live
// signature equals this bit for bit, for every parameter kind: for the custom
// RawChoice / RawBool the value is on-grid and both passes are the identity, for an
// interval-snapped float the first pass is a projection and the second is idempotent
// on it, and for the four log-mapped frequency ranges -- where the pass is NOT
// idempotent -- both sides apply it exactly twice. This is what lets a load fix its
// baseline WITHOUT reading the parameters back, which is the whole point: a
// read-back has a window that host automation can land in (KI-029).
//
// NO TOLERANCE, AND NONE IS NEEDED (D-2 round 11, ADR-0036 §19). Round 10 added a
// reconciliation here: it compared this prediction against one live read-back and took the
// LIVE value where the two agreed to within 1e-6, on the belief that the prediction was not
// bit-exact across toolchains (the ThreadSanitizer build had failed the equality). That
// tolerance was a hole: a tolerance cannot tell compiler noise from a real automation write
// of the same size, so an automation write inside the load window was ABSORBED INTO THE
// BASELINE and the preset read clean against a sound it does not hold.
//
// WHAT ROUND 11 ESTABLISHED, and what it did not. Round 10's stated mechanism was
// fused-multiply-add contraction differing between the inlined prediction chain and the
// value routed through the parameter. That mechanism is REFUTED for the build in which the
// equality failed: ADR-0031 compiles every x86-64 target, this suite included, with
// `-march=haswell -ffp-contract=off`, measured at 0 FMA instructions emitted, and the
// ThreadSanitizer build is one of them. Re-measured in round 11, 20000 random values x 33
// parameters and 3000 full save/XML/load round trips, in BOTH the Release and the
// ThreadSanitizer build: the prediction equals the post-load live signature at every point,
// bit for bit, and the XML text round-trips every value exactly.
//
// The CAUSE of round 10's failure is a candidate, not a measured fact, and is recorded as
// one: two suite instances sharing a fixed probe path in the temp folder reproduce hundreds
// of mismatches on demand, with value differences up to ~1.9e4 -- one process reading the
// other's file -- and round 10 itself later root-caused exactly that collision for a
// different test (docs/procedures/TESTING.md: one instance at a time). Round 10's own
// reproduction, though, listed failures in tests 10, 12, 13, 24, 28 and 52, not 55. So: the
// arithmetic is exonerated, the tolerance it bought was unjustified, and the specific
// interleaving that produced the red run is not claimed to be known.
//
// So the signature has ONE equivalence, stated once: two sounds are the same when their
// rendered normalised values agree to five decimal places. That quantisation is explicit,
// symmetric and applied identically on every path -- nothing is layered on top of it, here
// or anywhere else (State test 57 pins the parameter-set margin; the restore path's own
// 1e-6 write gate went with it, see reassertParameters).
//
// WHAT THE QUANTISATION ITSELF ABSORBS, stated rather than implied, because it is a
// resolution and not a proof of nothing. One signature bucket is 1e-5 of normalised range.
// For a parameter whose rendering lands on a GRID -- 29 of the 33 preset-carried parameters
// -- one bucket is far finer than one grid cell, so no two legal settings can share a
// bucket: measured, the smallest cell in the whole set is 8.08e-5 on Chorus Rate (the only
// skewed range: {0.05, 5.0, interval 0.001, skew 0.4}, at the top of its range), i.e. 8.08
// buckets, and every other grid parameter is wider. State test 57 asserts that margin over
// the whole parameter set rather than quoting it, so a future range change that narrowed a
// cell below one bucket fails the suite instead of silently making one full step of a real
// control read clean. For the four GRIDLESS log-mapped frequency ranges there is no cell,
// and the bucket is the only resolution there is: at worst 1.38 Hz at the 20 kHz end of the
// three multiband splits and 0.013 Hz on Mono Maker Freq. Movement smaller than that, at a
// value that does not straddle a bucket boundary, is not reported -- and it does not
// accumulate away either: successive sub-bucket writes cross a boundary and the preset then
// reads dirty and stays dirty, which State test 57's accumulation leg drives.
namespace
{
    template <typename Resolve>
    juce::String signatureAfterApplying (const juce::AudioProcessorValueTreeState& s, Resolve&& resolve)
    {
        juce::String sig;
        for (auto* p : s.processor.getParameters())
            if (auto* wid = dynamic_cast<const juce::AudioProcessorParameterWithID*> (p))
                if (! pid::isPresetExcluded (wid->paramID))
                {
                    const auto* rp = dynamic_cast<const juce::RangedAudioParameter*> (p);
                    sig << juce::String (rp != nullptr
                                           ? normalisedAsRendered (*rp, normalisedAsRendered (*rp, resolve (*rp, wid->paramID)))
                                           : p->getValue(), 5) << ',';
                }
        return sig;
    }
}

juce::String PresetManager::soundSignatureAfterLoading (const juce::AudioProcessorValueTreeState& s,
                                                        const juce::ValueTree& savedSound)
{
    // From the tree alone. Nothing live is read, so there is no window for automation to
    // land in and nothing to reconcile: this IS the baseline the load paths set.
    return signatureAfterApplying (s, [&] (const juce::RangedAudioParameter& rp, const juce::String& id)
    {
        return normalisedFromSavedTree (rp, savedSound, id);
    });
}

juce::String PresetManager::soundSignatureAfterRestoring (const juce::AudioProcessorValueTreeState& s,
                                                          const juce::ValueTree& sessionSound)
{
    // From the tree alone, through the session resolver (ADR-0037): `raw` first, then `value`,
    // then the default -- what reassertParameters will assert, one store/report pass before the
    // signature's own. Nothing live is read.
    return signatureAfterApplying (s, [&] (const juce::RangedAudioParameter& rp, const juce::String& id)
    {
        return anamorph::sessionNormalisedValue (rp, sessionSound.getChildWithProperty ("id", id)).normalised;
    });
}

juce::String PresetManager::soundSignatureForSavedTree (const juce::AudioProcessorValueTreeState& s,
                                                        const juce::ValueTree& savedSound)
{
    juce::String sig;
    for (auto* p : s.processor.getParameters())
        if (auto* wid = dynamic_cast<const juce::AudioProcessorParameterWithID*> (p))
            if (! pid::isPresetExcluded (wid->paramID))
            {
                // NOT canonicalised again here, and that is load-bearing. The tree already
                // holds the DENORMALISED value, so `normalisedFromSavedTree` returns
                // convertTo0to1(convertFrom0to1(live)) -- character for character the
                // expression the live side computes. Applying the canonicaliser a second
                // time would make this side convertTo0to1(convertFrom0to1(...)) twice, and
                // for the four frequency parameters whose range carries custom log/exp
                // conversion lambdas and no interval to snap to, that mapping is the
                // identity in real arithmetic but NOT idempotent in float: the two sides
                // then part company in the last bits and, at a 5-decimal rounding boundary,
                // in the printed signature -- a freshly saved preset reading MODIFIED.
                // Measured at 4 sweep points in 20001 before this was removed; State test
                // 52's sweep leg is dense enough to see it.
                const auto* rp = dynamic_cast<const juce::RangedAudioParameter*> (p);
                sig << juce::String (rp != nullptr
                                       ? normalisedFromSavedTree (*rp, savedSound, wid->paramID)
                                       : p->getValue(), 5) << ',';
            }
    return sig;
}

PresetManager::OpResult PresetManager::load (int index, std::function<void (bool)> onComplete)
{
    // ADR-0008 round 25 (R1279-1283), ADR-0036 round 28 (R802-807): not inside a user transaction,
    // not inside a dispatch of ours, and not while a whole-sound replacement is in flight. The gate
    // also carries the drain that used to be the next statement -- hoisted into `load` in round 16
    // (§23) so it runs BEFORE anything is derived rather than after -- with the non-blocking arm.
    // The replacement lock is HELD from here to the end of the load, which is what makes
    // `applyDefaults`' and `applySoundTree`'s own acquisitions free recursive re-entries.
    //
    // ROUND 43 (0.9.9): the deferral carries the completion, so a load queued behind a transaction
    // still answers its caller exactly once when it finally runs -- the same shape `loadFile` has
    // had since round 27, and the reason the retry re-enters this PUBLIC entry point rather than
    // the core.
    const auto admit = stateCommandAdmission ([this, index, cb = onComplete]
                                              { (void) load (index, cb); });
    if (! admit.admitted()) return OpResult::deferred;
    return loadAdopted (index, std::move (onComplete));
}

PresetManager::OpResult PresetManager::loadAdopted (int index, std::function<void (bool)> onComplete)
{
    // No tree in hand: the row is read here, exactly as it always was. This is the ABSOLUTE door --
    // `load(index)`, the menu row -- and its rules are unchanged, missing-versus-corrupt included.
    return loadAdopted (index, juce::ValueTree(), std::move (onComplete));
}

PresetManager::OpResult PresetManager::loadAdopted (int index, const juce::ValueTree& preParsed,
                                                    std::function<void (bool)> onComplete)
{
    // ADR-0008 round 25 (R1279-1283), round 28: guarded separately because it is public and both
    // `load` and `step` reach the row through it; when they deferred, this runs with nothing to
    // defer, and when they did not, its acquisition is their lock re-entered on the same thread.
    // NO DRAIN (`drainFirst == false`), which is the whole of what "Adopted" names: `step` derived
    // its row from the session the outer drain established, and draining again here would adopt a
    // restore AFTER that row was chosen and load it onto the wrong session (§23).
    //
    // THE RETRY DROPS THE CARRIED TREE ON PURPOSE (0.9.9). A deferral means the command waits for a
    // door that may be many milliseconds away and the list may be rebuilt before it opens, so
    // `index` may not name the row it named when the tree was parsed -- and applying that tree
    // under this row's name would make the sound and the preset name disagree. Re-deriving from
    // the row is the honest answer, and it is what this function did before the tree existed. The
    // window the tree closes is the SYNCHRONOUS one inside a single `step`, which a deferral has
    // already left behind. Unreachable from `step` in any case: the gate asks `refuseNow` before
    // the nesting shortcut (`StateCommandGate.h`), and between `step`'s admission and this one
    // nothing runs but this thread's own reads, so neither a user transaction nor a dispatch of
    // ours can open in between.
    const auto admit = stateCommandAdmission ([this, index, cb = onComplete]
                                              { (void) loadAdopted (index, cb); }, /*drainFirst*/ false);
    if (! admit.admitted()) return OpResult::deferred;

    // ROUND 43 (0.9.9, ADR-0055). EVERY REFUSAL BELOW IS NOW REPORTED. It used to be a bare
    // `return`, so the editor could not tell a load that happened from one that did not; the rule
    // that a refusal is a clean no-op is unchanged, and all that is added is that it says so.
    const auto fail = [&onComplete] { if (onComplete) onComplete (false); return OpResult::failed; };

    if (index < 0 || index >= list.size()) return fail();
    // COPIED, NOT REFERENCED. The missing-file branch below calls `refresh()`, which clears and
    // rebuilds `list` -- a reference into it would dangle from that statement on. The Entry is four
    // small members.
    const Entry e = list.getReference (index);

    // Resolve EVERYTHING that can fail BEFORE opening the undo bracket: a failure must be a
    // clean no-op, never an onAboutToLoad() with no matching onLoaded() (which would flush undo
    // coalescing yet record no step, leaving the undo timeline half-open). Mirrors loadFile().
    juce::ValueTree userSound;
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
        if (factory == nullptr) return fail();
    }
    else
    {
        // MISSING AND INVALID ARE DIFFERENT EVENTS (0.9.9, ADR-0055), and only one of them is
        // about the file's CONTENTS. A row whose file has been deleted or renamed since the last
        // scan is describing something that is no longer there, so the list is rescanned and the
        // row goes with it. A row whose file is still on disk but is not a readable preset KEEPS
        // its row: the user can see the file, the plug-in must not quietly disagree about whether
        // it exists, and nothing here ever deletes or hides a file the user put in the folder.
        // CARRIED FROM THE CANDIDATE PASS (0.9.9), and then the file is not touched again at all --
        // no second parse, and no second existence check either. `step` has already read this row
        // and decided on it, and the tree it read IS the preset the user asked for; re-reading
        // could only replace it with something the decision was never made about. It mirrors
        // `applyParsedFile`, which likewise applies the tree the chooser's parse produced without
        // asking again whether the file is still there.
        if (preParsed.isValid())
        {
            userSound = preParsed;
        }
        else
        {
            // MISSING AND INVALID ARE DIFFERENT EVENTS -- see above. Both belong to the absolute
            // door, which is the only one that reaches this branch.
            if (! e.file.existsAsFile())
            {
                refresh();
                return fail();
            }

            // Unparsable, foreign-rooted, more than one document, or structurally malformed -> the
            // same clean no-op, resolved here so it lands before onAboutToLoad() like every other
            // failure (ER-STATE-24, and ER-GUI-06's rule that a refused load raises no duck).
            userSound = parseSoundFile (e.file);
            if (! userSound.isValid()) return fail();
        }
    }

    if (onAboutToLoad) onAboutToLoad(); // flush any settled edit so the pre-load state is the undo baseline

    // THE BASELINE IS FIXED FROM WHAT THE LOAD WRITES, NOT FROM A READ-BACK (D-2 round
    // 10, ADR-0036 §18; this closes KI-029). It used to be `sigAtLoad = soundSig()` after
    // the apply -- the same two-read shape round 9 removed from the save: host automation
    // writing a sound parameter between the apply and that read made the baseline
    // describe the automated value, so the preset read CLEAN whenever the automation
    // returned to it while reloading it would move the sound. The signature is now
    // computed from the values being applied, through the same arithmetic the parameters
    // will report them by, so nothing live is read and there is no window.
    juce::String applied;
    if (e.isFactory)
    {
        // ONE AT A TIME (§24), AND THE TWO PARTS ARE ONE REPLACEMENT. A factory preset is applied
        // as every parameter to its default followed by the table's overrides; a restore landing
        // between the two halves would settle a sound that is part factory default, part session,
        // which is the same mixture the tree-shaped replacements are excluded from. The lock is
        // recursive, so applyDefaults() taking it again inside this scope is a no-op.
        {
            const juce::ScopedLock oneAtATime (replacementLock());
            applyDefaults();
            // Resolved through the ID, not the list position: the entry already carries the
            // identity the selection is about to adopt, so the two can never disagree (#4).
            for (const auto& o : factory->set)
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (o.id)))
                    anamorph::param::setValueNotifyingHost (rp, rp->convertTo0to1 (o.value));
            if (noteReplaced) noteReplaced();   // completion, published before the scope closes (§24)
        }
        // The resolver mirrors the two writes above: an override's value where the table
        // names the parameter, the default applyDefaults() wrote everywhere else. (The
        // signature only ever asks about preset-carried parameters, so an override on a
        // preset-EXCLUDED id -- none exists in the table -- would be applied above yet
        // never signed here, which is the correct answer for an excluded field.)
        if (beforeStateCapture) beforeStateCapture();   // test seam: after the apply, before the baseline
        applied = signatureAfterApplying (apvts, [&] (const juce::RangedAudioParameter& rp, const juce::String& id)
        {
            for (const auto& o : factory->set)
                if (id == o.id) return rp.convertTo0to1 (o.value);
            return rp.getDefaultValue();   // applyDefaults() wrote exactly this
        });
    }
    else
    {
        applySoundTree (userSound);
        if (beforeStateCapture) beforeStateCapture();   // test seam: after the apply, before the baseline
        applied = soundSignatureAfterLoading (apvts, userSound);
    }

    current = e.name;
    sel = e.isFactory ? Selection { Selection::Kind::factory,  e.factoryId, {} }
                      : Selection { Selection::Kind::userFile, {},          e.file };
    sigAtLoad = applied;
    if (onMetaChanged) onMetaChanged();
    if (onLoaded) onLoaded(); // record the switch as ONE undo step (name/baseline now reflect the new preset)
    if (onComplete) onComplete (true);
    return OpResult::completed;
}

// ADR-0055. THE ROW CANDIDATE `step` DERIVES ITS SKIP FROM. Deliberately a pure question: it moves
// no parameter, opens no undo bracket and takes no admission of its own, so asking it about a row
// `step` then declines to load costs that row nothing.
//
// 0.9.9: IT CARRIES THE TREE IT PARSED. Answering only `bool` meant the chosen row was read a
// SECOND time inside `loadAdopted`, and between the two reads the file can change: a row that was
// loadable when it was chosen could then fail to parse, `step` would return `failed`, and
// navigation would stop at exactly the row the skip exists to step past. The tree the decision was
// made from is now the tree the load applies -- the rule `loadFile` has followed since round 27,
// where the parse also happens once, before the command is queued, because "the parsed tree IS the
// preset: a file edited in the meantime cannot change what the user asked to load".
PresetManager::RowCandidate PresetManager::examineRow (int index) const
{
    if (index < 0 || index >= list.size()) return {};
    const auto& e = list.getReference (index);
    if (e.isFactory) return { findFactory (e.factoryId) != nullptr, {} };

    auto sound = parseSoundFile (e.file);
    const bool loadable = sound.isValid();
    return { loadable, std::move (sound) };
}

// ROUND 27 (Devin R640). THE PARSE IS SYNCHRONOUS, THE APPLY MAY BE DEFERRED, AND THE CALLER IS
// TOLD WHICH.
//
// Round 25's version returned `true` when it had merely QUEUED the load, with the deferred
// re-entry written `(void) loadFile (f)` -- so a file that turned out to be another plug-in's
// preset was refused into a void while the editor had already swept the knobs and refreshed the
// display for a load that never happened.
//
// The split is what makes the contract honest rather than a promise. "Is this an Anamorph preset"
// is knowable NOW -- it is a property of the bytes, not of the plug-in's state -- so it is decided
// before anything is queued (section 9), and the half that remains cannot fail. Parsing before the
// deferral is also one read instead of two, and the parsed tree IS the preset: a file edited in the
// meantime cannot change what the user asked to load.
//
// THE DRAIN STAYS WHERE IT WAS. `adoptPending` runs at the instant the apply happens, not when the
// command is queued, so a host restore arriving during the transaction is still adopted before the
// preset lands on it (ADR-0036 sections 18 and 23).
PresetManager::OpResult PresetManager::loadFile (const juce::File& f,
                                                 std::function<void (bool)> onComplete)
{
    // Unparsable OR foreign-rooted -> failed, and nothing is touched: the chooser can point at any
    // file on the machine, so this is the path a user is most likely to hand another plug-in's
    // preset to (ER-STATE-24). Decided here, before any queuing, because it is decidable here.
    auto sound = parseSoundFile (f);
    if (! sound.isValid())
    {
        if (onComplete) onComplete (false);
        return OpResult::failed;
    }

    // ADR-0008 round 25 (R1279-1283): the OS-chooser load is the same state-replacing command by
    // another door, so it waits for an open user transaction exactly as Undo does.
    // COPIED, NOT MOVED, and the test that found this is State test 102 leg A. An init-capture is
    // evaluated when the LAMBDA is constructed -- which happens before `deferIfBusy` is called and
    // regardless of what it answers -- so `cb = std::move (onComplete)` emptied `onComplete` even
    // on the path that then ran synchronously, and the synchronous caller was told nothing at all.
    // A `std::function` copy is cheap and cannot do that.
    // ROUND 28: the retry re-enters `applyParsedFileAdmitted`, NOT `applyParsedFile` directly. A
    // deferred command is run by the processor's flush, which holds nothing -- so a retry that
    // called the body straight would reach `applySoundTree`'s blocking acquisition with no
    // admission of its own, which is the R802 door by a second entrance.
    const auto admit = stateCommandAdmission ([this, f, sound, cb = onComplete]
                                              { applyParsedFileAdmitted (f, sound, cb); });
    if (! admit.admitted()) return OpResult::deferred;

    applyParsedFile (f, sound);
    if (onComplete) onComplete (true);
    return OpResult::completed;
}

// Everything `loadFile` used to do once the bytes had proved themselves. One body, so the deferred
// path and the synchronous one cannot drift.
// ROUND 28 (R802-807). The deferred half of `loadFile`, and the admission is retaken rather than
// assumed: a queued command runs from `flushDeferredCommands`, which holds no replacement lock of
// its own by the time it calls out. Refused again, this re-queues itself through the gate and the
// completion is NOT called -- the operation is still pending, and R640's contract is that the
// completion reports the FINAL result exactly once, not once per attempt.
void PresetManager::applyParsedFileAdmitted (const juce::File& f, const juce::ValueTree& sound,
                                             const std::function<void (bool)>& completion)
{
    const auto admit = stateCommandAdmission ([this, f, sound, completion]
                                              { applyParsedFileAdmitted (f, sound, completion); });
    if (! admit.admitted()) return;

    applyParsedFile (f, sound);
    if (completion) completion (true);
}

// The same for the deferred save, with the same admission `saveUser` takes -- the bare APVTS
// acquisition inside `writeUserPreset` is there whichever door reaches it.
void PresetManager::writeUserPresetAdmitted (const juce::String& legalName,
                                             const std::function<void (bool)>& completion)
{
    const auto admit = stateCommandAdmission ([this, legalName, completion]
                                              { writeUserPresetAdmitted (legalName, completion); });
    if (! admit.admitted()) return;

    const bool ok = writeUserPreset (legalName);
    if (completion) completion (ok);
}

void PresetManager::applyParsedFile (const juce::File& f, const juce::ValueTree& sound)
{
    // ROUND 28: the drain that was here is the admission's, taken at `loadFile`'s top (or at the
    // top of the deferred retry) -- before the lock, never underneath it.
    if (onAboutToLoad) onAboutToLoad(); // flush any settled edit so the pre-load state is the undo baseline
    applySoundTree (sound);
    if (beforeStateCapture) beforeStateCapture();   // test seam: after the apply, before the baseline
    current = f.getFileNameWithoutExtension();
    // The chooser can point ANYWHERE, so the file is the identity whether or not it
    // lives in the preset folder; a file from outside simply matches no list row and
    // leaves the menu unticked, which is what it should show (#4).
    sel = { Selection::Kind::userFile, {}, f };
    sigAtLoad = soundSignatureAfterLoading (apvts, sound);   // from the bytes, no live read (§18, KI-029; §19)
    if (onMetaChanged) onMetaChanged();
    if (onLoaded) onLoaded(); // record the switch as ONE undo step (name/baseline now reflect the new preset)
}

PresetManager::OpResult PresetManager::step (int delta, std::function<void (bool)> onComplete)
{
    // ADR-0008 round 25 (R1279-1283): deferred at the OUTERMOST entry point on purpose -- a step
    // is relative, so re-running `step` later re-derives the row from the state it lands on,
    // while deferring the absolute load it computes would carry a mid-transaction row forward.
    // The seam fires from INSIDE the admission, between its drain and its try (§23): that is the
    // window this test seam exists for -- a restore arriving after the fixed point, which the step
    // must therefore NOT act on.
    const auto admit = stateCommandAdmission ([this, delta, cb = onComplete] { (void) step (delta, cb); },
                                              /*drainFirst*/ true,
                                              [this] { if (beforeRelativeTarget) beforeRelativeTarget(); });
    if (! admit.admitted()) return OpResult::deferred;
    if (list.isEmpty()) { if (onComplete) onComplete (false); return OpResult::failed; }
    // The step is RELATIVE to the current row, so the current row must be the authoritative
    // one: a pending host restore that moves the selection is adopted before it is read (D-2
    // round 10, §18). Without this, "next" from a session that had just been replaced landed
    // on the row after the OLD selection -- the same stale-derivation shape as the A/B toggle.
    // ROUND 28: that drain -- and the seam that used to be the next line -- are the admission's,
    // two statements above and still the FIRST thing this command does, which is the only property
    // §18 needs of them.

    // ...AND THE SELECTION MUST STILL BE THAT ONE WHEN THE ROW IS LOADED (§23, round 16). The
    // drain above was not enough on its own while `load` drained again on the way in, and the
    // `pollUndoCoalesce` inside its `onAboutToLoad` drained a third time: a restore landing in
    // between was adopted there, AFTER `cur` had been read from the session it replaced, and
    // "next" then loaded the row after the OUTGOING selection onto the incoming session. Those
    // drains are correct for `load`'s absolute callers, so they were not removed -- they were
    // hoisted into `load` itself, and this calls the ALREADY-ADOPTED core below them.
    const int cur = currentIndex();
    const int n   = list.size();
    // Unknown current name steps from "Default"; otherwise wrap around the list.
    const int from = cur >= 0 ? cur : 0;

    // AN UNREADABLE ROW IS NOT A WALL (0.9.9, ADR-0055). "Next" asks for the one after this, so a
    // row that will not load is stepped OVER rather than stopped at. Before this, a failed load
    // left `current` where it was and the next press re-derived the SAME row: measured on
    // `0e32e65` with one corrupt file between two good ones, four presses of Next gave the same
    // row four times and the preset beyond it was unreachable in both directions.
    //
    // The pass is bounded by the list length and stops when it comes back to where it started, so
    // a folder in which nothing loads answers `failed` once rather than spinning. The row is
    // chosen by `rowIsLoadable`, not by `loadAdopted`'s result, so exactly ONE `loadAdopted` runs
    // and it is the one that owns the completion -- a skip driven by the result would have to
    // decide what to do with a `deferred` whose retry carries no completion.
    //
    // §23 IS UNCHANGED: the drain is still the admission's, still the first thing this command
    // does, and every row considered here is derived from the session it established. `loadAdopted`
    // is still called with `drainFirst == false` so nothing adopts underneath a chosen row.
    for (int taken = 1; taken <= n; ++taken)
    {
        const int target = ((from + delta * taken) % n + n) % n;
        if (target == from) break;                 // all the way round: nothing else to try
        const auto candidate = examineRow (target);
        if (! candidate.loadable) continue;
        return loadAdopted (target, candidate.sound, std::move (onComplete));
    }

    if (onComplete) onComplete (false);
    return OpResult::failed;
}

// ROUND 27 (Devin R640). A NAME IS JUDGED NOW; A DISK IS JUDGED WHEN IT IS WRITTEN.
//
// Round 25's version deferred FIRST and returned `true`, so the editor closed the Save dialog on a
// save that had not happened -- and the deferred re-entry was written `(void) saveUser (rawName)`,
// so when the write later failed (a read-only preset folder, a full disk, a name that resolves to
// a degenerate path -- see the tilde note below) nothing anywhere learned of it. The user had a
// closed dialog, an unchanged preset list, and no error.
//
// SO THE TWO KINDS OF FAILURE ARE SEPARATED (section 9). An illegal or empty name is a property of
// the ARGUMENT: knowable now, answered now, nothing queued. Everything else is a property of the
// FILESYSTEM at the moment of writing, which is exactly what cannot be known in advance -- so it
// travels on `onComplete`, and the initiating UI stays pending until it arrives.
PresetManager::OpResult PresetManager::saveUser (const juce::String& rawName,
                                                 std::function<void (bool)> onComplete)
{
    // The one failure that is decidable without touching the disk.
    const juce::String name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty())
    {
        if (onComplete) onComplete (false);
        return OpResult::failed;
    }

    // ADR-0008 round 25 (R1279-1283): a save writes no parameter, but the processor's `onSaved`
    // hook calls `syncCommitted`, which clears `pendingGestureCommit` -- so a save arriving
    // inside a transaction silently deletes that transaction's undo step. Deferred like the
    // loads, and for the better outcome too: the file then records the COMPLETED action.
    // Copied, not moved: see the note in `loadFile`. Moving here emptied the completion on the
    // synchronous path too, because the capture is evaluated before `deferIfBusy` answers.
    // ROUND 28 (R802-807), AND THE LOCK IS TAKEN EVEN THOUGH A SAVE REPLACES NOTHING. `saveUser`
    // writes no parameter, so §24's mutual exclusion is not what it needs it for -- what it needs
    // is the APVTS lock underneath it. `writeUserPreset`'s capture calls `apvts.copyState()`, which
    // opens `ScopedLock lock (valueTreeChanging)`, and that is the ONE bare acquisition of the APVTS
    // lock in this tree: every other one sits inside `soundReplacement` already. Reached from a
    // pumped click it is the same cycle by the other lock -- this thread holds a parameter's
    // `listenerLock` and waits for `valueTreeChanging`, while a host thread inside
    // `AudioProcessorValueTreeState::replaceState` holds `valueTreeChanging` and waits for that
    // `listenerLock`. Holding `soundReplacement` closes it because every thread that can hold
    // `valueTreeChanging` while waiting for a `listenerLock` must take `soundReplacement` FIRST:
    // both of this plug-in's `replaceState` sites are inside it (PluginProcessor.cpp
    // `applyStatePreservingView`, `applySoundTree`) and so is the host-thread `copyState` in
    // `copyStateWithRawValues`. That invariant is now a rule rather than an accident -- ADR-0036
    // §31 states it, and it is what this line depends on.
    //
    // THE COST IS A FILE WRITE UNDER THE LOCK, and it is stated rather than hidden: a host thread's
    // `setStateInformation` waits for as long as this save takes. A user preset is a few kilobytes
    // of XML and the lock is already held across every preset LOAD; the alternative is a residual
    // deadlock on a door the user presses by hand.
    //
    // The retry re-enters `writeUserPresetAdmitted` so a deferred save is admitted like any other.
    const auto admit = stateCommandAdmission ([this, name, cb = onComplete]
                                              { writeUserPresetAdmitted (name, cb); });
    if (! admit.admitted()) return OpResult::deferred;

    const bool ok = writeUserPreset (name);
    if (onComplete) onComplete (ok);
    return ok ? OpResult::completed : OpResult::failed;
}

// Everything `saveUser` used to do once the name had proved itself. `legalName` has already been
// through `createLegalFileName` and is non-empty, so the deferred execution re-derives nothing --
// the name the user typed is the name that gets written, whenever the write happens.
bool PresetManager::writeUserPreset (const juce::String& name)
{
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

    // ONE CAPTURE, ONE SNAPSHOT, ONE MEANING (D-2 round 9, ADR-0036 §17). The preset IS
    // this tree: `copyState()` flushes the live parameters into the APVTS tree under
    // JUCE's own lock and hands back a private copy, and everything downstream -- the
    // bytes on disk AND the clean baseline -- is derived from that one object. No
    // parameter change, however fast or however sustained, can land "between" two reads,
    // because there is only one read. (Precisely: one read of any parameter this plug-in
    // has. `soundSignatureForSavedTree` keeps a live fallback for a parameter that is not
    // a RangedAudioParameter, and Anamorph has none -- State test 52 asserts that rather
    // than assuming it.)
    //
    // `saveUser` itself contains no loop. The drain it calls through `onAboutToSave` is
    // §15's, whose termination argument is §11's supported-host boundary, not this
    // function's.
    //
    // WHAT THIS REPLACED and why the replacement is not a stronger version of it. Round
    // 8 took two reads -- a live signature and then the state copy -- and tried to prove
    // them coherent by re-reading the sound generation, retrying up to eight times. Two
    // things were wrong with that. The loop FELL THROUGH after eight failures and used
    // the unproven pair anyway, so sustained automation (which is exactly when the check
    // fails) was the case it silently stopped covering. And its stated fallback -- that a
    // disagreement "reads as dirty rather than as a false clean" -- does not hold: a
    // baseline describing an EARLIER sound reads clean again the moment the sound returns
    // to it, which is what cycling automation does by definition. The preset then sat
    // there marked clean while its file held a different sound.
    //
    // The retry is gone rather than bounded harder, because no number of retries can
    // establish an invariant that one capture gets for free.
    //
    // A pending host restore is adopted BEFORE that capture (D-2 round 8, §16). It used
    // to be adopted after the file had been written, which put the restore between the
    // bytes and the baseline: the file held the outgoing session's sound while the
    // baseline came from the restored one. Draining first is also what every other
    // message-thread entry point does, so the save writes the session the rest of the
    // program is on.
    if (onAboutToSave) onAboutToSave();

    // Test seam (empty in production: one null check, on a non-audio path). It fires at
    // the one instant a mutation would have to land to split the bytes from the baseline,
    // which is what makes State test 52 deterministic instead of a race to lose.
    if (beforeStateCapture) beforeStateCapture();

    const auto savedSound = apvts.copyState();
    const auto xml = savedSound.createXml();
    if (xml == nullptr || ! file.replaceWithText (xml->toString())) return false;

    refresh();
    current = name;
    // Saving SELECTS what was just written, by file. This is the case the ID split
    // exists for: saving a user preset under a factory preset's name now moves the
    // tick to the USER row instead of leaving it on the factory one (#4).
    sel = { Selection::Kind::userFile, {}, file };
    // The baseline is the sound THIS FILE RESTORES, computed from the tree that was just
    // written (§17) -- never a second read of the live parameters. So "clean" means exactly
    // "the live sound is the sound this file holds", and a mutation that lands during the
    // save leaves the preset DIRTY, correctly and immediately, because the live sound has
    // moved away from what the file holds.
    //
    // It deliberately does NOT mean "reloading this preset would change nothing". A reload
    // drives every value through the parameter's own store/report pass once more, and for
    // the four gridless log-mapped frequency ranges that pass is not idempotent in float,
    // so the reloaded value can differ in its last bits -- State test 55 measures the two
    // signature flavours differing at 2 points in 3000, which is precisely why both
    // flavours exist. The marker is unmoved by it: each side is compared against the
    // baseline built for its own path.
    sigAtLoad = soundSignatureForSavedTree (apvts, savedSound);
    if (onMetaChanged) onMetaChanged();
    if (onSaved) onSaved(); // re-baseline the processor's undo snapshot onto the saved preset
    return true;
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
