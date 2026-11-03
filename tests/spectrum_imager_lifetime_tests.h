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
                       addBandsBegin, addBandsValue, bandMoveBegin,
                       resetWidthValue, resetWidthBegin,
                       resetSplitValue, resetSplitBegin, resetSplitEnd,
                       soloMaskValue, soloMaskBegin, soloMaskEnd,
                       bandMoveEndLeft, bandMoveEndRight,
                       freqCommitBegin, freqCommitValue, freqCommitEnd,
                       addBandsMaskBegin, addBandsMaskValue,
                       removeBandsMaskBegin, removeBandsMaskValue,
                       cancelWidthEnd, cancelSplitEnd,
                       wheelSplitBegin, wheelSplitValue, wheelSplitEnd,
                       heldWheelWidthValue };

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
        if (target == DestroyAt::valueWrite || target == DestroyAt::addBandsValue
            || target == DestroyAt::resetWidthValue || target == DestroyAt::resetSplitValue
            || target == DestroyAt::soloMaskValue || target == DestroyAt::freqCommitValue
            || target == DestroyAt::addBandsMaskValue || target == DestroyAt::removeBandsMaskValue
            || target == DestroyAt::wheelSplitValue || target == DestroyAt::heldWheelWidthValue)
            fire();
    }

    void parameterGestureChanged (int, bool starting) override
    {
        if (starting) ++gestureBegins;
        else ++gestureEnds;
        if ((starting && (target == DestroyAt::gestureBegin
                          || target == DestroyAt::splitGestureBegin
                          || target == DestroyAt::addBandsBegin
                          || target == DestroyAt::bandMoveBegin
                          || target == DestroyAt::resetWidthBegin
                          || target == DestroyAt::resetSplitBegin
                          || target == DestroyAt::soloMaskBegin
                          || target == DestroyAt::freqCommitBegin
                          || target == DestroyAt::addBandsMaskBegin
                          || target == DestroyAt::removeBandsMaskBegin
                          || target == DestroyAt::wheelSplitBegin))
            || (! starting && (target == DestroyAt::gestureEnd
                               || target == DestroyAt::resetSplitEnd
                               || target == DestroyAt::soloMaskEnd
                               || target == DestroyAt::bandMoveEndLeft
                               || target == DestroyAt::bandMoveEndRight
                               || target == DestroyAt::freqCommitEnd
                               || target == DestroyAt::cancelWidthEnd
                               || target == DestroyAt::cancelSplitEnd
                               || target == DestroyAt::wheelSplitEnd)))
            fire();
    }
};

static void testSpectrumImagerFrequencyEditorLifetimes (int selected = -1)
{
    using namespace spectrum_imager_lifetime_tests;
    std::printf ("NR-029 probe: inline frequency commit survives synchronous editor destruction\n");
    check (selected >= -1 && selected <= 2, "NR029 frequency commit selector is valid");
    if (selected < -1 || selected > 2) return;

    const DestroyAt targets[] { DestroyAt::freqCommitBegin,
                                DestroyAt::freqCommitValue,
                                DestroyAt::freqCommitEnd };
    for (int path = 0; path < 3; ++path)
    {
        if (selected >= 0 && selected != path) continue;
        for (const bool destroy : { false, true })
        {
            auto [processorOwner, processor] = heapProcessorFixture();
            processor.prepareToPlay (48000.0, 512);
            auto& apvts = processor.getAPVTS();
            auto setPlain = [&] (const char* id, float value)
            {
                auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id));
                if (parameter != nullptr)
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            };
            setPlain (pid::advancedMode, 1.0f);
            setPlain (pid::mbEnable, 1.0f);
            setPlain (pid::mbBands, 4.0f);

            auto* const split = dynamic_cast<juce::RangedAudioParameter*> (
                apvts.getParameter (pid::mbFreqLow));
            std::array<float, 3> originalSplits {};
            for (int k = 0; k < 3; ++k)
                if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (
                        apvts.getParameter (k == 0 ? pid::mbFreqLow
                                                   : k == 1 ? pid::mbFreqMid : pid::mbFreqHigh)))
                    originalSplits[(size_t) k] = parameter->getValue();

            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
            auto* imager = editor != nullptr
                         ? findComponent<anamorph::gui::SpectrumImager> (*editor) : nullptr;
            check (split != nullptr && imager != nullptr && imager->getWidth() > 300,
                   "NR029 frequency commit: actual laid-out imager and split exist");
            if (split == nullptr || imager == nullptr || imager->getWidth() <= 300)
            {
                if (editor != nullptr)
                {
                    processor.editorBeingDeleted (editor.get());
                    editor.reset();
                }
                continue;
            }

            const auto source = juce::Desktop::getInstance().getMainMouseSource();
            auto event = [&] (float x, float y)
            {
                return juce::MouseEvent (source, { x, y }, juce::ModifierKeys(),
                                         1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                         imager, imager, juce::Time::getCurrentTime(),
                                         { x, y }, juce::Time::getCurrentTime(), 1, false);
            };
            const float width = (float) imager->getWidth();
            const float height = (float) imager->getHeight();
            const float chipY = height - 15.0f;
            float handleX = -1.0f;
            for (float x = 2.0f; x < width - 2.0f; x += 1.0f)
            {
                imager->mouseMove (event (x, 0.5f * height));
                if (imager->getTooltip() == "Drag to change the split frequency")
                {
                    handleX = x;
                    break;
                }
            }
            check (handleX >= 0.0f,
                   "NR029 frequency commit: production hit-testing finds the first split");
            if (handleX < 0.0f)
            {
                processor.editorBeingDeleted (editor.get());
                editor.reset();
                continue;
            }

            imager->mouseDoubleClick (event (handleX, chipY));
            juce::TextEditor* text = nullptr;
            for (int i = 0; i < imager->getNumChildComponents(); ++i)
                if (auto* candidate = dynamic_cast<juce::TextEditor*> (imager->getChildComponent (i));
                    candidate != nullptr && candidate->isVisible())
                    text = candidate;
            check (text != nullptr && (bool) text->onReturnKey,
                   "NR029 frequency commit: the production inline editor and Return callback open");
            if (text == nullptr || ! text->onReturnKey)
            {
                processor.editorBeingDeleted (editor.get());
                editor.reset();
                continue;
            }
            text->setText ("5 kHz", false);

            DestroyEditorOnParameter listener (processor, editor, targets[path], destroy);
            split->addListener (&listener);
            // Invoke the callback installed by openFreqEditor. It is the same
            // callback JUCE invokes for Return; the parameter listener destroys
            // the owning editor synchronously from the selected notification.
            text->onReturnKey();
            split->removeListener (&listener);

            check (listener.events > 0,
                   "NR029 frequency commit: the selected synchronous callback is reached");
            check (listener.closes == (destroy ? 1 : 0) && (editor == nullptr) == destroy,
                   "NR029 frequency commit: editor destruction control matches the selected case");
            check (listener.gestureBegins == 1 && listener.gestureEnds == 1,
                   "NR029 frequency commit: the split gesture remains balanced");
            check (! juce::exactlyEqual (split->getValue(), originalSplits[0]),
                   "NR029 frequency commit: the processor-side edit completes after editor destruction");

            if (destroy)
            {
                processor.pollUndoCoalesce();
                check (processor.canUndo(),
                       "NR029 frequency commit: the complete edit remains undoable after editor destruction");
                if (processor.canUndo()) processor.undo();
                processor.pollUndoCoalesce();
                bool restored = true;
                for (int k = 0; k < 3; ++k)
                {
                    const char* id = k == 0 ? pid::mbFreqLow : k == 1 ? pid::mbFreqMid : pid::mbFreqHigh;
                    auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id));
                    restored = restored && parameter != nullptr
                            && juce::exactlyEqual (parameter->getValue(), originalSplits[(size_t) k]);
                }
                check (restored,
                       "NR029 frequency commit: Undo restores the exact original split layout");
            }

            if (editor != nullptr)
            {
                processor.editorBeingDeleted (editor.get());
                editor.reset();
            }
        }
    }
}
} // namespace spectrum_imager_lifetime_tests

// Drive the production SpectrumImager mouse callbacks with an actual editor and
// the affected parameter. A synchronous parameter listener destroys the editor during
// beginChangeGesture, setValueNotifyingHost, or endChangeGesture. Live-editor
// cases distinguish a valid drag from a vacuous callback path.
static void testSpectrumImagerCallbackLifetimes (int selected = -1)
{
    using namespace spectrum_imager_lifetime_tests;
    std::printf ("NR-029 probe: SpectrumImager survives synchronous editor destruction\n");
    check (selected >= -1 && selected <= 29, "NR029: selector is valid");
    if (selected < -1 || selected > 29) return;
    // Inline frequency editing has its own production TextEditor harness below.
    // Route the legacy selector aliases there and keep the generic mouse-path
    // sweep from treating those three non-mouse callbacks as inert controls.
    if (selected >= (int) DestroyAt::freqCommitBegin
        && selected <= (int) DestroyAt::freqCommitEnd)
    {
        testSpectrumImagerFrequencyEditorLifetimes (selected - (int) DestroyAt::freqCommitBegin);
        return;
    }

    for (int path = 0; path < 30; ++path)
    {
        if (selected >= 0 && selected != path) continue;
        const auto target = static_cast<DestroyAt> (path);
        if (target == DestroyAt::freqCommitBegin || target == DestroyAt::freqCommitValue
            || target == DestroyAt::freqCommitEnd)
            continue;
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
                                     || target == DestroyAt::addBandsValue
                                     || target == DestroyAt::addBandsMaskBegin
                                     || target == DestroyAt::addBandsMaskValue) ? 3.0f : 4.0f);
            if (target == DestroyAt::soloMaskValue || target == DestroyAt::soloMaskBegin
                || target == DestroyAt::soloMaskEnd || target == DestroyAt::addBandsMaskBegin
                || target == DestroyAt::addBandsMaskValue)
                setPlain (pid::mbSolo, 0.0f);
            if (target == DestroyAt::addBandsMaskBegin || target == DestroyAt::addBandsMaskValue)
                setPlain (pid::mbSolo, 4.0f);
            if (target == DestroyAt::removeBandsMaskBegin || target == DestroyAt::removeBandsMaskValue)
                setPlain (pid::mbSolo, 8.0f);
            auto* width = setPlain (pid::mbWidthMid,
                                    (target == DestroyAt::resetWidthValue
                                     || target == DestroyAt::resetWidthBegin) ? 0.6f : 1.0f);
            if (target == DestroyAt::resetSplitValue || target == DestroyAt::resetSplitBegin
                || target == DestroyAt::resetSplitEnd)
            {
                setPlain (pid::mbFreqLow, 2000.0f);
                setPlain (pid::mbFreqMid, 2400.0f);
                setPlain (pid::mbFreqHigh, 2900.0f);
            }
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
            auto event = [&] (float x, float y, float downX, float downY, bool dragged,
                              bool alt = false)
            {
                auto modifiers = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier);
                if (alt) modifiers = modifiers.withFlags (juce::ModifierKeys::altModifier);
                return juce::MouseEvent (source, { x, y }, modifiers,
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
            else if (target == DestroyAt::resetSplitValue || target == DestroyAt::resetSplitBegin
                     || target == DestroyAt::resetSplitEnd)
            {
                targetParameter = dynamic_cast<juce::RangedAudioParameter*> (
                    apvts.getParameter (pid::mbFreqLow));
                pressX = splitX;
                pressY = laneY;
            }
            else if (target == DestroyAt::soloMaskValue || target == DestroyAt::soloMaskBegin
                     || target == DestroyAt::soloMaskEnd)
            {
                targetParameter = soloParameter;
                pressX = pressY = -1.0f;
                for (float x = 2.0f; x < w - 2.0f; x += 1.0f)
                {
                    imager->mouseMove (event (x, 10.0f, x, 10.0f, false));
                    if (imager->getTooltip() == "Solo this band")
                    { pressX = x; pressY = 10.0f; break; }
                }
                check (pressX >= 0.0f,
                       "NR029: production hit-testing finds a solo toggle");
                if (pressX < 0.0f)
                {
                    processor.editorBeingDeleted (editor.get());
                    editor.reset();
                    continue;
                }
            }
            else if (target == DestroyAt::addBandsMaskBegin || target == DestroyAt::addBandsMaskValue)
            {
                targetParameter = soloParameter;
                pressX = pressY = -1.0f;
                for (float y = 2.0f; y < h - 2.0f && pressX < 0.0f; y += 1.0f)
                    for (float x = 2.0f; x < w - 2.0f; x += 1.0f)
                    {
                        imager->mouseMove (event (x, y, x, y, false));
                        if (imager->getTooltip() == "Click to add a band split")
                        { pressX = x; pressY = y; break; }
                    }
                check (pressX >= 0.0f,
                       "NR029: production hit-testing finds the add affordance for nested teardown");
                if (pressX < 0.0f)
                {
                    processor.editorBeingDeleted (editor.get());
                    editor.reset();
                    continue;
                }
            }
            else if (target == DestroyAt::removeBandsMaskBegin || target == DestroyAt::removeBandsMaskValue)
            {
                targetParameter = soloParameter;
                pressX = 13.0f;
                pressY = h - 30.0f;
            }
            else if (target == DestroyAt::cancelSplitEnd)
            {
                targetParameter = dynamic_cast<juce::RangedAudioParameter*> (
                    apvts.getParameter (pid::mbFreqLow));
                pressX = splitX;
                pressY = laneY;
            }
            else if (target == DestroyAt::wheelSplitBegin || target == DestroyAt::wheelSplitValue
                     || target == DestroyAt::wheelSplitEnd)
            {
                targetParameter = dynamic_cast<juce::RangedAudioParameter*> (
                    apvts.getParameter (pid::mbFreqLow));
                pressX = splitX;
                pressY = laneY;
            }
            else if (target == DestroyAt::heldWheelWidthValue)
            {
                targetParameter = width;
                pressX = widthX;
                pressY = widthY;
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
            else if (target == DestroyAt::bandMoveBegin
                     || target == DestroyAt::bandMoveEndLeft
                     || target == DestroyAt::bandMoveEndRight)
            {
                targetParameter = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (
                    target == DestroyAt::bandMoveEndRight ? pid::mbFreqMid : pid::mbFreqLow));
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
                || target == DestroyAt::addBandsMaskBegin || target == DestroyAt::addBandsMaskValue
                || target == DestroyAt::removeBandsMaskBegin || target == DestroyAt::removeBandsMaskValue
                || target == DestroyAt::bandMoveBegin || target == DestroyAt::resetWidthValue
                || target == DestroyAt::resetWidthBegin || target == DestroyAt::resetSplitValue
                || target == DestroyAt::resetSplitBegin || target == DestroyAt::resetSplitEnd)
                targetParameter->addListener (&listener);
            const bool wheelTarget = target == DestroyAt::wheelSplitBegin
                                  || target == DestroyAt::wheelSplitValue
                                  || target == DestroyAt::wheelSplitEnd;
            if (! wheelTarget)
                imager->mouseDown (event (pressX, pressY, pressX, pressY, false,
                                          target == DestroyAt::resetWidthValue
                                          || target == DestroyAt::resetWidthBegin
                                          || target == DestroyAt::resetSplitValue
                                          || target == DestroyAt::resetSplitBegin
                                          || target == DestroyAt::resetSplitEnd));
            if (target != DestroyAt::gestureBegin && target != DestroyAt::splitGestureBegin
                && target != DestroyAt::addBandsBegin && target != DestroyAt::addBandsValue
                && target != DestroyAt::addBandsMaskBegin && target != DestroyAt::addBandsMaskValue
                && target != DestroyAt::removeBandsMaskBegin && target != DestroyAt::removeBandsMaskValue
                && target != DestroyAt::bandMoveBegin && target != DestroyAt::resetWidthValue
                && target != DestroyAt::resetWidthBegin && target != DestroyAt::resetSplitValue
                && target != DestroyAt::resetSplitBegin && target != DestroyAt::resetSplitEnd)
                targetParameter->addListener (&listener);

            if (editor != nullptr && target == DestroyAt::valueWrite)
                imager->mouseDrag (event (pressX, pressY + 10.0f, pressX, pressY, true));
            else if (editor != nullptr && (target == DestroyAt::bandMoveBegin
                                           || target == DestroyAt::bandMoveEndLeft
                                           || target == DestroyAt::bandMoveEndRight))
                imager->mouseDrag (event (pressX + 5.0f, pressY, pressX, pressY, true));
            else if (editor != nullptr && target == DestroyAt::gestureEnd)
                imager->mouseUp (event (pressX, pressY, pressX, pressY, false));
            else if (editor != nullptr && (target == DestroyAt::cancelWidthEnd
                                           || target == DestroyAt::cancelSplitEnd))
            {
                if (target == DestroyAt::cancelWidthEnd)
                {
                    // The first >3 px event engages a width drag and anchors it;
                    // a second event is needed to issue a value store.
                    imager->mouseDrag (event (pressX, pressY + 10.0f, pressX, pressY, true));
                    if (editor != nullptr)
                        imager->mouseDrag (event (pressX, pressY + 15.0f, pressX, pressY, true));
                }
                else
                    imager->mouseDrag (event (pressX + 5.0f, pressY, pressX, pressY, true));
                if (editor != nullptr) imager->cancelActiveDrag();
            }
            else if (editor != nullptr && (wheelTarget || target == DestroyAt::heldWheelWidthValue))
            {
                auto wheelEvent = [&] (float x, float y)
                {
                    return juce::MouseEvent (source, { x, y }, juce::ModifierKeys(),
                                             1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                             imager, imager, juce::Time::getCurrentTime(),
                                             { x, y }, juce::Time::getCurrentTime(), 1, false);
                };
                juce::MouseWheelDetails wheel;
                wheel.deltaX = 0.0f;
                wheel.deltaY = -0.6f;
                wheel.isReversed = false;
                wheel.isSmooth = false;
                wheel.isInertial = false;
                if (target == DestroyAt::heldWheelWidthValue)
                    imager->takeWheelNotch (event (pressX, pressY, pressX, pressY, true), wheel);
                else
                    imager->mouseWheelMove (wheelEvent (pressX, pressY), wheel);
            }

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
            if (target == DestroyAt::resetWidthValue || target == DestroyAt::resetWidthBegin)
                check (listener.gestureBegins == listener.gestureEnds,
                       "NR029: a reset whose parameter callback destroys the editor closes its gesture");
            if (target == DestroyAt::resetSplitValue || target == DestroyAt::resetSplitBegin
                || target == DestroyAt::resetSplitEnd)
                check (listener.gestureBegins == listener.gestureEnds,
                       "NR029: a crossover reset whose callback destroys the editor closes its gesture");
            if (target == DestroyAt::valueWrite)
                check (listener.gestureEnds == 1,
                       "NR029: destroying during a value write still closes its active gesture");
            if (target == DestroyAt::resetWidthValue || target == DestroyAt::resetWidthBegin)
            {
                check (width != nullptr && juce::exactlyEqual (width->getValue(),
                                                                width->getDefaultValue()),
                       "NR029: the reset's processor-side value change completes after editor destruction");
                check (listener.gestureBegins == 1 && listener.gestureEnds == 1,
                       "NR029: the reset's host gesture remains balanced");
            }
            if (target == DestroyAt::resetSplitValue || target == DestroyAt::resetSplitBegin
                || target == DestroyAt::resetSplitEnd)
                check (targetParameter != nullptr && ! juce::exactlyEqual (targetParameter->getValue(),
                                                                            originalSplits[0]),
                       "NR029: the crossover reset changes its processor-side primary parameter after editor destruction");
            if (target == DestroyAt::soloMaskValue || target == DestroyAt::soloMaskBegin
                || target == DestroyAt::soloMaskEnd)
            {
                check (listener.gestureBegins == listener.gestureEnds,
                       "NR029: a solo-mask callback that destroys the editor closes its gesture");
                check (soloParameter != nullptr && ! juce::exactlyEqual (soloParameter->getValue(),
                                                                          originalSolo),
                       "NR029: the solo click completes its processor-side mask change after editor destruction");
            }
            if (target == DestroyAt::addBandsMaskBegin || target == DestroyAt::addBandsMaskValue
                || target == DestroyAt::removeBandsMaskBegin || target == DestroyAt::removeBandsMaskValue)
            {
                check (listener.gestureBegins == listener.gestureEnds,
                       "NR029: a nested topology transaction keeps its solo gesture balanced");
                auto* liveBands = dynamic_cast<juce::RangedAudioParameter*> (
                    apvts.getParameter (pid::mbBands));
                const bool adding = target == DestroyAt::addBandsMaskBegin
                                 || target == DestroyAt::addBandsMaskValue;
                const int originalCount = adding ? 3 : 4;
                const int expected = destroy ? originalCount : (adding ? 4 : 3);
                check (liveBands != nullptr
                       && (int) std::lround (liveBands->convertFrom0to1 (liveBands->getValue())) == expected,
                       destroy
                           ? "NR029: editor teardown cancels the nested topology command without a partial count change"
                           : "NR029: the live nested topology command completes its count change");
                if (destroy)
                    check (soloParameter != nullptr
                           && juce::exactlyEqual (soloParameter->getValue(), originalSolo),
                           "NR029: teardown leaves the solo word unchanged instead of partially remapping it");
                else
                {
                    const int liveMask = soloParameter != nullptr
                                       ? (int) std::lround (soloParameter->convertFrom0to1 (
                                             soloParameter->getValue())) : -1;
                    check (liveMask >= 0 && (liveMask & ~((1 << expected) - 1)) == 0,
                           "NR029: the live topology edit leaves no solo bit outside its new count");
                }
            }
            if (target == DestroyAt::bandMoveEndLeft || target == DestroyAt::bandMoveEndRight)
            {
                check (listener.gestureBegins > 0 && listener.gestureEnds > 0,
                       "NR029: the selected band-move split owns a real begin/end pair");
                check (listener.gestureBegins == listener.gestureEnds,
                       "NR029: a band-move gesture whose close destroys the editor remains balanced");
                bool moved = false;
                for (int k = 0; k < 3; ++k)
                {
                    const char* id = k == 0 ? pid::mbFreqLow : k == 1 ? pid::mbFreqMid : pid::mbFreqHigh;
                    auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id));
                    moved = moved || (parameter != nullptr
                                      && ! juce::exactlyEqual (parameter->getValue(), originalSplits[(size_t) k]));
                }
                check (moved,
                       "NR029: the band move's processor-side split changes complete after editor destruction");
            }
            if (target == DestroyAt::cancelWidthEnd || target == DestroyAt::cancelSplitEnd)
            {
                check (listener.gestureBegins == 0 && listener.gestureEnds == 1,
                       "NR029: cancellation closes its already-open gesture once after listener registration");
                const bool widthCancel = target == DestroyAt::cancelWidthEnd;
                const float original = widthCancel ? originalWidths[1] : originalSplits[0];
                check (targetParameter != nullptr
                       && ! juce::exactlyEqual (targetParameter->getValue(), original),
                       "NR029: the parameter edit remains committed after cancellation teardown");
            }
            if (target == DestroyAt::wheelSplitBegin || target == DestroyAt::wheelSplitValue
                || target == DestroyAt::wheelSplitEnd)
            {
                check (listener.gestureBegins == 1 && listener.gestureEnds == 1,
                       "NR029: standalone split-wheel gesture remains balanced across teardown");
                const bool abortedBeforeStore = destroy && target == DestroyAt::wheelSplitBegin;
                check (targetParameter != nullptr
                       && (juce::exactlyEqual (targetParameter->getValue(), originalSplits[0])
                           == abortedBeforeStore),
                       abortedBeforeStore
                           ? "NR029: destruction at gesture begin cancels the wheel before its value store"
                           : "NR029: standalone split-wheel processor edit completes across editor destruction");
            }
            if (target == DestroyAt::heldWheelWidthValue)
            {
                check (listener.gestureEnds == 1,
                       "NR029: the held width gesture closes when its value notification destroys the editor");
                check (width != nullptr && ! juce::exactlyEqual (width->getValue(), originalWidths[1]),
                       "NR029: the in-press wheel width edit completes after editor destruction");
            }
            if (editor != nullptr)
            {
                processor.editorBeingDeleted (editor.get());
                editor.reset();
            }
            targetParameter->removeListener (&listener);

            if (destroy && (target == DestroyAt::addBandsMaskBegin
                            || target == DestroyAt::addBandsMaskValue
                            || target == DestroyAt::removeBandsMaskBegin
                            || target == DestroyAt::removeBandsMaskValue))
            {
                processor.pollUndoCoalesce();
                check (! processor.canUndo(),
                       "NR029: cancelling before topology stores creates no partial Undo transaction");
                bool restored = true;
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
                       "NR029: cancelling nested topology edits preserves the original sound layout");
            }

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
            if (destroy && (target == DestroyAt::resetWidthValue
                            || target == DestroyAt::resetWidthBegin))
            {
                processor.pollUndoCoalesce();
                check (processor.canUndo(),
                       "NR029: the completed reset remains undoable after editor destruction");
                if (processor.canUndo()) processor.undo();
                processor.pollUndoCoalesce();
                check (width != nullptr && juce::exactlyEqual (width->getValue(), originalWidths[1]),
                       "NR029: Undo restores the exact pre-reset width after editor destruction");
            }
            if (destroy && (target == DestroyAt::resetSplitValue
                            || target == DestroyAt::resetSplitBegin
                            || target == DestroyAt::resetSplitEnd))
            {
                processor.pollUndoCoalesce();
                check (processor.canUndo(),
                       "NR029: the completed crossover reset remains undoable after editor destruction");
                if (processor.canUndo()) processor.undo();
                processor.pollUndoCoalesce();
                bool restored = true;
                for (int k = 0; k < 3; ++k)
                {
                    const char* id = k == 0 ? pid::mbFreqLow : k == 1 ? pid::mbFreqMid : pid::mbFreqHigh;
                    auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id));
                    restored = restored && parameter != nullptr
                            && juce::exactlyEqual (parameter->getValue(), originalSplits[(size_t) k]);
                }
                check (restored,
                       "NR029: Undo restores the exact pre-reset crossover layout after editor destruction");
            }
            if (destroy && (target == DestroyAt::soloMaskValue
                            || target == DestroyAt::soloMaskBegin
                            || target == DestroyAt::soloMaskEnd))
            {
                processor.pollUndoCoalesce();
                check (processor.canUndo(),
                       "NR029: the completed solo click remains undoable after editor destruction");
                if (processor.canUndo()) processor.undo();
                processor.pollUndoCoalesce();
                check (soloParameter != nullptr && juce::exactlyEqual (soloParameter->getValue(), originalSolo),
                       "NR029: Undo restores the exact pre-click solo mask after editor destruction");
            }
            if (destroy && (target == DestroyAt::bandMoveEndLeft
                            || target == DestroyAt::bandMoveEndRight))
            {
                processor.pollUndoCoalesce();
                check (processor.canUndo(),
                       "NR029: a completed band move remains undoable after editor destruction");
                if (processor.canUndo()) processor.undo();
                processor.pollUndoCoalesce();
                bool restored = true;
                for (int k = 0; k < 3; ++k)
                {
                    const char* id = k == 0 ? pid::mbFreqLow : k == 1 ? pid::mbFreqMid : pid::mbFreqHigh;
                    auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id));
                    restored = restored && parameter != nullptr
                            && juce::exactlyEqual (parameter->getValue(), originalSplits[(size_t) k]);
                }
                check (restored,
                       "NR029: Undo restores the exact pre-move split layout after editor destruction");
            }
            if (destroy && (target == DestroyAt::cancelWidthEnd || target == DestroyAt::cancelSplitEnd))
            {
                processor.pollUndoCoalesce();
                check (processor.canUndo(),
                       "NR029: a gesture cancelled during editor destruction remains undoable");
                if (processor.canUndo()) processor.undo();
                processor.pollUndoCoalesce();
                const bool widthCancel = target == DestroyAt::cancelWidthEnd;
                const float original = widthCancel ? originalWidths[1] : originalSplits[0];
                check (targetParameter != nullptr
                       && juce::exactlyEqual (targetParameter->getValue(), original),
                       "NR029: Undo restores the exact value from before cancellation");
            }
            if (destroy && (target == DestroyAt::wheelSplitBegin || target == DestroyAt::wheelSplitValue
                            || target == DestroyAt::wheelSplitEnd))
            {
                processor.pollUndoCoalesce();
                if (target == DestroyAt::wheelSplitBegin)
                    check (! processor.canUndo(),
                           "NR029: cancelling at wheel gesture begin creates no empty Undo step");
                else
                {
                    check (processor.canUndo(),
                           "NR029: standalone split-wheel edit remains undoable after editor destruction");
                    if (processor.canUndo()) processor.undo();
                    processor.pollUndoCoalesce();
                    check (targetParameter != nullptr
                           && juce::exactlyEqual (targetParameter->getValue(), originalSplits[0]),
                           "NR029: Undo restores the exact split from before the wheel edit");
                }
            }
            if (destroy && target == DestroyAt::heldWheelWidthValue)
            {
                processor.pollUndoCoalesce();
                check (processor.canUndo(),
                       "NR029: the completed in-press wheel edit remains undoable after editor destruction");
                if (processor.canUndo()) processor.undo();
                processor.pollUndoCoalesce();
                check (width != nullptr && juce::exactlyEqual (width->getValue(), originalWidths[1]),
                       "NR029: Undo restores the exact width from before the in-press wheel edit");
            }
        }
    }
}
