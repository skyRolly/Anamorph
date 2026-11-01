#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <memory>
#include <limits>
#include <vector>

// Programmatic metadata and component-event coverage. No window, native
// accessibility client, global focus or host keyboard dispatch is simulated.
static void testRotaryAccessibilityMetadataAndKeyboard()
{
    std::printf("State test 152: rotary captions and delivered keys survive editor reopen (NR-014)\n");
    auto processor = std::make_unique<AnamorphAudioProcessor>();
    auto& proc = *processor;
    proc.prepareToPlay(48000.0, 128);
    auto& state = proc.getAPVTS();
    struct Control { const char* id; const char* caption; };
    // These are the existing, unchanged visible captions supplied to setupRotary.
    const std::array<Control, 12> controls {{
        {pid::drive, "Drive"}, {pid::amount, "Amount"}, {pid::width, "Width"},
        {pid::haasDelay, "Delay"}, {pid::velvetDensity, "Density"},
        {pid::chorusRate, "Rate"}, {pid::chorusDepth, "Depth"}, {pid::mix, "Mix"},
        {pid::outputGain, "Output"}, {pid::outputBalance, "Balance"},
        {pid::monoMakerFreq, "Freq"}, {pid::inputBalance, "Balance"}
    }};
    const auto same = [] (float a, float b)
    { return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b); };
    // Lookup compares the double Slider mapping with APVTS's float mapping.
    // MonoMaker's skew conversion differs by a few ULPs; this bound is only
    // for widget identification/readback, never for DSP or unrelated raw state.
    const auto closeValue = [] (float a, float b)
    { return std::abs(a - b) <= 8.0f * std::numeric_limits<float>::epsilon()
                                      * juce::jmax(1.0f, std::abs(b)); };
    auto* advanced = state.getParameter(pid::advancedMode);
    check(advanced != nullptr, "GUI metadata: Advanced parameter exists");
    if (advanced == nullptr) return;

    for (bool expanded : { false, true, false })
    {
        advanced->setValueNotifyingHost(expanded ? 1.0f : 0.0f);
        const auto dispose = [&proc] (juce::AudioProcessorEditor* editor)
        { if (editor != nullptr) { proc.editorBeingDeleted(editor); delete editor; } };
        std::unique_ptr<juce::AudioProcessorEditor, decltype(dispose)> editor(proc.createEditor(), dispose);
        check(editor != nullptr, "GUI metadata: editor opens during Simple/Advanced/Simple cycle");
        if (editor == nullptr) return;
        std::vector<juce::Slider*> sliders;
        std::vector<juce::Label*> captions;
        const auto visit = [&] (auto&& self, juce::Component& component) -> void
        {
            if (auto* slider = dynamic_cast<juce::Slider*>(&component)) sliders.push_back(slider);
            if (auto* label = dynamic_cast<juce::Label*>(&component)) captions.push_back(label);
            for (auto* child : component.getChildren()) self(self, *child);
        };
        visit(visit, *editor);
        std::vector<juce::Component::SafePointer<juce::Slider>> lifetimes;
        int exercisedKeys = 0;
        for (const auto& control : controls)
        {
            auto* parameter = state.getParameter(control.id);
            auto* raw = state.getRawParameterValue(control.id);
            check(parameter != nullptr && raw != nullptr, "GUI metadata: registered control exists");
            if (parameter == nullptr || raw == nullptr) continue;

            // Identify the real attached widget through two distinct host writes,
            // without a private-field hook or a coincidentally matching default.
            parameter->setValueNotifyingHost(0.23f);
            const float firstRaw = raw->load();
            std::vector<float> first;
            for (auto* slider : sliders) first.push_back(static_cast<float>(slider->getValue()));
            parameter->setValueNotifyingHost(0.71f);
            const float secondRaw = raw->load();
            juce::Slider* selected = nullptr;
            int matched = 0;
            for (std::size_t i = 0; i < sliders.size(); ++i)
                if (closeValue(first[i], firstRaw)
                    && closeValue(static_cast<float>(sliders[i]->getValue()), secondRaw)
                    && !same(firstRaw, secondRaw))
                { selected = sliders[i]; ++matched; }
            check(matched == 1, "GUI metadata: two host writes identify exactly one attached slider");
            if (matched != 1 || selected == nullptr) continue;
            lifetimes.emplace_back(selected);
            bool captionPresent = false;
            for (auto* label : captions)
                captionPresent = captionPresent || label->getText() == control.caption;
            check(captionPresent, "GUI metadata: the expected existing visible caption is present");
            check(selected->getTitle() == control.caption,
                  "GUI metadata: rotary accessibility title matches its existing caption");
            check(closeValue(static_cast<float>(selected->getValue()), raw->load()),
                  "GUI metadata: naming the widget preserves its attached value");

            // A hidden widget is not a keyboard-dispatch target. Delivery below
            // tests the component handler only, not Tab traversal or a DAW route.
            if (!selected->isVisible() || !selected->isEnabled()) continue;
            ++exercisedKeys;
            std::vector<float> untouched;
            for (auto* p : proc.getParameters()) untouched.push_back(p->getValue());
            const float beforeKey = raw->load();
            check(selected->keyPressed(juce::KeyPress(juce::KeyPress::rightKey)),
                  "GUI keys: a delivered unmodified Right is handled");
            check(raw->load() > beforeKey && closeValue(static_cast<float>(selected->getValue()), raw->load()),
                  "GUI keys: Right changes the real parameter through its attachment");
            check(selected->keyPressed(juce::KeyPress(juce::KeyPress::leftKey)),
                  "GUI keys: a delivered Left is handled on the return leg");
            check(std::abs(raw->load() - beforeKey) <= 1.0e-6f * juce::jmax(1.0f, std::abs(beforeKey)),
                  "GUI keys: Right/Left returns to the starting legal value");
            for (int modifier : { juce::ModifierKeys::shiftModifier, juce::ModifierKeys::ctrlModifier,
                                  juce::ModifierKeys::altModifier, juce::ModifierKeys::commandModifier })
            {
                const float prior = raw->load();
                check(!selected->keyPressed(juce::KeyPress(juce::KeyPress::rightKey,
                                                          juce::ModifierKeys(modifier), 0))
                          && same(prior, raw->load()),
                      "GUI keys: modified arrows are left to existing host/parent routing");
            }
            for (int i = 0; i < proc.getParameters().size(); ++i)
                if (proc.getParameters()[i] != parameter)
                    check(same(proc.getParameters()[i]->getValue(), untouched[static_cast<std::size_t>(i)]),
                          "GUI keys: unrelated parameters retain their values");
        }
        check(lifetimes.size() == controls.size(), "GUI metadata: all twelve rotary controls were observed");
        check(exercisedKeys >= 2, "GUI keys: visible control delivery is non-vacuous in every view");
        editor.reset();
        bool released = true;
        for (const auto& pointer : lifetimes) released = released && pointer.getComponent() == nullptr;
        check(released, "GUI metadata: observed controls are destroyed before the editor is reopened");
    }
}
