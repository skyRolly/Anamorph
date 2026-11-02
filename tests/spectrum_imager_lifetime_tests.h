#pragma once

#include <array>
#include <memory>

namespace spectrum_imager_lifetime_tests
{
template <typename T>
static T* findComponent (juce::Component& component)
{
    if (auto* found = dynamic_cast<T*> (&component); found != nullptr)
        return found;
    for (auto* child : component.getChildren())
        if (auto* found = findComponent<T> (*child); found != nullptr)
            return found;
    return nullptr;
}

enum class DestroyAt { gestureBegin, valueWrite, gestureEnd, splitGestureBegin,
                       addBandsBegin, addBandsValue, bandMoveBegin };

struct DestroyEditorOnParameter final : juce::AudioProcessorParameter::Listener
{
    AnamorphAudioProcessor& processor;
    std::unique_ptr<juce::AudioProcessorEditor>& editor;
    DestroyAt target;
    bool destroy;
    int events = 0;
    int closes = 0;
    int valueEvents = 0;
    int gestureBegins = 0;
    int gestureEnds = 0;

    DestroyEditorOnParameter (AnamorphAudioProcessor& p,
                              std::unique_ptr<juce::AudioProcessorEditor>& e,
                              DestroyAt t, bool shouldDestroy)
        : processor (p), editor (e), target (t), destroy (shouldDestroy) {}

    void fire()
    {
        ++events;
        if (destroy && closes == 0 && editor != nullptr)
        {
            ++closes;
            processor.editorBeingDeleted (editor.get());
            editor.reset();
        }
    }

    void parameterValueChanged (int, float) override
    {
        ++valueEvents;
        if (target == DestroyAt::valueWrite || target == DestroyAt::addBandsValue) fire();
    }

    void parameterGestureChanged (int, bool starting) override
    {
        if (starting) ++gestureBegins;
        else ++gestureEnds;
        if ((starting && (target == DestroyAt::gestureBegin
                          || target == DestroyAt::splitGestureBegin
                          || target == DestroyAt::addBandsBegin
                          || target == DestroyAt::bandMoveBegin))
            || (! starting && target == DestroyAt::gestureEnd))
            fire();
    }
};
} // namespace spectrum_imager_lifetime_tests

// Drive the production SpectrumImager mouse callbacks with an actual editor and
// width parameter. A synchronous parameter listener destroys the editor during
// beginChangeGesture, setValueNotifyingHost, or endChangeGesture. Live-editor
// cases distinguish a valid drag from a vacuous callback path.
static void testSpectrumImagerCallbackLifetimes (int selected = -1)
{
    using namespace spectrum_imager_lifetime_tests;
    std::printf ("NR-029 probe: SpectrumImager survives synchronous editor destruction\n");
    check (selected >= -1 && selected <= 6, "NR029: selector is valid");
    if (selected < -1 || selected > 6) return;

    for (int path = 0; path < 7; ++path)
    {
        if (selected >= 0 && selected != path) continue;
        const auto target = static_cast<DestroyAt> (path);
        for (const bool destroy : { false, true })
        {
            auto [processorOwner, processor] = heapProcessorFixture();
            processor.prepareToPlay (48000.0, 512);
            auto& apvts = processor.getAPVTS();
            auto setPlain = [&] (const char* id, float value)
            {
                auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id));
                if (p != nullptr) p->setValueNotifyingHost (p->convertTo0to1 (value));
                return p;
            };
            setPlain (pid::advancedMode, 1.0f);
            setPlain (pid::mbEnable, 1.0f);
            setPlain (pid::mbBands, (target == DestroyAt::addBandsBegin
                                     || target == DestroyAt::addBandsValue) ? 3.0f : 4.0f);
            auto* width = setPlain (pid::mbWidthMid, 1.0f);
            auto* bandsParameter = dynamic_cast<juce::RangedAudioParameter*> (
                apvts.getParameter (pid::mbBands));
            auto* soloParameter = dynamic_cast<juce::RangedAudioParameter*> (
                apvts.getParameter (pid::mbSolo));
            std::array<float, 3> originalSplits {};
            std::array<float, 4> originalWidths {};
            for (int k = 0; k < 3; ++k)
                if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (
                        apvts.getParameter (k == 0 ? pid::mbFreqLow
                                                   : k == 1 ? pid::mbFreqMid : pid::mbFreqHigh)))
                    originalSplits[(size_t) k] = parameter->getValue();
            for (int k = 0; k < 4; ++k)
                if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (
                        apvts.getParameter (k == 0 ? pid::mbWidthLow
                                                   : k == 1 ? pid::mbWidthMid
                                                   : k == 2 ? pid::mbWidthHiMid : pid::mbWidthHigh)))
                    originalWidths[(size_t) k] = parameter->getValue();
            const float originalSolo = soloParameter != nullptr ? soloParameter->getValue() : 0.0f;

            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
            auto* imager = editor != nullptr
                         ? findComponent<anamorph::gui::SpectrumImager> (*editor) : nullptr;
            check (width != nullptr && imager != nullptr && imager->getWidth() > 300,
                   "NR029: actual laid-out imager and width parameter exist");
            if (width == nullptr || imager == nullptr || imager->getWidth() <= 300)
            {
                if (editor != nullptr)
                {
                    processor.editorBeingDeleted (editor.get());
                    editor.reset();
                }
                continue;
            }

            const auto source = juce::Desktop::getInstance().getMainMouseSource();
            auto event = [&] (float x, float y, float downX, float downY, bool dragged)
            {
                return juce::MouseEvent (source, { x, y }, juce::ModifierKeys::leftButtonModifier,
                                         1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                         imager, imager, juce::Time::getCurrentTime(),
                                         { downX, downY }, juce::Time::getCurrentTime(), 1, dragged);
            };
            const float w = (float) imager->getWidth();
            const float h = (float) imager->getHeight();
            const float laneY = 0.5f * h;
            float splitX = -1.0f;
            for (float x = 2.0f; x < w - 2.0f; x += 1.0f)
            {
                imager->mouseMove (event (x, laneY, x, laneY, false));
                if (imager->getTooltip() == "Drag to change the split frequency")
                { splitX = x; break; }
            }
            const float widthX = splitX + 40.0f;
            float widthY = -1.0f;
            for (float y = 2.0f; splitX >= 0.0f && y < h - 2.0f; y += 1.0f)
            {
                imager->mouseMove (event (widthX, y, widthX, y, false));
                if (imager->getTooltip() == "Band width") { widthY = y; break; }
            }
            check (splitX >= 0.0f && widthY >= 0.0f,
                   "NR029: production hit-testing finds a split and width line");
            if (splitX < 0.0f || widthY < 0.0f)
            {
                processor.editorBeingDeleted (editor.get());
                editor.reset();
                continue;
            }

            auto* targetParameter = width;
            float pressX = widthX;
            float pressY = widthY;
            if (target == DestroyAt::splitGestureBegin)
            {
                targetParameter = dynamic_cast<juce::RangedAudioParameter*> (
                    apvts.getParameter (pid::mbFreqLow));
                pressX = splitX;
                pressY = laneY;
            }
            else if (target == DestroyAt::addBandsBegin || target == DestroyAt::addBandsValue)
            {
                targetParameter = dynamic_cast<juce::RangedAudioParameter*> (
                    apvts.getParameter (pid::mbBands));
                pressX = pressY = -1.0f;
                for (float y = 2.0f; y < h - 2.0f && pressX < 0.0f; y += 1.0f)
                    for (float x = 2.0f; x < w - 2.0f; x += 1.0f)
                    {
                        imager->mouseMove (event (x, y, x, y, false));
                        if (imager->getTooltip() == "Click to add a band split")
                        { pressX = x; pressY = y; break; }
                    }
                check (pressX >= 0.0f, "NR029: production hit-testing finds the add affordance");
                if (pressX < 0.0f)
                {
                    processor.editorBeingDeleted (editor.get());
                    editor.reset();
                    continue;
                }
            }
            else if (target == DestroyAt::bandMoveBegin)
            {
                targetParameter = dynamic_cast<juce::RangedAudioParameter*> (
                    apvts.getParameter (pid::mbFreqLow));
                float centers[3] {};
                int centerCount = 0;
                bool inHandle = false;
                float groupStart = 0.0f;
                for (float x = 2.0f; x < w - 2.0f; x += 1.0f)
                {
                    imager->mouseMove (event (x, laneY, x, laneY, false));
                    const bool overHandle = imager->getTooltip()
                                          == "Drag to change the split frequency";
                    if (overHandle && ! inHandle) groupStart = x;
                    if (! overHandle && inHandle && centerCount < 3)
                        centers[centerCount++] = 0.5f * (groupStart + x - 1.0f);
                    inHandle = overHandle;
                }
                if (inHandle && centerCount < 3)
                    centers[centerCount++] = 0.5f * (groupStart + w - 3.0f);
                check (centerCount >= 2,
                       "NR029: production hit-testing finds two crossover handles");
                if (centerCount >= 2)
                {
                    pressX = 0.5f * (centers[0] + centers[1]);
                    pressY = 10.0f;
                    imager->mouseMove (event (pressX, pressY, pressX, pressY, false));
                    check (imager->getTooltip() == "Solo this band",
                           "NR029: production hit-testing finds the interior band's solo target");
                }
                if (centerCount < 2 || imager->getTooltip() != "Solo this band")
                {
                    processor.editorBeingDeleted (editor.get());
                    editor.reset();
                    continue;
                }
            }
            check (targetParameter != nullptr, "NR029: targeted gesture parameter exists");
            if (targetParameter == nullptr)
            {
                processor.editorBeingDeleted (editor.get());
                editor.reset();
                continue;
            }
            DestroyEditorOnParameter listener (processor, editor, target, destroy);
            if (target == DestroyAt::gestureBegin || target == DestroyAt::splitGestureBegin
                || target == DestroyAt::addBandsBegin || target == DestroyAt::addBandsValue
                || target == DestroyAt::bandMoveBegin)
                targetParameter->addListener (&listener);
            imager->mouseDown (event (pressX, pressY, pressX, pressY, false));
            if (target != DestroyAt::gestureBegin && target != DestroyAt::splitGestureBegin
                && target != DestroyAt::addBandsBegin && target != DestroyAt::addBandsValue
                && target != DestroyAt::bandMoveBegin)
                targetParameter->addListener (&listener);

            if (editor != nullptr && target == DestroyAt::valueWrite)
                imager->mouseDrag (event (pressX, pressY + 10.0f, pressX, pressY, true));
            else if (editor != nullptr && target == DestroyAt::bandMoveBegin)
                imager->mouseDrag (event (pressX + 5.0f, pressY, pressX, pressY, true));
            else if (editor != nullptr && target == DestroyAt::gestureEnd)
                imager->mouseUp (event (pressX, pressY, pressX, pressY, false));

            // Close the live controls before checking the begin/end pair. A
            // destruction case has no editor left to deliver this release.
            if (editor != nullptr && target != DestroyAt::gestureEnd)
                imager->mouseUp (event (pressX, pressY, pressX, pressY, false));

            check (listener.events > 0, "NR029: targeted parameter callback was reached");
            check (listener.closes == (destroy ? 1 : 0) && (editor == nullptr) == destroy,
                   "NR029: synchronous destruction control matches the selected case");
            if (target == DestroyAt::gestureBegin || target == DestroyAt::splitGestureBegin
                || target == DestroyAt::addBandsBegin || target == DestroyAt::addBandsValue
                || target == DestroyAt::bandMoveBegin)
                check (listener.gestureBegins == listener.gestureEnds,
                       "NR029: a gesture whose begin destroys the editor is still balanced");
            if (target == DestroyAt::valueWrite)
                check (listener.gestureEnds == 1,
                       "NR029: destroying during a value write still closes its active gesture");
            if (editor != nullptr)
            {
                processor.editorBeingDeleted (editor.get());
                editor.reset();
            }
            targetParameter->removeListener (&listener);

            if (destroy && (target == DestroyAt::gestureBegin
                            || target == DestroyAt::splitGestureBegin
                            || target == DestroyAt::addBandsBegin
                            || target == DestroyAt::addBandsValue
                            || target == DestroyAt::bandMoveBegin))
            {
                processor.pollUndoCoalesce();
                if (target == DestroyAt::addBandsBegin || target == DestroyAt::addBandsValue)
                {
                    // The topology add may already have made a bounded set of
                    // earlier stores before the Bands gesture begins. They
                    // must remain one undoable transaction even if the editor
                    // dies at that final begin dispatch.
                    check (processor.canUndo(),
                           "NR029: stores before a destroyed add remain one undo transaction");
                    if (processor.canUndo()) processor.undo();
                    processor.pollUndoCoalesce();
                    bool restored = bandsParameter != nullptr && soloParameter != nullptr
                                 && juce::exactlyEqual (bandsParameter->getValue(),
                                                        bandsParameter->convertTo0to1 (3.0f))
                                 && juce::exactlyEqual (soloParameter->getValue(), originalSolo);
                    for (int k = 0; k < 3; ++k)
                    {
                        const char* id = k == 0 ? pid::mbFreqLow : k == 1 ? pid::mbFreqMid : pid::mbFreqHigh;
                        auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id));
                        restored = restored && parameter != nullptr
                                && juce::exactlyEqual (parameter->getValue(), originalSplits[(size_t) k]);
                    }
                    for (int k = 0; k < 4; ++k)
                    {
                        const char* id = k == 0 ? pid::mbWidthLow : k == 1 ? pid::mbWidthMid
                                         : k == 2 ? pid::mbWidthHiMid : pid::mbWidthHigh;
                        auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id));
                        restored = restored && parameter != nullptr
                                && juce::exactlyEqual (parameter->getValue(), originalWidths[(size_t) k]);
                    }
                    check (restored,
                           "NR029: Undo restores the exact pre-add topology after editor destruction");
                }
                else
                {
                    // Listener event counts alone are insufficient here:
                    // JUCE walks listeners in reverse registration order, so
                    // the processor sees the outer begin only after this
                    // listener destroys the editor. A nested close from the
                    // destructor would be ignored by the processor.
                    check (! processor.canUndo(),
                           "NR029: empty destroyed press does not create a phantom Undo step");
                }
                const float before = width->convertFrom0to1 (width->getValue());
                const float after = before < 1.8f ? before + 0.15f : before - 0.15f;
                width->beginChangeGesture();
                width->setValueNotifyingHost (width->convertTo0to1 (after));
                width->endChangeGesture();
                processor.pollUndoCoalesce();
                check (processor.canUndo(),
                       "NR029: a later bracketed edit remains undoable after destroyed begin");
            }
        }
    }
}
