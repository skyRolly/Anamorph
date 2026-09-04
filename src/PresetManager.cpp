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
juce::ValueTree PresetManager::parseSoundFile (const juce::File& f) const
{
    if (auto xml = juce::parseXML (f))
        if (auto t = juce::ValueTree::fromXml (*xml); t.hasType (apvts.state.getType()))
            return t;
    return {};
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
    for (auto* p : apvts.processor.getParameters())
        if (auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            if (! pid::isPresetExcluded (wid->paramID))
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                    rp->setValueNotifyingHost (normalisedFromSavedTree (*rp, state, wid->paramID));
    resetSolo();
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

void PresetManager::load (int index)
{
    if (index < 0 || index >= list.size()) return;
    const auto& e = list.getReference (index);

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
        if (factory == nullptr) return;
    }
    else
    {
        // Unparsable OR foreign-rooted -> the same clean no-op, resolved here so
        // it lands before onAboutToLoad() like every other failure (ER-STATE-24).
        userSound = parseSoundFile (e.file);
        if (! userSound.isValid()) return;
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
        applyDefaults();
        // Resolved through the ID, not the list position: the entry already carries the
        // identity the selection is about to adopt, so the two can never disagree (#4).
        for (const auto& o : factory->set)
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (o.id)))
                rp->setValueNotifyingHost (rp->convertTo0to1 (o.value));
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
}

bool PresetManager::loadFile (const juce::File& f)
{
    // Unparsable OR foreign-rooted -> false, and nothing is touched: the chooser
    // can point at any file on the machine, so this is the path a user is most
    // likely to hand another plug-in's preset to (ER-STATE-24).
    auto sound = parseSoundFile (f);
    if (! sound.isValid()) return false;
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
    return true;
}

void PresetManager::step (int delta)
{
    if (list.isEmpty()) return;
    // The step is RELATIVE to the current row, so the current row must be the authoritative
    // one: a pending host restore that moves the selection is adopted before it is read (D-2
    // round 10, §18). Without this, "next" from a session that had just been replaced landed
    // on the row after the OLD selection -- the same stale-derivation shape as the A/B toggle.
    if (adoptPending) adoptPending();
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
