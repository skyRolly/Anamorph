#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <atomic>
#include <functional>

namespace anamorph
{

// ============================================================================
//  InternalState  (host-hidden session parameters)
//
//  The Settings controls (Oversampling, Window Size, Scope Persistence, Tooltips, UI
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
    InternalState()
    {
        tree = juce::ValueTree ("ANAMORPH_INTERNAL");
        tree.setProperty (iid::oversample,   1,    nullptr); // 1 == "Off (1x)"
        tree.setProperty (iid::uiScale,      3,    nullptr); // 3 == "M"
        tree.setProperty (iid::scopePersist, 0.5,  nullptr);
        tree.setProperty (iid::metersOn,     false, nullptr);
        tree.setProperty (iid::tooltipsOn,   false, nullptr);
        tree.setProperty (iid::uiAnimations, true,  nullptr);
        tree.addListener (this);
        syncAtomics();
    }

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

    // --- state persistence ----------------------------------------------
    juce::ValueTree copyState() const { return tree.createCopy(); }
    void restoreState (const juce::ValueTree& src)
    {
        if (! src.isValid()) return;
        for (auto id : { iid::oversample, iid::uiScale, iid::scopePersist,
                         iid::metersOn, iid::tooltipsOn, iid::uiAnimations })
            if (src.hasProperty (id)) tree.setProperty (id, src.getProperty (id), nullptr);
        // (syncAtomics + onOversampleChanged run via the property-change callbacks above.)
    }

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& id) override
    {
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
};

} // namespace anamorph
