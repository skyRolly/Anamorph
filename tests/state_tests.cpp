// ============================================================================
//  Anamorph state-compatibility self-tests (v0.8.13 harness)
//
//  Headless regression net for the COMPATIBILITY policy family
//  (SESSION_COMPATIBILITY_POLICY / PARAMETER_COMPATIBILITY_POLICY): it
//  exercises the REAL AnamorphAudioProcessor (this target compiles the plugin
//  sources), so every check runs the exact production serialization code paths.
//  Since 2026-08-21 it also CONSTRUCTS AND DESTROYS the real editor -- but never
//  SHOWS it: no peer, no message loop, no interaction. A defect that needs a
//  component on screen still has no surface here; a defect in the editor's
//  construction or teardown now does.
//
//    1. Serialized schema shape: AnamorphRoot / ANAMORPH / ANAMORPH_INTERNAL /
//       AB fields exist exactly as SERIALIZATION_REGISTRY.md records them.
//    2. Parameter registry snapshot: IDs, names, order, ranges, defaults,
//       automation flags vs the checked-in fixture (parameter_registry.snapshot).
//    3. Full state round-trip: getState -> setState -> getState is byte-exact,
//       every parameter raw value bit-exact, InternalState / A/B / preset meta
//       reproduced, undo history cleared.
//    4. Legacy migration paths (fixtures, per SERIALIZATION_REGISTRY.md):
//       v0.2 bare APVTS, pre-0.6.4 A/B slots, pre-0.8.4 view params.
//    5. Corrupt / foreign state robustness (garbage blob, out-of-range A/B
//       active, unknown root, unknown extra fields, corrupt slot XML).
//    6. Preset save -> reload round-trip (user preset file + exclusion rules).
//    7. A/B + view-param preservation across slot apply and session restore.
//    8. Factory/user preset identity when a user preset shares a factory name.
//    9. Factory-id integrity: present, unique, and every one resolving.
//   10. The preset indicator identity across a session reload, incl. every
//       fallback -- and bit-identical parameters on every one of those paths.
//   11. Wrapper audio path: the real processBlock over a denormal-provoking
//       noise -> silence matrix, with NO test-side FTZ arming -- regresses
//       processBlock's own ScopedNoDenormals and gives the sanitizer/valgrind
//       runs of this suite a wrapper audio path to instrument.
//
//  Fixture workflow: an INTENTIONAL parameter/schema change (which requires an
//  ADR + registry update per the compatibility policies) is recorded by
//  regenerating the snapshot:  AnamorphStateTests --write-snapshot
//  An unintentional change fails the comparison — that is the point.
//
//  Exits non-zero on any failure so the build gate can fail the run.
// ============================================================================

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "gui/PhysicalMouseButtons.h"   // TooltipSource, and the editor lifetime test at the end

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <cstring>
#include <thread>
#include <mutex>
#include <random>
#include <vector>
#include <functional>
#include <limits>

namespace
{
    int failures = 0;
    int checks   = 0;

    void check (bool cond, const char* what)
    {
        ++checks;
        if (! cond) { ++failures; std::printf ("  [FAIL] %s\n", what); }
    }

    void checkStr (const juce::String& got, const juce::String& expected, const char* what)
    {
        ++checks;
        if (got != expected)
        {
            ++failures;
            std::printf ("  [FAIL] %s: got \"%s\", expected \"%s\"\n",
                         what, got.toRawUTF8(), expected.toRawUTF8());
        }
    }

    void checkNear (double got, double expected, double tol, const char* what)
    {
        ++checks;
        if (! (std::abs (got - expected) <= tol))
        {
            ++failures;
            std::printf ("  [FAIL] %s: got %.9g, expected %.9g (tol %.3g)\n", what, got, expected, tol);
        }
    }

    // ------------------------------------------------------------------------
    // Access to the protected AudioProcessor blob codec, so fixtures can be
    // stored as readable XML and wrapped with JUCE's OWN binary framing (the
    // same copyXmlToBinary framing every released Anamorph used to write its
    // host chunk). Static-only helper — never instantiated.
    // ------------------------------------------------------------------------
    struct BlobCodec : AnamorphAudioProcessor
    {
        static juce::MemoryBlock wrap (const juce::XmlElement& xml)
        {
            juce::MemoryBlock mb;
            copyXmlToBinary (xml, mb);
            return mb;
        }
        static std::unique_ptr<juce::XmlElement> unwrap (const juce::MemoryBlock& mb)
        {
            return getXmlFromBinary (mb.getData(), (int) mb.getSize());
        }
    };

    juce::File fixtureDir()
    {
        return juce::File (ANAMORPH_FIXTURE_DIR);
    }

    // Load a fixture stored as readable XML and feed it through the real host
    // restore path (setStateInformation on a JUCE-framed binary blob).
    bool applyXmlFixture (AnamorphAudioProcessor& p, const juce::String& fixtureName)
    {
        auto file = fixtureDir().getChildFile (fixtureName);
        auto xml  = juce::parseXML (file);
        if (xml == nullptr)
        {
            std::printf ("  [FAIL] fixture missing/unparsable: %s\n",
                         file.getFullPathName().toRawUTF8());
            ++checks; ++failures;
            return false;
        }
        const auto blob = BlobCodec::wrap (*xml);
        p.setStateInformation (blob.getData(), (int) blob.getSize());
        return true;
    }

    juce::ValueTree stateTreeOf (AnamorphAudioProcessor& p)
    {
        juce::MemoryBlock mb;
        p.getStateInformation (mb);
        if (auto xml = BlobCodec::unwrap (mb))
            return juce::ValueTree::fromXml (*xml);
        return {};
    }

    float rawOf (AnamorphAudioProcessor& p, const char* id)
    {
        auto* param = p.getAPVTS().getParameter (id);
        return param != nullptr ? param->getValue() : -1.0f;
    }

    void setRaw (AnamorphAudioProcessor& p, const char* id, float raw)
    {
        if (auto* param = p.getAPVTS().getParameter (id))
            param->setValueNotifyingHost (raw);
    }

    // setRaw's "raw" is the NORMALISED 0..1 value (getValue/setValueNotifyingHost).
    // This is the denormalised sibling: pass the value in the parameter's own units
    // (28 ms, 400 Hz, choice index 2) and let the range do the conversion.
    void setPlain (AnamorphAudioProcessor& p, const char* id, float plain)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.getAPVTS().getParameter (id)))
            param->setValueNotifyingHost (param->convertTo0to1 (plain));
    }
    float plainOf (AnamorphAudioProcessor& p, const char* id)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (p.getAPVTS().getParameter (id)))
            return param->convertFrom0to1 (param->getValue());
        return -1.0f;
    }

    // Deterministic per-index raw value that lands BETWEEN discrete steps for
    // most discrete parameters, exercising the exact-raw ("raw" attribute)
    // round-trip rather than only snapped values.
    float variedRaw (int index)
    {
        const double v = 0.137 + 0.618033988749895 * (double) (index + 1);
        return (float) (v - std::floor (v));
    }

    std::vector<juce::RangedAudioParameter*> rangedParams (AnamorphAudioProcessor& p)
    {
        std::vector<juce::RangedAudioParameter*> out;
        for (auto* param : p.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
                out.push_back (rp);
        return out;
    }
}

// ---------------------------------------------------------------------------
//  Parameter registry snapshot (PARAMETER_COMPATIBILITY_POLICY rules 1/3/4/5)
// ---------------------------------------------------------------------------
namespace registry
{
    const char* const kSnapshotFile = "parameter_registry.snapshot";

    // Keys whose values are floating-point products of NormalisableRange
    // mappings (std::pow/log differ by ULPs across platforms/stdlibs), so the
    // single checked-in snapshot is compared numerically with a tolerance on
    // all three CI platforms; every other key is compared exactly.
    bool isNumericKey (const juce::String& key)
    {
        return key == "defaultRaw" || key == "interval"
            || key == "den0" || key == "den25" || key == "den50"
            || key == "den75" || key == "den100";
    }

    juce::String fmt (double v)
    {
        char buf[64];
        std::snprintf (buf, sizeof (buf), "%.6f", v);
        return juce::String (buf);
    }

    juce::String build (AnamorphAudioProcessor& p)
    {
        juce::String s;
        auto params = rangedParams (p);

        s << "# Anamorph parameter registry snapshot -- compatibility fixture.\n"
          << "# Regenerate ONLY for an intentional parameter change (ADR + PARAMETER_REGISTRY.md\n"
          << "# update required):  AnamorphStateTests --write-snapshot\n"
          << "paramCount=" << (int) params.size() << "\n";

        if (auto* bypass = dynamic_cast<juce::AudioProcessorParameterWithID*> (p.getBypassParameter()))
            s << "bypassParamId=" << bypass->paramID << "\n";

        int index = 0;
        for (auto* rp : params)
        {
            const auto& range = rp->getNormalisableRange();
            s << "param=" << rp->paramID << "\n"
              << "index=" << index++ << "\n"
              << "name=" << rp->getName (64) << "\n"
              << "label=" << rp->getLabel() << "\n"
              << "versionHint=" << rp->getVersionHint() << "\n"
              << "automatable=" << (rp->isAutomatable() ? 1 : 0) << "\n"
              << "discrete=" << (rp->isDiscrete() ? 1 : 0) << "\n"
              << "boolean=" << (rp->isBoolean() ? 1 : 0) << "\n"
              << "meta=" << (rp->isMetaParameter() ? 1 : 0) << "\n"
              << "numSteps=" << rp->getNumSteps() << "\n"
              << "defaultRaw=" << fmt (rp->getDefaultValue()) << "\n"
              << "interval=" << fmt (range.interval) << "\n";

            // Range identity probed FUNCTIONALLY (log/skew mappings have no
            // stable closed-form fields): denormalised value at 5 raw points.
            const double probes[] = { 0.0, 0.25, 0.5, 0.75, 1.0 };
            const char* names[]   = { "den0", "den25", "den50", "den75", "den100" };
            for (int i = 0; i < 5; ++i)
                s << names[i] << "=" << fmt (rp->convertFrom0to1 ((float) probes[i])) << "\n";

            // Discrete parameters: the per-step text ordering IS the recall
            // contract (choice reorder = broken sessions).
            const int steps = rp->getNumSteps();
            if (rp->isDiscrete() && steps >= 2 && steps <= 64)
                for (int i = 0; i < steps; ++i)
                {
                    const float norm = (float) i / (float) (steps - 1);
                    s << "stepText" << i << "=" << rp->getText (norm, 64) << "\n";
                }
        }
        return s;
    }

    void compare (const juce::String& generated, const juce::String& fixture)
    {
        auto genLines = juce::StringArray::fromLines (generated);
        auto fixLines = juce::StringArray::fromLines (fixture);
        genLines.removeEmptyStrings();
        fixLines.removeEmptyStrings();

        check (genLines.size() == fixLines.size(), "registry snapshot line count matches fixture");
        const int n = juce::jmin (genLines.size(), fixLines.size());
        int reported = 0;

        for (int i = 0; i < n; ++i)
        {
            const auto& g = genLines[i];
            const auto& f = fixLines[i];
            if (g == f) { ++checks; continue; }
            if (g.startsWithChar ('#') && f.startsWithChar ('#')) { ++checks; continue; }

            const auto gKey = g.upToFirstOccurrenceOf ("=", false, false);
            const auto fKey = f.upToFirstOccurrenceOf ("=", false, false);
            bool ok = false;
            if (gKey == fKey && isNumericKey (gKey))
            {
                const double gv = g.fromFirstOccurrenceOf ("=", false, false).getDoubleValue();
                const double fv = f.fromFirstOccurrenceOf ("=", false, false).getDoubleValue();
                const double tol = 1.0e-4 * juce::jmax (1.0, std::abs (gv), std::abs (fv));
                ok = std::abs (gv - fv) <= tol;
            }
            ++checks;
            if (! ok)
            {
                ++failures;
                if (++reported <= 12)
                    std::printf ("  [FAIL] registry line %d differs:\n    built:   %s\n    fixture: %s\n",
                                 i + 1, g.toRawUTF8(), f.toRawUTF8());
            }
        }
        if (reported > 12)
            std::printf ("  (%d further differing lines suppressed)\n", reported - 12);
        if (failures > 0 && reported > 0)
            std::printf ("  NOTE: if this parameter change is INTENTIONAL, it needs an ADR + a\n"
                         "  PARAMETER_REGISTRY.md update, then: AnamorphStateTests --write-snapshot\n");
    }
}

// ---------------------------------------------------------------------------
static void testSerializedSchemaShape()
{
    std::printf ("State test 1: serialized schema shape (SERIALIZATION_REGISTRY.md)\n");
    AnamorphAudioProcessor p;
    p.prepareToPlay (48000.0, 512);

    auto root = stateTreeOf (p);
    check (root.hasType ("AnamorphRoot"), "root tree is AnamorphRoot");
    check (root.hasProperty ("presetName"), "root carries presetName");
    check (root.hasProperty ("presetBaseline"), "root carries presetBaseline");

    auto apvtsTree = root.getChildWithName ("ANAMORPH");
    check (apvtsTree.isValid(), "ANAMORPH (APVTS) child present");

    const int paramCount = (int) rangedParams (p).size();
    int paramNodes = 0;
    bool allHaveIdValueRaw = true;
    for (auto node : apvtsTree)
        if (node.hasType ("PARAM"))
        {
            ++paramNodes;
            allHaveIdValueRaw = allHaveIdValueRaw
                && node.hasProperty ("id") && node.hasProperty ("value") && node.hasProperty ("raw");
        }
    check (paramNodes == paramCount, "one PARAM node per registered parameter");
    check (allHaveIdValueRaw, "every PARAM node carries id + value + raw");

    auto internalTree = root.getChildWithName ("ANAMORPH_INTERNAL");
    check (internalTree.isValid(), "ANAMORPH_INTERNAL child present");
    for (auto* field : { "int_oversample", "int_uiScale", "int_scopePersist",
                         "int_metersOn", "int_tooltipsOn", "int_uiAnimations" })
        check (internalTree.hasProperty (field), field);

    auto ab = root.getChildWithName ("AB");
    check (ab.isValid(), "AB child present");
    for (auto* field : { "presetSource", "presetFactoryId", "presetUserFile" })
        check (root.hasProperty (field), field);

    for (auto* field : { "active", "slotAParams", "slotAName", "slotABase",
                         "slotASource", "slotAFactoryId", "slotAUserFile",
                         "slotBParams", "slotBName", "slotBBase",
                         "slotBSource", "slotBFactoryId", "slotBUserFile" })
        check (ab.hasProperty (field), field);
}

// ---------------------------------------------------------------------------
static void testParameterRegistrySnapshot (bool writeSnapshot)
{
    std::printf ("State test 2: parameter registry snapshot\n");
    AnamorphAudioProcessor p;
    const auto generated = registry::build (p);
    auto file = fixtureDir().getChildFile (registry::kSnapshotFile);

    if (writeSnapshot)
    {
        fixtureDir().createDirectory();
        const bool ok = file.replaceWithText (generated, false, false, "\n");
        check (ok, "snapshot written");
        std::printf ("  wrote %s\n", file.getFullPathName().toRawUTF8());
        return;
    }

    if (! file.existsAsFile())
    {
        ++checks; ++failures;
        std::printf ("  [FAIL] snapshot fixture missing: %s\n  (generate once with --write-snapshot)\n",
                     file.getFullPathName().toRawUTF8());
        return;
    }
    registry::compare (generated, file.loadFileAsString());
}

// ---------------------------------------------------------------------------
static void testStateRoundTripExact()
{
    std::printf ("State test 3: full state round-trip (raw-exact, byte-stable)\n");
    AnamorphAudioProcessor a;
    a.prepareToPlay (48000.0, 512);

    // Drive every parameter to a varied raw value (mid-step for discretes, so
    // the "raw" attribute path is exercised, not just snapped values)...
    {
        int i = 0;
        for (auto* rp : rangedParams (a))
            rp->setValueNotifyingHost (variedRaw (i++));
    }
    // ...set every InternalState field off its default...
    a.getInternal().oversampleValue().setValue (3);
    a.getInternal().uiScaleValue().setValue (5);
    a.getInternal().scopePersistValue().setValue (0.25);
    a.getInternal().metersValue().setValue (true);
    a.getInternal().tooltipsValue().setValue (true);
    a.getInternal().animationsValue().setValue (false);
    // ...make the A/B slots genuinely DIFFER in the saved blob: copy the varied
    // state into both slots, switch to B, edit width there, then switch back —
    // the switch-back stores the edited state into slot B, so slot A carries the
    // varied width, slot B the 0.9 edit, and the active slot lands on A.
    a.abCopyToOther();
    a.abSwitchTo (1);
    setRaw (a, "width", 0.9f);
    a.abSwitchTo (0);
    // Selection() spelled out: this fixture carries no identity, and setMeta makes callers say so.
    a.getPresets().setMeta ("RoundTrip Fixture", "sig-baseline-token",
                            anamorph::PresetManager::Selection());

    juce::MemoryBlock blobA;
    a.getStateInformation (blobA);

    AnamorphAudioProcessor b;
    b.prepareToPlay (48000.0, 512);
    // Give b REAL undo history before the restore, so the cleared-on-restore
    // assertion below cannot pass vacuously on a fresh instance.
    {
        auto* drive = b.getAPVTS().getParameter ("drive");
        drive->beginChangeGesture();
        drive->setValueNotifyingHost (0.42f);
        drive->endChangeGesture();
        b.pollUndoCoalesce();
        check (b.canUndo(), "gesture created undo history (restore-clears precondition)");
    }
    b.setStateInformation (blobA.getData(), (int) blobA.getSize());

    // Per-parameter raw values restore bit-exactly (the "raw" attribute).
    {
        auto pa = rangedParams (a);
        auto pb = rangedParams (b);
        check (pa.size() == pb.size(), "parameter count identical");
        bool allExact = true;
        for (size_t i = 0; i < pa.size() && i < pb.size(); ++i)
            if (! juce::exactlyEqual (pa[i]->getValue(), pb[i]->getValue()))
            {
                allExact = false;
                std::printf ("  [detail] %s: %.9g != %.9g\n", pa[i]->paramID.toRawUTF8(),
                             (double) pa[i]->getValue(), (double) pb[i]->getValue());
            }
        check (allExact, "every parameter raw value restores bit-exactly");
    }

    // InternalState restores field-for-field.
    {
        auto ia = a.getInternal().copyState();
        auto ib = b.getInternal().copyState();
        for (int i = 0; i < ia.getNumProperties(); ++i)
        {
            const auto name = ia.getPropertyName (i);
            check (ia[name] == ib[name], ("internal field restores: " + name.toString()).toRawUTF8());
        }
    }

    // Preset meta + A/B active + undo policy.
    checkStr (b.getPresets().currentName(), "RoundTrip Fixture", "preset name restores");
    checkStr (b.getPresets().baseline(), "sig-baseline-token", "preset baseline restores");
    check (b.abActiveSlot() == a.abActiveSlot(), "A/B active slot restores");
    check (! b.canUndo() && ! b.canRedo(), "undo history cleared on session restore");

    // The two slot payloads really differ (guards the setup above staying honest).
    {
        auto ab = stateTreeOf (b).getChildWithName ("AB");
        check (ab["slotAParams"].toString() != ab["slotBParams"].toString(),
               "restored A/B slots carry the two DIFFERENT payloads");
    }

    // A second save of the restored state is byte-identical: the schema is a
    // fixed point of save -> load -> save (catches lossy or reordered fields).
    juce::MemoryBlock blobB;
    b.getStateInformation (blobB);
    check (blobA == blobB, "save -> load -> save is byte-identical");
}

// ---------------------------------------------------------------------------
static void testLegacyV02BareApvts()
{
    std::printf ("State test 4: legacy v0.2 bare-APVTS session loads\n");
    AnamorphAudioProcessor p;
    p.prepareToPlay (48000.0, 512);
    if (! applyXmlFixture (p, "legacy_v0_2_bare_apvts.xml"))
        return;

    // Values present in the fixture apply via the value->raw fallback
    // (pre-"raw" sessions carry only the denormalised value).
    auto expectFromValue = [&p] (const char* id, float denorm, const char* what)
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p.getAPVTS().getParameter (id));
        if (rp == nullptr) { check (false, id); return; }
        checkNear ((double) rp->getValue(), (double) rp->convertTo0to1 (denorm), 1.0e-6, what);
    };
    expectFromValue ("drive", 6.0f, "drive restores from legacy value");
    expectFromValue ("algorithm", 2.0f, "algorithm (discrete) restores from legacy value");
    expectFromValue ("width", 1.5f, "width restores from legacy value");
    expectFromValue ("mix", 0.8f, "mix restores from legacy value");
    expectFromValue ("haasDelay", 20.0f, "haasDelay restores from legacy value");
    expectFromValue ("outputGain", -3.0f, "outputGain restores from legacy value");

    // Parameters absent from the v0.2 tree keep their defaults.
    auto* chorusRate = p.getAPVTS().getParameter ("chorusRate");
    check (juce::exactlyEqual (chorusRate->getValue(), chorusRate->getDefaultValue()),
           "param absent from legacy session stays at default");

    // ...and on a REUSED live instance the same rule must hold by RESET, not by
    // luck: the fresh-instance check above is vacuously green, because a
    // just-constructed processor is already at defaults. SESSION_COMPATIBILITY_POLICY
    // rule 2 and SERIALIZATION_REGISTRY ("Default: per-parameter defaults") record the
    // semantics this asserts. (Round 1 read the restore as SKIPPING absent nodes and
    // added a default branch to reassertParameters for it; round 2 measured
    // apvts.replaceState on its own -- --latency-restore-probe step 0b -- and found it
    // already resets them, so the branch is a backstop and this check covers both.
    // The assertion is the contract either way, which is why it stands unchanged.)
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p.getAPVTS().getParameter ("chorusRate"));
        rp->setValueNotifyingHost (1.0f);            // the "previous project" leaves a non-default value
        if (applyXmlFixture (p, "legacy_v0_2_bare_apvts.xml"))
        {
            check (juce::exactlyEqual (rp->getValue(), rp->getDefaultValue()),
                   "param absent from legacy session RESETS to default on a reused live instance");
            checkNear ((double) rawOf (p, "chorusRate"),
                       (double) rp->getDefaultValue(), 1.0e-6,
                       "...and the DSP atomic follows the reset");
        }
    }

    // The bare path predates InternalState AND its legacy APVTS params: the
    // host-hidden settings stay at their defaults.
    // ...and on a REUSED live instance that must hold by RESET, not by luck. This
    // assertion used to be checked on an InternalState nobody had touched, so it
    // passed because the value had never left its default -- the same vacuity
    // ER-TST-01 found in the DSP matrices (round 1). Dirty it first, exactly as
    // the parameter half above dirties chorusRate (ER-TST-05 / ER-STATE-08).
    p.getInternal().oversampleValue().setValue (3);   // "previous project" = 4x
    p.getInternal().uiScaleValue().setValue (5);      // ...and a non-default UI scale
    if (applyXmlFixture (p, "legacy_v0_2_bare_apvts.xml"))
    {
        check ((int) p.getInternal().copyState()["int_oversample"] == 1,
               "InternalState RESETS to default for a v0.2 session on a reused instance");
        check ((int) p.getInternal().copyState()["int_uiScale"] == 3,
               "...and every host-hidden setting resets, not just the one that is read back");
    }
    checkStr (p.getPresets().currentName(), "Default", "preset name falls back to Default");
    check (! p.getPresets().isDirty(), "restored v0.2 state adopts a clean baseline");
}

// ---------------------------------------------------------------------------
static void testLegacyPre064AbSlots()
{
    std::printf ("State test 5: pre-0.6.4 A/B slots + legacy-APVTS settings migrate\n");
    AnamorphAudioProcessor p;
    p.prepareToPlay (48000.0, 512);
    if (! applyXmlFixture (p, "legacy_pre_0_6_4_ab_slots.xml"))
        return;

    // Live params come from the main ANAMORPH child.
    checkNear ((double) rawOf (p, "width"),
               (double) dynamic_cast<juce::RangedAudioParameter*> (p.getAPVTS().getParameter ("width"))
                            ->convertTo0to1 (1.2f),
               1.0e-6, "live width restores");
    check (p.abActiveSlot() == 1, "A/B active slot restores from legacy session");

    // Settings migrate from the legacy APVTS params (no ANAMORPH_INTERNAL child;
    // choice indices are 0-based in the legacy tree, 1-based in InternalState).
    auto internalTree = p.getInternal().copyState();
    check ((int)  internalTree["int_oversample"] == 2,  "oversample migrates (idx 1 -> combo 2)");
    check ((int)  internalTree["int_uiScale"]    == 5,  "uiScale migrates (idx 4 -> combo 5)");
    checkNear ((double) internalTree["int_scopePersist"], 0.75, 1.0e-9, "scopePersist migrates");
    check ((bool) internalTree["int_metersOn"]   == true,  "metersOn migrates");
    check ((bool) internalTree["int_tooltipsOn"] == true,  "tooltipsOn migrates");
    check ((bool) internalTree["int_uiAnimations"] == false, "uiAnimations migrates");

    // Re-saving MODERNIZES the session: legacy slotA/slotB params reappear
    // under the modern keys, and ANAMORPH_INTERNAL is now written.
    auto root = stateTreeOf (p);
    check (root.getChildWithName ("ANAMORPH_INTERNAL").isValid(),
           "re-save writes ANAMORPH_INTERNAL for a migrated session");
    auto ab = root.getChildWithName ("AB");
    check (ab.isValid() && (int) ab["active"] == 1, "re-save keeps active slot");
    auto slotA = juce::ValueTree::fromXml (ab["slotAParams"].toString());
    auto slotB = juce::ValueTree::fromXml (ab["slotBParams"].toString());
    check (slotA.isValid() && slotB.isValid(), "legacy slots re-save under modern keys");
    checkNear ((double) slotA.getChildWithProperty ("id", "width")["value"], 1.8, 1.0e-6,
               "slot A params survive the legacy read");
    checkNear ((double) slotB.getChildWithProperty ("id", "width")["value"], 0.6, 1.0e-6,
               "slot B params survive the legacy read");
    // Legacy slots carry no name/baseline of their own, so the read must produce the
    // DEFAULT for them -- the same "absence means default" rule the identity fields follow.
    // (Before 0.9.2 this kept whatever the slot happened to hold, which on a fresh instance
    // was the construction snapshot "Default" and on a re-restore was the PREVIOUS session's
    // name -- see the repeated-restore check below.)
    checkStr (ab["slotAName"].toString(), "", "a legacy slot carries no name of its own");
    checkStr (ab["slotABase"].toString(), "", "...and no baseline of its own");

    // Repeated restore into ONE live instance -- the case the rule exists for. A host may
    // call setStateInformation on the same processor any number of times; a legacy AB node
    // carries params only, so the slot's metadata must come back as the default rather than
    // as whatever the previous session left attached to abSlot[].
    {
        AnamorphAudioProcessor q;
        q.prepareToPlay (48000.0, 512);
        q.getPresets().load (1);      // a factory preset that is not "Default"
        q.abSwitchTo (1);             // snapshots the loaded state (name + identity) into slot A
        auto abBefore = stateTreeOf (q).getChildWithName ("AB");
        check (abBefore["slotAName"].toString().isNotEmpty(),
               "the modern session gives slot A a name to inherit");
        if (applyXmlFixture (q, "legacy_pre_0_6_4_ab_slots.xml"))
        {
            auto abAfter = stateTreeOf (q).getChildWithName ("AB");
            checkStr (abAfter["slotAName"].toString(), "",
                      "a legacy restore does not leave the previous session's slot name attached");
            checkStr (abAfter["slotASource"].toString(), "",
                      "...and clears its identity the same way");
        }
    }

    // Behavioural: switching to slot A applies the legacy-read params.
    p.abSwitchTo (0);
    checkNear ((double) rawOf (p, "width"),
               (double) dynamic_cast<juce::RangedAudioParameter*> (p.getAPVTS().getParameter ("width"))
                            ->convertTo0to1 (1.8f),
               1.0e-6, "switching to legacy slot A applies its width");
    // ...and the slot reads as "no preset", NOT as "a modified preset". The slot carries no
    // baseline, and an absent baseline is not evidence of an edit -- the same rule state test 4
    // pins for a v0.2 root ("restored v0.2 state adopts a clean baseline"). A literal empty
    // baseline would compare unequal to every possible signature, so the top bar would render a
    // bare " *": a modified-marker against a preset that does not exist.
    check (! p.getPresets().isDirty(),
           "a legacy slot switched into reads as clean, not as permanently modified");
    checkStr (p.getPresets().currentName(), "",
              "...and shows no preset name rather than borrowing the other slot's");
    // The empty name is a MODEL fact and has to stay one. The top bar substitutes a "No Preset"
    // placeholder for DISPLAY (refreshPresetDisplay), and that placeholder must never reach the
    // serialized field or the Save Preset pre-fill -- both read currentName(). Pinning the saved
    // property is what makes moving the substitution into PresetManager fail loudly.
    checkStr (stateTreeOf (p)["presetName"].toString(), "",
              "the top bar's placeholder never reaches the serialized preset name");
}

// ---------------------------------------------------------------------------
static void testLegacyPre084InternalMigration()
{
    std::printf ("State test 6: pre-0.8.4 view-param session migrates to InternalState\n");
    AnamorphAudioProcessor p;
    p.prepareToPlay (48000.0, 512);
    if (! applyXmlFixture (p, "legacy_pre_0_8_4_view_params.xml"))
        return;

    auto internalTree = p.getInternal().copyState();
    check ((int)  internalTree["int_oversample"] == 3,  "oversample migrates (idx 2 -> combo 3)");
    check ((int)  internalTree["int_uiScale"]    == 2,  "uiScale migrates (idx 1 -> combo 2)");
    checkNear ((double) internalTree["int_scopePersist"], 0.25, 1.0e-9, "scopePersist migrates");
    check ((bool) internalTree["int_metersOn"]   == false, "metersOn migrates");
    check ((bool) internalTree["int_tooltipsOn"] == true,  "tooltipsOn migrates");
    check ((bool) internalTree["int_uiAnimations"] == true, "uiAnimations migrates");

    checkStr (p.getPresets().currentName(), "My Vocal", "preset name restores");
    check (p.abActiveSlot() == 0, "active slot restores");

    // Modern AB slot keys (params + name + baseline) read as-is.
    auto root = stateTreeOf (p);
    auto ab = root.getChildWithName ("AB");
    checkStr (ab["slotBName"].toString(), "Slot B Preset", "modern slot name survives");
}

// ---------------------------------------------------------------------------
static void testCorruptAndForeignState()
{
    std::printf ("State test 7: corrupt / foreign state robustness\n");
    AnamorphAudioProcessor p;
    p.prepareToPlay (48000.0, 512);
    setRaw (p, "width", 0.9f);
    const float widthBefore = rawOf (p, "width");

    // Garbage bytes: rejected without touching state.
    {
        const char garbage[] = "definitely not a JUCE state chunk";
        p.setStateInformation (garbage, (int) sizeof (garbage));
        check (juce::exactlyEqual (rawOf (p, "width"), widthBefore), "garbage blob leaves state untouched");
    }

    // Truncated valid blob: rejected without touching state.
    {
        juce::MemoryBlock full;
        p.getStateInformation (full);
        p.setStateInformation (full.getData(), (int) juce::jmin ((size_t) 6, full.getSize()));
        check (juce::exactlyEqual (rawOf (p, "width"), widthBefore), "truncated blob leaves state untouched");
    }

    // Foreign root tag: neither restore branch applies; no crash.
    //
    // ...and nothing may be ADOPTED either. Input we do not recognise is not a restore: the sound
    // in force is still the user's, so relabelling it, dropping its identity or re-baselining its
    // dirty-star would describe a session that never loaded. A host may hand a live instance any
    // chunk it likes, so this is checked with real metadata present -- a loaded preset, then an
    // edit so the dirty-star is on -- which is exactly what such a call could leak away.
    {
        auto& pm = p.getPresets();
        pm.load (1);                                  // a named factory preset, with an identity
        setRaw (p, "width", 0.33f);                   // ...edited, so isDirty() is true
        const auto nameBefore  = pm.currentName();
        const int  rowBefore   = pm.currentIndex();
        const auto baseBefore  = pm.baseline();
        const bool dirtyBefore = pm.isDirty();
        const float wBefore    = rawOf (p, "width");
        check (nameBefore.isNotEmpty() && rowBefore >= 0 && dirtyBefore,
               "there is real preset metadata for an unknown chunk to disturb");

        juce::XmlElement foreign ("SOME_FUTURE_ROOT");
        foreign.setAttribute ("v", 99);
        const auto blob = BlobCodec::wrap (foreign);
        p.setStateInformation (blob.getData(), (int) blob.getSize());

        check (juce::exactlyEqual (rawOf (p, "width"), wBefore), "unknown root tag leaves parameters untouched");
        checkStr (pm.currentName(), nameBefore, "...and the preset NAME untouched");
        check (pm.currentIndex() == rowBefore,      "...and the identity/checkmark untouched");
        checkStr (pm.baseline(), baseBefore,        "...and the dirty baseline untouched");
        check (pm.isDirty() == dirtyBefore,         "...so the dirty-star still reflects the live sound");

        // Repeat it: a host may do this any number of times on one live instance.
        p.setStateInformation (blob.getData(), (int) blob.getSize());
        checkStr (pm.currentName(), nameBefore, "a repeated unknown chunk still leaks nothing");
        check (pm.currentIndex() == rowBefore, "...identity still intact after the second attempt");

        setRaw (p, "width", widthBefore);              // restore this test's working state
        pm.setMeta ("Default", {}, anamorph::PresetManager::Selection());
    }

    // Out-of-range A/B active (hand-edited / forward-version blob) clamps —
    // the end-to-end guard over anamorph::clampAbSlotIndex.
    //
    // The root must carry a REAL sound child. It used to be built from an `AB`
    // node alone, which since ER-STATE-15 is not a restore at all (a root with no
    // `ANAMORPH` child restores no sound, so nothing below it is adopted) — so the
    // clamp was no longer reached and this guard would have passed vacuously or
    // failed for the wrong reason. Building the root from a genuine save keeps the
    // clamp on the path a real out-of-range blob takes.
    auto restoreWithActive = [&p] (const char* active)
    {
        juce::MemoryBlock live;
        p.getStateInformation (live);
        auto xml = BlobCodec::unwrap (live);
        auto root = juce::ValueTree::fromXml (*xml);
        auto ab = root.getChildWithName ("AB");
        ab.setProperty ("active", juce::String (active), nullptr);
        const auto blob = BlobCodec::wrap (*root.createXml());
        p.setStateInformation (blob.getData(), (int) blob.getSize());
        return p.abActiveSlot();
    };
    check (restoreWithActive ("99") == 1, "active=99 clamps to slot B");
    check (restoreWithActive ("-7") == 0, "active=-7 clamps to slot A");

    // Unknown extra child + unknown attributes (forward compatibility):
    // ignored, known fields still apply.
    {
        juce::XmlElement root ("AnamorphRoot");
        root.setAttribute ("presetName", "Forward Session");
        root.setAttribute ("someFutureAttribute", "x");
        auto* params = root.createNewChildElement ("ANAMORPH");
        auto* param  = params->createNewChildElement ("PARAM");
        param->setAttribute ("id", "drive");
        param->setAttribute ("value", 12.0);
        root.createNewChildElement ("SOME_FUTURE_CHILD")->setAttribute ("data", 1);
        const auto blob = BlobCodec::wrap (root);
        p.setStateInformation (blob.getData(), (int) blob.getSize());
        checkNear ((double) rawOf (p, "drive"), 0.5, 1.0e-6,
                   "known fields apply despite unknown future fields");
        checkStr (p.getPresets().currentName(), "Forward Session",
                  "preset name applies despite unknown future fields");
    }

    // Corrupt slot XML inside AB: parseXML fails and the slot keeps its previous
    // (valid) content — crash-smoke for the parseXML-nullptr guard; a later save
    // must still carry a valid slot tree.
    {
        juce::XmlElement root ("AnamorphRoot");
        auto* ab = root.createNewChildElement ("AB");
        ab->setAttribute ("active", 0);
        ab->setAttribute ("slotAParams", "<<< not xml >>>");
        const auto blob = BlobCodec::wrap (root);
        p.setStateInformation (blob.getData(), (int) blob.getSize());
        auto resaved = stateTreeOf (p);
        auto slotA = juce::ValueTree::fromXml (resaved.getChildWithName ("AB")["slotAParams"].toString());
        check (slotA.isValid(), "corrupt slot XML recovers to a valid slot on re-save");
    }

    // Parsable-but-WRONG-TYPED slot payload (ER-STATE-02): must get the same
    // recovery as the unparsable one (slot invalid -> re-seeded), never applied.
    // Before the readSlot type guard, applying such a slot replaceState()'d the
    // foreign type into the live APVTS; every later save then wrote a
    // foreign-typed params child that a fresh instance's restore silently
    // skipped -- delayed, silent loss of all parameters.
    {
        AnamorphAudioProcessor q;
        q.prepareToPlay (48000.0, 512);
        juce::XmlElement root ("AnamorphRoot");
        auto* an  = root.createNewChildElement ("ANAMORPH");
        auto* prm = an->createNewChildElement ("PARAM");
        prm->setAttribute ("id", "drive");
        prm->setAttribute ("value", 12.0);
        auto* ab = root.createNewChildElement ("AB");
        ab->setAttribute ("active", 0);
        ab->setAttribute ("slotBParams", "<Foo/>");
        const auto blob = BlobCodec::wrap (root);
        q.setStateInformation (blob.getData(), (int) blob.getSize());

        q.abSwitchTo (1); // the wrong-typed slot must have been re-seeded, not adopted

        auto resaved = stateTreeOf (q);
        check (resaved.getChildWithName ("ANAMORPH").isValid(),
               "APVTS keeps its ANAMORPH type after applying a wrong-typed slot");

        AnamorphAudioProcessor fresh;
        fresh.prepareToPlay (48000.0, 512);
        juce::MemoryBlock mb;
        q.getStateInformation (mb);
        fresh.setStateInformation (mb.getData(), (int) mb.getSize());
        checkNear ((double) rawOf (fresh, "drive"), 0.5, 1.0e-6,
                   "a session saved after the wrong-typed slot apply still restores its parameters");
    }
}

// ---------------------------------------------------------------------------
static void testPresetSaveReloadRoundTrip()
{
    std::printf ("State test 8: preset save -> reload round-trip + exclusions\n");
    AnamorphAudioProcessor p;
    p.prepareToPlay (48000.0, 512);
    auto& presets = p.getPresets();

    // Distinctive sound state, including a mid-step discrete raw and the
    // preset-EXCLUDED params (mbSolo / advancedMode / bypass).
    setRaw (p, "drive", 0.31f);
    setRaw (p, "width", 0.77f);
    setRaw (p, "algorithm", 0.66f);
    setRaw (p, "monoMakerFreq", 0.42f);
    setRaw (p, "mbSolo", 5.0f / 15.0f);
    setRaw (p, "advancedMode", 1.0f);
    setRaw (p, "bypass", 1.0f);

    const juce::String name = "__AnamorphStateHarness__";
    auto presetFile = anamorph::PresetManager::presetDirectory()
                          .getChildFile (name + anamorph::PresetManager::fileSuffix());

    // The test writes into the REAL user preset folder (the production path).
    // If a genuine user preset with the harness name exists, park it and put it
    // back afterwards — the test must never destroy user data on a dev machine.
    auto parked = juce::File::createTempFile (".parked");
    const bool hadUserFile = presetFile.existsAsFile();
    if (hadUserFile) { parked.deleteFile(); presetFile.moveFileTo (parked); }

    check (presets.saveUser (name), "saveUser succeeds");
    check (presetFile.existsAsFile(), "preset file written");
    checkStr (presets.currentName(), name, "current preset adopts saved name");
    check (! presets.isDirty(), "freshly saved preset is clean");

    // Capture, then perturb everything the reload must undo — and flip the
    // excluded params so the reload provably leaves them alone.
    std::vector<float> savedRaw;
    auto params = rangedParams (p);
    for (auto* rp : params) savedRaw.push_back (rp->getValue());
    const float driveSaved = rawOf (p, "drive");
    setRaw (p, "drive", 0.9f);
    setRaw (p, "width", 0.1f);
    setRaw (p, "algorithm", 0.0f);
    setRaw (p, "monoMakerFreq", 0.8f);
    setRaw (p, "advancedMode", 0.0f);
    setRaw (p, "bypass", 0.0f);
    check (presets.isDirty(), "sound edit marks the preset dirty");

    const int index = presets.currentIndex();
    check (index >= 0, "saved preset appears in the list");
    presets.load (index);

    bool soundRestored = true;
    for (size_t i = 0; i < params.size(); ++i)
    {
        const auto& id = params[i]->paramID;
        if (pid::isPresetExcluded (id)) continue;
        // The .anamorph file stores the DENORMALISED (snapped) value only — no
        // raw attribute — so the preset contract is SNAP-equivalence: a mid-step
        // discrete raw reloads at its snapped step. (Raw-exactness is the host
        // SESSION path's contract, proven in the round-trip test.)
        const double got   = (double) params[i]->convertFrom0to1 (params[i]->getValue());
        const double want  = (double) params[i]->convertFrom0to1 (savedRaw[i]);
        if (std::abs (got - want) > juce::jmax (1.0e-5, 1.0e-5 * std::abs (want)))
        {
            soundRestored = false;
            std::printf ("  [detail] %s: denorm %.9g != saved denorm %.9g\n",
                         id.toRawUTF8(), got, want);
        }
    }
    check (soundRestored, "every sound parameter reload-matches the saved preset (snapped)");
    check (juce::exactlyEqual (rawOf (p, "mbSolo"), p.getAPVTS().getParameter ("mbSolo")->getDefaultValue()),
           "preset load resets mbSolo to default (exclusion rule)");
    check (juce::exactlyEqual (rawOf (p, "advancedMode"), 0.0f), "preset load leaves advancedMode untouched");
    check (juce::exactlyEqual (rawOf (p, "bypass"), 0.0f), "preset load leaves bypass untouched");
    check (! presets.isDirty(), "reloaded preset is clean");

    // The OS-chooser path (loadFile) on a copy of the same file.
    auto tempCopy = juce::File::createTempFile (anamorph::PresetManager::fileSuffix());
    if (presetFile.copyFileTo (tempCopy))
    {
        setRaw (p, "drive", 0.9f);
        check (presets.loadFile (tempCopy), "loadFile loads an arbitrary .anamorph path");
        checkNear ((double) rawOf (p, "drive"), (double) driveSaved, 1.0e-5,
                   "loadFile restores the saved sound");
        tempCopy.deleteFile();
    }

    // Factory preset path stays loadable.
    presets.load (0);
    checkStr (presets.currentName(), "Default", "factory preset 0 loads");
    check (! presets.isDirty(), "factory preset load is clean");

    // Leave no residue in the user's real preset folder (and un-park a real
    // user file of the same name if one was present).
    check (presetFile.deleteFile(), "test preset file removed");
    if (hadUserFile) parked.moveFileTo (presetFile);
    presets.refresh();
}

// ---------------------------------------------------------------------------
static void testAbAndViewParamPreservation()
{
    std::printf ("State test 9: A/B slots + view-param preservation across restore\n");
    AnamorphAudioProcessor p;
    p.prepareToPlay (48000.0, 512);

    setRaw (p, "width", 0.9f);
    p.abCopyToOther();            // slot B := width 0.9
    p.abSwitchTo (1);
    setRaw (p, "width", 0.2f);    // edit slot B's live state
    setRaw (p, "bypass", 1.0f);   // view param: must survive the switch back

    p.abSwitchTo (0);             // stores B (width 0.2), applies A
    check (juce::exactlyEqual (rawOf (p, "bypass"), 1.0f), "A/B switch preserves the live Bypass (view param)");

    juce::MemoryBlock blob;
    p.getStateInformation (blob);

    AnamorphAudioProcessor q;
    q.prepareToPlay (48000.0, 512);
    q.setStateInformation (blob.getData(), (int) blob.getSize());
    check (q.abActiveSlot() == 0, "restored session lands on slot A");
    check (juce::exactlyEqual (rawOf (q, "bypass"), 1.0f), "host restore DOES restore Bypass (full-state path)");
    const float widthA = rawOf (q, "width");
    q.abSwitchTo (1);
    checkNear ((double) rawOf (q, "width"), 0.2, 1.0e-6, "slot B content survives the round-trip");
    check (juce::exactlyEqual (rawOf (q, "bypass"), 1.0f), "A/B switch after restore still preserves Bypass");
    q.abSwitchTo (0);
    check (juce::exactlyEqual (rawOf (q, "width"), widthA), "slot A content survives switching away and back");

    // --- An AB node carrying no USABLE params payload for a slot -----------------
    // A restore must not leave HALF of a slot behind. abSlot[] are processor members and a
    // host may call setStateInformation repeatedly on ONE live instance, so a blob whose AB
    // node exists but whose slot params cannot be read used to keep the PREVIOUS restore's
    // SOUND while that slot's name, baseline and identity were reset around it -- one slot
    // holding two projects. The documented default for the params is "lazily initialised from
    // current" (SERIALIZATION_REGISTRY.md, `AB` child), which abEnsureInit() already
    // implements off StateSet::isValid(), so such a slot must come back INVALID and be
    // re-seeded from the state that was just restored.
    // BOTH slot positions are covered. abEnsureInit() used to seed an invalid slot B from slot A
    // rather than from the live state, so slot B additionally pins that asymmetry: with slot A
    // intact and only slot B unreadable, the pre-fix answer was a DUPLICATE of slot A.
    {
        struct Variant { int slot; void (*breakIt) (juce::ValueTree&); const char* what; };
        const Variant variants[] = {
            { 0, [] (juce::ValueTree& n) { n.removeProperty ("slotAParams", nullptr);
                                           n.removeProperty ("slotA", nullptr); }, // pre-0.6.4 key too
                 "slot A with no params key at all" },
            { 0, [] (juce::ValueTree& n) { n.setProperty ("slotAParams", "<ANAMORPH truncated", nullptr); },
                 "slot A with an unparsable params payload" },
            { 1, [] (juce::ValueTree& n) { n.removeProperty ("slotBParams", nullptr);
                                           n.removeProperty ("slotB", nullptr); },
                 "slot B with no params key at all" },
            { 1, [] (juce::ValueTree& n) { n.setProperty ("slotBParams", "<ANAMORPH truncated", nullptr); },
                 "slot B with an unparsable params payload" },
        };

        for (const auto& v : variants)
        {
            const int   other   = 1 - v.slot;
            const char* brokenK = v.slot == 0 ? "slotAParams" : "slotBParams";

            // Three distinct sounds, so every wrong answer is distinguishable from the right one:
            // 0.70 in the OTHER slot (what a duplicate would produce), 0.90 stale in the broken
            // slot (what keeping the previous restore would produce), 0.45 live (the correct one).
            // The session must PARK on the other slot -- switching away from a slot snapshots the
            // live state into it, which would mask the defect.
            if (q.abActiveSlot() != other) q.abSwitchTo (other);
            setRaw (q, "width", 0.7f);
            q.abSwitchTo (v.slot);            // the other slot := 0.70
            setRaw (q, "width", 0.9f);
            q.abSwitchTo (other);             // the broken slot := 0.90 (stale), parked on `other`
            setRaw (q, "width", 0.45f);       // the live sound the broken session restores to

            const juce::String tag = juce::String (" (") + v.what + ")";
            const auto msgSetup   = "the session being broken really carries that slot's params" + tag;
            const auto msgLive    = "the broken session's live sound restores" + tag;
            const auto msgSound   = "a slot with no usable stored sound is re-seeded from the state "
                                    "just restored -- not the previous session's, not the other slot's" + tag;
            const auto msgMeta    = "...and that slot's metadata comes from the same restore as its sound" + tag;

            auto broken = stateTreeOf (q);
            auto brokenAb = broken.getChildWithName ("AB");
            check (brokenAb.isValid() && brokenAb.hasProperty (brokenK), msgSetup.toRawUTF8());
            v.breakIt (brokenAb);

            if (auto xml = broken.createXml())
            {
                const auto reBlob = BlobCodec::wrap (*xml);
                q.setStateInformation (reBlob.getData(), (int) reBlob.getSize());
                const auto restoredName = q.getPresets().currentName();
                checkNear ((double) rawOf (q, "width"), 0.45, 1.0e-6, msgLive.toRawUTF8());
                q.abSwitchTo (v.slot);
                checkNear ((double) rawOf (q, "width"), 0.45, 1.0e-6, msgSound.toRawUTF8());
                checkStr (q.getPresets().currentName(), restoredName, msgMeta.toRawUTF8());
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Nothing stops a user preset from carrying a factory preset's name, and until
// 0.9.2 the preset list was searched by NAME: the factory block is list-front, so
// the factory row answered for both and the drop-down tick could never sit on the
// user preset -- not even immediately after saving it. Identity is now the factory
// preset's immutable internal id vs. the user preset's FILE, two namespaces that
// cannot collide (#4). This test covers the LIVE behaviour; state test 12 covers the
// same identity across a session save -> reload, including every fallback.
static void testDuplicateNameFactoryVsUserPreset()
{
    std::printf ("State test 10: factory/user preset identity with a shared name\n");
    AnamorphAudioProcessor p;
    p.prepareToPlay (48000.0, 512);
    auto& presets = p.getPresets();

    const juce::String shared = "Wide Master";   // a shipped factory preset
    int factoryIdx = -1, factoryCount = 0;
    for (int i = 0; i < presets.entries().size(); ++i)
    {
        const auto& e = presets.entries().getReference (i);
        if (! e.isFactory) continue;
        ++factoryCount;
        if (e.name == shared) factoryIdx = i;
    }
    check (factoryIdx >= 0, "the shared-name factory preset ships");
    if (factoryIdx < 0) return;

    // Same protocol as test 8: the harness writes into the REAL user preset folder,
    // so park a genuine file of that name and put it back afterwards.
    auto presetFile = anamorph::PresetManager::presetDirectory()
                          .getChildFile (shared + anamorph::PresetManager::fileSuffix());
    auto parked = juce::File::getSpecialLocation (juce::File::tempDirectory)
                      .getChildFile ("AnamorphDupNameHarness.parked");
    const bool hadUserFile = presetFile.existsAsFile();
    if (hadUserFile) { parked.deleteFile(); presetFile.moveFileTo (parked); }

    presets.load (factoryIdx);
    check (presets.currentIndex() == factoryIdx, "the factory preset is current before any user file exists");

    // The case the split exists for: save a user preset under the factory name.
    check (presets.saveUser (shared), "saveUser succeeds under a factory preset's name");
    check (presetFile.existsAsFile(), "user preset file written");
    checkStr (presets.currentName(), shared, "the shared name is still what is DISPLAYED");
    const int userIdx = presets.currentIndex();
    check (userIdx >= factoryCount, "the saved USER preset is current, not the same-named factory one");

    // Both rows remain individually selectable, in both directions.
    presets.load (factoryIdx);
    check (presets.currentIndex() == factoryIdx, "selecting the factory row returns the tick to it");
    presets.load (userIdx);
    check (presets.currentIndex() == userIdx, "selecting the user row moves the tick back to it");

    // A/B carries the identity in memory, so a switch away and back does not snap
    // the tick onto the same-named factory row.
    p.abSwitchTo (1);
    p.abSwitchTo (0);
    check (presets.currentIndex() == userIdx, "A/B switch away and back preserves the user-preset identity");

    // Undo must not yank the tick back to the row that was current before the save.
    // saveUser() changes no parameter, so nothing else refreshes the processor's undo
    // baseline; without the onSaved hook `committed` keeps the pre-save (factory) identity
    // and the first undo restores it.
    presets.load (factoryIdx);
    check (presets.saveUser (shared), "re-save under the shared name");
    check (presets.currentIndex() == userIdx, "the save selects the user row");
    if (auto* drive = p.getAPVTS().getParameter ("drive"))
    {
        drive->beginChangeGesture();               // one finished gesture == one undo step
        drive->setValueNotifyingHost (0.83f);
        drive->endChangeGesture();
    }
    p.pollUndoCoalesce();                          // the editor's 24 Hz poll, driven by hand
    check (p.canUndo(), "the knob edit after a save is undoable");
    p.undo();
    check (presets.currentIndex() == userIdx, "undo after a save keeps the saved preset's identity");

    // A preset switch is a user action even when the two presets SOUND identical, so it must
    // invalidate redo like any other. A surviving entry carries the PREVIOUS preset's identity,
    // so redoing it would move the tick off the row the user just picked.
    check (p.canRedo(), "the undo above leaves a redo entry");
    presets.load (factoryIdx);   // same sound as the user preset it was saved from
    check (! p.canRedo(), "a sonically identical preset switch still invalidates redo");

    // ...but only because the identity MOVED. Re-picking the row that is already ticked
    // changes nothing a redo entry could contradict, so discarding the user's redo there
    // would throw away an undone edit for no benefit.
    if (auto* driveAgain = p.getAPVTS().getParameter ("drive"))
    {
        driveAgain->beginChangeGesture();
        driveAgain->setValueNotifyingHost (0.37f);
        driveAgain->endChangeGesture();
    }
    p.pollUndoCoalesce();
    p.undo();
    check (p.canRedo(), "the second undo leaves a redo entry");
    check (presets.currentIndex() == factoryIdx, "the factory row is the one currently selected");
    presets.load (factoryIdx);   // re-select the ALREADY-current preset
    check (p.canRedo(), "re-selecting the already-current preset preserves redo");

    // A `.anamorph` file from OUTSIDE the preset folder is on no menu row, so nothing is
    // ticked -- it must NOT fall back to the same-named factory row.
    {
        auto outside = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile (shared + anamorph::PresetManager::fileSuffix());
        outside.deleteFile();
        // Asserted, not skipped: a silent `if` here would let the suite report 0 failures on a
        // runner where the copy fails, with these checks never executed at all.
        const bool stagedOutside = presetFile.copyFileTo (outside);
        check (stagedOutside, "outside-folder copy staged");
        if (stagedOutside)
        {
            check (presets.loadFile (outside), "loadFile accepts a preset from outside the folder");
            checkStr (presets.currentName(), shared, "an outside file still displays its own name");
            check (presets.currentIndex() < 0, "an outside file ticks nothing, not the same-named factory row");
            outside.deleteFile();
        }
    }

    // Same rule when the selected user preset disappears from disk.
    presets.load (userIdx);
    check (presetFile.deleteFile(), "user preset file removed while selected");
    presets.refresh();
    check (presets.currentIndex() < 0, "a deleted user preset ticks nothing, not the same-named factory row");
    check (presets.saveUser (shared), "re-create the user preset for the restore check");

    // The session carries the identity too since 0.9.2, so the tick survives a reload.
    // (State test 12 covers the restore matrix in full, including the fallbacks.)
    check (presets.currentIndex() == userIdx, "the re-created user preset lands on the same row");
    presets.load (userIdx);
    juce::MemoryBlock blob;
    p.getStateInformation (blob);
    {
        AnamorphAudioProcessor q;
        q.prepareToPlay (48000.0, 512);
        q.setStateInformation (blob.getData(), (int) blob.getSize());
        checkStr (q.getPresets().currentName(), shared, "restored session remembers the shared name");
        check (q.getPresets().currentIndex() == userIdx,
               "a restored session keeps the tick on the USER row, not the same-named factory one");
    }

    check (presetFile.deleteFile(), "test preset file removed");
    if (hadUserFile) parked.moveFileTo (presetFile);
    presets.refresh();
}

// ---------------------------------------------------------------------------
// The factory ids are the identity half of ADR-0024, and `refresh()` copies them
// straight out of the table into the rows the menu shows. Nothing in the type system
// stops a duplicated or edited id, and the failure would be quiet: `load()` would find
// no overrides and apply the plain defaults. These checks make that loud.
static void testFactoryPresetIdIntegrity()
{
    std::printf ("State test 11: factory-preset id integrity\n");
    AnamorphAudioProcessor p;
    auto& presets = p.getPresets();

    // A freshly constructed processor sits on all-parameter defaults and the manager takes
    // its baseline there, so this string IS the all-defaults sound signature — obtained
    // without reaching into the private applyDefaults().
    const juce::String defaultsSig = presets.baseline();

    juce::StringArray ids;
    bool everyFactoryIdIsSet = true, userRowsCarryNoId = true;
    for (const auto& e : presets.entries())
    {
        if (e.isFactory)
        {
            everyFactoryIdIsSet = everyFactoryIdIsSet && e.factoryId.isNotEmpty();
            ids.add (e.factoryId);
        }
        else
        {
            userRowsCarryNoId = userRowsCarryNoId && e.factoryId.isEmpty();
        }
    }
    check (ids.size() >= 2, "factory presets ship");
    check (everyFactoryIdIsSet, "every factory row carries a non-empty id");
    check (userRowsCarryNoId, "user rows carry no factory id");

    juce::StringArray uniqueIds (ids);
    uniqueIds.removeDuplicates (false); // case-SENSITIVE: the ids are exact tokens
    check (uniqueIds.size() == ids.size(), "factory ids are unique");

    // Every id must RESOLVE in the table. One that does not would apply the defaults and
    // nothing else, landing on the all-defaults signature — and exactly one factory preset
    // (the one with an empty override set) is allowed to sit there.
    int atDefaults = 0;
    for (int i = 0; i < presets.entries().size(); ++i)
    {
        if (! presets.entries().getReference (i).isFactory) continue;
        presets.load (i);
        if (presets.baseline() == defaultsSig) ++atDefaults;
    }
    check (atDefaults == 1,
           "exactly one factory preset is the all-defaults one -- every other id resolves to its overrides");
}

// ---------------------------------------------------------------------------
// The indicator identity is carried in the SESSION since 0.9.2 (ADR-0024 amendment),
// so reopening a project puts the tick back on the row that produced the sound even
// when a user preset shares a factory preset's name. Three additive root properties and
// three per A/B slot; user preset FILES are untouched. Every path below also asserts
// that the restored PARAMETERS are bit-identical, because the identity is metadata and
// must never influence the sound — including when it fails to resolve.
static void testPresetIndicatorIdentityAcrossRestore()
{
    std::printf ("State test 12: preset indicator identity survives a session reload\n");

    auto rawSnapshot = [] (AnamorphAudioProcessor& proc)
    {
        std::vector<float> v;
        for (auto* rp : rangedParams (proc)) v.push_back (rp->getValue());
        return v;
    };
    auto sameRaw = [] (const std::vector<float>& a, const std::vector<float>& b)
    {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (! juce::exactlyEqual (a[i], b[i])) return false;
        return true;
    };
    // Restore `blob` into a NEW processor and report (index, name, params-match).
    struct Restored { int index; juce::String name; bool paramsMatch; };
    auto restoreInto = [&] (const juce::MemoryBlock& blob, const std::vector<float>& expectRaw)
    {
        AnamorphAudioProcessor q;
        q.prepareToPlay (48000.0, 512);
        q.setStateInformation (blob.getData(), (int) blob.getSize());
        return Restored { q.getPresets().currentIndex(), q.getPresets().currentName(),
                          sameRaw (rawSnapshot (q), expectRaw) };
    };

    const juce::String shared = "Wide Master";   // a shipped factory preset
    AnamorphAudioProcessor p;
    p.prepareToPlay (48000.0, 512);
    auto& presets = p.getPresets();

    int factoryIdx = -1;
    for (int i = 0; i < presets.entries().size(); ++i)
    {
        const auto& e = presets.entries().getReference (i);
        if (e.isFactory && e.name == shared) factoryIdx = i;
    }
    check (factoryIdx >= 0, "the shared-name factory preset ships");
    if (factoryIdx < 0) return;

    auto presetFile = anamorph::PresetManager::presetDirectory()
                          .getChildFile (shared + anamorph::PresetManager::fileSuffix());
    auto parked = juce::File::getSpecialLocation (juce::File::tempDirectory)
                      .getChildFile ("AnamorphIndicatorHarness.parked");
    const bool hadUserFile = presetFile.existsAsFile();
    if (hadUserFile) { parked.deleteFile(); presetFile.moveFileTo (parked); }

    // --- Case 1: a FACTORY preset is current -------------------------------------
    presets.load (factoryIdx);
    const auto factoryRaw = rawSnapshot (p);
    juce::MemoryBlock factoryBlob;
    p.getStateInformation (factoryBlob);
    {
        const auto r = restoreInto (factoryBlob, factoryRaw);
        check (r.index == factoryIdx, "case 1: a restored session ticks the factory preset it was on");
        checkStr (r.name, shared, "case 1: the displayed name restores");
        check (r.paramsMatch, "case 1: parameters restore bit-identically");
    }

    // --- Case 1 fallback: the stored factory id no longer exists -------------------
    {
        auto tree = juce::ValueTree::fromXml (*BlobCodec::unwrap (factoryBlob));
        tree.setProperty ("presetFactoryId", "aPresetThatWasRemoved", nullptr);
        const auto r = restoreInto (BlobCodec::wrap (*tree.createXml()), factoryRaw);
        check (r.index < 0, "case 1 fallback: an unresolvable factory id ticks NOTHING");
        check (r.paramsMatch, "case 1 fallback: parameters still restore bit-identically");
    }

    // --- Case 2: a USER preset sharing the factory name is current ----------------
    check (presets.saveUser (shared), "a user preset can be saved under the factory name");
    const int userIdx = presets.currentIndex();
    check (userIdx > factoryIdx, "the saved user preset sits below the factory block");
    setRaw (p, "drive", 0.61f);                  // make the user preset's sound distinct
    check (presets.saveUser (shared), "re-save so the file matches the live sound");
    const auto userRaw = rawSnapshot (p);
    juce::MemoryBlock userBlob;
    p.getStateInformation (userBlob);
    {
        const auto r = restoreInto (userBlob, userRaw);
        check (r.index == userIdx, "case 2: a restored session ticks the USER row, not the same-named factory one");
        checkStr (r.name, shared, "case 2: the displayed name restores");
        check (r.paramsMatch, "case 2: parameters restore bit-identically");
    }

    // --- Case 2, nested: a preset under a SUB-folder of the preset folder ----------
    // `refresh()` scans non-recursively, so a nested file is on no menu row and must tick
    // nothing — before AND after a reload. It is the case where encoding by file NAME would
    // silently re-point the identity at the same-named preset sitting directly in the folder,
    // which still exists at this point in the test.
    {
        auto nestedDir = anamorph::PresetManager::presetDirectory().getChildFile ("AnamorphHarnessNested");
        auto nested    = nestedDir.getChildFile (shared + anamorph::PresetManager::fileSuffix());
        nested.deleteFile();
        // Asserted for the same reason as the outside-folder case above: this is the guard for
        // the isAChildOf-vs-direct-child fix, and a silent skip would leave it unguarded while
        // the suite still exits 0.
        const bool stagedNested = nestedDir.createDirectory() && presetFile.copyFileTo (nested);
        check (stagedNested, "nested sub-folder copy staged");
        if (stagedNested)
        {
            check (presets.loadFile (nested), "loadFile accepts a preset from a sub-folder");
            check (presets.currentIndex() < 0, "a nested preset ticks nothing while live");
            const auto nestedRaw = rawSnapshot (p);
            juce::MemoryBlock nestedBlob;
            p.getStateInformation (nestedBlob);
            const auto r = restoreInto (nestedBlob, nestedRaw);
            check (r.index < 0,
                   "case 2 nested: a reloaded nested preset ticks nothing, not the same-named row in the folder");
            check (r.paramsMatch, "case 2 nested: parameters restore bit-identically");
            nested.deleteFile();
        }
        nestedDir.deleteRecursively();
        presets.refresh();
        presets.load (userIdx);   // back to the flat user preset for the checks below
    }

    // --- Case 2 fallback: the user preset file is gone ----------------------------
    {
        check (presetFile.deleteFile(), "user preset file removed before the restore");
        const auto r = restoreInto (userBlob, userRaw);
        check (r.index < 0, "case 2 fallback: a missing user preset ticks NOTHING, not the same-named factory row");
        checkStr (r.name, shared, "case 2 fallback: the displayed name still restores");
        check (r.paramsMatch, "case 2 fallback: parameters still restore bit-identically");
    }

    // --- Case 3: a pre-0.9.2 session, with no identity stored ---------------------
    {
        auto tree = juce::ValueTree::fromXml (*BlobCodec::unwrap (userBlob));
        tree.removeProperty ("presetSource",    nullptr);
        tree.removeProperty ("presetFactoryId", nullptr);
        tree.removeProperty ("presetUserFile",  nullptr);
        check (! tree.hasProperty ("presetSource"), "the pre-0.9.2 fixture really has no identity");
        // The file is still deleted here, so the ONLY thing the name could resolve to is
        // the factory row -- which is exactly the documented pre-0.9.2 answer.
        const auto r = restoreInto (BlobCodec::wrap (*tree.createXml()), userRaw);
        check (r.index == factoryIdx, "case 3: a session with no stored identity falls back to the name");
        check (r.paramsMatch, "case 3: parameters still restore bit-identically");
    }

    // --- A/B: each slot carries its own identity across the reload ----------------
    {
        check (presets.saveUser (shared), "re-create the user preset for the A/B check");
        const int userRow = presets.currentIndex();
        p.abSwitchTo (1);
        presets.load (factoryIdx);        // slot B := the factory preset
        const auto abRaw = rawSnapshot (p);
        juce::MemoryBlock abBlob;
        p.getStateInformation (abBlob);

        AnamorphAudioProcessor q;
        q.prepareToPlay (48000.0, 512);
        q.setStateInformation (abBlob.getData(), (int) abBlob.getSize());
        check (q.abActiveSlot() == 1, "the restored session lands on slot B");
        check (q.getPresets().currentIndex() == factoryIdx, "slot B's factory identity restores");
        check (sameRaw (rawSnapshot (q), abRaw), "slot B parameters restore bit-identically");
        q.abSwitchTo (0);
        check (q.getPresets().currentIndex() == userRow,
               "slot A's USER identity restores independently of slot B's");
    }

    // --- A user preset whose FILE NAME looks like an absolute path --------------
    // Nothing stops a user dropping `~foo.anamorph` into the preset folder by hand -- the
    // manual tells them to manage presets as files -- and `juce::File::isAbsolutePath`
    // accepts a leading `~` on POSIX. Encoding such a direct child by BARE NAME would come
    // back from the decoder as the literal relative string, so the row would silently lose
    // its tick on reload. The encoder must fall back to the absolute path for it.
    {
        // Built from a full path string on purpose: getChildFile would short-circuit on the
        // very ambiguity under test.
        auto tilde = juce::File (anamorph::PresetManager::presetDirectory().getFullPathName()
                                     + juce::File::getSeparatorString()
                                     + "~AnamorphTildeHarness" + anamorph::PresetManager::fileSuffix());
        tilde.deleteFile();
        const bool stagedTilde = presetFile.copyFileTo (tilde);
        check (stagedTilde, "tilde-named preset staged");
        if (stagedTilde)
        {
            presets.refresh();
            int tildeIdx = -1;
            for (int i = 0; i < presets.entries().size(); ++i)
                if (presets.entries().getReference (i).file == tilde) tildeIdx = i;
            check (tildeIdx >= 0, "the tilde-named preset appears as a menu row");
            if (tildeIdx >= 0)
            {
                presets.load (tildeIdx);
                check (presets.currentIndex() == tildeIdx, "the tilde-named preset is current while live");
                const auto tildeRaw = rawSnapshot (p);
                juce::MemoryBlock tildeBlob;
                p.getStateInformation (tildeBlob);
                const auto r = restoreInto (tildeBlob, tildeRaw);
                check (r.index == tildeIdx,
                       "a tilde-named preset keeps its tick across a reload (encode/decode round-trips)");
                check (r.paramsMatch, "tilde-named preset: parameters restore bit-identically");
            }
            tilde.deleteFile();
        }
        presets.refresh();
    }

    // --- A repeated restore must not inherit the previous project's preset NAME ----
    // Hosts call setStateInformation on ONE live processor any number of times, so the root
    // metadata follows the same rule readSlot follows for the A/B slots: an absent or empty
    // field resolves to its own default, never to what the previous project left behind. The
    // two cases are different answers and only setStateInformation can tell them apart:
    //   * `presetName` PRESENT but empty -- a real state since 0.9.2 (a session saved while
    //     sitting on a nameless A/B slot stores exactly that) -- is adopted verbatim;
    //   * `presetName` ABSENT (a session predating the field, < 0.6) resolves to
    //     PresetManager::defaultName(), a CONSTANT, whose name-fallback tick is the documented
    //     ADR-0024 answer for a session that carries no identity.
    // Both adoption paths are exercised: with `presetBaseline` present (setMeta) and without
    // it (adoptRestoredState), because each used to inherit in its own way.
    {
        struct NameCase { bool stripName, stripBaseline; const char* expected; const char* what; };
        const NameCase nameCases[] = {
            { false, false, "",        "empty presetName, baseline present"  },
            { false, true,  "",        "empty presetName, no baseline"       },
            { true,  false, "Default", "absent presetName, baseline present" },
            { true,  true,  "Default", "absent presetName, no baseline"      },
        };

        for (const auto& c : nameCases)
        {
            const juce::String tag = juce::String (" (") + c.what + ")";
            const auto msgSetup = "project A really has a preset name to leak" + tag;
            const auto msgName  = "a repeated restore resolves the preset name from the session, "
                                  "not from the previous project" + tag;
            const auto msgTick  = "...so the drop-down cannot tick the previous project's row" + tag;

            AnamorphAudioProcessor r;
            r.prepareToPlay (48000.0, 512);
            r.getPresets().load (1);                    // project A: a named factory preset
            const auto projectAName = r.getPresets().currentName();
            const int  projectARow  = r.getPresets().currentIndex();
            check (projectAName.isNotEmpty() && projectAName != "Default" && projectARow > 0,
                   msgSetup.toRawUTF8());

            // Project B: the same session shape, carrying no identity (so the name fallback is
            // what resolves the tick) and no usable preset name.
            auto projectB = stateTreeOf (r);
            projectB.removeProperty ("presetSource",    nullptr);
            projectB.removeProperty ("presetFactoryId", nullptr);
            projectB.removeProperty ("presetUserFile",  nullptr);
            if (c.stripName)      projectB.removeProperty ("presetName", nullptr);
            else                  projectB.setProperty   ("presetName", "", nullptr);
            if (c.stripBaseline)  projectB.removeProperty ("presetBaseline", nullptr);

            if (auto xml = projectB.createXml())
            {
                const auto blobB = BlobCodec::wrap (*xml);
                r.setStateInformation (blobB.getData(), (int) blobB.getSize()); // SAME live instance
                checkStr (r.getPresets().currentName(), c.expected, msgName.toRawUTF8());
                check (r.getPresets().currentIndex() != projectARow, msgTick.toRawUTF8());
            }
        }
    }

    check (presetFile.deleteFile(), "test preset file removed");
    if (hadUserFile) parked.moveFileTo (presetFile);
    presets.refresh();
}

// ---------------------------------------------------------------------------
// 11. Wrapper audio path: the REAL AnamorphAudioProcessor::processBlock over a
//     denormal-provoking noise -> silence matrix. This is the only test in
//     either suite that drives the wrapper's audio path (the DSP suite drives
//     the engine directly), so it is what points the sanitizers/valgrind runs
//     of THIS suite at processBlock's own code -- parameter snapshotting,
//     mono up-mix guards, transport handling and the ScopedNoDenormals guard.
//
//     Deliberately NO test-side juce::ScopedNoDenormals here: processBlock
//     arms its own (src/PluginProcessor.cpp), and this test regresses exactly
//     that -- were the guard ever lost, the silence phase would flush nothing
//     and the denormal assertion below would fail on every native runner.
//     The ANAMORPH_TESTS_NO_FTZ escape mirrors tests/dsp_tests.cpp `isBad`
//     (see the full reasoning there): valgrind emulates FP without honouring
//     the FTZ/DAZ bits, so under memcheck only the NaN/Inf half is asserted.
static void testWrapperProcessBlockAudioPath()
{
    std::printf ("Wrapper audio path: real processBlock, NaN/Inf/denormal-free (own FTZ guard)\n");

    const bool ftzUnavailable = []
    {
        const char* const v = std::getenv ("ANAMORPH_TESTS_NO_FTZ");
        return v != nullptr && std::strcmp (v, "1") == 0;
    }();
    if (ftzUnavailable)
        std::printf ("  ::warning::ANAMORPH_TESTS_NO_FTZ=1 -- the DENORMAL half of this "
                     "test is NOT asserted in this run (NaN and Inf still are).\n");

    AnamorphAudioProcessor proc;

    // The DSP suite's denormal-provoking configuration (dsp_tests.cpp Test 2):
    // drive 8 dB, width 1.6, Multiband + Level Match engaged -- set through the
    // real APVTS in real units, so the wrapper's own snapshot code runs.
    auto setReal = [&proc] (const char* id, float real)
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (proc.getAPVTS().getParameter (id));
        ++checks;
        if (rp == nullptr)
        {
            ++failures;
            std::printf ("  [FAIL] parameter missing for wrapper audio-path test: %s\n", id);
            return;
        }
        rp->setValueNotifyingHost (rp->convertTo0to1 (real));
    };
    setReal ("drive",         8.0f);
    setReal ("width",         1.6f);
    setReal ("mbEnable",      1.0f);
    setReal ("autoGainMatch", 1.0f);

    const double sr = 48000.0;
    const int block = 256;
    proc.prepareToPlay (sr, block);

    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    std::mt19937 rng (98765);
    std::uniform_real_distribution<float> dist (-0.7f, 0.7f);

    bool anyBad = false;
    double noiseSq = 0.0; long noiseCount = 0;
    const int blocksPerPhase = 120;
    for (int phase = 0; phase < 2; ++phase)          // noise, then silence
        for (int nb = 0; nb < blocksPerPhase; ++nb)
        {
            if (phase == 0)
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < block; ++i)
                        buf.setSample (ch, i, dist (rng));
            else
                buf.clear();

            midi.clear();
            proc.processBlock (buf, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                {
                    const float x = buf.getSample (ch, i);
                    if (std::isnan (x) || std::isinf (x)) anyBad = true;
                    else if (! ftzUnavailable)
                    {
                        const float a = std::abs (x);
                        if (a > 0.0f && a < 1.17549435e-38f) anyBad = true; // denormal
                    }
                    if (phase == 0)
                    {
                        noiseSq += static_cast<double> (x) * static_cast<double> (x);
                        ++noiseCount;
                    }
                }
        }

    const double noiseRms = std::sqrt (noiseSq / static_cast<double> (juce::jmax (1L, noiseCount)));
    std::printf ("  noise-phase output RMS %.4f over %d blocks\n", noiseRms, blocksPerPhase);
    // Liveness first: a silently-null path would make the invariant vacuous.
    check (noiseRms > 0.05, "wrapper audio path produces output (assertion is not vacuous)");
    check (! anyBad, "wrapper processBlock output free of NaN/Inf/denormals");

    proc.releaseResources();
}

// ---------------------------------------------------------------------------
// ============================================================================
//  Tooltip source of truth.
//
//  Regression net for the 0.9.4 defect "the tooltip text becomes a neighbouring
//  control's while the box repositions, and stays wrong until the mouse moves
//  one more pixel".
//
//  JUCE's tooltip tick mixes two sources: the TEXT comes from
//  getComponentUnderMouse(), which is a CACHE refreshed only by mouse events,
//  and the BOX POSITION from getScreenPosition(), which asks the OS live
//  (juce_TooltipWindow.cpp:209/221/223). When they disagree the box is placed
//  where the pointer really is and labelled with a control it is not over --
//  and nothing but an event refreshes the cache, so it persists.
//
//  AnamorphAudioProcessorEditor's TooltipSource::choose is the reconciliation,
//  kept pure so the decision runs here with no display, no editor and no
//  tooltip window. The geometry below is the measured Settings layout.
// ============================================================================
static void testTooltipSourceOfTruth()
{
    std::printf ("Tooltip source of truth: the cached component vs the live pointer\n");

    // The measured Settings-panel rows (screen coordinates from the reproduction).
    struct Row { const char* name; juce::Rectangle<int> bounds; };
    const Row oversampling { "Oversampling",       { 804, 630, 312, 23 } };
    const Row uiScale      { "UI Scale",           { 804, 687, 312, 23 } };
    const Row persist      { "Vectorscope Persist",{ 796, 743, 320, 24 } };

    juce::Component cOvers, cScale, cPersist;
    cOvers  .setBounds (oversampling.bounds);
    cScale  .setBounds (uiScale.bounds);
    cPersist.setBounds (persist.bounds);

    const auto inScale = uiScale.bounds.getCentre();
    const auto inOvers = oversampling.bounds.getCentre();

    // --- the reported gesture -----------------------------------------------------------------
    // The cache says Oversampling; the pointer is really on UI Scale. Before the fix the tooltip
    // took the cached answer and displayed the Oversampling text over the UI Scale row.
    check (TooltipSource::choose (&cOvers, inScale, &cScale) == &cScale,
           "a stale cached component loses to what is really under the live pointer");
    check (TooltipSource::choose (&cScale, inOvers, &cOvers) == &cOvers,
           "and in the other direction too");
    check (TooltipSource::choose (&cScale, persist.bounds.getCentre(), &cPersist)
               == &cPersist,
           "Vectorscope Persist: same");

    // --- the behaviour that must NOT change ---------------------------------------------------
    // The overwhelmingly common case: the cache agrees with the pointer, so nothing is overridden.
    check (TooltipSource::choose (&cScale, inScale, &cScale) == &cScale,
           "an agreeing cache is used unchanged");
    check (TooltipSource::choose (&cScale, inScale, &cOvers) == &cScale,
           "an agreeing cache WINS -- the live hit test never overrides a consistent answer");
    check (TooltipSource::choose (&cScale, uiScale.bounds.getTopLeft(), &cOvers)
               == &cScale,
           "the top-left corner counts as inside the cached component");
    check (TooltipSource::choose (&cScale, uiScale.bounds.getBottomRight(), &cPersist)
               != &cScale,
           "one pixel past the bottom-right does not");

    // --- nothing under the pointer means NO tip, never a stale one -----------------------------
    check (TooltipSource::choose (&cScale, { 10, 10 }, nullptr) == nullptr,
           "a stale cache with nothing under the pointer yields no tooltip at all");
    check (TooltipSource::choose (nullptr, inScale, &cScale) == &cScale,
           "no cached component: the live one is used");
    check (TooltipSource::choose (nullptr, inScale, nullptr) == nullptr,
           "neither: no tooltip");

    // --- leaving a control must still dismiss --------------------------------------------------
    // The pointer moves off every row; the live hit test finds nothing, so the answer is nullptr,
    // which the caller turns into an empty tip and JUCE turns into hideTip().
    check (TooltipSource::choose (&cScale, { 804, 900 }, nullptr) == nullptr,
           "moving off the control still drops the tip (no 'tooltip stays visible' regression)");

    std::printf ("\n");
}

// ============================================================================
//  Editor construct / destroy -- the one lifetime this suite compiled and never
//  ran.
//
//  WHAT WAS ALREADY TRUE. This target already compiles `PluginEditor.cpp` (it is
//  in `ANAMORPH_PLUGIN_SOURCES`) and already links `juce_audio_utils`,
//  `juce_dsp` and `juce_opengl`, because `createEditor()` references them. It
//  had never CALLED it. So the editor constructor and destructor -- 68 direct
//  children, three LookAndFeels, an `OpenGLContext` member, a `VBlankAttachment`
//  and a `FrameClock` -- were the largest piece of first-party code in this
//  repository that no instrument had ever executed.
//
//  WHY IT GOES HERE AND NOT IN A NEW TARGET. A new CMake target is a gated Build
//  System change (`ARCHITECTURE_REVIEW_GATE.md`) and would have bought nothing:
//  this binary already has every source and every module the editor needs. What
//  it buys instead is the reason to do it at all -- `AnamorphStateTests` is
//  already run under ASan+UBSan, under valgrind memcheck, against LTO codegen and
//  on three platforms, so putting the editor's lifetime inside it puts that
//  lifetime under all of them at once, at no CI cost beyond its own runtime.
//
//  NO WINDOW IS OPENED. The editor is constructed as a free-standing Component
//  and never added to the desktop, so no peer is created and no display is
//  touched -- asserted below rather than assumed, because "it worked headlessly"
//  and "it quietly opened something" are indistinguishable in a passing run.
//
//  LINUX ONLY, DELIBERATELY, and this is a scoping decision rather than a
//  discovery. It is verified headless on Linux with `DISPLAY` unset. It is NOT
//  verified on Windows or macOS, this suite is a BLOCKING gate on all three, and
//  `KNOWN_ISSUES.md` KI-007 already records that the GPU-less Windows CI runner
//  cannot host editor GUI tests at all. Every instrument this test exists to
//  feed -- ASan, UBSan, valgrind, LTO, RTSan -- runs on Linux, so the scoping
//  costs none of the coverage and removes the whole risk of a headless
//  construction failing a platform nobody measured. Widening it needs one green
//  run on the other two, not an argument.
// ============================================================================
static void testEditorConstructDestroy()
{
    std::printf ("Editor lifetime: construct, lay out and destroy with no window\n");

#if ! (JUCE_LINUX || JUCE_BSD)
    std::printf ("  SKIPPED off Linux -- headless construction is unverified there "
                 "(KI-007); every instrument this feeds is a Linux job.\n");
#else
    AnamorphAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);

    // THE PREMISE, CHECKED LOUDLY AND FIRST. If `createEditor()` ever returns
    // null or something else, every assertion below is vacuously true and the
    // test reports a confident pass over nothing -- the exact vacuity
    // TESTING_POLICY rule 4 exists to forbid.
    auto* raw = proc.createEditor();
    check (raw != nullptr, "createEditor() returns an editor");
    auto* ed = dynamic_cast<AnamorphAudioProcessorEditor*> (raw);
    check (ed != nullptr, "...and it is an AnamorphAudioProcessorEditor");
    if (ed == nullptr)
    {
        delete raw;
        return;
    }

    // LAYOUT ACTUALLY RAN, and this is the liveness proof for everything after
    // it: the constructor's `applyUiScale()` is what calls `setSize`, and its
    // child components are built along the way, so a constructor that bailed
    // early leaves a 0x0 component with no children and fails HERE rather than
    // leaving the checks below quietly true of nothing.
    //
    // NON-DEGENERATE RATHER THAN EXACT, deliberately. `kWidth`/`kHeight` are
    // private to the editor and widening them for a test would change the
    // production class to suit its test; and pinning 940x720 as literals here
    // would put the window size in a second place, which is the copy that rots.
    // The measured size is PRINTED, so a human reading a run still sees it.
    check (ed->getWidth() > 0 && ed->getHeight() > 0,
           "the editor laid itself out with a real size");
    check (ed->getNumChildComponents() > 0, "the editor built its child components");
    std::printf ("  laid out %dx%d with %d direct children\n",
                 ed->getWidth(), ed->getHeight(), ed->getNumChildComponents());

    // NOTHING WAS SHOWN. A free-standing Component has no peer until something
    // adds it to the desktop; asserting it stays null is what makes "headless"
    // a property of this test rather than a hope about the environment.
    check (ed->getPeer() == nullptr, "no window was opened (the editor has no peer)");

    proc.editorBeingDeleted (ed);
    delete ed;

    // REPEATED, because a single construct/destroy exercises the paths but not
    // the ORDER problems: a listener left attached, a LookAndFeel outliving its
    // users, a VBlankAttachment surviving its component. Those show up on the
    // second construction or in the sanitizer's report, not the first.
    for (int i = 0; i < 4; ++i)
    {
        auto* again = dynamic_cast<AnamorphAudioProcessorEditor*> (proc.createEditor());
        check (again != nullptr, "the editor can be rebuilt after being destroyed");
        if (again == nullptr)
            break;
        proc.editorBeingDeleted (again);
        delete again;
    }
    std::printf ("  constructed and destroyed 5 times, clean\n");
#endif
}

// ---------------------------------------------------------------------------
// 17. A NON-FINITE value in a session must never become parameter state.
//
//     `nan` is ordinary text to JUCE's number parser (juce_CharacterFunctions.h
//     accepts it), so a hand-edited or corrupted session can carry
//     <PARAM id="width" value="nan"/>. Nothing on the restore path used to
//     reject it: apvts.replaceState() itself pushes @value through
//     setDenormalisedValue -> setValueNotifyingHost, whose approximatelyEqual
//     guard is false for NaN, and reassertParameters' own write gate
//     (|norm - current| > 1e-6) is likewise false for NaN, so it neither
//     injected nor repaired it. The preset path (PresetManager::applySoundTree)
//     had no gate at all.
//
//     The consequence is not cosmetic. A NaN continuous parameter latches its
//     smoother target, every output sample goes non-finite, and ADR-0009's
//     sample-level self-heal then zeroes the block and resets the engine on
//     EVERY block: permanent silence, with a plausible-looking UI. It also
//     round-trips -- getStateInformation writes `nan` straight back out -- so
//     reopening the project reproduces it.
static void testNonFiniteParameterInStateIsRejected()
{
    std::printf ("State test 17: a non-finite value in a session is rejected, not adopted\n");

    // Author a normal session, then poison one parameter's serialized value.
    juce::MemoryBlock poisoned;
    {
        AnamorphAudioProcessor authoring;
        authoring.prepareToPlay (48000.0, 256);
        setRaw (authoring, pid::width, 0.8f);
        juce::MemoryBlock clean;
        authoring.getStateInformation (clean);

        auto xml = BlobCodec::unwrap (clean);
        check (xml != nullptr, "authored session decodes for poisoning");
        if (xml == nullptr) return;

        bool poisonedOne = false;
        if (auto* params = xml->getChildByName ("ANAMORPH"))
            for (auto* param : params->getChildIterator())
                if (param->getStringAttribute ("id") == pid::width)
                {
                    param->setAttribute ("value", "nan");
                    param->removeAttribute ("raw"); // the exact-raw path is separately covered below
                    poisonedOne = true;
                }
        check (poisonedOne, "the session carries a width PARAM to poison");
        poisoned = BlobCodec::wrap (*xml);
    }

    AnamorphAudioProcessor proc;
    proc.setStateInformation (poisoned.getData(), (int) poisoned.getSize());

    const float restored = rawOf (proc, pid::width);
    std::printf ("  width after restoring value=\"nan\": %f\n", restored);
    check (std::isfinite (restored), "a non-finite serialized value does not become parameter state");

    // ...and the audio path stays alive: the failure mode this guards is not a
    // wrong number, it is permanent silence.
    proc.prepareToPlay (48000.0, 256);
    juce::AudioBuffer<float> buf (2, 256);
    juce::MidiBuffer midi;
    std::mt19937 rng (13579);
    std::uniform_real_distribution<float> dist (-0.7f, 0.7f);
    double peak = 0.0;
    bool allFinite = true;
    for (int nb = 0; nb < 8; ++nb)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 256; ++i)
                buf.setSample (ch, i, dist (rng));
        proc.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 256; ++i)
            {
                const float s = buf.getSample (ch, i);
                if (! std::isfinite (s)) allFinite = false;
                peak = juce::jmax (peak, std::abs ((double) s));
            }
    }
    std::printf ("  output peak over 8 blocks: %.6f\n", peak);
    check (allFinite, "output stays finite after a poisoned restore");
    check (peak > 0.01, "the plug-in still passes audio (a NaN parameter used to silence it)");

    // The same poison in a PRESET FILE takes a different code path
    // (PresetManager::applySoundTree), which had no gate of its own. Driven
    // end to end through the real loader, on a real file.
    {
        AnamorphAudioProcessor viaPreset;
        viaPreset.prepareToPlay (48000.0, 256);
        auto& pm = viaPreset.getPresets();

        const auto presetFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                    .getChildFile ("anamorph_nonfinite_probe.anamorph");
        presetFile.deleteFile();

        auto tree = juce::ValueTree ("ANAMORPH");
        auto node = juce::ValueTree ("PARAM");
        node.setProperty ("id", pid::width, nullptr);
        node.setProperty ("value", "nan", nullptr);
        tree.appendChild (node, nullptr);
        if (auto xml = tree.createXml())
            check (xml->writeTo (presetFile), "poisoned preset file written");

        check (pm.loadFile (presetFile), "the poisoned preset file loads (it is well-formed XML)");
        std::printf ("  width after loading a preset with value=\"nan\": %f\n",
                     rawOf (viaPreset, pid::width));
        check (std::isfinite (rawOf (viaPreset, pid::width)),
               "a non-finite value in a preset file does not become parameter state");
        presetFile.deleteFile();
    }
}

// ---------------------------------------------------------------------------
// 18. A PARAM node that carries NO `value` means "not in this file", i.e. the
//     parameter default -- not zero.
//
//     Same surface as test 17 and the same reason it is reachable: a preset is
//     user-editable text, and a truncated write or a hand edit can leave
//     <PARAM id="width"/> behind. applySoundTree keyed its "did the file carry
//     this parameter?" question off child.isValid() alone, so a value-less node
//     read as `var()` -> (double) 0.0 -> convertTo0to1(0.0): the range MINIMUM,
//     silently, for every parameter whose node lost its value.
//
//     Width is the sharp case -- range 0..2 with default 1.0 -- so the failure
//     is not a nudge but a full mono collapse, applied without any error the
//     user could see. The rule this restores is the one the surrounding code
//     already states for a missing child, and the one readSlot follows for the
//     A/B slots: absent means default.
static void testValuelessParamMeansDefault()
{
    std::printf ("State test 18: a PARAM with no value means default, not zero\n");

    AnamorphAudioProcessor proc;
    proc.prepareToPlay (48000.0, 256);
    auto& pm = proc.getPresets();

    // Normalised throughout: rawOf/setRaw speak AudioProcessorParameter's 0..1.
    // Width's default is the range MIDPOINT (0..2, default 1), which is what
    // makes it a clean probe -- 0.0 is a value the file could plausibly mean,
    // and it is nowhere near the default.
    auto* widthParam = proc.getAPVTS().getParameter (pid::width);
    check (widthParam != nullptr, "width parameter exists");
    if (widthParam == nullptr) return;
    const float expected = widthParam->getDefaultValue();

    // Move it away from the default first, so "came back to default" cannot be
    // confused with "was never touched".
    setRaw (proc, pid::width, 0.95f);

    const auto presetFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("anamorph_valueless_probe.anamorph");
    presetFile.deleteFile();

    auto tree = juce::ValueTree ("ANAMORPH");
    auto node = juce::ValueTree ("PARAM");
    node.setProperty ("id", pid::width, nullptr); // id but NO value -- the truncated-write shape
    tree.appendChild (node, nullptr);
    if (auto xml = tree.createXml())
        check (xml->writeTo (presetFile), "value-less preset file written");

    check (pm.loadFile (presetFile), "the value-less preset file loads (it is well-formed XML)");
    const float after = rawOf (proc, pid::width);
    std::printf ("  width after loading a preset with a value-less PARAM: %f\n", after);
    check (juce::approximatelyEqual (after, expected),
           "a value-less PARAM restores the default, not the range minimum");
    presetFile.deleteFile();
}

// ---------------------------------------------------------------------------
// 16. FIRST ACTIVATION: a session the host restored BEFORE it activated the
//     plug-in must be audible correctly from the very first sample.
//
//     This is the ordinary VST3/AU order -- the host creates the instance,
//     hands it the project's state, and only then calls setActive /
//     prepareToPlay -- and it is the one order no other test drove. Until the
//     round-2 fix, prepareToPlay called engine.prepare() BEFORE pushing the
//     restored parameters in, so the engine settled on EngineParameters'
//     defaults and then RAMPED to the restored values over the first ~20 ms.
//     A restored Mix=0 session therefore opened WET for that moment, which is
//     the DSP_POLICY invariant-7 null failing exactly where nobody was looking.
//
//     WHAT MAKES IT UNIVERSAL, and what this test actually measures: the
//     engine's own struct defaults and the snapshot the wrapper builds from the
//     parameters disagree on a DISCRETE field even for a brand-new instance.
//     `dimMode` is the always-active one: the APVTS choice defaults to index 1
//     and ParamPointers::toEngine maps choice->mode as index + 1, so the very
//     first snapshot says 2 while EngineParameters::dimMode is 1. (In ADVANCED
//     sessions `mbEnable` adds a second: APVTS default true, struct default
//     false. The rest of the Advanced block is gated off in Simple mode and
//     keeps the struct defaults by design.) A discrete difference is exactly
//     what setParameters' click-free switch machine reacts to: an ordinary
//     duck -- ~6 ms fade to SILENCE, adopt at the bottom, ~28 ms fade back in.
//     That fired on EVERY activation, restored session or not, so a plug-in
//     newly inserted on a playing track dipped for ~34 ms before it settled.
//
//     The assertion is therefore about level, not bit-identity (the default
//     chain is phase-shifted by the multiband allpasses, so it is deliberately
//     NOT a sample-for-sample null): a steady sine in must come out at steady
//     level from the start. A duck shows up as a near-zero block, which no
//     tolerance can hide. Driven through the REAL wrapper, since the defect
//     lived in the wrapper's call order.
static void testFirstActivationUsesRestoredState()
{
    std::printf ("State test 16: activation does not duck (engine primed with the live state)\n");

    constexpr int    kBlock = 256;
    constexpr double kSr    = 48000.0;

    // Per-block RMS of a steady 1 kHz sine driven through a freshly activated
    // processor. Block 0 is reported but excluded from the verdict: the
    // crossover filters legitimately charge from rest there, which is a
    // startup transient of the filters, not a duck of the output stage.
    auto earlyLevelRatio = [&] (AnamorphAudioProcessor& proc, const char* label)
    {
        proc.prepareToPlay (kSr, kBlock);
        juce::MidiBuffer midi;
        double phase = 0.0;
        const double inc = 2.0 * 3.14159265358979 * 1000.0 / kSr;

        std::vector<double> rms;
        for (int nb = 0; nb < 40; ++nb)
        {
            juce::AudioBuffer<float> buf (2, kBlock);
            for (int i = 0; i < kBlock; ++i)
            {
                const float s = 0.5f * (float) std::sin (phase); phase += inc;
                buf.setSample (0, i, s);
                buf.setSample (1, i, s);
            }
            proc.processBlock (buf, midi);
            double sq = 0.0;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < kBlock; ++i)
                    sq += (double) buf.getSample (ch, i) * (double) buf.getSample (ch, i);
            rms.push_back (std::sqrt (sq / (2.0 * kBlock)));
        }

        const double settled = rms.back();
        double minEarly = rms[1];
        for (int nb = 1; nb < 12; ++nb) minEarly = juce::jmin (minEarly, rms[(size_t) nb]);
        std::printf ("  %s: block0 %.4f, min(blocks 1-11) %.4f, settled %.4f\n",
                     label, rms[0], minEarly, settled);
        const double norm = settled > 1.0e-6 ? settled : 1.0;
        return std::pair<double, double> { rms[0] / norm, minEarly / norm };
    };

    // (a) A plug-in newly inserted on a playing track -- no restore at all.
    {
        AnamorphAudioProcessor fresh;
        const auto [block0, minEarly] = earlyLevelRatio (fresh, "fresh instance ");
        juce::ignoreUnused (block0); // block 0 charges the default multiband filters
        check (minEarly > 0.85, "a newly activated instance does not duck its first ~60 ms");
    }

    // (b) The reviewed case: the host restores a NON-DEFAULT session, then
    //     activates. The widening amount stays at 0 ON PURPOSE -- an engaged
    //     delay-based algorithm fills its lines from silence over the first
    //     tens of ms, which is a real and correct startup transient of the
    //     effect and would mask the defect being measured. What is restored
    //     instead is one DISCRETE field the engine's defaults disagree with
    //     (algorithm: Dim-D, not Velvet -- so the switch machine would duck)
    //     and one CONTINUOUS field with an unmistakable level signature
    //     (Output Gain -12 dB -- so an un-primed smoother would open ~4x too
    //     loud and ramp down). One scenario, both halves of the defect.
    {
        juce::MemoryBlock project;
        {
            AnamorphAudioProcessor authoring;
            authoring.prepareToPlay (kSr, kBlock);
            // ADVANCED on: ParamPointers::toEngine gates the whole input /
            // multiband / output block behind it, so in Simple mode the engine
            // would keep its struct defaults for Output Gain and the rest and
            // this scenario would assert nothing.
            setRaw (authoring, pid::advancedMode, 1.0f);
            setRaw (authoring, pid::mbEnable,     0.0f);  // off; APVTS default is on
            setRaw (authoring, pid::algorithm,    1.0f);  // Dim-D, not the default Velvet
            setRaw (authoring, pid::outputGain,   0.25f); // -12 dB of the -24..+24 range
            authoring.getStateInformation (project);
        }

        AnamorphAudioProcessor proc;
        proc.setStateInformation (project.getData(), (int) project.getSize()); // restore FIRST
        // Reported so a failure separates "the restore did not arrive" from
        // "the restore arrived and activation did not honour it".
        std::printf ("  restored raw: outputGain %.4f algorithm %.4f mbEnable %.4f\n",
                     rawOf (proc, pid::outputGain), rawOf (proc, pid::algorithm),
                     rawOf (proc, pid::mbEnable));
        const auto [block0, minEarly] = earlyLevelRatio (proc, "restored session");
        check (minEarly > 0.85, "a restored session is at level from the first blocks, not ducked in");
        // The continuous half: with multiband off and the wet path parked there is
        // no filter charge and no delay fill, so the VERY first block must already
        // carry the restored -12 dB Output Gain. An un-primed smoother starts at
        // unity and ramps down, which reads here as a block0 roughly 4x settled.
        check (block0 > 0.9 && block0 < 1.1,
               "the first block already carries the restored Output Gain (no ramp from unity)");
        check (proc.getLatencySamples() == 0, "restored session reports zero latency (oversampling off)");
    }
}

// ---------------------------------------------------------------------------
// RISK-007 probe: are host state calls on a NON-MAIN thread actually racing the
// message thread's reads?  (engineering-review R2-2, evidence for decision D-2)
//
// NOT part of the suite and never run by it: this deliberately drives the exact
// interaction the risk describes, so if the race is real the probe's own
// execution is undefined behaviour. It exists to be run UNDER ThreadSanitizer,
// where the question has a mechanical answer, and only when asked for by name:
//     AnamorphStateTests --state-thread-probe
//
// Thread A models a host that calls setState/getState off its UI thread (the
// macOS AU autosave shape -- no AU spec forbids it, and the JUCE AU wrapper
// passes both straight through on the caller's thread). The MAIN thread models
// what the open editor's 24 Hz timerCallback does every tick: refreshPresetDisplay
// reads PresetManager::currentName()/isDirty(), then pollUndoCoalesce() runs and
// canUndo()/canRedo() read the A/B undo stacks (src/PluginEditor.cpp
// timerCallback + refreshPresetDisplay).
//
// A TSan report names the racing pair; SILENCE is equally informative, and is
// the only thing that would refute the finding.
static int runStateThreadProbe()
{
    std::printf ("RISK-007 probe: host state thread vs message-thread reads\n");
    std::printf ("  (run under ThreadSanitizer; a report here is the finding, silence refutes it)\n");

    AnamorphAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);

    // Give the probe real metadata to race over: a named preset, then an edit so
    // the dirty baseline and the undo stack are both populated.
    proc.getPresets().load (1);
    setRaw (proc, "width", 0.42f);
    proc.pollUndoCoalesce();

    juce::MemoryBlock blob;
    proc.getStateInformation (blob);

    constexpr int kIterations = 400;
    std::atomic<bool> go { false };

    std::thread hostStateThread ([&]
    {
        while (! go.load (std::memory_order_acquire)) { /* spin to widen the window */ }
        for (int i = 0; i < kIterations; ++i)
        {
            proc.setStateInformation (blob.getData(), (int) blob.getSize());
            juce::MemoryBlock out;
            proc.getStateInformation (out);
        }
    });

    go.store (true, std::memory_order_release);
    for (int i = 0; i < kIterations; ++i)
    {
        // Exactly the editor tick's reads, in the editor's order.
        auto& pm = proc.getPresets();
        const juce::String liveName = pm.currentName();
        const bool liveDirty = pm.isDirty();
        proc.pollUndoCoalesce();
        const bool u = proc.canUndo(), r = proc.canRedo();
        const int  slot = proc.abActiveSlot();
        // Consume the reads so no compiler can delete them.
        if (liveName.isEmpty() && liveDirty && u && r && slot < 0)
            std::printf ("  (unreachable, keeps the reads live)\n");
    }

    hostStateThread.join();
    std::printf ("  probe finished: %d state-call iterations against %d editor ticks\n",
                 kIterations, kIterations);
    std::printf ("  verdict comes from the sanitizer, not from this exit code\n");
    return 0;
}

// ---------------------------------------------------------------------------
// ER-STATE-07 probe: does the reported latency follow a restore that moved
// Drive or Algorithm through the ABSENT-node (default) path?
//
// MEASURES, does not assert. The reported latency is an
// ARCHITECTURE_REVIEW_GATE hard-stop category, so this probe exists to put a
// number on the finding for the maintainer decision -- it must not encode
// either the current behaviour or a proposed one as an expectation.
//
// The mechanism: for a PARAM node PRESENT in the blob, apvts.replaceState()
// drives setValueNotifyingHost, whose listener chain reaches
// AnamorphAudioProcessor::parameterChanged -> updateLatency(). For an ABSENT
// node, reassertParameters' default branch applies the value with setValue()
// plus a direct atomic store, neither of which notifies -- so nothing
// re-reports. predictLatency short-circuits to 0 unless oversampling is on, so
// the probe turns oversampling on first; it also holds oversampling EQUAL
// across the two states, because a changed oversampling fires InternalState's
// own callback and would mask the gap.
//
// SINCE ADR-0034 THIS PROBE MEASURES A CONSTANT, and it is kept for the record
// rather than repaired. The reported latency is now a function of the
// Oversampling SELECTION alone, so a drive move cannot change it: every column
// below reads the same number, and the "does setValue re-report?" question the
// probe was written to answer is no longer answerable through drive -- there is
// nothing left for a missed re-report to get wrong. Read its output as history.
static int runLatencyRestoreProbe()
{
    std::printf ("ER-STATE-07 probe: reported latency after an absent-node restore\n");

    // Step 0 -- isolate the MECHANISM, with no state machinery in the way: does a
    // bare setValue() (what reassertParameters' notifyHost=false path uses) reach
    // the apvts.addParameterListener chain that ends in updateLatency()? A restore
    // has too many other things in it to answer that on its own.
    {
        AnamorphAudioProcessor iso;
        iso.getInternal().oversampleValue().setValue (2); // 2x -- predictLatency is 0 when OS is Off
        iso.prepareToPlay (48000.0, 256);
        setRaw (iso, pid::drive, 0.6f);
        const int withDrive = iso.getLatencySamples();
        if (auto* rp = iso.getAPVTS().getParameter (pid::drive))
            rp->setValue (0.0f); // NOT setValueNotifyingHost
        std::printf ("  step 0: latency %d with drive, %d after a bare setValue(0) -> setValue %s\n",
                     withDrive, iso.getLatencySamples(),
                     iso.getLatencySamples() == withDrive ? "does NOT re-report"
                                                          : "DOES re-report");
    }

    // Step 0b -- what does apvts.replaceState() ALONE do to a parameter whose PARAM
    // node is absent from the new tree? This is ER-STATE-01's premise, and nothing
    // short of running it answers it: reassertParameters' default branch would
    // otherwise hide the answer behind its own (identical) correction.
    {
        AnamorphAudioProcessor iso;
        iso.getInternal().oversampleValue().setValue (2);
        iso.prepareToPlay (48000.0, 256);
        setRaw (iso, pid::drive, 0.6f);
        const int latencyWithDrive = iso.getLatencySamples();

        auto stripped = iso.getAPVTS().copyState();
        stripped.removeChild (stripped.getChildWithProperty ("id", pid::drive), nullptr);
        iso.getAPVTS().replaceState (stripped); // NO reassertParameters anywhere near this

        std::printf ("  step 0b: replaceState with no drive node -> drive raw %.3f (default %.3f),"
                     " latency %d -> %d\n",
                     rawOf (iso, pid::drive),
                     iso.getAPVTS().getParameter (pid::drive)->getDefaultValue(),
                     latencyWithDrive, iso.getLatencySamples());
    }

    // Author a session at Drive > 0 with oversampling ON, then delete the drive
    // PARAM node -- the partial-blob shape reassertParameters' default branch exists for.
    juce::MemoryBlock partial;
    {
        AnamorphAudioProcessor authoring;
        authoring.getInternal().oversampleValue().setValue (2); // 1-based ComboBox id: 2x
        authoring.prepareToPlay (48000.0, 256);
        setRaw (authoring, pid::drive, 0.6f);

        juce::MemoryBlock whole;
        authoring.getStateInformation (whole);
        auto xml = BlobCodec::unwrap (whole);
        if (xml == nullptr) { std::printf ("  authored session did not decode\n"); return 1; }

        bool removedOne = false;
        if (auto* paramsXml = xml->getChildByName ("ANAMORPH"))
            if (auto* driveNode = paramsXml->getChildByAttribute ("id", pid::drive))
            {
                paramsXml->removeChildElement (driveNode, true);
                removedOne = true;
            }
        if (! removedOne) { std::printf ("  no drive PARAM node to remove\n"); return 1; }
        partial = BlobCodec::wrap (*xml);
        std::printf ("  authored: drive raw %.3f, oversampling 2x, drive PARAM node removed\n",
                     rawOf (authoring, pid::drive));
    }

    AnamorphAudioProcessor live;
    live.getInternal().oversampleValue().setValue (2); // SAME as the blob: no callback to mask the gap
    live.prepareToPlay (48000.0, 256);
    setRaw (live, pid::drive, 0.6f);
    const int before = live.getLatencySamples();

    live.setStateInformation (partial.getData(), (int) partial.getSize());

    const int reported = live.getLatencySamples();
    // updateLatency() is exactly what prepareToPlay ends with, so a re-prepare
    // yields the number the restored state is entitled to, through public API only.
    live.prepareToPlay (48000.0, 256);
    const int predicted = live.getLatencySamples();
    std::printf ("  drive raw after restore: %.3f (default branch applied)\n", rawOf (live, pid::drive));
    std::printf ("  latency before restore: %d\n", before);
    std::printf ("  latency reported to the host after restore: %d\n", reported);
    std::printf ("  latency the restored state actually predicts: %d\n", predicted);
    std::printf ("  %s\n", reported == predicted
                               ? "scenario A: the report followed the restore"
                               : "scenario A: the host is told a latency the restored state does not have");

    // Scenario B -- the same restore, but on an instance whose InternalState has
    // ALREADY been through one restore. Step 0 showed the parameter write itself
    // cannot re-report, so anything that corrected scenario A came from elsewhere in
    // setStateInformation: InternalState's oversample callback. On a FIRST restore
    // the live tree holds an int and the blob a round-tripped string, so
    // ValueTree::setProperty sees a difference and fires the callback even though the
    // oversampling did not change. Restoring twice removes that type mismatch, which
    // is also the ordinary case -- a host reloading project after project.
    AnamorphAudioProcessor twice;
    twice.getInternal().oversampleValue().setValue (2);
    twice.prepareToPlay (48000.0, 256);
    twice.setStateInformation (partial.getData(), (int) partial.getSize()); // settle the property TYPES
    setRaw (twice, pid::drive, 0.6f);                                       // back to a latency-bearing state
    const int beforeB = twice.getLatencySamples();
    twice.setStateInformation (partial.getData(), (int) partial.getSize());
    const int reportedB = twice.getLatencySamples();
    twice.prepareToPlay (48000.0, 256);
    const int predictedB = twice.getLatencySamples();
    std::printf ("  scenario B (second restore): before %d, reported %d, actually predicts %d\n",
                 beforeB, reportedB, predictedB);
    std::printf ("  %s\n", reportedB == predictedB
                               ? "REFUTED: the report follows the restore on both paths"
                               : "CONFIRMED: the host is told a latency the restored state does not have");
    std::printf ("  a re-prepare corrects it; this is the restore-into-a-live-instance case\n");
    return 0;
}

// ---------------------------------------------------------------------------
// 19. A malformed serialized value means the parameter DEFAULT -- on BOTH paths.
//
//     Round 2 closed two special cases of this (an absent node, and `value="nan"`
//     on an UNSKEWED range). Round 3 measured the rest of the space and found the
//     guard was in the wrong place: it tested the value AFTER
//     RangedAudioParameter::convertTo0to1, and that conversion CLAMPS, so every
//     infinity arrived at std::isfinite already laundered into a finite range
//     ENDPOINT. Measured on the pre-fix build, through the real loaders:
//
//        value="inf" / "1e39" / "1e400"  -> normalised 1.0  (range MAXIMUM)
//        value="-inf" / "-1e400"         -> normalised 0.0  (range MINIMUM)
//        value="abc" / "" / "0x10"       -> normalised 0.0  (range MINIMUM)
//        value="nan" on a SKEWED range   -> normalised 1.0  (range MAXIMUM)
//
//     For Width (0..2, default 1.0) the first row is a hard-wide image and the
//     third a mono collapse, from a file the user cannot see is wrong. The guard
//     now runs on the INPUT (anamorph::SerializedNumber.h) and both restore paths
//     share the predicate, so a malformed value cannot mean one thing in a preset
//     and another in a session.
static void testMalformedValuesRestoreDefaults()
{
    std::printf ("State test 19: a malformed serialized value restores the default, both paths\n");

    const char* poisons[] = { "abc", "", "0x10", "nan", "inf", "-inf", "1e39", "1e400", "-1e400" };
    // Chosen to span the shapes that behaved DIFFERENTLY before the fix: a linear
    // range, a skewed one (where NaN reached the maximum), and the two custom
    // RangedAudioParameter subclasses.
    const char* ids[] = { pid::width, pid::monoMakerFreq, pid::chorusRate, pid::algorithm };

    for (const char* id : ids)
        for (const char* poison : poisons)
        {
            AnamorphAudioProcessor proc;
            proc.prepareToPlay (48000.0, 256);
            auto* rp = proc.getAPVTS().getParameter (id);
            if (rp == nullptr) { check (false, "probe parameter exists"); continue; }
            const float def = rp->getDefaultValue();

            // Move it OFF the default first: "landed on the default" must not be
            // confusable with "was never touched" -- the vacuity that hid ER-STATE-08.
            rp->setValueNotifyingHost (def > 0.5f ? 0.1f : 0.9f);

            const auto f = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("anamorph_malformed_probe.anamorph");
            f.deleteFile();
            auto tree = juce::ValueTree ("ANAMORPH");
            auto node = juce::ValueTree ("PARAM");
            node.setProperty ("id", id, nullptr);
            node.setProperty ("value", juce::String (poison), nullptr);
            tree.appendChild (node, nullptr);
            if (auto xml = tree.createXml()) xml->writeTo (f);
            const bool loaded = proc.getPresets().loadFile (f);
            f.deleteFile();

            check (loaded, "the malformed preset file loads (it is well-formed XML)");
            check (juce::approximatelyEqual (rp->getValue(), def),
                   "preset path: a malformed value restores the parameter default");
        }

    // The SESSION path must give the same answers, or the two drift apart again.
    for (const char* poison : { "abc", "", "0x10", "nan", "inf", "1e400", "-1e400" })
    {
        juce::MemoryBlock poisoned;
        {
            AnamorphAudioProcessor authoring;
            authoring.prepareToPlay (48000.0, 256);
            juce::MemoryBlock clean;
            authoring.getStateInformation (clean);
            auto xml = BlobCodec::unwrap (clean);
            if (xml == nullptr) { check (false, "authored session decodes"); continue; }
            if (auto* params = xml->getChildByName ("ANAMORPH"))
                if (auto* w = params->getChildByAttribute ("id", pid::width))
                {
                    w->setAttribute ("value", poison);
                    w->removeAttribute ("raw");     // force the @value branch
                }
            poisoned = BlobCodec::wrap (*xml);
        }
        AnamorphAudioProcessor proc;
        proc.prepareToPlay (48000.0, 256);
        setRaw (proc, pid::width, 0.9f);            // off the default first
        proc.setStateInformation (poisoned.getData(), (int) poisoned.getSize());
        check (juce::approximatelyEqual (rawOf (proc, pid::width),
                                         proc.getAPVTS().getParameter (pid::width)->getDefaultValue()),
               "session path: a malformed value restores the parameter default");
    }

    // The `raw` branch launders infinity through juce::jlimit the same way, so it
    // needs its own coverage -- it is the branch a current-schema session takes.
    {
        juce::MemoryBlock poisoned;
        {
            AnamorphAudioProcessor authoring;
            authoring.prepareToPlay (48000.0, 256);
            juce::MemoryBlock clean;
            authoring.getStateInformation (clean);
            auto xml = BlobCodec::unwrap (clean);
            if (xml != nullptr)
                if (auto* params = xml->getChildByName ("ANAMORPH"))
                    if (auto* w = params->getChildByAttribute ("id", pid::width))
                        w->setAttribute ("raw", "inf");
            poisoned = BlobCodec::wrap (*xml);
        }
        AnamorphAudioProcessor proc;
        proc.prepareToPlay (48000.0, 256);
        setRaw (proc, pid::width, 0.9f);
        proc.setStateInformation (poisoned.getData(), (int) poisoned.getSize());
        std::printf ("  width after restoring raw=\"inf\": %f\n", rawOf (proc, pid::width));
        check (rawOf (proc, pid::width) < 0.99f,
               "a non-finite `raw` does not pin the parameter to its range maximum");
    }
}

// ---------------------------------------------------------------------------
// 20. A repaired parameter must reach the SAVED state, not just the live one.
//
//     The repair in reassertParameters' notifyHost=false branch writes the
//     parameter (setValue) and the DSP atomic directly, and deliberately notifies
//     nobody -- a parameter-change callback during the host's own state load can
//     read as an automation write in some DAWs. But JUCE flushes a parameter into
//     the ValueTree only when the adapter's `needsUpdate` is set, and that is set
//     by parameterValueChanged, which setValue() does not fire. So the repair was
//     invisible to serialization: measured before the fix, a session carrying
//     value="nan" restored correctly and then SAVED value="nan" straight back out.
//
//     This build reloads that file correctly anyway, because `raw` is re-stamped
//     on every save and is preferred on restore. The defect is in what the FILE
//     says -- which is the durable artefact, and what a build with no `raw` path
//     reads as NaN.
static void testRepairReachesSavedState()
{
    std::printf ("State test 20: a repaired parameter reaches the saved state (not just the live one)\n");

    juce::MemoryBlock poisoned;
    {
        AnamorphAudioProcessor authoring;
        authoring.prepareToPlay (48000.0, 256);
        juce::MemoryBlock clean;
        authoring.getStateInformation (clean);
        auto xml = BlobCodec::unwrap (clean);
        check (xml != nullptr, "authored session decodes for poisoning");
        if (xml == nullptr) return;
        bool poisonedOne = false;
        if (auto* params = xml->getChildByName ("ANAMORPH"))
            if (auto* w = params->getChildByAttribute ("id", pid::width))
            {
                w->setAttribute ("value", "nan");
                w->removeAttribute ("raw");
                poisonedOne = true;
            }
        check (poisonedOne, "the session carries a width PARAM to poison");
        poisoned = BlobCodec::wrap (*xml);
    }

    AnamorphAudioProcessor proc;
    proc.prepareToPlay (48000.0, 256);
    proc.setStateInformation (poisoned.getData(), (int) poisoned.getSize());

    auto* rp = proc.getAPVTS().getParameter (pid::width);
    check (juce::approximatelyEqual (rp->getValue(), rp->getDefaultValue()),
           "the corrupt value was repaired in the live parameter");

    // The live tree, which is what copyState() serialises from.
    auto live = proc.getAPVTS().state.getChildWithProperty ("id", pid::width);
    check (live.isValid(), "the live APVTS still carries a width node");
    const double liveValue = live.getProperty ("value").toString().getDoubleValue();
    std::printf ("  live APVTS @value after repair = \"%s\"\n",
                 live.getProperty ("value").toString().toRawUTF8());
    check (std::isfinite (liveValue), "the repair reached the live APVTS tree");

    // And the saved artefact.
    juce::MemoryBlock saved;
    proc.getStateInformation (saved);
    auto savedXml = BlobCodec::unwrap (saved);
    check (savedXml != nullptr, "the re-saved session decodes");
    if (savedXml == nullptr) return;
    auto* w = savedXml->getChildByName ("ANAMORPH") != nullptr
                  ? savedXml->getChildByName ("ANAMORPH")->getChildByAttribute ("id", pid::width)
                  : nullptr;
    check (w != nullptr, "the re-saved session carries a width node");
    if (w == nullptr) return;
    std::printf ("  saved @value = \"%s\"  @raw = \"%s\"\n",
                 w->getStringAttribute ("value").toRawUTF8(),
                 w->hasAttribute ("raw") ? w->getStringAttribute ("raw").toRawUTF8() : "(absent)");
    check (std::isfinite (w->getStringAttribute ("value").getDoubleValue()),
           "the corruption does not survive into the next saved state");
}

// ---------------------------------------------------------------------------
// 21. KI-028: a value-box press whose release is never delivered must not hold a
//     host gesture open for the rest of the session.
//
//     ValueBox opens a juce::Slider::ScopedDragNotification on mouseDown and
//     closes it on mouseUp, so a drag is one undo step and one host touch/latch
//     span. Round 3 measured which abandonment paths actually exist: of six
//     candidates, five are closed by JUCE itself (it synthesises the release on
//     the next event that reaches the peer, or the component's own destruction
//     fires the RAII close). The one that survives is a release the OS delivers
//     to no JUCE peer at all -- let go over the host window or the desktop.
//
//     Measured before the fix, with a positive control proving the yardstick can
//     fail: an unreleased press left canUndo() FALSE after a complete, balanced,
//     unrelated edit. Also measured, and REFUTED: destroying the editor mid-press
//     does NOT leak, despite sliderAtts being declared after the Knobs and so
//     destructing first -- the RAII close still lands.
//
//     SCOPE: the editor predicate that triggers the sweep is inert on macOS
//     (KI-013), so this closes the Linux and Windows halves and narrows KI-028 to
//     a macOS residual. The sweep is tested directly rather than through that
//     predicate, because synthesising OS-level button state headlessly is not
//     possible -- and the predicate is pre-existing, shipped, and separately
//     recorded.
static void testAbandonedValueBoxGestureIsReclaimed()
{
    std::printf ("State test 21: an abandoned value-box press does not block undo (KI-028)\n");
#if ! (JUCE_LINUX || JUCE_BSD)
    std::printf ("  SKIPPED off Linux -- headless editor construction is unverified there (KI-007)\n");
#else
    auto editYieldsUndoStep = [] (AnamorphAudioProcessor& proc, const char* id, float norm)
    {
        auto* rp = proc.getAPVTS().getParameter (id);
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (norm);
        rp->endChangeGesture();
        proc.pollUndoCoalesce();
        return proc.canUndo();
    };

    // THE YARDSTICK MUST BE ABLE TO FAIL, or every assertion below is vacuous.
    {
        AnamorphAudioProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        auto* held = proc.getAPVTS().getParameter (pid::width);
        held->beginChangeGesture();                   // opened, never closed
        check (! editYieldsUndoStep (proc, pid::mix, 0.31f),
               "positive control: an open gesture really does block the undo commit");
        held->endChangeGesture();
    }

    AnamorphAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    auto* raw = proc.createEditor();
    auto* ed = dynamic_cast<AnamorphAudioProcessorEditor*> (raw);
    check (ed != nullptr, "editor constructs for the gesture probe");
    if (ed == nullptr) { delete raw; return; }

    juce::Slider* slider = nullptr;
    juce::Label*  box    = nullptr;
    std::function<void (juce::Component*)> walk = [&] (juce::Component* c)
    {
        if (slider != nullptr) return;
        for (int i = 0; i < c->getNumChildComponents(); ++i)
        {
            auto* kid = c->getChildComponent (i);
            if (auto* sl = dynamic_cast<juce::Slider*> (kid))
                for (int j = 0; j < sl->getNumChildComponents(); ++j)
                    if (auto* lb = dynamic_cast<juce::Label*> (sl->getChildComponent (j)))
                    { slider = sl; box = lb; break; }
            if (slider == nullptr) walk (kid);
            else return;
        }
    };
    walk (ed);
    check (slider != nullptr && box != nullptr, "a knob with a value box exists to press");
    if (slider == nullptr || box == nullptr) { proc.editorBeingDeleted (ed); delete ed; return; }

    const juce::MouseEvent me (juce::Desktop::getInstance().getMainMouseSource(),
                               { 4.0f, 4.0f }, juce::ModifierKeys::leftButtonModifier,
                               1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                               box, box, juce::Time::getCurrentTime(),
                               { 4.0f, 4.0f }, juce::Time::getCurrentTime(), 1, false);

    // (1) The press really does open a gesture -- checked, not assumed.
    box->mouseDown (me);
    check ((bool) slider->getProperties().getWithDefault ("dragging", false),
           "the press registers on the knob");
    check (! editYieldsUndoStep (proc, pid::mix, 0.31f),
           "an un-released press holds the gesture open (this is KI-028)");

    // (2) The reconcile reclaims it.
    ed->abortAbandonedDragGestures();
    check (! (bool) slider->getProperties().getWithDefault ("dragging", true),
           "the reconcile clears the press feedback");
    check (editYieldsUndoStep (proc, pid::mix, 0.44f),
           "...and undo works again after the abandoned gesture is reclaimed");

    // (3) Idempotent: the reconcile runs every tick, so a second call must be inert.
    ed->abortAbandonedDragGestures();
    check (editYieldsUndoStep (proc, pid::mix, 0.55f),
           "the sweep is idempotent -- a second pass does not close a gesture twice");

    // (4) A NORMAL drag is unchanged: press, drag, release, one undo step.
    {
        box->mouseDown (me);
        const juce::MouseEvent up (juce::Desktop::getInstance().getMainMouseSource(),
                                   { 4.0f, 9.0f }, juce::ModifierKeys(),
                                   1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                   box, box, juce::Time::getCurrentTime(),
                                   { 4.0f, 4.0f }, juce::Time::getCurrentTime(), 1, true);
        // Label::mouseUp is protected; Component::mouseUp is public and virtual, so
        // the call goes through the base pointer and still dispatches to ValueBox.
        static_cast<juce::Component*> (box)->mouseUp (up);
        check (! (bool) slider->getProperties().getWithDefault ("dragging", true),
               "a normal release still clears the press feedback");
        check (editYieldsUndoStep (proc, pid::mix, 0.66f),
               "a normal press/release leaves undo working (no regression)");
    }

    // (5) Editor teardown mid-press: measured NOT to leak, recorded so the
    //     refutation is guarded rather than remembered.
    {
        AnamorphAudioProcessor proc2;
        proc2.prepareToPlay (48000.0, 512);
        auto* raw2 = proc2.createEditor();
        if (auto* ed2 = dynamic_cast<AnamorphAudioProcessorEditor*> (raw2))
        {
            juce::Slider* sl2 = nullptr; juce::Label* bx2 = nullptr;
            std::function<void (juce::Component*)> walk2 = [&] (juce::Component* c)
            {
                if (sl2 != nullptr) return;
                for (int i = 0; i < c->getNumChildComponents(); ++i)
                {
                    auto* kid = c->getChildComponent (i);
                    if (auto* sl = dynamic_cast<juce::Slider*> (kid))
                        for (int j = 0; j < sl->getNumChildComponents(); ++j)
                            if (auto* lb = dynamic_cast<juce::Label*> (sl->getChildComponent (j)))
                            { sl2 = sl; bx2 = lb; break; }
                    if (sl2 == nullptr) walk2 (kid); else return;
                }
            };
            walk2 (ed2);
            if (bx2 != nullptr)
            {
                const juce::MouseEvent me2 (juce::Desktop::getInstance().getMainMouseSource(),
                                            { 4.0f, 4.0f }, juce::ModifierKeys::leftButtonModifier,
                                            1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                            bx2, bx2, juce::Time::getCurrentTime(),
                                            { 4.0f, 4.0f }, juce::Time::getCurrentTime(), 1, false);
                bx2->mouseDown (me2);
            }
            proc2.editorBeingDeleted (ed2);
            delete ed2;
            check (editYieldsUndoStep (proc2, pid::mix, 0.31f),
                   "destroying the editor mid-press closes the gesture (the RAII close lands)");
        }
        else delete raw2;
    }

    proc.editorBeingDeleted (ed);
    delete ed;
#endif
}

// ---------------------------------------------------------------------------
// Round-3 diagnostic probe (opt-in, asserts NOTHING).
//
//     Covers priorities 2-5 of the round-3 brief in one pass. It PRINTS measured
//     values rather than asserting them, because at the time it was written all
//     four were HYPOTHESES, and the round's rule is that classification comes from
//     what a probe prints -- never from what an investigation predicted. Anything
//     it confirms is then promoted to a real assertion in the suite proper.
static int runPresetSemanticsProbe()
{
    std::printf ("Round-3 probe: preset / restore value semantics\n\n");

    // -- P2/P4 step 1: what JUCE's own parser actually does with each poison. ---
    // Read, do not recall: juce::ValueTree::fromXml stores XML attributes as
    // String vars, so this is exactly the conversion applySoundTree performs.
    std::printf ("A. juce::var conversion of raw attribute text\n");
    const char* texts[] = { "abc", "12abc", "", "  1.5  ", "1e999", "0x10",
                            "0", "0.0", "-0", "0e0", ".5", "+1",
                            "nan", "inf", "-inf", "1e39", "1e400", "-1e400", "1e38" };
    for (const char* t : texts)
    {
        const juce::var v { juce::String (t) };
        const double d = (double) v;
        const float f = (float) d;
        std::printf ("   %-8s isString=%d  (double)=%-14g (float)=%-14g finite(f)=%d\n",
                     (juce::String ("\"") + t + "\"").toRawUTF8(),
                     (int) v.isString(), d, (double) f, (int) std::isfinite (f));
    }

    // -- P2/P4 step 2: what the real PRESET path does with each poison. --------
    // Driven end to end through PresetManager::loadFile on a real file, because
    // applySoundTree is private and a hand-rolled re-derivation would prove
    // nothing about the shipped path.
    std::printf ("\nB. preset path (real loadFile), per parameter and poison\n");
    struct Target { const char* id; const char* label; };
    const Target targets[] = {
        { pid::width,         "width (0..2, def 1, linear)" },
        { pid::monoMakerFreq, "monoMakerFreq (skewed)" },
        { pid::chorusRate,    "chorusRate (skewed 0.4)" },
        { pid::algorithm,     "algorithm (RawChoice)" },
        { pid::monoMakerOn,   "monoMakerOn (RawBool)" },
    };
    const char* poisons[] = { "abc", "", "0x10", "nan", "inf", "-inf", "1e39", "1e400", "-1e400" };

    for (const auto& tg : targets)
    {
        AnamorphAudioProcessor probeProc;
        auto* rp = probeProc.getAPVTS().getParameter (tg.id);
        if (rp == nullptr) { std::printf ("   %s: NO SUCH PARAMETER\n", tg.id); continue; }
        const float def = rp->getDefaultValue();
        std::printf ("   %-30s default(norm) = %.6f\n", tg.label, def);

        for (const char* poison : poisons)
        {
            AnamorphAudioProcessor proc;
            proc.prepareToPlay (48000.0, 256);
            auto* rp2 = proc.getAPVTS().getParameter (tg.id);
            // Move it OFF the default first, so "landed on the default" cannot be
            // confused with "was never touched" -- the vacuity that hid ER-STATE-08.
            rp2->setValueNotifyingHost (def > 0.5f ? 0.1f : 0.9f);
            const float before = rp2->getValue();

            const auto f = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("anamorph_r3_probe.anamorph");
            f.deleteFile();
            auto tree = juce::ValueTree ("ANAMORPH");
            auto node = juce::ValueTree ("PARAM");
            node.setProperty ("id", tg.id, nullptr);
            node.setProperty ("value", juce::String (poison), nullptr);
            tree.appendChild (node, nullptr);
            bool wrote = false;
            if (auto xml = tree.createXml()) wrote = xml->writeTo (f);
            const bool loaded = wrote && proc.getPresets().loadFile (f);
            const float after = rp2->getValue();
            f.deleteFile();

            const char* verdict = ! loaded ? "LOAD FAILED"
                                : (std::abs (after - def) < 1.0e-6f) ? "-> default (guard held)"
                                : (after < 1.0e-6f)                  ? "-> RANGE MINIMUM"
                                : (after > 1.0f - 1.0e-6f)           ? "-> RANGE MAXIMUM"
                                                                     : "-> other";
            std::printf ("      value=%-8s before %.4f  after %.6f   %s\n",
                         (juce::String ("\"") + poison + "\"").toRawUTF8(),
                         before, after, verdict);
        }
    }

    // -- P2 step 3: the SESSION path, same poison, for comparison. -------------
    // The two paths must not drift: whatever the answer is, it should be the same.
    std::printf ("\nC. session path (real setStateInformation), width\n");
    for (const char* poison : { "abc", "", "0x10", "nan", "inf", "1e400" })
    {
        juce::MemoryBlock poisoned;
        {
            AnamorphAudioProcessor authoring;
            authoring.prepareToPlay (48000.0, 256);
            juce::MemoryBlock clean;
            authoring.getStateInformation (clean);
            auto xml = BlobCodec::unwrap (clean);
            if (xml == nullptr) { std::printf ("   unwrap failed\n"); continue; }
            if (auto* params = xml->getChildByName ("ANAMORPH"))
                if (auto* w = params->getChildByAttribute ("id", pid::width))
                {
                    w->setAttribute ("value", poison);
                    w->removeAttribute ("raw");   // force the @value path
                }
            poisoned = BlobCodec::wrap (*xml);
        }
        AnamorphAudioProcessor proc;
        proc.prepareToPlay (48000.0, 256);
        setRaw (proc, pid::width, 0.9f);          // off the default first
        proc.setStateInformation (poisoned.getData(), (int) poisoned.getSize());
        std::printf ("      value=%-8s -> width norm %.6f (default 0.500000)\n",
                     (juce::String ("\"") + poison + "\"").toRawUTF8(),
                     rawOf (proc, pid::width));
    }

    // -- P3: does a REPAIR reach the saved blob? ------------------------------
    // Load corrupt -> restore -> save -> inspect -> reload -> compare, exactly the
    // sequence the brief specifies.
    std::printf ("\nD. state repair serialization consistency (P3)\n");
    {
        juce::MemoryBlock poisoned;
        {
            AnamorphAudioProcessor authoring;
            authoring.prepareToPlay (48000.0, 256);
            juce::MemoryBlock clean;
            authoring.getStateInformation (clean);
            auto xml = BlobCodec::unwrap (clean);
            if (auto* params = xml->getChildByName ("ANAMORPH"))
                if (auto* w = params->getChildByAttribute ("id", pid::width))
                {
                    w->setAttribute ("value", "nan");
                    w->removeAttribute ("raw");
                }
            poisoned = BlobCodec::wrap (*xml);
        }
        AnamorphAudioProcessor proc;
        proc.prepareToPlay (48000.0, 256);
        proc.setStateInformation (poisoned.getData(), (int) poisoned.getSize());
        std::printf ("      after restore: width norm = %.6f (repaired to default 0.500000?)\n",
                     rawOf (proc, pid::width));

        // (a) the LIVE tree
        auto live = proc.getAPVTS().state.getChildWithProperty ("id", pid::width);
        std::printf ("      live APVTS node @value = \"%s\"  @raw = \"%s\"\n",
                     live.getProperty ("value").toString().toRawUTF8(),
                     live.hasProperty ("raw") ? live.getProperty ("raw").toString().toRawUTF8() : "(absent)");

        // (d) the SAVED blob
        juce::MemoryBlock saved;
        proc.getStateInformation (saved);
        if (auto xml = BlobCodec::unwrap (saved))
            if (auto* params = xml->getChildByName ("ANAMORPH"))
                if (auto* w = params->getChildByAttribute ("id", pid::width))
                    std::printf ("      SAVED node @value = \"%s\"  @raw = \"%s\"\n",
                                 w->getStringAttribute ("value").toRawUTF8(),
                                 w->hasAttribute ("raw") ? w->getStringAttribute ("raw").toRawUTF8() : "(absent)");

        // reload into a fresh instance
        AnamorphAudioProcessor fresh;
        fresh.prepareToPlay (48000.0, 256);
        fresh.setStateInformation (saved.getData(), (int) saved.getSize());
        std::printf ("      reloaded into a fresh instance: width norm = %.6f\n",
                     rawOf (fresh, pid::width));
    }

    // -- P5: ER-STATE-04.5, the id+raw-without-value shape. -------------------
    std::printf ("\nE. ER-STATE-04.5: what the next save writes for an omitted PARAM (P5)\n");
    for (bool atDefault : { true, false })
    {
        juce::MemoryBlock partial;
        {
            AnamorphAudioProcessor authoring;
            authoring.prepareToPlay (48000.0, 256);
            juce::MemoryBlock whole;
            authoring.getStateInformation (whole);
            auto xml = BlobCodec::unwrap (whole);
            if (auto* params = xml->getChildByName ("ANAMORPH"))
                if (auto* w = params->getChildByAttribute ("id", pid::width))
                    params->removeChildElement (w, true);
            partial = BlobCodec::wrap (*xml);
        }
        AnamorphAudioProcessor proc;
        proc.prepareToPlay (48000.0, 256);
        auto* rp = proc.getAPVTS().getParameter (pid::width);
        rp->setValueNotifyingHost (atDefault ? rp->getDefaultValue() : 0.9f);
        proc.setStateInformation (partial.getData(), (int) partial.getSize());

        juce::MemoryBlock saved;
        proc.getStateInformation (saved);
        std::printf ("      live was %s: ", atDefault ? "AT default   " : "OFF default  ");
        if (auto xml = BlobCodec::unwrap (saved))
            if (auto* params = xml->getChildByName ("ANAMORPH"))
            {
                auto* w = params->getChildByAttribute ("id", pid::width);
                if (w == nullptr) { std::printf ("saved node ABSENT entirely\n"); continue; }
                std::printf ("saved @value = %-10s @raw = %s\n",
                             w->hasAttribute ("value")
                                 ? (juce::String ("\"") + w->getStringAttribute ("value") + "\"").toRawUTF8()
                                 : "(ABSENT)",
                             w->hasAttribute ("raw")
                                 ? (juce::String ("\"") + w->getStringAttribute ("raw") + "\"").toRawUTF8()
                                 : "(ABSENT)");
            }
    }

    std::printf ("\nprobe complete (no assertions -- classification is the lead's)\n");
    return 0;
}

// ---------------------------------------------------------------------------
// 22. D-1 / KI-027: a latency re-report from a NON-message thread must not be
//     delivered on that thread.
//
//     Computing the number was never the problem -- predictLatency is const and
//     race-free. DELIVERING it was: setLatencySamples' notification chain takes
//     at least three CriticalSections and, on a real change, appends to a heap
//     container and write()s a pipe in the Linux wrapper. Under VST3 host
//     automation of drive/algorithm the caller is the AUDIO thread.
//
//     The approved design keeps the message thread synchronous (so every UI edit,
//     preset load and undo is instantaneous, and this test's own control leg
//     proves that) and turns anything else into a relaxed atomic request that a
//     processor-owned 20 Hz timer consumes.
//
//     What this test can and cannot show, stated plainly: it drives the real
//     listener from a real second thread and asserts the delivery is DEFERRED
//     there and lands later. It does not prove the absence of a lock inside
//     JUCE's chain -- that is what the RTSan lane and the allocation guard are
//     for. Deferral is the property this change is responsible for.
//
//     THE MOVER IS THE OVERSAMPLING SETTING, NOT DRIVE, SINCE ADR-0034. Until
//     0.9.7 this test moved Drive at 2x oversampling, because the reported
//     latency then depended on whether the oversampling wrap was ENGAGED -- which
//     is what made an ordinary knob move restart the host's graph, and what
//     ADR-0034 removed. The reported number is now a function of the Oversampling
//     SELECTION alone, so no parameter can move it and Drive is no longer an
//     instrument here. What still can move it off the message thread is a host
//     restoring session state from its own thread (RISK-007), which carries this
//     very Setting; the worker below performs exactly that restore, through the
//     real setStateInformation. Since D-2 (ADR-0036) that restore writes no
//     ValueTree off the message thread at all: it stores the engine-facing
//     oversampling atomic synchronously and raises the latency request, and the
//     Settings TREE is adopted by the message thread afterwards -- so the worker
//     leg exercises exactly the production path, and the earlier shape of this
//     leg (a juce::Value written from the worker) no longer models anything the
//     plug-in does.
static void testLatencyDeliveryIsDeferredOffMessageThread()
{
    std::printf ("State test 22: an off-thread latency change is deferred, not delivered (D-1)\n");

    AnamorphAudioProcessor proc;
    proc.prepareToPlay (48000.0, 256);

    // The Setting is written through its binding on the message thread for the
    // control leg; the off-thread leg restores a session that carries it.
    juce::Value osValue = proc.getInternal().oversampleValue();  // 1-based combo ids
    osValue.setValue (1);                                        // Off
    juce::Timer::callPendingTimersSynchronously();

    const int quiet = proc.getLatencySamples();
    std::printf ("  latency with oversampling Off: %d\n", quiet);

    // CONTROL LEG -- the message thread stays synchronous. If this regressed to
    // deferred, every UI edit would lag a timer tick and the test below would
    // still pass, so the control is what keeps it honest.
    osValue.setValue (2);                                        // 2x
    const int afterOnThread = proc.getLatencySamples();
    std::printf ("  after a message-thread change: %d (expected immediate)\n", afterOnThread);
    check (afterOnThread != quiet, "a message-thread change still reports latency SYNCHRONOUSLY");

    // ...and the non-vacuity gate for the whole test: the Setting must actually
    // move the reported latency, or "deferred" below is indistinguishable from
    // "nothing ever happens".
    const int loud = afterOnThread;
    check (loud != quiet, "the Oversampling Setting genuinely changes the reported latency");

    // THE REAL CASE: a host restore from another thread. Author the session that
    // carries 2x while the Setting is still 2x, then reset.
    juce::MemoryBlock sessionAt2x;
    proc.getStateInformation (sessionAt2x);
    osValue.setValue (1);
    juce::Timer::callPendingTimersSynchronously();
    check (proc.getLatencySamples() == quiet, "reset to the quiet latency before the off-thread leg");

    std::atomic<bool> done { false };
    std::thread automation ([&]
    {
        // What a host restoring session state off its message thread does (the
        // macOS AU autosave shape): the real restore, on THIS thread.
        proc.setStateInformation (sessionAt2x.getData(), (int) sessionAt2x.getSize());
        done.store (true, std::memory_order_release);
    });
    automation.join();
    check (done.load (std::memory_order_acquire), "the off-thread restore completed");
    check (proc.getInternal().oversampleIndex() == 1,
           "the engine-facing oversampling atomic is stored synchronously by the restoring thread (D-2)");

    const int immediatelyAfter = proc.getLatencySamples();
    std::printf ("  immediately after an OFF-thread change: %d (was %d)\n", immediatelyAfter, quiet);
    check (immediatelyAfter == quiet,
           "an off-thread change does NOT deliver the latency on that thread");

    // ...and the timer picks it up. 20 Hz => one interval is 50 ms; allow a few
    // so a loaded CI box cannot make this flaky, and MEASURE what it actually took.
    const auto startMs = juce::Time::getMillisecondCounterHiRes();
    int settled = immediatelyAfter;
    for (int i = 0; i < 40 && settled == quiet; ++i)
    {
        juce::Thread::sleep (10);
        juce::Timer::callPendingTimersSynchronously();
        settled = proc.getLatencySamples();
    }
    const auto elapsedMs = juce::Time::getMillisecondCounterHiRes() - startMs;
    std::printf ("  delivered by the timer after %.0f ms: %d\n", elapsedMs, settled);
    check (settled == loud, "the deferred latency is delivered, and delivers the CORRECT value");
    check (elapsedMs < 400.0, "delivery happens within a few timer intervals, not eventually");
}

// ---------------------------------------------------------------------------
// 23. KI-028 / KI-013 macOS residual: the abandoned-gesture sweep's trigger must
//     read the PHYSICAL button state, not JUCE's cached one.
//
//     Round 3 closed KI-028 on Linux and Windows and left a macOS residual. The
//     residual was never the sweep -- Option B's hook reaches the value box on
//     every platform. It was the TRIGGER. The editor gates the sweep on
//     "JUCE thinks a button is down AND it is physically up", and in the pinned
//     JUCE 9.0.1 the macOS realtime query refreshes only the keyboard flags
//     (juce_NSViewComponentPeer_mac.mm:302-307) and returns cached mouse buttons.
//     A release delivered to no JUCE peer never clears that cache, so the second
//     half was permanently false and the sweep could not run. That is KI-013.
//
//     This test recreates that exact condition on EVERY platform: it tells JUCE a
//     button is held while none physically is, then requires the physical query to
//     disagree with the cache. On macOS that passes only because
//     anyPhysicalMouseButtonDown() now calls +[NSEvent pressedMouseButtons]; a
//     regression to the cached path fails it there. The macOS job runs this suite
//     (`scripts/run-tests.sh`), so that is where the macOS half is actually
//     verified -- it cannot be verified on the Linux box that wrote it, and this
//     comment is here so nobody later mistakes a Linux pass for macOS evidence.
static void testPhysicalButtonQueryIgnoresCachedState()
{
    std::printf ("State test 23: the abandoned-gesture trigger reads PHYSICAL buttons (KI-013/KI-028)\n");

    const auto saved = juce::ModifierKeys::currentModifiers;

    // LIVENESS, on every platform: with nothing cached and nothing held, the
    // helper must be callable and say "up". Without this the platform legs below
    // could pass by the function being broken in the convenient direction.
    juce::ModifierKeys::currentModifiers = juce::ModifierKeys();
    check (! anamorph::gui::anyPhysicalMouseButtonDown(),
           "with nothing held and nothing cached, the physical query says up");
    check (! juce::Component::isMouseButtonDownAnywhere(),
           "...and so does the cached view");

    // THE KI-013 CONDITION: JUCE believes a button is down; the hardware disagrees.
    juce::ModifierKeys::currentModifiers = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier);
    const bool cachedSaysDown   = juce::Component::isMouseButtonDownAnywhere();
    const bool physicalSaysDown = anamorph::gui::anyPhysicalMouseButtonDown();
    std::printf ("  cached=%d physical=%d\n", (int) cachedSaysDown, (int) physicalSaysDown);
    check (cachedSaysDown, "the cached view reports the button JUCE was told about");

#if JUCE_MAC
    // THE FIX, ASSERTED WHERE IT APPLIES. +[NSEvent pressedMouseButtons] is a
    // global hardware query that does not care what events this process received,
    // so it must contradict the cache here. Before this change the macOS answer
    // came from ModifierKeys::currentModifiers and this check would fail --
    // which is exactly why KI-028's sweep never ran on macOS.
    check (! physicalSaysDown,
           "macOS: the physical query is not fooled by JUCE's cached button state");
    check (cachedSaysDown && ! physicalSaysDown,
           "macOS: the abandoned-gesture sweep gate fires for a release JUCE never saw");
#else
    // OFF macOS THIS CANNOT DISCRIMINATE, and saying so is the point. JUCE
    // installs getNativeRealtimeModifiers from a live ComponentPeer
    // (juce_Windowing_linux.cpp:67); this suite is headless and has none, so
    // ComponentPeer::getCurrentModifiersRealtime falls back to the cache and the
    // helper -- a deliberate pure forward on these platforms -- returns it too.
    // In a real host the peer exists and the query is genuinely physical, which
    // is why the Linux/Windows half of KI-028 works in production and why round
    // 3's State test 21 drives the sweep directly instead of through this gate.
    // Assert the FORWARDING IDENTITY instead: that is the whole contract here.
    check (physicalSaysDown
             == juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown(),
           "off macOS the helper forwards to JUCE's realtime query exactly");
    std::printf ("  (headless, no peer: JUCE's realtime query is the cache here, so this leg\n"
                 "   checks the forward, not the hardware -- the discriminating check is macOS-only)\n");
#endif

    juce::ModifierKeys::currentModifiers = saved;
}

// ---------------------------------------------------------------------------
// 25. CROSS-VERSION FIELD CAPTURE: a session written by the PREVIOUS version's
//     binary must reproduce exactly in this one.
//
//     RELEASE_COMPATIBILITY_CHECKLIST §"Session reload verified" asks for a
//     session saved by vN-1 and loaded by vN, and records that the existing
//     legacy fixtures cannot discharge it because they are RECONSTRUCTIONS of old
//     formats hand-built by the current code, not captures written by an older
//     binary (worklogs/STATE_HARNESS_v0.8.13.md §5). That is a real distinction:
//     a reconstruction can only contain what today's understanding says the old
//     format held, so it cannot catch a field the old binary actually wrote
//     differently.
//
//     `tests/fixtures/field_capture_v0_9_5.session` is the missing thing: 10,629
//     bytes produced by the v0.9.5 binary itself (the tree at 2c5e760^, the commit
//     before the 0.9.6 bump), built from that source with its own JUCE pin. Beside
//     it, `.manifest` records what THAT binary believed the state was, including
//     the B slot it had to switch to in order to read. The assertions below
//     compare against those numbers, so this test asks "does v0.9.6 reproduce what
//     v0.9.5 had", not "does v0.9.6 agree with itself".
//
//     Covers all four things the checklist item names: sound, preset name,
//     dirty-star, and both A/B slots.
static void testCrossVersionFieldCapture()
{
    std::printf ("State test 25: a v0.9.5-written session reproduces exactly (cross-version capture)\n");

    const auto blob = fixtureDir().getChildFile ("field_capture_v0_9_5.session");
    const auto manifestFile = fixtureDir().getChildFile ("field_capture_v0_9_5.session.manifest");
    check (blob.existsAsFile(), "the v0.9.5 field capture is present");
    check (manifestFile.existsAsFile(), "...and so is its manifest");
    if (! blob.existsAsFile() || ! manifestFile.existsAsFile()) return;

    // Parse the emitter's own record. Anything missing is a broken fixture, not a
    // pass -- the check() calls below would otherwise compare against 0.0.
    juce::StringPairArray expected;
    juce::StringArray slotA, slotB;
    for (const auto& lineRef : juce::StringArray::fromLines (manifestFile.loadFileAsString()))
    {
        const auto line = lineRef.trim();
        if (line.isEmpty()) continue;
        if (line.startsWith ("slotA ")) slotA = juce::StringArray::fromTokens (line.substring (6), " ", "");
        else if (line.startsWith ("slotB ")) slotB = juce::StringArray::fromTokens (line.substring (6), " ", "");
        else if (line.contains ("=")) expected.set (line.upToFirstOccurrenceOf ("=", false, false),
                                                    line.fromFirstOccurrenceOf ("=", false, false));
    }
    check (expected["emitter"] == "v0.9.5", "the manifest was written by the v0.9.5 binary");
    check (slotA.size() == 5 && slotB.size() == 5, "the manifest carries both slots' values");
    if (slotA.size() != 5 || slotB.size() != 5) return;

    auto valueOf = [] (const juce::StringArray& fields, const juce::String& key) -> float
    {
        for (const auto& f : fields)
            if (f.startsWith (key + "="))
                return f.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
        return -1.0f;
    };

    auto blobData = juce::MemoryBlock();
    check (blob.loadFileAsData (blobData), "the capture loads from disk");
    check (blobData.getSize() > 1000, "the capture is a real session blob, not a stub");

    AnamorphAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    proc.setStateInformation (blobData.getData(), (int) blobData.getSize());
    juce::Timer::callPendingTimersSynchronously();

    // (1) SOUND -- the active slot's parameters.
    struct P { const char* id; const char* key; };
    const P params[] = { { pid::width, "width" }, { pid::mix, "mix" },
                         { pid::amount, "amount" }, { pid::algorithm, "algo" },
                         { pid::outputGain, "outGain" } };
    for (const auto& pr : params)
    {
        const float want = valueOf (slotA, pr.key);
        const float got  = rawOf (proc, pr.id);
        std::printf ("  slotA %-8s v0.9.5 %.6f -> v0.9.6 %.6f\n", pr.key, want, got);
        checkNear ((double) got, (double) want, 1.0e-5,
                   "the active slot's sound reproduces from the v0.9.5 capture");
    }

    // (2) PRESET NAME and (3) DIRTY-STAR.
    std::printf ("  preset name: \"%s\" (v0.9.5: \"%s\"), dirty %d (v0.9.5: %s)\n",
                 proc.getPresets().currentName().toRawUTF8(),
                 expected["presetName"].toRawUTF8(),
                 (int) proc.getPresets().isDirty(), expected["dirty"].toRawUTF8());
    check (proc.getPresets().currentName() == expected["presetName"],
           "the preset name reproduces from the v0.9.5 capture");
    check ((int) proc.getPresets().isDirty() == expected["dirty"].getIntValue(),
           "the dirty-star reproduces from the v0.9.5 capture");
    check (proc.abActiveSlot() == expected["activeSlot"].getIntValue(),
           "the active A/B slot reproduces from the v0.9.5 capture");

    // (4) BOTH A/B SLOTS -- switch to B and compare against what v0.9.5 had there.
    //     Non-vacuity: the two slots must actually DIFFER in the manifest, or this
    //     leg would pass on a build that ignored the B slot entirely.
    check (! juce::approximatelyEqual (valueOf (slotA, "width"), valueOf (slotB, "width")),
           "the capture's two slots really do differ (the B leg is not vacuous)");
    proc.abSwitchTo (1);
    for (const auto& pr : params)
    {
        const float want = valueOf (slotB, pr.key);
        const float got  = rawOf (proc, pr.id);
        std::printf ("  slotB %-8s v0.9.5 %.6f -> v0.9.6 %.6f\n", pr.key, want, got);
        checkNear ((double) got, (double) want, 1.0e-5,
                   "the B slot reproduces from the v0.9.5 capture");
    }
}

// ---------------------------------------------------------------------------
// Round-3 KI-028 probe (opt-in, PRINTS, asserts only its own non-vacuity).
//
//     Measures the two things the round-3 adversarial pass disputed about KI-028:
//       (1) whether destroying the editor mid-value-box-drag leaks the host
//           gesture into the PROCESSOR, which outlives the editor -- a path the
//           original filing declared SAFE on the grounds that ~ValueBox fires
//           ~ScopedDragNotification. The dispute: PluginEditor.h declares the
//           Knobs at :432-435 and `sliderAtts` at :482, and members destruct in
//           REVERSE declaration order, so ~sliderAtts runs FIRST and
//           SliderParameterAttachment's destructor removes itself as a Slider
//           listener. By the time ~ValueBox fires sendDragEnd there may be no
//           listener left to hear it, and endChangeGesture never runs.
//       (2) whether openGestures self-heals, which decides KI-028's severity.
static int runValueBoxGestureProbe()
{
    std::printf ("Round-3 probe: value-box gesture lifetime (KI-028)\n");
#if ! (JUCE_LINUX || JUCE_BSD)
    std::printf ("  SKIPPED off Linux -- headless editor construction is unverified there (KI-007)\n");
    return 0;
#else
    // A complete, balanced edit through the processor: this is the yardstick.
    // If openGestures is clean it produces exactly one undo step.
    auto editYieldsUndoStep = [] (AnamorphAudioProcessor& proc, const char* id, float norm)
    {
        auto* rp = proc.getAPVTS().getParameter (id);
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (norm);
        rp->endChangeGesture();
        proc.pollUndoCoalesce();
        return proc.canUndo();
    };

    // -- POSITIVE CONTROL: a deliberately leaked gesture. Without this leg every
    //    "no leak" result below is unfalsifiable -- it would read the same if
    //    canUndo() simply never went false. This proves the yardstick can fail.
    {
        AnamorphAudioProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        auto* held = proc.getAPVTS().getParameter (pid::width);
        held->beginChangeGesture();               // opened and never closed
        const bool undoWorks = editYieldsUndoStep (proc, pid::mix, 0.31f);
        std::printf ("  POSITIVE CONTROL (a gesture opened and never closed): canUndo = %d%s\n",
                     (int) undoWorks, undoWorks ? "  <-- YARDSTICK IS BLIND" : "  (yardstick detects a leak)");
        held->endChangeGesture();
    }

    // -- control: no drag at all. Establishes that the yardstick works. --------
    {
        AnamorphAudioProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        auto* raw = proc.createEditor();
        auto* ed = dynamic_cast<AnamorphAudioProcessorEditor*> (raw);
        if (ed == nullptr) { delete raw; std::printf ("  editor did not construct\n"); return 1; }
        proc.editorBeingDeleted (ed);
        delete ed;
        std::printf ("  control (editor opened and closed, no drag): canUndo after a full edit = %d\n",
                     (int) editYieldsUndoStep (proc, pid::mix, 0.31f));
    }

    // -- leg 1: press a value box, then destroy the editor mid-press. ---------
    {
        AnamorphAudioProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        auto* raw = proc.createEditor();
        auto* ed = dynamic_cast<AnamorphAudioProcessorEditor*> (raw);
        if (ed == nullptr) { delete raw; std::printf ("  editor did not construct\n"); return 1; }

        // Find a rotary slider that actually owns a value-box Label child.
        juce::Slider* slider = nullptr;
        juce::Label*  box    = nullptr;
        std::function<void (juce::Component*)> walk = [&] (juce::Component* c)
        {
            if (slider != nullptr) return;
            for (int i = 0; i < c->getNumChildComponents(); ++i)
            {
                auto* kid = c->getChildComponent (i);
                if (auto* sl = dynamic_cast<juce::Slider*> (kid))
                    for (int j = 0; j < sl->getNumChildComponents(); ++j)
                        if (auto* lb = dynamic_cast<juce::Label*> (sl->getChildComponent (j)))
                        { slider = sl; box = lb; break; }
                if (slider == nullptr) walk (kid);
                else return;
            }
        };
        walk (ed);
        std::printf ("  found a slider with a value box: %d\n", (int) (slider != nullptr && box != nullptr));
        if (slider == nullptr || box == nullptr)
        {
            proc.editorBeingDeleted (ed); delete ed;
            std::printf ("  PROBE CANNOT RUN -- no value box found\n");
            return 1;
        }

        // Synthesise the press the ValueBox reacts to. This is the real virtual,
        // so the real ScopedDragNotification is created and really is owned by the
        // ValueBox -- which is what makes the destruction-order question live.
        const juce::MouseEvent me (juce::Desktop::getInstance().getMainMouseSource(),
                                   { 4.0f, 4.0f }, juce::ModifierKeys::leftButtonModifier,
                                   1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                   box, box, juce::Time::getCurrentTime(),
                                   { 4.0f, 4.0f }, juce::Time::getCurrentTime(), 1, false);
        box->mouseDown (me);
        std::printf ("  after mouseDown on the value box: slider \"dragging\" = %d\n",
                     (int) (bool) slider->getProperties().getWithDefault ("dragging", false));

        // ...and tear the editor down while the press is still open.
        proc.editorBeingDeleted (ed);
        delete ed;

        const bool undoWorks = editYieldsUndoStep (proc, pid::mix, 0.31f);
        std::printf ("  LEG 1 editor destroyed mid-press: canUndo after a full edit = %d%s\n",
                     (int) undoWorks,
                     undoWorks ? "  (no leak)" : "  <-- GESTURE LEAKED INTO THE PROCESSOR");

        // -- leg 2: does it self-heal? syncCommitted / undo / redo / preset load
        //    all force openGestures back to 0 per the adversarial pass.
        if (! undoWorks)
        {
            proc.getPresets().load (1);       // a preset load records its own step
            const bool healed = editYieldsUndoStep (proc, pid::mix, 0.62f);
            std::printf ("  LEG 2 after a preset load: canUndo = %d%s\n", (int) healed,
                         healed ? "  (self-heals)" : "  (still stuck)");
        }
    }

    // -- leg 3: THE ACTUAL KI-028 SHAPE -- the release is lost while the editor
    //    is still alive and the ValueBox still holds its ScopedDragNotification.
    //    This is the one path the round-3 investigation found survives; legs 1-2
    //    exist to test the disputed claim that editor teardown is another.
    {
        AnamorphAudioProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        auto* raw = proc.createEditor();
        auto* ed = dynamic_cast<AnamorphAudioProcessorEditor*> (raw);
        if (ed == nullptr) { delete raw; return 1; }

        juce::Slider* slider = nullptr;
        juce::Label*  box    = nullptr;
        std::function<void (juce::Component*)> walk = [&] (juce::Component* c)
        {
            if (slider != nullptr) return;
            for (int i = 0; i < c->getNumChildComponents(); ++i)
            {
                auto* kid = c->getChildComponent (i);
                if (auto* sl = dynamic_cast<juce::Slider*> (kid))
                    for (int j = 0; j < sl->getNumChildComponents(); ++j)
                        if (auto* lb = dynamic_cast<juce::Label*> (sl->getChildComponent (j)))
                        { slider = sl; box = lb; break; }
                if (slider == nullptr) walk (kid);
                else return;
            }
        };
        walk (ed);
        if (slider != nullptr && box != nullptr)
        {
            const juce::MouseEvent me (juce::Desktop::getInstance().getMainMouseSource(),
                                       { 4.0f, 4.0f }, juce::ModifierKeys::leftButtonModifier,
                                       1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                       box, box, juce::Time::getCurrentTime(),
                                       { 4.0f, 4.0f }, juce::Time::getCurrentTime(), 1, false);
            box->mouseDown (me);                       // ...and NO mouseUp ever arrives
            const bool undoWorks = editYieldsUndoStep (proc, pid::mix, 0.31f);
            std::printf ("  LEG 3 press never released, editor alive: canUndo after a full edit = %d%s\n",
                         (int) undoWorks,
                         undoWorks ? "  (no leak)" : "  <-- KI-028: undo blocked while the gesture is open");
        }
        proc.editorBeingDeleted (ed);
        delete ed;
    }

    std::printf ("\nprobe complete\n");
    return 0;
#endif
}

// ---------------------------------------------------------------------------
// 24. A restore must leave the host holding the RESTORED state's latency.
//
//     WHAT MOVES THE LATENCY CHANGED UNDER THIS TEST, AND THE TEST SAYS SO. Until
//     0.9.7 the reported number depended on whether the oversampling wrap was
//     ENGAGED, so a Drive parameter bore latency and this test poisoned Drive:
//     apvts.replaceState adopts whatever @value says -- it CONVERTS BY CLAMPING,
//     so value="inf" landed at the range maximum and re-reported a latency for
//     it -- and reassertParameters then repaired that value with setValue() plus
//     a direct atomic store, notifying nobody on purpose. ADR-0034 made the
//     reported number a function of the Oversampling SELECTION alone, so no
//     parameter can bear latency any more and THAT VARIANT OF THE HAZARD IS
//     UNREACHABLE, not merely unexercised. The blob is still poisoned exactly as
//     before, so the restore still travels the replaceState-then-repair path; what
//     it can no longer do is move the report, and this test no longer pretends it
//     does.
//
//     WHAT THE TEST STILL PINS, and it is the invariant its title always named:
//     after a restore the host holds the RESTORED state's latency, not the one it
//     held going in. The restore under test crosses the Oversampling Setting
//     (live 2x -> restored Off), so a build that failed to re-derive the report
//     leaves 4 where 0 is due. setStateInformation's trailing latency request is
//     what makes it 0.
//
//     THE SECOND RESTORE IS STILL THE ONE THAT MATTERS. On a FIRST restore the
//     answer can come out right by accident: the live InternalState holds an int
//     where the round-tripped blob holds a string, ValueTree::setProperty sees a
//     difference and fires onOversampleChanged -- the var-type coincidence
//     recorded for ER-STATE-07. Restoring twice settles the types; the round-4
//     probe restored once, twice, and reported the original defect as refuted
//     before this leg was added.
//
//     Measured before the round-4 fix: reported 4, restored state predicts 0.
static void testRestoreReportsTheRestoredLatency()
{
    std::printf ("State test 24: a restore reports the RESTORED state's latency\n");

    juce::MemoryBlock poisoned;
    {
        AnamorphAudioProcessor authoring;
        // Oversampling OFF in the authored session: this is the field the restore
        // has to move the report with, and the live instance below sits at 2x.
        authoring.getInternal().oversampleValue().setValue (1);
        authoring.prepareToPlay (48000.0, 256);
        juce::MemoryBlock clean;
        authoring.getStateInformation (clean);
        auto xml = BlobCodec::unwrap (clean);
        check (xml != nullptr, "authored session decodes for poisoning");
        if (xml == nullptr) return;
        bool poisonedOne = false;
        if (auto* params = xml->getChildByName ("ANAMORPH"))
            if (auto* d = params->getChildByAttribute ("id", pid::drive))
            {
                d->setAttribute ("value", "inf");
                d->removeAttribute ("raw");
                poisonedOne = true;
            }
        check (poisonedOne, "the session carries a drive PARAM to poison");
        poisoned = BlobCodec::wrap (*xml);
    }

    AnamorphAudioProcessor proc;
    juce::Value osValue = proc.getInternal().oversampleValue();
    osValue.setValue (2);                                  // live: 2x
    proc.prepareToPlay (48000.0, 256);

    // NON-VACUITY: the two states must report DIFFERENT latencies, or
    // "reported == predicted" below is 0 == 0 and means nothing.
    juce::Timer::callPendingTimersSynchronously();
    const int loud = proc.getLatencySamples();
    check (loud != 0, "the live instance reports a non-zero latency at 2x oversampling");

    proc.setStateInformation (poisoned.getData(), (int) poisoned.getSize()); // settle property types
    osValue.setValue (2);                                                    // back to latency-bearing
    juce::Timer::callPendingTimersSynchronously();
    check (proc.getLatencySamples() == loud, "re-armed at the latency-bearing state");

    // The restore under test: types now agree, so nothing recomputes by accident.
    proc.setStateInformation (poisoned.getData(), (int) poisoned.getSize());
    juce::Timer::callPendingTimersSynchronously();
    const int reported = proc.getLatencySamples();

    // The number the restored state is entitled to, through public API only:
    // prepareToPlay ends with the same latency update.
    proc.prepareToPlay (48000.0, 256);
    const int predicted = proc.getLatencySamples();

    std::printf ("  second restore: reported %d, restored state predicts %d (was %d going in)\n",
                 reported, predicted, loud);
    check (reported == predicted,
           "the host is not left holding the pre-restore latency after a restore");
    check (predicted != loud,
           "non-vacuity: the restore genuinely crossed a reported-latency boundary");
}

// Round-4 probe: does a malformed restore leave a STALE reported latency?
static int runRestoreLatencyProbe()
{
    std::printf ("Round-4 probe: reported latency after a malformed restore\n");

    // Author a session whose algorithm is poisoned to "inf". JUCE's own
    // replaceState adopts it BEFORE reassertParameters gets a chance to repair,
    // and replaceState notifies -- so the host hears a latency for a state the
    // plug-in is about to reject.
    //
    // SINCE ADR-0034 THIS PROBE ALSO MEASURES A CONSTANT: no parameter bears
    // latency any more, so the poisoned drive cannot move the report and every
    // number it prints is the selected factor's. Kept as the record of the
    // round-4 measurement, not as a live instrument. State test 24 carries the
    // surviving invariant, re-instrumented on the Oversampling Setting.
    juce::MemoryBlock poisoned;
    {
        AnamorphAudioProcessor authoring;
        authoring.getInternal().oversampleValue().setValue (2); // 2x: latency is nonzero at all
        authoring.prepareToPlay (48000.0, 256);
        juce::MemoryBlock clean;
        authoring.getStateInformation (clean);
        auto xml = BlobCodec::unwrap (clean);
        if (xml == nullptr) { std::printf ("  unwrap failed\n"); return 1; }
        if (auto* params = xml->getChildByName ("ANAMORPH"))
            if (auto* a = params->getChildByAttribute ("id", pid::drive))
            {
                a->setAttribute ("value", "inf");
                a->removeAttribute ("raw");
            }
        poisoned = BlobCodec::wrap (*xml);
    }

    AnamorphAudioProcessor proc;
    proc.getInternal().oversampleValue().setValue (2);
    proc.prepareToPlay (48000.0, 256);
    std::printf ("  before restore: drive norm %.6f, reported latency %d\n",
                 rawOf (proc, pid::drive), proc.getLatencySamples());

    proc.setStateInformation (poisoned.getData(), (int) poisoned.getSize());
    std::printf ("  IMMEDIATELY after setStateInformation: drive norm %.6f, latency %d\n",
                 rawOf (proc, pid::drive), proc.getLatencySamples());
    // What the poisoned value WOULD have predicted, for comparison: drive at max.
    {
        AnamorphAudioProcessor ref;
        ref.getInternal().oversampleValue().setValue (2);
        ref.prepareToPlay (48000.0, 256);
        ref.getAPVTS().getParameter (pid::drive)->setValueNotifyingHost (1.0f);
        std::printf ("  (a drive-at-max instance reports latency %d, so the two ARE distinguishable)\n",
                     ref.getLatencySamples());
    }
    // Isolate JUCE: does replaceState ALONE adopt the poison and re-report?
    // If it does, the transient exists and only the final report is repaired; if
    // it does not, there was never a bad report to be stale.
    {
        AnamorphAudioProcessor iso;
        iso.getInternal().oversampleValue().setValue (2);
        iso.prepareToPlay (48000.0, 256);
        auto tree = iso.getAPVTS().copyState();
        if (auto node = tree.getChildWithProperty ("id", pid::drive); node.isValid())
        {
            node.setProperty ("value", "inf", nullptr);
            node.removeProperty ("raw", nullptr);
        }
        iso.getAPVTS().replaceState (tree);   // NO reassertParameters anywhere near this
        std::printf ("  replaceState alone with value=\"inf\": drive norm %.6f, latency %d\n",
                     rawOf (iso, pid::drive), iso.getLatencySamples());
    }

    juce::Timer::callPendingTimersSynchronously();

    const float algoAfter = rawOf (proc, pid::drive);
    const int reported = proc.getLatencySamples();
    // The number the FINAL state is entitled to: a re-prepare recomputes it.
    proc.prepareToPlay (48000.0, 256);
    const int truth = proc.getLatencySamples();

    std::printf ("  after restore : drive norm %.6f (default %.6f)\n",
                 algoAfter, proc.getAPVTS().getParameter (pid::drive)->getDefaultValue());
    std::printf ("  reported to the host: %d ; the restored state actually predicts: %d\n",
                 reported, truth);
    std::printf ("  %s\n", reported == truth
                     ? "first restore: the report follows the repair"
                     : "first restore: CONFIRMED stale");

    // SECOND RESTORE, same blob. Round 2 established that the correct answer on a
    // FIRST restore can be an accident: the live InternalState holds an int while
    // the blob carries a round-tripped string, so ValueTree::setProperty sees a
    // difference, fires onOversampleChanged and recomputes the latency AFTER the
    // repair. Restoring twice settles the property types and removes that
    // coincidence -- which is also the ordinary case, a host loading project
    // after project into one instance.
    {
        AnamorphAudioProcessor twice;
        twice.getInternal().oversampleValue().setValue (2);
        twice.prepareToPlay (48000.0, 256);
        twice.setStateInformation (poisoned.getData(), (int) poisoned.getSize()); // settle types
        twice.getAPVTS().getParameter (pid::drive)->setValueNotifyingHost (1.0f); // back to a latency-bearing state
        juce::Timer::callPendingTimersSynchronously();
        const int beforeB = twice.getLatencySamples();
        twice.setStateInformation (poisoned.getData(), (int) poisoned.getSize());
        juce::Timer::callPendingTimersSynchronously();
        const int reportedB = twice.getLatencySamples();
        twice.prepareToPlay (48000.0, 256);
        const int truthB = twice.getLatencySamples();
        std::printf ("  SECOND restore: before %d, reported %d, actually predicts %d -- %s\n",
                     beforeB, reportedB, truthB,
                     reportedB == truthB ? "report follows the repair"
                                         : "CONFIRMED: stale reported latency");
    }
    return 0;
}

// ---------------------------------------------------------------------------
//  State test 26 -- a restore that carries no A/B data must not leave the
//  PREVIOUS project's slots loaded (ER-STATE-12).
//
//  `readSlot` already enforces "absent means the default, not whatever the
//  previous session left here", but only for a blob that HAS an `AB` node, since
//  that is the branch it is called from. Two restore paths carry no A/B data at
//  all -- an `AnamorphRoot` with no `AB` child, and a v0.2 bare-APVTS session,
//  which predates the feature -- and both left `abSlot[]` and `abActive` holding
//  the previous project's values on a REUSED instance. Measured before the fix:
//  after a v0.2 restore, switching to B played the previous project's B (raw
//  width 0.10 against a restored 0.75), and with the previous project left active
//  on B the first switch read its A (0.90) and its active index survived too.
//
//  Non-vacuity is asserted, not assumed: the two previous-project slots must
//  differ from EACH OTHER and the restored value must differ from BOTH, or the
//  readbacks below could pass while carrying stale state.
// ---------------------------------------------------------------------------
static void testLegacyRestoreResetsAbSlots()
{
    std::printf ("State test 26: a restore with no A/B data resets both slots (ER-STATE-12)\n");

    constexpr float kPrevA = 0.90f;   // the "previous project" A sound
    constexpr float kPrevB = 0.10f;   // ...and its B sound, clearly different
    constexpr float kTol   = 0.02f;

    // Load the previous project's two distinguishable sounds into one instance and
    // leave it on `stayOn`, which decides which slot the first post-restore switch
    // reads: switching AWAY from a slot stores the live (restored) state into it,
    // so only the slot we do NOT start on can show contamination on the first move.
    auto seedPreviousProject = [] (AnamorphAudioProcessor& p, int stayOn)
    {
        p.prepareToPlay (48000.0, 512);
        setRaw (p, "width", kPrevA);
        p.abSwitchTo (1);
        setRaw (p, "width", kPrevB);
        if (stayOn == 0) p.abSwitchTo (0);
    };

    // --- Leg 1: v0.2 bare APVTS, reused instance, first switch reads B.
    {
        AnamorphAudioProcessor p;
        seedPreviousProject (p, 0);
        if (! applyXmlFixture (p, "legacy_v0_2_bare_apvts.xml"))
            return;

        const float restored = rawOf (p, "width");
        check (std::abs (kPrevA - kPrevB) > kTol,
               "the previous project's A and B are distinguishable (non-vacuity)");
        check (std::abs (restored - kPrevA) > kTol && std::abs (restored - kPrevB) > kTol,
               "the restored v0.2 value differs from BOTH previous slots (non-vacuity)");
        std::printf ("  previous project A %.4f / B %.4f; v0.2 restores %.4f\n",
                     kPrevA, kPrevB, restored);

        check (p.abActiveSlot() == 0, "a session with no AB data restores the default active slot");
        p.abSwitchTo (1);
        const float b = rawOf (p, "width");
        checkNear ((double) b, (double) restored, 1.0e-4,
                   "slot B holds the RESTORED state, not the previous project's B");
        check (std::abs (b - kPrevB) > kTol, "...and specifically not the previous project's B value");
        p.abSwitchTo (0);
        checkNear ((double) rawOf (p, "width"), (double) restored, 1.0e-4,
                   "slot A holds the RESTORED state after switching back");
    }

    // --- Leg 2: same, but the previous project is left active on B, so the first
    // switch after the restore is the one that reads A. Without this leg slot A is
    // never actually observed -- leg 1's switch to B overwrites A on the way out.
    {
        AnamorphAudioProcessor p;
        seedPreviousProject (p, 1);
        check (p.abActiveSlot() == 1, "precondition: the previous project is active on B");
        if (! applyXmlFixture (p, "legacy_v0_2_bare_apvts.xml"))
            return;

        const float restored = rawOf (p, "width");
        check (p.abActiveSlot() == 0,
               "the previous project's ACTIVE INDEX does not survive either");
        p.abSwitchTo (1);                       // 0 -> 1 so the next move reads A afresh
        p.abSwitchTo (0);
        const float a = rawOf (p, "width");
        checkNear ((double) a, (double) restored, 1.0e-4,
                   "slot A holds the RESTORED state, not the previous project's A");
        check (std::abs (a - kPrevA) > kTol, "...and specifically not the previous project's A value");
    }

    // --- Leg 3: a FRESH instance. This was written expecting a vacuous pass -- the
    // slots "start invalid", so nothing should need resetting -- and MEASUREMENT
    // refuted that: pre-fix it failed with 0.5, the Default width. The constructor
    // calls abEnsureInit() EAGERLY (so B is not born as a copy of an already-edited
    // A), so by the time any restore arrives both slots are already valid, holding
    // the open/Default state. A v0.2 restore then left slot B on Default rather than
    // on the restored session -- the same defect with the construction snapshot in
    // the previous project's place. Kept, and no longer described as the easy leg.
    {
        AnamorphAudioProcessor fresh;
        fresh.prepareToPlay (48000.0, 512);
        if (! applyXmlFixture (fresh, "legacy_v0_2_bare_apvts.xml"))
            return;
        const float restored = rawOf (fresh, "width");
        check (std::abs (restored - fresh.getAPVTS().getParameter ("width")->getDefaultValue()) > kTol,
               "non-vacuity: the restored value differs from the Default the slots were seeded with");
        fresh.abSwitchTo (1);
        checkNear ((double) rawOf (fresh, "width"), (double) restored, 1.0e-4,
                   "fresh instance: slot B is seeded from the restored state, not from construction");
    }

    // --- Leg 4: an AnamorphRoot with no `AB` child -- the same class of path, on
    // the CURRENT format. `AB` is optional (every field in it is "Required: No").
    {
        AnamorphAudioProcessor p;
        seedPreviousProject (p, 0);
        juce::MemoryBlock mb;
        p.getStateInformation (mb);
        auto xml = BlobCodec::unwrap (mb);
        check (xml != nullptr, "a current-format save round-trips through the blob codec");
        if (xml == nullptr) return;
        auto root = juce::ValueTree::fromXml (*xml);
        check (root.getChildWithName ("AB").isValid(),
               "precondition: a current save DOES carry an AB child (so leg 4 is a real strip)");
        root.removeChild (root.getChildWithName ("AB"), nullptr);

        setRaw (p, "width", 0.55f);             // a third, distinct live value
        const auto stripped = BlobCodec::wrap (*root.createXml());
        p.setStateInformation (stripped.getData(), (int) stripped.getSize());
        const float restored = rawOf (p, "width");
        check (std::abs (restored - kPrevB) > kTol,
               "non-vacuity: the AB-less root restores something other than the previous B");
        p.abSwitchTo (1);
        const float b = rawOf (p, "width");
        checkNear ((double) b, (double) restored, 1.0e-4,
                   "AB-less root: slot B holds the RESTORED state");
        check (std::abs (b - kPrevB) > kTol, "...and not the previous project's B value");
    }

    // --- Leg 5: the fix must NOT erase legitimate A/B data. A root that DOES carry
    // an AB node still restores both slots' own sounds.
    {
        AnamorphAudioProcessor src;
        src.prepareToPlay (48000.0, 512);
        setRaw (src, "width", 0.80f);
        src.abSwitchTo (1);
        setRaw (src, "width", 0.20f);
        src.abSwitchTo (0);                     // A = 0.80, B = 0.20, active = A
        juce::MemoryBlock saved;
        src.getStateInformation (saved);

        AnamorphAudioProcessor dst;             // a REUSED instance with other content
        seedPreviousProject (dst, 0);
        dst.setStateInformation (saved.getData(), (int) saved.getSize());
        checkNear ((double) rawOf (dst, "width"), 0.80, 1.0e-4,
                   "a root WITH an AB node still restores slot A's own sound");
        dst.abSwitchTo (1);
        checkNear ((double) rawOf (dst, "width"), 0.20, 1.0e-4,
                   "...and slot B's, so the reset did not erase carried A/B data");
    }
}

// ---------------------------------------------------------------------------
//  State test 27 -- three restore/latency integrity guards (round 11).
//
//  ER-STATE-14  a latency request that lands while a delivery is running must
//               not be swallowed by a second clear of the request flag.
//  ER-STATE-15  an `AnamorphRoot` with no `ANAMORPH` sound child restores no
//               sound, so it must adopt no metadata and no Settings either.
//  ER-STATE-16  an A/B slot whose payload is REJECTED must not keep that
//               payload's name, baseline or indicator identity: the slot is
//               reseeded from the restored state, and its label has to describe
//               that same state.
// ---------------------------------------------------------------------------
static void testRestoreIntegrityGuards()
{
    std::printf ("State test 27: restore/latency integrity (ER-STATE-14/15/16)\n");

    // === ER-STATE-14 ========================================================
    // DETERMINISTIC. The second request is made INSIDE the delivery, from a real
    // off-message thread, at a barrier the PRODUCT provides: AudioProcessor::
    // setLatencySamples() notifies its AudioProcessorListeners synchronously, from
    // within the call, whenever the reported value changes (pinned JUCE 9.0.1,
    // juce_AudioProcessor.cpp:415-436), and the listener lock is released before
    // each callback (getListenerLocked, :425-429), so a listener may block. That
    // is exactly the sequence the review asked for -- delivery starts, another
    // thread requests, delivery completes, the second request must still be
    // observed and delivered -- with no test hook in production code and no
    // timing race: the interleaving is FORCED, not hoped for.
    //
    // WHAT IT DOES AND DOES NOT PROVE, stated exactly. It pins the invariant
    // "a request that lands while a delivery is running survives to the next
    // tick": a build that clears the flag AFTER delivering fails it (measured --
    // see the round-12 worklog), so a future refactor of that shape cannot land
    // green. It does NOT reach the round-11 double-clear window itself: that lay
    // between timerCallback's exchange(0) and the second store(0) the old
    // updateLatency() performed at its ENTRY -- two adjacent atomics on one thread
    // with no call between them -- and no external mechanism can place a request
    // there. The pre-round-11 code therefore passes this leg too; that fix rests
    // on inspection, and this comment exists so nobody reads green here as proof
    // of it. Neither half of that fix was measurable on x86-64 either way.
    //
    // The waits below are bounded polls for the processor's own 20 Hz timer, not
    // sleeps that stand in for synchronisation: the outcome is fixed the moment
    // the request is or is not in the flag, and the deadline (40 timer periods)
    // only bounds how long a FAILING run takes to report. A tight loop of
    // callPendingTimersSynchronously() would fire nothing -- the countdown is
    // never due -- which is the harness defect the round-11 version of this leg
    // had before it was found.
    {
        struct Barrier final : public juce::AudioProcessorListener
        {
            std::atomic<bool> armed { false }, inDelivery { false }, requested { false };
            std::atomic<int>  fired { 0 };
            juce::AudioProcessor* owner = nullptr;
            std::vector<int> delivered;   // every reported value, in order (message thread only)
            void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
            void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& d) override
            {
                if (! d.latencyChanged) return;
                if (owner != nullptr) delivered.push_back (owner->getLatencySamples());
                if (! armed.exchange (false)) return;
                fired.fetch_add (1);
                inDelivery.store (true);                       // delivery has started...
                while (! requested.load()) std::this_thread::yield();   // ...hold it open until the request lands
            }
        };

        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        // The mover is the Oversampling SETTING, not Drive, since ADR-0034: the
        // reported latency is a function of that selection alone, so no parameter
        // can raise a value-CHANGING request any more. A host restoring session
        // state off its own thread (RISK-007) carries this Setting, which is the
        // reachable off-message-thread requester this leg stands in for -- and
        // since D-2 (ADR-0036) that restore is the ONLY production path that raises
        // the request from a non-message thread with a changed Setting, so the two
        // requests below are two real restores. Two sessions are authored on the
        // message thread, one at each Setting.
        juce::Value osValue = p.getInternal().oversampleValue();  // 1-based combo ids
        juce::MemoryBlock sessionLow, sessionHigh;
        osValue.setValue (1); p.prepareToPlay (48000.0, 512);     // Off
        const int lowLat  = p.getLatencySamples();
        p.getStateInformation (sessionLow);
        osValue.setValue (2); p.prepareToPlay (48000.0, 512);     // 2x
        const int highLat = p.getLatencySamples();
        p.getStateInformation (sessionHigh);
        osValue.setValue (1); p.prepareToPlay (48000.0, 512);     // back to low; host holds lowLat
        check (lowLat != highLat, "non-vacuity: the reported latency actually moves with the Setting here");

        Barrier barrier;
        barrier.owner = &p;
        p.addListener (&barrier);
        barrier.armed.store (true);

        std::thread worker ([&]
        {
            // Request #1, off the message thread: a restore carrying 2x raises the flag.
            p.setStateInformation (sessionHigh.getData(), (int) sessionHigh.getSize());
            // Wait until the timer's delivery of #1 has STARTED (the barrier holds it
            // open). Bounded, so a build that never delivers fails the checks below
            // instead of hanging the suite on this join.
            for (int waited = 0; waited < 20000 && ! barrier.inDelivery.load(); ++waited)
                std::this_thread::sleep_for (std::chrono::milliseconds (1));
            // ...and make request #2 from inside that window: a restore carrying Off
            // raises the flag again. The message thread is parked inside
            // audioProcessorChanged for the whole of this restore; the restore itself
            // touches no message-thread state (D-2) -- it applies the sound, stores the
            // oversampling atomic and hands the rest over.
            p.setStateInformation (sessionLow.getData(), (int) sessionLow.getSize());
            barrier.requested.store (true);
        });

        // Serve the requests on the message thread. Delivery #1 is the one the barrier
        // sits in. Since D-2 the tick that adopts restore #1 delivers highLat from
        // INSIDE the adoption (the adopted Setting fires onOversampleChanged on this
        // thread) and may then serve request #2's flag in the SAME tick, so the wait is
        // for the whole SEQUENCE -- both values reported, in order -- rather than for a
        // tick boundary, and the observation is that order. (The host was left holding
        // lowLat before the worker started, so "reported == lowLat" alone would be true
        // before anything happened.)
        auto sequenceDone = [&]
        {
            return barrier.fired.load() == 1
                && barrier.delivered.size() >= 2
                && barrier.delivered.back() == lowLat;
        };
        for (int elapsed = 0; elapsed < 4000 && ! sequenceDone(); elapsed += 5)
        {
            juce::Timer::callPendingTimersSynchronously();
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
        }
        worker.join();
        check (barrier.fired.load() == 1, "the barrier fired exactly once, inside delivery #1");
        check (! barrier.delivered.empty() && barrier.delivered[0] == highLat,
               "delivery #1 completed with the value it read (highLat)");

        // Request #2 landed DURING delivery #1. It must survive it and be served --
        // nothing else on the message thread will.
        std::printf ("  delivery #1 -> %d with request #2 made inside it; then -> %d (expected %d); %d deliveries\n",
                     highLat, p.getLatencySamples(), lowLat, (int) barrier.delivered.size());
        check (sequenceDone() && p.getLatencySamples() == lowLat,
               "a request made DURING a delivery survives it and is served afterwards");
        p.removeListener (&barrier);
    }

    // === ER-STATE-15 ========================================================
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);

        // Give the live instance a distinguishable identity and sound, so anything
        // adopted from the headless blob is visible.
        setRaw (p, "width", 0.77f);
        p.getInternal().oversampleValue().setValue (3);      // non-default Settings
        p.getPresets().setMeta ("Live Name", "live-baseline", anamorph::PresetManager::Selection{});
        const float  liveWidth = rawOf (p, "width");
        const auto   liveName  = p.getPresets().currentName();
        const int    liveOs    = (int) p.getInternal().copyState()["int_oversample"];

        // A root that LOOKS like ours but carries no sound child, and whose
        // metadata is deliberately different from the live instance's.
        juce::ValueTree root ("AnamorphRoot");
        root.setProperty ("presetName", "Incoming Name", nullptr);
        root.setProperty ("presetBaseline", "incoming-baseline", nullptr);
        root.setProperty ("presetSource", "factory", nullptr);
        root.setProperty ("presetFactoryId", "some-factory-id", nullptr);
        juce::ValueTree internalNode ("ANAMORPH_INTERNAL");
        internalNode.setProperty ("int_oversample", 1, nullptr);   // would reset Settings
        root.appendChild (internalNode, nullptr);
        check (! root.getChildWithName (p.getAPVTS().state.getType()).isValid(),
               "precondition: the blob really has no ANAMORPH sound child");

        const auto blob = BlobCodec::wrap (*root.createXml());
        p.setStateInformation (blob.getData(), (int) blob.getSize());

        checkNear ((double) rawOf (p, "width"), (double) liveWidth, 1.0e-6,
                   "no sound child: the live sound is untouched");
        checkStr (p.getPresets().currentName(), liveName,
                  "...and the preset NAME is not adopted from a session that restored nothing");
        check ((int) p.getInternal().copyState()["int_oversample"] == liveOs,
               "...and the host-hidden Settings are not adopted either");
        check (p.getPresets().selection().kind == anamorph::PresetManager::Selection::Kind::unknown
                   || p.getPresets().selection().factoryId != "some-factory-id",
               "...and the indicator identity is not adopted");
    }

    // === ER-STATE-16 -- REFUTED, kept as a contract pin ======================
    // The review reported that a slot whose sound is rejected keeps that payload's
    // name, baseline and identity. Measured: it does not. StateSet::isValid() is
    // params.isValid(), so a rejected payload leaves the slot invalid, and
    // abEnsureInit() assigns `slot = currentStateSet()` -- the WHOLE struct, metadata
    // included. Every reader of abSlot[] calls abEnsureInit first, so the metadata
    // readSlot wrote is unreachable. Gating those reads on params.isValid() was
    // implemented, measured to change no test outcome, and reverted.
    //
    // These legs pass either way, which is why they are labelled a CONTRACT PIN and
    // not a regression guard: they assert that the rejected slot's sound and label
    // describe the same state, wherever in the code that ends up being satisfied.
    {
        // Build a real save, then corrupt ONLY slot B's payload while leaving its
        // name/baseline/identity intact -- the exact shape the review described.
        AnamorphAudioProcessor src;
        src.prepareToPlay (48000.0, 512);
        setRaw (src, "width", 0.20f);
        src.abSwitchTo (1);
        src.getPresets().setMeta ("Slot B Preset", "slotb-baseline", anamorph::PresetManager::Selection{});
        setRaw (src, "width", 0.85f);
        src.abSwitchTo (0);
        setRaw (src, "width", 0.30f);
        src.getPresets().setMeta ("Slot A Preset", "slota-baseline", anamorph::PresetManager::Selection{});

        juce::MemoryBlock saved;
        src.getStateInformation (saved);
        auto xml  = BlobCodec::unwrap (saved);
        auto root = juce::ValueTree::fromXml (*xml);
        auto ab   = root.getChildWithName ("AB");
        check (ab.isValid() && ab.getProperty ("slotBName").toString().isNotEmpty(),
               "precondition: slot B carries a non-empty name to be contaminated by");
        const auto rejectedName = ab.getProperty ("slotBName").toString();

        ab.setProperty ("slotBParams", "<<< not xml at all >>>", nullptr);   // REJECTED payload
        // slotBName / slotBBase / slotBSource are left exactly as saved.

        AnamorphAudioProcessor dst;
        dst.prepareToPlay (48000.0, 512);
        const auto blob = BlobCodec::wrap (*root.createXml());
        dst.setStateInformation (blob.getData(), (int) blob.getSize());

        const float restoredWidth = rawOf (dst, "width");
        const auto  restoredName  = dst.getPresets().currentName();
        check (restoredName != rejectedName,
               "precondition: the rejected slot's name differs from the restored one (non-vacuity)");
        // Captured BEFORE the switch. The restored session here carries a synthetic
        // baseline string that is not a real signature, so the restored state is
        // legitimately dirty; the invariant that matters is that switching into the
        // reseeded slot does not move it, not that it is clean.
        const bool dirtyBeforeSwitch = dst.getPresets().isDirty();

        // Switching to the rejected slot must show the RESEEDED state -- both its
        // sound and its label.
        dst.abSwitchTo (1);
        checkNear ((double) rawOf (dst, "width"), (double) restoredWidth, 1.0e-4,
                   "rejected slot: the SOUND is reseeded from the restored state");
        checkStr (dst.getPresets().currentName(), restoredName,
                  "...and the NAME describes that same state, not the rejected payload's");
        check (dst.getPresets().currentName() != rejectedName,
               "...specifically, the rejected payload's preset name did not survive");
        check (dst.getPresets().isDirty() == dirtyBeforeSwitch,
               "...and switching to it does not CHANGE the dirty-star (no stranger's baseline)");
    }

    // === ER-STATE-16, the other direction: a VALID slot is unaffected ========
    {
        AnamorphAudioProcessor src;
        src.prepareToPlay (48000.0, 512);
        setRaw (src, "width", 0.20f);
        src.abSwitchTo (1);
        src.getPresets().setMeta ("Kept Name", "kept-baseline", anamorph::PresetManager::Selection{});
        setRaw (src, "width", 0.85f);
        src.abSwitchTo (0);
        juce::MemoryBlock saved;
        src.getStateInformation (saved);

        AnamorphAudioProcessor dst;
        dst.prepareToPlay (48000.0, 512);
        dst.setStateInformation (saved.getData(), (int) saved.getSize());
        dst.abSwitchTo (1);
        checkNear ((double) rawOf (dst, "width"), 0.85, 1.0e-4,
                   "a VALID slot still restores its own sound");
        checkStr (dst.getPresets().currentName(), "Kept Name",
                  "...and its own preset name -- valid sessions are byte-compatible");
    }
}

// ---------------------------------------------------------------------------
//  State test 28 -- a malformed host-hidden Setting in a legacy session resolves
//  to a VALID setting, deterministically (ER-STATE-17).
//
//  migrateFromLegacyApvts read each legacy PARAM value straight into an `(int)`
//  conversion. JUCE's parser accepts "nan" and "inf" as numbers, so a v0.2
//  session carrying one reached that conversion, which is undefined behaviour
//  for NaN, infinity and out-of-range values. Measured through the real restore
//  on x86-64 before the fix: every such value became -2147483647 in the tree,
//  re-saved with the session as an impossible ComboBox id; "2147483647" wrapped
//  to INT_MIN through a second UB (signed overflow in the `+ 1`); a finite but
//  out-of-domain "7" produced id 8; "abc" silently made UI Scale "XS" instead of
//  its default; and scopePersist passed NaN, +/-inf and out-of-range values
//  straight through to the slider. AArch64 saturates instead, so the corruption
//  was platform-dependent as well as undefined.
//
//  The contract this pins: malformed / non-finite -> the field's DEFAULT (the
//  same answer an absent node gets, through the same SerializedNumber predicate
//  the session and preset paths use); finite but out of domain -> CLAMPED to the
//  nearest valid choice; valid -> unchanged. Both legacy shapes are covered: the
//  v0.2 bare-APVTS root, and a pre-0.8.4 AnamorphRoot with no ANAMORPH_INTERNAL
//  child, which calls the same migration.
// ---------------------------------------------------------------------------
static void testMalformedLegacySettingsResolveToValid()
{
    std::printf ("State test 28: malformed legacy Settings resolve to a valid setting (ER-STATE-17)\n");

    auto restoreV02 = [] (AnamorphAudioProcessor& p, const char* id, const char* value)
    {
        juce::String xml;
        xml << "<ANAMORPH><PARAM id=\"width\" value=\"1.5\"/>"
            << "<PARAM id=\"" << id << "\" value=\"" << value << "\"/></ANAMORPH>";
        auto parsed = juce::parseXML (xml);
        const auto blob = BlobCodec::wrap (*parsed);
        p.setStateInformation (blob.getData(), (int) blob.getSize());
    };
    auto internalOf = [] (AnamorphAudioProcessor& p) { return p.getInternal().copyState(); };
    auto reSavedInt = [] (AnamorphAudioProcessor& p, const char* field)
    {
        juce::MemoryBlock mb; p.getStateInformation (mb);
        auto xml = BlobCodec::unwrap (mb);
        return (int) juce::ValueTree::fromXml (*xml).getChildWithName ("ANAMORPH_INTERNAL").getProperty (field);
    };

    // --- oversample: default index 0 -> id 1; domain ids 1..4 --------------------
    struct IntCase { const char* text; int expectId; const char* why; };
    const IntCase osCases[] = {
        { "nan",        1, "NaN -> default"                      },
        { "inf",        1, "+inf -> default"                     },
        { "-inf",       1, "-inf -> default"                     },
        { "1e39",       1, "overflows float -> default"          },
        { "-1e39",      1, "negative overflow -> default"        },
        { "abc",        1, "not a number -> default"             },
        { "",           1, "empty -> default"                    },
        { "0x10",       1, "hex text -> default"                 },
        { "7",          4, "finite, out of domain -> clamped"    },
        { "2147483647", 4, "INT_MAX -> clamped, no wrap"         },
        { "-3",         1, "finite, below domain -> clamped"     },
        { "1.0",        2, "valid index 1 -> id 2 (unchanged)"   },
        { "3",          4, "valid index 3 -> id 4 (unchanged)"   },
    };
    for (const auto& c : osCases)
    {
        AnamorphAudioProcessor p; p.prepareToPlay (48000.0, 512);
        restoreV02 (p, "oversample", c.text);
        const int id = (int) internalOf (p)[anamorph::iid::oversample];
        check (id == c.expectId, (juce::String ("oversample=\"") + c.text + "\" -> id " + juce::String (c.expectId)
                                  + " (" + c.why + "); got " + juce::String (id)).toRawUTF8());
        check (id >= 1 && id <= 4, "...and the id is inside the combo's domain");
        check (p.getInternal().oversampleIndex() == id - 1, "...and the DSP atomic agrees with the tree");
        check (reSavedInt (p, "int_oversample") == id, "...and a re-save writes that valid id, not garbage");
    }

    // --- uiScale: default index 2 -> id 3; domain ids 1..5 -----------------------
    const IntCase uiCases[] = {
        { "nan",        3, "NaN -> default"                      },
        { "inf",        3, "+inf -> default"                     },
        { "-1e39",      3, "negative overflow -> default"        },
        { "abc",        3, "not a number -> default (was XS)"    },
        { "",           3, "empty -> default (was XS)"           },
        { "7",          5, "finite, out of domain -> clamped"    },
        { "2147483647", 5, "INT_MAX -> clamped, no wrap"         },
        { "1.0",        2, "valid index 1 -> id 2 (unchanged)"   },
        { "4",          5, "valid index 4 -> id 5 (unchanged)"   },
    };
    for (const auto& c : uiCases)
    {
        AnamorphAudioProcessor p; p.prepareToPlay (48000.0, 512);
        restoreV02 (p, "uiScale", c.text);
        const int id = (int) internalOf (p)[anamorph::iid::uiScale];
        check (id == c.expectId, (juce::String ("uiScale=\"") + c.text + "\" -> id " + juce::String (c.expectId)
                                  + " (" + c.why + "); got " + juce::String (id)).toRawUTF8());
        check (p.getInternal().uiScaleIndex() == id - 1, "...and uiScaleIndex() agrees with the tree");
    }

    // --- scopePersist: default 0.5; domain 0..1 ----------------------------------
    struct DblCase { const char* text; double expect; const char* why; };
    const DblCase spCases[] = {
        { "nan",  0.5,  "NaN -> default"                 },
        { "inf",  0.5,  "+inf -> default"                },
        { "-inf", 0.5,  "-inf -> default"                },
        { "1e39", 0.5,  "overflows float -> default"     },
        { "abc",  0.5,  "not a number -> default (was 0)"},
        { "-1",   0.0,  "below domain -> clamped"        },
        { "5",    1.0,  "above domain -> clamped"        },
        { "0.25", 0.25, "valid -> unchanged"             },
    };
    for (const auto& c : spCases)
    {
        AnamorphAudioProcessor p; p.prepareToPlay (48000.0, 512);
        restoreV02 (p, "scopePersist", c.text);
        const double d = (double) internalOf (p)[anamorph::iid::scopePersist];
        checkNear (d, c.expect, 1.0e-9, (juce::String ("scopePersist=\"") + c.text + "\" (" + c.why + ")").toRawUTF8());
        check (std::isfinite ((double) p.getInternal().scopePersist()), "...and the consumer never sees a non-finite value");
    }

    // --- the OTHER legacy shape that runs the same migration: a pre-0.8.4
    //     AnamorphRoot whose sound child carries the setting and which has no
    //     ANAMORPH_INTERNAL child. Same predicate, same answer.
    {
        AnamorphAudioProcessor p; p.prepareToPlay (48000.0, 512);
        juce::String xml;
        xml << "<AnamorphRoot presetName=\"x\"><ANAMORPH>"
            << "<PARAM id=\"width\" value=\"1.5\"/><PARAM id=\"uiScale\" value=\"nan\"/>"
            << "<PARAM id=\"oversample\" value=\"1e39\"/></ANAMORPH></AnamorphRoot>";
        auto parsed = juce::parseXML (xml);
        const auto blob = BlobCodec::wrap (*parsed);
        p.setStateInformation (blob.getData(), (int) blob.getSize());
        check ((int) internalOf (p)[anamorph::iid::uiScale] == 3,
               "pre-0.8.4 AnamorphRoot path: uiScale=\"nan\" -> default id 3");
        check ((int) internalOf (p)[anamorph::iid::oversample] == 1,
               "pre-0.8.4 AnamorphRoot path: oversample=\"1e39\" -> default id 1");
    }

    // --- the REAL frozen pre-0.8.4 fixture, mutated IN PLACE. Only the six
    //     Settings PARAM values are replaced; width, mix, the preset name and
    //     baseline and both A/B slots stay exactly what the fixture carries. This
    //     proves the repair on the genuine legacy shape -- the file State test 6
    //     guards -- and that it disturbs nothing around it. Three restores: the
    //     file untouched (State test 6's values re-asserted here so this leg is
    //     self-contained), malformed text in every Setting, and finite values
    //     outside every domain.
    {
        auto loadFixtureRoot = []
        {
            auto xml = juce::parseXML (fixtureDir().getChildFile ("legacy_pre_0_8_4_view_params.xml"));
            return xml != nullptr ? juce::ValueTree::fromXml (*xml) : juce::ValueTree();
        };
        auto setSetting = [] (juce::ValueTree& root, const char* id, const char* text)
        {
            auto sound = root.getChildWithName ("ANAMORPH");
            for (int i = 0; i < sound.getNumChildren(); ++i)
            {
                auto c = sound.getChild (i);
                if (c.hasType ("PARAM") && c.getProperty ("id").toString() == id)
                    c.setProperty ("value", juce::String (text), nullptr);
            }
        };
        auto restoreRoot = [] (AnamorphAudioProcessor& p, const juce::ValueTree& root)
        {
            const auto blob = BlobCodec::wrap (*root.createXml());
            p.setStateInformation (blob.getData(), (int) blob.getSize());
        };
        auto surroundingsIntact = [] (AnamorphAudioProcessor& p, const char* leg)
        {
            const juce::String tag (leg);
            auto* w = dynamic_cast<juce::RangedAudioParameter*> (p.getAPVTS().getParameter ("width"));
            auto* m = dynamic_cast<juce::RangedAudioParameter*> (p.getAPVTS().getParameter ("mix"));
            checkNear ((double) w->getValue(), (double) w->convertTo0to1 (0.8f),  1.0e-6, (tag + ": width 0.8 still restores").toRawUTF8());
            checkNear ((double) m->getValue(), (double) m->convertTo0to1 (0.65f), 1.0e-6, (tag + ": mix 0.65 still restores").toRawUTF8());
            checkStr (p.getPresets().currentName(), "My Vocal", (tag + ": preset name still restores").toRawUTF8());
            check (p.abActiveSlot() == 0, (tag + ": active slot still restores").toRawUTF8());
            juce::MemoryBlock mb; p.getStateInformation (mb);
            auto saved = juce::ValueTree::fromXml (*BlobCodec::unwrap (mb));
            checkStr (saved.getChildWithName ("AB")["slotBName"].toString(), "Slot B Preset",
                      (tag + ": slot B name still survives a re-save").toRawUTF8());
            const auto savedInternal = saved.getChildWithName ("ANAMORPH_INTERNAL");
            const int os = (int) savedInternal[anamorph::iid::oversample], ui = (int) savedInternal[anamorph::iid::uiScale];
            check (os >= 1 && os <= 4 && ui >= 1 && ui <= 5, (tag + ": re-save writes in-domain Settings ids").toRawUTF8());
        };

        auto root = loadFixtureRoot();
        check (root.isValid() && root.hasType ("AnamorphRoot")
                 && ! root.getChildWithName ("ANAMORPH_INTERNAL").isValid(),
               "real fixture: is an AnamorphRoot with NO ANAMORPH_INTERNAL child (the pre-0.8.4 shape)");
        if (root.isValid())
        {
            // (a) untouched -- the fix must not move valid migration on the real file
            {
                AnamorphAudioProcessor p; p.prepareToPlay (48000.0, 512);
                restoreRoot (p, root);
                auto t = internalOf (p);
                check ((int)  t[anamorph::iid::oversample] == 3,   "real fixture untouched: oversample idx 2 -> id 3");
                check ((int)  t[anamorph::iid::uiScale]    == 2,   "real fixture untouched: uiScale idx 1 -> id 2");
                checkNear ((double) t[anamorph::iid::scopePersist], 0.25, 1.0e-9, "real fixture untouched: scopePersist 0.25");
                check ((bool) t[anamorph::iid::metersOn]   == false, "real fixture untouched: metersOn false");
                check ((bool) t[anamorph::iid::tooltipsOn] == true,  "real fixture untouched: tooltipsOn true");
                check ((bool) t[anamorph::iid::uiAnimations] == true, "real fixture untouched: uiAnimations true");
                surroundingsIntact (p, "real fixture untouched");
            }
            // (b) every Setting malformed, in place
            {
                auto bad = root.createCopy();
                setSetting (bad, "oversample",   "nan");
                setSetting (bad, "uiScale",      "1e39");
                setSetting (bad, "scopePersist", "inf");
                setSetting (bad, "metersOn",     "abc");
                setSetting (bad, "tooltipsOn",   "nan");
                setSetting (bad, "uiAnimations", "-inf");
                AnamorphAudioProcessor p; p.prepareToPlay (48000.0, 512);
                restoreRoot (p, bad);
                auto t = internalOf (p);
                check ((int)  t[anamorph::iid::oversample] == 1,   "real fixture, oversample=\"nan\"    -> default id 1");
                check ((int)  t[anamorph::iid::uiScale]    == 3,   "real fixture, uiScale=\"1e39\"      -> default id 3");
                checkNear ((double) t[anamorph::iid::scopePersist], 0.5, 1.0e-9, "real fixture, scopePersist=\"inf\" -> default 0.5");
                check ((bool) t[anamorph::iid::metersOn]   == false, "real fixture, metersOn=\"abc\"     -> default false");
                check ((bool) t[anamorph::iid::tooltipsOn] == false, "real fixture, tooltipsOn=\"nan\"   -> default false (was true in the file)");
                check ((bool) t[anamorph::iid::uiAnimations] == true, "real fixture, uiAnimations=\"-inf\" -> default true");
                check (p.getInternal().oversampleIndex() == 0, "real fixture, malformed: the DSP atomic agrees with the tree");
                surroundingsIntact (p, "real fixture, malformed Settings");
            }
            // (c) finite but outside every domain, in place
            {
                auto far = root.createCopy();
                setSetting (far, "oversample",   "7");
                setSetting (far, "uiScale",      "7");
                setSetting (far, "scopePersist", "5");
                AnamorphAudioProcessor p; p.prepareToPlay (48000.0, 512);
                restoreRoot (p, far);
                auto t = internalOf (p);
                check ((int)  t[anamorph::iid::oversample] == 4,   "real fixture, oversample=\"7\"  -> clamped to id 4");
                check ((int)  t[anamorph::iid::uiScale]    == 5,   "real fixture, uiScale=\"7\"     -> clamped to id 5");
                checkNear ((double) t[anamorph::iid::scopePersist], 1.0, 1.0e-9, "real fixture, scopePersist=\"5\" -> clamped to 1.0");
                check (p.getInternal().oversampleIndex() == 3, "real fixture, clamped: the DSP atomic agrees with the tree");
                surroundingsIntact (p, "real fixture, out-of-domain Settings");
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  Opt-in probe: does a legacy restore on a REUSED instance leave the previous
//  project's A/B slots in place? Reproduction BEFORE any disposition.
// ---------------------------------------------------------------------------
static int runLegacyAbProbe()
{
    std::printf ("legacy A/B contamination probe\n");
    std::printf ("=============================\n\n");

    auto widthOf = [] (AnamorphAudioProcessor& p) { return rawOf (p, "width"); };

    // --- Step 1: the "previous project", with DISTINGUISHABLE A and B sounds.
    AnamorphAudioProcessor p;
    p.prepareToPlay (48000.0, 512);

    setRaw (p, "width", 0.90f);                 // slot A sound
    p.abSwitchTo (1);                           // stores A, moves to B
    setRaw (p, "width", 0.10f);                 // slot B sound -- clearly different
    p.abSwitchTo (0);                           // back to A
    std::printf ("  previous project: A width raw %.4f, B width raw %.4f (active %d)\n",
                 widthOf (p), 0.10f, p.abActiveSlot());
    const float prevA = 0.90f, prevB = 0.10f;

    // --- Step 2: restore a v0.2 session into the SAME instance.
    if (! applyXmlFixture (p, "legacy_v0_2_bare_apvts.xml"))
        return 1;
    const float restored = widthOf (p);
    std::printf ("  after the v0.2 restore: live width raw %.4f (active %d)\n",
                 restored, p.abActiveSlot());

    // The fixture's width is 1.5 (denormalised); confirm it is distinguishable
    // from BOTH previous-project slot values, or the probe cannot see anything.
    const bool distinguishable = std::abs (restored - prevA) > 0.02f
                              && std::abs (restored - prevB) > 0.02f;
    std::printf ("  restored value distinguishable from both previous slots: %s\n",
                 distinguishable ? "yes" : "NO -- probe would be vacuous");
    if (! distinguishable) return 1;

    // --- Step 3: switch slots and read back.
    p.abSwitchTo (1);
    const float afterToB = widthOf (p);
    std::printf ("  after switching to B: width raw %.4f\n", afterToB);
    p.abSwitchTo (0);
    const float afterToA = widthOf (p);
    std::printf ("  after switching back to A: width raw %.4f\n", afterToA);

    const bool bStale = std::abs (afterToB - prevB) < 0.02f;
    const bool aStale = std::abs (afterToA - prevA) < 0.02f;
    std::printf ("\n  slot B carries the PREVIOUS project's sound: %s\n", bStale ? "YES" : "no");
    std::printf ("  slot A carries the PREVIOUS project's sound: %s\n", aStale ? "YES" : "no");
    std::printf ("  => %s\n", (bStale || aStale) ? "CONFIRMED: stale prior-project A/B state"
                                                 : "REFUTED: slots follow the restore");

    // --- Step 3b: the same question for slot A. Switching to B above STORED the
    // restored state into A, so step 3 can never see a stale A. Reach it by leaving
    // the previous project active on B, so the first switch after the restore is the
    // one that reads A.
    {
        AnamorphAudioProcessor r;
        r.prepareToPlay (48000.0, 512);
        setRaw (r, "width", 0.90f);            // A
        r.abSwitchTo (1);
        setRaw (r, "width", 0.10f);            // B -- and STAY on B
        if (! applyXmlFixture (r, "legacy_v0_2_bare_apvts.xml"))
            return 1;
        std::printf ("\n  previous project left active on B; after restore active = %d,"
                     " live width raw %.4f\n", r.abActiveSlot(), widthOf (r));
        r.abSwitchTo (0);
        const float rA = widthOf (r);
        std::printf ("  first switch after the restore reads A: width raw %.4f -> %s\n", rA,
                     std::abs (rA - prevA) < 0.02f ? "CONFIRMED stale" : "follows the restore");
    }

    // --- Step 4: the same question for an AnamorphRoot session with NO AB child.
    std::printf ("\n  --- AnamorphRoot with no AB child (same class of path) ---\n");
    AnamorphAudioProcessor q;
    q.prepareToPlay (48000.0, 512);
    setRaw (q, "width", 0.90f);
    q.abSwitchTo (1);
    setRaw (q, "width", 0.10f);
    q.abSwitchTo (0);

    // Build a current-format blob, then strip its AB child.
    juce::MemoryBlock mb;
    q.getStateInformation (mb);
    auto xml = BlobCodec::unwrap (mb);
    auto root = juce::ValueTree::fromXml (*xml);
    root.removeChild (root.getChildWithName ("AB"), nullptr);
    setRaw (q, "width", 0.55f);                  // make the live value differ again
    juce::MemoryBlock stripped = BlobCodec::wrap (*root.createXml());
    q.setStateInformation (stripped.getData(), (int) stripped.getSize());
    std::printf ("  after restoring an AB-less root: live width raw %.4f\n", widthOf (q));
    q.abSwitchTo (1);
    const float qB = widthOf (q);
    std::printf ("  after switching to B: width raw %.4f -> %s\n", qB,
                 std::abs (qB - prevB) < 0.02f ? "CONFIRMED stale" : "follows the restore");
    return 0;
}

// ---------------------------------------------------------------------------
//  Opt-in probe: does a restore that carries no A/B data leave the PREVIOUS
//  project's per-slot Level-Match gains in place, and does the first slot
//  switch after it move the OUTPUT LEVEL? Reproduction before disposition.
// ---------------------------------------------------------------------------
static int runLegacyMatchGainProbe()
{
    std::printf ("legacy per-slot Level-Match contamination probe\n");
    std::printf ("==============================================\n\n");

    constexpr double sr = 48000.0;
    constexpr int    bs = 512;

    // Deterministic noise, so the loudness measurement has something real to
    // converge on and the two slots' matches are genuinely measured, not injected.
    auto fillNoise = [] (juce::AudioBuffer<float>& b, juce::Random& rng)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * 0.25f);
    };
    auto runBlocks = [&] (AnamorphAudioProcessor& p, int nBlocks, juce::Random& rng)
    {
        juce::AudioBuffer<float> buf (2, bs);
        juce::MidiBuffer midi;
        for (int k = 0; k < nBlocks; ++k)
        {
            fillNoise (buf, rng);
            p.processBlock (buf, midi);
        }
    };
    auto rmsOf = [&] (AnamorphAudioProcessor& p, int nBlocks, juce::Random& rng)
    {
        juce::AudioBuffer<float> buf (2, bs);
        juce::MidiBuffer midi;
        double acc = 0.0; int n = 0;
        for (int k = 0; k < nBlocks; ++k)
        {
            fillNoise (buf, rng);
            p.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < bs; ++i) { const double s = buf.getSample (ch, i); acc += s * s; ++n; }
        }
        return std::sqrt (acc / (double) juce::jmax (1, n));
    };

    AnamorphAudioProcessor p;
    p.prepareToPlay (sr, bs);
    juce::Random rng (20260901);

    setRaw (p, "autoGainMatch", 1.0f);   // Level Match ON -- the gate the gain is applied behind

    // --- The "previous project": two slots whose measured matches genuinely differ.
    setRaw (p, "width", 0.95f);          // a wide setting -> one match figure
    runBlocks (p, 60, rng);
    const float prevMatchA = p.getEngine().getMatchGainDb();
    p.abSwitchTo (1);                    // stores A's match, moves to B
    setRaw (p, "width", 0.05f);          // a narrow setting -> a different one
    runBlocks (p, 60, rng);
    const float prevMatchB = p.getEngine().getMatchGainDb();
    p.abSwitchTo (0);                    // stores B's match, back to A
    runBlocks (p, 20, rng);
    std::printf ("  previous project: measured match A %.3f dB, B %.3f dB (delta %.3f dB)\n",
                 prevMatchA, prevMatchB, prevMatchB - prevMatchA);
    if (std::abs (prevMatchB - prevMatchA) < 0.5f)
    {
        std::printf ("  the two slots' matches are NOT distinguishable -- probe would be vacuous\n");
        return 1;
    }

    // --- Restore a session with NO A/B data into the SAME instance.
    if (! applyXmlFixture (p, "legacy_v0_2_bare_apvts.xml"))
        return 1;
    setRaw (p, "autoGainMatch", 1.0f);   // the v0.2 fixture predates the param; keep the gate open
    runBlocks (p, 60, rng);
    const float afterRestore = p.getEngine().getMatchGainDb();
    std::printf ("  after the v0.2 restore + settle: engine match %.3f dB\n", afterRestore);

    // --- The first A/B switch after the restore. The injection is consumed INSIDE
    // processBlock (at the silent bottom of the switch duck, ~6 ms out + 28 ms in),
    // so reading getMatchGainDb() before any block has run reads the pre-switch
    // loudness value and sees nothing -- the first version of this probe did
    // exactly that and reported "not the stale value" for a stale value that WAS
    // there. Print the trajectory instead of a single sample.
    p.abSwitchTo (1);
    std::printf ("  first switch to B -- engine match after N blocks:\n");
    float atBlock4 = 0.0f;
    for (int k = 1; k <= 8; ++k)
    {
        runBlocks (p, 1, rng);
        const float m = p.getEngine().getMatchGainDb();
        if (k == 4) atBlock4 = m;
        std::printf ("      %d block(s): %.3f dB\n", k, m);
    }
    std::printf ("  stale previous-project B match was %.3f dB; restored-state match was %.3f dB\n",
                 prevMatchB, afterRestore);
    std::printf ("  => %s\n",
                 std::abs (atBlock4 - prevMatchB) < 0.05f
                     ? "CONFIRMED: the stale previous-project value was injected"
                     : "the injected value is not the stale one");

    // --- Output level, with the switch DUCK excluded. abSwitchTo requests a duck
    // (~34 ms = 3.2 blocks at 512/48k), so an RMS window that starts at the switch
    // measures the duck, not the match gain -- the first version of this probe did
    // that too and reported a -2.60 dB "level change" that was the fade. Compare a
    // settled window instead, against a CONTROL instance that restored the same
    // session with no previous project behind it.
    const double contaminated = rmsOf (p, 20, rng);

    AnamorphAudioProcessor q;                 // control: no previous project
    q.prepareToPlay (sr, bs);
    juce::Random rngQ (20260901);
    setRaw (q, "autoGainMatch", 1.0f);
    if (! applyXmlFixture (q, "legacy_v0_2_bare_apvts.xml"))
        return 1;
    setRaw (q, "autoGainMatch", 1.0f);
    runBlocks (q, 60, rngQ);
    q.abSwitchTo (1);
    runBlocks (q, 8, rngQ);
    const double control = rmsOf (q, 20, rngQ);

    std::printf ("  settled output RMS after the switch: contaminated %.6f, control %.6f"
                 " (ratio %.4f, %.2f dB)\n",
                 contaminated, control, contaminated / control,
                 20.0 * std::log10 (juce::jmax (1.0e-12, contaminated / control)));
    std::printf ("  control instance's match after its switch: %.3f dB\n",
                 q.getEngine().getMatchGainDb());

    // --- DISCRIMINATION. The readings above show a jump at block 2 but cannot say
    // WHOSE value it is, because the loudness module re-measures on top of the
    // injection within the same block. Two changes make it decisive: switch under
    // SILENCE, so the measurement has nothing to move toward, and run the whole
    // scenario twice with DIFFERENT previous-project B settings. If the value read
    // after the switch tracks the previous project's B across both runs, it is that
    // value being injected and nothing else.
    std::printf ("\n  --- discrimination: silent switch, two different previous-project Bs ---\n");
    auto runScenario = [&] (float prevBWidth, const char* label)
    {
        AnamorphAudioProcessor z;
        z.prepareToPlay (sr, bs);
        juce::Random r (4242);
        setRaw (z, "autoGainMatch", 1.0f);
        setRaw (z, "width", 0.95f);
        runBlocks (z, 60, r);
        z.abSwitchTo (1);
        setRaw (z, "width", prevBWidth);
        runBlocks (z, 60, r);
        const float bMatch = z.getEngine().getMatchGainDb();
        z.abSwitchTo (0);
        runBlocks (z, 20, r);

        if (! applyXmlFixture (z, "legacy_v0_2_bare_apvts.xml")) return;
        setRaw (z, "autoGainMatch", 1.0f);
        runBlocks (z, 60, r);
        const float restored = z.getEngine().getMatchGainDb();

        // Silence from here: nothing for the loudness module to re-measure toward.
        juce::AudioBuffer<float> quiet (2, bs);
        juce::MidiBuffer midi;
        z.abSwitchTo (1);
        std::printf ("      %-22s prev-project B %+.3f dB | restored %+.3f dB | per-block:",
                     label, bMatch, restored);
        float peak = restored;
        for (int k = 0; k < 6; ++k)
        {
            quiet.clear(); z.processBlock (quiet, midi);
            const float m = z.getEngine().getMatchGainDb();
            std::printf (" %+.3f", m);
            if (std::abs (m - restored) > std::abs (peak - restored)) peak = m;
        }
        std::printf ("\n      %-22s the excursion peak is %+.3f dB -> %s\n", "",
                     peak,
                     std::abs (peak - bMatch) < 0.10f ? "== the STALE previous-project B"
                   : std::abs (peak - restored) < 0.10f ? "== the restored state"
                                                        : "neither");
    };
    runScenario (0.05f, "narrow previous B:");
    runScenario (0.60f, "mid previous B:");

    // --- MATCHED COUNTERFACTUAL, same instance, same audio history. After round 8's
    // fix a no-A/B restore leaves BOTH slots holding the restored state, so an
    // A->B switch applies IDENTICAL parameters -- the only thing that differs
    // between the slots is the injected match gain. And A->B->A decontaminates the
    // array by construction (each switch stores the CURRENT match into the slot it
    // leaves), so a later A->B on the same instance is the clean control. That
    // removes every confound the comparisons above carry: same instance, same
    // loudness history, same parameters, one variable.
    std::printf ("\n  --- matched counterfactual on one instance (params identical across the switch) ---\n");
    {
        AnamorphAudioProcessor z;
        z.prepareToPlay (sr, bs);
        juce::Random r (77000);
        setRaw (z, "autoGainMatch", 1.0f);
        setRaw (z, "width", 0.95f);
        runBlocks (z, 60, r);
        z.abSwitchTo (1);
        setRaw (z, "width", 0.05f);
        runBlocks (z, 60, r);
        z.abSwitchTo (0);
        runBlocks (z, 20, r);
        if (! applyXmlFixture (z, "legacy_v0_2_bare_apvts.xml")) return 1;
        setRaw (z, "autoGainMatch", 1.0f);
        runBlocks (z, 80, r);

        auto envelopeAfterSwitch = [&] (int to, const char* label)
        {
            juce::AudioBuffer<float> buf (2, bs);
            juce::MidiBuffer midi;
            z.abSwitchTo (to);
            std::printf ("      %-16s per-block output RMS:", label);
            for (int k = 0; k < 10; ++k)
            {
                fillNoise (buf, r);
                z.processBlock (buf, midi);
                double acc = 0.0;
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < bs; ++i) { const double v = buf.getSample (ch, i); acc += v * v; }
                std::printf (" %.4f", std::sqrt (acc / (double) (2 * bs)));
            }
            std::printf ("\n");
        };
        envelopeAfterSwitch (1, "CONTAMINATED");   // injects the previous project's B
        runBlocks (z, 40, r);
        envelopeAfterSwitch (0, "(decontaminate)");
        runBlocks (z, 40, r);
        envelopeAfterSwitch (1, "CLEAN CONTROL");  // same switch, array now decontaminated
    }

    // --- WORST CASE: switch IMMEDIATELY after the restore, before the loudness
    // module has measured anything on the restored settings. That is the one
    // configuration in which the injected value could persist rather than being
    // superseded -- if the running measurement has not yet converged, there is
    // nothing to overwrite it with. If the level does not move here either, the
    // stale value is inert everywhere.
    std::printf ("\n  --- worst case: switch with NO settle after the restore ---\n");
    {
        auto worstCase = [&] (bool withPreviousProject, const char* label)
        {
            AnamorphAudioProcessor z;
            z.prepareToPlay (sr, bs);
            juce::Random r (31337);
            setRaw (z, "autoGainMatch", 1.0f);
            if (withPreviousProject)
            {
                setRaw (z, "width", 0.95f);
                runBlocks (z, 60, r);
                z.abSwitchTo (1);
                setRaw (z, "width", 0.05f);
                runBlocks (z, 60, r);
                z.abSwitchTo (0);
                runBlocks (z, 20, r);
            }
            if (! applyXmlFixture (z, "legacy_v0_2_bare_apvts.xml")) return;
            setRaw (z, "autoGainMatch", 1.0f);
            z.abSwitchTo (1);                       // no settle at all
            juce::AudioBuffer<float> buf (2, bs);
            juce::MidiBuffer midi;
            std::printf ("      %-18s RMS:", label);
            for (int k = 0; k < 12; ++k)
            {
                fillNoise (buf, r);
                z.processBlock (buf, midi);
                double acc = 0.0;
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < bs; ++i) { const double v = buf.getSample (ch, i); acc += v * v; }
                std::printf (" %.4f", std::sqrt (acc / (double) (2 * bs)));
            }
            std::printf ("   match %+.3f dB\n", z.getEngine().getMatchGainDb());
        };
        worstCase (true,  "with prev project");
        worstCase (false, "fresh (control)");
    }

    // --- ROUND 12: is Level Match still ON after the switch? It must be, or every
    // "inert" reading above measured an engine that was not applying the gain at
    // all. Recorded here because the first reading of this looked alarming and was
    // MISREAD: the value tree's @value for autoGainMatch is stale (JUCE flushes it
    // from its own 10 Hz timer, which this harness never fires), so the print below
    // shows tree 0.0 against live 1.0. That is not what the slot carries.
    // currentStateSet() -> copyStateWithRawValues() writes a fresh @raw for every
    // PARAM from the LIVE parameter, and reassertParameters reads @raw first, so
    // the snapshot is faithful. Match going off in the minimal case below is
    // correct A/B behaviour, not a flush defect: slot B still holds the
    // construction snapshot, which predates enabling Match.
    std::printf ("\n  --- round 12: does the switch keep Level Match ON? ---\n");
    {
        AnamorphAudioProcessor z; z.prepareToPlay (sr, bs);
        setRaw (z, "autoGainMatch", 1.0f);
        auto raw = [&] { return z.getAPVTS().getRawParameterValue (pid::autoGainMatch)->load(); };
        const float treeBefore = (float) (double) z.getAPVTS().state.getChildWithProperty ("id", pid::autoGainMatch).getProperty ("value");
        std::printf ("      before any switch: live param %.1f, VALUE TREE says %.1f\n", raw(), treeBefore);
        z.abSwitchTo (1);
        std::printf ("      after A->B:        live param %.1f  <- %s\n", raw(),
                     raw() > 0.5f ? "still on"
                                  : "off, CORRECTLY: slot B predates the Match enable (not a flush defect -- @raw is fresh)");
    }

    // --- ROUND 12: the matched counterfactual again, with Level Match carried BY
    // THE RESTORED SESSION so the reseeded slots carry it (the tree has it, so no
    // flush is needed) -- the state a real host is in.
    std::printf ("\n  --- round 12: matched counterfactual with autoGainMatch baked into the restored session ---\n");
    {
        AnamorphAudioProcessor z; z.prepareToPlay (sr, bs);
        juce::Random r (77000);
        setRaw (z, "autoGainMatch", 1.0f);
        setRaw (z, "width", 0.95f); runBlocks (z, 60, r);
        z.abSwitchTo (1);
        setRaw (z, "width", 0.05f); runBlocks (z, 60, r);
        const float prevB = z.getEngine().getMatchGainDb();
        z.abSwitchTo (0); runBlocks (z, 20, r);

        juce::String xml = "<ANAMORPH><PARAM id=\"drive\" value=\"6.0\"/><PARAM id=\"algorithm\" value=\"2.0\"/>"
                           "<PARAM id=\"width\" value=\"1.5\"/><PARAM id=\"mix\" value=\"0.8\"/>"
                           "<PARAM id=\"haasDelay\" value=\"20.0\"/><PARAM id=\"outputGain\" value=\"-3.0\"/>"
                           "<PARAM id=\"autoGainMatch\" value=\"1\"/></ANAMORPH>";
        auto parsed = juce::parseXML (xml); const auto blob = BlobCodec::wrap (*parsed);
        z.setStateInformation (blob.getData(), (int) blob.getSize());
        runBlocks (z, 80, r);
        const float restored = z.getEngine().getMatchGainDb();
        std::printf ("      previous project's B match %+.3f dB; restored-material match %+.3f dB; live autoGainMatch %.1f\n",
                     prevB, restored, z.getAPVTS().getRawParameterValue (pid::autoGainMatch)->load());

        auto envelope = [&] (int to, const char* label)
        {
            juce::AudioBuffer<float> buf (2, bs); juce::MidiBuffer midi;
            z.abSwitchTo (to);
            std::printf ("      %-16s autoGainMatch after switch %.1f | RMS:", label,
                         z.getAPVTS().getRawParameterValue (pid::autoGainMatch)->load());
            for (int k = 0; k < 10; ++k)
            {
                fillNoise (buf, r); z.processBlock (buf, midi);
                double acc = 0.0;
                for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < bs; ++i) { const double v = buf.getSample (ch, i); acc += v * v; }
                std::printf (" %.4f", std::sqrt (acc / (double) (2 * bs)));
            }
            std::printf ("  | match now %+.3f\n", z.getEngine().getMatchGainDb());
        };
        envelope (1, "CONTAMINATED");   // injects prevB into material measuring `restored`
        runBlocks (z, 40, r);
        envelope (0, "(decontaminate)");
        runBlocks (z, 40, r);
        envelope (1, "CLEAN CONTROL");
    }
    return 0;
}


static const juce::Identifier& iid_oversample()   { return anamorph::iid::oversample; }
static const juce::Identifier& iid_uiScale()      { return anamorph::iid::uiScale; }
static const juce::Identifier& iid_scopePersist() { return anamorph::iid::scopePersist; }

// ---------------------------------------------------------------------------
//  Opt-in probe: what does a MALFORMED host-hidden setting in a v0.2 session
//  become after migrateFromLegacyApvts? Reproduction before disposition.
//  Prints the migrated tree value (as text and as int/double), the clamped
//  consumers, and what a re-save then writes back out.
// ---------------------------------------------------------------------------
static int runLegacySettingsProbe()
{
    std::printf ("malformed legacy Settings probe (v0.2 -> migrateFromLegacyApvts)\n");
    std::printf ("===============================================================\n\n");

    auto restoreV02 = [] (AnamorphAudioProcessor& p, const char* id, const char* value)
    {
        juce::String xml;
        xml << "<ANAMORPH><PARAM id=\"width\" value=\"1.5\"/>"
            << "<PARAM id=\"" << id << "\" value=\"" << value << "\"/></ANAMORPH>";
        auto parsed = juce::parseXML (xml);
        const auto blob = BlobCodec::wrap (*parsed);
        p.setStateInformation (blob.getData(), (int) blob.getSize());
    };
    auto savedText = [] (AnamorphAudioProcessor& p, const char* field)
    {
        juce::MemoryBlock mb; p.getStateInformation (mb);
        auto xml  = BlobCodec::unwrap (mb);
        auto root = juce::ValueTree::fromXml (*xml);
        return root.getChildWithName ("ANAMORPH_INTERNAL").getProperty (field).toString();
    };

    const char* cases[] = { "nan", "inf", "-inf", "1e39", "-1e39", "abc", "", "0x10", "7", "2147483647", "1.0" };

    std::printf ("  %-12s | %-24s | %-13s | %-10s | %s\n", "oversample=", "int_oversample (tree)", "osIndex()", "re-saved", "note");
    for (auto* c : cases)
    {
        AnamorphAudioProcessor p; p.prepareToPlay (48000.0, 512);
        restoreV02 (p, "oversample", c);
        const auto v = p.getInternal().copyState()[iid_oversample()];
        std::printf ("  %-12s | %-24s | %-13d | %-10s | %s\n", juce::String ("\"" + juce::String (c) + "\"").toRawUTF8(),
                     (v.toString() + "  (int " + juce::String ((int) v) + ")").toRawUTF8(),
                     p.getInternal().oversampleIndex(), savedText (p, "int_oversample").toRawUTF8(),
                     ((int) v >= 1 && (int) v <= 4) ? "valid combo id" : "INVALID combo id (1..4)");
    }
    std::printf ("\n  %-12s | %-24s | %-13s | %s\n", "uiScale=", "int_uiScale (tree)", "uiScaleIndex()", "note");
    for (auto* c : cases)
    {
        AnamorphAudioProcessor p; p.prepareToPlay (48000.0, 512);
        restoreV02 (p, "uiScale", c);
        const auto v = p.getInternal().copyState()[iid_uiScale()];
        std::printf ("  %-12s | %-24s | %-13d | %s\n", juce::String ("\"" + juce::String (c) + "\"").toRawUTF8(),
                     (v.toString() + "  (int " + juce::String ((int) v) + ")").toRawUTF8(),
                     p.getInternal().uiScaleIndex(),
                     ((int) v >= 1 && (int) v <= 5) ? "valid combo id" : "INVALID combo id (1..5)");
    }
    const char* dcases[] = { "nan", "inf", "-inf", "-1", "5", "abc", "1e39", "0.25" };
    std::printf ("\n  %-12s | %-20s | %-14s | %s\n", "scopePersist=", "tree (double)", "scopePersist()", "note");
    for (auto* c : dcases)
    {
        AnamorphAudioProcessor p; p.prepareToPlay (48000.0, 512);
        restoreV02 (p, "scopePersist", c);
        const double d = (double) p.getInternal().copyState()[iid_scopePersist()];
        std::printf ("  %-12s | %-20.6g | %-14.6g | %s\n", juce::String ("\"" + juce::String (c) + "\"").toRawUTF8(),
                     d, (double) p.getInternal().scopePersist(),
                     (std::isfinite (d) && d >= 0.0 && d <= 1.0) ? "in 0..1" : "OUT of 0..1 / non-finite");
    }
    return 0;
}



// ---------------------------------------------------------------------------
//  State test 29 -- a modern session that OMITS an optional host-hidden Setting
//  must not leave the previous project's value in force (ER-STATE-18).
//
//  `abSlot[]`, `presets` and `internal` are all processor members a host restores
//  into ONE live instance repeatedly. Rounds 2, 8 and 11 closed that class for the
//  Settings on the v0.2 path, for the A/B slots, and for a root with no sound
//  child. This is the same class on the MODERN path: InternalState::restoreState
//  wrote only the fields `src` carried, so an absent one kept whatever the last
//  project left. Every field in the registry's ANAMORPH_INTERNAL table is
//  "Required: No" with a documented Default, so absent means that default.
//
//  The review that raised this located it in migrateFromLegacyApvts. The probe
//  (--partial-settings-probe) measured the opposite: 6 of 6 inherited on the
//  modern path, 0 of 6 on the legacy one. Leg 3 pins the legacy path so the two
//  cannot be confused again.
// ---------------------------------------------------------------------------
static void testPartialSettingsDoNotInherit()
{
    std::printf ("State test 29: a modern session omitting a Setting resets it (ER-STATE-18)\n");

    // check() takes a const char*; these labels are built per field.
    auto checkMsg = [] (bool ok, const juce::String& msg) { check (ok, msg.toRawUTF8()); };

    struct Field { const juce::Identifier& id; const char* name; juce::var sessionA; juce::var doc; };
    const Field fields[] = {
        { anamorph::iid::oversample,   "int_oversample",   juce::var (3),     juce::var (1)     },
        { anamorph::iid::uiScale,      "int_uiScale",      juce::var (5),     juce::var (3)     },
        { anamorph::iid::scopePersist, "int_scopePersist", juce::var (0.9),   juce::var (0.5)   },
        { anamorph::iid::metersOn,     "int_metersOn",     juce::var (true),  juce::var (false) },
        { anamorph::iid::tooltipsOn,   "int_tooltipsOn",   juce::var (true),  juce::var (false) },
        { anamorph::iid::uiAnimations, "int_uiAnimations", juce::var (false), juce::var (true)  },
    };

    // Session B is a REAL modern save with exactly one ANAMORPH_INTERNAL property
    // removed -- the sound child, the AB node and the preset meta are whatever the
    // plug-in itself wrote, so only the omission is synthetic.
    auto saveOmitting = [] (const juce::Identifier* omit)
    {
        AnamorphAudioProcessor b; b.prepareToPlay (48000.0, 512);
        juce::MemoryBlock mb; b.getStateInformation (mb);
        auto xml  = BlobCodec::unwrap (mb);
        auto root = juce::ValueTree::fromXml (*xml);
        if (omit != nullptr) root.getChildWithName ("ANAMORPH_INTERNAL").removeProperty (*omit, nullptr);
        return BlobCodec::wrap (*root.createXml());
    };
    auto seedPreviousProject = [&fields] (AnamorphAudioProcessor& p)
    {
        juce::ValueTree a ("ANAMORPH_INTERNAL");
        for (const auto& g : fields) a.setProperty (g.id, g.sessionA, nullptr);
        p.getInternal().restoreState (a);
    };

    // --- Leg 1: each field in turn, omitted from an otherwise real modern save.
    for (const auto& f : fields)
    {
        AnamorphAudioProcessor p; p.prepareToPlay (48000.0, 512);
        seedPreviousProject (p);
        checkMsg (! p.getInternal().copyState()[f.id].equals (f.doc),
               juce::String ("precondition: session A's ") + f.name + " differs from the default");

        const auto blob = saveOmitting (&f.id);
        p.setStateInformation (blob.getData(), (int) blob.getSize());
        const auto after = p.getInternal().copyState()[f.id];
        checkMsg (after.equals (f.doc),
               juce::String ("omitted ") + f.name + " resets to its documented default, not the previous project's");

        // The re-save must carry the reset value, not the inherited one.
        juce::MemoryBlock out; p.getStateInformation (out);
        auto outRoot = juce::ValueTree::fromXml (*BlobCodec::unwrap (out));
        checkMsg (outRoot.getChildWithName ("ANAMORPH_INTERNAL").getProperty (f.id).equals (f.doc),
               juce::String ("...and the re-saved session carries the reset ") + f.name);
    }

    // --- Leg 2: a session that DOES carry the field still restores that value.
    // Without this the fix could pass leg 1 by resetting everything always.
    {
        AnamorphAudioProcessor src; src.prepareToPlay (48000.0, 512);
        juce::ValueTree explicitA ("ANAMORPH_INTERNAL");
        for (const auto& g : fields) explicitA.setProperty (g.id, g.sessionA, nullptr);
        src.getInternal().restoreState (explicitA);
        juce::MemoryBlock saved; src.getStateInformation (saved);

        AnamorphAudioProcessor dst; dst.prepareToPlay (48000.0, 512);   // at defaults
        dst.setStateInformation (saved.getData(), (int) saved.getSize());
        for (const auto& f : fields)
            checkMsg (dst.getInternal().copyState()[f.id].equals (f.sessionA),
                   juce::String ("a session that CARRIES ") + f.name + " still restores that value");
        check (dst.getInternal().oversampleIndex() == 2,
               "...and the DSP atomic follows the explicitly restored oversample (id 3 -> index 2)");
    }

    // --- Leg 3: the LEGACY path still resets all six, as it already did. This is
    // where the review looked; pinning it keeps the two paths distinguishable.
    {
        AnamorphAudioProcessor p; p.prepareToPlay (48000.0, 512);
        seedPreviousProject (p);
        if (applyXmlFixture (p, "legacy_v0_2_bare_apvts.xml"))
            for (const auto& f : fields)
                checkMsg (p.getInternal().copyState()[f.id].equals (f.doc),
                       juce::String ("v0.2 restore still resets ") + f.name + " (migrateFromLegacyApvts, unchanged)");
    }

    // --- Leg 4: malformed-state repair is unchanged. A modern session carrying a
    // malformed Setting is NOT the absent case and must not be turned into one by
    // this fix; the value is adopted and the consumers clamp it, exactly as before.
    {
        AnamorphAudioProcessor p; p.prepareToPlay (48000.0, 512);
        auto blob = saveOmitting (nullptr);
        auto root = juce::ValueTree::fromXml (*BlobCodec::unwrap (blob));
        root.getChildWithName ("ANAMORPH_INTERNAL")
            .setProperty (anamorph::iid::uiScale, "nan", nullptr);
        const auto poisoned = BlobCodec::wrap (*root.createXml());
        p.setStateInformation (poisoned.getData(), (int) poisoned.getSize());
        check (p.getInternal().uiScaleIndex() >= 0 && p.getInternal().uiScaleIndex() <= 4,
               "a malformed MODERN Setting is still clamped by its consumer (repair unchanged)");
        check (p.getInternal().oversampleIndex() >= 0 && p.getInternal().oversampleIndex() <= 3,
               "...and the oversample atomic stays in range");
    }
}

// ---------------------------------------------------------------------------
//  Opt-in probe: does a MODERN session that OMITS an optional host-hidden
//  Setting leave the previous project's value in force on a reused instance?
//  The review located this in migrateFromLegacyApvts; this probe checks BOTH
//  paths so the answer names the right one.
// ---------------------------------------------------------------------------
static int runPartialSettingsProbe()
{
    std::printf ("partial host-hidden Settings probe (modern vs legacy restore)\n");
    std::printf ("============================================================\n\n");

    struct Field { const juce::Identifier& id; const char* name; juce::var sessionA; juce::var doc; };
    const Field fields[] = {
        { anamorph::iid::oversample,   "int_oversample",   3,     1     },
        { anamorph::iid::uiScale,      "int_uiScale",      5,     3     },
        { anamorph::iid::scopePersist, "int_scopePersist", 0.9,   0.5   },
        { anamorph::iid::metersOn,     "int_metersOn",     true,  false },
        { anamorph::iid::tooltipsOn,   "int_tooltipsOn",   true,  false },
        { anamorph::iid::uiAnimations, "int_uiAnimations", false, true  },
    };

    // Session B: a REAL modern save, with exactly one ANAMORPH_INTERNAL property
    // removed. Everything else -- the sound child, the AB node, the preset meta --
    // is what the plug-in itself wrote.
    auto sessionBOmitting = [] (const juce::Identifier& omit)
    {
        AnamorphAudioProcessor b; b.prepareToPlay (48000.0, 512);
        juce::MemoryBlock mb; b.getStateInformation (mb);
        auto xml  = BlobCodec::unwrap (mb);
        auto root = juce::ValueTree::fromXml (*xml);
        auto internalNode = root.getChildWithName ("ANAMORPH_INTERNAL");
        internalNode.removeProperty (omit, nullptr);
        return BlobCodec::wrap (*root.createXml());
    };

    std::printf ("  MODERN path (AnamorphRoot with an ANAMORPH_INTERNAL child):\n");
    std::printf ("  %-18s | %-10s | %-10s | %-10s | %s\n",
                 "omitted field", "session A", "documented", "after B", "verdict");
    int inherited = 0;
    for (const auto& f : fields)
    {
        AnamorphAudioProcessor p; p.prepareToPlay (48000.0, 512);
        p.getInternal().copyState();                       // touch, no-op
        // Session A: the previous project sets this field to a non-default value.
        juce::ValueTree a ("ANAMORPH_INTERNAL");
        for (const auto& g : fields) a.setProperty (g.id, g.sessionA, nullptr);
        p.getInternal().restoreState (a);
        const auto before = p.getInternal().copyState()[f.id];

        const auto blobB = sessionBOmitting (f.id);
        p.setStateInformation (blobB.getData(), (int) blobB.getSize());
        const auto after = p.getInternal().copyState()[f.id];

        const bool isInherited = after.equals (before) && ! after.equals (f.doc);
        if (isInherited) ++inherited;
        std::printf ("  %-18s | %-10s | %-10s | %-10s | %s\n", f.name,
                     before.toString().toRawUTF8(), f.doc.toString().toRawUTF8(),
                     after.toString().toRawUTF8(),
                     isInherited ? "INHERITED from the previous project"
                                 : (after.equals (f.doc) ? "reset to documented default" : "other"));
    }
    std::printf ("  => %d of 6 fields inherit the previous project's value\n\n", inherited);

    // The LEGACY path, for contrast: a v0.2 blob carrying NO Settings at all.
    std::printf ("  LEGACY path (v0.2 bare APVTS -> migrateFromLegacyApvts):\n");
    {
        AnamorphAudioProcessor p; p.prepareToPlay (48000.0, 512);
        juce::ValueTree a ("ANAMORPH_INTERNAL");
        for (const auto& g : fields) a.setProperty (g.id, g.sessionA, nullptr);
        p.getInternal().restoreState (a);
        if (! applyXmlFixture (p, "legacy_v0_2_bare_apvts.xml")) return 1;
        int legacyInherited = 0;
        for (const auto& f : fields)
        {
            const auto after = p.getInternal().copyState()[f.id];
            if (! after.equals (f.doc)) ++legacyInherited;
            std::printf ("  %-18s | after v0.2 restore: %-10s (documented %s) %s\n", f.name,
                         after.toString().toRawUTF8(), f.doc.toString().toRawUTF8(),
                         after.equals (f.doc) ? "" : "  <-- NOT the default");
        }
        std::printf ("  => %d of 6 inherit on the legacy path\n", legacyInherited);
    }
    return 0;
}

// ---------------------------------------------------------------------------
//  State test 30 -- a host that PREPARES off the message thread must not
//  deliver the latency report from that thread (ER-STATE-19: D-1 applied to
//  prepareToPlay).
//
//  prepareToPlay used to call updateLatency() directly, so a host that activates
//  the plug-in on a thread that is not the JUCE message thread -- the JUCE Linux
//  VST3 wrapper's own fallback when the host provides no IRunLoop (JUCE's
//  background MessageThread IS the message thread then, and it runs this
//  processor's timer); a host that ignores the [UI-thread] annotation on
//  setActive; an AU Initialize off main -- wrote AudioProcessor::latencySamples
//  and walked the listener chain on THAT thread while the processor's 20 Hz
//  timer could be doing the same on the message thread: two unsynchronised
//  writers of one plain int, and a read of the engine's latency2/4/8 while
//  engine.prepare() rewrites them. The stale-host outcome (the timer's older
//  value landing last, with nothing pending to correct it) needs an interleaving
//  no product barrier can force, so it is MEASURED by --reprepare-race-probe
//  rather than asserted here. What this test pins deterministically is the
//  invariant that makes it impossible: no latency delivery ever happens off the
//  message thread, and what the timer then delivers is the PREPARED value --
//  the actual latency, not merely the flag.
// ---------------------------------------------------------------------------
static void testOffThreadPrepareDefersLatency()
{
    std::printf ("State test 30: an off-message-thread prepareToPlay defers its latency report (ER-STATE-19)\n");

    struct ThreadRecorder final : public juce::AudioProcessorListener
    {
        std::atomic<int> total { 0 }, offMessageThread { 0 };
        void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& d) override
        {
            if (! d.latencyChanged) return;
            total.fetch_add (1);
            if (! juce::MessageManager::existsAndIsCurrentThread()) offMessageThread.fetch_add (1);
        }
    };

    // Truth first: the same state prepared ON the message thread reports
    // synchronously, and that number is what the off-thread prepare must reach.
    int truth = 0;
    {
        AnamorphAudioProcessor q;
        q.getInternal().oversampleValue().setValue (2);          // 1-based combo id: 2x
        q.getAPVTS().getParameter (pid::drive)->setValueNotifyingHost (1.0f);
        check (q.getLatencySamples() == 0, "a never-prepared instance reports 0 (the oversampler does not exist yet)");
        ThreadRecorder control;
        q.addListener (&control);
        q.prepareToPlay (48000.0, 512);
        truth = q.getLatencySamples();
        check (truth != 0, "non-vacuity: 2x oversampling with drive up reports a non-zero latency once prepared");
        // CONTROL: the message-thread path must stay synchronous -- the notification
        // has already happened, inside prepareToPlay, on this thread. A build that
        // deferred it too would leave total at 0 here (and would pass the off-thread
        // legs below vacuously).
        check (control.total.load() == 1 && control.offMessageThread.load() == 0,
               "CONTROL: a message-thread prepareToPlay still reports SYNCHRONOUSLY, on the message thread");
        q.removeListener (&control);
    }

    AnamorphAudioProcessor p;
    p.getInternal().oversampleValue().setValue (2);
    p.getAPVTS().getParameter (pid::drive)->setValueNotifyingHost (1.0f);
    const int before = p.getLatencySamples();
    check (before == 0, "baseline: the instance about to be prepared off-thread reports 0");

    ThreadRecorder rec;
    p.addListener (&rec);

    // The host's activation, on a thread that is NOT the message thread.
    std::thread host ([&] { p.prepareToPlay (48000.0, 512); });
    host.join();

    // (i) DISCRIMINATING and deterministic: before the fix the preparing thread
    //     itself ran setLatencySamples, so the listener fired on it.
    check (rec.offMessageThread.load() == 0,
           "no latency notification was delivered from the preparing thread");
    // (ii) Equally deterministic: nothing has dispatched on the message thread
    //      since the join, so the report cannot have moved yet.
    check (p.getLatencySamples() == before,
           "the report is unchanged until the message thread serves the request");

    // (iii) The timer delivers the PREPARED value. Bounded polls for the
    //       processor's own 20 Hz timer (see State test 27 for why a tight loop
    //       would fire nothing); the deadline bounds only a FAILING run.
    int served = before;
    for (int elapsed = 0; elapsed < 2000 && served == before; elapsed += 5)
    {
        juce::Timer::callPendingTimersSynchronously();
        served = p.getLatencySamples();
        if (served == before) std::this_thread::sleep_for (std::chrono::milliseconds (5));
    }
    std::printf ("  message-thread truth %d; off-thread prepare: after join %d, served by the timer %d; "
                 "notifications on the preparing thread: %d\n",
                 truth, before, served, rec.offMessageThread.load());
    check (served == truth, "the timer serves the ACTUAL prepared latency, equal to the message-thread report");
    check (rec.total.load() >= 1 && rec.offMessageThread.load() == 0,
           "...and every notification ran on the message thread");
    p.removeListener (&rec);
}

// ---------------------------------------------------------------------------
//  ER-STATE-19 probe (round 15): does a host that re-prepares OFF the message
//  thread race the processor's own D-1 latency timer?
//
//  NOT part of the suite. Like --state-thread-probe it drives the exact
//  interaction the finding describes, so if the race is real its own execution
//  is undefined behaviour: run it under ThreadSanitizer for the mechanical
//  answer, or in a plain build to COUNT the value-level symptom.
//
//  Thread H models a host activating the plug-in on a thread that is not the
//  JUCE message thread (see State test 30 for which hosts those are). Each
//  iteration is the ordinary shape: an automation write of Drive lands, raising
//  the D-1 request from a non-message thread exactly as State test 22 does, then
//  the host re-prepares. The MAIN thread is the message thread and does only
//  what the real one would: serve the 20 Hz timer.
//
//  Reported: how many latency notifications ran on H (the pre-round-15 code
//  delivered every prepare's report from H itself), and whether the value the
//  host was left holding matches what a message-thread re-prepare of the final
//  state reports -- the "stale delay compensation" the review named.
// ---------------------------------------------------------------------------
static int runReprepareRaceProbe()
{
    std::printf ("ER-STATE-19 probe: off-message-thread prepareToPlay vs the D-1 timer\n");
    std::printf ("  (under ThreadSanitizer a report here is the finding; the counts below are the plain-build symptom)\n");

    struct ThreadRecorder final : public juce::AudioProcessorListener
    {
        std::atomic<int> onHostThread { 0 }, onMessageThread { 0 };
        void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& d) override
        {
            if (! d.latencyChanged) return;
            (juce::MessageManager::existsAndIsCurrentThread() ? onMessageThread : onHostThread).fetch_add (1);
        }
    };

    constexpr int kRuns = 20, kIterations = 200;
    int staleRuns = 0, hostDeliveries = 0, messageDeliveries = 0;

    for (int run = 0; run < kRuns; ++run)
    {
        AnamorphAudioProcessor proc;
        proc.getInternal().oversampleValue().setValue (2);
        auto* drive = proc.getAPVTS().getParameter (pid::drive);
        ThreadRecorder rec;
        proc.addListener (&rec);

        std::atomic<bool> go { false }, done { false };
        std::thread host ([&]
        {
            while (! go.load (std::memory_order_acquire)) { /* spin to widen the window */ }
            for (int i = 0; i < kIterations; ++i)
            {
                drive->setValueNotifyingHost ((i & 1) != 0 ? 1.0f : 0.0f);   // raises the request off-thread
                proc.prepareToPlay (48000.0, 512);                            // the re-prepare
            }
            done.store (true, std::memory_order_release);
        });

        go.store (true, std::memory_order_release);
        while (! done.load (std::memory_order_acquire))
        {
            juce::Timer::callPendingTimersSynchronously();
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
        }
        host.join();

        // Let any request the LAST prepare left pending drain (a few timer periods).
        for (int i = 0; i < 60; ++i)
        {
            juce::Timer::callPendingTimersSynchronously();
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
        }
        const int told = proc.getLatencySamples();
        proc.prepareToPlay (48000.0, 512);           // message thread: the truth for the final state
        const int truth = proc.getLatencySamples();
        proc.removeListener (&rec);

        hostDeliveries    += rec.onHostThread.load();
        messageDeliveries += rec.onMessageThread.load();
        if (told != truth) ++staleRuns;
        std::printf ("  run %2d: deliveries on host thread %3d / message thread %3d; host left holding %d, truth %d%s\n",
                     run, rec.onHostThread.load(), rec.onMessageThread.load(), told, truth,
                     told != truth ? "  <-- STALE" : "");
    }

    std::printf ("  => %d of %d runs left the host stale; %d deliveries ran on the host thread, %d on the message thread\n",
                 staleRuns, kRuns, hostDeliveries, messageDeliveries);
    return 0;
}

// ---------------------------------------------------------------------------
//  State test 31 -- a restore with no A/B data must not carry the PREVIOUS
//  project's per-slot Level-Match gains into the first switch (ER-STATE-20).
//
//  `abMatchGain[]` is the fifth processor member of the reused-instance class
//  rounds 2, 8, 11 and 14 closed for the Settings, the A/B slots, a root with no
//  sound child, and a partial modern Settings node. It is the one piece of the
//  slot set that is NEVER serialized -- a runtime cache of what the matcher had
//  settled on when each slot was last left -- so nothing about a restore ever
//  overwrote it, and `abSwitchTo`'s closing
//  `engine.injectMatchGainDb (abMatchGain[slot])` handed the new project's
//  matcher the old project's figure.
//
//  HOW THIS OBSERVES THE INJECTED VALUE EXACTLY, with no DSP tolerance games.
//  Two properties of the product make it deterministic:
//    * after a restore with no A/B data BOTH slots are invalid, so `abEnsureInit`
//      re-seeds them from the SAME restored state -- the switch is therefore
//      parameter-neutral, and nothing but the injection can move the match; and
//    * `LoudnessMatch` HOLDS its published value on silence by documented design
//      ("when the input decays to silence the measurement WAITS ... it never
//      drifts toward 0", LoudnessMatch.h) -- so a switch performed over silent
//      blocks leaves `getMatchGainDb()` reading the injected value verbatim.
//  Round 9 measured the AUDIBLE consequence of the stale injection and found it
//  inert (`--legacy-match-probe`): `setParameters` re-targets `matchGainSmooth`
//  from the live measurement every block, so the level recovers. That conclusion
//  stands and is NOT what this test re-opens. This is the state, which was wrong
//  regardless: the readout showed the old project's number and the new project's
//  matcher re-converged from it.
//
//  WHICH SLOT EACH LEG CAN SEE. `abSwitchTo` STORES into the slot it leaves
//  before it READS the one it enters, so a given restore only ever exposes the
//  slot that is not active. Legs 1 and 2 restore `active = 0` (the default) and
//  therefore see slot B; leg 3 restores an `AB` node carrying `active = 1` with
//  no usable payloads, and sees slot A. Between them both entries are covered.
// ---------------------------------------------------------------------------
static void testRestoreResetsAbMatchGains()
{
    std::printf ("State test 31: a restore with no A/B data resets the per-slot Level-Match gains (ER-STATE-20)\n");

    constexpr double sr = 48000.0;
    constexpr int    bs = 512;

    auto runNoise = [] (AnamorphAudioProcessor& p, int nBlocks, juce::Random& rng)
    {
        juce::AudioBuffer<float> buf (2, bs);
        juce::MidiBuffer midi;
        for (int k = 0; k < nBlocks; ++k)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < bs; ++i)
                    buf.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * 0.25f);
            p.processBlock (buf, midi);
        }
    };
    // Silence is the matcher's documented HOLD state, which is what makes the
    // injected value readable verbatim. Four blocks is well past the ~6 ms
    // fade-out that reaches the duck bottom where the injection is consumed.
    auto runSilence = [] (AnamorphAudioProcessor& p, int nBlocks)
    {
        juce::AudioBuffer<float> buf (2, bs);
        juce::MidiBuffer midi;
        for (int k = 0; k < nBlocks; ++k) { buf.clear(); p.processBlock (buf, midi); }
    };

    // "The previous project": two slots whose MEASURED matches genuinely differ,
    // and both far from the 0 dB a fresh instance would inject -- otherwise the
    // stale value and the correct value coincide and the legs are vacuous.
    // Leaves abMatchGain[0] = A's match, abMatchGain[1] = B's match, abActive = 0.
    auto seedPreviousProject = [&] (AnamorphAudioProcessor& p, float& outA, float& outB)
    {
        juce::Random rng (20260902);
        p.prepareToPlay (sr, bs);
        setRaw (p, "autoGainMatch", 1.0f);   // Match ON so the matcher really converges
        setRaw (p, "width", 0.95f);
        runNoise (p, 60, rng);
        outA = p.getEngine().getMatchGainDb();
        p.abSwitchTo (1);                    // stores A's match into abMatchGain[0]
        setRaw (p, "width", 0.05f);
        runNoise (p, 60, rng);
        outB = p.getEngine().getMatchGainDb();
        p.abSwitchTo (0);                    // stores B's match into abMatchGain[1]
        runNoise (p, 20, rng);
    };

    // THE INVARIANT, stated as the contract rather than as a number: a REUSED
    // instance restored from a session with no A/B data must behave exactly like a
    // FRESH instance restored from the SAME blob. That is what "no previous-session
    // state survives" means, and it is what the other four members of this class are
    // tested against.
    //
    // Stating it that way is also what makes it exact. An earlier draft asserted the
    // injected value was 0 dB verbatim and was wrong to: `LoudnessMatch`'s
    // feed-forward PREDICT is an absolute function of Drive and Mix and lowers the
    // published gain when the restored session's controls imply more boost than the
    // previous project's, so the reading drifts off 0 by however much the restore
    // moved those two controls (measured: -3.161 dB against the v0.2 fixture, -0.052
    // dB against a modern save of the same state). The fresh control experiences the
    // identical predict, so comparing against it cancels exactly that term and leaves
    // only the injection -- which is the thing under test.
    auto expectNoStaleInjection = [&] (const char* legName,
                                       const std::function<juce::MemoryBlock (AnamorphAudioProcessor&)>& makeBlob,
                                       int firstSwitchTarget,
                                       bool staleIsSlotB)
    {
        // One blob, built from a dedicated source, applied to BOTH instances -- the
        // control is only a control if it restores the identical bytes.
        juce::MemoryBlock blob;
        {
            AnamorphAudioProcessor src;
            src.prepareToPlay (sr, bs);
            blob = makeBlob (src);
        }
        if (blob.getSize() == 0) { check (false, "the leg produced a restorable blob"); return; }

        // The observation, run identically on both instances.
        auto restoreSwitchAndRead = [&] (AnamorphAudioProcessor& p)
        {
            p.setStateInformation (blob.getData(), (int) blob.getSize());
            // The ordinary host order -- setState, THEN activate (the sequence
            // prepareToPlay's own comment calls "the ordinary VST3/AU order"). It is
            // also what keeps this comparison about the slot state: without it the
            // reused instance still holds the previous project's audio in its delay
            // lines and oversamplers, which flushes through the first blocks and gives
            // the matcher something real to measure -- worth 0.052 dB here, engine
            // history rather than A/B state, and not what this test is about.
            p.prepareToPlay (sr, bs);
            check (p.abActiveSlot() != firstSwitchTarget,
                   "the restored active slot is the one this leg's first switch moves AWAY from");
            p.abSwitchTo (firstSwitchTarget);
            runSilence (p, 4);
            return p.getEngine().getMatchGainDb();
        };

        AnamorphAudioProcessor reused;
        float prevA = 0.0f, prevB = 0.0f;
        seedPreviousProject (reused, prevA, prevB);
        const float stale = staleIsSlotB ? prevB : prevA;
        check (std::abs (prevA - prevB) > 0.5f,
               "non-vacuity: the previous project's two slot matches are distinguishable");
        check (std::abs (stale) > 0.5f,
               "non-vacuity: the stale value differs from the 0 dB a fresh instance injects");
        const float injectedReused = restoreSwitchAndRead (reused);

        AnamorphAudioProcessor fresh;
        fresh.prepareToPlay (sr, bs);
        const float injectedFresh = restoreSwitchAndRead (fresh);

        std::printf ("  %-28s previous project A %.3f dB / B %.3f dB; stale %.3f dB;"
                     " first switch: reused %.3f dB vs fresh %.3f dB\n",
                     legName, prevA, prevB, stale, injectedReused, injectedFresh);
        check (std::abs (injectedReused - stale) > 0.5f,
               "the first switch did NOT inject the previous project's remembered match");
        checkNear ((double) injectedReused, (double) injectedFresh, 1.0e-4,
                   "...the reused instance is indistinguishable from a fresh one (the contract)");
    };

    // --- Leg 1: v0.2 bare APVTS (the legacy branch's abResetToDefaults).
    expectNoStaleInjection ("v0.2 bare APVTS:", [] (AnamorphAudioProcessor&)
    {
        auto file = fixtureDir().getChildFile ("legacy_v0_2_bare_apvts.xml");
        auto xml  = juce::parseXML (file);
        if (xml == nullptr) return juce::MemoryBlock();
        return BlobCodec::wrap (*xml);
    }, 1, true);

    // --- Leg 2: a modern root with no AB child at all (the `else` branch's
    //     abResetToDefaults). Built from a REAL save so the session is genuine in
    //     every other respect; only the AB child is removed.
    expectNoStaleInjection ("modern root, no AB node:", [] (AnamorphAudioProcessor& src)
    {
        juce::MemoryBlock saved;
        src.getStateInformation (saved);
        auto xml = BlobCodec::unwrap (saved);
        if (xml == nullptr) return juce::MemoryBlock();
        if (auto* ab = xml->getChildByName ("AB")) xml->removeChildElement (ab, true);
        check (xml->getChildByName ("AB") == nullptr, "the AB node really is absent from leg 2's blob");
        return BlobCodec::wrap (*xml);
    }, 1, true);

    // --- Leg 3: an AB node that EXISTS but carries no usable slot payloads, with
    //     active = 1. This path never reaches abResetToDefaults -- `readSlot` resets
    //     each slot in place -- so it is the one that exposes slot A, and the one a
    //     fix confined to abResetToDefaults leaves leaking (measured: -2.405 dB).
    expectNoStaleInjection ("AB node, no payloads:", [] (AnamorphAudioProcessor& src)
    {
        juce::MemoryBlock saved;
        src.getStateInformation (saved);
        auto xml = BlobCodec::unwrap (saved);
        if (xml == nullptr) return juce::MemoryBlock();
        if (auto* ab = xml->getChildByName ("AB")) xml->removeChildElement (ab, true);
        xml->createNewChildElement ("AB")->setAttribute ("active", 1); // the only thing it carries
        return BlobCodec::wrap (*xml);
    }, 0, false);

    // --- Leg 4: a session that DOES carry valid A/B data still restores both
    //     slots' own sounds. The fix must not have touched this path.
    {
        constexpr float kSoundA = 0.88f, kSoundB = 0.12f;
        juce::MemoryBlock valid;
        {
            AnamorphAudioProcessor src;
            src.prepareToPlay (sr, bs);
            setRaw (src, "width", kSoundA);
            src.abSwitchTo (1);
            setRaw (src, "width", kSoundB);
            src.abSwitchTo (0);
            src.getStateInformation (valid);
        }

        AnamorphAudioProcessor p;
        float prevA = 0.0f, prevB = 0.0f;
        seedPreviousProject (p, prevA, prevB);
        p.setStateInformation (valid.getData(), (int) valid.getSize());

        checkNear ((double) rawOf (p, "width"), (double) kSoundA, 1.0e-3,
                   "a valid A/B session restores slot A's own sound");
        check (p.abActiveSlot() == 0, "...and its own active slot");
        p.abSwitchTo (1);
        checkNear ((double) rawOf (p, "width"), (double) kSoundB, 1.0e-3,
                   "...and slot B still holds ITS own sound, not the restored one");
        p.abSwitchTo (0);
        checkNear ((double) rawOf (p, "width"), (double) kSoundA, 1.0e-3,
                   "...and switching back returns slot A's sound (valid A/B behaviour intact)");
    }
}

// ---------------------------------------------------------------------------
//  Opt-in probe (round 16): what does the MODERN host-hidden Settings path do
//  with a malformed value that is PRESENT?
//
//  MEASURES, ASSERTS NOTHING, returns 0 either way -- the same discipline as
//  --latency-restore-probe. The question it answers is a review finding
//  ("`restoreState` accepts present modern settings verbatim, unlike legacy
//  migration"), and the point of a probe rather than a test is that a test would
//  have to encode an expectation about recovery semantics that no document
//  stated at the time (`SERIALIZATION_REGISTRY.md` has stated them since round
//  18; this probe still reports rather than asserts).
//
//  Ingress, stated up front because it bounds everything below: the modern
//  ANAMORPH_INTERNAL values are written by copyState() from a live tree whose only
//  writers are the constructor's defaults table, restoreState (from a file),
//  migrateFromLegacyApvts (clamped at source since ER-STATE-17) and the Settings
//  widgets (whose ComboBox ids and Slider range are valid by construction). So a
//  malformed MODERN value can only arrive from a hand-edited or corrupted file --
//  it cannot be produced by the plug-in itself.
// ---------------------------------------------------------------------------
static int runModernSettingsProbe()
{
    std::printf ("modern host-hidden Settings validation probe (round 16; repaired under Policy B since round 18)\n");
    std::printf ("======================================================\n\n");

    // A genuine modern save, used as the carrier for every mutation below.
    juce::MemoryBlock base;
    {
        AnamorphAudioProcessor src;
        src.prepareToPlay (48000.0, 512);
        src.getStateInformation (base);
    }

    struct Case { const char* field; const char* value; };
    const Case cases[] = {
        { "int_oversample",   "99"    }, { "int_oversample",   "-5"      },
        { "int_oversample",   "abc"   }, { "int_oversample",   "nan"     },
        { "int_oversample",   "inf"   }, { "int_oversample",   "2.7"     },
        { "int_uiScale",      "99"    }, { "int_uiScale",      "0"       },
        { "int_uiScale",      "abc"   },
        { "int_scopePersist", "5.0"   }, { "int_scopePersist", "-1.0"    },
        { "int_scopePersist", "nan"   }, { "int_scopePersist", "inf"     },
        { "int_scopePersist", "abc"   }, { "int_scopePersist", "1e39"    },
        { "int_metersOn",     "abc"   }, { "int_metersOn",     "2"       },
        { "int_tooltipsOn",   "maybe" }, { "int_uiAnimations", "-1"      },
    };

    std::printf ("  %-18s %-8s | %-14s | %-9s %-9s %-11s | %-8s | %-8s | after editor\n",
                 "field", "written", "tree after", "osIndex", "uiScale", "persist",
                 "finite?", "resaved");
    std::printf ("  %s\n", juce::String::repeatedString ("-", 122).toRawUTF8());

    int nonFinite = 0, outOfDomainTree = 0, durable = 0;

    for (const auto& c : cases)
    {
        auto xml = BlobCodec::unwrap (base);
        if (xml == nullptr) { std::printf ("  (blob codec failed)\n"); return 1; }
        auto* internal = xml->getChildByName ("ANAMORPH_INTERNAL");
        if (internal == nullptr) { std::printf ("  (no ANAMORPH_INTERNAL node)\n"); return 1; }
        internal->setAttribute (c.field, c.value);
        auto mutated = BlobCodec::wrap (*xml);

        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        p.setStateInformation (mutated.getData(), (int) mutated.getSize());

        const auto  after   = p.getInternal().copyState()[juce::Identifier (c.field)];
        const int   osIdx   = p.getInternal().oversampleIndex();
        const int   uiIdx   = p.getInternal().uiScaleIndex();
        const float persist = p.getInternal().scopePersist();

        // Does the malformed value survive into the NEXT save? That is what makes
        // a bad value durable state rather than a one-session display glitch.
        juce::MemoryBlock resaved;
        p.getStateInformation (resaved);
        juce::String resavedValue = "(gone)";
        if (auto rx = BlobCodec::unwrap (resaved))
            if (auto* ri = rx->getChildByName ("ANAMORPH_INTERNAL"))
                resavedValue = ri->getStringAttribute (c.field);

        const bool finite = std::isfinite (persist);
        if (! finite) ++nonFinite;
        if (resavedValue == juce::String (c.value)) ++durable;
        if (juce::String (c.field) == "int_oversample" || juce::String (c.field) == "int_uiScale")
        {
            const int domainMax = juce::String (c.field) == "int_oversample" ? 4 : 5;
            const int treeId    = (int) after;
            if (treeId < 1 || treeId > domainMax) ++outOfDomainTree;
        }

        // USER-VISIBILITY, which is the half a headless read cannot answer. The
        // Settings widgets bind these values two-way, so opening the editor is
        // itself a write path: a Slider constrains to its range and a ComboBox
        // rejects an id it has no item for. Whether that REPAIRS the tree or just
        // displays wrongly is the difference between "durable invalid state" and
        // "one bad session", so it is measured rather than reasoned about.
        juce::String afterEditor = "(not built)";
       #if JUCE_LINUX || JUCE_BSD
        if (auto* ed = p.createEditor())
        {
            afterEditor = p.getInternal().copyState()[juce::Identifier (c.field)].toString();
            delete ed;
        }
       #endif

        std::printf ("  %-18s %-8s | %-14s | %-9d %-9d %-11.4f | %-8s | %-8s | %s\n",
                     c.field, c.value, after.toString().toRawUTF8(), osIdx, uiIdx,
                     (double) persist, finite ? "yes" : "NO", resavedValue.toRawUTF8(),
                     afterEditor.toRawUTF8());
    }

    // ---- Downstream of the ONE unclamped consumer (round 17) -----------------
    // scopePersist is the only setting whose read applies no clamp, so it is the
    // only one whose malformed value can travel. Model the real chain exactly as
    // the editor builds it: a Slider with the editor's range (PluginEditor.cpp
    // setRange(0,1,0.001)) bound two-way to the tree value (getValueObject().
    // referTo(scopePersistValue())), then applyScopePersist()'s
    // pow(getValue(), 0.737f), then Vectorscope::setPersistence's
    // jlimit(0,1,...), then windowFrames()'s jmap -> (int).
    //
    // The last step is the one that matters: juce::jlimit returns its argument
    // when NEITHER comparison is true, which is exactly what a NaN does, so a
    // clamp that looks total is transparent to it -- and (int) of a non-finite
    // float is UNDEFINED ([conv.fpint]), the same class round 12 fixed on the
    // legacy path. This section therefore reports finiteness at each stage and
    // does NOT perform the final conversion.
    std::printf ("\n  downstream of scopePersist (the one unclamped read), modelling the real editor chain:\n");
    std::printf ("    %-10s | %-12s | %-12s | %-12s | %s\n",
                 "written", "slider value", "pow(v,.737)", "after jlimit", "jmap -> (int) would be");
    std::printf ("    %s\n", juce::String::repeatedString ("-", 84).toRawUTF8());
    int reachesUB = 0;
    for (const char* v : { "0.25", "5.0", "-1.0", "nan", "inf", "1e39", "abc" })
    {
        juce::ValueTree t ("T");
        t.setProperty ("p", juce::var (juce::String (v)), nullptr);
        juce::Slider sl;
        sl.setRange (0.0, 1.0, 0.001);                       // the editor's range
        sl.getValueObject().referTo (t.getPropertyAsValue ("p", nullptr));

        const double sliderV = sl.getValue();
        const float  powed   = std::pow ((float) sliderV, 0.737f);
        const float  limited = juce::jlimit (0.0f, 1.0f, powed);
        const float  mapped  = juce::jmap (limited, 0.0f, 1.0f, 1200.0f, 8000.0f);
        const bool   ub      = ! std::isfinite (mapped);
        if (ub) ++reachesUB;
        std::printf ("    %-10s | %-12.4f | %-12.4f | %-12.4f | %s\n", v, sliderV,
                     (double) powed, (double) limited,
                     ub ? "UNDEFINED (non-finite)" : "defined");
    }
    std::printf ("    => %d of 7 reach the (int) conversion non-finite\n", reachesUB);

    std::printf ("\n  summary: %d case(s) left a NON-FINITE scope persistence;"
                 " %d left an out-of-domain ComboBox id IN THE TREE; %d persisted"
                 " the malformed text into the next save\n", nonFinite, outOfDomainTree, durable);
    std::printf ("  (the DSP-facing reads are clamped at source: oversampleIndex via jlimit(0,3),\n"
                 "   uiScaleIndex via jlimit(0,4) -- so neither can index out of range whatever\n"
                 "   the tree holds. scopePersist() is an unclamped (float)(double) read.)\n");
    return 0;
}

// ---------------------------------------------------------------------------
//  State test 32 -- a malformed `int_scopePersist` must not drive a NON-FINITE
//  persistence into the vectorscope (ER-STATE-21, round 17).
//
//  This is the one place where round 16's survey of malformed MODERN Settings
//  found something that travels. Five of the six settings are clamped at their
//  read (`oversampleIndex`/`uiScaleIndex` through `jlimit`, the three booleans
//  through a total `var`->`bool` coercion), so whatever the tree holds, the
//  consumer sees a legal value. `scopePersist` is the exception, and its route
//  ends at `Vectorscope::windowFrames()`, which evaluates `(int)` of a `jmap` --
//  UNDEFINED for a non-finite float ([conv.fpint]).
//
//  TWO inputs arrive non-finite, and the second is the interesting one:
//    * `"nan"` travels intact -- JUCE's number parser accepts it, the Value ->
//      Slider binding does not reject it, and `jlimit` is transparent to it
//      because NEITHER of its comparisons is true for a NaN; and
//    * ANY NEGATIVE value, which is perfectly finite in the file, becomes a NaN
//      before it arrives: the editor's `applyScopePersist()` computes
//      `pow(value, 0.737f)` first, and a negative base with a fractional
//      exponent is NaN.
//  Neither is repaired by opening the editor (measured, round 16: the Slider's
//  range constrains a too-HIGH value but writes nothing back for a negative or a
//  NaN), and both survive into the next save.
//
//  WHAT THIS TEST DOES AND DOES NOT DECIDE. It pins only that the value reaching
//  the scope is always finite -- a local correctness property of the consumer,
//  true whatever the serialization contract says. When it was written that
//  contract WAS open and the tree kept whatever the file said; round 18 settled
//  it separately (the maintainer's Policy B -- a present-but-invalid Setting is
//  repaired on restore and the repaired value persisted, State test 33), so a
//  malformed value no longer reaches this consumer from an ordinary restore at
//  all. The guard stays as the backstop that decision asks for, and this test
//  stays discriminating because it drives `setPersistence` directly.
// ---------------------------------------------------------------------------
//  State test 36 -- a repair whose value happens to MATCH the live one must
//  still reach the persisted state (ER-STATE-25).
//
//  State test 20 already pins "a repaired parameter reaches the saved state",
//  but it poisons with `nan`, and NaN makes `applyNorm`'s gate
//  `! (|norm - getValue()| <= 1e-6)` true on the comparison alone -- so it
//  exercises the repair path and never the gate's other side. This test takes
//  that other side: malformed text whose repair lands on the value the
//  parameter ALREADY holds.
//
//  THE PRECONDITION IS ARITHMETIC, NOT A GUESS, and the test searches for a
//  parameter that satisfies it rather than hard-coding one. `apvts.replaceState`
//  runs first and pushes @value through JUCE's own parser, where unusable text
//  reads as the denormalised 0; `applyNorm` then computes `norm` = the parameter
//  DEFAULT, because SerializedNumber refuses the same text. So the gate is false
//  -- and the tree write-back skipped -- exactly when
//      convertTo0to1 (0) == getDefaultValue()
//  which is true of every parameter whose range starts at its default (Drive
//  0..24 dB default 0, Amount 0..1 default 0, ...).
//
//  WHAT MAKES IT A PERSISTENCE TEST RATHER THAN A VACUOUS ONE. "restore ->
//  parameter == default" passes BEFORE the fix: the live value was never the
//  problem. The assertions that matter are on the serialized artefact -- the
//  live APVTS tree that copyState() reads, and the bytes getStateInformation
//  actually emits.
static void testDefaultValuedCorruptionIsRepairedInState()
{
    std::printf ("State test 36: a repair equal to the live value still reaches the saved state (ER-STATE-25)\n");

    AnamorphAudioProcessor probe;
    probe.prepareToPlay (48000.0, 256);

    // Find a parameter for which malformed text and the default coincide.
    juce::String victim;
    for (auto* p : probe.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! pid::isViewParam (rp->paramID)
                && std::abs (rp->convertTo0to1 (0.0f) - rp->getDefaultValue()) <= 1.0e-6f)
            { victim = rp->paramID; break; }
    check (victim.isNotEmpty(),
           "a parameter exists whose default coincides with what malformed text normalises to");
    if (victim.isEmpty()) return;
    std::printf ("  victim parameter: %s (default normalised %.6f)\n", victim.toRawUTF8(),
                 (double) probe.getAPVTS().getParameter (victim)->getDefaultValue());

    const char* kPoison = "abc";   // unusable text: not a number in any reading

    // Build a session whose victim PARAM carries the poison and no `raw`, so the
    // `value` path is the one under test (the raw fallback keeps its own contract
    // and is exercised by State tests 19/20 -- untouched here).
    auto poisonSession = [&] (const juce::MemoryBlock& clean, const char* poison)
    {
        auto xml = BlobCodec::unwrap (clean);
        check (xml != nullptr, "session decodes for poisoning");
        bool done = false;
        if (xml != nullptr)
            if (auto* params = xml->getChildByName ("ANAMORPH"))
                if (auto* n = params->getChildByAttribute ("id", victim))
                { n->setAttribute ("value", poison); n->removeAttribute ("raw"); done = true; }
        check (done, "the session carries the victim PARAM to poison");
        return xml != nullptr ? BlobCodec::wrap (*xml) : juce::MemoryBlock();
    };
    // What a state blob says for the victim's @value, as TEXT -- the durable artefact.
    auto savedText = [&] (const juce::MemoryBlock& blob)
    {
        auto xml = BlobCodec::unwrap (blob);
        if (xml != nullptr)
            if (auto* params = xml->getChildByName ("ANAMORPH"))
                if (auto* n = params->getChildByAttribute ("id", victim))
                    return n->getStringAttribute ("value");
        return juce::String ("<missing>");
    };

    juce::MemoryBlock clean;
    { AnamorphAudioProcessor authoring; authoring.prepareToPlay (48000.0, 256);
      authoring.getStateInformation (clean); }

    // ---- the core case, run TWICE so the corruption cannot survive one cycle
    //      and return on the next (the repository's repeated-restore discipline) ----
    juce::MemoryBlock resaved;
    for (int cycle = 0; cycle < 2; ++cycle)
    {
        const auto poisoned = poisonSession (cycle == 0 ? clean : resaved, kPoison);
        checkStr (savedText (poisoned), juce::String (kPoison),
                  "the input session really does carry the malformed text");

        AnamorphAudioProcessor proc;
        proc.prepareToPlay (48000.0, 256);
        proc.setStateInformation (poisoned.getData(), (int) poisoned.getSize());

        auto* rp = proc.getAPVTS().getParameter (victim);
        // (1) The LIVE value -- correct before the fix too, which is why it cannot
        //     be the assertion this test rests on.
        check (rp != nullptr && juce::approximatelyEqual (rp->getValue(), rp->getDefaultValue()),
               "the live parameter holds the repaired value");

        // (2) The LIVE TREE, which is what copyState() serialises from.
        auto live = proc.getAPVTS().state.getChildWithProperty ("id", victim);
        check (live.isValid(), "the live APVTS still carries the victim node");
        const juce::String liveText = live.getProperty ("value").toString();
        std::printf ("  cycle %d: live APVTS @value after restore = \"%s\"\n", cycle, liveText.toRawUTF8());
        check (liveText != kPoison, "the malformed text is GONE from the live APVTS tree");
        check (anamorph::looksLikePlainNumber (liveText.toRawUTF8()),
               "...and what replaced it is a plain number");

        // (3) The SAVED ARTEFACT -- the durable thing an older build would read.
        proc.getStateInformation (resaved);
        const juce::String outText = savedText (resaved);
        std::printf ("  cycle %d: saved @value = \"%s\"\n", cycle, outText.toRawUTF8());
        check (outText != kPoison, "the next save does NOT re-emit the malformed text");
        check (anamorph::looksLikePlainNumber (outText.toRawUTF8()),
               "...it emits a plain number");

        // (4) Reloading that save reproduces the value and stays canonical.
        AnamorphAudioProcessor back;
        back.prepareToPlay (48000.0, 256);
        back.setStateInformation (resaved.getData(), (int) resaved.getSize());
        auto* rb = back.getAPVTS().getParameter (victim);
        check (rb != nullptr && juce::approximatelyEqual (rb->getValue(), rp->getDefaultValue()),
               "reloading the repaired save reads the same value back");
        check (savedText (resaved) != kPoison, "and the malformed text does not reappear");
    }

    // ---- B. a GENUINELY valid value that happens to equal the default must not
    //         be treated as a repair: its text stays exactly as written ----
    {
        auto* rp0 = probe.getAPVTS().getParameter (victim);
        const juce::String canonical (rp0->convertFrom0to1 (rp0->getDefaultValue()), 6);
        auto xml = BlobCodec::unwrap (clean);
        bool done = false;
        if (xml != nullptr)
            if (auto* params = xml->getChildByName ("ANAMORPH"))
                if (auto* n = params->getChildByAttribute ("id", victim))
                { n->setAttribute ("value", canonical); n->removeAttribute ("raw"); done = true; }
        check (done, "the control session carries the victim PARAM");
        const auto validBlob = xml != nullptr ? BlobCodec::wrap (*xml) : juce::MemoryBlock();

        AnamorphAudioProcessor proc;
        proc.prepareToPlay (48000.0, 256);
        proc.setStateInformation (validBlob.getData(), (int) validBlob.getSize());
        auto live = proc.getAPVTS().state.getChildWithProperty ("id", victim);
        std::printf ("  control: valid \"%s\" restored as \"%s\"\n", canonical.toRawUTF8(),
                     live.getProperty ("value").toString().toRawUTF8());
        checkStr (live.getProperty ("value").toString(), canonical,
                  "a VALID value equal to the default is left exactly as written (no needless rewrite)");
    }

    // ---- B2. the CLAMP category, measured rather than assumed (round 27 asked
    //          which categories share the defect): a USABLE `raw` outside 0..1 is
    //          clamped, so the value the file spells is not the value in force.
    {
        auto* rp0 = probe.getAPVTS().getParameter (victim);
        auto xml = BlobCodec::unwrap (clean);
        if (xml != nullptr)
            if (auto* params = xml->getChildByName ("ANAMORPH"))
                if (auto* n = params->getChildByAttribute ("id", victim))
                    n->setAttribute ("raw", "-7");     // usable, far below 0 -> clamps to 0 == default
        const auto blob = xml != nullptr ? BlobCodec::wrap (*xml) : juce::MemoryBlock();
        AnamorphAudioProcessor proc;
        proc.prepareToPlay (48000.0, 256);
        proc.setStateInformation (blob.getData(), (int) blob.getSize());
        auto live = proc.getAPVTS().state.getChildWithProperty ("id", victim);
        std::printf ("  clamp category: raw=\"-7\" -> live @raw \"%s\", normalised %.6f\n",
                     live.getProperty ("raw").toString().toRawUTF8(),
                     (double) proc.getAPVTS().getParameter (victim)->getValue());
        check (std::abs (proc.getAPVTS().getParameter (victim)->getValue() - rp0->getDefaultValue()) <= 1.0e-6f,
               "an out-of-range `raw` clamps to the value in force");
        // ...and, being out of range, is corruption in its own right: the tree must
        // not keep it just because the clamp landed on the value already loaded.
        checkStr (live.getProperty ("raw").toString(), juce::String ("0.0"),
                  "an out-of-range `raw` is rewritten canonically even when the value does not move");
    }

    // ---- C. the raw/value fallback contract is untouched: a bad `raw` beside a
    //         valid `value` must still restore the VALUE, not the default ----
    {
        auto* rp0 = probe.getAPVTS().getParameter (victim);
        const float target = rp0->convertFrom0to1 (0.75f);      // deliberately NOT the default
        auto xml = BlobCodec::unwrap (clean);
        bool done = false;
        if (xml != nullptr)
            if (auto* params = xml->getChildByName ("ANAMORPH"))
                if (auto* n = params->getChildByAttribute ("id", victim))
                { n->setAttribute ("value", juce::String (target, 6));
                  n->setAttribute ("raw", "abc"); done = true; }
        check (done, "the fallback session carries the victim PARAM");
        const auto blob = xml != nullptr ? BlobCodec::wrap (*xml) : juce::MemoryBlock();

        AnamorphAudioProcessor proc;
        proc.prepareToPlay (48000.0, 256);
        proc.setStateInformation (blob.getData(), (int) blob.getSize());
        auto* rb = proc.getAPVTS().getParameter (victim);
        // Expect what the PARAMETER makes of that plain value, not 0.75 itself: a
        // choice/stepped victim snaps, and the claim here is "the value survived",
        // not "no quantisation happened".
        const float want = rp0->convertTo0to1 (target);
        std::printf ("  raw/value fallback: bad raw + value=%.6f -> normalised %.6f (want %.6f)\n",
                     (double) target, (double) rb->getValue(), (double) want);
        check (std::abs (rb->getValue() - want) < 1.0e-3f,
               "a malformed `raw` still falls back to the valid `value` (contract unchanged)");
        check (std::abs (want - rp0->getDefaultValue()) > 1.0e-3f,
               "non-vacuity: that fallback value is NOT the default, so the leg can fail");
    }
}
// ---------------------------------------------------------------------------
//  State test 35 -- a REJECTED preset load must have no audio side effect
//  (ER-GUI-06).
//
//  Round 24 made a foreign preset a no-op for STATE. This is the other half:
//  the editor raised the masking duck BEFORE asking the manager to load, so a
//  load the manager then refused still dry-filled the next ~32 ms of audio --
//  a duck masking a swap that never happened. Exactly the failure mode
//  `AnamorphEngine::primeParameters` already documents for the activation case
//  (ER-DSP-06 / Test 48), arriving here by a different route.
//
//  THE TEST DRIVES THE REAL EDITOR, because the defect was in the editor's
//  ORDERING and nothing below it could see the bug: `presetPrev`/`presetNext`
//  carry `setComponentID ("presetnav")` and are distinguished by their button
//  text, so the child walk reaches the actual production `onClick`.
//
//  THE OBSERVABLE IS TEST 48's, for the same reason it was chosen there: with a
//  MONO stimulus every trace of side energy in the output is the widener's own,
//  so a duck's dry fill collapses it. Twin processors are driven identically and
//  compared against each other rather than against a threshold -- the control
//  says what the block WOULD have been.
static void testRejectedPresetDoesNotDuck()
{
    std::printf ("State test 35: a rejected preset load causes no duck (ER-GUI-06)\n");

    auto dir = anamorph::PresetManager::presetDirectory();
    check (dir.createDirectory(), "preset directory available");
    // Sorted by name after the factory block, so B is the row immediately after A.
    auto validFile   = dir.getChildFile (juce::String ("__DuckHarnessA__") + anamorph::PresetManager::fileSuffix());
    auto foreignFile = dir.getChildFile (juce::String ("__DuckHarnessB__") + anamorph::PresetManager::fileSuffix());

    constexpr double sr = 48000.0;
    constexpr int    block = 512;

    // An ENGAGED widener on a mono stimulus: all output side energy is its own.
    auto engage = [] (AnamorphAudioProcessor& p)
    {
        setPlain (p, pid::algorithm, 0.0f);   // Haas
        setPlain (p, pid::amount,    1.0f);
        setPlain (p, pid::width,     1.6f);
        setPlain (p, pid::mix,       1.0f);
    };
    auto fill = [] (juce::AudioBuffer<float>& buf, std::mt19937& rng)
    {
        std::uniform_real_distribution<float> dist (-0.35f, 0.35f);
        for (int i = 0; i < block; ++i)
        { const float v = dist (rng); buf.setSample (0, i, v); buf.setSample (1, i, v); }
    };
    auto sideRms = [] (const juce::AudioBuffer<float>& buf)
    {
        double acc = 0.0;
        for (int i = 0; i < block; ++i)
        { const double d = (double) buf.getSample (0, i) - (double) buf.getSample (1, i); acc += d * d; }
        return std::sqrt (acc / (double) block);
    };

    // Write the valid harness preset from a settled engaged instance, then the
    // foreign one beside it.
    {
        AnamorphAudioProcessor seed;
        seed.prepareToPlay (sr, block);
        engage (seed);
        auto xml = seed.getAPVTS().copyState().createXml();
        check (xml != nullptr && validFile.replaceWithText (xml->toString()), "valid harness preset written");
    }
    check (foreignFile.replaceWithText ("<SomeOtherPluginPreset version=\"2\">\n"
                                        "  <PARAM id=\"width\" value=\"0.05\"/>\n"
                                        "</SomeOtherPluginPreset>\n"),
           "foreign harness preset written");

    // Find the two rows, and assert B really does follow A -- the whole point of
    // the step(+1) below is that it targets the FOREIGN entry.
    int idxValid = -1, idxForeign = -1;
    {
        AnamorphAudioProcessor probe;
        probe.getPresets().refresh();
        const auto& es = probe.getPresets().entries();
        for (int i = 0; i < es.size(); ++i)
        {
            if (es[i].name == "__DuckHarnessA__") idxValid = i;
            if (es[i].name == "__DuckHarnessB__") idxForeign = i;
        }
    }
    check (idxValid >= 0 && idxForeign == idxValid + 1,
           "the foreign harness row is listed immediately after the valid one");

    // Reach the real editor's Next button by the id its production code sets.
    auto findPresetNav = [] (juce::Component& root, const juce::String& text) -> juce::Button*
    {
        std::function<juce::Button* (juce::Component&)> walk = [&] (juce::Component& c) -> juce::Button*
        {
            for (int i = 0; i < c.getNumChildComponents(); ++i)
            {
                auto* kid = c.getChildComponent (i);
                if (auto* b = dynamic_cast<juce::Button*> (kid))
                    if (b->getComponentID() == "presetnav" && b->getButtonText() == text) return b;
                if (kid != nullptr) if (auto* found = walk (*kid)) return found;
            }
            return nullptr;
        };
        return walk (root);
    };

    // One run: settle an engaged widener, optionally click "next preset" (which
    // steps onto the FOREIGN row and is refused), then measure the next block.
    auto run = [&] (bool clickNext, double& outSide, bool& outClicked)
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (sr, block);
        engage (p);
        p.getPresets().refresh();
        p.getPresets().load (idxValid);          // a real, successful load: current row = A
        outClicked = false;

        std::mt19937 rng (24601);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        for (int nb = 0; nb < 60; ++nb) { fill (buf, rng); p.processBlock (buf, midi); }   // settle

        if (clickNext)
        {
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            if (auto* nav = ed != nullptr ? findPresetNav (*ed, juce::String::charToString ((juce::juce_wchar) 0x203A))
                                          : nullptr)
                if (nav->onClick) { nav->onClick(); outClicked = true; }
        }

        fill (buf, rng);
        p.processBlock (buf, midi);              // the block the duck would dry-fill
        outSide = sideRms (buf);
        return p.getPresets().currentName();
    };

    double sideRejected = 0.0, sideControl = 0.0;
    bool clicked = false, unusedClicked = false;
    const juce::String nameAfter  = run (true,  sideRejected, clicked);
    const juce::String nameControl = run (false, sideControl,  unusedClicked);

    check (clicked, "the editor's Next-preset button was found and its production onClick fired");
    std::printf ("  side RMS after a REJECTED preset step: %.6f | control (no click): %.6f | ratio %.4f\n",
                 sideRejected, sideControl, sideControl > 0.0 ? sideRejected / sideControl : 0.0);

    check (sideControl > 1.0e-3, "non-vacuity: the control block really carries the widener's side energy");
    // THE ASSERTION. A refused load must leave the audio exactly as the control's.
    check (std::abs (sideRejected - sideControl) < 1.0e-9,
           "a REJECTED preset step leaves the next audio block identical to no load at all");
    // ...and Round 24's state contract still holds through the same click.
    checkStr (nameAfter, nameControl, "a rejected preset step does not move the preset identity");

    // The other half of the invariant: a SUCCESSFUL load must still duck. Without
    // this, deleting the duck outright would pass everything above.
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (sr, block);
        engage (p);
        p.getPresets().refresh();
        std::mt19937 rng (24601);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        for (int nb = 0; nb < 60; ++nb) { fill (buf, rng); p.processBlock (buf, midi); }
        check (p.getPresets().loadFile (validFile), "the valid harness preset loads");
        fill (buf, rng);
        p.processBlock (buf, midi);
        const double sideLoaded = sideRms (buf);
        std::printf ("  side RMS after a SUCCESSFUL preset load: %.6f (control %.6f)\n", sideLoaded, sideControl);
        check (sideLoaded < 0.5 * sideControl,
               "a SUCCESSFUL preset load still opens its masking duck");
    }

    // A malformed file takes the same no-audio-side-effect path as a foreign one.
    {
        auto brokenFile = dir.getChildFile (juce::String ("__DuckHarnessC__") + anamorph::PresetManager::fileSuffix());
        check (brokenFile.replaceWithText ("<ANAMORPH><PARAM id=\"width\" value="), "malformed harness preset written");
        AnamorphAudioProcessor p;
        p.prepareToPlay (sr, block);
        engage (p);
        std::mt19937 rng (24601);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        for (int nb = 0; nb < 60; ++nb) { fill (buf, rng); p.processBlock (buf, midi); }
        check (! p.getPresets().loadFile (brokenFile), "a malformed preset is still refused");
        fill (buf, rng);
        p.processBlock (buf, midi);
        check (std::abs (sideRms (buf) - sideControl) < 1.0e-9,
               "a malformed preset load leaves the next audio block untouched too");
        check (brokenFile.deleteFile(), "malformed harness file removed");
    }

    check (validFile.deleteFile(),   "valid harness file removed");
    check (foreignFile.deleteFile(), "foreign harness file removed");
}
// ---------------------------------------------------------------------------
//  State test 34 -- a FOREIGN preset must never overwrite the current sound
//  (ER-STATE-24).
//
//  The defect. `applySoundTree` walks the PROCESSOR's parameters and looks each
//  one up in the tree by `getChildWithProperty("id", ...)`. With a foreign root
//  no child matches ANY of them, so every parameter takes the
//  "absent means default" branch that exists for a genuinely missing PARAM node
//  -- and both loaders then reported success and replaced the sound with
//  defaults. Neither loader checked the root type; only the value inside a
//  matched child was ever validated.
//
//  The contract this test pins is not invented here: it is the one ER-STATE-02
//  already settled for A/B slot payloads (`readSlot` accepts only
//  `apvts.state.getType()`, and a foreign-typed tree is refused exactly like an
//  unparsable one). A preset is the same question about the same kind of
//  payload, so it gets the same answer -- `loadFile` returns false, `load` is a
//  clean no-op, and the sound and the preset identity are both untouched.
//
//  WHY THE SENTINEL IS FIVE PARAMETERS AND NOT ONE. The failure mode IS "every
//  parameter becomes its default", so a single-parameter probe could pass by
//  accident whenever that one default happened to match. Every sentinel value
//  below is asserted to differ from its own default before the foreign load, so
//  a reset cannot hide.
static void testForeignPresetDoesNotResetSound()
{
    std::printf ("State test 34: a foreign preset never overwrites the current sound (ER-STATE-24)\n");
    AnamorphAudioProcessor p;
    p.prepareToPlay (48000.0, 512);
    auto& presets = p.getPresets();

    // Five distinct, mutually independent, deliberately non-default values.
    struct Sentinel { const char* id; float raw; };
    const Sentinel sentinelA[] = {
        { "drive",         0.31f }, { "width",     0.77f }, { "algorithm", 0.66f },
        { "monoMakerFreq", 0.42f }, { "chorusRate", 0.58f }
    };
    const Sentinel sentinelB[] = {   // a SECOND, different sound for the repeat cycle
        { "drive",         0.83f }, { "width",     0.19f }, { "algorithm", 0.33f },
        { "monoMakerFreq", 0.91f }, { "chorusRate", 0.07f }
    };

    // The comparison is against what the parameters ACTUALLY HOLD after being
    // set, captured immediately, not against the literals above: a stepped or
    // skewed parameter quantises what setRaw stores (chorusRate moves by 3e-5
    // here), and the claim under test is PRESERVATION, not equality with a
    // literal. Captured values are compared EXACTLY -- a preserved parameter is
    // not merely close to what it was, it is the same float.
    float held[5] = {};
    auto applySentinel = [&p, &held] (const Sentinel* s, int n)
    {
        for (int i = 0; i < n; ++i) setRaw (p, s[i].id, s[i].raw);
        for (int i = 0; i < n; ++i) held[i] = rawOf (p, s[i].id);
    };
    auto sameAsSentinel = [&p, &held] (const Sentinel* s, int n)
    {
        for (int i = 0; i < n; ++i)
            if (! juce::exactlyEqual (rawOf (p, s[i].id), held[i])) return false;
        return true;
    };
    // NON-VACUITY: the sentinel must differ from the defaults, or "unchanged"
    // and "reset to defaults" would be the same observation.
    auto differsFromDefaults = [&p] (const Sentinel* s, int n)
    {
        int differing = 0;
        for (int i = 0; i < n; ++i)
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p.getAPVTS().getParameter (s[i].id)))
                if (std::abs (rp->getDefaultValue() - s[i].raw) > 1.0e-3f) ++differing;
        return differing;
    };

    applySentinel (sentinelA, 5);
    check (differsFromDefaults (sentinelA, 5) == 5,
           "non-vacuity: all five sentinel values differ from their parameter defaults");

    auto dir = anamorph::PresetManager::presetDirectory();
    check (dir.createDirectory(), "preset directory available");
    const juce::String stem = "__AnamorphForeignHarness__";
    auto foreignFile = dir.getChildFile (stem + anamorph::PresetManager::fileSuffix());
    auto brokenFile  = dir.getChildFile (stem + "Broken" + anamorph::PresetManager::fileSuffix());
    auto sparseFile  = dir.getChildFile (stem + "Sparse" + anamorph::PresetManager::fileSuffix());
    auto goodFile    = dir.getChildFile (stem + "Good"   + anamorph::PresetManager::fileSuffix());

    // A well-formed document from some OTHER plug-in: a foreign root, carrying
    // children that LOOK like ours (same tag, same id attribute) so the rejection
    // cannot be attributed to the children being unrecognisable.
    const juce::String foreignXml =
        "<SomeOtherPluginPreset version=\"2\">\n"
        "  <PARAM id=\"width\" value=\"0.05\"/>\n"
        "  <PARAM id=\"drive\" value=\"0.95\"/>\n"
        "</SomeOtherPluginPreset>\n";
    check (foreignFile.replaceWithText (foreignXml), "foreign-root preset written");
    check (juce::parseXML (foreignXml) != nullptr,
           "the foreign document really is VALID XML (the rejection is about the root, not the syntax)");

    // ---- loader 1: loadFile (the OS chooser path) ----
    {
        // Printed so the failure mode is legible in the log rather than inferred
        // from a boolean: the pre-fix build shows every value moving to its default.
        std::printf ("  before foreign load:");
        for (const auto& s : sentinelA) std::printf (" %s=%.4f", s.id, (double) rawOf (p, s.id));
        std::printf ("\n");
    }
    const bool foreignAccepted = presets.loadFile (foreignFile);
    {
        std::printf ("  after  foreign load: loadFile returned %s |", foreignAccepted ? "TRUE" : "false");
        for (const auto& s : sentinelA)
        {
            auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p.getAPVTS().getParameter (s.id));
            std::printf (" %s=%.4f(default %.4f)", s.id, (double) rawOf (p, s.id),
                         rp != nullptr ? (double) rp->getDefaultValue() : -1.0);
        }
        std::printf ("\n");
    }
    check (! foreignAccepted, "loadFile REJECTS a valid document with a foreign root");
    check (sameAsSentinel (sentinelA, 5), "loadFile: the current sound is untouched by the foreign preset");
    checkStr (presets.currentName(), presets.currentName(),
              "loadFile: preset identity is not adopted from a rejected file");

    // ---- loader 2: load(index) (the preset menu path) ----
    presets.refresh();
    int foreignIndex = -1;
    for (int i = 0; i < presets.entries().size(); ++i)
        if (presets.entries()[i].name == stem) foreignIndex = i;
    check (foreignIndex >= 0, "the foreign file is listed by refresh() (the menu path really reaches it)");
    const juce::String nameBefore = presets.currentName();
    if (foreignIndex >= 0) presets.load (foreignIndex);
    check (sameAsSentinel (sentinelA, 5), "load(index): the current sound is untouched by the foreign preset");
    checkStr (presets.currentName(), nameBefore, "load(index): a rejected load does not move the preset identity");

    // ---- the repeat cycle: the rule must hold from a SECOND state too ----
    // The repository's state-mutation discipline: a guard that only survives one
    // A->B transition is not a guard. Change the sound, reject again, re-check.
    applySentinel (sentinelB, 5);
    check (differsFromDefaults (sentinelB, 5) == 5, "second sentinel also differs from every default");
    check (! presets.loadFile (foreignFile), "loadFile still rejects the foreign root from the second state");
    if (foreignIndex >= 0) presets.load (foreignIndex);
    check (sameAsSentinel (sentinelB, 5), "the second sound survives BOTH loaders unchanged");

    // ---- malformed XML keeps its existing behaviour ----
    check (brokenFile.replaceWithText ("<ANAMORPH><PARAM id=\"width\" value="), "malformed preset written");
    check (! presets.loadFile (brokenFile), "loadFile still rejects unparsable XML (unchanged behaviour)");
    check (sameAsSentinel (sentinelB, 5), "malformed XML leaves the sound untouched (unchanged behaviour)");

    // ---- a VALID Anamorph root with missing parameters keeps the documented
    //      "absent means default" behaviour: this fix must not have disabled it ----
    auto* widthP = dynamic_cast<juce::RangedAudioParameter*> (p.getAPVTS().getParameter ("width"));
    auto* driveP = dynamic_cast<juce::RangedAudioParameter*> (p.getAPVTS().getParameter ("drive"));
    check (widthP != nullptr && driveP != nullptr, "width and drive resolve");
    const juce::String sparseXml =
        "<" + p.getAPVTS().state.getType().toString() + ">\n"
        "  <PARAM id=\"width\" value=\"1.85\"/>\n"
        "</" + p.getAPVTS().state.getType().toString() + ">\n";
    check (sparseFile.replaceWithText (sparseXml), "sparse Anamorph preset written");
    check (presets.loadFile (sparseFile), "a VALID Anamorph root still LOADS, however few params it carries");
    if (widthP != nullptr)
        check (std::abs (rawOf (p, "width") - widthP->convertTo0to1 (1.85f)) < 1.0e-4f,
               "the one parameter the sparse preset carries is adopted");
    if (driveP != nullptr)
        check (std::abs (rawOf (p, "drive") - driveP->getDefaultValue()) < 1.0e-6f,
               "a parameter ABSENT from a valid Anamorph preset still takes its default (unchanged)");

    // ---- and a full, valid Anamorph preset still round-trips ----
    // The expectation is captured AFTER the save rather than from the literals:
    // the file carries PLAIN values, so a stepped parameter comes back on its own
    // step (`algorithm` returns as 0.6666667 for a 0.66 request) -- the
    // parameter's normal quantisation, which State test 8 owns and this leg has
    // no business re-litigating. What this leg must show is only that the new
    // root check did not start rejecting our own files.
    applySentinel (sentinelA, 5);
    {
        auto xml = p.getAPVTS().copyState().createXml();
        check (xml != nullptr && goodFile.replaceWithText (xml->toString()), "valid preset written");
    }
    float savedSound[5] = {};
    {
        // What a reload of that file settles on: load it once from the state it
        // was saved in, and take the reading as the expectation for the reload
        // from a DIFFERENT state below. Any discrepancy is then about the load,
        // not about float text.
        check (presets.loadFile (goodFile), "a valid Anamorph preset still loads successfully");
        for (int i = 0; i < 5; ++i) savedSound[i] = rawOf (p, sentinelA[i].id);
    }
    applySentinel (sentinelB, 5);
    check (presets.loadFile (goodFile), "...and loads again from a different current sound");
    {
        bool restored = true;
        for (int i = 0; i < 5; ++i)
            if (! juce::exactlyEqual (rawOf (p, sentinelA[i].id), savedSound[i])) restored = false;
        check (restored, "...restoring exactly the sound the file carries, from either starting state");
    }

    check (foreignFile.deleteFile(), "foreign harness file removed");
    check (brokenFile.deleteFile(),  "malformed harness file removed");
    check (sparseFile.deleteFile(),  "sparse harness file removed");
    check (goodFile.deleteFile(),    "valid harness file removed");
    presets.refresh();
}
// ---------------------------------------------------------------------------
static void testMalformedScopePersistStaysFinite()
{
    std::printf ("State test 32: a malformed scope persistence stays finite at the consumer (ER-STATE-21)\n");

    // --- Leg 1: the consumer's own contract, on the real Vectorscope.
    {
        AnamorphAudioProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        anamorph::gui::Vectorscope vs (proc.getEngine().getScopeBuffer());

        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float inf = std::numeric_limits<float>::infinity();

        vs.setPersistence (0.25f);
        checkNear ((double) vs.getPersistence(), 0.25, 1.0e-6,
                   "a legal persistence passes through unchanged");
        vs.setPersistence (5.0f);
        checkNear ((double) vs.getPersistence(), 1.0, 1.0e-6, "a too-high value clamps to 1");
        vs.setPersistence (-1.0f);
        checkNear ((double) vs.getPersistence(), 0.0, 1.0e-6, "a negative value clamps to 0");
        vs.setPersistence (nan);
        check (std::isfinite (vs.getPersistence()), "a NaN does NOT become the persistence");
        vs.setPersistence (inf);
        check (std::isfinite (vs.getPersistence()), "an infinity does NOT become the persistence");
        vs.setPersistence (-inf);
        check (std::isfinite (vs.getPersistence()), "a negative infinity does NOT become the persistence");
    }

    // --- Leg 2: end to end, through the REAL editor and a real malformed session.
#if ! (JUCE_LINUX || JUCE_BSD)
    std::printf ("  leg 2 SKIPPED off Linux -- headless editor construction is unverified there (KI-007)\n");
#else
    // A genuine modern save, with only this one field replaced.
    juce::MemoryBlock base;
    {
        AnamorphAudioProcessor src;
        src.prepareToPlay (48000.0, 512);
        src.getStateInformation (base);
    }

    for (const char* bad : { "nan", "-1.0", "-0.5", "inf" })
    {
        auto xml = BlobCodec::unwrap (base);
        if (xml == nullptr) { check (false, "the real save round-trips through the blob codec"); return; }
        auto* internal = xml->getChildByName ("ANAMORPH_INTERNAL");
        if (internal == nullptr) { check (false, "the save carries an ANAMORPH_INTERNAL node"); return; }
        internal->setAttribute ("int_scopePersist", bad);
        auto mutated = BlobCodec::wrap (*xml);

        AnamorphAudioProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        proc.setStateInformation (mutated.getData(), (int) mutated.getSize());

        auto* raw = proc.createEditor();
        auto* ed  = dynamic_cast<AnamorphAudioProcessorEditor*> (raw);
        check (ed != nullptr, "editor constructs over the malformed session");
        if (ed == nullptr) { delete raw; return; }

        // The scope is a direct child of the editor (addAndMakeVisible in the ctor).
        anamorph::gui::Vectorscope* vs = nullptr;
        for (int i = 0; i < ed->getNumChildComponents() && vs == nullptr; ++i)
            vs = dynamic_cast<anamorph::gui::Vectorscope*> (ed->getChildComponent (i));
        check (vs != nullptr, "the editor's vectorscope is reachable (non-vacuity)");

        if (vs != nullptr)
        {
            const float p = vs->getPersistence();
            std::printf ("  int_scopePersist=%-6s -> tree keeps %-6s, scope persistence %.4f\n", bad,
                         proc.getInternal().copyState()["int_scopePersist"].toString().toRawUTF8(),
                         (double) p);
            check (std::isfinite (p),
                   "a malformed persistence in the session leaves the scope FINITE");
            check (p >= 0.0f && p <= 1.0f, "...and inside the documented 0..1 domain");
        }
        proc.editorBeingDeleted (ed);
        delete ed;
    }
#endif
}

// ---------------------------------------------------------------------------
//  State test 33 -- a modern Setting that is PRESENT but invalid is REPAIRED on
//  restore, and the repaired value is what gets persisted (ER-STATE-21,
//  maintainer decision of 2026-09-02, "Policy B").
//
//  Rounds 16 and 17 established the evidence and left the contract open: a
//  present-but-invalid value was adopted verbatim, all nineteen malformed inputs
//  survived into the next save, and the one unclamped consumer could be reached
//  by a non-finite one (fixed at the consumer in round 17, which stays as the
//  backstop). The maintainer then chose repair-at-restore-and-persist, and this
//  test is that policy's contract:
//
//    * a VALID present value is preserved exactly -- the controls below are what
//      stop a fix that merely resets everything to defaults from passing;
//    * a finite OUT-OF-DOMAIN value clamps to the nearest valid one;
//    * anything not usable as a number -- malformed text, non-finite -- becomes
//      the field's documented default;
//    * an ABSENT field keeps taking its documented default (ER-STATE-18), which
//      the last leg pins so the two rules cannot be confused; and
//    * the repaired value is written into the tree, so the NEXT SAVE carries it
//      and a RELOAD of that save reads back the same thing.
//
//  The save leg is the one that makes this Policy B rather than Policy "clamp at
//  the read": it asserts the malformed text is GONE from the persisted state.
// ---------------------------------------------------------------------------
static void testModernSettingsAreRepairedOnRestore()
{
    std::printf ("State test 33: present-but-invalid modern Settings are repaired and persisted (ER-STATE-21)\n");

    // A genuine modern save, used as the carrier for every mutation.
    juce::MemoryBlock base;
    {
        AnamorphAudioProcessor src;
        src.prepareToPlay (48000.0, 512);
        src.getStateInformation (base);
    }

    struct Case
    {
        const char* field;
        const char* written;
        double      expected;   // the repaired value, as a double (booleans: 0 / 1)
        bool        valid;      // true = a valid present value, which must be PRESERVED
    };
    const Case cases[] = {
        // int_oversample: ComboBox ids 1..4, default 1
        { "int_oversample",   "2",     2.0,  true  },   // control: valid, preserved
        { "int_oversample",   "4",     4.0,  true  },   // control: the domain's top
        { "int_oversample",   "99",    4.0,  false },   // finite out of range -> nearest
        { "int_oversample",   "-5",    1.0,  false },
        { "int_oversample",   "0",     1.0,  false },
        { "int_oversample",   "2.7",   2.0,  false },   // fractional id -> truncation after clamp
        { "int_oversample",   "abc",   1.0,  false },   // malformed -> default
        { "int_oversample",   "nan",   1.0,  false },   // non-finite -> default
        { "int_oversample",   "inf",   1.0,  false },
        // int_uiScale: ComboBox ids 1..5, default 3
        { "int_uiScale",      "5",     5.0,  true  },   // control
        { "int_uiScale",      "99",    5.0,  false },
        { "int_uiScale",      "0",     1.0,  false },
        { "int_uiScale",      "abc",   3.0,  false },
        { "int_uiScale",      "-inf",  3.0,  false },
        // int_scopePersist: 0..1, default 0.5
        { "int_scopePersist", "0.25",  0.25, true  },   // control
        { "int_scopePersist", "5.0",   1.0,  false },
        { "int_scopePersist", "-1.0",  0.0,  false },
        { "int_scopePersist", "nan",   0.5,  false },
        { "int_scopePersist", "inf",   0.5,  false },
        { "int_scopePersist", "1e39",  0.5,  false },   // finite as double, infinite as float
        { "int_scopePersist", "abc",   0.5,  false },
        // booleans
        { "int_metersOn",     "1",     1.0,  true  },   // control
        { "int_metersOn",     "0",     0.0,  true  },   // control
        { "int_metersOn",     "abc",   0.0,  false },   // malformed -> default false
        // A boolean has exactly two valid spellings, "0" and "1" -- the two this
        // plug-in's own writer emits. Every other number is MALFORMED and takes the
        // documented default; it does not get coerced to true (ER-STATE-22). The
        // negative cases are the ones that used to silently ENABLE a setting.
        { "int_metersOn",     "2",     0.0,  false },
        { "int_metersOn",     "-1",    0.0,  false },
        { "int_metersOn",     "-2",    0.0,  false },
        { "int_metersOn",     "0.5",   0.0,  false },
        { "int_tooltipsOn",   "1",     1.0,  true  },   // control
        { "int_tooltipsOn",   "0",     0.0,  true  },   // control
        { "int_tooltipsOn",   "maybe", 0.0,  false },
        { "int_tooltipsOn",   "-1",    0.0,  false },
        { "int_tooltipsOn",   "-0.5",  0.0,  false },
        { "int_uiAnimations", "0",     0.0,  true  },   // control: OFF must survive
        { "int_uiAnimations", "1",     1.0,  true  },   // control
        { "int_uiAnimations", "abc",   1.0,  false },   // malformed -> default true
        { "int_uiAnimations", "-1",    1.0,  false },   // ...and so does a negative
        { "int_uiAnimations", "-2",    1.0,  false },
        { "int_uiAnimations", "2",     1.0,  false }
    };

    auto internalTextOf = [] (const juce::MemoryBlock& blob, const char* field)
    {
        if (auto x = BlobCodec::unwrap (blob))
            if (auto* n = x->getChildByName ("ANAMORPH_INTERNAL"))
                return n->getStringAttribute (field);
        return juce::String();
    };

    int repaired = 0, preserved = 0;
    for (const auto& c : cases)
    {
        auto xml = BlobCodec::unwrap (base);
        if (xml == nullptr) { check (false, "the carrier save round-trips through the blob codec"); return; }
        auto* internal = xml->getChildByName ("ANAMORPH_INTERNAL");
        if (internal == nullptr) { check (false, "the carrier save carries an ANAMORPH_INTERNAL node"); return; }
        internal->setAttribute (c.field, c.written);
        auto mutated = BlobCodec::wrap (*xml);

        // (1) Restore: the LIVE value must be the repaired one.
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        p.setStateInformation (mutated.getData(), (int) mutated.getSize());
        const double live = (double) p.getInternal().copyState()[juce::Identifier (c.field)];
        checkNear (live, c.expected, 1.0e-6,
                   c.valid ? "a VALID present value is preserved exactly"
                           : "an invalid present value is repaired to its documented resolution");

        // (2) Save: the malformed text must be GONE from the persisted state.
        juce::MemoryBlock resaved;
        p.getStateInformation (resaved);
        const auto savedText = internalTextOf (resaved, c.field);
        if (! c.valid)
            check (savedText != juce::String (c.written),
                   "...and the malformed text is NOT what the next save writes");

        // (3) Reload: the same value comes back, so the repair is stable.
        AnamorphAudioProcessor q;
        q.prepareToPlay (48000.0, 512);
        q.setStateInformation (resaved.getData(), (int) resaved.getSize());
        const double reloaded = (double) q.getInternal().copyState()[juce::Identifier (c.field)];
        checkNear (reloaded, c.expected, 1.0e-6,
                   "...and a reload of that save reads back the same value");

        if (c.valid) ++preserved; else ++repaired;
        std::printf ("  %-18s %-6s -> live %-8.4g saved \"%s\"%s\n", c.field, c.written, live,
                     savedText.toRawUTF8(), c.valid ? "   (valid, preserved)" : "");
    }
    std::printf ("  => %d invalid value(s) repaired, %d valid value(s) preserved\n", repaired, preserved);

    // (4) ABSENT is a different rule and must stay different (ER-STATE-18): the
    //     documented default, NOT a repair of something that is not there.
    {
        auto xml = BlobCodec::unwrap (base);
        if (xml == nullptr) return;
        auto* internal = xml->getChildByName ("ANAMORPH_INTERNAL");
        if (internal == nullptr) return;
        internal->removeAttribute ("int_uiScale");
        auto stripped = BlobCodec::wrap (*xml);

        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        p.getInternal().uiScaleValue().setValue (5);      // a distinguishable previous value
        p.setStateInformation (stripped.getData(), (int) stripped.getSize());
        checkNear ((double) p.getInternal().copyState()["int_uiScale"], 3.0, 1.0e-6,
                   "an ABSENT field still takes its documented default, not the previous session's");
    }
}

// ---------------------------------------------------------------------------
//  RISK-008 probe (round 18): what happens to a pending D-1 latency request when
//  nothing is servicing the JUCE message queue?
//
//  SYNTHETIC, AND LABELLED AS SUCH. The CAUSE this models -- a Linux VST3 host
//  that supplies its `IRunLoop` only through `IPlugFrame`, so JUCE detaches the
//  event loop when the editor closes and never restarts its internal message
//  thread -- is established by reading the pinned wrapper, not by running one:
//  no Linux VST3 host is installed in this environment, and none was available to
//  test. What this probe reproduces faithfully is the CONSEQUENCE of that state,
//  which is the part the risk turns on: with the queue unserviced, does a request
//  made off the message thread stay pending, and is it delivered when servicing
//  resumes?
//
//  That consequence is exact rather than modelled, because `juce::Timer` delivers
//  through the queue and nothing else: the timer thread POSTS a `CallTimersMessage`
//  and the message thread runs the callbacks (pinned `juce_Timer.cpp`). A console
//  harness never runs a dispatch loop, so "not pumping" here IS "queue unserviced"
//  -- the same reason State tests 27, 30 and 31 have to pump explicitly to make
//  the D-1 timer fire at all.
//
//  No sleep is used as proof of anything. The negative phase asserts a STATE (the
//  reported latency has not moved) that cannot become true later without the
//  queue being serviced; its deadline only bounds how long the phase runs.
// ---------------------------------------------------------------------------
static int runRisk008Probe()
{
    std::printf ("RISK-008 probe: a pending latency request against an UNSERVICED message queue\n");
    std::printf ("  (synthetic: models the editor-closed state a run-loop detach leaves behind;\n");
    std::printf ("   the wrapper lifecycle that produces it is established by code reading)\n\n");

    AnamorphAudioProcessor p;
    p.getInternal().oversampleValue().setValue (2);      // 2x: latency moves with drive
    auto* drive = p.getAPVTS().getParameter (pid::drive);

    drive->setValueNotifyingHost (0.0f);
    p.prepareToPlay (48000.0, 512);
    const int lowLat = p.getLatencySamples();
    drive->setValueNotifyingHost (1.0f);
    p.prepareToPlay (48000.0, 512);
    const int highLat = p.getLatencySamples();
    drive->setValueNotifyingHost (0.0f);
    p.prepareToPlay (48000.0, 512);
    std::printf ("  reported latency moves %d -> %d with drive (non-vacuity: %s)\n",
                 lowLat, highLat, lowLat != highLat ? "yes" : "NO -- probe is vacuous");
    if (lowLat == highLat) return 1;

    const int before = p.getLatencySamples();

    // The request, from a thread that is not the message thread -- the shape host
    // automation of Drive takes under VST3 (KI-027), and the one D-1 defers.
    const auto requestedAt = juce::Time::getMillisecondCounterHiRes();
    std::thread worker ([&] { drive->setValueNotifyingHost (1.0f); });
    worker.join();

    // --- Phase A: queue UNSERVICED (the editor-closed state). ---------------
    // Deliberately NOT pumping. Poll the observable only.
    constexpr int kUnservicedMs = 1000;   // 20 timer periods
    int delivered = -1;
    for (int elapsed = 0; elapsed < kUnservicedMs; elapsed += 25)
    {
        if (p.getLatencySamples() != before) { delivered = elapsed; break; }
        std::this_thread::sleep_for (std::chrono::milliseconds (25));
    }
    const bool stalledWhileUnserviced = (delivered < 0);
    std::printf ("  phase A -- queue unserviced for %d ms (%d timer periods): reported latency %s\n",
                 kUnservicedMs, kUnservicedMs / 50,
                 stalledWhileUnserviced ? "UNCHANGED (request still pending)"
                                        : "changed -- the request was served without servicing");

    // --- Phase B: servicing resumes (the editor is reopened). ---------------
    int servedAfterMs = -1;
    for (int elapsed = 0; elapsed < 2000; elapsed += 5)
    {
        juce::Timer::callPendingTimersSynchronously();      // the run loop, restored
        if (p.getLatencySamples() != before)
        {
            servedAfterMs = (int) (juce::Time::getMillisecondCounterHiRes() - requestedAt);
            break;
        }
        std::this_thread::sleep_for (std::chrono::milliseconds (5));
    }

    std::printf ("  phase B -- servicing resumed: reported latency %s%s\n",
                 servedAfterMs >= 0 ? "delivered" : "STILL not delivered",
                 servedAfterMs >= 0 ? "" : " (unexpected)");
    if (servedAfterMs >= 0)
        std::printf ("  request -> delivery: %d ms total, of which %d ms was the unserviced window\n",
                     servedAfterMs, kUnservicedMs);
    std::printf ("  now reported: %d (was %d, the settled state predicts %d)\n",
                 p.getLatencySamples(), before, highLat);

    std::printf ("\n  => %s\n", stalledWhileUnserviced && servedAfterMs >= 0
                 ? "the request SURVIVES an unserviced window and is delivered when servicing resumes;\n"
                   "     it is deferred, not dropped -- the host is stale for exactly that window"
                 : "inconclusive -- see the phase lines above");
    std::printf ("  EVIDENCE LIMIT: no real Linux VST3 host is available here, so this shows what a\n"
                 "  detached run loop COSTS, not that any shipping host detaches one. The real-host\n"
                 "  half is recorded separately (FUTURE_RISKS RISK-008): on Linux in REAPER the\n"
                 "  latency updates with the editor both open and closed.\n");
    return 0;
}

// ---------------------------------------------------------------------------
//  Restore fade-in probe (round 20): does a NON-DEFAULT restored session reach
//  its settled sound from the first sample, or glide into it?
//
//  MEASURES, ASSERTS NOTHING. Drives the ordinary host order -- restore, THEN
//  activate -- on a fresh instance, and prints the per-block deviation of the
//  output from the input. A module that is already at its restored target
//  deviates by the same amount in block 1 as in block 12; one that glides in
//  from its default starts near zero and climbs.
//
//  WHAT THE RATIO DOES AND DOES NOT SEPARATE. Two different things hold block 1
//  below the settled level, and this metric sees their sum:
//    (a) a SMOOTHER gliding from the wrong start -- the ER-DSP-09 defect, fixed;
//    (b) empty DELAY-LINE and FILTER history filling up -- not a defect at all.
//        prepare() clears that history by contract, and Haas's own 28 ms line is
//        longer than the 512-sample block this metric's first point covers.
//  So the ratio RISES when the fix lands but does not reach 1.0, and a ratio
//  below 1.0 here is not by itself evidence of a defect. Measured block1/block12,
//  before -> after the four engine snapToTargets() calls: Haas 0.17 -> 0.72,
//  Velvet 0.09 -> 0.18, Chorus 0.29 -> 0.68, Dimension-D 0.39 -> 0.90, Mono Maker
//  0.35 -> 0.58. (Velvet moves least because its presence follower and sparse-tap
//  history dominate its own first block; that settling is (b), and is unchanged.)
//  The DISCRIMINATING instrument is DSP Test 49, which compares each module
//  against a REFERENCE settled on the same targets and so cancels (b) exactly.
//  This probe is the magnitude, in the product's own terms; the test is the rule.
// ---------------------------------------------------------------------------
static int runRestoreFadeProbe()
{
    std::printf ("restored-session fade-in probe (round 20)\n");
    std::printf ("=========================================\n\n");

    constexpr double sr = 48000.0;
    constexpr int    bs = 512;
    constexpr int    kBlocks = 12;

    struct Case { const char* name; int algo; const char* extraId; float extraRaw; bool monoCase; };
    const Case cases[] = {
        { "Haas",        0, pid::haasDelay,     28.0f, false },
        { "Velvet",      1, pid::velvetDensity,  0.9f, false },
        { "Chorus",      2, pid::chorusDepth,    0.9f, false },
        { "Dimension-D", 3, pid::chorusDepth,    0.9f, false },
        // Downward, and only one octave: the crossover glides at 8 octaves/second,
        // so 120 -> 60 completes in ~125 ms (about 12 blocks) and is visible here.
        // A 90 Hz tone sits BELOW the default 120 (mono'd, little side energy) and
        // ABOVE the restored 60 (stereo, full side energy), so a glide shows as
        // side energy CLIMBING out of the first block instead of starting high.
        { "Mono Maker",  0, pid::monoMakerFreq,  60.0f, true  },
    };

    for (const auto& c : cases)
    {
        // A real non-default session, saved by one instance...
        juce::MemoryBlock blob;
        {
            AnamorphAudioProcessor src;
            src.prepareToPlay (sr, bs);
            setPlain (src, pid::algorithm, (float) c.algo);
            setPlain (src, pid::mix, 1.0f);
            setPlain (src, c.extraId, c.extraRaw);
            // Mono Maker is an ADVANCED-mode control: `toEngine` only maps it when
            // advancedMode is on, so without this the module never runs at all.
            if (c.monoCase) { setPlain (src, pid::advancedMode, 1.0f);
                              setPlain (src, pid::monoMakerOn, 1.0f); setPlain (src, pid::amount, 0.0f); }
            else            { setPlain (src, pid::amount, 1.0f); }
            src.getStateInformation (blob);
        }

        // ...restored into a FRESH one in the ordinary host order: state, then activate.
        AnamorphAudioProcessor p;
        p.setStateInformation (blob.getData(), (int) blob.getSize());
        p.prepareToPlay (sr, bs);

        std::printf ("  [restored: algorithm=%.0f amount=%.3f mix=%.3f %s=%.3f monoOn=%.0f]\n",
                     plainOf (p, pid::algorithm), plainOf (p, pid::amount), plainOf (p, pid::mix),
                     c.extraId, plainOf (p, c.extraId), plainOf (p, pid::monoMakerOn));

        juce::Random rng (20260902);
        juce::AudioBuffer<float> buf (2, bs), dry (2, bs);
        juce::MidiBuffer midi;
        std::printf ("  %-12s per-block %s:\n", c.name,
                     c.monoCase ? "SIDE energy rms(L-R) (a widening crossover glide makes it fall)"
                                : "deviation rms(out-in) (a fading effect makes it rise)");
        double first = 0.0, last = 0.0;
        for (int k = 1; k <= kBlocks; ++k)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < bs; ++i)
                {
                    // Decorrelated per channel so the Mono Maker case has real side energy,
                    // with a low-frequency component the 120..400 Hz crossover moves over.
                    const double t = (double) i / sr;
                    const double toneHz = c.monoCase ? 90.0 : 220.0;
                    const float v = (float) (0.30 * std::sin (2.0 * 3.14159265 * toneHz * t
                                                              + (ch == 1 ? 1.1 : 0.0))
                                           + (c.monoCase ? 0.0 : 0.15)
                                             * (rng.nextFloat() * 2.0f - 1.0f));
                    buf.setSample (ch, i, v);
                    dry.setSample (ch, i, v);
                }
            p.processBlock (buf, midi);

            double acc = 0.0;
            int n = 0;
            for (int i = 0; i < bs; ++i)
            {
                if (c.monoCase)
                {
                    const double d = (double) buf.getSample (0, i) - (double) buf.getSample (1, i);
                    acc += d * d; ++n;
                }
                else
                {
                    // BOTH channels: Haas delays only one side by construction, so a
                    // single-channel metric is blind to it.
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        const double d = (double) buf.getSample (ch, i) - (double) dry.getSample (ch, i);
                        acc += d * d; ++n;
                    }
                }
            }
            const double v = std::sqrt (acc / (double) juce::jmax (1, n));
            if (k == 1) first = v;
            if (k == kBlocks) last = v;
            std::printf ("      block %2d: %.6f\n", k, v);
        }
        const double ratio = last > 1.0e-9 ? first / last : 0.0;
        std::printf ("    => block1 / block%d = %.4f  %s\n\n", kBlocks, ratio,
                     ratio > 0.90 && ratio < 1.10
                       ? "(first block already at the settled level)"
                       : "(first block below settled -- smoother glide and/or history fill;\n"
                         "       see the header: this metric does not separate the two)");
    }
    return 0;
}

// ---------------------------------------------------------------------------
//  ER-STATE-23 probe (round 20): does pairing RESTORE with PREPARE add any race
//  beyond the four RISK-007/D-2 already records?
//
//  The review finding says the latency atomics "do not synchronize concurrent
//  restore, prepare, A/B, preset, or engine state". They do not, and were never
//  meant to -- they carry the latency REQUEST and nothing else. The real question
//  is whether those other states race, and whether that is anything D-2 has not
//  already recorded and deferred. This probe answers the one pairing D-2's scope
//  does NOT mention: a host calling `setStateInformation` and `prepareToPlay`
//  from two different threads while the editor tick reads.
//
//  Run under ThreadSanitizer. The verdict is the REPORT SET, compared against the
//  four D-2 already measured (`--state-thread-probe`): abActive, the abUndo
//  vector twice, and a juce::String refcount exchange. Anything else is new.
//  Like its sibling, if the race is real this probe's own execution is undefined
//  behaviour, which is why it is opt-in and never part of the suite.
// ---------------------------------------------------------------------------
static int runStatePrepareRaceProbe()
{
    std::printf ("ER-STATE-23 probe: restore + prepare + editor reads, three threads\n");
    std::printf ("  (run under ThreadSanitizer; compare the report set against the four D-2 records)\n");

    AnamorphAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    proc.getPresets().load (1);
    setRaw (proc, "width", 0.42f);
    proc.pollUndoCoalesce();

    juce::MemoryBlock blob;
    proc.getStateInformation (blob);

    constexpr int kIterations = 300;
    std::atomic<bool> go { false };

    // Host thread A: state calls off the main thread (the AU autosave shape).
    std::thread stateThread ([&]
    {
        while (! go.load (std::memory_order_acquire)) { }
        for (int i = 0; i < kIterations; ++i)
        {
            proc.setStateInformation (blob.getData(), (int) blob.getSize());
            juce::MemoryBlock out;
            proc.getStateInformation (out);
        }
    });

    // Host thread B: activation off the main thread, alternating the spec so each
    // call really re-prepares the engine rather than being a no-op.
    std::thread prepareThread ([&]
    {
        while (! go.load (std::memory_order_acquire)) { }
        for (int i = 0; i < kIterations; ++i)
            proc.prepareToPlay ((i & 1) ? 44100.0 : 48000.0, (i & 1) ? 256 : 512);
    });

    go.store (true, std::memory_order_release);
    for (int i = 0; i < kIterations; ++i)
    {
        auto& pm = proc.getPresets();
        const juce::String liveName = pm.currentName();
        const bool liveDirty = pm.isDirty();
        proc.pollUndoCoalesce();
        const bool u = proc.canUndo(), r = proc.canRedo();
        const int  slot = proc.abActiveSlot();
        if (liveName.isEmpty() && liveDirty && u && r && slot < 0)
            std::printf ("  (unreachable, keeps the reads live)\n");
    }

    stateThread.join();
    prepareThread.join();
    std::printf ("  probe finished: %d restore + %d prepare iterations against %d editor ticks\n",
                 kIterations, kIterations, kIterations);
    std::printf ("  verdict comes from the sanitizer's report SET, not from this exit code\n");
    return 0;
}

// ===========================================================================
//  D-2 / RISK-007 (ADR-0036): the program-state ownership contract.
//
//  Five deterministic tests and one stress probe, over one model of the host:
//  the SOUND (the APVTS) is applied on whichever thread the host restores from,
//  the PROGRAM METADATA (preset name / identity / baseline, the A/B slot set, the
//  undo history, the committed baseline, the Settings tree) is message-thread
//  state that an off-message-thread restore hands over as one immutable value,
//  adopted at the next drain point -- the editor's tick (pollUndoCoalesce), the
//  processor's 20 Hz timer, or any message-thread entry point that mutates it.
//  These tests drain through pollUndoCoalesce() deliberately: it is the editor's
//  own path, it is synchronous, and it is what the pre-D-2 probes already
//  called, so this file measures the pre-fix tree with the same instruments.
//
//  What the audio thread sees is out of these tests' reach on purpose: its inputs
//  are JUCE's per-parameter atomics, InternalState's oversampling atomic and the
//  solo mask, published exactly as before D-2 and masked across a bulk change by
//  ADR-0004's duck. The tests below assert the two halves D-2 adds -- the
//  oversampling atomic stored synchronously by the restoring thread, and the
//  audio path staying finite with restores landing under it -- and the probe
//  puts the audio thread under ThreadSanitizer with everything else.
// ===========================================================================
namespace d2
{
    static juce::MemoryBlock saveOf (AnamorphAudioProcessor& p)
    {
        juce::MemoryBlock b;
        p.getStateInformation (b);
        return b;
    }

    static void restoreFrom (AnamorphAudioProcessor& p, const juce::MemoryBlock& b)
    {
        p.setStateInformation (b.getData(), (int) b.getSize());
    }

    // Run `f` on a thread that is not the message thread and wait for it -- the
    // sequenced shape of a host that calls state functions from its own thread.
    template <typename F>
    void offMessageThread (F&& f)
    {
        std::thread t (std::forward<F> (f));
        t.join();
    }

    // A session with two distinguishable A/B slots, its own preset name and a
    // chosen Oversampling, authored on the message thread by an instance of its own.
    struct Session
    {
        juce::MemoryBlock blob;
        juce::String name;
        float widthA = 0.0f, widthB = 0.0f;
        int   active = 0;
        int   oversampleId = 1;   // 1-based combo id
    };

    static Session author (const char* name, float widthA, float widthB, int active, int oversampleId)
    {
        AnamorphAudioProcessor a;
        a.prepareToPlay (48000.0, 512);
        a.getInternal().oversampleValue().setValue (oversampleId);
        setRaw (a, "width", widthA);
        a.abCopyToOther();            // B := A's sound for now
        a.abSwitchTo (1);
        setRaw (a, "width", widthB);  // B's own sound
        a.abSwitchTo (0);             // stores B, applies A
        if (active == 1) a.abSwitchTo (1);
        a.getPresets().setMeta (name, "d2-baseline-" + juce::String (name),
                                anamorph::PresetManager::Selection());
        Session sn;
        sn.blob = saveOf (a);
        sn.name = name;
        sn.widthA = widthA; sn.widthB = widthB; sn.active = active; sn.oversampleId = oversampleId;
        return sn;
    }

    // Reads exactly what the editor's tick reads, as one coherent view.
    struct View
    {
        int active; juce::String name; bool canUndo, canRedo;
        static View of (AnamorphAudioProcessor& p)
        {
            return { p.abActiveSlot(), p.getPresets().currentName(), p.canUndo(), p.canRedo() };
        }
        bool matches (const Session& sn) const { return active == sn.active && name == sn.name; }
    };

    // Bounded poll for the processor's own 20 Hz timer (the same shape State tests
    // 22/27/30 use: a tight callPendingTimersSynchronously() loop fires nothing).
    template <typename Pred>
    bool waitFor (Pred&& done, int deadlineMs = 2000)
    {
        for (int elapsed = 0; elapsed < deadlineMs; elapsed += 5)
        {
            juce::Timer::callPendingTimersSynchronously();
            if (done()) return true;
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
        }
        return done();
    }

    // A noise block the audio-thread loops feed, and a finiteness scan of the output.
    static bool processStaysFinite (AnamorphAudioProcessor& p, int blocks, juce::AudioBuffer<float>& buf, std::mt19937& rng)
    {
        std::uniform_real_distribution<float> dist (-0.5f, 0.5f);
        juce::MidiBuffer midi;
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
                for (int i = 0; i < buf.getNumSamples(); ++i)
                    buf.setSample (ch, i, dist (rng));
            p.processBlock (buf, midi);
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
                for (int i = 0; i < buf.getNumSamples(); ++i)
                    if (! std::isfinite (buf.getSample (ch, i))) return false;
        }
        return true;
    }
}

// ---------------------------------------------------------------------------
//  State test 37 -- A/B state under off-message-thread restores (D-2, contract A).
//
//  A host thread restores two sessions with different A/B slot sets, names and
//  active indices, in turn; the message thread cycles A -> B -> A -> B between
//  them and asserts that what it sees is always ONE session's slot set, never a
//  mix. The load-bearing assertions:
//    * BEFORE the drain the message thread still sees the PREVIOUS program whole
//      (its active index, its name, its undo history) -- the ownership contract:
//      an off-thread restore changes nothing this thread owns until this thread
//      adopts it -- while the sound (the parameter atomics) has already moved;
//    * AFTER the drain it sees the restored program whole, and both slots hold
//      that session's two sounds;
//    * a save from the host thread, taken inside the pending window, is byte-
//      identical to the save the message thread writes after adopting -- the
//      restoring thread's own view describes exactly what the owner will own;
//    * a save from the host thread after the adoption is byte-identical to the
//      owner's -- the published snapshot is current.
//  On the pre-D-2 tree the first assertion fails (the restore wrote the owner's
//  members from the host thread) and the byte comparisons are the data race
//  --state-thread-probe reports.
// ---------------------------------------------------------------------------
static void testAbStateCoherentAcrossOffThreadRestores()
{
    std::printf ("State test 37: A/B state stays coherent across off-message-thread restores (D-2)\n");

    const auto X = d2::author ("D2-X", 0.10f, 0.90f, 0, 1);
    const auto Y = d2::author ("D2-Y", 0.20f, 0.80f, 1, 1);
    check (X.blob != Y.blob, "non-vacuity: the two sessions differ");

    AnamorphAudioProcessor p;
    p.prepareToPlay (48000.0, 512);

    // Real undo history, so "cleared by the adoption" cannot pass vacuously.
    {
        auto* drive = p.getAPVTS().getParameter (pid::drive);
        drive->beginChangeGesture(); drive->setValueNotifyingHost (0.42f); drive->endChangeGesture();
        p.pollUndoCoalesce();
        check (p.canUndo(), "gesture created undo history");
    }

    const d2::Session* previous = nullptr;
    for (int i = 0; i < 6; ++i)
    {
        const auto& sn = (i & 1) ? Y : X;
        const auto before = d2::View::of (p);

        juce::MemoryBlock hostSaveInWindow;
        d2::offMessageThread ([&]
        {
            d2::restoreFrom (p, sn.blob);
            hostSaveInWindow = d2::saveOf (p);   // a save from the same host thread, BEFORE any drain
        });

        // The sound moved on the restoring thread; the program did not, yet.
        const float liveWidth = sn.active == 0 ? sn.widthA : sn.widthB;
        checkNear ((double) rawOf (p, "width"), (double) liveWidth, 1.0e-6,
                   "the sound is applied synchronously on the restoring thread");
        const auto pending = d2::View::of (p);
        check (pending.active == before.active && pending.name == before.name
               && pending.canUndo == before.canUndo && pending.canRedo == before.canRedo,
               "before the drain the message thread still sees the PREVIOUS program, whole");
        if (previous != nullptr)
            check (pending.matches (*previous), "...and that previous program is the last adopted session");

        p.pollUndoCoalesce();   // the editor's tick: the drain
        const auto adopted = d2::View::of (p);
        check (adopted.matches (sn), "after the drain the restored program is visible, whole");
        check (! adopted.canUndo && ! adopted.canRedo, "the adoption cleared the undo history");

        const auto ownerSave = d2::saveOf (p);
        check (hostSaveInWindow == ownerSave,
               "a host-thread save inside the pending window equals the owner's save after adoption");

        juce::MemoryBlock hostSaveAfter;
        d2::offMessageThread ([&] { hostSaveAfter = d2::saveOf (p); });
        check (hostSaveAfter == ownerSave, "a host-thread save after adoption equals the owner's save");

        // A -> B -> A -> B on the owner's thread: each leg shows that session's slot.
        const int other = 1 - sn.active;
        const float otherWidth = other == 0 ? sn.widthA : sn.widthB;
        for (int leg = 0; leg < 2; ++leg)
        {
            p.abSwitchTo (other);
            checkNear ((double) rawOf (p, "width"), (double) otherWidth, 1.0e-6, "A/B cycle: the other slot's sound");
            p.abSwitchTo (sn.active);
            checkNear ((double) rawOf (p, "width"), (double) liveWidth, 1.0e-6, "A/B cycle: back to the restored slot's sound");
        }
        check (p.abActiveSlot() == sn.active && p.getPresets().currentName() == sn.name,
               "the cycle returns to the restored session's program");
        previous = &sn;
    }
}

// ---------------------------------------------------------------------------
//  State test 38 -- restore B -> C -> B -> C with the audio thread running
//  (D-2, contract B). The two sessions differ in Oversampling as well as in
//  their sound, so the one part of a restore the AUDIO thread and prepareToPlay
//  read -- InternalState's oversampling atomic -- is observable: it must be
//  stored on the restoring thread, before setStateInformation returns, while the
//  Settings TREE (the editor's binding) follows at the drain. The reported
//  latency must end up at what a message-thread restore of the same session
//  reports; the audio path must stay finite throughout; and a save after the
//  final adoption must be byte-identical to the session restored (State test 3's
//  fixed point, now reached through the off-thread path).
// ---------------------------------------------------------------------------
static void testRestoreCyclesUnderRunningAudio()
{
    std::printf ("State test 38: restore B -> C -> B -> C off the message thread with audio running (D-2)\n");

    const auto B = d2::author ("D2-B", 0.30f, 0.70f, 0, 2);   // 2x
    const auto C = d2::author ("D2-C", 0.60f, 0.40f, 1, 1);   // Off

    // Truth: what a message-thread restore reports for each session.
    auto truthFor = [] (const d2::Session& sn)
    {
        AnamorphAudioProcessor q;
        q.prepareToPlay (48000.0, 512);
        d2::restoreFrom (q, sn.blob);
        q.prepareToPlay (48000.0, 512);
        return q.getLatencySamples();
    };
    const int latB = truthFor (B), latC = truthFor (C);
    check (latB != latC, "non-vacuity: the two sessions report different latencies");

    AnamorphAudioProcessor p;
    p.prepareToPlay (48000.0, 512);

    std::atomic<bool> stopAudio { false };
    std::atomic<bool> audioFinite { true };
    std::thread audio ([&]
    {
        juce::AudioBuffer<float> buf (2, 512);
        std::mt19937 rng (0x0d2u);
        while (! stopAudio.load (std::memory_order_acquire))
            if (! d2::processStaysFinite (p, 4, buf, rng))
                audioFinite.store (false, std::memory_order_relaxed);
    });

    for (int i = 0; i < 4; ++i)
    {
        const auto& sn = (i & 1) ? C : B;
        d2::offMessageThread ([&] { d2::restoreFrom (p, sn.blob); });

        check (p.getInternal().oversampleIndex() == sn.oversampleId - 1,
               "the oversampling atomic is stored on the restoring thread, before the restore returns");

        p.pollUndoCoalesce();
        check ((int) p.getInternal().copyState()[anamorph::iid::oversample] == sn.oversampleId,
               "the Settings tree carries the restored Oversampling after the drain");
        check (p.abActiveSlot() == sn.active && p.getPresets().currentName() == sn.name,
               "the restored program is visible after the drain");

        const int want = (i & 1) ? latC : latB;
        const bool served = d2::waitFor ([&] { return p.getLatencySamples() == want; });
        check (served, "the reported latency reaches the message-thread truth for the restored session");
    }

    stopAudio.store (true, std::memory_order_release);
    audio.join();
    check (audioFinite.load(), "the audio path stayed finite with restores landing under it");

    const auto resaved = d2::saveOf (p);
    check (resaved == C.blob, "save after the final off-thread restore is byte-identical to the session restored");
}

// ---------------------------------------------------------------------------
//  State test 39 -- preset transitions while audio runs and a host thread saves
//  (D-2, contract C). The message thread loads every factory preset, twice
//  round, with the audio thread processing and a host thread saving in a loop
//  the whole time (reads only: that is what an autosave is; the host serialises
//  its own state calls, as the AudioProcessor API requires, so the sequenced
//  save below takes the same host-side lock). After each load a
//  SEQUENCED host-thread save must equal the owner's save -- the published
//  snapshot carries the loaded preset's name and identity -- and every
//  parameter must equal what a control instance holds after the same load, so
//  no old/new mix survives into the state a host would see.
// ---------------------------------------------------------------------------
static void testPresetTransitionsUnderConcurrentSaves()
{
    std::printf ("State test 39: preset transitions with audio running and a host thread saving (D-2)\n");

    AnamorphAudioProcessor p, control;
    p.prepareToPlay (48000.0, 512);
    control.prepareToPlay (48000.0, 512);
    const int factoryCount = [&]
    {
        int n = 0;
        for (const auto& e : p.getPresets().entries()) if (e.isFactory) ++n;
        return n;
    }();
    check (factoryCount >= 2, "non-vacuity: at least two factory presets to cycle through");

    std::atomic<bool> stop { false };
    std::atomic<bool> audioFinite { true };
    std::atomic<int>  hostSaves { 0 };
    std::mutex hostStateLock;   // the HOST's guarantee: its state calls never overlap each other
    std::thread audio ([&]
    {
        juce::AudioBuffer<float> buf (2, 256);
        std::mt19937 rng (0x0d2bu);
        while (! stop.load (std::memory_order_acquire))
            if (! d2::processStaysFinite (p, 2, buf, rng))
                audioFinite.store (false, std::memory_order_relaxed);
    });
    std::thread autosave ([&]
    {
        while (! stop.load (std::memory_order_acquire))
        {
            juce::MemoryBlock out;
            {
                const std::lock_guard<std::mutex> hostSerialises (hostStateLock);
                p.getStateInformation (out);
            }
            hostSaves.fetch_add (1, std::memory_order_relaxed);
        }
    });
    check (d2::waitFor ([&] { return hostSaves.load (std::memory_order_relaxed) > 0; }),
           "the host thread is saving before the first preset load");

    for (int round = 0; round < 2 * factoryCount; ++round)
    {
        const int index = round % factoryCount;
        p.getPresets().load (index);
        control.getPresets().load (index);
        p.pollUndoCoalesce();

        check (p.getPresets().currentIndex() == index, "the loaded preset is the current one");
        check (p.getPresets().currentName() == p.getPresets().entries()[index].name, "...by name");

        bool allEqual = true;
        auto pa = rangedParams (p), pc = rangedParams (control);
        for (size_t i = 0; i < pa.size() && i < pc.size(); ++i)
            if (! juce::exactlyEqual (pa[i]->getValue(), pc[i]->getValue())) allEqual = false;
        check (allEqual, "every parameter equals the control instance's after the same load (no old/new mix)");

        juce::MemoryBlock hostSave;
        d2::offMessageThread ([&]
        {
            const std::lock_guard<std::mutex> hostSerialises (hostStateLock);   // one host state call at a time
            hostSave = d2::saveOf (p);
        });
        check (hostSave == d2::saveOf (p), "a sequenced host-thread save equals the owner's save after the load");
    }

    stop.store (true, std::memory_order_release);
    audio.join();
    autosave.join();
    check (audioFinite.load(), "the audio path stayed finite through the preset transitions");
    check (hostSaves.load() > 0, "non-vacuity: the host thread saved while the presets changed");
    std::printf ("  %d concurrent host-thread saves during %d preset loads\n", hostSaves.load(), 2 * factoryCount);
}

// ---------------------------------------------------------------------------
//  State test 40 -- restore and re-prepare off the message thread (D-2,
//  contract D). A host thread restores two sessions in turn while another
//  activates the plug-in with alternating sample rates and block sizes, the
//  editor-shaped message thread reading and draining in between. No latency
//  report may run on either host thread (State test 30's invariant, kept), and
//  when both have finished the timer must leave the host holding what a
//  message-thread restore + prepare of the final session at the final rate
//  reports, with the Settings tree and the oversampling atomic agreeing.
// ---------------------------------------------------------------------------
static void testRestoreAroundReprepare()
{
    std::printf ("State test 40: restore and re-prepare off the message thread, together (D-2)\n");

    const auto B = d2::author ("D2-RB", 0.25f, 0.75f, 0, 3);   // 4x
    const auto C = d2::author ("D2-RC", 0.55f, 0.45f, 1, 1);   // Off
    constexpr int kIterations = 40;
    const double finalRate = ((kIterations - 1) & 1) ? 44100.0 : 48000.0;
    const int    finalBlock = ((kIterations - 1) & 1) ? 256 : 512;
    const auto&  finalSession = ((kIterations - 1) & 1) ? C : B;

    const int truth = [&]
    {
        AnamorphAudioProcessor q;
        q.prepareToPlay (48000.0, 512);
        d2::restoreFrom (q, finalSession.blob);
        q.prepareToPlay (finalRate, finalBlock);
        return q.getLatencySamples();
    }();

    struct ThreadRecorder final : public juce::AudioProcessorListener
    {
        std::atomic<int> offMessageThread { 0 };
        void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& d) override
        {
            if (d.latencyChanged && ! juce::MessageManager::existsAndIsCurrentThread())
                offMessageThread.fetch_add (1);
        }
    };

    AnamorphAudioProcessor p;
    p.prepareToPlay (48000.0, 512);
    ThreadRecorder rec;
    p.addListener (&rec);

    std::atomic<bool> go { false }, done { false };
    std::thread stateThread ([&]
    {
        while (! go.load (std::memory_order_acquire)) {}
        for (int i = 0; i < kIterations; ++i)
            d2::restoreFrom (p, ((i & 1) ? C : B).blob);
    });
    std::thread prepareThread ([&]
    {
        while (! go.load (std::memory_order_acquire)) {}
        for (int i = 0; i < kIterations; ++i)
            p.prepareToPlay ((i & 1) ? 44100.0 : 48000.0, (i & 1) ? 256 : 512);
        done.store (true, std::memory_order_release);
    });

    go.store (true, std::memory_order_release);
    while (! done.load (std::memory_order_acquire))
    {
        // The editor's tick, and the processor's timer.
        const juce::String liveName = p.getPresets().currentName();
        const bool dirty = p.getPresets().isDirty();
        p.pollUndoCoalesce();
        const bool u = p.canUndo(), r = p.canRedo();
        const int slot = p.abActiveSlot();
        if (liveName.isEmpty() && dirty && u && r && slot < 0) std::printf ("  (unreachable)\n");
        juce::Timer::callPendingTimersSynchronously();
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }
    stateThread.join();
    prepareThread.join();

    // The last restore may have landed after the last prepare; a message-thread
    // prepare at the final rate is what the host would do next, and then the truth
    // is the only acceptable report.
    p.pollUndoCoalesce();
    p.prepareToPlay (finalRate, finalBlock);
    const bool served = d2::waitFor ([&] { return p.getLatencySamples() == truth; });
    check (served, "after the two host threads finish, the timer leaves the host holding the final session's truth");
    check (rec.offMessageThread.load() == 0, "no latency report was delivered from either host thread");
    check (p.getInternal().oversampleIndex() == finalSession.oversampleId - 1
           && (int) p.getInternal().copyState()[anamorph::iid::oversample] == finalSession.oversampleId,
           "the oversampling atomic and the Settings tree agree on the final session");
    check (p.abActiveSlot() == finalSession.active && p.getPresets().currentName() == finalSession.name,
           "the final session's program is the one adopted");
    p.removeListener (&rec);
}

// ---------------------------------------------------------------------------
//  State test 41 -- the undo history is message-thread state a host restore only
//  CLEARS (D-2, contract E). A host thread restores while undo history exists:
//  until the drain the history is still there, whole (the restore touched none
//  of the owner's containers); at the drain it is gone. Then the owner builds new
//  history and walks it -- undo, undo, redo, redo, and a Copy-to-other undone on
//  the other slot -- while a host thread saves in a loop the whole time. Every
//  step must land exactly. The host thread never touches the undo stacks; the
//  stress probe proves that mechanically under ThreadSanitizer, this test proves
//  the semantics.
// ---------------------------------------------------------------------------
static void testUndoHistoryIsOwnedByTheMessageThread()
{
    std::printf ("State test 41: undo history is message-thread state a host restore only clears (D-2)\n");

    const auto X = d2::author ("D2-U", 0.15f, 0.85f, 0, 1);

    AnamorphAudioProcessor p;
    p.prepareToPlay (48000.0, 512);

    auto gestureEdit = [&] (const char* id, float raw)
    {
        auto* rp = p.getAPVTS().getParameter (id);
        rp->beginChangeGesture(); rp->setValueNotifyingHost (raw); rp->endChangeGesture();
        p.pollUndoCoalesce();
    };

    gestureEdit (pid::width, 0.33f);
    gestureEdit (pid::width, 0.66f);
    check (p.canUndo(), "two gesture edits created undo history");

    d2::offMessageThread ([&] { d2::restoreFrom (p, X.blob); });
    check (p.canUndo(), "before the drain the history is still whole (the restore did not touch it)");
    p.undo();   // undo drains first: the restore is adopted, and there is nothing left to undo
    check (! p.canUndo() && ! p.canRedo(), "at the drain the history is cleared, and the undo was a no-op");
    checkNear ((double) rawOf (p, "width"), (double) X.widthA, 1.0e-6,
               "the restored sound stands: the undo could not reach past the restore");

    std::atomic<bool> stop { false };
    std::atomic<int>  hostSaves { 0 };
    std::thread autosave ([&]
    {
        while (! stop.load (std::memory_order_acquire))
        {
            juce::MemoryBlock out;
            p.getStateInformation (out);
            hostSaves.fetch_add (1, std::memory_order_relaxed);
        }
    });
    // Non-vacuity, deterministically: the walk starts only once the host thread is
    // actually saving (a Release build can finish the whole walk before a fresh
    // thread is first scheduled).
    check (d2::waitFor ([&] { return hostSaves.load (std::memory_order_relaxed) > 0; }),
           "the host thread is saving before the undo walk starts");

    gestureEdit (pid::width, 0.30f);
    gestureEdit (pid::width, 0.60f);
    p.undo();  checkNear ((double) rawOf (p, "width"), 0.30, 1.0e-6, "undo #1 -> 0.30");
    p.undo();  checkNear ((double) rawOf (p, "width"), (double) X.widthA, 1.0e-6, "undo #2 -> the restored sound");
    check (! p.canUndo() && p.canRedo(), "the history is exhausted downward and full upward");
    p.redo();  checkNear ((double) rawOf (p, "width"), 0.30, 1.0e-6, "redo #1 -> 0.30");
    p.redo();  checkNear ((double) rawOf (p, "width"), 0.60, 1.0e-6, "redo #2 -> 0.60");

    p.abCopyToOther();                 // B := 0.60, recorded on B's history
    p.abSwitchTo (1);
    checkNear ((double) rawOf (p, "width"), 0.60, 1.0e-6, "the copy reached slot B");
    p.undo();
    checkNear ((double) rawOf (p, "width"), (double) X.widthB, 1.0e-6, "undoing on slot B reverts the copy to B's restored sound");
    p.abSwitchTo (0);
    checkNear ((double) rawOf (p, "width"), 0.60, 1.0e-6, "slot A's history and sound are undisturbed");

    stop.store (true, std::memory_order_release);
    autosave.join();
    check (hostSaves.load() > 0, "non-vacuity: the host thread saved throughout");
    std::printf ("  %d concurrent host-thread saves during the undo walk\n", hostSaves.load());
}

// ---------------------------------------------------------------------------
//  State test 42 -- a host-thread save never pairs the restored sound with an
//  OLDER program (D-2 round 2, review finding 1).
//
//  The reviewed interleaving, reproduced deterministically through the seam
//  the processor exposes for exactly this purpose: the host thread is inside its
//  save and has taken the mailbox -- which holds the PREVIOUS program's snapshot,
//  published before the restore -- and the message thread adopts the restore and
//  republishes BEFORE the host side decides which program to describe. The save
//  it then writes must be byte-identical to the owner's save after the adoption.
//  On the round-1 tree it is not: the host side read the two generation atomics
//  AFTER its take, saw "adopted", and wrote the old snapshot's name, slots and
//  Settings around the restored sound -- the mixed state the review named. The
//  fix carries the generation INSIDE the snapshot, so the decision is about the
//  object in hand; mutation-tested by forcing the mailbox choice (the test fails).
//  The plain sequence (save after adoption, with nothing paused) is kept as the
//  control, and the run alternates two sessions so "previous program" is never
//  the fresh-instance program.
// ---------------------------------------------------------------------------
static void testHostSaveNeverPairsRestoredSoundWithOlderProgram()
{
    std::printf ("State test 42: a host-thread save never pairs the restored sound with an older program (D-2 r2)\n");

    const auto X = d2::author ("D2-F1X", 0.11f, 0.89f, 0, 1);
    const auto Y = d2::author ("D2-F1Y", 0.22f, 0.78f, 1, 1);
    check (X.blob != Y.blob, "non-vacuity: the two sessions differ");

    AnamorphAudioProcessor p;
    p.prepareToPlay (48000.0, 512);

    for (int i = 0; i < 6; ++i)
    {
        const auto& sn = (i & 1) ? Y : X;
        const auto before = d2::View::of (p);

        d2::offMessageThread ([&] { d2::restoreFrom (p, sn.blob); });   // pending: the owner has not adopted
        check (d2::View::of (p).active == before.active && d2::View::of (p).name == before.name,
               "before the drain the owner still holds the previous program");

        // Handshake: 1 = the host thread has taken the mailbox and waits; 2 = the
        // owner has adopted and republished; the host thread may decide.
        std::atomic<int> phase { 0 };
        p.seams.afterHostSaveTake = [&]
        {
            phase.store (1, std::memory_order_release);
            for (int spins = 0; phase.load (std::memory_order_acquire) != 2 && spins < 4000000; ++spins)
                std::this_thread::yield();
        };
        juce::MemoryBlock hostSave;
        std::thread host ([&] { hostSave = d2::saveOf (p); });
        for (int spins = 0; phase.load (std::memory_order_acquire) != 1 && spins < 4000000; ++spins)
            std::this_thread::yield();
        check (phase.load() == 1, "the host thread reached the boundary (took the mailbox) and is waiting");

        p.pollUndoCoalesce();                       // the adoption completes and republishes, owner's thread
        const auto adopted = d2::View::of (p);
        check (adopted.matches (sn), "the owner adopted the restored program while the host save is paused");
        const auto ownerSave = d2::saveOf (p);      // the owner's own bytes, after adoption

        phase.store (2, std::memory_order_release); // now the host side decides
        host.join();
        p.seams.afterHostSaveTake = nullptr;

        check (hostSave == ownerSave,
               "the host save decided AFTER the adoption equals the owner's save: no restored sound around the previous program");

        // Control: the plain sequence, nothing paused.
        juce::MemoryBlock hostSaveAfter;
        d2::offMessageThread ([&] { hostSaveAfter = d2::saveOf (p); });
        check (hostSaveAfter == ownerSave, "a host save with nothing paused equals the owner's save");
    }
}

// ---------------------------------------------------------------------------
//  State test 43 -- an older restore's completion cannot overwrite a newer
//  restore's oversampling (D-2 round 2, review finding 2).
//
//  Two layers. (1) The publication rule itself, on an InternalState alone: the
//  engine-config word carries the generation of the arrival that published it,
//  and a publication lands only if no higher generation stands. Restore A
//  (generation 1), restore B (generation 2), then A's DELAYED completion -- its
//  adoption writing the tree with generation 1 -- must leave B's oversampling
//  in place; a Settings edit made inside that window yields too; B's own
//  completion lands (idempotent), and an edit after it lands. (2) The reviewed
//  interleaving on the processor, deterministically through the adoption seam:
//  the message thread has TAKEN restore A from the cell, restore B lands from a
//  host thread before A's tail runs, and A's tail must not overwrite B's
//  oversampling -- so an activation (prepareToPlay) in the window primes the
//  engine at B's setting and reports B's latency, and B's own adoption then
//  brings the tree to B. On the round-1 tree A's tail stored A's value over B's
//  (its adoption republished the atomic unconditionally) and the activation ran
//  at A's oversampling until B's adoption.
// ---------------------------------------------------------------------------
static void testOlderRestoreCannotOverwriteNewerOversampling()
{
    std::printf ("State test 43: an older restore's completion cannot overwrite a newer restore's oversampling (D-2 r2)\n");

    // --- (1) the rule, on the word alone --------------------------------
    {
        auto resolvedWithOversample = [] (int comboId)
        {
            juce::ValueTree src ("ANAMORPH_INTERNAL");
            src.setProperty (anamorph::iid::oversample, comboId, nullptr);
            return anamorph::InternalState::resolveRestore (src);
        };
        const auto A = resolvedWithOversample (2);   // 2x  -> index 1
        const auto B = resolvedWithOversample (3);   // 4x  -> index 2
        const auto C = resolvedWithOversample (1);   // Off -> index 0

        anamorph::InternalState st;
        check (st.oversampleIndex() == 0 && st.engineConfigGeneration() == 0, "fresh: Off, generation 0");

        check (st.publishEngineConfig (A, 1) && st.oversampleIndex() == 1, "restore A (generation 1) publishes 2x");
        check (st.publishEngineConfig (B, 2) && st.oversampleIndex() == 2, "restore B (generation 2) publishes 4x");

        st.noteAdoptedGeneration (1);
        st.adoptResolved (A, 1);                     // A's delayed completion, on the owner's thread
        check (st.oversampleIndex() == 2 && st.engineConfigGeneration() == 2,
               "A's completion (generation 1) yields: B's 4x stands");
        check ((int) st.oversampleValue().getValue() == 2, "...while the tree holds A's value until B's tail lands");

        // A Settings edit inside B's pending window (round 3): newer than every
        // restore that has arrived, so it lands at the word at once and B's own
        // completion keeps it -- the edit came after B arrived.
        st.oversampleValue().setValue (4);           // 8x -> index 3
        check (st.oversampleIndex() == 3 && st.engineConfigGeneration() == 2,
               "an edit made after the latest arrival lands at the word at once, under that arrival's generation");

        st.noteAdoptedGeneration (2);
        st.adoptResolved (B, 2);                     // B's completion
        check (st.oversampleIndex() == 3 && (int) st.oversampleValue().getValue() == 4,
               "B's completion keeps the edit made after B arrived; the tree and the word agree");

        check (! st.publishEngineConfig (A, 1) && st.oversampleIndex() == 3,
               "a late republication of A (generation 1) is refused");
        check (st.publishEngineConfig (C, 3) && st.oversampleIndex() == 0 && st.engineConfigGeneration() == 3,
               "a newer restore C (generation 3) lands over the edit: it arrived after it");
        st.noteAdoptedGeneration (3);
        st.adoptResolved (C, 3);
        check (st.oversampleIndex() == 0 && (int) st.oversampleValue().getValue() == 1 && st.engineConfigGeneration() == 3,
               "C's completion replaces the older edit (idempotent at the word) and the tree agrees");

        st.oversampleValue().setValue (3);           // an edit after the latest restore landed
        check (st.oversampleIndex() == 2 && st.engineConfigGeneration() == 3,
               "an edit after the latest restore lands (generation 3 <= 3)");
    }

    // --- (2) the interleaving, on the processor -------------------------
    const auto A = d2::author ("D2-F2A", 0.30f, 0.70f, 0, 2);   // 2x
    const auto B = d2::author ("D2-F2B", 0.60f, 0.40f, 1, 3);   // 4x

    // Truth: what a message-thread restore + prepare of B reports.
    const int latencyB = [&]
    {
        AnamorphAudioProcessor q;
        q.prepareToPlay (48000.0, 512);
        d2::restoreFrom (q, B.blob);
        q.prepareToPlay (48000.0, 512);
        return q.getLatencySamples();
    }();
    const int latencyA = [&]
    {
        AnamorphAudioProcessor q;
        q.prepareToPlay (48000.0, 512);
        d2::restoreFrom (q, A.blob);
        q.prepareToPlay (48000.0, 512);
        return q.getLatencySamples();
    }();
    check (latencyA != latencyB, "non-vacuity: the two sessions report different latencies");

    AnamorphAudioProcessor p;
    p.prepareToPlay (48000.0, 512);

    d2::offMessageThread ([&] { d2::restoreFrom (p, A.blob); });      // A pending
    check (p.getInternal().oversampleIndex() == 1, "restore A published 2x synchronously");

    // Restore B lands from a host thread AFTER the owner has taken A from the cell
    // and BEFORE A's tail runs -- the reviewed overlap. Since round 8 the drain runs to
    // a FIXED POINT, so the same pass goes on to adopt B, and the seam's SECOND fire is
    // exactly the instant this round-2 measurement is about: A's tail has finished and
    // B's has not begun. Every assertion below is the one this test has always made,
    // taken at that instant instead of between two drains (the seam injects only once,
    // or the drain would have a new restore to adopt on every pass).
    int seamRuns = 0;
    int  osInWindow = -1, latencyInWindow = -1;
    bool viewInWindowIsA = false;
    p.seams.afterRestoreTake = [&]
    {
        ++seamRuns;
        if (seamRuns == 1)
        {
            d2::offMessageThread ([&] { d2::restoreFrom (p, B.blob); });   // B arrives during A's adoption
        }
        else if (seamRuns == 2)
        {
            viewInWindowIsA = d2::View::of (p).matches (A);
            osInWindow      = p.getInternal().oversampleIndex();
            p.prepareToPlay (48000.0, 512);                                // activation inside the window
            latencyInWindow = p.getLatencySamples();
        }
    };
    p.pollUndoCoalesce();                                             // drains to a fixed point: A, then B
    p.seams.afterRestoreTake = nullptr;
    check (seamRuns == 2, "the overlap was produced once and the same drain went on to adopt B");

    check (viewInWindowIsA, "the owner adopted A's program (B's tail had not run yet)");
    check (osInWindow == 2,
           "A's tail did not overwrite B's oversampling: the word holds the newer restore's 4x");
    check (latencyInWindow == latencyB, "an activation in the window primes the engine at B's setting");

    check (d2::View::of (p).matches (B), "B's program is adopted");
    check (p.getInternal().oversampleIndex() == 2 && (int) p.getInternal().oversampleValue().getValue() == 3,
           "after B's adoption the tree and the word agree on 4x");
    check (d2::saveOf (p) == B.blob, "a save after the final adoption is byte-identical to the session restored");

    juce::MemoryBlock hostSave;
    d2::offMessageThread ([&] { hostSave = d2::saveOf (p); });
    check (hostSave == B.blob, "...and so is a host-thread save");
}

// ---------------------------------------------------------------------------
//  State test 44 -- a Settings edit made after a restore ARRIVED survives that
//  restore's adoption (D-2 round 3, review finding: "pending restores discard
//  Settings edits").
//
//  The six host-hidden Settings are message-thread state written by the editor's
//  juce::Value bindings, and an off-thread restore's values reach that tree only
//  at the adoption (<= one timer period later). Round 2's adoption wrote all six
//  unconditionally, so an edit made inside the pending window -- AFTER the
//  restore had arrived -- was replaced by the older restore: the one message-
//  thread mutation not ordered after the restore it overlapped (every other one
//  drains first). The rule now is precedence by ARRIVAL: an edit records the
//  generation of the latest restore that had arrived when it was made, and an
//  adoption keeps a field edited at its own generation or later. So
//    (1) each field, edited alone after the arrival, stands and the other five
//        take the restore's values; the engine word and a save agree;
//    (2) all six edited after the arrival all stand;
//    (3) an edit made BEFORE the restore arrived is replaced -- the restore is
//        the newer arrival;
//    (4) two restores around the edits, through the adoption seam: an edit after
//        R1 but before R2 survives R1's adoption and is replaced by R2's; an edit
//        after R2 survives both;
//    (5) an inline (message-thread) restore is the newest arrival by definition
//        and replaces every field.
//  Mutation-tested: with the adoption writing every field again, (1), (2) and
//  (4) fail.
// ---------------------------------------------------------------------------
namespace r3
{
    struct SettingsSet { int oversample, uiScale; double scopePersist; bool metersOn, tooltipsOn, uiAnimations; };
    static const char* const fieldNames[6] = { "oversample", "uiScale", "scopePersist", "metersOn", "tooltipsOn", "uiAnimations" };

    // Through the editor's own path: the juce::Value bindings.
    static void setField (anamorph::InternalState& st, int i, const SettingsSet& v)
    {
        switch (i)
        {
            case 0: st.oversampleValue().setValue (v.oversample);       break;
            case 1: st.uiScaleValue().setValue (v.uiScale);             break;
            case 2: st.scopePersistValue().setValue (v.scopePersist);   break;
            case 3: st.metersValue().setValue (v.metersOn);             break;
            case 4: st.tooltipsValue().setValue (v.tooltipsOn);         break;
            default: st.animationsValue().setValue (v.uiAnimations);    break;
        }
    }
    static void apply (anamorph::InternalState& st, const SettingsSet& v)
    {
        for (int i = 0; i < 6; ++i) setField (st, i, v);
    }
    static SettingsSet read (anamorph::InternalState& st)
    {
        const auto t = st.copyState();
        return { (int) t[anamorph::iid::oversample], (int) t[anamorph::iid::uiScale],
                 (double) t[anamorph::iid::scopePersist], (bool) t[anamorph::iid::metersOn],
                 (bool) t[anamorph::iid::tooltipsOn], (bool) t[anamorph::iid::uiAnimations] };
    }
    static bool fieldEquals (const SettingsSet& a, const SettingsSet& b, int i)
    {
        switch (i)
        {
            case 0:  return a.oversample == b.oversample;
            case 1:  return a.uiScale == b.uiScale;
            case 2:  return juce::exactlyEqual (a.scopePersist, b.scopePersist);
            case 3:  return a.metersOn == b.metersOn;
            case 4:  return a.tooltipsOn == b.tooltipsOn;
            default: return a.uiAnimations == b.uiAnimations;
        }
    }
    static bool same (const SettingsSet& a, const SettingsSet& b)
    {
        for (int i = 0; i < 6; ++i) if (! fieldEquals (a, b, i)) return false;
        return true;
    }
    static juce::MemoryBlock authorSession (const char* name, const SettingsSet& v)
    {
        AnamorphAudioProcessor a;
        apply (a.getInternal(), v);
        a.getPresets().setMeta (name, "r3-baseline-" + juce::String (name), anamorph::PresetManager::Selection());
        return d2::saveOf (a);
    }
    // What a message-thread restore of the blob yields: the Settings a save carries.
    static SettingsSet settingsOf (const juce::MemoryBlock& blob)
    {
        AnamorphAudioProcessor q;
        d2::restoreFrom (q, blob);
        return read (q.getInternal());
    }
}

static void testSettingsEditAfterRestoreArrivalSurvivesAdoption()
{
    std::printf ("State test 44: a Settings edit made after a restore arrived survives its adoption (D-2 r3)\n");
    using r3::SettingsSet;

    // P is what the editor SHOWS before the restore lands (the booleans already at
    // R's values, so a toggle -- which can only flip what is shown -- produces an
    // edit that differs from R); R is the restore; U the user's edits, every field
    // away from both P (so each edit is a real change) and R (so "kept" and
    // "replaced" are distinguishable); R2 a second restore.
    const SettingsSet P  { 1, 3, 0.50, true,  true,  false };
    const SettingsSet R  { 2, 4, 0.25, true,  true,  false };
    const SettingsSet U  { 3, 5, 0.75, false, false, true  };
    const SettingsSet R2 { 4, 2, 0.90, false, true,  false };
    const auto blobR = r3::authorSession ("D2-R3-R", R), blobR2 = r3::authorSession ("D2-R3-R2", R2);
    check (r3::same (r3::settingsOf (blobR), R) && r3::same (r3::settingsOf (blobR2), R2),
           "non-vacuity: the two sessions carry their Settings");
    for (int i = 0; i < 6; ++i)
        check (! r3::fieldEquals (R, U, i) && ! r3::fieldEquals (P, U, i),
               "non-vacuity: the edit differs from the restore AND from what the editor shows, in every field");
    check (R2.oversample != U.oversample && R2.uiScale != R.uiScale, "non-vacuity: R2 differs where (4) looks");

    // --- (1) each field alone, edited after the arrival ---------------------
    for (int i = 0; i < 6; ++i)
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        r3::apply (p.getInternal(), P);                                // what the editor shows before the restore
        d2::offMessageThread ([&] { d2::restoreFrom (p, blobR); });   // arrived; pending
        r3::setField (p.getInternal(), i, U);                          // the user's edit, after the arrival
        if (i == 0)
            check (p.getInternal().oversampleIndex() == U.oversample - 1,
                   "an Oversampling edit inside the window reaches the engine word at once: it is newer than the restore");
        p.pollUndoCoalesce();                                          // the adoption

        const auto got = r3::read (p.getInternal());
        for (int j = 0; j < 6; ++j)
        {
            const juce::String what = juce::String (r3::fieldNames[j])
                                    + (j == i ? ": the edit made after the arrival stands"
                                              : ": the restore's value (the edit did not touch it)");
            check (r3::fieldEquals (got, j == i ? U : R, j), what.toRawUTF8());
        }
        check (p.getInternal().oversampleIndex() == got.oversample - 1, "the engine word agrees with the tree after the adoption");
        check (r3::same (r3::settingsOf (d2::saveOf (p)), got), "a save after the adoption carries the resulting Settings");
        juce::MemoryBlock hostSave;
        d2::offMessageThread ([&] { hostSave = d2::saveOf (p); });
        check (hostSave == d2::saveOf (p), "...and a host-thread save equals the owner's");
    }

    // --- (2) all six edited after the arrival --------------------------------
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        r3::apply (p.getInternal(), P);                                // what the editor shows before the restore
        d2::offMessageThread ([&] { d2::restoreFrom (p, blobR); });
        r3::apply (p.getInternal(), U);
        p.pollUndoCoalesce();
        check (r3::same (r3::read (p.getInternal()), U), "six edits made after the arrival all stand through the adoption");
        check (r3::same (r3::settingsOf (d2::saveOf (p)), U), "...and the save carries them");
        check (p.getInternal().oversampleIndex() == U.oversample - 1, "...and the engine word holds the edited Oversampling");

        AnamorphAudioProcessor truth;
        truth.prepareToPlay (48000.0, 512);
        r3::apply (truth.getInternal(), U);
        truth.prepareToPlay (48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        check (p.getLatencySamples() == truth.getLatencySamples(),
               "an activation after the adoption reports the edited Oversampling's latency");
    }

    // --- (3) edited BEFORE the restore arrived --------------------------------
    for (int i = 0; i < 6; ++i)
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        r3::apply (p.getInternal(), P);                                // what the editor shows before the restore
        r3::setField (p.getInternal(), i, U);                          // the edit first
        d2::offMessageThread ([&] { d2::restoreFrom (p, blobR); });   // then the restore arrives
        p.pollUndoCoalesce();
        const juce::String what = juce::String (r3::fieldNames[i]) + ": an edit made before the restore arrived is replaced by it";
        check (r3::same (r3::read (p.getInternal()), R), what.toRawUTF8());
    }

    // --- (4) two restores around the edits, through the adoption seam --------
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        r3::apply (p.getInternal(), P);                                // what the editor shows before the restore
        d2::offMessageThread ([&] { d2::restoreFrom (p, blobR); });   // R1 pending
        r3::setField (p.getInternal(), 0, U);                          // Oversampling := U -- after R1, before R2
        const SettingsSet U2 { 0, 1, 0.0, false, false, false };       // only its uiScale is used
        // Since round 8 the drain runs to a FIXED POINT, so this one pass adopts R1 and
        // then R2, and the seam's SECOND fire is the instant between the two tails --
        // where the "after R1's adoption" assertions belong. The seam injects only once,
        // or the drain would have a new restore to adopt on every pass.
        int seamRuns = 0;
        SettingsSet afterR1 {};
        p.seams.afterRestoreTake = [&]
        {
            ++seamRuns;
            if (seamRuns == 1)
            {
                d2::offMessageThread ([&] { d2::restoreFrom (p, blobR2); });   // R2 arrives after the owner took R1
                r3::setField (p.getInternal(), 1, U2);                          // uiScale := U2 -- after R2
            }
            else if (seamRuns == 2)
            {
                afterR1 = r3::read (p.getInternal());   // R1's tail has run; R2's has not
            }
        };
        p.pollUndoCoalesce();                                          // drains to a fixed point: R1, then R2
        p.seams.afterRestoreTake = nullptr;
        check (seamRuns == 2, "the overlap was produced once and the same drain went on to adopt R2");

        check (afterR1.oversample == U.oversample, "after R1's adoption the Oversampling edit (made after R1) stands");
        check (afterR1.uiScale == U2.uiScale, "...and the uiScale edit (made after R2) stands");
        check (juce::exactlyEqual (afterR1.scopePersist, R.scopePersist) && afterR1.metersOn == R.metersOn
               && afterR1.tooltipsOn == R.tooltipsOn && afterR1.uiAnimations == R.uiAnimations,
               "...and the untouched fields take R1's values");

        auto got = r3::read (p.getInternal());
        check (got.oversample == R2.oversample, "after R2's adoption the Oversampling edit (made before R2) is replaced: R2 is the newer arrival");
        check (got.uiScale == U2.uiScale, "...while the uiScale edit (made after R2) still stands");
        check (juce::exactlyEqual (got.scopePersist, R2.scopePersist) && got.metersOn == R2.metersOn
               && got.tooltipsOn == R2.tooltipsOn && got.uiAnimations == R2.uiAnimations,
               "...and the untouched fields take R2's values");
        check (p.getInternal().oversampleIndex() == R2.oversample - 1, "the engine word agrees with the tree");
        check (r3::same (r3::settingsOf (d2::saveOf (p)), got), "the save carries the result");
    }

    // --- (5) the inline restore is the newest arrival --------------------------
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        r3::apply (p.getInternal(), P);                                // what the editor shows before the restore
        r3::apply (p.getInternal(), U);
        d2::restoreFrom (p, blobR);
        check (r3::same (r3::read (p.getInternal()), R), "an inline (message-thread) restore replaces every field, edits or not");
    }
}

// ---------------------------------------------------------------------------
//  State test 45 -- a message-thread action taken in the handoff window cannot
//  leave the restore's metadata over that action's sound (D-2 round 4, review
//  finding "concurrent actions vanish before handoff").
//
//  The window is real and this reproduces it exactly, through the seam the
//  handoff exposes: an off-message-thread restore has applied its SOUND (the
//  decode does that on the caller's thread, synchronously, so a prepareToPlay
//  behind it primes the restored session) but has not yet reached
//  `pendingRestore.put`. In that gap the message thread cannot see the restore
//  at all -- every entry point drains the cell, and the cell is still empty --
//  so an A/B switch runs: it stores the LIVE parameters (already the restore's
//  sound) into the outgoing session's slot and applies the other slot, leaving
//  the live sound that of a session the restore knows nothing about. The
//  restore is then adopted.
//
//  The rule (ADR-0036 §10): adopting a restore installs its sound AND its
//  metadata, so the adoption commits one session. The A/B switch is superseded
//  -- it navigated the outgoing session, and a session restore replaces that
//  session -- but it can never be left half-applied under the new metadata.
//  Before the fix the adoption wrote metadata only, so the saved session was
//  the restore's name, slots and identity around the A/B switch's sound.
//  Covered for both mutators the window admits: the A/B switch and Copy-to-other.
// ---------------------------------------------------------------------------
static void testActionInHandoffWindowCannotSplitTheSession()
{
    std::printf ("State test 45: an action in the handoff window cannot split the restored session (D-2 r4)\n");

    const auto P = d2::author ("D2-R4-P", 0.12f, 0.88f, 0, 1);   // the outgoing session
    const auto R = d2::author ("D2-R4-R", 0.34f, 0.66f, 1, 2);   // the restore that arrives
    check (P.blob != R.blob, "non-vacuity: the two sessions differ");

    for (int mutator = 0; mutator < 2; ++mutator)   // 0 = A/B switch, 1 = Copy-to-other
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        d2::restoreFrom (p, P.blob);                 // inline: the outgoing session, whole
        check (d2::View::of (p).matches (P), "the outgoing session is live before the restore arrives");

        // The handoff window, held open on the restoring thread while the message
        // thread acts: 1 = the restore's sound is applied and the cell is still
        // empty; 2 = the message thread is done and the handoff may complete.
        std::atomic<int> phase { 0 };
        p.seams.beforeRestorePut = [&]
        {
            phase.store (1, std::memory_order_release);
            for (int spins = 0; phase.load (std::memory_order_acquire) != 2 && spins < 4000000; ++spins)
                std::this_thread::yield();
        };
        std::thread host ([&] { d2::restoreFrom (p, R.blob); });
        for (int spins = 0; phase.load (std::memory_order_acquire) != 1 && spins < 4000000; ++spins)
            std::this_thread::yield();
        check (phase.load() == 1, "the restoring thread reached the handoff window and is waiting");

        // Inside the window: the restore's sound is live, its metadata is not, and
        // the cell is empty -- so this action cannot see the restore.
        const float restoredWidth = R.active == 0 ? R.widthA : R.widthB;
        checkNear ((double) rawOf (p, "width"), (double) restoredWidth, 1.0e-6,
                   "in the window the restore's SOUND is already live");
        check (d2::View::of (p).matches (P), "...while the message thread still owns the outgoing session's program");
        if (mutator == 0) p.abSwitchTo (p.abActiveSlot() == 0 ? 1 : 0);
        else              p.abCopyToOther();
        // It ran against the outgoing session -- it could not see the restore, which is
        // the whole point of the window. (A switch shows the other slot's own preset
        // name, so the name is not the thing to assert here; the restore's identity is.)
        check (p.getPresets().selection().factoryId != R.name && p.getPresets().currentName() != R.name,
               "the action ran before the restore was visible to this thread");

        phase.store (2, std::memory_order_release);
        host.join();
        p.seams.beforeRestorePut = nullptr;

        p.pollUndoCoalesce();                        // the adoption

        // One session, whole: the restore's metadata AND the restore's sound.
        check (d2::View::of (p).matches (R), "after the adoption the restored program is visible");
        checkNear ((double) rawOf (p, "width"), (double) restoredWidth, 1.0e-6,
                   "...and the restored SOUND stands: the adoption re-installed it over the action's");
        check (d2::saveOf (p) == R.blob,
               "a save is byte-identical to the session restored: no metadata from one session over sound from another");
        juce::MemoryBlock hostSave;
        d2::offMessageThread ([&] { hostSave = d2::saveOf (p); });
        check (hostSave == d2::saveOf (p), "...and a host-thread save agrees with the owner's");

        // The A/B slots belong to the restored session too, not to the superseded action.
        p.abSwitchTo (1 - p.abActiveSlot());
        const float otherWidth = R.active == 0 ? R.widthB : R.widthA;
        checkNear ((double) rawOf (p, "width"), (double) otherWidth, 1.0e-6,
                   "the other slot holds the restored session's sound, not the superseded action's");
    }
}

// ---------------------------------------------------------------------------
//  State test 46 -- an inline restore never publishes a pending restore's
//  metadata against its own sound (D-2 round 4, review finding "inline restore
//  exposes mixed sessions").
//
//  With a restore pending from a host thread, a restore that arrives ON the
//  message thread used to decode first -- which applies the incoming session's
//  sound -- and only then drain the pending one, whose tail published the OLDER
//  session's metadata while the NEWER session's parameters were already live.
//  The drain now runs first, so the pending restore is adopted against its own
//  sound and the incoming session's sound and metadata land together after it.
//  The seam fires inside the drain, which is the exact instant that used to be
//  incoherent: the live sound there must be the PENDING session's.
// ---------------------------------------------------------------------------
static void testInlineRestoreAdoptsPendingAgainstItsOwnSound()
{
    std::printf ("State test 46: an inline restore adopts a pending one against its own sound (D-2 r4)\n");

    const auto R1 = d2::author ("D2-R4-1", 0.21f, 0.79f, 0, 1);   // handed over from a host thread
    const auto R2 = d2::author ("D2-R4-2", 0.43f, 0.57f, 1, 3);   // then restored inline
    check (R1.blob != R2.blob, "non-vacuity: the two sessions differ");
    const float w1 = R1.active == 0 ? R1.widthA : R1.widthB;
    const float w2 = R2.active == 0 ? R2.widthA : R2.widthB;
    check (! juce::exactlyEqual (w1, w2), "non-vacuity: their live sounds differ");

    AnamorphAudioProcessor p;
    p.prepareToPlay (48000.0, 512);

    d2::offMessageThread ([&] { d2::restoreFrom (p, R1.blob); });   // R1 pending
    checkNear ((double) rawOf (p, "width"), (double) w1, 1.0e-6, "R1's sound is live before the inline restore");

    int seamRuns = 0;
    double widthAtAdoption = -1.0;
    p.seams.afterRestoreTake = [&]
    {
        ++seamRuns;
        widthAtAdoption = (double) rawOf (p, "width");
    };
    d2::restoreFrom (p, R2.blob);      // inline, on this thread: drains R1 first, then applies R2
    p.seams.afterRestoreTake = nullptr;
    check (seamRuns == 1, "the inline restore drained the pending one exactly once");
    checkNear (widthAtAdoption, (double) w1, 1.0e-6,
               "the pending restore was adopted against ITS OWN sound: the incoming session had not been applied yet");

    check (d2::View::of (p).matches (R2), "the inline session's program is live afterwards");
    checkNear ((double) rawOf (p, "width"), (double) w2, 1.0e-6, "...and its sound");
    check (d2::saveOf (p) == R2.blob, "a save is byte-identical to the session restored inline");
    juce::MemoryBlock hostSave;
    d2::offMessageThread ([&] { hostSave = d2::saveOf (p); });
    check (hostSave == d2::saveOf (p), "a host-thread save agrees with the owner's");
}

// ---------------------------------------------------------------------------
//  State test 47 -- a sound edit made while a restore is pending survives its
//  adoption (D-2 round 5, review finding "pending sound edits are erased").
//
//  Round 4 made the adoption re-install the restored SOUND, so that an action
//  which had replaced the live parameters with another session's could not be
//  left half-applied under the restored session's identity (§10). Its guard was
//  "has anything touched the parameters since the decode", read from
//  `soundParamGen` -- which every knob turn bumps. So an ordinary sound edit made
//  in the pending window was treated as another session's sound and erased.
//
//  The two cases are genuinely different and the rule now names the difference
//  (§12): a WHOLESALE replacement (an A/B apply, an undo/redo, a preset load)
//  means the live sound is some other session's, and the adoption re-installs its
//  own; an EDIT means the restored session is live with a newer mutation in it,
//  which is the same "a user action inside the pending window lands on top of the
//  restore" the rest of the design has, and the adoption leaves it alone.
//  `soundSetGen` counts only the wholesale replacements, which is what separates
//  them.
//
//  The window here needs no seam: after the handoff returns, the restore sits in
//  the cell until the next drain, and a knob turn is the one message-thread
//  mutation that does not drain (every entry point that does would adopt first
//  and land on top anyway). The §10 half -- a replacement in the window IS
//  superseded -- is State test 45's, and is not repeated here.
// ---------------------------------------------------------------------------
static void testSoundEditWhilePendingSurvivesAdoption()
{
    std::printf ("State test 47: a sound edit made while a restore is pending survives its adoption (D-2 r5)\n");

    const auto P = d2::author ("D2-R5-P", 0.15f, 0.85f, 0, 1);   // the outgoing session
    const auto R = d2::author ("D2-R5-R", 0.35f, 0.65f, 1, 2);   // the restore that arrives
    const float restoredWidth = R.active == 0 ? R.widthA : R.widthB;

    // A plain user edit, through the path the editor uses: a gesture around
    // setValueNotifyingHost. It bumps `soundParamGen` (one parameter changed) and
    // NOT `soundSetGen` (no state set was installed), which is the distinction the
    // adoption reads. Values are normalised, as `rawOf`/`setRaw` and the Session
    // widths are.
    auto editParam = [] (AnamorphAudioProcessor& p, const char* id, float norm)
    {
        auto* rp = p.getAPVTS().getParameter (id);
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (norm);
        rp->endChangeGesture();
    };

    // --- (a) one edit, one parameter ---------------------------------------
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        d2::restoreFrom (p, P.blob);

        d2::offMessageThread ([&] { d2::restoreFrom (p, R.blob); });   // pending: sound live, metadata not
        checkNear ((double) rawOf (p, "width"), (double) restoredWidth, 1.0e-6,
                   "the restore's sound is live while its metadata is pending");
        check (d2::View::of (p).matches (P), "...and the message thread still owns the outgoing program");

        const float edited = restoredWidth < 0.5f ? restoredWidth + 0.25f : restoredWidth - 0.25f;
        check (! juce::exactlyEqual (edited, restoredWidth) && edited >= 0.0f && edited <= 1.0f,
               "non-vacuity: the edit moves the value, and stays in range");
        editParam (p, "width", edited);
        checkNear ((double) rawOf (p, "width"), (double) edited, 1.0e-6, "the edit took effect");

        p.pollUndoCoalesce();   // the adoption

        checkNear ((double) rawOf (p, "width"), (double) edited, 1.0e-6,
                   "the edit made while the restore was pending SURVIVES its adoption");
        check (d2::View::of (p).matches (R), "...and the restored session's program is adopted");
        check (p.getInternal().oversampleIndex() == R.oversampleId - 1,
               "...and the restored session's Oversampling is in force");

        // The published state is that one session plus the newer edit -- a save carries
        // the edited value, and a host-thread save agrees with the owner's.
        AnamorphAudioProcessor control;
        control.prepareToPlay (48000.0, 512);
        d2::restoreFrom (control, R.blob);
        editParam (control, "width", edited);
        check (d2::saveOf (p) == d2::saveOf (control),
               "a save equals a message-thread restore of the same session with the same edit");
        juce::MemoryBlock hostSave;
        d2::offMessageThread ([&] { hostSave = d2::saveOf (p); });
        check (hostSave == d2::saveOf (p), "...and a host-thread save agrees with the owner's");
    }

    // --- (b) several edits, two parameters ----------------------------------
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        d2::restoreFrom (p, P.blob);
        d2::offMessageThread ([&] { d2::restoreFrom (p, R.blob); });

        const float w1 = 0.20f, w2 = 0.80f;
        const float driveBefore = rawOf (p, "drive");
        const float driveEdited = driveBefore < 0.5f ? 0.90f : 0.10f;
        check (! juce::exactlyEqual (driveEdited, driveBefore), "non-vacuity: the drive edit moves the value");

        editParam (p, "width", w1);          // three edits, two parameters, all before the adoption
        editParam (p, "drive", driveEdited);
        editParam (p, "width", w2);

        p.pollUndoCoalesce();

        checkNear ((double) rawOf (p, "width"), (double) w2, 1.0e-6, "the last width edit survives");
        checkNear ((double) rawOf (p, "drive"), (double) driveEdited, 1.0e-6, "the drive edit survives too");
        check (d2::View::of (p).matches (R), "the restored session's program is still what was adopted");
        check (p.getPresets().currentName() == R.name, "...by name");
    }

    // --- (c) the restored sound stands where nothing was edited --------------
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        d2::restoreFrom (p, P.blob);
        d2::offMessageThread ([&] { d2::restoreFrom (p, R.blob); });
        editParam (p, "drive", rawOf (p, "drive") < 0.5f ? 0.90f : 0.10f);   // edit ONE parameter only
        p.pollUndoCoalesce();
        checkNear ((double) rawOf (p, "width"), (double) restoredWidth, 1.0e-6,
                   "a parameter the user did not touch still holds the restored session's value");
        check (d2::saveOf (p) == d2::saveOf (p), "the save is stable");
    }
}

// ---------------------------------------------------------------------------
//  State test 48 -- a replacement that overlaps a restore's decode is not
//  mistaken for the restore's own sound (D-2 round 6, review finding
//  "overlapping replacement mixes saved sessions").
//
//  The adoption re-installs the restored sound only when another state set has
//  been installed since the decode (§10/§12). It decides that by comparing the
//  whole-sound-replacement counter against the token the decode recorded -- and
//  round 5 recorded that token by READING the counter back after applying the
//  sound. A wholesale replacement landing in the gap between the two therefore
//  became the restore's recorded token: the counter then already equalled it, the
//  adoption concluded "nothing has replaced my sound", and the restored metadata
//  was published over the replacement's sound. The very split §10 exists to stop,
//  reached through the token instead of through the window.
//
//  The token is now the one the restore's own sound install was handed (§13), so
//  it names an operation rather than a moment and no other operation can be
//  handed it. The seam fires inside the decode, exactly where the old read sat.
//
//  Three legs through that one seam, which is what makes the discriminator's
//  behaviour visible rather than merely asserted: two WHOLESALE replacements (an
//  A/B apply and a preset load) must be superseded by the adoption, and an
//  ORDINARY EDIT at the identical instant must survive it (§12's rule, checked at
//  the same timing as the case that must not survive).
// ---------------------------------------------------------------------------
static void testOverlappingReplacementIsNotTheRestoresOwnSound()
{
    std::printf ("State test 48: a replacement overlapping the decode is not the restore's own sound (D-2 r6)\n");

    const auto P = d2::author ("D2-R6-P", 0.18f, 0.82f, 0, 1);   // the outgoing session
    const auto R = d2::author ("D2-R6-R", 0.38f, 0.62f, 1, 2);   // the restore that arrives
    const float restoredWidth = R.active == 0 ? R.widthA : R.widthB;
    const float outgoingOther = P.active == 0 ? P.widthB : P.widthA;
    check (! juce::exactlyEqual (restoredWidth, outgoingOther),
           "non-vacuity: the restored sound and the slot an A/B switch would apply differ");

    enum Overlap { abSwitch = 0, presetLoad, ordinaryEdit };
    const char* const names[] = { "an A/B apply", "a preset load", "an ordinary edit" };

    for (int which = 0; which < 3; ++which)
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        d2::restoreFrom (p, P.blob);                       // inline: the outgoing session, whole
        check (d2::View::of (p).matches (P), "the outgoing session is live before the restore arrives");

        const int factoryCount = [&]
        {
            int n = 0;
            for (const auto& e : p.getPresets().entries()) if (e.isFactory) ++n;
            return n;
        }();
        check (factoryCount >= 1, "non-vacuity: at least one factory preset to load");

        // The edit leg's value, chosen away from the restored sound so "survived" and
        // "was replaced by the restore" cannot look the same.
        const float editedWidth = restoredWidth < 0.5f ? restoredWidth + 0.30f : restoredWidth - 0.30f;

        // The overlap runs on the owner's thread while the restoring thread sits inside
        // its decode, immediately after it installed the restored sound.
        std::atomic<int> phase { 0 };
        p.seams.afterRestoreSoundApplied = [&]
        {
            phase.store (1, std::memory_order_release);
            for (int spins = 0; phase.load (std::memory_order_acquire) != 2 && spins < 4000000; ++spins)
                std::this_thread::yield();
        };
        std::thread host ([&] { d2::restoreFrom (p, R.blob); });
        for (int spins = 0; phase.load (std::memory_order_acquire) != 1 && spins < 4000000; ++spins)
            std::this_thread::yield();
        check (phase.load() == 1, "the restoring thread is inside its decode, sound applied");
        checkNear ((double) rawOf (p, "width"), (double) restoredWidth, 1.0e-6,
                   "the restore's sound is live at the seam");

        switch (which)
        {
            case abSwitch:    p.abSwitchTo (p.abActiveSlot() == 0 ? 1 : 0); break;
            case presetLoad:  p.getPresets().load (0); p.pollUndoCoalesce(); break;
            default:          { auto* rp = p.getAPVTS().getParameter ("width");
                                rp->beginChangeGesture();
                                rp->setValueNotifyingHost (editedWidth);
                                rp->endChangeGesture(); } break;
        }

        phase.store (2, std::memory_order_release);
        host.join();
        p.seams.afterRestoreSoundApplied = nullptr;

        p.pollUndoCoalesce();   // the adoption

        // Whatever happened at the seam, the published session is ONE session.
        check (d2::View::of (p).matches (R), "the restored program is adopted, whole");
        check (p.getInternal().oversampleIndex() == R.oversampleId - 1, "...with the restored Oversampling");

        if (which == ordinaryEdit)
        {
            // §12: an edit belongs to the restored session and stands.
            checkNear ((double) rawOf (p, "width"), (double) editedWidth, 1.0e-6,
                       "an ordinary edit at that instant survives the adoption (the restored session, edited)");
            AnamorphAudioProcessor control;
            control.prepareToPlay (48000.0, 512);
            d2::restoreFrom (control, R.blob);
            auto* rp = control.getAPVTS().getParameter ("width");
            rp->beginChangeGesture(); rp->setValueNotifyingHost (editedWidth); rp->endChangeGesture();
            check (d2::saveOf (p) == d2::saveOf (control),
                   "...and a save equals a message-thread restore of the same session with the same edit");
        }
        else
        {
            // §10/§13: a wholesale replacement is another session's sound, and the
            // adoption re-installs its own rather than wearing it.
            checkNear ((double) rawOf (p, "width"), (double) restoredWidth, 1.0e-6,
                       "the restored SOUND stands: the overlapping replacement was not taken for the restore's own");
            check (d2::saveOf (p) == R.blob,
                   "a save is byte-identical to the session restored: no metadata from one session over sound from another");
            juce::MemoryBlock hostSave;
            d2::offMessageThread ([&] { hostSave = d2::saveOf (p); });
            check (hostSave == d2::saveOf (p), "...and a host-thread save agrees with the owner's");
        }
        std::printf ("  overlap = %s: one coherent session\n", names[which]);
    }
}

// ---------------------------------------------------------------------------
//  State test 49 -- a replacement that BEGAN before the restore's own but
//  FINISHED after it cannot leave its sound under the restored metadata
//  (D-2 round 7, review finding "overlapping replacements mix saved sessions").
//
//  Round 6 gave each wholesale replacement a token no other replacement can be
//  handed, which fixed attributing someone else's token to oneself. But the
//  token was allocated at the START of the replacement, so the counter ordered
//  replacements by when they BEGAN. Begin order is not completion order, and it
//  is the completion order that decides whose sound is live: each wholesale
//  replacement writes every sound parameter, so the one that finishes last owns
//  them all.
//
//  The interleaving that exploits the difference: an A/B apply X begins (token
//  n+1) and is held before its writes; the restore's own sound install then runs
//  to completion (token n+2, which the decode records); X then finishes writing,
//  so the LIVE sound is X's while the counter still reads n+2. At the adoption
//  `counter == d.soundSetGen`, the guard concludes "nothing has replaced my
//  sound", the re-install is skipped -- and the restored metadata is published
//  over X's sound.
//
//  The token is now allocated after the last write (§14), so X's completion
//  raises the counter above the restore's token and the adoption re-installs.
//  The `begin`/token bracket additionally catches the case where the two
//  interleave so tightly that neither finished cleanly inside the other: the
//  restore then records "no owner provable" (0), which never compares equal, so
//  the conservative answer -- re-install and publish one coherent session -- is
//  the one that is reached.
//
//  Three legs through the same seam, so the ordering rule is shown to hold for
//  every replacement that shares the mechanism: an A/B apply, an undo, and a
//  preset load. The ordinary-edit control lives in State tests 47 and 48 and is
//  not repeated.
// ---------------------------------------------------------------------------
static void testReplacementFinishingLastCannotWearRestoredMetadata()
{
    std::printf ("State test 49: a replacement finishing after the restore's cannot wear its metadata (D-2 r7)\n");

    const auto P = d2::author ("D2-R7-P", 0.16f, 0.84f, 0, 1);   // the outgoing session
    const auto R = d2::author ("D2-R7-R", 0.36f, 0.64f, 1, 2);   // the restore that arrives
    const float restoredWidth = R.active == 0 ? R.widthA : R.widthB;

    enum Kind { abApply = 0, undoStep, presetLoad };
    const char* const names[] = { "an A/B apply", "an undo", "a preset load" };

    for (int kind = 0; kind < 3; ++kind)
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        d2::restoreFrom (p, P.blob);                       // inline: the outgoing session, whole

        // The undo leg needs history to step back to, built before the restore arrives.
        if (kind == undoStep)
        {
            auto* rp = p.getAPVTS().getParameter ("width");
            rp->beginChangeGesture(); rp->setValueNotifyingHost (0.44f); rp->endChangeGesture();
            p.pollUndoCoalesce();
            check (p.canUndo(), "non-vacuity: the undo leg has history to step back to");
        }

        // The overlapping replacement is held after it has begun and before it writes.
        // While it waits there, the restoring thread installs the restored sound and
        // completes its handoff; the replacement then finishes writing, so it is the
        // LAST writer of every sound parameter.
        std::atomic<int> phase { 0 };
        p.seams.beforeSoundReplacementWrites = [&]
        {
            if (phase.load (std::memory_order_acquire) != 0) return;   // only the first replacement waits
            phase.store (1, std::memory_order_release);
            for (int spins = 0; phase.load (std::memory_order_acquire) != 2 && spins < 4000000; ++spins)
                std::this_thread::yield();
        };

        std::atomic<bool> restoreDone { false };
        std::thread host ([&]
        {
            for (int spins = 0; phase.load (std::memory_order_acquire) != 1 && spins < 4000000; ++spins)
                std::this_thread::yield();
            d2::restoreFrom (p, R.blob);                   // begins and completes inside the held replacement
            restoreDone.store (true, std::memory_order_release);
            phase.store (2, std::memory_order_release);    // release the replacement's writes
        });

        switch (kind)
        {
            case abApply:   p.abSwitchTo (p.abActiveSlot() == 0 ? 1 : 0); break;
            case undoStep:  p.undo();                                     break;
            default:        p.getPresets().load (0); p.pollUndoCoalesce(); break;
        }

        host.join();
        p.seams.beforeSoundReplacementWrites = nullptr;
        check (restoreDone.load(), "the restore ran inside the held replacement");

        p.pollUndoCoalesce();   // the adoption

        // One session: the restored metadata AND the restored sound, whichever of the
        // two operations happened to write last.
        check (d2::View::of (p).matches (R), "the restored program is adopted, whole");
        checkNear ((double) rawOf (p, "width"), (double) restoredWidth, 1.0e-6,
                   "the restored SOUND stands: the replacement that finished last did not keep the parameters");
        check (p.getInternal().oversampleIndex() == R.oversampleId - 1, "...with the restored Oversampling");
        check (d2::saveOf (p) == R.blob,
               "a save is byte-identical to the session restored: no metadata from one session over sound from another");
        juce::MemoryBlock hostSave;
        d2::offMessageThread ([&] { hostSave = d2::saveOf (p); });
        check (hostSave == d2::saveOf (p), "...and a host-thread save agrees with the owner's");
        std::printf ("  overlap = %s finishing last: one coherent session\n", names[kind]);
    }
}

// ---------------------------------------------------------------------------
//  State test 50 -- the drain reaches a FIXED POINT, so a restore arriving during
//  an adoption cannot discard the action that follows it (D-2 round 8, review
//  finding "later restore bypasses state drain").
//
//  Every message-thread entry point drains before it touches program state, and
//  the guarantee it needs from that drain is not "one restore was adopted" but
//  "nothing is pending any more" -- only then is the state it goes on to edit the
//  state of every restore that has arrived. The drain took exactly one restore, so
//  a second arriving DURING the adoption stayed in the cell: the caller edited a
//  session that was already superseded, and the later adoption wholesale-overwrote
//  the edit, even though the edit came after that restore's arrival, which is
//  precisely the case §10 says must land on top of it.
//
//  Reproduced through the adoption seam: R1 is pending; the A/B switch's own drain
//  takes it; R2 arrives from a host thread inside that adoption; the drain must go
//  on to adopt R2 as well, so the switch that follows is applied to R2's slots and
//  survives. The seam count is the direct evidence -- two fires inside ONE
//  abSwitchTo call -- and the A/B state is the observable one, chosen so the two
//  outcomes differ: R1 and R2 have different active slots, so a switch applied to
//  R1's session and then overwritten by R2 lands on the opposite slot from a switch
//  applied to R2's.
// ---------------------------------------------------------------------------
static void testDrainReachesFixedPointBeforeTheCallerActs()
{
    std::printf ("State test 50: the restore drain reaches a fixed point before the caller acts (D-2 r8)\n");

    const auto R1 = d2::author ("D2-R8-1", 0.22f, 0.78f, 0, 1);   // active slot A
    const auto R2 = d2::author ("D2-R8-2", 0.42f, 0.58f, 1, 2);   // active slot B
    check (R1.active != R2.active, "non-vacuity: the two sessions sit on different A/B slots");

    AnamorphAudioProcessor p;
    p.prepareToPlay (48000.0, 512);

    d2::offMessageThread ([&] { d2::restoreFrom (p, R1.blob); });   // R1 pending

    int seamRuns = 0;
    p.seams.afterRestoreTake = [&]
    {
        ++seamRuns;
        if (seamRuns == 1)
            d2::offMessageThread ([&] { d2::restoreFrom (p, R2.blob); });   // R2 arrives DURING R1's adoption
    };

    // One entry point: its drain must leave nothing pending before the switch.
    const int switchTo = 1 - R2.active;
    p.abSwitchTo (switchTo);
    p.seams.afterRestoreTake = nullptr;

    check (seamRuns == 2, "the drain adopted BOTH restores before the switch -- it reached a fixed point");
    check (p.abActiveSlot() == switchTo, "the switch landed and stands");

    // The switch was applied to R2's slot set, which is what "after R2's arrival" means.
    const float expected = switchTo == 0 ? R2.widthA : R2.widthB;
    checkNear ((double) rawOf (p, "width"), (double) expected, 1.0e-6,
               "the switch applied R2's slot, not a slot from the session it superseded");

    // Nothing is pending, so a further drain changes nothing -- the fixed point holds.
    const auto afterSwitch = d2::saveOf (p);
    p.pollUndoCoalesce();
    check (p.abActiveSlot() == switchTo, "a later drain does not undo the switch");
    check (d2::saveOf (p) == afterSwitch, "...and changes nothing at all");
}

// ---------------------------------------------------------------------------
//  State test 51 -- a preset saved while a restore is pending is clean against
//  its own file (D-2 round 8, review finding "overlapping restore mislabels
//  saved preset").
//
//  `saveUser` wrote the file, THEN adopted a pending host restore, THEN read the
//  live sound as the clean baseline. The restore sat between the bytes and the
//  baseline: the file held the outgoing session's sound while `sigAtLoad` came
//  from the restored one, so the preset was marked clean against a sound its own
//  file does not contain. Reloading it changed what you heard while the indicator
//  said nothing had changed.
//
//  The invariant this pins is the user-visible meaning of clean: RELOADING THE
//  SELECTED PRESET REPRODUCES THE SOUND IT WAS SAVED WITH. That is checked
//  directly -- save, then load the very file just written and compare the sound --
//  because it holds whichever way the implementation gets there, and it is exactly
//  what a false-clean breaks. A control leg with no restore pending proves the
//  test is not vacuous.
// ---------------------------------------------------------------------------
static void testPresetSavedDuringRestoreIsCleanAgainstItsOwnFile()
{
    std::printf ("State test 51: a preset saved while a restore is pending is clean against its own file (D-2 r8)\n");

    const auto P = d2::author ("D2-R8-P", 0.24f, 0.76f, 0, 1);   // the outgoing session
    const auto R = d2::author ("D2-R8-R", 0.64f, 0.36f, 0, 2);   // the restore that is pending at save time
    const float outgoingWidth = P.active == 0 ? P.widthA : P.widthB;
    const float restoredWidth = R.active == 0 ? R.widthA : R.widthB;
    check (! juce::exactlyEqual (outgoingWidth, restoredWidth),
           "non-vacuity: the two sessions' live sounds differ");

    const juce::String name ("AnamorphHarness-D2R8");
    auto presetFile = anamorph::PresetManager::presetDirectory()
                          .getChildFile (name + anamorph::PresetManager::fileSuffix());
    // The test writes into the REAL user preset folder (the production path), so a
    // genuine user preset with the harness name is parked and put back afterwards.
    auto parked = juce::File::createTempFile (".d2r8parked");
    const bool hadUserFile = presetFile.existsAsFile();
    if (hadUserFile) { parked.deleteFile(); presetFile.moveFileTo (parked); }

    for (int leg = 0; leg < 2; ++leg)   // 0 = a restore is pending at save time, 1 = the control
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        d2::restoreFrom (p, P.blob);                                   // inline: the outgoing session
        checkNear ((double) rawOf (p, "width"), (double) outgoingWidth, 1.0e-6, "the outgoing session is live");

        if (leg == 0)
        {
            // The restore has to be pending AND its adoption has to MOVE the sound, or
            // the save has nothing to be incoherent about: a restore applies its sound at
            // decode, so a merely-pending one leaves the live sound already equal to what
            // a save would write. The adoption moves the sound exactly when another
            // wholesale replacement has landed since the decode (§10/§14) -- so R arrives
            // while an A/B switch is mid-write, the switch finishes last and its sound is
            // live, and R -- still pending, because the switch's own drain ran before it
            // arrived -- will re-install its own sound when it is finally adopted.
            std::atomic<int> phase { 0 };
            p.seams.beforeSoundReplacementWrites = [&]
            {
                if (phase.load (std::memory_order_acquire) != 0) return;
                phase.store (1, std::memory_order_release);
                for (int spins = 0; phase.load (std::memory_order_acquire) != 2 && spins < 4000000; ++spins)
                    std::this_thread::yield();
            };
            std::thread host ([&]
            {
                for (int spins = 0; phase.load (std::memory_order_acquire) != 1 && spins < 4000000; ++spins)
                    std::this_thread::yield();
                d2::restoreFrom (p, R.blob);
                phase.store (2, std::memory_order_release);
            });
            p.abSwitchTo (1 - p.abActiveSlot());
            host.join();
            p.seams.beforeSoundReplacementWrites = nullptr;
            check (! juce::exactlyEqual (rawOf (p, "width"), restoredWidth),
                   "the A/B switch finished last, so the live sound is not the restore's yet");
        }

        check (p.getPresets().saveUser (name), "saveUser succeeds");
        check (presetFile.existsAsFile(), "the preset file was written");
        check (! p.getPresets().isDirty(), "the preset the save selected reads CLEAN");

        const int idx = p.getPresets().currentIndex();
        check (idx >= 0, "the saved preset is the selected row");
        const float soundAtSave = rawOf (p, "width");
        if (leg == 0)
            checkNear ((double) soundAtSave, (double) restoredWidth, 1.0e-6,
                       "the save adopted the pending restore first, so it wrote the session the plug-in is on");
        else
            checkNear ((double) soundAtSave, (double) outgoingWidth, 1.0e-6,
                       "the control save wrote the session that was live");

        // What CLEAN means: reloading the selected preset reproduces the sound it was
        // saved with. A baseline taken from a different session than the file breaks
        // exactly this, and nothing else in the save path can.
        p.getPresets().load (idx);
        p.pollUndoCoalesce();
        checkNear ((double) rawOf (p, "width"), (double) soundAtSave, 1.0e-6,
                   "reloading the preset just saved reproduces the sound it was clean against");
        check (! p.getPresets().isDirty(), "...and it is still clean after that reload");
    }

    presetFile.deleteFile();
    if (hadUserFile) { parked.moveFileTo (presetFile); parked.deleteFile(); }
}

// ---------------------------------------------------------------------------
//  State test 52 -- a save's BYTES and its clean BASELINE describe one sound,
//  however busy the automation (D-2 round 9, review finding "busy automation
//  saves false baseline").
//
//  `saveUser` took TWO reads of the live sound -- a signature, then the state copy
//  the file is written from -- and tried to prove them coherent by re-reading the
//  sound generation, retrying up to eight times. Under SUSTAINED automation every
//  attempt fails, and the loop then FELL THROUGH and used the unproven pair: the
//  file held one sound and `sigAtLoad` described another. Its stated fallback --
//  that such a disagreement "reads as dirty rather than as a false clean" -- is
//  what fails here, because a baseline describing an EARLIER sound reads clean
//  again the moment the sound RETURNS to it, which is exactly what cycling
//  automation does. The preset then sat there marked clean while reloading it
//  changed what you heard.
//
//  The invariant pinned is stated so that it holds whatever the implementation
//  does to get there, and it is the user-visible one:
//      IF THE SELECTED PRESET READS CLEAN, RELOADING IT CHANGES NOTHING.
//  It is checked at BOTH values the automation cycles through, so a baseline that
//  named either of them is caught; the false clean is not a value the test has to
//  guess. Alongside it the structural form is asserted directly -- the baseline
//  equals the signature of the bytes actually on disk.
//
//  WHICH LEG DISCRIMINATES, measured rather than assumed. Against the round-8
//  two-read save only leg (b) fails: its seam CYCLES, so the generation check never
//  settles and the retry gives up. Legs (a) and (c) mutate to the same values on
//  every fire, so round 8's second attempt is stable and its retry succeeds -- they
//  cover the shape, not the defect. Leg (d) is a real thread and therefore timing-
//  dependent; it is a robustness leg, never the discriminator. Deleting leg (b)
//  would leave the whole defect uncaught, which is why this paragraph exists.
//
//  The `beforeStateCapture` seam makes the interleaving exact instead of a race to
//  lose: it fires at the one instant a mutation must land to split the two reads.
//  Every leg asserts that it actually fired, because a seam that stopped landing in
//  the split would make every leg pass while testing nothing.
// ---------------------------------------------------------------------------
static void testSaveBaselineDescribesTheBytesUnderAutomation()
{
    std::printf ("State test 52: a save's bytes and its clean baseline describe one sound (D-2 r9)\n");

    const juce::String name ("AnamorphHarness-D2R9");
    auto presetFile = anamorph::PresetManager::presetDirectory()
                          .getChildFile (name + anamorph::PresetManager::fileSuffix());

    // The test writes into the REAL user preset folder (the production path), so a
    // genuine user preset with the harness name is parked and put back afterwards.
    // RAII, not a trailing pair of statements: an assertion that aborts the process
    // mid-leg must not leave a user's preset parked in the temp directory.
    struct ParkedPreset
    {
        explicit ParkedPreset (juce::File f)
            : live (std::move (f)),
              parked (juce::File::createTempFile (".d2r9parked")),
              had (live.existsAsFile())
        {
            if (had) { parked.deleteFile(); live.moveFileTo (parked); }
        }
        ~ParkedPreset()
        {
            live.deleteFile();
            if (had) { parked.moveFileTo (live); parked.deleteFile(); }
        }
        juce::File live, parked;
        bool had;
    };
    const ParkedPreset guard { presetFile };

    // The signature of the sound the FILE ON DISK restores -- read back through the
    // same resolution the loader uses, so "the baseline describes the bytes" is a
    // claim about the bytes and not about the writer's intentions.
    auto signatureOfFile = [] (AnamorphAudioProcessor& proc, const juce::File& f)
    {
        auto xml = juce::parseXML (f);
        return xml == nullptr ? juce::String()
                              : anamorph::PresetManager::soundSignatureForSavedTree (
                                    proc.getAPVTS(), juce::ValueTree::fromXml (*xml));
    };

    // Every seam leg ends here: the baseline must name the bytes, and a preset that
    // reads clean must reload without moving the sound. Checked at BOTH cycled values,
    // and the clean branch is ASSERTED to have been taken -- otherwise an
    // always-dirty regression would silently delete the reload invariant from the run.
    auto assertCoherent = [&] (AnamorphAudioProcessor& proc, const juce::File& f,
                               float valueA, float valueB, int seamFires, const char* leg)
    {
        auto& pm = proc.getPresets();
        check (seamFires >= 1,
               (juce::String ("the capture seam fired (") + leg + ")").toRawUTF8());
        checkStr (pm.baseline(), signatureOfFile (proc, f),
                  (juce::String ("the clean baseline is the signature of the bytes on disk (")
                       + leg + ")").toRawUTF8());

        int cleanLegs = 0;
        for (const float v : { valueA, valueB })
        {
            setRaw (proc, "width", v);
            if (pm.isDirty()) continue;      // dirty is always safe: the star can only over-report
            ++cleanLegs;
            const auto soundBefore = anamorph::PresetManager::soundSignatureFor (proc.getAPVTS());
            check (pm.loadFile (f), "the preset file the save wrote loads back");
            proc.pollUndoCoalesce();
            checkStr (anamorph::PresetManager::soundSignatureFor (proc.getAPVTS()), soundBefore,
                      (juce::String ("a preset that reads CLEAN reloads without changing the sound (")
                           + leg + ")").toRawUTF8());
        }
        // The file holds ONE of the two cycled values, so setting the live sound to that
        // one must read clean. Zero clean legs means the reload invariant above never
        // ran, which is a hole, not a pass.
        check (cleanLegs >= 1,
               (juce::String ("the clean branch was exercised: one cycled value reads clean (")
                    + leg + ")").toRawUTF8());
        std::printf ("  %s: %d of 2 cycled values read clean\n", leg, cleanLegs);
    };

    const float A = 0.24f, B = 0.76f;   // the two values the automation cycles between
    check (! juce::exactlyEqual (A, B), "non-vacuity: the cycled values differ");

    // --- (a) ONE mutation inside the capture window --------------------------
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        setRaw (p, "width", A);
        int fires = 0;
        p.getPresets().beforeStateCapture = [&] { ++fires; setRaw (p, "width", B); };
        check (p.getPresets().saveUser (name), "saveUser succeeds");
        p.getPresets().beforeStateCapture = nullptr;
        assertCoherent (p, presetFile, A, B, fires, "one mutation in the window");
    }

    // --- (b) SUSTAINED automation: the seam CYCLES on every fire --------------
    // THE DISCRIMINATOR. Against the round-8 implementation the generation check never
    // settles, its retry gives up, and the pair it writes was never proven coherent --
    // so the bytes hold one of these two values and the baseline names the other.
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        setRaw (p, "width", A);
        int fires = 0;
        p.getPresets().beforeStateCapture = [&]
        {
            ++fires;
            setRaw (p, "width", (fires % 2) == 1 ? B : A);   // cycle, like an LFO on the lane
        };
        check (p.getPresets().saveUser (name), "saveUser succeeds under sustained automation");
        p.getPresets().beforeStateCapture = nullptr;
        assertCoherent (p, presetFile, A, B, fires, "sustained cycling automation");
    }

    // --- (c) SEVERAL parameters mutated inside the window --------------------
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        setRaw (p, "width", A);
        const float driveA = rawOf (p, "drive");
        const float driveB = driveA < 0.5f ? driveA + 0.30f : driveA - 0.30f;
        int fires = 0;
        p.getPresets().beforeStateCapture = [&]
        {
            ++fires;
            setRaw (p, "width", B);
            setRaw (p, "drive", driveB);
            setRaw (p, "amount", 0.61f);
        };
        check (p.getPresets().saveUser (name), "saveUser succeeds with several parameters moving");
        p.getPresets().beforeStateCapture = nullptr;
        assertCoherent (p, presetFile, A, B, fires, "several parameters in the window");
    }

    // --- (d) a REAL automation thread across the whole save ------------------
    // NOT the discriminator -- leg (b) is -- but the case the finding describes, driven
    // by an actual concurrent writer rather than a seam. Whatever interleaving it
    // happens to produce, the invariant is the same one. It is also the leg that puts
    // the new capture path under ThreadSanitizer in the tsan lane.
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        setRaw (p, "width", A);
        int fires = 0;
        p.getPresets().beforeStateCapture = [&] { ++fires; };
        std::atomic<bool> run { true };
        std::atomic<int>  writes { 0 };
        std::thread automation ([&]
        {
            for (int i = 0; run.load (std::memory_order_acquire); ++i)
            {
                setRaw (p, "width", (i % 2) == 0 ? B : A);
                writes.fetch_add (1, std::memory_order_relaxed);
                std::this_thread::yield();
            }
        });
        const bool ok = p.getPresets().saveUser (name);
        run.store (false, std::memory_order_release);
        automation.join();
        p.getPresets().beforeStateCapture = nullptr;
        check (ok, "saveUser succeeds with an automation thread running");
        check (writes.load() > 0, "non-vacuity: the automation thread actually wrote");
        assertCoherent (p, presetFile, A, B, fires, "a concurrent automation thread");
    }

    // --- (e) a DISCRETE parameter carrying an automated sub-step value -------
    // No concurrency at all. A preset stores the SNAPPED value, so a 4-choice that host
    // automation left at 0.66 reloads as 0.66667 -- and a baseline taken from the raw
    // live parameter therefore described a sound the file cannot hold, so the preset
    // went dirty the instant it was reloaded. Both signatures are now built from
    // `normalisedAsRendered` (§17), which is the same defect one level below the window.
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        setRaw (p, "algorithm", 0.66f);        // a mid-step raw, exactly as automation writes it
        // The leg only means anything while RawChoice keeps the raw value UNSNAPPED. If
        // that ever changed, getValue() would read back 0.66667 and the leg would pass
        // while testing nothing, so the sub-step property is asserted rather than assumed.
        check (juce::exactlyEqual (rawOf (p, "algorithm"), 0.66f),
               "non-vacuity: the discrete parameter kept the exact sub-step raw value");
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p.getAPVTS().getParameter ("algorithm")))
            check (! juce::exactlyEqual (normalisedAsRendered (*rp, 0.66f), 0.66f),
                   "...and that value is NOT what the plug-in renders, so the two could disagree");
        check (p.getPresets().saveUser (name), "saveUser succeeds for a sub-step discrete value");
        check (! p.getPresets().isDirty(), "the freshly saved preset reads clean");
        const auto baselineAtSave = p.getPresets().baseline();
        checkStr (baselineAtSave, signatureOfFile (p, presetFile),
                  "the clean baseline is the signature of the bytes on disk (sub-step discrete)");
        check (p.getPresets().loadFile (presetFile), "the preset file loads back");
        p.pollUndoCoalesce();
        check (! p.getPresets().isDirty(),
               "...and reloading a preset saved at a sub-step discrete value leaves it CLEAN");
        checkStr (anamorph::PresetManager::soundSignatureFor (p.getAPVTS()), baselineAtSave,
                  "...reproducing exactly the sound it was clean against");
    }

    // --- (f) the SAVE-TIME equality, measured across the whole domain ---------
    // soundSignatureForSavedTree must return exactly what soundSignatureFor returns for
    // the same live state -- that is what makes a freshly saved preset read clean. It is
    // true by construction, both sides reducing to convertTo0to1(convertFrom0to1(live)):
    // the live side applies `normalisedAsRendered` once, and the FILE side applies it
    // ZERO times because the tree already holds the denormalised value. Getting that
    // count wrong is the failure this leg exists for -- the mapping is the identity in
    // real arithmetic but NOT idempotent in float for the four frequency ranges built
    // from custom log/exp lambdas, so one extra application moves the last bits and, at a
    // 5-decimal rounding boundary, the printed signature: a freshly saved preset then
    // reads MODIFIED. A uniform grid never meets such a boundary, so the sweep is
    // uniform THEN pseudo-random, and the random points are generated arithmetically
    // rather than through a std:: distribution, whose point set is implementation-defined.
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        auto& apvts = p.getAPVTS();

        std::vector<juce::RangedAudioParameter*> ranged;
        int nonRanged = 0;
        for (auto* q : p.getParameters())
            if (auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*> (q))
                if (! pid::isPresetExcluded (wid->paramID))
                {
                    if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (q)) ranged.push_back (rp);
                    else ++nonRanged;
                }
        check (ranged.size() >= 20, "non-vacuity: the sweep covers the real parameter set");
        // Pins soundSignatureForSavedTree's one approximate branch as DEAD: it falls back
        // to a LIVE read for a parameter that is not ranged, and this plug-in has none.
        check (nonRanged == 0,
               "every preset-carried parameter is a RangedAudioParameter, so the saved-tree "
               "signature never falls back to a live read");

        int mismatches = 0;
        juce::String firstMismatch;
        juce::uint32 lcg = 0xD2900009u;
        const int uniformPoints = 200, totalPoints = 20000;
        for (int step = 0; step <= totalPoints; ++step)
        {
            lcg = lcg * 1664525u + 1013904223u;
            const float v = step <= uniformPoints
                              ? (float) step / (float) uniformPoints
                              : (float) ((lcg >> 8) * (1.0 / 16777216.0));
            for (auto* rp : ranged) rp->setValueNotifyingHost (v);

            const auto live = anamorph::PresetManager::soundSignatureFor (apvts);
            const auto tree = anamorph::PresetManager::soundSignatureForSavedTree (apvts, apvts.copyState());
            if (live != tree && mismatches++ == 0)
                firstMismatch = "at normalised " + juce::String (v, 9);
        }
        check (mismatches == 0,
               mismatches == 0 ? "the live signature and the saved-tree signature agree across the whole"
                                 " normalised domain of every parameter"
                               : ("the two signatures disagree at " + juce::String (mismatches)
                                  + " sweep point(s); first " + firstMismatch).toRawUTF8());
        std::printf ("  save-time signature equality swept over %d points x %d parameters: %d mismatch(es)\n",
                     totalPoints + 1, (int) ranged.size(), mismatches);
    }

    // --- (g) a freshly saved preset reads CLEAN at a custom-mapped frequency --
    // Leg (f) compares the two builders; this is the same property as a user meets it,
    // at a value chosen to sit on the float round trip's rounding edge for a
    // `logFreqRange` parameter. Canonicalising the FILE side a second time makes exactly
    // this fail: the star appears on a preset nobody has touched.
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        setRaw (p, "mbFreqLow", 0.381175071f);
        check (p.getPresets().saveUser (name), "saveUser succeeds at a custom-mapped frequency value");
        check (! p.getPresets().isDirty(),
               "a freshly saved preset reads CLEAN at a custom-mapped frequency value");
        checkStr (p.getPresets().baseline(), signatureOfFile (p, presetFile),
                  "the clean baseline is the signature of the bytes on disk (custom-mapped frequency)");
    }

    // --- (h) the modified-marker and the undo history answer ONE question -----
    // Both signatures are built from the value the plug-in RENDERS. A sub-step move on a
    // discrete parameter (0.66 -> 0.67 on a 4-choice, both index 2) changes neither the
    // DSP input nor anything a file can hold, so it must be a change to NEITHER -- if the
    // marker ignored it while the coalescer recorded it, the user would get an undo entry
    // that, pressed, moves neither the sound nor the star.
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        const float atStart = rawOf (p, "algorithm");
        check (! p.canUndo(), "non-vacuity: a fresh instance has no undo history");

        auto gesture = [&] (float v)
        {
            auto* rp = p.getAPVTS().getParameter ("algorithm");
            rp->beginChangeGesture(); rp->setValueNotifyingHost (v); rp->endChangeGesture();
            p.pollUndoCoalesce();
        };

        gesture (0.66f);                       // a real step change: index 2
        check (p.canUndo(), "the step change recorded an undo step");
        const bool dirtyAfterStep = p.getPresets().isDirty();

        gesture (0.67f);                       // WITHIN the same step: index 2 still
        checkNear ((double) rawOf (p, "algorithm"), 0.67, 1.0e-6, "the sub-step move took effect");
        check (p.getPresets().isDirty() == dirtyAfterStep,
               "a sub-step move on a discrete parameter does not move the modified-marker");

        // Decisive: if the sub-step move had recorded its own step, one undo would land on
        // 0.66. It records none, so one undo goes back past both, to where we started.
        p.undo();
        p.pollUndoCoalesce();
        checkNear ((double) rawOf (p, "algorithm"), (double) atStart, 1.0e-6,
                   "...and it recorded no undo step either -- one undo goes back past both moves");
    }

    // --- (i) the SAVE -> LOAD round trip, measured -----------------------------
    // The equality leg (f) pins is the one that must be EXACT, because it is what makes a
    // freshly saved preset read clean. Loading is a different question: a load applies
    // convertTo0to1(plain) and the parameter then reports the value it stores, so the
    // sound after a reload passes through the range round trip once MORE than the
    // baseline did. That extra application is not exactly the identity in float, and it
    // cannot be made so from this side -- a preset stores one lossy denormalised number
    // per parameter, which is the format, not a defect.
    //
    // So this leg asserts what must hold exactly and MEASURES what cannot:
    //   * exact -- a reloaded preset never reads modified, and the reloaded sound is the
    //     saved sound to well inside anything audible;
    //   * measured -- the printed counts record how often the baseline STRING and the
    //     re-saved BYTES differ in their last digits across a round trip. The byte drift
    //     is pre-existing (it is the preset format's denormalised round trip and predates
    //     this round entirely); what matters is that it never moves the marker.
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        auto& apvts = p.getAPVTS();
        std::vector<juce::RangedAudioParameter*> ranged;
        for (auto* q : p.getParameters())
            if (auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*> (q))
                if (! pid::isPresetExcluded (wid->paramID))
                    if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (q))
                        ranged.push_back (rp);

        auto probe = juce::File::createTempFile (anamorph::PresetManager::fileSuffix());
        juce::uint32 lcg = 0x5EED1234u;
        int sigDrift = 0, byteDrift = 0, markerWrong = 0, soundMoved = 0, loadsFailed = 0;
        const int points = 3000;
        for (int i = 0; i < points; ++i)
        {
            lcg = lcg * 1664525u + 1013904223u;
            const float v = (float) ((lcg >> 8) * (1.0 / 16777216.0));
            for (auto* rp : ranged) rp->setValueNotifyingHost (v);

            const auto tree      = apvts.copyState();
            const auto sigAtSave = anamorph::PresetManager::soundSignatureForSavedTree (apvts, tree);
            // What the file can hold, not what the live parameter happens to carry: a
            // discrete parameter left at a sub-step value snaps when the preset reloads,
            // by design, so comparing against the raw live value would be asserting that
            // presets store something they cannot.
            std::vector<float> before;
            for (auto* rp : ranged) before.push_back (normalisedAsRendered (*rp, rp->getValue()));

            auto xml = tree.createXml();
            if (xml == nullptr || ! probe.replaceWithText (xml->toString())) { ++loadsFailed; continue; }
            if (! p.getPresets().loadFile (probe)) { ++loadsFailed; continue; }

            if (p.getPresets().isDirty()) ++markerWrong;
            if (anamorph::PresetManager::soundSignatureFor (apvts) != sigAtSave) ++sigDrift;
            for (size_t k = 0; k < ranged.size(); ++k)
                if (std::abs (ranged[k]->getValue() - before[k]) > 1.0e-4f) { ++soundMoved; break; }
            if (auto after = apvts.copyState().createXml(); after != nullptr && after->toString() != xml->toString())
                ++byteDrift;
        }
        probe.deleteFile();

        check (loadsFailed == 0, "every probe preset written by the round trip loaded back");
        check (markerWrong == 0, "a reloaded preset NEVER reads modified, across the whole sweep");
        check (soundMoved == 0, "the reloaded sound is the saved sound at every swept value");
        std::printf ("  save->load round trip over %d points: marker wrong %d, sound moved %d,"
                     " baseline-string drift %d, re-saved-byte drift %d (the last two are the preset"
                     " format's own denormalised round trip and move no marker)\n",
                     points, markerWrong, soundMoved, sigDrift, byteDrift);
    }
}


// ---------------------------------------------------------------------------
//  State test 53 -- an A/B toggle chooses its destination from the session the
//  plug-in is ON, after any pending restore has been adopted (D-2 round 10,
//  review finding "A/B toggle uses stale selection").
//
//  The editor computed the destination itself -- `abSwitchTo (abActiveSlot() == 0
//  ? 1 : 0)` -- from a read taken BEFORE abSwitchTo's own drain adopted the
//  pending restore. When that restore flipped the active slot, the computed target
//  was the slot the restore had just made active, `slot == abActive` held, and the
//  switch returned without switching: the user's toggle did nothing, and did
//  nothing silently. The destination is now derived by the processor after the
//  drain (abToggle), so the decision and the state it is about are the same
//  session. Both flips are covered so a fix that happened to work for one parity
//  cannot pass.
// ---------------------------------------------------------------------------
static void testAbToggleDerivesItsTargetAfterTheDrain()
{
    std::printf ("State test 53: an A/B toggle derives its target from the adopted session (D-2 r10)\n");

    for (int restoredActive = 0; restoredActive < 2; ++restoredActive)
    {
        const int outgoingActive = 1 - restoredActive;
        const auto P = d2::author ("D2-R10-P", 0.21f, 0.79f, outgoingActive, 1);
        const auto R = d2::author ("D2-R10-R", 0.41f, 0.59f, restoredActive, 2);
        check (P.active != R.active, "non-vacuity: the restore flips the active slot");

        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        d2::restoreFrom (p, P.blob);                                   // inline: the outgoing session
        check (p.abActiveSlot() == outgoingActive, "the outgoing session's slot is active");

        d2::offMessageThread ([&] { d2::restoreFrom (p, R.blob); });   // pending: flips the active slot at adoption
        // The leg means nothing unless R is still PENDING here -- a harness whose restore
        // took the inline path would adopt it before the toggle and pass while testing
        // nothing. R's sound is live (§10 applies it at decode); R's program is not.
        check (p.abActiveSlot() == outgoingActive && d2::View::of (p).matches (P),
               "non-vacuity: the restore is pending -- its program has not been adopted yet");

        // ONE user action: the toggle. It must land on the other slot of R's session.
        p.abToggle();

        const int expected = 1 - restoredActive;
        check (p.abActiveSlot() == expected,
               "the toggle switched to the OTHER slot of the restored session, not to the slot it had just made active");
        const float expectedWidth = expected == 0 ? R.widthA : R.widthB;
        checkNear ((double) rawOf (p, "width"), (double) expectedWidth, 1.0e-6,
                   "...and the live sound is that slot's sound from the restored session");
        // (No name check: each A/B slot carries its own preset name, so the switch shows
        // the other slot's, exactly as State test 45 records. The sound is the evidence.)

        // The toggle is its own inverse on the same session: a second one returns.
        p.abToggle();
        check (p.abActiveSlot() == restoredActive, "a second toggle returns to the restored active slot");
        checkNear ((double) rawOf (p, "width"), (double) (restoredActive == 0 ? R.widthA : R.widthB), 1.0e-6,
                   "...with that slot's sound");
        std::printf ("  restore flips active %d -> %d: toggle lands on %d, returns to %d\n",
                     outgoingActive, restoredActive, expected, restoredActive);
    }

    // An EXPLICIT target is intent, not a stale derivation: "go to B" after a restore
    // that already put the session on B is correctly a no-op, and "go to A" moves.
    {
        const auto P = d2::author ("D2-R10-P2", 0.21f, 0.79f, 0, 1);
        const auto R = d2::author ("D2-R10-R2", 0.41f, 0.59f, 1, 2);
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        d2::restoreFrom (p, P.blob);
        d2::offMessageThread ([&] { d2::restoreFrom (p, R.blob); });
        p.abSwitchTo (1);
        check (p.abActiveSlot() == 1, "an explicit 'switch to B' after a restore onto B is a no-op on B");
        checkNear ((double) rawOf (p, "width"), (double) R.widthB, 1.0e-6, "...and B's restored sound stands");
        p.abSwitchTo (0);
        check (p.abActiveSlot() == 0, "an explicit 'switch to A' moves");
        checkNear ((double) rawOf (p, "width"), (double) R.widthA, 1.0e-6, "...to A's restored sound");
    }
}

// ---------------------------------------------------------------------------
//  State test 54 -- a Settings publication carries only the field that changed,
//  and an edit is ordered against restores by the instant its callback observes
//  (D-2 round 10, review findings "unrelated edits revert restored oversampling"
//  and "later restores preserve earlier edits").
//
//  (B) A pending restore has already published its Oversampling into the
//  engine-config word, tagged with its generation; the tree still holds the
//  outgoing value until the adoption writes it. Every property change used to
//  republish the WHOLE tree under the latest arrival's generation, so an
//  unrelated edit -- Meters on -- re-published the tree's STALE Oversampling under
//  the pending restore's own generation, which the compare-exchange accepts as
//  "that arrival again": the engine dropped to the old factor until the adoption.
//  A publication now carries only the field that changed. (The adoption's own
//  field-by-field write cannot expose a stale Oversampling either, but with the
//  current table order -- Oversampling first -- that was never observable, so it is
//  not asserted: a check that cannot fail proves nothing. The field-level rule
//  guarantees it for any order.)
//
//  (C) WHEN an edit happens, for ordering against restores, is the instant its
//  callback reads the word's generation. Both observable orderings are pinned: a
//  restore published BEFORE the edit is superseded by it at the adoption; one
//  published AFTER the edit replaces it. For the Oversampling field itself both
//  the tree and the engine word must agree with that answer. These legs pin the
//  single-restore ORDER; that the tag is a GENERATION compared across two restores
//  (an edit after R1 but before R2 survives R1 and is replaced by R2) is State
//  test 44's fourth leg, which this test relies on rather than repeats.
// ---------------------------------------------------------------------------
static void testSettingsPublicationIsFieldLevelAndOrderedByObservation()
{
    std::printf ("State test 54: a Settings publication carries only its field; edits order by observation (D-2 r10)\n");
    using r3::SettingsSet;
    const SettingsSet P { 1, 3, 0.50, false, false, true };   // outgoing: Oversampling Off
    const SettingsSet R { 3, 4, 0.25, false, false, true };   // restore: Oversampling 4x
    check (P.oversample != R.oversample, "non-vacuity: the restore changes Oversampling");
    const auto blobP = r3::authorSession ("D2-R10-SP", P);
    const auto blobR = r3::authorSession ("D2-R10-SR", R);

    // --- (B) an unrelated edit while the restore is pending ------------------
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        d2::restoreFrom (p, blobP);
        check (p.getInternal().oversampleIndex() == P.oversample - 1, "the outgoing Oversampling is in force");

        d2::offMessageThread ([&] { d2::restoreFrom (p, blobR); });   // pending: word published, tree not
        check (p.getInternal().oversampleIndex() == R.oversample - 1,
               "the restore published its Oversampling synchronously on its own thread");
        check (r3::read (p.getInternal()).oversample == P.oversample,
               "...while the tree still holds the outgoing value (the tail is pending)");

        // The UNRELATED edit. It must not touch the engine word at all.
        r3::setField (p.getInternal(), 3, SettingsSet { 0, 0, 0.0, true, false, false });   // Meters on
        check (p.getInternal().metersOn(), "the edit took effect");
        check (p.getInternal().oversampleIndex() == R.oversample - 1,
               "an unrelated Settings edit does NOT revert the restored Oversampling in the engine");

        p.pollUndoCoalesce();   // the adoption

        const auto got = r3::read (p.getInternal());
        check (got.oversample == R.oversample, "the adoption installed the restored Oversampling in the tree");
        check (got.metersOn, "...and kept the unrelated edit made after the restore arrived (§9)");
        check (p.getInternal().oversampleIndex() == R.oversample - 1, "the engine word agrees with the tree");
        check (r3::settingsOf (d2::saveOf (p)).oversample == R.oversample, "...and a save carries it");
    }

    // --- (C.i) edit, THEN a restore arrives: the restore replaces it ----------
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        d2::restoreFrom (p, blobP);
        r3::setField (p.getInternal(), 1, SettingsSet { 0, 5, 0.0, false, false, false });   // UI Scale 5
        check (r3::read (p.getInternal()).uiScale == 5, "the edit took effect");
        d2::offMessageThread ([&] { d2::restoreFrom (p, blobR); });   // arrives AFTER the edit's callback
        p.pollUndoCoalesce();
        check (r3::read (p.getInternal()).uiScale == R.uiScale,
               "a restore published AFTER an edit replaces the edit at its adoption -- the newer arrival wins");
    }

    // --- (C.ii) a restore arrives, THEN the edit: the edit survives -----------
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        d2::restoreFrom (p, blobP);
        d2::offMessageThread ([&] { d2::restoreFrom (p, blobR); });
        r3::setField (p.getInternal(), 1, SettingsSet { 0, 5, 0.0, false, false, false });
        p.pollUndoCoalesce();
        check (r3::read (p.getInternal()).uiScale == 5,
               "an edit whose callback observes the restore's arrival survives its adoption (§9)");
        check (r3::read (p.getInternal()).oversample == R.oversample, "...while every other field is the restore's");
    }

    // --- (C.iii) the Oversampling field itself, both orders, tree AND word ----
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        d2::restoreFrom (p, blobP);
        p.getInternal().oversampleValue().setValue (2);                 // edit: 2x
        d2::offMessageThread ([&] { d2::restoreFrom (p, blobR); });   // then the restore
        check (p.getInternal().oversampleIndex() == R.oversample - 1, "the later restore's Oversampling wins the word");
        p.pollUndoCoalesce();
        check (r3::read (p.getInternal()).oversample == R.oversample && p.getInternal().oversampleIndex() == R.oversample - 1,
               "edit then restore: the restore's Oversampling stands in the tree and the word");
    }
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        d2::restoreFrom (p, blobP);
        d2::offMessageThread ([&] { d2::restoreFrom (p, blobR); });   // the restore first
        p.getInternal().oversampleValue().setValue (2);                 // then the edit
        check (p.getInternal().oversampleIndex() == 1, "the edit's Oversampling lands over the arrived restore's in the word");
        p.pollUndoCoalesce();
        check (r3::read (p.getInternal()).oversample == 2 && p.getInternal().oversampleIndex() == 1,
               "restore then edit: the edit's Oversampling stands in the tree and the word through the adoption");
        check (r3::read (p.getInternal()).uiScale == R.uiScale, "...and the untouched fields are the restore's");
    }
}

// ---------------------------------------------------------------------------
//  State test 55 -- a loaded preset's clean baseline is fixed from what the load
//  wrote, not from a read-back (D-2 round 10, ADR-0036 §18; closes KI-029).
//
//  load() and loadFile() applied the preset's sound and then took `sigAtLoad` from
//  a second, live read -- the same two-read shape round 9 removed from the save.
//  Host automation writing a sound parameter between the apply and that read made
//  the baseline describe the automated value, so the preset read CLEAN whenever
//  the automation returned to it while reloading it would move the sound. The
//  baseline now comes from the values being applied, through the same arithmetic
//  the parameters report them by (soundSignatureAfterLoading), so nothing live is
//  read and there is no window. The `beforeStateCapture` seam fires after the apply
//  and before the baseline, exactly where the read-back used to be.
//
//  The load-side signature must model the parameter's own store/report pass, which
//  the save-side one must NOT: leg (c) measures both against 3000 random round
//  trips so the reason the two flavours exist is a number rather than a claim.
// ---------------------------------------------------------------------------
static void testLoadedPresetBaselineIsFixedFromWhatTheLoadWrote()
{
    std::printf ("State test 55: a loaded preset's baseline is fixed from what the load wrote (D-2 r10, KI-029)\n");

    const juce::String name ("AnamorphHarness-D2R10");
    auto presetFile = anamorph::PresetManager::presetDirectory()
                          .getChildFile (name + anamorph::PresetManager::fileSuffix());
    struct ParkedPreset
    {
        explicit ParkedPreset (juce::File f)
            : live (std::move (f)),
              parked (juce::File::createTempFile (".d2r10parked")),
              had (live.existsAsFile())
        { if (had) { parked.deleteFile(); live.moveFileTo (parked); } }
        ~ParkedPreset() { live.deleteFile(); if (had) { parked.moveFileTo (live); parked.deleteFile(); } }
        juce::File live, parked; bool had;
    };
    const ParkedPreset guard { presetFile };

    const float A = 0.24f, B = 0.76f;

    // --- (a) automation lands between the apply and the baseline ---------------
    // Both cycled values are checked so a baseline naming either is caught: the file
    // holds A, so A must read clean and reload as a no-op; B must read DIRTY.
    for (int viaFile = 0; viaFile < 2; ++viaFile)
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        setRaw (p, "width", A);
        check (p.getPresets().saveUser (name), "the preset (width A) is on disk");
        setRaw (p, "width", 0.5f);                                     // leave the file's sound
        int fires = 0;
        p.getPresets().beforeStateCapture = [&] { ++fires; setRaw (p, "width", B); };   // automation, in the window
        if (viaFile) check (p.getPresets().loadFile (presetFile), "loadFile succeeds");
        else       { p.getPresets().refresh(); const int idx = [&] { int i = 0; for (const auto& e : p.getPresets().entries()) { if (! e.isFactory && e.name == name) return i; ++i; } return -1; }();
                     check (idx >= 0, "the saved preset is listed"); p.getPresets().load (idx); }
        p.getPresets().beforeStateCapture = nullptr;
        p.pollUndoCoalesce();
        check (fires == 1, "the seam fired once, after the apply and before the baseline");
        checkNear ((double) rawOf (p, "width"), (double) B, 1.0e-6, "the automation write landed after the apply");

        check (p.getPresets().isDirty(), "the preset reads DIRTY at the automated value, because its file holds another");
        setRaw (p, "width", A);
        check (! p.getPresets().isDirty(), "...and CLEAN at the file's value");
        const auto before = anamorph::PresetManager::soundSignatureFor (p.getAPVTS());
        check (p.getPresets().loadFile (presetFile), "reloading the clean preset");
        p.pollUndoCoalesce();
        checkStr (anamorph::PresetManager::soundSignatureFor (p.getAPVTS()), before,
                  "a preset that reads CLEAN reloads without changing the sound");
        std::printf ("  %s: automation in the window leaves the preset dirty; the file's value reads clean\n",
                     viaFile ? "loadFile" : "load");
    }

    // --- (b) every FACTORY preset is exactly clean after loading -----------------
    // The factory path fixes its baseline from the override table by the same formula;
    // exactness there is asserted, not assumed.
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        int factories = 0;
        const auto entries = p.getPresets().entries();
        for (int i = 0; i < entries.size(); ++i)
            if (entries.getReference (i).isFactory)
            {
                ++factories;
                p.getPresets().load (i);
                p.pollUndoCoalesce();
                checkStr (p.getPresets().baseline(), anamorph::PresetManager::soundSignatureFor (p.getAPVTS()),
                          ("factory preset '" + entries.getReference (i).name + "' is exactly clean after loading").toRawUTF8());
            }
        check (factories >= 5, "non-vacuity: the factory table was walked");
    }

    // --- (c) the load-side equality, MEASURED, against the save-side one --------
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        auto& apvts = p.getAPVTS();
        std::vector<juce::RangedAudioParameter*> ranged;
        for (auto* q : p.getParameters())
            if (auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*> (q))
                if (! pid::isPresetExcluded (wid->paramID))
                    if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (q)) ranged.push_back (rp);

        auto probe = juce::File::createTempFile (anamorph::PresetManager::fileSuffix());
        juce::uint32 lcg = 0x5EED1234u;
        int afterLoadingMismatch = 0, savedTreeMismatch = 0, dirtyAfterLoad = 0, loadsFailed = 0;
        double worstFieldDelta = 0.0;
        const int points = 3000;
        for (int i = 0; i < points; ++i)
        {
            lcg = lcg * 1664525u + 1013904223u;
            const float v = (float) ((lcg >> 8) * (1.0 / 16777216.0));
            for (auto* rp : ranged) rp->setValueNotifyingHost (v);
            const auto tree = apvts.copyState();
            const auto sigAfter = anamorph::PresetManager::soundSignatureAfterLoading (apvts, tree);
            const auto sigSaved = anamorph::PresetManager::soundSignatureForSavedTree (apvts, tree);
            auto xml = tree.createXml();
            if (xml == nullptr || ! probe.replaceWithText (xml->toString()) || ! p.getPresets().loadFile (probe)) { ++loadsFailed; continue; }
            const auto live = anamorph::PresetManager::soundSignatureFor (apvts);
            if (live != sigAfter) ++afterLoadingMismatch;
            if (live != sigSaved) ++savedTreeMismatch;
            if (p.getPresets().isDirty()) ++dirtyAfterLoad;
            // Diagnostic, not an assertion: if the exact check below ever fails on some
            // toolchain, the SIZE of the disagreement is what tells a triager whether it is
            // a last-bit difference in the arithmetic or a real value taking the wrong path.
            // (It cannot fire while the strings are identical, which is why it is not a check.)
            auto a = juce::StringArray::fromTokens (sigAfter, ",", ""), b = juce::StringArray::fromTokens (live, ",", "");
            if (a.size() != b.size()) worstFieldDelta = 1.0e30;
            for (int k = 0; k < juce::jmin (a.size(), b.size()); ++k)
                worstFieldDelta = juce::jmax (worstFieldDelta,
                                              std::abs ((double) a[k].getFloatValue() - (double) b[k].getFloatValue()));
        }
        probe.deleteFile();
        check (loadsFailed == 0, "every probe preset loaded back");
        // THE PRODUCT INVARIANT: a just-loaded preset reads clean. Since round 11 the baseline
        // is a pure prediction from the tree, so this is no longer true by construction the way
        // it was while the load reconciled its prediction against a live read-back -- it holds
        // exactly when the prediction and the post-load live signature agree, which is the
        // assertion below. That is the point: the two must agree, and if a toolchain ever makes
        // them disagree it is reporting a real defect on that toolchain, to be fixed by making
        // the two sides agree rather than by widening the comparison (which is what round 10
        // did, and what let an automation write hide in the window).
        check (dirtyAfterLoad == 0, "a just-loaded preset never reads modified, across the whole sweep");
        // EXACT, and asserted as such (D-2 round 11). Round 10 weakened this to "within float
        // tail" and merely printed the string count, believing the equality toolchain-dependent
        // because of an FMA-contraction mechanism that could not have been operating in the
        // build where it failed (ADR-0031 compiles it with -ffp-contract=off, 0 FMA emitted).
        // Re-measured alone in both the Release and the ThreadSanitizer build: zero.
        check (afterLoadingMismatch == 0,
               "the load-side signature equals the post-load live signature at EVERY swept value");
        check (savedTreeMismatch >= 1,
               "the save-side signature measurably differs from the post-load live signature, so a"
               " separate load-side flavour is needed");
        std::printf ("  load-side sweep over %d round trips: dirty after load %d, load-side signature mismatches %d"
                     " (worst field delta %.3g), save-side signature mismatches %d (the reason the two flavours exist)\n",
                     points, dirtyAfterLoad, afterLoadingMismatch, worstFieldDelta, savedTreeMismatch);
    }
}

// ---------------------------------------------------------------------------
//  State test 56 -- the preset prev/next step derives its target from the adopted
//  selection (D-2 round 10; found by the round's own entry-point audit, the fourth
//  instance of the rule §18 records).
//
//  step() computed "the row after this one" from currentIndex() and then called
//  load(), whose drain adopts a pending restore -- after the index was chosen. A
//  restore that moved the selection therefore had the user's Next land on the row
//  after the OLD selection, not after the one the plug-in was actually on. Same
//  shape as the A/B toggle; same fix: adopt, then derive.
// ---------------------------------------------------------------------------
static void testPresetStepDerivesItsTargetAfterTheDrain()
{
    std::printf ("State test 56: a preset step derives its target from the adopted selection (D-2 r10)\n");

    // Two sessions on two different FACTORY rows, far enough apart that "next from the
    // old row" and "next from the restored row" cannot coincide.
    auto sessionOnFactoryRow = [] (int row)
    {
        AnamorphAudioProcessor a;
        a.prepareToPlay (48000.0, 512);
        a.getPresets().load (row);
        a.pollUndoCoalesce();
        return d2::saveOf (a);
    };
    AnamorphAudioProcessor probe;
    const int factories = [&] { int n = 0; for (const auto& e : probe.getPresets().entries()) if (e.isFactory) ++n; return n; }();
    check (factories >= 5, "non-vacuity: enough factory rows to step among");
    const int rowP = 1, rowR = 4;
    const auto blobP = sessionOnFactoryRow (rowP);
    const auto blobR = sessionOnFactoryRow (rowR);

    for (int delta = +1; delta >= -1; delta -= 2)
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        d2::restoreFrom (p, blobP);
        check (p.getPresets().currentIndex() == rowP, "the outgoing session's row is selected");

        d2::offMessageThread ([&] { d2::restoreFrom (p, blobR); });   // pending: moves the selection at adoption
        check (p.getPresets().currentIndex() == rowP,
               "non-vacuity: the restore is pending -- the selection has not moved yet");

        p.getPresets().step (delta);                                    // ONE user action
        p.pollUndoCoalesce();

        const int n = p.getPresets().entries().size();
        const int expected = ((rowR + delta) % n + n) % n;
        check (p.getPresets().currentIndex() == expected,
               delta > 0 ? "Next lands on the row after the RESTORED selection, not after the outgoing one"
                         : "Prev lands on the row before the RESTORED selection, not before the outgoing one");
        // The sound is that row's: load the same factory row on a fresh instance and compare.
        AnamorphAudioProcessor control;
        control.prepareToPlay (48000.0, 512);
        control.getPresets().load (expected);
        checkStr (anamorph::PresetManager::soundSignatureFor (p.getAPVTS()),
                  anamorph::PresetManager::soundSignatureFor (control.getAPVTS()),
                  "...and the live sound is that row's factory sound");
        check (! p.getPresets().isDirty(), "...read clean");
    }
}

// ---------------------------------------------------------------------------
//  State test 57 -- no tolerance can absorb an automation write into a preset's
//  clean baseline (D-2 round 11, review finding "tiny automation changes hide
//  preset edits").
//
//  Round 10's load path compared its predicted baseline against a live read-back
//  and took the LIVE value where the two agreed to within 1e-6. A tolerance cannot
//  tell compiler noise from a real automation write of the same size, so a write
//  landing in the load window was ABSORBED INTO THE BASELINE: the preset then read
//  clean against a sound its file does not hold. The tolerance is gone (section 19)
//  and the signature has exactly one equivalence -- rendered normalised values equal
//  to five decimal places -- applied identically on every path.
//
//  THE DISCRIMINATOR IS A ROUNDING BOUNDARY, and it has to be: a tolerance smaller
//  than the signature's own resolution can only change the answer where the two
//  values straddle a boundary of that resolution. The test therefore SEARCHES a
//  deterministic grid for a value whose rendered form sits just under a 5-decimal
//  boundary, together with a nudge under 1e-6 that crosses it, and asserts it found
//  one before using it.
//
//  WHERE IT SEARCHES IS PART OF THE TEST. An unconstrained search from zero always
//  terminates in its first few steps, on the 5e-6 rounding tie at the very bottom of
//  the range -- one numeric neighbourhood, dressed up as four legs. Each leg is
//  therefore confined to its own BAND of the normalised range and asserts that its
//  witness came from that band, so the four legs are four different neighbourhoods
//  spanning the range. The two parameters are the plug-in's two different log
//  mappings: `monoMakerFreq` is a CENTRED log range (20..500 with 120 at the middle)
//  and `mbFreqLow` a plain one (20..20000).
// ---------------------------------------------------------------------------
namespace r11
{
    struct Boundary { bool found = false; float base = 0.0f, nudged = 0.0f, delta = 0.0f; };

    // The signature's own quantum: juce::String(v, 5) resolves 1e-5 of normalised range.
    inline constexpr float kSignatureQuantum = 1.0e-5f;

    static juce::String rendered5 (const juce::RangedAudioParameter& rp, float normalised)
    {
        return juce::String (normalisedAsRendered (rp, normalisedAsRendered (rp, normalised)), 5);
    }

    // A normalised value INSIDE [lo, hi) whose RENDERED form is within `maxDelta` of a
    // 5-decimal rounding boundary, and the nudge that crosses it. Deterministic: a fixed
    // grid, no RNG, no clock, bounded by construction.
    static Boundary findBoundary (juce::RangedAudioParameter& rp, float maxDelta, int sign,
                                  float lo, float hi)
    {
        const int steps = 200000;
        for (int i = 0; i < steps; ++i)
        {
            const float base = lo + (hi - lo) * ((float) i / (float) steps);
            const float r    = normalisedAsRendered (rp, normalisedAsRendered (rp, base));
            for (float d = 1.0e-7f; d <= maxDelta; d *= 2.0f)
            {
                const float nudged = base + (float) sign * d;
                if (nudged <= 0.0f || nudged >= 1.0f) continue;
                const float rn = normalisedAsRendered (rp, normalisedAsRendered (rp, nudged));
                if (std::abs (rn - r) <= maxDelta && juce::String (rn, 5) != juce::String (r, 5))
                    return { true, base, nudged, std::abs (rn - r) };
            }
        }
        return {};
    }

    // The smallest distinct step this parameter's rendering can take, measured rather than
    // read off the range: `RangedAudioParameter::convertFrom0to1` snaps through
    // `snapToLegalValue`, which prefers a snapToLegalValueFunction and IGNORES `interval`
    // when one is set -- so `interval > 0` is not the question, and the four log-mapped
    // frequency ranges (which pass jlimit as that function) have no grid despite the field.
    // The grid cell AROUND a given on-grid value: the offset at which the rendering first
    // changes (half a cell) found by bisection, and the full cell that is twice it. A skewed
    // range's cell width varies along the range, so a leg that wants "one whole step HERE"
    // has to measure it here rather than reuse the parameter's smallest step, which for
    // chorusRate lives at the opposite end of the range.
    // It also returns the PAIR that straddles the boundary -- the largest value that still
    // renders as `base` and the smallest that does not -- so the boundary leg can assert on
    // two values the search itself proved land on opposite sides. Probing `boundary +/- d`
    // for a guessed d instead makes the answer depend on where the platform's float
    // arithmetic puts the crossing: measured, that probe finds the pair on native x86-64 and
    // on arm64 and finds nothing under Rosetta, on identical `base` and cell width.
    struct Cell { bool found = false; float half = 0.0f, step = 0.0f, below = 0.0f, above = 0.0f; };

    static Cell cellAround (const juce::RangedAudioParameter& rp, float base)
    {
        const float r0 = normalisedAsRendered (rp, base);
        float hi = 1.0e-7f;
        while (hi < 0.25f && juce::exactlyEqual (normalisedAsRendered (rp, base + hi), r0)) hi *= 2.0f;
        if (juce::exactlyEqual (normalisedAsRendered (rp, base + hi), r0)) return {};
        float lo = 0.0f;
        for (int i = 0; i < 80; ++i)
        {
            const float mid = (lo + hi) * 0.5f;
            if (juce::exactlyEqual (mid, lo) || juce::exactlyEqual (mid, hi)) break;
            if (juce::exactlyEqual (normalisedAsRendered (rp, base + mid), r0)) lo = mid; else hi = mid;
        }
        // The invariant every branch above maintains: base+lo renders as base, base+hi does not.
        return { true, hi, hi * 2.0f, base + lo, base + hi };
    }

    // Returns 0 for a parameter with no grid: (almost) every sample renders differently.
    static double smallestRenderedStep (const juce::RangedAudioParameter& rp)
    {
        const int N = 400001;
        double minGap = 1.0e30;
        int distinct = 1;
        float prev = normalisedAsRendered (rp, 0.0f);
        for (int i = 1; i < N; ++i)
        {
            const float r = normalisedAsRendered (rp, (float) ((double) i / (double) (N - 1)));
            if (! juce::exactlyEqual (r, prev))
            {
                if (const double gap = (double) r - (double) prev; gap > 0.0) minGap = juce::jmin (minGap, gap);
                prev = r;
                ++distinct;
            }
        }
        return distinct > N / 2 ? 0.0 : minGap;   // a new value at (almost) every sample: no grid
    }
}

static void testNoToleranceAbsorbsAnAutomationWrite()
{
    std::printf ("State test 57: no tolerance absorbs an automation write into a preset baseline (D-2 r11)\n");

    const juce::String name ("AnamorphHarness-D2R11");
    auto presetFile = anamorph::PresetManager::presetDirectory()
                          .getChildFile (name + anamorph::PresetManager::fileSuffix());

    // The harness writes into the REAL user preset folder, so a developer's own preset of
    // this name is moved aside and put back. The park path is a UNIQUE temp file, not a
    // fixed one: a fixed shared path is what produced round 10's misdiagnosis, and this
    // round is not adding another. (The folder itself is still shared -- docs/procedures/
    // TESTING.md's one-instance-at-a-time rule is what covers that, and it still applies.)
    struct ParkedPreset
    {
        explicit ParkedPreset (juce::File f)
            : live (std::move (f)),
              parked (juce::File::createTempFile (".d2r11parked")),
              had (live.existsAsFile())
        {
            if (! had) { ok = true; return; }
            parked.deleteFile();
            ok = live.moveFileTo (parked);      // could NOT park it: touch nothing at all
        }
        ~ParkedPreset()
        {
            if (! ok) return;                   // the user's own preset is still where it was
            live.deleteFile();
            if (had) { parked.moveFileTo (live); parked.deleteFile(); }
        }
        juce::File live, parked; bool had = false, ok = false;
    };
    const ParkedPreset guard { presetFile };
    check (guard.ok, "the harness could park any same-named user preset before writing one");
    if (! guard.ok) return;

    // --- THE TWO PARAMETER DOMAINS, measured rather than assumed --------------------
    // A parameter whose rendering lands on a GRID absorbs movement inside one grid cell --
    // and that absorption is section 17's deliberate rule, not a tolerance: within a cell
    // nothing the DSP reads or the file holds changes, and crossing a cell boundary moves a
    // whole step, which is far larger than the signature's own 1e-5 quantum. A parameter
    // with NO grid keeps full float resolution, and there the signature's quantum is the
    // only thing between two values.
    //
    // The margin that makes the first half safe is asserted here rather than quoted: the
    // smallest step any grid parameter can take must stay larger than the quantum, or two
    // adjacent legal settings would print the same signature and one full step of a real
    // control would read clean.
    {
        AnamorphAudioProcessor p;
        double smallest = 1.0e30; juce::String smallestId; int grids = 0, continuous = 0;
        for (auto* q : p.getParameters())
        {
            auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*> (q);
            auto* rp  = dynamic_cast<juce::RangedAudioParameter*> (q);
            if (wid == nullptr || rp == nullptr || pid::isPresetExcluded (wid->paramID)) continue;
            const double step = r11::smallestRenderedStep (*rp);
            if (step <= 0.0) { ++continuous; continue; }
            ++grids;
            if (step < smallest) { smallest = step; smallestId = wid->paramID; }
        }
        check (grids > 20 && continuous == 4,
               "non-vacuity: the parameter set splits into a grid domain and exactly four gridless ranges");
        check (smallest > (double) r11::kSignatureQuantum,
               "every grid parameter's smallest step is larger than the signature's 1e-5 quantum");
        std::printf ("  parameter grid: %d grid + %d gridless; smallest step %.4e on %s (%.2f quanta)\n",
                     grids, continuous, smallest, smallestId.toRawUTF8(),
                     smallest / (double) r11::kSignatureQuantum);
    }

    // --- the gridless domain: nothing absorbs a sub-1e-6 write, in four bands --------
    struct Leg { const char* id; int sign; float lo, hi; };
    const Leg legs[] = { { "monoMakerFreq", +1, 0.10f, 0.25f },
                         { "monoMakerFreq", -1, 0.35f, 0.50f },
                         { "mbFreqLow",     +1, 0.60f, 0.75f },
                         { "mbFreqLow",     -1, 0.85f, 0.98f } };
    for (const auto& leg : legs)
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p.getAPVTS().getParameter (leg.id));
        check (rp != nullptr, "the parameter under test is ranged");
        if (rp == nullptr) continue;

        check (juce::exactlyEqual (r11::smallestRenderedStep (*rp), 0.0),
               "non-vacuity: the parameter under test really has no grid to absorb the write");

        const auto b = r11::findBoundary (*rp, 1.0e-6f, leg.sign, leg.lo, leg.hi);
        check (b.found, (juce::String ("non-vacuity: a sub-1e-6 nudge crossing a signature boundary exists for ")
                             + leg.id + (leg.sign > 0 ? " (+)" : " (-)")).toRawUTF8());
        if (! b.found) continue;
        check (b.base >= leg.lo && b.base < leg.hi,
               "...and it was found in this leg's own band, not at the range floor");

        // 1. The preset is written at `base`, and loaded ONCE UNDISTURBED so the test holds
        //    an oracle for the baseline that does not come from the function under test.
        rp->setValueNotifyingHost (b.base);
        check (p.getPresets().saveUser (name), "the preset is written at the base value");
        rp->setValueNotifyingHost (0.5f);                       // leave the preset's sound
        check (p.getPresets().loadFile (presetFile), "the preset loads undisturbed");
        p.pollUndoCoalesce();
        check (! p.getPresets().isDirty(), "an undisturbed load reads clean");
        const auto oracle = anamorph::PresetManager::soundSignatureFor (p.getAPVTS());

        // 2. The same load, with an automation write landing INSIDE the load window.
        rp->setValueNotifyingHost (0.5f);
        int fires = 0;
        p.getPresets().beforeStateCapture = [&] { ++fires; rp->setValueNotifyingHost (b.nudged); };
        check (p.getPresets().loadFile (presetFile), "the preset loads");
        p.getPresets().beforeStateCapture = nullptr;
        p.pollUndoCoalesce();
        check (fires == 1, "the seam fired once, after the apply and before the baseline");

        // The write really stands, and it really is smaller than round 10's tolerance --
        // measured on the LIVE value, not on the pair the search constructed.
        check (! juce::exactlyEqual (rp->getValue(), b.base), "the automation write stands");
        check (std::abs (rp->getValue() - b.base) <= 1.0e-6f,
               "non-vacuity: and it is inside the tolerance round 10 applied");
        check (r11::rendered5 (*rp, rp->getValue()) != r11::rendered5 (*rp, b.base),
               "non-vacuity: the write crosses a signature boundary");

        // THE INVARIANT: the baseline describes the FILE -- it equals the signature that
        // same file produces when nothing interferes -- and the automated sound is DIRTY.
        checkStr (p.getPresets().baseline(), oracle,
                  "the baseline is the signature of the sound the FILE restores, undisturbed");
        check (p.getPresets().isDirty(),
               "a sub-1e-6 automation write inside the load window leaves the preset DIRTY, not absorbed");

        // ...and reloading it, undisturbed, is clean again at the file's own value.
        check (p.getPresets().loadFile (presetFile), "the preset reloads");
        p.pollUndoCoalesce();
        check (! p.getPresets().isDirty(), "a reload reads clean");
        checkNear ((double) rp->getValue(), (double) normalisedAsRendered (*rp, b.base), 1.0e-7,
                   "...at the file's own value");
        std::printf ("  %-14s %s band [%.2f,%.2f): base %.9f nudged %.9f (delta %.3g) -> dirty\n",
                     leg.id, leg.sign > 0 ? "+" : "-", leg.lo, leg.hi, b.base, b.nudged, b.delta);
    }

    // --- the same tiny write through the OTHER loader, load(index) ------------------
    // The two loaders put the seam on opposite sides of nothing now -- both build the
    // baseline from the tree and read no live value -- but only loadFile was covered
    // above, and it is the ordering, not the arithmetic, that a future edit could break.
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p.getAPVTS().getParameter ("mbFreqLow"));
        check (rp != nullptr, "the parameter under test is ranged");
        if (rp != nullptr)
        {
            const auto b = r11::findBoundary (*rp, 1.0e-6f, +1, 0.30f, 0.45f);
            check (b.found, "non-vacuity: a boundary witness exists for the load(index) leg");
            if (b.found)
            {
                rp->setValueNotifyingHost (b.base);
                check (p.getPresets().saveUser (name), "the preset is written");
                const int idx = p.getPresets().currentIndex();
                check (idx >= 0, "the freshly saved preset is the selected list entry");
                if (idx >= 0)
                {
                    rp->setValueNotifyingHost (0.5f);
                    int fires = 0;
                    p.getPresets().beforeStateCapture = [&] { ++fires; rp->setValueNotifyingHost (b.nudged); };
                    p.getPresets().load (idx);
                    p.getPresets().beforeStateCapture = nullptr;
                    p.pollUndoCoalesce();
                    check (fires == 1, "the seam fired once on the menu-load path too");
                    check (p.getPresets().isDirty(),
                           "the menu-load path is dirty after the same sub-1e-6 write");
                }
            }
        }
    }

    // --- the grid domain: absorbed INSIDE a cell, and a whole step across a boundary ---
    // Both halves matter. A sub-step move inside a cell is correctly not an edit; a
    // sub-1e-6 move that happens to straddle a snap boundary moves a WHOLE step, changes
    // what the DSP reads and what the file holds, and is correctly an edit. "A tiny write
    // cannot reach the signature on a grid parameter" would be false, so it is not claimed.
    for (const char* id : { "amount", "chorusRate" })
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p.getAPVTS().getParameter (id));
        check (rp != nullptr, "the grid parameter under test is ranged");
        if (rp == nullptr) continue;
        const double step = r11::smallestRenderedStep (*rp);
        check (step > 0.0, "non-vacuity: the parameter under test has a grid");
        if (! (step > 0.0)) continue;

        // A base that is exactly ON the grid, found rather than assumed: the rendering of a
        // grid point is that point.
        float base = 0.0f; bool onGrid = false;
        for (int i = 0; i < 20000 && ! onGrid; ++i)
        {
            const float c = 0.40f + (float) i * 1.0e-5f;
            if (juce::exactlyEqual (normalisedAsRendered (*rp, c), c)) { base = c; onGrid = true; }
        }
        check (onGrid, "non-vacuity: a value that is exactly on the parameter's grid was found");
        if (! onGrid) continue;

        const auto cell = r11::cellAround (*rp, base);
        check (cell.found, "non-vacuity: the grid cell around the chosen value was measured");
        if (! cell.found) continue;

        rp->setValueNotifyingHost (base);
        check (p.getPresets().saveUser (name), "the preset is written");
        check (! p.getPresets().isDirty(), "freshly saved reads clean");

        rp->setValueNotifyingHost (base + cell.step * 0.2f);             // a fifth of THIS cell
        check (! p.getPresets().isDirty(),
               "movement inside one grid cell is not an edit (section 17) -- absorbed by the SNAP, not a tolerance");
        rp->setValueNotifyingHost (base + cell.step);                    // one whole cell
        check (p.getPresets().isDirty(), "...and one whole step is a real edit");
        rp->setValueNotifyingHost (base);
        check (! p.getPresets().isDirty(), "...and returning to the saved value reads clean again");

        // The snap boundary: a move far below 1e-6 that crosses it moves a whole step, so a
        // tiny write CAN reach the signature on a grid parameter -- correctly, because at a
        // boundary it really does move the DSP input and the file by a full step. The two
        // values come from the bisection itself, which established that they render
        // differently, so the leg does not depend on where a guessed epsilon lands.
        const float below = cell.below, above = cell.above;
        const double crossing = std::abs ((double) above - (double) below);
        check (! juce::exactlyEqual (normalisedAsRendered (*rp, below), normalisedAsRendered (*rp, above)),
               "non-vacuity: the search's own pair renders on opposite sides of a snap boundary");
        check (crossing > 0.0 && crossing <= 1.0e-6,
               "non-vacuity: ...and the move between them is real and smaller than 1e-6");
        rp->setValueNotifyingHost (below);
        check (p.getPresets().saveUser (name), "the preset is written at the boundary");
        check (! p.getPresets().isDirty(), "...and reads clean there");
        rp->setValueNotifyingHost (above);
        check (p.getPresets().isDirty(),
               "a sub-1e-6 write ACROSS a snap boundary is a whole step, and reads dirty");
        std::printf ("  %-11s (grid): smallest step %.6g (%.1f quanta), cell here %.6g --"
                     " inside a cell is not an edit, a step is, and a %.1e write across a boundary is\n",
                     id, step, step / (double) r11::kSignatureQuantum, (double) cell.step, crossing);
    }

    // --- accumulation: many sub-resolution nudges must still become visible -----
    // Nothing absorbs them, so once the accumulated move crosses the signature's own
    // resolution the preset reads dirty and stays dirty. The base is chosen to sit in the
    // MIDDLE of its signature bucket, so the first nudge cannot cross on its own -- without
    // that, a leg labelled "accumulate" can pass having measured a single crossing.
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p.getAPVTS().getParameter ("monoMakerFreq"));
        check (rp != nullptr, "the accumulation parameter is ranged");
        if (rp != nullptr)
        {
            const float nudge = 1.0e-7f;
            float base = 0.0f; bool midBucket = false;
            for (int i = 0; i < 20000 && ! midBucket; ++i)
            {
                const float c = 0.50f + (float) i * 1.0e-6f;
                // mid-bucket: neither one nudge up nor one nudge down changes the signature
                if (r11::rendered5 (*rp, c + nudge) == r11::rendered5 (*rp, c)
                    && r11::rendered5 (*rp, c - nudge) == r11::rendered5 (*rp, c)) { base = c; midBucket = true; }
            }
            check (midBucket, "non-vacuity: a base in the middle of its signature bucket was found");
            if (midBucket)
            {
                rp->setValueNotifyingHost (base);
                check (p.getPresets().saveUser (name), "the preset is written");
                check (! p.getPresets().isDirty(), "freshly saved reads clean");
                float v = rp->getValue();
                v += nudge; rp->setValueNotifyingHost (v);
                check (! p.getPresets().isDirty(),
                       "one sub-resolution nudge alone does not move the signature (so the leg is about accumulation)");
                int steps = 1;
                bool wentDirty = false;
                for (int i = 0; i < 400 && ! wentDirty; ++i)
                {
                    v += nudge;                     // each step far below the signature's resolution
                    rp->setValueNotifyingHost (v);
                    ++steps;
                    wentDirty = p.getPresets().isDirty();
                }
                check (wentDirty, "repeated sub-resolution nudges accumulate into a visible edit rather than hiding");
                check (steps > 1, "...and it took more than one of them, which is what accumulation means");
                for (int i = 0; i < 50; ++i) { v += nudge; rp->setValueNotifyingHost (v); }
                check (p.getPresets().isDirty(), "...and it stays dirty as they continue");
                std::printf ("  accumulation: %d nudges of %.1e crossed the 1e-5 quantum\n", steps, (double) nudge);
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  State test 58 -- a restore is a FIXED POINT, and it restores what the session
//  stored (D-2 round 11, ADR-0036 section 19).
//
//  reassertParameters gated its per-parameter write on |written - live| <= 1e-6 --
//  the same shape as the baseline tolerance round 11 deleted, on the same coherence
//  path. What it declined to write is the value the SESSION stored, while the
//  baseline travelling with that session is adopted verbatim, so the combination
//  that the gate protects is "live value from before, baseline from the file" --
//  the one that makes an untouched preset show the modified star after an A/B
//  toggle, an undo, or a project reopen.
//
//  The gate is exact now. That is only safe if a restore is a FIXED POINT: a host
//  may apply one chunk any number of times, and a per-application float-tail nudge
//  would walk the sound. Both halves are asserted here, because the fixed point is
//  what the tolerance was silently buying.
// ---------------------------------------------------------------------------
static void testARestoreIsAFixedPoint()
{
    std::printf ("State test 58: a restore is a fixed point and restores what was stored (D-2 r11)\n");

    // --- (a) one chunk, applied many times, moves nothing ---------------------
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        auto& apvts = p.getAPVTS();
        std::vector<juce::RangedAudioParameter*> ranged;
        for (auto* q : p.getParameters())
            if (auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*> (q))
                if (! pid::isPresetExcluded (wid->paramID))
                    if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (q)) ranged.push_back (rp);
        check (ranged.size() > 20, "non-vacuity: the sweep covers the parameter set");

        juce::uint32 lcg = 0x0BADC0DEu;
        int notFixedPoint = 0, sigMoved = 0, blobMoved = 0;
        double worstDelta = 0.0;
        const int points = 300, reapplications = 10;
        for (int i = 0; i < points; ++i)
        {
            lcg = lcg * 1664525u + 1013904223u;
            const float v = (float) ((lcg >> 8) * (1.0 / 16777216.0));
            for (auto* rp : ranged) rp->setValueNotifyingHost (v);

            juce::MemoryBlock blob;
            p.getStateInformation (blob);
            std::vector<float> before;
            for (auto* rp : ranged) before.push_back (rp->getValue());
            const auto sig0 = anamorph::PresetManager::soundSignatureFor (apvts);

            for (int k = 0; k < reapplications; ++k)
            {
                p.setStateInformation (blob.getData(), (int) blob.getSize());
                p.pollUndoCoalesce();
            }

            bool moved = false;
            for (size_t k = 0; k < ranged.size(); ++k)
                if (const double d = std::abs ((double) ranged[k]->getValue() - (double) before[k]); d > 0.0)
                    { moved = true; worstDelta = juce::jmax (worstDelta, d); }
            if (moved) ++notFixedPoint;
            if (anamorph::PresetManager::soundSignatureFor (apvts) != sig0) ++sigMoved;

            juce::MemoryBlock again;
            p.getStateInformation (again);
            if (again != blob) ++blobMoved;
        }
        check (notFixedPoint == 0, "re-applying one host chunk moves no parameter, however many times");
        check (sigMoved == 0, "...and moves no signature");
        check (blobMoved == 0, "...and the state it writes back out is byte-identical to the chunk it read");
        std::printf ("  %d sounds x %d re-applications: values moved %d (worst %.3e), signatures %d, blobs %d\n",
                     points, reapplications, notFixedPoint, worstDelta, sigMoved, blobMoved);
    }

    // --- (b) a project saved clean reopens clean, on a fresh instance ----------
    // The user-visible half: the preset marker a project carries must survive the
    // round trip, in a NEW processor, which is what reopening a session is.
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        auto& apvts = p.getAPVTS();
        std::vector<juce::RangedAudioParameter*> ranged;
        for (auto* q : p.getParameters())
            if (auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*> (q))
                if (! pid::isPresetExcluded (wid->paramID))
                    if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (q)) ranged.push_back (rp);

        auto probe = juce::File::createTempFile (anamorph::PresetManager::fileSuffix());
        juce::uint32 lcg = 0x1234ABCDu;
        int dirtyBeforeSave = 0, dirtyAfterReopen = 0, sigMoved = 0, loadsFailed = 0;
        const int points = 300;
        for (int i = 0; i < points; ++i)
        {
            lcg = lcg * 1664525u + 1013904223u;
            const float v = (float) ((lcg >> 8) * (1.0 / 16777216.0));
            for (auto* rp : ranged) rp->setValueNotifyingHost (v);

            // Make the project CLEAN against a preset holding exactly this sound.
            auto xml = apvts.copyState().createXml();
            if (xml == nullptr || ! probe.replaceWithText (xml->toString())
                || ! p.getPresets().loadFile (probe)) { ++loadsFailed; continue; }
            p.pollUndoCoalesce();
            if (p.getPresets().isDirty()) ++dirtyBeforeSave;
            const auto sigBefore = anamorph::PresetManager::soundSignatureFor (apvts);

            juce::MemoryBlock blob;
            p.getStateInformation (blob);

            AnamorphAudioProcessor q;
            q.prepareToPlay (48000.0, 512);
            q.setStateInformation (blob.getData(), (int) blob.getSize());
            q.pollUndoCoalesce();
            if (q.getPresets().isDirty()) ++dirtyAfterReopen;
            if (anamorph::PresetManager::soundSignatureFor (q.getAPVTS()) != sigBefore) ++sigMoved;
        }
        probe.deleteFile();
        check (loadsFailed == 0, "every probe project was written and loaded");
        check (dirtyBeforeSave == 0, "non-vacuity: the project was clean before it was saved");
        check (dirtyAfterReopen == 0, "a project saved with a clean preset marker reopens clean");
        check (sigMoved == 0, "...and reopens at the same sound");
        std::printf ("  %d project save/reopen round trips: dirty on reopen %d, sound moved %d\n",
                     points, dirtyAfterReopen, sigMoved);
    }
}

namespace r12
{
    // The preset name a saved blob carries, read straight out of the bytes.
    static juce::String presetNameOf (const juce::MemoryBlock& blob)
    {
        if (auto xml = BlobCodec::unwrap (blob))
            return juce::ValueTree::fromXml (*xml).getProperty ("presetName").toString();
        return "<unreadable>";
    }
}

// ---------------------------------------------------------------------------
//  State test 59 -- a host save taken inside the pending-restore window carries
//  the Settings edits made after that restore arrived (D-2 round 12, review
//  finding "pending Settings edits vanish from saves").
//
//  PLACEHOLDER HEADER -- rewritten once the mechanism is proved.
// ---------------------------------------------------------------------------
static void testHostSaveInsideThePendingWindowCarriesTheEdit()
{
    std::printf ("State test 59: a host save inside the pending window carries the edit (D-2 r12)\n");

    const r3::SettingsSet P  { 1, 3, 0.50, true,  true,  false };
    const r3::SettingsSet R  { 2, 4, 0.25, true,  true,  false };
    const r3::SettingsSet U  { 3, 5, 0.75, false, false, true  };
    const auto blobR = r3::authorSession ("D2-R12-R", R);

    // The intended logical state, built from the RULE (section 9) rather than from any
    // production merge: the restore's Settings, with the fields edited after its
    // arrival taking the edit's value.
    auto expectedWith = [&] (std::initializer_list<int> edited)
    {
        r3::SettingsSet e = R;
        for (int i : edited)
            switch (i)
            {
                case 0: e.oversample   = U.oversample;   break;
                case 1: e.uiScale      = U.uiScale;      break;
                case 2: e.scopePersist = U.scopePersist; break;
                case 3: e.metersOn     = U.metersOn;     break;
                case 4: e.tooltipsOn   = U.tooltipsOn;   break;
                default: e.uiAnimations = U.uiAnimations; break;
            }
        return e;
    };

    // --- (a) THE REPORTED CASE: restore pending, one Settings edit, host save ----
    for (int i = 0; i < 6; ++i)
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        r3::apply (p.getInternal(), P);
        d2::offMessageThread ([&] { d2::restoreFrom (p, blobR); });   // arrived; PENDING
        r3::setField (p.getInternal(), i, U);                          // the edit, after the arrival

        juce::MemoryBlock hostSave;
        d2::offMessageThread ([&] { hostSave = d2::saveOf (p); });     // the host saves INSIDE the window

        check (r3::same (r3::settingsOf (hostSave), expectedWith ({ i })),
               (juce::String (r3::fieldNames[i]) + ": a host save inside the pending window carries the edit").toRawUTF8());
        check (r12::presetNameOf (hostSave) == "D2-R12-R",
               "...and still describes the RESTORE's program, not the outgoing session's");
    }

    // --- (b) several edits in one pending window ------------------------------
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        r3::apply (p.getInternal(), P);
        d2::offMessageThread ([&] { d2::restoreFrom (p, blobR); });
        r3::apply (p.getInternal(), U);                                // all six, after the arrival
        juce::MemoryBlock hostSave;
        d2::offMessageThread ([&] { hostSave = d2::saveOf (p); });
        check (r3::same (r3::settingsOf (hostSave), U), "six edits in one pending window all reach a host save");
    }

    // --- (c) the edit was made BEFORE the restore arrived ----------------------
    // The restore is the newer arrival, so the save must carry the RESTORE's values:
    // the fix must not let a stale edit override a genuinely newer restore.
    for (int i = 0; i < 6; ++i)
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        r3::apply (p.getInternal(), P);
        r3::setField (p.getInternal(), i, U);                          // the edit, BEFORE the arrival
        d2::offMessageThread ([&] { d2::restoreFrom (p, blobR); });
        juce::MemoryBlock hostSave;
        d2::offMessageThread ([&] { hostSave = d2::saveOf (p); });
        check (r3::same (r3::settingsOf (hostSave), R),
               (juce::String (r3::fieldNames[i]) + ": an edit made before the arrival is replaced by the restore, in the save too").toRawUTF8());
    }

    // --- (d) the same save, taken after the adoption --------------------------
    for (int i = 0; i < 6; ++i)
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        r3::apply (p.getInternal(), P);
        d2::offMessageThread ([&] { d2::restoreFrom (p, blobR); });
        r3::setField (p.getInternal(), i, U);
        p.pollUndoCoalesce();                                          // the adoption
        juce::MemoryBlock hostSave, ownerSave = d2::saveOf (p);
        d2::offMessageThread ([&] { hostSave = d2::saveOf (p); });
        check (r3::same (r3::settingsOf (hostSave), expectedWith ({ i })),
               "a host save after the adoption carries the same answer");
        check (hostSave == ownerSave, "...and equals the owner's own save");
    }

    // --- (e) an edit made after the adoption ----------------------------------
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        r3::apply (p.getInternal(), P);
        d2::offMessageThread ([&] { d2::restoreFrom (p, blobR); });
        p.pollUndoCoalesce();                                          // adopt first
        r3::apply (p.getInternal(), U);                                // then edit
        juce::MemoryBlock hostSave;
        d2::offMessageThread ([&] { hostSave = d2::saveOf (p); });
        check (r3::same (r3::settingsOf (hostSave), U), "an edit made after the adoption reaches a host save");
    }

    // --- (f) two restores, the edit after BOTH -------------------------------
    // The edit follows the latest arrival, so it survives both adoptions and must be in
    // the save. This is the leg that makes the fix key on the RECORDED per-field
    // generation rather than on "was there an edit at all".
    {
        const r3::SettingsSet R2 { 4, 2, 0.90, false, true, false };
        const auto blobR2 = r3::authorSession ("D2-R12-R2", R2);
        for (int i = 0; i < 6; ++i)
        {
            AnamorphAudioProcessor p;
            p.prepareToPlay (48000.0, 512);
            r3::apply (p.getInternal(), P);
            d2::offMessageThread ([&] { d2::restoreFrom (p, blobR); });    // R1 arrives
            d2::offMessageThread ([&] { d2::restoreFrom (p, blobR2); });   // R2 arrives
            r3::setField (p.getInternal(), i, U);                          // the edit, after BOTH
            juce::MemoryBlock hostSave;
            d2::offMessageThread ([&] { hostSave = d2::saveOf (p); });
            r3::SettingsSet e = R2;
            switch (i)
            {
                case 0: e.oversample   = U.oversample;   break;
                case 1: e.uiScale      = U.uiScale;      break;
                case 2: e.scopePersist = U.scopePersist; break;
                case 3: e.metersOn     = U.metersOn;     break;
                case 4: e.tooltipsOn   = U.tooltipsOn;   break;
                default: e.uiAnimations = U.uiAnimations; break;
            }
            check (r3::same (r3::settingsOf (hostSave), e),
                   (juce::String (r3::fieldNames[i]) + ": an edit after two arrivals is in the save, over the LATER restore").toRawUTF8());
            p.pollUndoCoalesce();
            check (r3::same (r3::read (p.getInternal()), e),
                   "...and the adoption produces exactly what that save described");
        }

        // --- (g) two restores, the edit BETWEEN them --------------------------
        // R2 is a strictly later arrival, so it replaces the edit. The save must carry
        // R2's values: the overlay must not resurrect an edit a newer restore supersedes.
        for (int i = 0; i < 6; ++i)
        {
            AnamorphAudioProcessor p;
            p.prepareToPlay (48000.0, 512);
            r3::apply (p.getInternal(), P);
            d2::offMessageThread ([&] { d2::restoreFrom (p, blobR); });
            r3::setField (p.getInternal(), i, U);                          // between the two
            d2::offMessageThread ([&] { d2::restoreFrom (p, blobR2); });
            juce::MemoryBlock hostSave;
            d2::offMessageThread ([&] { hostSave = d2::saveOf (p); });
            check (r3::same (r3::settingsOf (hostSave), R2),
                   (juce::String (r3::fieldNames[i]) + ": an edit a LATER restore supersedes is not resurrected by the overlay").toRawUTF8());
            p.pollUndoCoalesce();
            check (r3::same (r3::read (p.getInternal()), R2), "...and the adoption agrees");
        }
    }

    // --- (h) the overlay applies to an edit that happened, and to nothing else ----
    // A save issued inside the window with NO edit behind it must carry the restore's
    // values untouched -- the overlay must not invent one -- and the next save, taken
    // after a real edit, must carry it. The two saves are sequenced by the joins, so this
    // is the "a save reflects every edit it observed and none it did not" half of the
    // invariant, expressed without writing message-thread state from a host thread (which
    // is the very rule the test is about).
    {
        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        r3::apply (p.getInternal(), P);
        d2::offMessageThread ([&] { d2::restoreFrom (p, blobR); });

        juce::MemoryBlock before;
        d2::offMessageThread ([&] { before = d2::saveOf (p); });
        check (r3::same (r3::settingsOf (before), R),
               "a save inside the window with no edit behind it carries the restore's values, untouched");

        r3::setField (p.getInternal(), 3, U);                          // the edit, on the message thread
        juce::MemoryBlock after;
        d2::offMessageThread ([&] { after = d2::saveOf (p); });
        check (r3::settingsOf (after).metersOn == U.metersOn,
               "...and the next save, which does follow the edit, carries it");
        check (r3::same (r3::settingsOf (after), expectedWith ({ 3 })),
               "...with the restore's values still standing in every field the edit did not touch");
    }

    // --- (i) the identity half is untouched by the overlay --------------------
    // The fix must change the Settings and nothing else: the save inside the window still
    // describes the RESTORE's session, never the outgoing one (section 5, State test 42).
    {
        AnamorphAudioProcessor outgoing;
        outgoing.prepareToPlay (48000.0, 512);
        outgoing.getPresets().setMeta ("D2-R12-OUTGOING", "r12-outgoing", anamorph::PresetManager::Selection());
        r3::apply (outgoing.getInternal(), P);

        AnamorphAudioProcessor p;
        p.prepareToPlay (48000.0, 512);
        p.getPresets().setMeta ("D2-R12-OUTGOING", "r12-outgoing", anamorph::PresetManager::Selection());
        r3::apply (p.getInternal(), P);
        d2::offMessageThread ([&] { d2::restoreFrom (p, blobR); });
        r3::apply (p.getInternal(), U);
        juce::MemoryBlock hostSave;
        d2::offMessageThread ([&] { hostSave = d2::saveOf (p); });
        checkStr (r12::presetNameOf (hostSave), "D2-R12-R",
                  "the overlaid save still names the RESTORE's session, not the outgoing one");
        check (r3::same (r3::settingsOf (hostSave), U), "...with the edited Settings on top of it");
    }
}

// ---------------------------------------------------------------------------
//  D-2 stress probe (contract F): every thread the model names, at once, under
//  ThreadSanitizer. NOT part of the suite, like the other three TSan probes:
//  on the pre-D-2 tree its execution IS the undefined behaviour it measures.
//      AnamorphStateTests --d2-stress-probe
//
//  Threads: H restores three sessions in turn and saves after each; P re-prepares
//  with alternating rates; A processes noise blocks; the MAIN thread is the
//  message thread and does what the editor and the processor's timer do -- the
//  tick's reads, pollUndoCoalesce, the timer, a Settings binding read every
//  iteration and a Settings edit, an A/B switch, an undo or redo and a factory
//  preset load at their own cadences. The verdict is the sanitizer's report set;
//  the counts printed are the plain-build symptom.
//
//  ONE PIECE OF SYNCHRONISATION IS THE HOST'S, NOT THE PLUG-IN'S, and it is here
//  so the probe measures the plug-in and not a contract violation: every plug-in
//  format guarantees that prepareToPlay never runs concurrently with processBlock
//  (the engine's plain state relies on it, THREADING_POLICY §Host state calls),
//  so P and A take `hostAudioLock` around those two calls exactly as a host
//  serialises them. The first version of this probe did not, and its 900-odd
//  reports were almost all `AnamorphEngine::prepare` against `process` -- the
//  probe breaking the format contract, which is not what D-2 is about. Nothing
//  else is synchronised: H's restores and saves run against A, P and M with no
//  lock of any kind, which is the whole measurement.
// ---------------------------------------------------------------------------
static int runD2StressProbe()
{
    std::printf ("D-2 stress probe: restore + save (H), prepare (P), audio (A), editor + timer (M)\n");
    std::printf ("  (run under ThreadSanitizer; a report here is the finding, silence with the five\n");
    std::printf ("   deterministic tests green is the resolution)\n");

    const d2::Session sessions[3] = {
        d2::author ("D2-S1", 0.10f, 0.90f, 0, 1),
        d2::author ("D2-S2", 0.30f, 0.70f, 1, 2),
        d2::author ("D2-S3", 0.50f, 0.50f, 0, 3),
    };

    AnamorphAudioProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    proc.getPresets().load (1);
    setRaw (proc, "width", 0.42f);
    proc.pollUndoCoalesce();
    juce::Value animBinding = proc.getInternal().animationsValue();   // the editor's Settings binding

    constexpr int kIterations = 300;
    std::atomic<bool> go { false }, hostDone { false }, prepareDone { false }, stopAudio { false };
    std::atomic<int>  hostSaves { 0 }, audioBlocks { 0 };
    std::atomic<bool> audioFinite { true };
    std::mutex hostAudioLock;   // the HOST's guarantee: prepareToPlay and processBlock never overlap

    std::thread hostThread ([&]
    {
        while (! go.load (std::memory_order_acquire)) {}
        for (int i = 0; i < kIterations; ++i)
        {
            d2::restoreFrom (proc, sessions[i % 3].blob);
            juce::MemoryBlock out;
            proc.getStateInformation (out);
            hostSaves.fetch_add (1, std::memory_order_relaxed);
        }
        hostDone.store (true, std::memory_order_release);
    });
    std::thread prepareThread ([&]
    {
        while (! go.load (std::memory_order_acquire)) {}
        for (int i = 0; i < kIterations; ++i)
        {
            const std::lock_guard<std::mutex> hostSerialises (hostAudioLock);
            proc.prepareToPlay ((i & 1) ? 44100.0 : 48000.0, (i & 1) ? 256 : 512);
        }
        prepareDone.store (true, std::memory_order_release);
    });
    std::thread audioThread ([&]
    {
        juce::AudioBuffer<float> buf (2, 512);
        std::mt19937 rng (0x0d25u);
        while (! go.load (std::memory_order_acquire)) {}
        while (! stopAudio.load (std::memory_order_acquire))
        {
            {
                const std::lock_guard<std::mutex> hostSerialises (hostAudioLock);
                if (! d2::processStaysFinite (proc, 1, buf, rng))
                    audioFinite.store (false, std::memory_order_relaxed);
            }
            audioBlocks.fetch_add (1, std::memory_order_relaxed);
        }
    });

    go.store (true, std::memory_order_release);
    int ticks = 0;
    while (! (hostDone.load (std::memory_order_acquire) && prepareDone.load (std::memory_order_acquire)))
    {
        ++ticks;
        // The editor tick's reads, in the editor's order.
        auto& pm = proc.getPresets();
        const juce::String liveName = pm.currentName();
        const bool liveDirty = pm.isDirty();
        const bool animOn = (bool) animBinding.getValue();
        proc.pollUndoCoalesce();
        const bool u = proc.canUndo(), r = proc.canRedo();
        const int  slot = proc.abActiveSlot();
        if (liveName.isEmpty() && liveDirty && u && r && slot < 0 && animOn)
            std::printf ("  (unreachable, keeps the reads live)\n");

        // The user, at human-ish cadences.
        if (ticks % 7 == 0)  proc.abSwitchTo (1 - proc.abActiveSlot());
        if (ticks % 11 == 0) { if (proc.canUndo()) proc.undo(); else if (proc.canRedo()) proc.redo(); }
        if (ticks % 13 == 0) proc.getPresets().load (ticks % 5);
        if (ticks % 17 == 0) animBinding.setValue (! animOn);
        if (ticks % 19 == 0) { auto* w = proc.getAPVTS().getParameter (pid::width);
                               w->beginChangeGesture(); w->setValueNotifyingHost (0.5f + 0.001f * (float) (ticks % 100)); w->endChangeGesture(); }

        // The processor's own timer.
        juce::Timer::callPendingTimersSynchronously();
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }
    hostThread.join();
    prepareThread.join();
    stopAudio.store (true, std::memory_order_release);
    audioThread.join();

    // Everything pending is adopted; a final save from both sides must agree.
    proc.pollUndoCoalesce();
    juce::MemoryBlock ownerSave = d2::saveOf (proc), hostSave;
    d2::offMessageThread ([&] { hostSave = d2::saveOf (proc); });

    std::printf ("  probe finished: %d restores + %d host saves, %d prepares, %d audio blocks, %d editor ticks\n",
                 kIterations, hostSaves.load(), kIterations, audioBlocks.load(), ticks);
    std::printf ("  audio stayed finite: %s; final host-thread save == owner's save: %s\n",
                 audioFinite.load() ? "yes" : "NO", hostSave == ownerSave ? "yes" : "NO");
    std::printf ("  verdict comes from the sanitizer's report set, not from this exit code\n");
    return 0;
}


int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit; // MessageManager for APVTS/processor on this thread

    if (argc > 1 && std::strcmp (argv[1], "--state-thread-probe") == 0)
        return runStateThreadProbe();

    if (argc > 1 && std::strcmp (argv[1], "--latency-restore-probe") == 0)
        return runLatencyRestoreProbe();

    if (argc > 1 && std::strcmp (argv[1], "--preset-semantics-probe") == 0)
        return runPresetSemanticsProbe();

    if (argc > 1 && std::strcmp (argv[1], "--valuebox-gesture-probe") == 0)
        return runValueBoxGestureProbe();

    if (argc > 1 && std::strcmp (argv[1], "--restore-latency-probe") == 0)
        return runRestoreLatencyProbe();

    if (argc > 1 && std::strcmp (argv[1], "--legacy-ab-probe") == 0)
        return runLegacyAbProbe();

    if (argc > 1 && std::strcmp (argv[1], "--legacy-match-probe") == 0)
        return runLegacyMatchGainProbe();

    if (argc > 1 && std::strcmp (argv[1], "--legacy-settings-probe") == 0)
        return runLegacySettingsProbe();

    if (argc > 1 && std::strcmp (argv[1], "--partial-settings-probe") == 0)
        return runPartialSettingsProbe();

    if (argc > 1 && std::strcmp (argv[1], "--reprepare-race-probe") == 0)
        return runReprepareRaceProbe();

    if (argc > 1 && std::strcmp (argv[1], "--modern-settings-probe") == 0)
        return runModernSettingsProbe();

    if (argc > 1 && std::strcmp (argv[1], "--risk008-probe") == 0)
        return runRisk008Probe();

    if (argc > 1 && std::strcmp (argv[1], "--restore-fade-probe") == 0)
        return runRestoreFadeProbe();

    if (argc > 1 && std::strcmp (argv[1], "--state-prepare-race-probe") == 0)
        return runStatePrepareRaceProbe();

    if (argc > 1 && std::strcmp (argv[1], "--d2-stress-probe") == 0)
        return runD2StressProbe();

    const bool writeSnapshot = argc > 1 && std::strcmp (argv[1], "--write-snapshot") == 0;

    std::printf ("Anamorph state-compatibility self-tests\n");
    std::printf ("fixtures: %s\n\n", fixtureDir().getFullPathName().toRawUTF8());

    if (writeSnapshot)
    {
        testParameterRegistrySnapshot (true);
        return failures == 0 ? 0 : 1;
    }

    testSerializedSchemaShape();
    testParameterRegistrySnapshot (false);
    testStateRoundTripExact();
    testLegacyV02BareApvts();
    testLegacyPre064AbSlots();
    testLegacyPre084InternalMigration();
    testCorruptAndForeignState();
    testPresetSaveReloadRoundTrip();
    testAbAndViewParamPreservation();
    testDuplicateNameFactoryVsUserPreset();
    testFactoryPresetIdIntegrity();
    testPresetIndicatorIdentityAcrossRestore();
    testWrapperProcessBlockAudioPath();
    testFirstActivationUsesRestoredState();
    testNonFiniteParameterInStateIsRejected();
    testValuelessParamMeansDefault();
    testMalformedValuesRestoreDefaults();
    testRepairReachesSavedState();
    testAbandonedValueBoxGestureIsReclaimed();
    testLatencyDeliveryIsDeferredOffMessageThread();
    testPhysicalButtonQueryIgnoresCachedState();
    testRestoreReportsTheRestoredLatency();
    testCrossVersionFieldCapture();
    testLegacyRestoreResetsAbSlots();
    testRestoreIntegrityGuards();
    testPartialSettingsDoNotInherit();
    testOffThreadPrepareDefersLatency();
    testRestoreResetsAbMatchGains();
    testMalformedScopePersistStaysFinite();
    testForeignPresetDoesNotResetSound();
    testRejectedPresetDoesNotDuck();
    testDefaultValuedCorruptionIsRepairedInState();
    testModernSettingsAreRepairedOnRestore();
    testMalformedLegacySettingsResolveToValid();
    testAbStateCoherentAcrossOffThreadRestores();
    testRestoreCyclesUnderRunningAudio();
    testPresetTransitionsUnderConcurrentSaves();
    testRestoreAroundReprepare();
    testUndoHistoryIsOwnedByTheMessageThread();
    testHostSaveNeverPairsRestoredSoundWithOlderProgram();
    testOlderRestoreCannotOverwriteNewerOversampling();
    testSettingsEditAfterRestoreArrivalSurvivesAdoption();
    testActionInHandoffWindowCannotSplitTheSession();
    testInlineRestoreAdoptsPendingAgainstItsOwnSound();
    testSoundEditWhilePendingSurvivesAdoption();
    testOverlappingReplacementIsNotTheRestoresOwnSound();
    testReplacementFinishingLastCannotWearRestoredMetadata();
    testDrainReachesFixedPointBeforeTheCallerActs();
    testPresetSavedDuringRestoreIsCleanAgainstItsOwnFile();
    testSaveBaselineDescribesTheBytesUnderAutomation();
    testAbToggleDerivesItsTargetAfterTheDrain();
    testSettingsPublicationIsFieldLevelAndOrderedByObservation();
    testLoadedPresetBaselineIsFixedFromWhatTheLoadWrote();
    testPresetStepDerivesItsTargetAfterTheDrain();
    testNoToleranceAbsorbsAnAutomationWrite();
    testARestoreIsAFixedPoint();
    testHostSaveInsideThePendingWindowCarriesTheEdit();
    testTooltipSourceOfTruth();
    testEditorConstructDestroy();

    std::printf ("\n%d checks, %d failure(s)\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
