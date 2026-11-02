#pragma once

#include <functional>
#include <memory>
#include <optional>

// Real component callbacks and synchronous parameter notifications; no window,
// native host, assistive client or fabricated GUI acceptance.
static void testEditorCommandLifetimes (int selected = -1)
{
    std::printf ("State test 154: Undo/Redo/A-B callbacks survive reentrant editor destruction\n");
    check (selected >= -1 && selected <= 2, "154: selected command is valid, never an empty passing suite");
    if (selected < -1 || selected > 2) return;

    struct CloseOnWrite final : juce::AudioProcessorParameter::Listener
    {
        AnamorphAudioProcessor& proc;
        std::unique_ptr<juce::AudioProcessorEditor>& editor;
        int closes = 0;
        CloseOnWrite (AnamorphAudioProcessor& p, std::unique_ptr<juce::AudioProcessorEditor>& e) : proc (p), editor (e) {}
        void parameterValueChanged (int, float) override
        {
            if (closes != 0) return;
            ++closes;
            proc.editorBeingDeleted (editor.get());
            editor.reset();
        }
        void parameterGestureChanged (int, bool) override {}
    };
    const auto presetLabel = [] (juce::Component& parent)
    {
        std::optional<juce::String> found;
        const auto visit = [&] (auto&& self, juce::Component& c) -> void
        {
            if (auto* b = dynamic_cast<juce::Button*> (&c); b != nullptr && b->getComponentID() == "presetname")
                found = b->getButtonText();
            for (auto* child : c.getChildren()) self (self, *child);
        };
        visit (visit, parent);
        return found;
    };

    for (int command = 0; command < 3; ++command)
    {
        if (selected >= 0 && selected != command) continue;
        for (const bool destroy : { false, true })
        {
            auto owner = std::make_unique<AnamorphAudioProcessor>();
            auto& proc = *owner;
            proc.prepareToPlay (48000.0, 128);
            auto* width = proc.getAPVTS().getParameter (pid::width);
            const float initial = width->getValue();
            const auto edit = [&]
            {
                width->beginChangeGesture();
                width->setValueNotifyingHost (0.8f);
                width->endChangeGesture();
                proc.pollUndoCoalesce();
            };
            if (command == 2) proc.abSwitchTo (1);
            edit();
            if (command == 1) proc.undo();
            if (command == 2) proc.abSwitchTo (0);

            // Repeat after reconstructing the same editor on the same processor.
            // Reverse the command's state first, except A/B which already cycles.
            for (int cycle = 0; cycle < 2; ++cycle)
            {
                if (cycle != 0 && command == 0) proc.redo();
                if (cycle != 0 && command == 1) proc.undo();
                const float before = width->getValue();
                const int slotBefore = proc.abActiveSlot();
                const float expected = command == 0 ? initial : command == 1 ? 0.8f
                                                     : slotBefore == 0 ? 0.8f : initial;
                std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
                check (editor != nullptr, "154: real editor constructs for each command/cycle");
                if (editor == nullptr) continue;
                juce::Component::SafePointer<juce::AudioProcessorEditor> weak (editor.get());
                const auto beforeTitle = presetLabel (*editor);
                check (beforeTitle.has_value(), "154 control: preset display is found");

                juce::Component* target = nullptr;
                const char* tooltip = command == 0 ? "Undo" : command == 1 ? "Redo" : "A/B Compare";
                const auto visit = [&] (auto&& self, juce::Component& c) -> void
                {
                    if (auto* t = dynamic_cast<juce::TooltipClient*> (&c); t != nullptr && t->getTooltip() == tooltip)
                        target = &c;
                    for (auto* child : c.getChildren()) self (self, *child);
                };
                visit (visit, *editor);
                check (target != nullptr, "154: production command component is found");
                if (target == nullptr) { proc.editorBeingDeleted (editor.get()); editor.reset(); continue; }
                std::function<void()> click;
                if (command < 2)
                {
                    auto* button = dynamic_cast<juce::Button*> (target);
                    check (button != nullptr && button->onClick != nullptr, "154: the production button callback exists");
                    if (button != nullptr) click = button->onClick;
                }
                CloseOnWrite listener (proc, editor);
                if (destroy) width->addListener (&listener);
                if (command < 2)
                {
                    // Invoke the component-owned callback too: deleting the
                    // editor destroys its std::function while it is executing.
                    auto* button = dynamic_cast<juce::Button*> (target);
                    if (button != nullptr && button->onClick) button->onClick();
                }
                else
                {
                    const auto source = juce::Desktop::getInstance().getMainMouseSource();
                    const auto position = target->getLocalBounds().getCentre().toFloat();
                    const auto time = juce::Time::getCurrentTime();
                    target->mouseDown (juce::MouseEvent (source, position, juce::ModifierKeys::leftButtonModifier,
                        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, target, target, time, position, time, 1, false));
                }
                if (destroy) width->removeListener (&listener);
                check (! juce::exactlyEqual (before, expected), "154 control: this command must move Width");
                check (juce::exactlyEqual (width->getValue(), expected), "154: processor command finishes despite editor lifetime changes");
                if (command < 2)
                    check (command == 0 ? proc.canRedo() : proc.canUndo(), "154: the opposite history endpoint survives");
                else
                    check (proc.abActiveSlot() == 1 - slotBefore, "154: A/B switches once to its destination");
                if (destroy)
                {
                    check (listener.closes == 1 && weak == nullptr && editor == nullptr,
                           "154 control: the notification really destroyed this editor exactly once");
                    if (command < 2 && click)
                    {
                        click(); // a copied dead-editor callback must not repeat the action
                        check (juce::exactlyEqual (width->getValue(), expected), "154: dead-editor callback cannot act on the processor again");
                    }
                    editor.reset (proc.createEditor());
                    check (editor != nullptr, "154: editor reopens after the completed command");
                }
                else
                    check (weak != nullptr && listener.closes == 0, "154 control: ordinary callback keeps its editor alive");
                if (editor != nullptr)
                {
                    const auto afterTitle = presetLabel (*editor);
                    check (afterTitle.has_value() && afterTitle != beforeTitle,
                           "154: surviving or reopened editor displays the resulting preset dirty state");
                    proc.editorBeingDeleted (editor.get());
                    editor.reset();
                }
            }
        }
    }
}
