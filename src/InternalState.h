#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "SerializedNumber.h"
#include <array>
#include <atomic>
#include <cmath>
#include <functional>

namespace anamorph
{

// ============================================================================
//  InternalState  (host-hidden session parameters)
//
//  The Settings controls (Oversampling, UI Scale, Scope Persistence, Tooltips, UI
//  Animations) and Show Meters are NOT musical parameters: they are view / engine-config
//  state. Exposing them as VST3 parameters cluttered the host's parameter list (REAPER
//  shows every parameter regardless of JUCE's `withAutomatable(false)` flag), so they are
//  deliberately kept OUT of the APVTS / VST3 parameter tree entirely -- the only reliable
//  way to hide a parameter from a host.
//
//  They still:
//    * persist with the session (serialised in get/setStateInformation),
//    * bind two-way to the GUI via juce::Value (ComboBox::getSelectedIdAsValue,
//      ToggleButton::getToggleStateValue, Slider::getValueObject),
//    * and -- for Oversampling only -- drive the DSP (mirrored to an atomic for the
//      audio thread, with a callback so the wrapper can re-report PDC latency).
//
//  They never participate in A/B, Undo or preset recall: Oversampling is a global engine
//  config, not a musical parameter, and the rest are pure view state. The Multiband
//  parameters are intentionally NOT here -- they remain ordinary APVTS parameters.
// ============================================================================
namespace iid
{
    inline const juce::Identifier oversample   { "int_oversample" };  // ComboBox ID (1..4)
    inline const juce::Identifier uiScale       { "int_uiScale" };     // ComboBox ID (1..5)
    inline const juce::Identifier scopePersist  { "int_scopePersist" };// 0..1
    inline const juce::Identifier metersOn       { "int_metersOn" };
    inline const juce::Identifier tooltipsOn     { "int_tooltipsOn" };
    inline const juce::Identifier uiAnimations   { "int_uiAnimations" };
}

class InternalState : private juce::ValueTree::Listener
{
public:
    // The six host-hidden Settings and their DOCUMENTED defaults, in one place.
    // SERIALIZATION_REGISTRY.md's `ANAMORPH_INTERNAL` table is this list: every
    // field is "Required: No" with a stated Default, which is the contract both
    // the constructor (seed) and restoreState (absent -> default) implement. They
    // were two separate hand-written lists until ER-STATE-18; keeping one is what
    // stops them drifting into two different answers for "absent".
    struct Setting { const juce::Identifier& id; juce::var defaultValue; };

    static const std::array<Setting, 6>& settings()
    {
        static const std::array<Setting, 6> table {{
            { iid::oversample,   juce::var (1)     },  // 1 == "Off (1x)"
            { iid::uiScale,      juce::var (3)     },  // 3 == "M"
            { iid::scopePersist, juce::var (0.5)   },
            { iid::metersOn,     juce::var (false) },
            { iid::tooltipsOn,   juce::var (false) },
            { iid::uiAnimations, juce::var (true)  },
        }};
        return table;
    }

    InternalState()
    {
        tree = juce::ValueTree ("ANAMORPH_INTERNAL");
        for (const auto& s : settings())
            tree.setProperty (s.id, s.defaultValue, nullptr);
        tree.addListener (this);
        syncAtomics();
    }

    ~InternalState() override { tree.removeListener (this); }

    // --- GUI two-way binding (message thread) ----------------------------
    juce::Value oversampleValue()   { return tree.getPropertyAsValue (iid::oversample,   nullptr); }
    juce::Value uiScaleValue()      { return tree.getPropertyAsValue (iid::uiScale,      nullptr); }
    juce::Value scopePersistValue() { return tree.getPropertyAsValue (iid::scopePersist, nullptr); }
    juce::Value metersValue()       { return tree.getPropertyAsValue (iid::metersOn,     nullptr); }
    juce::Value tooltipsValue()     { return tree.getPropertyAsValue (iid::tooltipsOn,   nullptr); }
    juce::Value animationsValue()   { return tree.getPropertyAsValue (iid::uiAnimations, nullptr); }

    // --- DSP read (audio thread, lock-free) ------------------------------
    int oversampleIndex() const noexcept { return osAtomic.load (std::memory_order_relaxed); } // 0..3

    // The SpectrumImager polls UI-animation state through a float atomic (legacy shape):
    // 1.0 == on, 0.0 == off. Mirrors the uiAnimations property.
    const std::atomic<float>* animationsFloatPtr() const noexcept { return &animFloat; }

    // --- message-thread reads (editor) -----------------------------------
    bool  metersOn()     const { return (bool)  tree[iid::metersOn]; }
    bool  tooltipsOn()   const { return (bool)  tree[iid::tooltipsOn]; }
    bool  animationsOn() const { return (bool)  tree[iid::uiAnimations]; }
    float scopePersist() const { return (float) (double) tree[iid::scopePersist]; }
    int   uiScaleIndex() const { return juce::jlimit (0, 4, (int) tree[iid::uiScale] - 1); }

    // Fired (message thread) when Oversampling changes, so the wrapper can re-report PDC.
    std::function<void()> onOversampleChanged;

    // Change generation (H15, Wave 2): bumped on every property change, including a
    // session restore. The Settings widgets bind these values two-way (juce::Value),
    // so they can move with the cursor OUTSIDE the editor -- the editor's micro-anim
    // poll re-arms on this counter instead of hashing widget values every frame.
    juce::uint32 generation() const noexcept { return gen.load (std::memory_order_relaxed); }

    // --- state persistence ----------------------------------------------
    juce::ValueTree copyState() const { return tree.createCopy(); }
    void restoreState (const juce::ValueTree& src)
    {
        if (! src.isValid()) return;

        // EVERY field is written, present or not: an absent one takes its documented
        // default (the registry's `ANAMORPH_INTERNAL` table), exactly as
        // migrateFromLegacyApvts already does for the legacy shape. `tree` is a
        // processor member and a host restores into ONE live instance repeatedly,
        // so skipping an absent field does not mean "leave it alone" -- it means
        // "keep the PREVIOUS project's value", which is not a state this session
        // ever described. Measured before this loop wrote unconditionally
        // (ER-STATE-18, --partial-settings-probe): a modern session omitting a
        // single Setting inherited the previous project's value in 6 cases out of
        // 6, while the legacy path inherited in 0 -- the reverse of where the
        // review looked. A session that CARRIES the field is unaffected: it is
        // written from `src` exactly as before.
        for (const auto& s : settings())
            tree.setProperty (s.id,
                              src.hasProperty (s.id) ? src.getProperty (s.id) : s.defaultValue,
                              nullptr);
        // (syncAtomics + onOversampleChanged run via the property-change callbacks above.)
    }

    // One-time migration from a pre-0.8.4 session, where these were ordinary APVTS
    // parameters. Old sessions have no ANAMORPH_INTERNAL child, so without this their
    // saved Oversampling / UI Scale / Persistence / Tooltips / Animations / Show Meters
    // would silently revert to defaults. Reads the legacy PARAM nodes (id/value) out of the
    // saved APVTS state and maps them onto the host-hidden InternalState.
    void migrateFromLegacyApvts (const juce::ValueTree& apvtsState)
    {
        if (! apvtsState.isValid()) return;

        // A legacy PARAM value is used only when it is a USABLE serialized number --
        // the same predicate the session and preset restore paths apply
        // (SerializedNumber.h, ER-STATE-05): plain decimal text, finite after
        // parsing. Anything else means the field's default, exactly as an absent
        // node does. This is not cosmetic: JUCE's text parser accepts "nan" and
        // "inf" as numbers, and the `(int)` conversion below of a NaN, an infinity
        // or an out-of-range double is UNDEFINED BEHAVIOUR (C++ [conv.fpint]).
        // Measured through the real v0.2 restore before this guard existed, on
        // x86-64: every such value became -2147483647 in the tree -- an impossible
        // ComboBox id, saved back out with the session on the next save -- and
        // "2147483647" wrapped to INT_MIN through a second UB, signed overflow in
        // the `+ 1`. On AArch64 the same inputs saturate differently (NaN -> 0,
        // +overflow -> INT_MAX, which the `+ 1` then overflows), so the corruption
        // was also platform-dependent. State test 28 pins every case.
        auto legacy = [&apvtsState] (juce::StringRef id, double fallback) -> double
        {
            for (int i = 0; i < apvtsState.getNumChildren(); ++i)
            {
                auto c = apvtsState.getChild (i);
                if (c.hasType ("PARAM") && c.getProperty ("id").toString() == id)
                {
                    const auto prop = c.getProperty ("value");
                    if (prop.isVoid()) return fallback;
                    if (prop.isString()
                         && ! looksLikePlainNumber (prop.toString().trim().toRawUTF8()))
                        return fallback;                              // "abc", "", "0x10", "inf", "nan"
                    const double v = (double) prop;
                    // Usability is judged on the float narrowing, as the other two
                    // paths judge it, so "1e39" (finite as a double, infinite as the
                    // float these values were) resolves the same way everywhere.
                    if (! isUsableSerializedValue ((float) v)) return fallback;
                    return v;
                }
            }
            return fallback;
        };

        // Choice params stored a 0-based index; the ComboBox IDs here are 1-based.
        // Clamp into the combo's domain IN DOUBLE, before the integer conversion:
        // the conversion is then of a value in [0, count-1], defined for every
        // input this lambda can return, and the `+ 1` cannot overflow. A finite
        // out-of-domain value ("7") lands on the nearest valid choice, which is
        // what NormalisableRange does for an out-of-range parameter.
        auto comboId = [] (double index0, int count) -> int
        {
            return (int) juce::jlimit (0.0, (double) (count - 1), index0) + 1;
        };
        tree.setProperty (iid::oversample,   comboId (legacy ("oversample", 0.0), 4), nullptr); // ids 1..4
        tree.setProperty (iid::uiScale,      comboId (legacy ("uiScale",    2.0), 5), nullptr); // ids 1..5
        tree.setProperty (iid::scopePersist, juce::jlimit (0.0, 1.0, legacy ("scopePersist", 0.5)), nullptr);
        tree.setProperty (iid::metersOn,     legacy ("metersOn",   0.0) > 0.5,     nullptr);
        tree.setProperty (iid::tooltipsOn,   legacy ("tooltipsOn", 0.0) > 0.5,     nullptr);
        tree.setProperty (iid::uiAnimations, legacy ("uiAnimations", 1.0) > 0.5,   nullptr);
    }

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& id) override
    {
        gen.fetch_add (1, std::memory_order_relaxed); // H15 re-arm signal
        syncAtomics();
        if (id == iid::oversample && onOversampleChanged) onOversampleChanged();
    }
    void syncAtomics()
    {
        osAtomic.store (juce::jlimit (0, 3, (int) tree[iid::oversample] - 1), std::memory_order_relaxed);
        animFloat.store ((bool) tree[iid::uiAnimations] ? 1.0f : 0.0f, std::memory_order_relaxed);
    }

    juce::ValueTree tree;
    std::atomic<int>   osAtomic  { 0 };
    std::atomic<float> animFloat { 1.0f };
    std::atomic<juce::uint32> gen { 1 };
};

} // namespace anamorph
