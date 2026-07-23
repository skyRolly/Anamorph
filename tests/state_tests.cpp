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
    for (auto* field : { "active", "slotAParams", "slotAName", "slotABase",
                         "slotBParams", "slotBName", "slotBBase" })
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

    std::printf ("\n%d checks, %d failure(s)\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
