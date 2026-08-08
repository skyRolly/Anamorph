// ============================================================================
//  Anamorph state-compatibility self-tests (v0.8.13 harness)
//
//  Headless regression net for the COMPATIBILITY policy family
//  (SESSION_COMPATIBILITY_POLICY / PARAMETER_COMPATIBILITY_POLICY): it
//  exercises the REAL AnamorphAudioProcessor (this target compiles the plugin
//  sources — the editor is linked but never instantiated), so every check runs
//  the exact production serialization code paths.
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
//
//  Fixture workflow: an INTENTIONAL parameter/schema change (which requires an
//  ADR + registry update per the compatibility policies) is recorded by
//  regenerating the snapshot:  AnamorphStateTests --write-snapshot
//  An unintentional change fails the comparison — that is the point.
//
//  Exits non-zero on any failure so the build gate can fail the run.
// ============================================================================

#include "PluginProcessor.h"

#include <cmath>
#include <cstdio>
#include <cstring>
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
    a.getPresets().setMeta ("RoundTrip Fixture", "sig-baseline-token");

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

    // The bare path predates InternalState AND its legacy APVTS params: the
    // host-hidden settings stay at their defaults.
    check ((int) p.getInternal().copyState()["int_oversample"] == 1,
           "InternalState stays default for a v0.2 session");
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
    // Legacy slots carry no name/baseline of their own: the read keeps the slot's
    // pre-restore meta — for a fresh instance, the construction snapshot ("Default").
    checkStr (ab["slotAName"].toString(), "Default", "legacy slot keeps pre-restore meta");

    // Behavioural: switching to slot A applies the legacy-read params.
    p.abSwitchTo (0);
    checkNear ((double) rawOf (p, "width"),
               (double) dynamic_cast<juce::RangedAudioParameter*> (p.getAPVTS().getParameter ("width"))
                            ->convertTo0to1 (1.8f),
               1.0e-6, "switching to legacy slot A applies its width");
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
    {
        juce::XmlElement foreign ("SOME_FUTURE_ROOT");
        foreign.setAttribute ("v", 99);
        const auto blob = BlobCodec::wrap (foreign);
        p.setStateInformation (blob.getData(), (int) blob.getSize());
        check (juce::exactlyEqual (rawOf (p, "width"), widthBefore), "unknown root tag leaves parameters untouched");
    }

    // Out-of-range A/B active (hand-edited / forward-version blob) clamps —
    // the end-to-end guard over anamorph::clampAbSlotIndex.
    auto restoreWithActive = [&p] (const char* active)
    {
        juce::XmlElement root ("AnamorphRoot");
        auto* ab = root.createNewChildElement ("AB");
        ab->setAttribute ("active", active);
        const auto blob = BlobCodec::wrap (root);
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

    check (presetFile.deleteFile(), "test preset file removed");
    if (hadUserFile) parked.moveFileTo (presetFile);
    presets.refresh();
}

// ---------------------------------------------------------------------------
int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit; // MessageManager for APVTS/processor on this thread

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

    std::printf ("\n%d checks, %d failure(s)\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
