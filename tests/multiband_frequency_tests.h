#pragma once

namespace multiband_frequency_tests
{
using Visual = anamorph::gui::ScopeVisualTestAccess;
using Imager = anamorph::gui::SpectrumImager;
constexpr const char* splitIds[] { pid::mbFreqLow, pid::mbFreqMid, pid::mbFreqHigh };

struct Notifications final : juce::AudioProcessorParameter::Listener
{
    int writes = 0, begins = 0, ends = 0;
    void parameterValueChanged (int, float) override { ++writes; }
    void parameterGestureChanged (int, bool begin) override { if (begin) ++begins; else ++ends; }
};

struct Fixture
{
    std::unique_ptr<AnamorphAudioProcessor> processor = std::make_unique<AnamorphAudioProcessor>();
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    Imager* imager = nullptr;
    Notifications notifications;
    int sweeps = 0;

    explicit Fixture (int bands = 2)
    {
        processor->prepareToPlay (48000.0, 512);
        set (pid::advancedMode, 1);
        set (pid::mbBands, (float) bands);
        editor.reset (processor->createEditor());
        imager = spectrum_imager_lifetime_tests::findComponent<Imager> (*editor);
        check (imager != nullptr, "frequency: production editor contains the imager");
        if (imager == nullptr) return;
        editor->setVisible (true);
        const auto sweep = imager->onSweep;
        imager->onSweep = [this, sweep] { ++sweeps; if (sweep) sweep(); };
        for (const auto* id : splitIds) parameter (id)->addListener (&notifications);
        paint();
    }
    ~Fixture()
    {
        for (const auto* id : splitIds) parameter (id)->removeListener (&notifications);
        if (editor != nullptr) processor->editorBeingDeleted (editor.get());
        editor.reset();
    }
    juce::RangedAudioParameter* parameter (const char* id) const
    { return processor->getAPVTS().getParameter (id); }
    void set (const char* id, float plain)
    { auto* p = parameter (id); p->setValueNotifyingHost (p->convertTo0to1 (plain)); }
    float plain (int i = 0) const
    { auto* p = parameter (splitIds[i]); return p->convertFrom0to1 (p->getValue()); }
    std::array<float, 3> row() const
    { return { parameter (splitIds[0])->getValue(), parameter (splitIds[1])->getValue(), parameter (splitIds[2])->getValue() }; }
    void paint()
    {
        juce::Image image (juce::Image::ARGB, imager->getWidth(), imager->getHeight(), true);
        juce::Graphics graphics (image);
        imager->paint (graphics);
    }
    juce::MouseEvent event (juce::Point<float> point, bool held = false) const
    {
        return { juce::Desktop::getInstance().getMainMouseSource(), point,
                 juce::ModifierKeys (held ? juce::ModifierKeys::leftButtonModifier : 0),
                 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, imager, imager,
                 juce::Time::getCurrentTime(), point, juce::Time::getCurrentTime(), 1, false };
    }
    juce::TextEditor* open (int i = 0)
    {
        imager->mouseDoubleClick (event (Visual::chip (*imager, i).getCentre()));
        for (auto* child : imager->getChildren())
            if (auto* text = dynamic_cast<juce::TextEditor*> (child); text != nullptr && text->isVisible())
                return text;
        check (false, "frequency: double-click opens the production inline editor");
        return nullptr;
    }
    void commit (const juce::String& value, int i = 0, bool focus = false)
    {
        if (auto* text = open (i))
        {
            text->setText (value, false);
            if (focus) text->onFocusLost(); else text->onReturnKey();
        }
    }
    void click (const char* tooltip)
    {
        juce::Component* target = nullptr;
        const auto visit = [&] (auto&& self, juce::Component& c) -> void
        {
            if (auto* tip = dynamic_cast<juce::TooltipClient*> (&c); tip != nullptr && tip->getTooltip() == tooltip)
                target = &c;
            for (auto* child : c.getChildren()) self (self, *child);
        };
        visit (visit, *editor);
        check (target != nullptr, "frequency: production command component exists");
        if (target == nullptr) return;
        if (auto* button = dynamic_cast<juce::Button*> (target)) { if (button->onClick) button->onClick(); }
        else
        {
            const auto point = target->getLocalBounds().getCentre().toFloat();
            target->mouseDown ({ juce::Desktop::getInstance().getMainMouseSource(), point,
                juce::ModifierKeys::leftButtonModifier, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                target, target, juce::Time::getCurrentTime(), point, juce::Time::getCurrentTime(), 1, false });
        }
    }
    // The production visibility wrapper requires a native peer. Drive its
    // unchanged visible-frame body directly so this remains headless on CI.
    void tick (double dt = 1.0 / 60.0) { Visual::advanceFrame (*imager, dt); }
    void settle() { for (int i = 0; i < 180; ++i) tick(); }
};

static bool nearHz (float a, float b)
{ return std::isfinite (a) && std::abs (a - b) <= juce::jmax (0.002f, std::abs (b) * 3.0e-6f); }

static void test (bool report = false)
{
    std::printf ("State test 158: Multiband frequency text, placement and display transitions\n");
    struct Input { const char* text; float hz; };
    const Input valid[] {
        { "20", 20 }, { "20Hz", 20 }, { "20 Hz", 20 }, { "19", 19000 },
        { "19k", 19000 }, { "19 k", 19000 }, { "19 kHz", 19000 }, { "19 K", 19000 },
        { "1", 1000 }, { "1k", 1000 }, { "1 khz", 1000 }, { "1.25", 1250 }, { "1.25k", 1250 },
        { "21", 21 }, { "21k", 20000 }, { "0", 20 }, { "3", 3000 }, { "0.1", 100 },
        { "0.1k", 100 }, { "0.578", 578 }, { "1.578", 1578 }, { "1.578k", 1578 },
        { "-10", 20 }, { "-10k", 20 }, { "190k", 20000 }, { "190 kHz", 20000 },
        { "1,000", 1000 }, { "1,5kHz", 15000 }, { "1.5kHz", 1500 }, { "1,0,0,00", 10000 },
        { "10,000", 10000 }, { "1,8k", 18000 }, { "1.8k", 1800 }, { "1.8 K", 1800 },
        { "19Hz", 20 }, { "20kHz", 20000 }, { "20k", 20000 }, { "20.0Hz", 20 }, { "20.5Hz", 20.5f },
        { "20.5", 20.5f }, { "20/20Hz", 2020 }, { "1 k Hz", 1000 }, { "1 K H Z", 1000 },
        { " 1 \t. 8\n k H z ", 1800 }, { "19.999", 19999 }, { "20.0001", 20.0001f },
        { "0Hz", 20 }, { "-0.1 K H Z", 20 }, { "+1.5k", 1500 }, { "(1'500)Hz!", 1500 },
        { ".5k", 500 }, { "20.", 20 }, { "20000", 20000 }, { "20001 Hz", 20000 },
        { "0.000001", 20 }, { "1Hz", 20 }, { "9999999999999999999999999999999999999999999999999999k", 20000 }
    };
    {
        Fixture f;
        for (const auto& input : valid)
        {
            for (int i = 0; i < 3; ++i)
            {
                auto* p = f.parameter (splitIds[i]);
                const float actual = p->convertFrom0to1 (p->getValueForText (input.text));
                const auto message = juce::String ("frequency host split ") + juce::String (i + 1) + ": " + input.text;
                check (nearHz (actual, input.hz), message.toRawUTF8());
                if (report) std::printf ("  input [%s] host%d=%.7g expected=%.7g\n", input.text, i + 1, actual, input.hz);
            }
            f.set (splitIds[0], 777);
            f.commit (input.text);
            check (nearHz (f.plain(), input.hz), (juce::String ("frequency inline: ") + input.text).toRawUTF8());
            if (report) std::printf ("  input [%s] inline=%.7g expected=%.7g\n", input.text, f.plain(), input.hz);
        }
        // These retain their original bare-Hz conventions and normalization.
        auto* mono = f.parameter (pid::monoMakerFreq);
        auto* chorus = f.parameter (pid::chorusRate);
        check (nearHz (mono->convertFrom0to1 (mono->getValueForText ("1")), 20), "frequency: Mono Maker keeps bare Hz");
        check (nearHz (chorus->convertFrom0to1 (chorus->getValueForText ("1")), 1), "frequency: Chorus keeps bare Hz");
    }

    const char* invalid[] { "", " \t\n ", ",;!/+", ".", "-", "abc", "20abc", "20Hzfoo",
        "2kkHz", "20hzz", "NaN", "Inf", "Infinity", "1..5", "1.5.0k", "Hz", "kHz", "1e3", "k20" };
    {
        Fixture f;
        juce::MemoryBlock baseline;
        f.processor->getStateInformation (baseline);
        for (const auto* input : invalid)
        {
            f.processor->setStateInformation (baseline.getData(), (int) baseline.getSize());
            f.processor->pollUndoCoalesce();
            const auto before = f.row();
            f.notifications = {};
            f.sweeps = 0;
            for (int i = 0; i < 3; ++i)
            {
                auto* p = f.parameter (splitIds[i]);
                check (juce::exactlyEqual (p->getValueForText (input), p->getValue()),
                       (juce::String ("frequency: invalid host text preserves readback: ") + input).toRawUTF8());
            }
            f.commit (input, 0, true);
            f.processor->pollUndoCoalesce();
            check (f.row() == before && f.notifications.writes == 0 && f.notifications.begins == 0
                       && f.notifications.ends == 0 && f.sweeps == 0 && ! Visual::easing (*f.imager)
                       && ! f.processor->canUndo(),
                   (juce::String ("frequency: invalid inline text has no effects: ") + input).toRawUTF8());
        }
    }

    for (int bands = 2; bands <= 4; ++bands)
        for (int split = 0; split < bands - 1; ++split)
            for (const auto* endpoint : { "20Hz", "20kHz" })
            {
                Fixture f (bands);
                const auto beforeEndpoint = f.row();
                f.commit (endpoint, split);
                if (bands == 4 && split == 1)
                {
                    const auto untouched = (size_t) (juce::String (endpoint) == "20Hz" ? 2 : 0);
                    check (juce::exactlyEqual (f.row()[untouched], beforeEndpoint[untouched]),
                           "frequency: an interior endpoint leaves the unrelated far neighbour untouched");
                }
                const auto plot = Visual::plot (*f.imager);
                const float x = Visual::splitX (*f.imager, f.plain (split));
                const float expectedX = juce::String (endpoint) == "20Hz"
                                      ? plot.getX() + 46.0f * (float) split
                                      : plot.getRight() - 46.0f * (float) (bands - 2 - split);
                check (std::abs (x - expectedX) < 0.01f, "frequency: endpoint placement respects only necessary neighbour spacing");
                for (int i = 1; i < bands - 1; ++i)
                    check (Visual::splitX (*f.imager, f.plain (i)) - Visual::splitX (*f.imager, f.plain (i - 1)) >= 45.99f,
                           "frequency: endpoint projection preserves ordered usable split gaps");
                f.settle(); f.notifications = {}; f.sweeps = 0;
                const auto beforeRepeat = f.row();
                f.commit (endpoint, split);
                if (report && f.notifications.writes != 0)
                    std::printf ("  repeated endpoint bands=%d split=%d text=%s norm before=%.9g after=%.9g delta=%.9g\n",
                                 bands, split, endpoint, beforeRepeat[(size_t) split], f.row()[(size_t) split],
                                 f.row()[(size_t) split] - beforeRepeat[(size_t) split]);
                check (f.notifications.writes == 0 && f.notifications.begins == 0 && f.sweeps == 0 && ! Visual::easing (*f.imager),
                       "frequency: repeating a constrained endpoint performs no edit");
            }

    for (const bool animations : { false, true })
        for (const bool focus : { false, true })
        {
            Fixture f (4);
            f.processor->getInternal().animationsValue() = animations;
            f.paint();
            const auto before = f.row();
            const float start = Visual::drawnSplit (*f.imager, 0);
            f.commit ("8k", 0, focus);
            const auto committed = f.row();
            check (before != committed && f.notifications.begins == 1 && f.notifications.ends == 1,
                   "frequency: Enter/focus commit writes immediately with one balanced gesture");
            f.paint();
            const float afterPaint = Visual::drawnSplit (*f.imager, 0);
            check (animations ? nearHz (afterPaint, start) : nearHz (afterPaint, f.plain()),
                   "frequency: first paint preserves the sweep origin or updates immediately when disabled");
            f.tick();
            const float next = Visual::drawnSplit (*f.imager, 0);
            check (animations ? next > start && next < f.plain() : nearHz (next, f.plain()),
                   "frequency: typed transition progresses only when animations are enabled");
            f.settle();
            for (int i = 0; i < 3; ++i)
                check (nearHz (Visual::drawnSplit (*f.imager, i), f.plain (i)), "frequency: coordinated neighbours converge");
            f.processor->pollUndoCoalesce();
            check (f.processor->canUndo(), "frequency: typed multi-split edit is undoable");
            f.click ("Undo");
            check (f.row() == before && ! f.processor->canUndo(), "frequency: one Undo restores the exact whole row");
            f.paint();
            check (animations ? nearHz (Visual::drawnSplit (*f.imager, 0),
                                        f.parameter (splitIds[0])->convertFrom0to1 (committed[0]))
                              : nearHz (Visual::drawnSplit (*f.imager, 0), f.plain()),
                   "frequency: existing Undo sweep follows animation settings");
            f.settle();
            f.click ("Redo");
            check (f.row() == committed, "frequency: Redo restores the exact committed row");
            f.settle();
        }

    {
        Fixture f;
        const auto before = f.row();
        auto* text = f.open();
        if (text != nullptr) { text->setText ("5k", false); text->onEscapeKey(); }
        f.commit ("180Hz");
        check (f.row() == before && f.notifications.writes == 0 && f.notifications.begins == 0 && f.sweeps == 0,
               "frequency: Escape and equivalent valid input have no effects");
        f.commit ("3k"); f.tick();
        f.commit ("6k", 0, true); f.tick();
        check (Visual::easing (*f.imager) && f.plain() > Visual::drawnSplit (*f.imager, 0),
               "frequency: rapid consecutive edits retain movement");
        f.set (splitIds[0], 9000); f.settle();
        check (nearHz (f.plain(), 9000) && nearHz (Visual::drawnSplit (*f.imager, 0), 9000),
               "frequency: active animation converges to newer host authority");
        f.commit ("4k"); f.tick();
        f.processor->getInternal().animationsValue() = false;
        f.paint();
        check (nearHz (Visual::drawnSplit (*f.imager, 0), f.plain()), "frequency: disabling animations snaps even before the next tick");
        f.processor->getInternal().animationsValue() = true;
        f.commit ("2k"); f.tick();
        f.set (pid::mbBands, 1); f.paint();
        check (! Visual::easing (*f.imager), "frequency: topology change clears the display tail");
        f.set (pid::mbBands, 2);
        f.imager->setVisible (false); Visual::tick (*f.imager, 1.0 / 60.0);
        f.imager->setVisible (true); f.settle();
        text = f.open();
        check (text != nullptr, "frequency: editor reopens after hide/show and topology changes");
        f.imager->cancelInlineEdit();
    }

    {
        Fixture f (4);
        f.set (splitIds[1], 100); f.paint(); // a host-owned crowded/inverted row
        const auto before = f.row();
        f.notifications = {};
        f.commit ("180Hz");
        check (f.row() == before && f.notifications.writes == 0 && f.notifications.begins == 0
                   && ! Visual::easing (*f.imager), "frequency: same numeric input preserves a host-owned crowded layout");
    }

    {
        Fixture sixty, oneTwenty;
        sixty.commit ("5k"); oneTwenty.commit ("5k");
        for (int i = 0; i < 6; ++i) sixty.tick (1.0 / 60.0);
        for (int i = 0; i < 12; ++i) oneTwenty.tick (1.0 / 120.0);
        check (nearHz (Visual::drawnSplit (*sixty.imager, 0), Visual::drawnSplit (*oneTwenty.imager, 0))
                   && Visual::drawnSplit (*sixty.imager, 0) < sixty.plain(),
               "frequency: equal elapsed time at 60/120 Hz gives the same intermediate position");
        sixty.settle(); oneTwenty.settle();
        check (nearHz (Visual::drawnSplit (*sixty.imager, 0), sixty.plain())
                   && ! Visual::easing (*sixty.imager), "frequency: a typed tail converges without any global sweep window");
    }

    for (const bool animations : { false, true })
    {
        Fixture f (4);
        f.processor->getInternal().animationsValue() = animations;
        f.set (splitIds[0], 5000); f.paint();
        check (nearHz (Visual::drawnSplit (*f.imager, 0), f.plain()), "frequency: idle host automation paints immediately");
        const float start = f.plain();
        f.imager->mouseDoubleClick (f.event ({ Visual::splitX (*f.imager, start), Visual::plot (*f.imager).getY() + 10 }));
        f.paint();
        check (animations ? nearHz (Visual::drawnSplit (*f.imager, 0), start)
                          : nearHz (Visual::drawnSplit (*f.imager, 0), f.plain()),
               "frequency: existing reset sweep follows animation settings");
        f.settle();
        f.set (splitIds[0], 12345); f.paint(); f.tick(); f.settle();
        const float presetStart = Visual::drawnSplit (*f.imager, 0);
        const float oldBands = f.parameter (pid::mbBands)->getValue();
        f.click ("Next preset");
        check (! nearHz (f.plain(), presetStart), "frequency: production preset load changes the crossover");
        f.paint();
        const bool sameBands = juce::exactlyEqual (oldBands, f.parameter (pid::mbBands)->getValue());
        check (animations && sameBands ? nearHz (Visual::drawnSplit (*f.imager, 0), presetStart)
                                      : nearHz (Visual::drawnSplit (*f.imager, 0), f.plain()),
               "frequency: existing preset sweep/topology snap follows animation settings");
        f.settle();
    }

    for (const bool animations : { false, true })
    {
        Fixture f (4);
        f.processor->getInternal().animationsValue() = animations;
        f.set (splitIds[0], 5000); f.paint();
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            const float start = Visual::drawnSplit (*f.imager, 0);
            f.click ("A/B Compare"); f.paint();
            check (! nearHz (f.plain(), start)
                       && (animations ? nearHz (Visual::drawnSplit (*f.imager, 0), start)
                                      : nearHz (Visual::drawnSplit (*f.imager, 0), f.plain())),
                   "frequency: existing A/B sweep follows animation settings");
            f.settle();
            check (nearHz (Visual::drawnSplit (*f.imager, 0), f.plain()), "frequency: A/B sweep converges on both legs");
        }
    }

    {
        Fixture f;
        f.commit ("8k"); f.tick();
        const auto point = juce::Point<float> { Visual::splitX (*f.imager, f.plain()), Visual::plot (*f.imager).getY() + 10 };
        f.imager->mouseDown (f.event (point, true)); f.tick(); f.paint();
        check (! Visual::easing (*f.imager) && nearHz (Visual::drawnSplit (*f.imager, 0), f.plain()),
               "frequency: a new drag takes over an active typed transition immediately");
        const float beforeDrag = f.plain();
        f.imager->mouseDrag (f.event (point.translated (-30, 0), true)); f.paint();
        check (! nearHz (f.plain(), beforeDrag) && nearHz (Visual::drawnSplit (*f.imager, 0), f.plain()),
               "frequency: an actual drag change tracks immediately");
        f.imager->mouseUp (f.event (point.translated (-30, 0)));
        juce::MouseWheelDetails wheel; wheel.deltaY = 0.2f;
        const float beforeWheel = f.plain();
        f.imager->mouseWheelMove (f.event ({ Visual::splitX (*f.imager, f.plain()), point.y }), wheel); f.paint();
        check (! nearHz (f.plain(), beforeWheel) && nearHz (Visual::drawnSplit (*f.imager, 0), f.plain()),
               "frequency: an actual wheel change tracks immediately");
    }

    // The visible-frame seam also retires ADR-0043's former headless hold-guard
    // gap. Control the timestamp, rather than sleeping or adding another timer.
    for (const bool stale : { false, true })
    {
        Fixture f (4);
        int previews = 0;
        const auto preview = f.imager->onSoloPreview;
        f.imager->onSoloPreview = [&] (int mask) { ++previews; if (preview) preview (mask); };
        f.imager->mouseDown (f.event (Visual::solo (*f.imager, 0).getCentre(), true));
        Visual::ageSoloPress (*f.imager);
        if (stale) f.set (pid::mbBands, 2);
        f.tick();
        check (previews == (stale ? 0 : 1), "frequency frame: stale topology cannot promote a held solo; live control does");
        f.imager->cancelActiveDrag();
    }
}
} // namespace multiband_frequency_tests
