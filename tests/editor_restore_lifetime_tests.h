#pragma once

#include <memory>

namespace editor_restore_lifetime_tests
{
// Deliver the real private timer override directly, without a timer-clock race.
struct TimerEntry
{
    using type = void (AnamorphAudioProcessorEditor::*)();
    friend type getTimerEntry (TimerEntry);
};
template <typename Tag, typename Tag::type Member>
struct AccessTimer { friend typename Tag::type getTimerEntry (Tag) { return Member; } };
template struct AccessTimer<TimerEntry, &AnamorphAudioProcessorEditor::timerCallback>;

template <typename T, typename Predicate>
static T* find (juce::Component& c, const Predicate& matches)
{
    if (auto* t = dynamic_cast<T*> (&c); t != nullptr && matches (*t)) return t;
    for (auto* child : c.getChildren())
        if (auto* t = find<T> (*child, matches)) return t;
    return nullptr;
}

struct CloseOnRestore final : juce::AudioProcessorParameter::Listener
{
    AnamorphAudioProcessor& processor;
    std::unique_ptr<juce::AudioProcessorEditor>& editor;
    const bool destroy;
    int notifications = 0, closes = 0, offMessageThread = 0;

    CloseOnRestore (AnamorphAudioProcessor& p,
                    std::unique_ptr<juce::AudioProcessorEditor>& e,
                    bool shouldDestroy)
        : processor (p), editor (e), destroy (shouldDestroy) {}

    void parameterValueChanged (int, float) override
    {
        ++notifications;
        if (! juce::MessageManager::existsAndIsCurrentThread())
        {
            ++offMessageThread;
            return;
        }
        if (destroy && closes == 0 && editor != nullptr)
        {
            ++closes;
            processor.editorBeingDeleted (editor.get());
            editor.reset();
        }
    }

    void parameterGestureChanged (int, bool) override {}
};

struct RestoreHandoff
{
    AnamorphAudioProcessor& processor;
    const juce::MemoryBlock& blob;
    std::mutex mutex;
    std::condition_variable cv;
    bool blocked = false, release = false;
    std::thread host;

    RestoreHandoff (AnamorphAudioProcessor& p, const juce::MemoryBlock& b)
        : processor (p), blob (b)
    {
        processor.seams.beforeRestorePut = [this]
        {
            std::unique_lock lock (mutex);
            blocked = true;
            cv.notify_all();
            cv.wait_for (lock, std::chrono::seconds (5), [this] { return release; });
        };
        host = std::thread ([this] { d2::restoreFrom (processor, blob); });
    }

    bool waitUntilBlocked()
    {
        std::unique_lock lock (mutex);
        return cv.wait_for (lock, std::chrono::seconds (2), [this] { return blocked; });
    }

    void finish()
    {
        {
            std::lock_guard lock (mutex);
            release = true;
        }
        cv.notify_all();
        if (host.joinable()) host.join();
        processor.seams.beforeRestorePut = nullptr;
    }

    ~RestoreHandoff() { finish(); }
};
} // namespace editor_restore_lifetime_tests

// Actual editor timer and preset-button callbacks. The off-thread restore is
// held after installing its sound; an A/B action changes that sound before the
// restore becomes visible. Adoption then reinstalls the restored sound and
// synchronously notifies the parameter listener that destroys the editor.
static void testEditorRestoreAdoptionLifetimes (int selected = -1)
{
    using namespace editor_restore_lifetime_tests;
    std::printf ("State test 157: restore adoption survives synchronous editor destruction\n");
    check (selected >= -1 && selected <= 1, "157: selector is valid, never an empty passing suite");
    if (selected < -1 || selected > 1) return;

    // PopupMenu's placement code assumes at least one display. The production
    // preset callback is useful to exercise headlessly, but this test process has
    // no native peer or display server. Supply only JUCE's in-memory placement
    // metadata so the callback can reach its lifetime behavior; this creates no
    // window and is not visual/native acceptance evidence.
    auto& displays = const_cast<juce::Displays&> (juce::Desktop::getInstance().getDisplays());
    if (displays.displays.isEmpty())
    {
        juce::Displays::Display testDisplay {};
        testDisplay.isMain = true;
        testDisplay.logicalBounds = { 0.0f, 0.0f, 1920.0f, 1080.0f };
        testDisplay.userBounds = testDisplay.logicalBounds;
        testDisplay.physicalBounds = { 0, 0, 1920, 1080 };
        testDisplay.scale = 1.0;
        testDisplay.dpi = 96.0;
        displays.displays.add (testDisplay);
    }

    const auto outgoing = d2::author ("NR028-outgoing", 0.12f, 0.88f, 0, 1);
    const auto restored = d2::author ("NR028-restored", 0.34f, 0.66f, 1, 2);
    check (outgoing.blob != restored.blob, "157: restore sessions are distinguishable");

    for (int path = 0; path < 2; ++path)
    {
        if (selected >= 0 && selected != path) continue;
        for (const bool destroy : { false, true })
        {
            auto [processorOwner, processor] = heapProcessorFixture();
            processor.prepareToPlay (48000.0, 512);
            d2::restoreFrom (processor, outgoing.blob);

            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
            auto* concrete = dynamic_cast<AnamorphAudioProcessorEditor*> (editor.get());
            check (concrete != nullptr && editor->getPeer() == nullptr,
                   "157: actual editor constructs headlessly");
            if (concrete == nullptr)
            {
                if (editor != nullptr) { processor.editorBeingDeleted (editor.get()); editor.reset(); }
                continue;
            }

            auto* width = processor.getAPVTS().getParameter (pid::width);
            auto* presetButton = find<juce::Button> (*editor, [] (const auto& b)
                { return b.getComponentID() == "presetname"; });
            check (width != nullptr, "157: width parameter exists");
            check (presetButton != nullptr && presetButton->onClick != nullptr,
                   "157: actual preset-name button callback exists");
            if (width == nullptr || (path == 1 && (presetButton == nullptr || ! presetButton->onClick)))
            {
                processor.editorBeingDeleted (editor.get());
                editor.reset();
                continue;
            }

            // Run any already-due processor/editor timers before arming the
            // pending restore. The timer path below is invoked directly through
            // the real private override, so the processor's own 20 Hz timer
            // cannot consume the restore first.
            std::this_thread::sleep_for (std::chrono::milliseconds (60));
            juce::Timer::callPendingTimersSynchronously();

            RestoreHandoff handoff (processor, restored.blob);
            const bool reached = handoff.waitUntilBlocked();
            check (reached, "157: host restore is paused after its sound install and before handoff");
            if (! reached)
            {
                handoff.finish();
                processor.editorBeingDeleted (editor.get());
                editor.reset();
                continue;
            }

            check (processor.abActiveSlot() == outgoing.active,
                   "157: outgoing metadata remains authoritative inside the handoff window");
            processor.abSwitchTo (1);
            const float restoredWidth = restored.widthB;
            const float displacedWidth = rawOf (processor, "width");
            check (std::abs (displacedWidth - restoredWidth) > 1.0e-3f,
                   "157 non-vacuity: A/B changes the live sound that adoption must reinstall");
            handoff.finish();

            CloseOnRestore listener (processor, editor, destroy);
            width->addListener (&listener);

            if (path == 0)
                (concrete->*getTimerEntry (TimerEntry{}))();
            else
                presetButton->onClick();

            width->removeListener (&listener);

            check (listener.notifications > 0,
                   "157: adoption synchronously notifies the changed parameter");
            check (listener.offMessageThread == 0,
                   "157: the destructive notification runs on the message thread");
            check (listener.closes == (destroy ? 1 : 0) && (editor == nullptr) == destroy,
                   "157: listener destruction control matches the selected case");
            check (processor.abActiveSlot() == restored.active
                       && processor.getPresets().currentName() == restored.name,
                   "157: processor restore completes independently of editor lifetime");
            checkNear (rawOf (processor, "width"), restoredWidth, 1.0e-6,
                       "157: restored sound survives the editor callback");

            if (editor != nullptr)
            {
                if (path == 1) juce::PopupMenu::dismissAllActiveMenus();
                processor.editorBeingDeleted (editor.get());
                editor.reset();
            }
        }
    }
}
