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
#include <random>
#include <vector>

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
    auto parked = juce::File::getSpecialLocation (juce::File::tempDirectory)
                      .getChildFile ("AnamorphStateHarness.parked");
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
    auto tempCopy = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("AnamorphStateHarnessCopy" + anamorph::PresetManager::fileSuffix());
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
static void testLatencyDeliveryIsDeferredOffMessageThread()
{
    std::printf ("State test 22: an off-thread latency change is deferred, not delivered (D-1)\n");

    AnamorphAudioProcessor proc;
    // Oversampling ON: predictLatency short-circuits to 0 when it is Off, so
    // without this every measurement below would be 0 == 0 and vacuous.
    proc.getInternal().oversampleValue().setValue (2);   // 1-based combo id: 2x
    proc.prepareToPlay (48000.0, 256);

    auto* drive = proc.getAPVTS().getParameter (pid::drive);
    check (drive != nullptr, "the drive parameter exists");
    if (drive == nullptr) return;

    const int quiet = proc.getLatencySamples();
    std::printf ("  latency at drive 0: %d\n", quiet);

    // CONTROL LEG -- the message thread stays synchronous. If this regressed to
    // deferred, every UI edit would lag a timer tick and the test below would
    // still pass, so the control is what keeps it honest.
    drive->setValueNotifyingHost (0.6f);
    const int afterOnThread = proc.getLatencySamples();
    std::printf ("  after a message-thread change: %d (expected immediate)\n", afterOnThread);
    check (afterOnThread != quiet, "a message-thread change still reports latency SYNCHRONOUSLY");

    // ...and the non-vacuity gate for the whole test: drive must actually move
    // the reported latency, or "deferred" below is indistinguishable from
    // "nothing ever happens".
    const int loud = afterOnThread;
    check (loud != quiet, "drive genuinely changes the reported latency at 2x oversampling");

    // THE REAL CASE: the same listener, driven from another thread.
    drive->setValueNotifyingHost (0.0f);
    juce::Timer::callPendingTimersSynchronously();
    check (proc.getLatencySamples() == quiet, "reset to the quiet latency before the off-thread leg");

    std::atomic<bool> done { false };
    std::thread automation ([&]
    {
        // Exactly what a VST3 host's automation does on the audio thread: move the
        // parameter, which fires the APVTS listener on THIS thread.
        drive->setValueNotifyingHost (0.6f);
        done.store (true, std::memory_order_release);
    });
    automation.join();
    check (done.load (std::memory_order_acquire), "the off-thread parameter write completed");

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
//     Two things in setStateInformation move a latency-bearing parameter without
//     the latency listener hearing the final value. apvts.replaceState adopts
//     whatever @value says -- and it CONVERTS BY CLAMPING, so a poisoned
//     value="inf" for drive lands at the range maximum and re-reports a latency
//     for it. reassertParameters then repairs that value with setValue() plus a
//     direct atomic store, notifying nobody on purpose.
//
//     THE SECOND RESTORE IS THE ONE THAT MATTERS, and this is why the test looks
//     the way it does. On a FIRST restore the answer comes out right by accident:
//     the live InternalState holds an int where the round-tripped blob holds a
//     string, ValueTree::setProperty sees a difference, fires
//     onOversampleChanged, and recomputes the latency after the repair -- the
//     same var-type coincidence recorded for ER-STATE-07. Restoring twice settles
//     the types, removes the coincidence, and exposes the defect. A test that
//     only did one restore would have reported this as refuted; the round-4 probe
//     did exactly that, twice, before the second-restore leg was added.
//
//     Measured before the fix: reported 4, restored state predicts 0.
static void testRestoreReportsTheRestoredLatency()
{
    std::printf ("State test 24: a restore reports the RESTORED state's latency\n");

    juce::MemoryBlock poisoned;
    {
        AnamorphAudioProcessor authoring;
        // Oversampling on: predictLatency short-circuits to 0 when it is Off, so
        // without this every number below is 0 and the test proves nothing.
        authoring.getInternal().oversampleValue().setValue (2);
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
    proc.getInternal().oversampleValue().setValue (2);
    proc.prepareToPlay (48000.0, 256);

    // NON-VACUITY: drive must actually be able to move the reported latency here,
    // or "reported == predicted" below is 0 == 0 and means nothing.
    proc.getAPVTS().getParameter (pid::drive)->setValueNotifyingHost (1.0f);
    juce::Timer::callPendingTimersSynchronously();
    const int loud = proc.getLatencySamples();
    check (loud != 0, "drive at maximum reports a non-zero latency at 2x oversampling");

    proc.setStateInformation (poisoned.getData(), (int) poisoned.getSize()); // settle property types
    proc.getAPVTS().getParameter (pid::drive)->setValueNotifyingHost (1.0f); // back to latency-bearing
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

    std::printf ("  second restore: reported %d, restored state predicts %d\n", reported, predicted);
    check (reported == predicted,
           "the host is not left holding the poisoned value's latency after a repair");
}

// Round-4 probe: does a malformed restore leave a STALE reported latency?
static int runRestoreLatencyProbe()
{
    std::printf ("Round-4 probe: reported latency after a malformed restore\n");

    // Author a session whose algorithm is poisoned to "inf". JUCE's own
    // replaceState adopts it BEFORE reassertParameters gets a chance to repair,
    // and replaceState notifies -- so the host hears a latency for a state the
    // plug-in is about to reject.
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
            void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
            void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& d) override
            {
                if (! d.latencyChanged || ! armed.exchange (false)) return;
                fired.fetch_add (1);
                inDelivery.store (true);                       // delivery has started...
                while (! requested.load()) std::this_thread::yield();   // ...hold it open until the request lands
            }
        };

        AnamorphAudioProcessor p;
        p.getInternal().oversampleValue().setValue (2);        // 1-based combo id: 2x -- latency moves with drive
        p.prepareToPlay (48000.0, 512);
        auto* drive = p.getAPVTS().getParameter (pid::drive);
        drive->setValueNotifyingHost (0.0f); p.prepareToPlay (48000.0, 512);
        const int lowLat  = p.getLatencySamples();
        drive->setValueNotifyingHost (1.0f); p.prepareToPlay (48000.0, 512);
        const int highLat = p.getLatencySamples();
        drive->setValueNotifyingHost (0.0f); p.prepareToPlay (48000.0, 512);   // back to low; host holds lowLat
        check (lowLat != highLat, "non-vacuity: the reported latency actually moves with drive here");

        auto waitForReported = [&p] (int expected, int deadlineMs)
        {
            for (int elapsed = 0; elapsed < deadlineMs; elapsed += 5)
            {
                juce::Timer::callPendingTimersSynchronously();
                if (p.getLatencySamples() == expected) return true;
                std::this_thread::sleep_for (std::chrono::milliseconds (5));
            }
            return p.getLatencySamples() == expected;
        };

        Barrier barrier;
        p.addListener (&barrier);
        barrier.armed.store (true);

        std::thread worker ([&]
        {
            // Request #1, off the message thread: drive -> 1 raises the flag.
            drive->setValueNotifyingHost (1.0f);
            // Wait until the timer's delivery of #1 has STARTED (the barrier holds it open)...
            while (! barrier.inDelivery.load()) std::this_thread::yield();
            // ...and make request #2 from inside that window: drive -> 0 raises the flag again.
            drive->setValueNotifyingHost (0.0f);
            barrier.requested.store (true);
        });

        // Serve request #1 on the message thread. This is the delivery the barrier sits in.
        const bool firstDelivered = waitForReported (highLat, 2000);
        worker.join();
        check (barrier.fired.load() == 1, "the barrier fired exactly once, inside delivery #1");
        check (firstDelivered, "delivery #1 completed with the value it read (highLat)");

        // Request #2 landed DURING delivery #1. It must still be pending, and the
        // next tick must serve it -- nothing else on the message thread will.
        const bool secondDelivered = waitForReported (lowLat, 2000);
        std::printf ("  delivery #1 -> %d with request #2 made inside it; next tick -> %d (expected %d)\n",
                     highLat, p.getLatencySamples(), lowLat);
        check (secondDelivered, "a request made DURING a delivery survives it and is served by the next tick");
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
    testMalformedLegacySettingsResolveToValid();
    testTooltipSourceOfTruth();
    testEditorConstructDestroy();

    std::printf ("\n%d checks, %d failure(s)\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
