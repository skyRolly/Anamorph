#pragma once

#include <memory>
#include <vector>

namespace gui_scale_lifetime_tests
{
// Deliver the real private timer override without a shipping test seam or a
// different class definition. Explicit template instantiation permits access.
struct TimerEntry
{
    using type = void (AnamorphAudioProcessorEditor::*)();
    friend type getTimerEntry (TimerEntry);
};
template <typename Tag, typename Tag::type Member>
struct AccessTimer { friend typename Tag::type getTimerEntry (Tag) { return Member; } };
template struct AccessTimer<TimerEntry, &AnamorphAudioProcessorEditor::timerCallback>;

struct HostParent final : juce::Component
{
    AnamorphAudioProcessor& proc;
    std::unique_ptr<juce::AudioProcessorEditor>& editor;
    const bool destroy;
    bool armed = false, onMessageThread = true;
    int callbacks = 0, closes = 0;
    HostParent (AnamorphAudioProcessor& p, std::unique_ptr<juce::AudioProcessorEditor>& e, bool d)
        : proc (p), editor (e), destroy (d) {}
    void childBoundsChanged (juce::Component* child) override
    {
        if (! armed || child != editor.get()) return;
        ++callbacks;
        onMessageThread = onMessageThread && juce::MessageManager::existsAndIsCurrentThread();
        if (destroy && editor != nullptr)
        {
            ++closes;
            proc.editorBeingDeleted (editor.get());
            editor.reset();
        }
    }
};

template <typename T, typename Predicate>
static T* find (juce::Component& c, const Predicate& matches)
{
    if (auto* t = dynamic_cast<T*> (&c); t != nullptr && matches (*t)) return t;
    for (auto* child : c.getChildren())
        if (auto* t = find<T> (*child, matches)) return t;
    return nullptr;
}
} // namespace gui_scale_lifetime_tests

// Actual editor DPI, Advanced click and timer entry points. No peer, native host,
// visual audition or claim about arbitrary deletion from any child setter.
static void testGuiScaleLifetimes (int selected = -1)
{
    using namespace gui_scale_lifetime_tests;
    std::printf ("State test 156: editor scaling survives synchronous host-parent teardown\n");
    check (selected >= -1 && selected <= 3, "156: selector is valid, never an empty passing suite");
    if (selected < -1 || selected > 3) return;
    for (int action = 0; action < 4; ++action)
    {
        if (selected >= 0 && selected != action) continue;
        for (bool destroy : { false, true })
        {
            auto owner = std::make_unique<AnamorphAudioProcessor>();
            auto& proc = *owner;
            proc.prepareToPlay (48000.0, 128);
            for (int cycle = 0; cycle < 2; ++cycle)
            {
                // Return to Simple/M before each fresh editor. No event loop is
                // pumped and no window or host-owned object is fabricated.
                proc.getAPVTS().getParameter (pid::advancedMode)->setValueNotifyingHost (0.0f);
                proc.getInternal().uiScaleValue().setValue (3);
                std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
                check (editor != nullptr && editor->getPeer() == nullptr, "156: real editor constructs without a native peer");
                if (editor == nullptr) continue;
                const int simpleHeight = editor->getHeight();
                const int width = editor->getWidth();
                const auto dispose = [&]
                { if (editor != nullptr) { proc.editorBeingDeleted (editor.get()); editor.reset(); } };
                auto* adv = find<juce::ToggleButton> (*editor, [] (auto& b) { return b.getTooltip() == "Advanced mode"; });
                auto* scale = find<juce::ComboBox> (*editor, [] (auto& b)
                    { return b.getNumItems() == 5 && b.getItemText (0) == "XS" && b.getItemText (4) == "XL"; });
                auto* undo = find<juce::Button> (*editor, [] (auto& b) { return b.getTooltip() == "Undo"; });
                check (adv != nullptr && scale != nullptr && undo != nullptr,
                       "156: actual mode, scale and Undo controls are found");
                if (adv == nullptr || scale == nullptr || undo == nullptr) { dispose(); continue; }
                HostParent host (proc, editor, destroy);
                host.addAndMakeVisible (*editor);
                const juce::Component::SafePointer<juce::AudioProcessorEditor> weak (editor.get());
                std::vector<float> parameters;
                for (auto* p : proc.getParameters()) parameters.push_back (p->getValue());
                const int latency = proc.getLatencySamples();
                if (action == 0)
                {
                    // Compose an existing user scale with the host's DPI scale.
                    scale->setSelectedId (4, juce::dontSendNotification);
                    (static_cast<AnamorphAudioProcessorEditor*> (editor.get())->*getTimerEntry (TimerEntry{}))();
                    check (juce::approximatelyEqual (editor->getTransform().getScaleFactor(), 1.25f),
                           "156 DPI control: the user scale was really installed first");
                }
                else if (action == 1)
                    scale->setSelectedId (4, juce::dontSendNotification);
                else
                    // Deliver the production callback/timer after the toggle's
                    // view state changes. This is not a DAW automation dispatch.
                    adv->setToggleState (true, juce::dontSendNotification);
                if (action == 1 || action == 2)
                    check (undo->isEnabled(), "156 timer premise: a later Undo refresh must still be needed");
                check (host.callbacks == 0 && editor != nullptr, "156 control: teardown is armed only at the intended callout");
                host.armed = true;
                if (action == 0)
                    editor->setScaleFactor (1.4f);
                else if (action == 3)
                {
                    check (adv->onClick != nullptr, "156: actual component-owned Advanced callback exists");
                    if (adv->onClick) adv->onClick();
                }
                else
                    (static_cast<AnamorphAudioProcessorEditor*> (editor.get())->*getTimerEntry (TimerEntry{}))();

                check (host.callbacks > 0 && host.onMessageThread, "156 control: the real bounds/transform call synchronously reaches its parent");
                if (destroy)
                    check (host.closes == 1 && weak == nullptr && editor == nullptr,
                           "156 control: the parent really destroys this editor once during the callout");
                else
                {
                    check (host.closes == 0 && weak != nullptr && editor != nullptr, "156: ordinary scaling preserves the editor");
                    if (editor != nullptr)
                    {
                        const float expectedScale = action == 0 ? 1.4f * 1.25f : action == 1 ? 1.25f : 1.0f;
                        check (juce::approximatelyEqual (editor->getTransform().getScaleFactor(), expectedScale),
                               "156: host/user scale composition and ordinary transforms are preserved");
                        check (editor->getWidth() == width && (action < 2 ? editor->getHeight() == simpleHeight
                                                                        : editor->getHeight() > simpleHeight),
                               "156: scaling retains logical width and applies the selected mode height");
                        if (action == 1 || action == 2)
                            check (! undo->isEnabled(), "156 timer control: a surviving scale still reaches the later Undo refresh");
                    }
                }
                for (int i = 0; i < proc.getParameters().size(); ++i)
                    check (juce::exactlyEqual (proc.getParameters()[i]->getValue(), parameters[static_cast<std::size_t> (i)]),
                           "156: these GUI entry points do not rewrite processor parameters");
                check (proc.getLatencySamples() == latency && ! anamorph::param::insideDispatch(),
                       "156: GUI teardown preserves latency and leaves no dispatch scope");
                host.armed = false;
                dispose();
                editor.reset (proc.createEditor());
                check (editor != nullptr && editor->getWidth() == width && editor->getHeight() == simpleHeight,
                       "156: fresh editor reflects processor mode, not the destroyed view's cached toggle");
                if (editor != nullptr)
                    check (juce::approximatelyEqual (editor->getTransform().getScaleFactor(), action < 2 ? 1.25f : 1.0f),
                           "156: fresh editor retains user scale, while host DPI belongs to each editor");
                dispose();
            }
        }
    }
}
