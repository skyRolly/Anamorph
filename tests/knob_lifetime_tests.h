#pragma once

// ADR-0072 B2: actual editor/Knob/ValueBox and real parameter notification paths.
// Only the processor and host-route witness survive editor destruction.
struct AnamorphKnobLifetimeTestAccess
{
    static void beforeWheel (AnamorphAudioProcessorEditor& editor, std::function<void()> callback)
    { editor.widthK.onStandaloneWheel = std::move (callback); }
};

namespace knob_lifetime_tests
{
    enum class Phase { live, begin, value, end, before, after };
    enum class Input { positive, negative, horizontal, reverse, zero, duplicate, tiny, rail, invalid, disabled };
    enum class Target { knob, valueBox, textWheel, textReturn, textFocus, textEscape };

    struct Witness final : juce::AudioProcessorParameter::Listener, juce::AudioProcessorListener
    {
        AnamorphAudioProcessor& processor;
        std::unique_ptr<juce::AudioProcessorEditor>& editor;
        juce::RangedAudioParameter& parameter;
        Phase phase;
        bool armed = false;
        int begins = 0, ends = 0, values = 0, closes = 0;
        Witness (AnamorphAudioProcessor& p, std::unique_ptr<juce::AudioProcessorEditor>& e,
                 juce::RangedAudioParameter& param, Phase f)
            : processor (p), editor (e), parameter (param), phase (f) {}
        void destroy (Phase at)
        {
            if (armed && phase == at && editor)
            {
                ++closes;
                processor.editorBeingDeleted (editor.get());
                editor.reset();
            }
        }
        void parameterValueChanged (int, float) override { destroy (Phase::value); }
        void parameterGestureChanged (int, bool start) override { destroy (start ? Phase::begin : Phase::end); }
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}
        void audioProcessorParameterChanged (juce::AudioProcessor*, int index, float) override
        { if (index == parameter.getParameterIndex()) ++values; }
        void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int index) override
        { if (index == parameter.getParameterIndex()) ++begins; }
        void audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int index) override
        { if (index == parameter.getParameterIndex()) ++ends; }
    };

    static juce::Slider* widthSlider (juce::Component& c)
    {
        if (auto* s = dynamic_cast<juce::Slider*> (&c); s != nullptr && s->getTitle() == "Width") return s;
        for (auto* child : c.getChildren()) if (auto* s = widthSlider (*child)) return s;
        return nullptr;
    }

    static void run (bool held, Phase phase, Input input, Target target = Target::knob)
    {
        auto processor = std::make_unique<AnamorphAudioProcessor>();
        processor->prepareToPlay (48000.0, 128);
        auto* parameter = processor->getAPVTS().getParameter (pid::width);
        check (parameter != nullptr, "B2 Width parameter exists");
        if (parameter == nullptr) return;
        if (input == Input::rail)
        {
            anamorph::param::setValueNotifyingHost (parameter, 1.0f);
            processor->pollUndoCoalesce();
        }
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
            auto* slider = editor ? widthSlider (*editor) : nullptr;
            check (slider != nullptr, "B2 actual Width component exists after reopen");
            if (slider == nullptr) return;
            juce::Label* box = nullptr;
            for (auto* child : slider->getChildren())
                if (auto* label = dynamic_cast<juce::Label*> (child)) { box = label; break; }
            check (box != nullptr, "B2 actual ValueBox exists");
            if (box == nullptr) return;
            if (input == Input::disabled) slider->setScrollWheelEnabled (false);
            processor->pollUndoCoalesce();
            const float before = parameter->getValue();
            Witness witness (*processor, editor, *parameter, phase);
            parameter->addListener (&witness);
            processor->addListener (&witness);
            const auto source = juce::Desktop::getInstance().getMainMouseSource();
            auto event = [&] (juce::Component* c, bool down)
            {
                const auto pos = c->getLocalBounds().getCentre().toFloat();
                const auto time = juce::Time (100000 + cycle * 100);
                return juce::MouseEvent (source, pos,
                    down ? juce::ModifierKeys::leftButtonModifier : juce::ModifierKeys(),
                    1.0f, 0, 0, 0, 0, c, c, time, pos, time, 1, false);
            };
            const bool text = target == Target::textWheel || target == Target::textReturn
                           || target == Target::textFocus || target == Target::textEscape;
            if (text)
            {
                box->showEditor();
                auto* edit = box->getCurrentTextEditor();
                check (edit != nullptr, "B2 real TextEditor opens");
                if (edit == nullptr) return;
                edit->setText (parameter->getText (before + 0.1f, 0), false);
            }
            if (phase == Phase::before)
                AnamorphKnobLifetimeTestAccess::beforeWheel (
                    *dynamic_cast<AnamorphAudioProcessorEditor*> (editor.get()),
                    [&witness] { witness.destroy (Phase::before); });
            witness.armed = true;
            auto* press = target == Target::valueBox ? static_cast<juce::Component*> (box)
                                                     : static_cast<juce::Component*> (slider);
            if (held) press->mouseDown (event (press, true));
            juce::MouseWheelDetails wheel {};
            wheel.deltaX = input == Input::horizontal ? -0.2f : 0.0f;
            wheel.deltaY = input == Input::zero || input == Input::horizontal ? 0.0f
                         : input == Input::negative ? -0.2f
                         : input == Input::tiny ? 1.0e-6f : 0.2f;
            if (input == Input::invalid) wheel.deltaY = std::numeric_limits<float>::quiet_NaN();
            wheel.isReversed = input == Input::reverse;
            wheel.isSmooth = false;
            wheel.isInertial = false;
            if (editor)
            {
                // Dispatch the real messages posted by TextEditor 9.0.5
                // (TextEditorDefs), through Component's public virtual API.
                // Label keeps its listener implementations protected.
                if (target == Target::textReturn)
                    static_cast<juce::Component*> (box->getCurrentTextEditor())->handleCommandMessage (0x10003002);
                else if (target == Target::textFocus)
                    static_cast<juce::Component*> (box->getCurrentTextEditor())->handleCommandMessage (0x10003004);
                else if (target == Target::textEscape)
                    static_cast<juce::Component*> (box->getCurrentTextEditor())->handleCommandMessage (0x10003003);
                else
                {
                    slider->mouseWheelMove (event (slider, held), wheel);
                    if (editor && input == Input::duplicate) slider->mouseWheelMove (event (slider, held), wheel);
                }
            }
            if (held && editor) press->mouseUp (event (press, false));
            if (phase == Phase::after) witness.destroy (Phase::after);
            witness.armed = false;
            parameter->removeListener (&witness);
            processor->removeListener (&witness);
            const float after = parameter->getValue();
            const bool noEdit = (input == Input::zero && target != Target::textWheel)
                             || input == Input::rail || input == Input::invalid || input == Input::disabled
                             || target == Target::textEscape;
            const bool expectEdit = ! noEdit && phase != Phase::begin && phase != Phase::before;
            const bool expectGesture = held || (! noEdit && phase != Phase::before);
            const bool expectClose = phase == Phase::before || phase == Phase::after
                                  || (phase != Phase::live && expectGesture
                                      && (phase != Phase::value || expectEdit));
            // A pending typed edit has its own pair, then the standalone notch
            // has another. ADR-0053 groups both pairs into the same Undo action.
            const int pairs = ! expectGesture ? 0 : target == Target::textWheel && phase == Phase::live && input != Input::zero ? 2 : 1;
            check (witness.begins == pairs, "B2 host Begin count is exact");
            check (witness.ends == witness.begins, "B2 host End is paired exactly once");
            check (witness.closes == (expectClose ? 1 : 0), "B2 requested destruction phase was reached");
            check ((! juce::exactlyEqual (before, after)) == expectEdit, "B2 applied/no-edit value survives teardown");
            const float movement = ! expectEdit ? 0.0f
                : target == Target::textWheel ? (phase == Phase::live && input != Input::zero ? 0.13f : 0.1f)
                : target == Target::textReturn || target == Target::textFocus ? 0.1f
                : input == Input::tiny ? 0.0005f
                : input == Input::negative || input == Input::reverse ? -0.03f : 0.03f;
            check (std::abs (after - (before + movement)) < 1.0e-5f,
                   "B2 Width direction, floor and typed-before-wheel endpoint are preserved");
            if (input == Input::duplicate) check (witness.values <= 1, "B2 duplicate timestamp cannot write twice");
            processor->pollUndoCoalesce();
            const bool undoable = processor->canUndo();
            check (undoable == expectEdit, "B2 applied edit retains meaningful Undo; no-edit has none");
            if (undoable)
            {
                processor->undo();
                check (juce::exactlyEqual (parameter->getValue(), before), "B2 actual Undo restores preceding endpoint");
                check (processor->canRedo(), "B2 actual Undo creates Redo");
                processor->redo();
                check (juce::exactlyEqual (parameter->getValue(), after), "B2 actual Redo restores applied endpoint");
            }
            if (editor) { processor->editorBeingDeleted (editor.get()); editor.reset(); }
            editor.reset (processor->createEditor());
            auto* reopened = editor ? widthSlider (*editor) : nullptr;
            check (reopened != nullptr && std::abs (reopened->getValue() - parameter->convertFrom0to1 (after)) < 1.0e-5,
                   "B2 reopened editor displays surviving processor value");
            check (! anamorph::param::insideDispatch(), "B2 dispatch bracket is balanced after lifecycle completion");
            std::printf ("B2 target=%d held=%d phase=%d input=%d cycle=%d before=%.9g after=%.9g begin=%d end=%d undo=%d closes=%d\n",
                         (int) target, held, (int) phase, (int) input, cycle, before, after,
                         witness.begins, witness.ends, undoable, witness.closes);
            processor->editorBeingDeleted (editor.get());
            editor.reset();
        }
    }

    static void hostFeedbackTeardown()
    {
        auto processor = std::make_unique<AnamorphAudioProcessor>();
        processor->prepareToPlay (48000.0, 128);
        auto* parameter = processor->getAPVTS().getParameter (pid::width);
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
            auto* slider = widthSlider (*editor);
            Witness witness (*processor, editor, *parameter, Phase::live);
            processor->addListener (&witness);
            // Host feedback traverses ParameterAttachment -> actual Slider.
            // Its UI callback detaches the binding while that updater is active.
            slider->onValueChange = [&]
            {
                ++witness.closes;
                processor->editorBeingDeleted (editor.get());
                editor.reset();
            };
            const float expected = 0.6f + 0.1f * (float) cycle;
            anamorph::param::setValueNotifyingHost (parameter, expected);
            processor->removeListener (&witness);
            check (! editor && witness.closes == 1, "B2 host feedback reaches actual UI teardown callback");
            check (witness.begins == 0 && witness.ends == 0 && witness.values == 1,
                   "B2 host feedback manufactures no gesture or parameter echo");
            check (std::abs (parameter->getValue() - expected) < 1.0e-5f, "B2 host feedback value survives detachment");
            processor->pollUndoCoalesce();
            check (! processor->canUndo(), "B2 host feedback creates no user Undo step");
            editor.reset (processor->createEditor());
            slider = widthSlider (*editor);
            check (std::abs (slider->getValue() - parameter->convertFrom0to1 (expected)) < 1.0e-5,
                   "B2 reopened editor synchronizes the surviving host value");
            processor->editorBeingDeleted (editor.get());
            editor.reset();
        }
    }

    static void repeatedLiveScroll()
    {
        auto processor = std::make_unique<AnamorphAudioProcessor>();
        processor->prepareToPlay (48000.0, 128);
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
        auto* parameter = processor->getAPVTS().getParameter (pid::width);
        auto* slider = widthSlider (*editor);
        const float before = parameter->getValue();
        Witness witness (*processor, editor, *parameter, Phase::live);
        processor->addListener (&witness);
        for (int notch = 0; notch < 3; ++notch)
        {
            const auto position = slider->getLocalBounds().getCentre().toFloat();
            const auto time = juce::Time (200000 + notch);
            const juce::MouseEvent event (juce::Desktop::getInstance().getMainMouseSource(), position, {},
                1.0f, 0, 0, 0, 0, slider, slider, time, position, time, 1, false);
            juce::MouseWheelDetails wheel {};
            wheel.deltaY = 0.2f;
            slider->mouseWheelMove (event, wheel);
            processor->pollUndoCoalesce();
            check (std::abs (parameter->getValue() - before - 0.03f * (float) (notch + 1)) < 1.0e-5f,
                   "B2 closed record restarts for each real scroll transition");
        }
        processor->removeListener (&witness);
        check (witness.begins == 3 && witness.ends == 3 && witness.values == 3,
               "B2 repeated notches each own one pair and one parameter write");
        check (processor->canUndo(), "B2 repeated scroll has meaningful Undo");
        processor->undo();
        check (juce::exactlyEqual (parameter->getValue(), before), "B2 repeated same-control scroll is one Undo step");
        check (! processor->canUndo(), "B2 repeated scroll creates no extra Undo steps");
        processor->redo();
        check (std::abs (parameter->getValue() - before - 0.09f) < 1.0e-5f,
               "B2 repeated scroll Redo restores the complete sequence");
        processor->editorBeingDeleted (editor.get());
        editor.reset();
    }

    static void reentrantHostWheelAnchor()
    {
        for (bool valueBox : { false, true })
        {
            auto processor = std::make_unique<AnamorphAudioProcessor>();
            processor->prepareToPlay (48000.0, 128);
            std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
            auto* parameter = processor->getAPVTS().getParameter (pid::width);
            auto* slider = widthSlider (*editor);
            juce::Component* press = slider;
            if (valueBox)
                for (auto* child : slider->getChildren())
                    if (dynamic_cast<juce::Label*> (child)) { press = child; break; }
            struct Host final : juce::AudioProcessorListener
            {
                juce::RangedAudioParameter& parameter;
                bool fired = false;
                explicit Host (juce::RangedAudioParameter& p) : parameter (p) {}
                void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}
                void audioProcessorParameterChanged (juce::AudioProcessor*, int index, float) override
                {
                    if (index == parameter.getParameterIndex() && ! fired)
                    {
                        fired = true;
                        anamorph::param::setValueNotifyingHost (&parameter, 0.8f);
                    }
                }
            } host (*parameter);
            processor->addListener (&host);
            auto event = [&] (juce::Component* c, bool down, bool dragged)
            {
                const auto pos = c->getLocalBounds().getCentre().toFloat();
                const auto time = juce::Time (500000);
                return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), pos,
                    down ? juce::ModifierKeys::leftButtonModifier : juce::ModifierKeys(),
                    1.0f, 0, 0, 0, 0, c, c, time, pos, time, 1, dragged);
            };
            press->mouseDown (event (press, true, false));
            juce::MouseWheelDetails wheel {};
            wheel.deltaY = 0.2f;
            slider->mouseWheelMove (event (slider, true, false), wheel);
            check (host.fired && std::abs (parameter->getValue() - 0.8f) < 1.0e-5f,
                   "B2 Wheel delivered actual reentrant host feedback");
            press->mouseDrag (event (press, true, true));
            check (std::abs (parameter->getValue() - 0.8f) < 1.0e-5f,
                   "B2 Knob/ValueBox drag banks the delivered value, not a pre-notification prediction");
            press->mouseUp (event (press, false, true));
            processor->removeListener (&host);
            processor->pollUndoCoalesce();
            check (processor->canUndo(), "B2 reentrant-feedback press keeps meaningful user history");
            processor->undo();
            check (std::abs (parameter->getValue() - 0.5f) < 1.0e-5f, "B2 feedback press Undo restores the preceding value");
            processor->redo();
            check (std::abs (parameter->getValue() - 0.53f) < 1.0e-5f,
                   "B2 feedback press Redo restores the owned Wheel endpoint");
            processor->editorBeingDeleted (editor.get());
            editor.reset();
        }
    }

    static void test()
    {
        std::printf ("NR-026 B2: Knob, held press and text gesture/Undo lifecycle\n");
        hostFeedbackTeardown();
        repeatedLiveScroll();
        reentrantHostWheelAnchor();
        for (bool held : { false, true })
            for (auto phase : { Phase::live, Phase::begin, Phase::value, Phase::end })
                for (auto input : { Input::positive, Input::negative, Input::horizontal, Input::reverse,
                                    Input::zero, Input::duplicate })
                    run (held, phase, input);
        for (bool held : { false, true })
            for (auto input : { Input::tiny, Input::rail, Input::invalid })
                run (held, Phase::live, input);
        for (auto phase : { Phase::live, Phase::begin, Phase::value, Phase::end })
        {
            run (true, phase, Input::positive, Target::valueBox);
            run (false, phase, Input::positive, Target::textWheel);
            run (false, phase, Input::positive, Target::textReturn);
            run (false, phase, Input::positive, Target::textFocus);
            run (false, phase, Input::zero, Target::textWheel);
        }
        run (false, Phase::live, Input::zero, Target::textEscape);
        run (true, Phase::live, Input::invalid, Target::valueBox);
        run (false, Phase::live, Input::disabled);
        for (bool held : { false, true })
            for (auto input : { Input::positive, Input::zero }) run (held, Phase::after, input);
        for (auto input : { Input::positive, Input::zero }) run (false, Phase::before, input);
    }
}
