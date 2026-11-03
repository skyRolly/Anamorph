#pragma once

#include <memory>
#include <vector>

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

// Execute A -> B -> C -> B on ONE processor per callback/destruction case.
// Every adoption must reinstall displaced sound, complete metadata, and survive
// teardown; immediate oracles precede serialization, which can itself adopt.
static void testEditorRestoreAdoptionLifetimes (int selected = -1)
{
    using namespace editor_restore_lifetime_tests;
    std::printf ("State test 157: same-processor A -> B -> C -> B restore adoption and lifetime\n");
    check (selected >= -1 && selected <= 1, "157: selector is valid, never an empty passing suite");
    if (selected < -1 || selected > 1) return;

    // In-memory popup placement metadata only; no native/host acceptance.
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

    const auto sessionA = d2::author ("NR028-A", 0.12f, 0.88f, 0, 1);
    const auto sessionB = d2::author ("NR028-B", 0.34f, 0.66f, 1, 2);
    const auto sessionC = d2::author ("NR028-C", 0.52f, 0.24f, 0, 3);
    check (sessionA.blob != sessionB.blob && sessionB.blob != sessionC.blob,
           "157: A/B/C sessions have distinct sound, identity, slot and Settings");
    int latencyB = 0, latencyC = 0;
    {
        auto [truthOwner, truth] = heapProcessorFixture();
        truth.prepareToPlay (48000.0, 512);
        d2::restoreFrom (truth, sessionB.blob);
        latencyB = truth.getLatencySamples();
        d2::restoreFrom (truth, sessionC.blob);
        latencyC = truth.getLatencySamples();
    }

    for (int path = 0; path < 2; ++path)
    {
        if (selected >= 0 && selected != path) continue;
        for (const bool destroy : { false, true })
        {
            auto [processorOwner, processor] = heapProcessorFixture();
            processor.prepareToPlay (48000.0, 512);
            d2::restoreFrom (processor, sessionA.blob);
            check (d2::View::of (processor).matches (sessionA), "157 cycle: processor starts in A");
            checkNear (rawOf (processor, "width"), sessionA.widthA, 1.0e-6, "157 cycle: A sound is installed");
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
            const auto dispose = [&]
            {
                if (editor != nullptr) { processor.editorBeingDeleted (editor.get()); editor.reset(); }
            };
            std::vector<float> firstBParameters;
            int completedAdoptions = 0;
            const d2::Session* previous = &sessionA;
            for (int leg = 0; leg < 3; ++leg)
            {
                const auto& target = leg == 1 ? sessionC : sessionB;
                auto* concrete = dynamic_cast<AnamorphAudioProcessorEditor*> (editor.get());
                check (concrete != nullptr && editor->getPeer() == nullptr,
                       "157: real editor reopens headlessly on the same processor");
                if (concrete == nullptr) break;
                auto* width = processor.getAPVTS().getParameter (pid::width);
                auto* presetButton = find<juce::Button> (*editor, [] (const auto& b)
                    { return b.getComponentID() == "presetname"; });
                auto* undo = find<juce::Button> (*editor, [] (auto& b)
                    { return b.getTooltip() == "Undo"; });
                check (width != nullptr && presetButton != nullptr && presetButton->onClick != nullptr && undo != nullptr,
                       "157: actual parameter, callback and timer indicator exist");
                if (width == nullptr || presetButton == nullptr || ! presetButton->onClick || undo == nullptr) break;

                // Preserve the existing pacing and handoff deadlines. Do not pump
                // timers once the restore has become pending.
                std::this_thread::sleep_for (std::chrono::milliseconds (60));
                juce::Timer::callPendingTimersSynchronously();
                RestoreHandoff handoff (processor, target.blob);
                const bool reached = handoff.waitUntilBlocked();
                check (reached, "157: every host restore pauses after sound install and before publication");
                if (! reached) { handoff.finish(); break; }
                check (d2::View::of (processor).matches (*previous), "157: previous metadata owns each handoff window");
                processor.abSwitchTo (1 - processor.abActiveSlot());
                const float targetWidth = target.active == 0 ? target.widthA : target.widthB;
                check (std::abs (rawOf (processor, "width") - targetWidth) > 1.0e-3f,
                       "157 non-vacuity: each leg displaces sound that adoption must reinstall");
                handoff.finish();

                CloseOnRestore listener (processor, editor, destroy);
                width->addListener (&listener);
                undo->setEnabled (true);
                const int modalBefore = juce::ModalComponentManager::getInstance()->getNumModalComponents();
                if (path == 0)
                    (concrete->*getTimerEntry (TimerEntry{}))();
                else
                    presetButton->onClick();
                width->removeListener (&listener);

                check (listener.notifications > 0, "157: every adoption reaches its synchronous parameter listener");
                check (listener.offMessageThread == 0, "157: notifications stay on the message thread");
                check (listener.closes == (destroy ? 1 : 0) && (editor == nullptr) == destroy,
                       "157: each leg preserves the selected live/destructive control");
                check (d2::View::of (processor).matches (target), "157: each adoption completes identity and active slot");
                check (processor.getPresets().baseline() == "d2-baseline-" + target.name,
                       "157: each adoption completes its preset baseline");
                checkNear (rawOf (processor, "width"), targetWidth, 1.0e-6, "157: restored sound survives callback teardown");
                check (processor.getInternal().oversampleIndex() == target.oversampleId - 1
                           && processor.getLatencySamples() == (leg == 1 ? latencyC : latencyB),
                       "157: repeated adoption restores Settings and latency");
                check (! processor.canUndo() && ! processor.canRedo() && ! anamorph::param::insideDispatch(),
                       "157: restoration clears history and balances dispatch");
                if (listener.notifications > 0) ++completedAdoptions;
                if (! destroy && editor != nullptr)
                {
                    if (path == 0)
                        check (! undo->isEnabled(), "157 live timer: continuation refreshes the real Undo button");
                    else
                        check (juce::ModalComponentManager::getInstance()->getNumModalComponents() > modalBefore,
                               "157 live preset: continuation opens the production popup");
                }
                if (path == 1) juce::PopupMenu::dismissAllActiveMenus();

                // Do not let serialization rescue a skipped callback above.
                const auto xml = BlobCodec::unwrap (d2::saveOf (processor));
                check (xml != nullptr, "157: adopted state serializes on every leg");
                if (xml != nullptr)
                {
                    const auto ab = juce::ValueTree::fromXml (*xml).getChildWithName ("AB");
                    const auto slotA = juce::ValueTree::fromXml (ab["slotAParams"].toString());
                    const auto slotB = juce::ValueTree::fromXml (ab["slotBParams"].toString());
                    check (ab.isValid() && slotA.isValid() && slotB.isValid(), "157: both restored A/B payloads stay valid");
                    checkNear ((double) slotA.getChildWithProperty ("id", "width")["value"], width->convertFrom0to1 (target.widthA), 1.0e-6,
                               "157: serialized A slot belongs to the target session");
                    checkNear ((double) slotB.getChildWithProperty ("id", "width")["value"], width->convertFrom0to1 (target.widthB), 1.0e-6,
                               "157: serialized B slot belongs to the target session");
                }
                if (leg == 0)
                    for (auto* parameter : processor.getParameters()) firstBParameters.push_back (parameter->getValue());
                std::printf ("  State157 path=%s editor=%s leg=%d target=%s notifications=%d closes=%d width=%.4f\n",
                             path == 0 ? "timer" : "preset", destroy ? "destroy" : "live", leg,
                             target.name.toRawUTF8(), listener.notifications, listener.closes, rawOf (processor, "width"));
                dispose();
                editor.reset (processor.createEditor());
                auto* attachedWidth = editor != nullptr ? find<juce::Slider> (*editor, [] (const auto& slider)
                    { return slider.getTitle() == "Width"; }) : nullptr;
                check (attachedWidth != nullptr, "157: editor reopens after every adoption on this processor");
                if (attachedWidth != nullptr)
                    checkNear (attachedWidth->getValue(), width->convertFrom0to1 (targetWidth), 1.0e-6, "157: reopened attachment reads the target sound");
                previous = &target;
            }
            check (completedAdoptions == 3, "157 cycle: all three B/C/B adoptions actually execute");
            check (d2::View::of (processor).matches (sessionB), "157 cycle: final return restores B metadata after C");
            check (firstBParameters.size() == static_cast<size_t> (processor.getParameters().size()),
                   "157 cycle: first B snapshot covers the parameter registry");
            if (firstBParameters.size() == static_cast<size_t> (processor.getParameters().size()))
                for (int i = 0; i < processor.getParameters().size(); ++i)
                    check (juce::exactlyEqual (processor.getParameters()[i]->getValue(), firstBParameters[static_cast<size_t> (i)]),
                           "157 cycle: final B restores every exact normalized value of first B");
            dispose();
        }
    }
}
