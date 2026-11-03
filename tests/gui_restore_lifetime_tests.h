#pragma once

#include <functional>
#include <memory>
#include <thread>

namespace gui_restore_lifetime_tests
{
// Explicit-instantiation access exemption: call the real private Timer override
// without changing the shipping class definition, its layout, or JUCE headers.
struct TimerEntry
{
    using type = void (AnamorphAudioProcessorEditor::*)();
    friend type getTimerEntry (TimerEntry);
};
template <typename Tag, typename Tag::type Member>
struct AccessTimer { friend typename Tag::type getTimerEntry (Tag) { return Member; } };
template struct AccessTimer<TimerEntry, &AnamorphAudioProcessorEditor::timerCallback>;

struct CloseOnLatency final : juce::AudioProcessorListener
{
    AnamorphAudioProcessor& proc;
    std::unique_ptr<juce::AudioProcessorEditor>& editor;
    const bool destroy;
    int callbacks = 0, closes = 0;
    bool onMessageThread = true;
    CloseOnLatency (AnamorphAudioProcessor& p, std::unique_ptr<juce::AudioProcessorEditor>& e, bool d)
        : proc (p), editor (e), destroy (d) {}
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& details) override
    {
        if (! details.latencyChanged) return;
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

static juce::Button* findButton (juce::Component& root, const juce::String& id, const juce::String& tip = {})
{
    if (auto* b = dynamic_cast<juce::Button*> (&root); b != nullptr
        && (id.isNotEmpty() ? b->getComponentID() == id : b->getTooltip() == tip)) return b;
    for (auto* child : root.getChildren())
        if (auto* b = findButton (*child, id, tip)) return b;
    return nullptr;
}

static juce::Slider* findWidth (juce::Component& root)
{
    if (auto* s = dynamic_cast<juce::Slider*> (&root); s != nullptr && s->getTitle() == "Width") return s;
    for (auto* child : root.getChildren())
        if (auto* s = findWidth (*child)) return s;
    return nullptr;
}
} // namespace gui_restore_lifetime_tests

// Default: the three display-independent cases. The positive popup case is an
// explicit separate CLI/CI control requiring a display (Linux CI uses Xvfb).
// These are delivered component calls, not manual host/visual acceptance.
static void testGuiRestoreLifetimes (int selected = -1)
{
    using namespace gui_restore_lifetime_tests;
    std::printf ("State test 155: preset-menu and timer restore callouts survive editor destruction\n");
    check (selected >= -1 && selected <= 3, "155: selection is valid, never an empty passing suite");
    if (selected < -1 || selected > 3) return;
    if (selected == 3)
    {
        const bool display = ! juce::Desktop::getInstance().getDisplays().displays.isEmpty();
        check (display, "155 popup control requires a real or virtual display; no silent skip");
        if (! display) return;
    }
    for (int action = 0; action <= 3; ++action)
    {
        if (selected == -1 ? action == 3 : selected != action) continue;
        const bool menu = action == 0 || action == 3;
        const bool destroy = action <= 1;
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            auto author = std::make_unique<AnamorphAudioProcessor>();
            author->getAPVTS().getParameter (pid::width)->setValueNotifyingHost (0.8f);
            author->getInternal().oversampleValue().setValue (2);
            author->prepareToPlay (48000.0, 128);
            const int expectedLatency = author->getLatencySamples();
            juce::MemoryBlock incoming;
            author->getStateInformation (incoming);

            auto owner = std::make_unique<AnamorphAudioProcessor>();
            auto& proc = *owner;
            proc.prepareToPlay (48000.0, 128);
            juce::MemoryBlock outgoing;
            proc.getStateInformation (outgoing);
            std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
            check (editor != nullptr, "155: real editor constructs before the pending restore");
            if (editor == nullptr) continue;
            const auto dispose = [&]
            { if (editor != nullptr) { proc.editorBeingDeleted (editor.get()); editor.reset(); } };
            auto* preset = findButton (*editor, "presetname");
            auto* undo = findButton (*editor, {}, "Undo");
            check (preset != nullptr && preset->onClick != nullptr && undo != nullptr,
                   "155: actual component-owned menu callback and undo indicator exist");
            if (preset == nullptr || ! preset->onClick || undo == nullptr) { dispose(); continue; }
            const std::function<void()> staleClick = preset->onClick;
            const juce::Component::SafePointer<juce::AudioProcessorEditor> weak (editor.get());
            CloseOnLatency listener (proc, editor, destroy);
            proc.addListener (&listener);
            std::thread host ([&] { proc.setStateInformation (incoming.getData(), static_cast<int> (incoming.getSize())); });
            host.join(); // publish off-message-thread; do not pump a GUI loop before the action
            check (expectedLatency > 0 && proc.getLatencySamples() == 0 && listener.callbacks == 0,
                   "155 control: a real pending restore must still need a latency notification");
            check (undo->isEnabled(), "155 control: the timer has a stale enabled Undo indicator to refresh");
            const int modalBefore = juce::ModalComponentManager::getInstance()->getNumModalComponents();
            if (menu)
                preset->onClick(); // editor destruction also destroys this running std::function
            else
                (static_cast<AnamorphAudioProcessorEditor*> (editor.get())->*getTimerEntry (TimerEntry{}))();

            check (listener.callbacks == 1 && listener.onMessageThread,
                   "155: real adoption synchronously notifies latency exactly once on the message thread");
            check (proc.getLatencySamples() == expectedLatency
                       && proc.getInternal().oversampleIndex() == 1,
                   "155: the complete restore/latency transition finishes independently of the editor");
            check (juce::exactlyEqual (proc.getAPVTS().getParameter (pid::width)->getValue(), 0.8f),
                   "155: the restored sound survives callback teardown");
            check (! anamorph::param::insideDispatch(), "155: no parameter-dispatch scope is stranded");
            if (destroy)
            {
                check (listener.closes == 1 && weak == nullptr && editor == nullptr,
                       "155 control: this editor really died inside the latency callback");
                check (juce::ModalComponentManager::getInstance()->getNumModalComponents() == modalBefore,
                       "155: a dead editor never opens the pending menu");
            }
            else
            {
                check (listener.closes == 0 && weak != nullptr && editor != nullptr,
                       "155 control: the ordinary action retains its editor");
                if (menu)
                {
                    check (juce::ModalComponentManager::getInstance()->getNumModalComponents() > modalBefore,
                           "155 popup control: surviving adoption still opens the production popup");
                    juce::PopupMenu::dismissAllActiveMenus();
                }
                else
                    check (editor != nullptr && ! undo->isEnabled() && ! proc.canUndo(),
                           "155 timer control: surviving adoption still refreshes the actual Undo button");
            }
            proc.removeListener (&listener);
            dispose();
            editor.reset (proc.createEditor());
            check (editor != nullptr, "155: a fresh editor reopens on the completed restore");
            if (editor != nullptr)
            {
                auto* width = findWidth (*editor);
                check (width != nullptr && juce::exactlyEqual (static_cast<float> (width->getValue()),
                                       proc.getAPVTS().getRawParameterValue (pid::width)->load()),
                       "155: the reopened Width attachment displays the current restored sound");
                dispose();
            }
            if (menu)
            {
                // Make a stale replay observable: an Off restore is waiting, but
                // a copied callback belonging to a dead editor may not adopt it.
                std::thread nextHost ([&] { proc.setStateInformation (outgoing.getData(), static_cast<int> (outgoing.getSize())); });
                nextHost.join();
                check (proc.getLatencySamples() == expectedLatency, "155 stale control: the second restore remains pending");
                staleClick();
                check (proc.getLatencySamples() == expectedLatency,
                       "155: a copied dead-editor menu callback cannot adopt another restore");
                proc.adoptPendingHostState (false);
                check (proc.getLatencySamples() == 0, "155 stale control: a live processor action can still adopt that restore");
            }
        }
    }
}
