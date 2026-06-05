#include "PluginEditor.h"

using namespace anamorph::gui;

AnamorphAudioProcessorEditor::AnamorphAudioProcessorEditor (AnamorphAudioProcessor& p)
    : juce::AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lnf);

    // GPU-composite everything (spec section 8 / 21).
    openGLContext.setContinuousRepainting (false);
    openGLContext.attachTo (*this);

    scope = std::make_unique<Vectorscope> (processor.getEngine().getScopeBuffer());
    corrH = std::make_unique<CorrelationMeter> (processor.getEngine().getCorrelation(),
                                                CorrelationMeter::Orientation::Horizontal);
    corrV = std::make_unique<CorrelationMeter> (processor.getEngine().getCorrelation(),
                                                CorrelationMeter::Orientation::Vertical);
    addAndMakeVisible (*scope);
    addAndMakeVisible (*corrH);
    addAndMakeVisible (*corrV);

    // --- Top bar: A/B (understated), Advanced, Bypass ---
    for (auto* b : { &aButton, &bButton, &copyToB, &copyToA })
        addAndMakeVisible (b);
    aButton.setToggleState (true, juce::dontSendNotification);
    aButton.onClick = [this] { switchTo (0); };
    bButton.onClick = [this] { switchTo (1); };
    copyToB.onClick = [this] { copyAB (0, 1); };
    copyToA.onClick = [this] { copyAB (1, 0); };

    setupToggle (advancedToggle, pid::advancedMode, "Advanced");
    advanced = advancedToggle.getToggleState();
    advancedToggle.onClick = [this]
    {
        advanced = advancedToggle.getToggleState();
        setSize (kWidth, advanced ? kHeightAdvanced : kHeightSimple);
        updateModeVisibility();
    };
    setupToggle (bypassToggle, pid::bypass, "Bypass");

    // --- Core controls ---
    setupCombo (algorithmBox, pid::algorithm);
    algorithmBox.onChange = [this] { updateAlgoControls(); resized(); };
    algorithmLabel.setText ("Algorithm", juce::dontSendNotification);
    algorithmLabel.setJustificationType (juce::Justification::centred);
    algorithmLabel.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (algorithmLabel);

    setupRotary (driveK, driveL, "Drive");      attachSlider (driveK, pid::drive);
    setupRotary (widthK, widthL, "Width");      attachSlider (widthK, pid::width);
    setupRotary (mixK,   mixL,   "Mix");        attachSlider (mixK,   pid::mix);
    setupRotary (outputK,outputL,"Output");     attachSlider (outputK,pid::outputGain);

    setupRotary (haasDelayK,   haasDelayL,   "Delay");   attachSlider (haasDelayK,   pid::haasDelay);
    setupRotary (velvetK,      velvetL,      "Density"); attachSlider (velvetK,      pid::velvetDensity);
    setupRotary (chorusRateK,  chorusRateL,  "Rate");    attachSlider (chorusRateK,  pid::chorusRate);
    setupRotary (chorusDepthK, chorusDepthL, "Depth");   attachSlider (chorusDepthK, pid::chorusDepth);
    setupRotary (dimAmountK,   dimAmountL,   "Amount");  attachSlider (dimAmountK,   pid::dimAmount);

    // --- Mono maker ---
    setupToggle (monoMakerToggle, pid::monoMakerOn, "Mono Maker");
    setupRotary (monoFreqK, monoFreqL, "Freq");  attachSlider (monoFreqK, pid::monoMakerFreq);

    // --- Auto gain ---
    setupToggle (autoMatchToggle, pid::autoGainMatch, "Match");
    addAndMakeVisible (applyGainButton);
    applyGainButton.onClick = [this] { processor.applyAutoGain(); };
    matchReadout.setJustificationType (juce::Justification::centred);
    matchReadout.setColour (juce::Label::textColourId, colours::textDim);
    matchReadout.setFont (juce::Font (juce::FontOptions (11.0f)));
    addAndMakeVisible (matchReadout);

    // --- Advanced: input conditioning + monitoring ---
    setupToggle (msToggle,   pid::msMode,    "MS Mode");
    setupToggle (swapToggle, pid::swap,      "Swap L/R");
    setupToggle (polLToggle, pid::polarityL, "Pol L");
    setupToggle (polRToggle, pid::polarityR, "Pol R");
    setupRotary (balanceK, balanceL, "Balance");  attachSlider (balanceK, pid::inputBalance);

    setupCombo (channelModeBox, pid::channelMode);
    channelModeLabel.setText ("Channel", juce::dontSendNotification);
    channelModeLabel.setJustificationType (juce::Justification::centred);
    channelModeLabel.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (channelModeLabel);

    setupCombo (haasSideBox, pid::haasSide);
    setupCombo (dimModeBox,  pid::dimMode);

    setupCombo (soloBox, pid::solo);
    soloLabel.setText ("Solo", juce::dontSendNotification);
    soloLabel.setJustificationType (juce::Justification::centred);
    soloLabel.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (soloLabel);

    setupCombo (oversampleBox, pid::oversample);
    oversampleLabel.setText ("Oversampling", juce::dontSendNotification);
    oversampleLabel.setJustificationType (juce::Justification::centred);
    oversampleLabel.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (oversampleLabel);

    // --- Advanced: multiband ---
    setupToggle (mbEnableToggle, pid::mbEnable, "Multiband");
    setupRotary (mbFreqLowK,  mbFreqLowL,  "Lo/Mid"); attachSlider (mbFreqLowK,  pid::mbFreqLow);
    setupRotary (mbFreqHighK, mbFreqHighL, "Mid/Hi"); attachSlider (mbFreqHighK, pid::mbFreqHigh);
    setupRotary (mbWLowK,  mbWLowL,  "W Low");  attachSlider (mbWLowK,  pid::mbWidthLow);
    setupRotary (mbWMidK,  mbWMidL,  "W Mid");  attachSlider (mbWMidK,  pid::mbWidthMid);
    setupRotary (mbWHighK, mbWHighL, "W High"); attachSlider (mbWHighK, pid::mbWidthHigh);

    // --- Advanced: scope persistence ---
    setupRotary (scopePersistK, scopePersistL, "Persist");
    attachSlider (scopePersistK, pid::scopePersist);
    scopePersistK.onValueChange = [this]
    { scope->setPersistence ((float) scopePersistK.getValue()); };
    scope->setPersistence ((float) scopePersistK.getValue());

    // A/B: seed both slots from the current state.
    stateA = processor.getAPVTS().copyState();
    stateB = stateA.createCopy();

    updateAlgoControls();
    updateModeVisibility();
    setSize (kWidth, advanced ? kHeightAdvanced : kHeightSimple);
    startTimerHz (8);
}

AnamorphAudioProcessorEditor::~AnamorphAudioProcessorEditor()
{
    stopTimer();
    openGLContext.detach();
    setLookAndFeel (nullptr);
}

// ----------------------------------------------------------------------------
void AnamorphAudioProcessorEditor::setupRotary (juce::Slider& s, juce::Label& l, const juce::String& name)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 15);
    s.setColour (juce::Slider::textBoxTextColourId, colours::text);
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setColour (juce::Label::textColourId, colours::textDim);
    l.setFont (juce::Font (juce::FontOptions (11.5f)));
    addAndMakeVisible (l);
}

void AnamorphAudioProcessorEditor::attachSlider (juce::Slider& s, const char* id)
{
    sliderAtts.add (new SliderAttachment (processor.getAPVTS(), id, s));
}

void AnamorphAudioProcessorEditor::setupCombo (juce::ComboBox& box, const char* id)
{
    if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (processor.getAPVTS().getParameter (id)))
        box.addItemList (cp->choices, 1);
    addAndMakeVisible (box);
    comboAtts.add (new ComboBoxAttachment (processor.getAPVTS(), id, box));
}

void AnamorphAudioProcessorEditor::setupToggle (juce::ToggleButton& t, const char* id, const juce::String& text)
{
    t.setButtonText (text);
    addAndMakeVisible (t);
    buttonAtts.add (new ButtonAttachment (processor.getAPVTS(), id, t));
}

// ----------------------------------------------------------------------------
void AnamorphAudioProcessorEditor::updateAlgoControls()
{
    const int algo = algorithmBox.getSelectedItemIndex(); // 0 Haas,1 Velvet,2 Chorus,3 Dim-D

    haasDelayK.setVisible (algo == 0);  haasDelayL.setVisible (algo == 0);
    velvetK.setVisible    (algo == 1);  velvetL.setVisible    (algo == 1);
    chorusRateK.setVisible (algo == 2); chorusRateL.setVisible (algo == 2);
    chorusDepthK.setVisible (algo == 2);chorusDepthL.setVisible (algo == 2);
    dimAmountK.setVisible  (algo == 3); dimAmountL.setVisible  (algo == 3);

    // Algorithm-specific selectors live in the Advanced strip.
    haasSideBox.setVisible (advanced && algo == 0);
    dimModeBox.setVisible  (advanced && algo == 3);
}

void AnamorphAudioProcessorEditor::updateModeVisibility()
{
    const bool a = advanced;
    juce::Component* comps[] = { &msToggle, &swapToggle, &polLToggle, &polRToggle,
                                 &balanceK, &balanceL, &channelModeBox, &channelModeLabel,
                                 &soloBox, &soloLabel, &oversampleBox, &oversampleLabel,
                                 &mbEnableToggle, &mbFreqLowK, &mbFreqLowL, &mbFreqHighK, &mbFreqHighL,
                                 &mbWLowK, &mbWLowL, &mbWMidK, &mbWMidL, &mbWHighK, &mbWHighL,
                                 &scopePersistK, &scopePersistL };
    for (juce::Component* c : comps)
        c->setVisible (a);

    updateAlgoControls();
    resized();
}

// ----------------------------------------------------------------------------
void AnamorphAudioProcessorEditor::timerCallback()
{
    // Keep the editor in sync with host automation / preset recall.
    static int lastAlgo = -1;
    if (algorithmBox.getSelectedItemIndex() != lastAlgo)
    {
        lastAlgo = algorithmBox.getSelectedItemIndex();
        updateAlgoControls();
        resized();
    }
    if (advancedToggle.getToggleState() != advanced)
    {
        advanced = advancedToggle.getToggleState();
        setSize (kWidth, advanced ? kHeightAdvanced : kHeightSimple);
        updateModeVisibility();
    }

    const float mg = processor.getEngine().getMatchGainDb();
    matchReadout.setText (juce::String (mg, 1) + " dB", juce::dontSendNotification);
}

// ----------------------------------------------------------------------------
//  A/B compare
// ----------------------------------------------------------------------------
void AnamorphAudioProcessorEditor::captureTo (int slot)
{
    (slot == 1 ? stateB : stateA) = processor.getAPVTS().copyState();
}

void AnamorphAudioProcessorEditor::switchTo (int slot)
{
    if (slot == activeSlot) return;
    captureTo (activeSlot);
    activeSlot = slot;
    processor.getAPVTS().replaceState ((slot == 1 ? stateB : stateA).createCopy());
    aButton.setToggleState (slot == 0, juce::dontSendNotification);
    bButton.setToggleState (slot == 1, juce::dontSendNotification);
    repaint();
}

void AnamorphAudioProcessorEditor::copyAB (int from, int to)
{
    captureTo (activeSlot); // make sure the live edits are stored first
    (to == 1 ? stateB : stateA) = (from == 1 ? stateB : stateA).createCopy();
    if (to == activeSlot)
        processor.getAPVTS().replaceState ((to == 1 ? stateB : stateA).createCopy());
}

// ----------------------------------------------------------------------------
void AnamorphAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);

    auto top = getLocalBounds().removeFromTop (46).toFloat();
    g.setColour (colours::bgPanel);
    g.fillRect (top);
    g.setColour (colours::outline);
    g.drawLine (0, top.getBottom(), (float) getWidth(), top.getBottom(), 1.0f);

    // Wordmark
    g.setColour (colours::text);
    g.setFont (juce::Font (juce::FontOptions (22.0f)).withExtraKerningFactor (0.18f));
    g.drawText ("ANAMORPH", 18, 0, 240, 46, juce::Justification::centredLeft);
    g.setColour (colours::accent);
    g.setFont (juce::Font (juce::FontOptions (10.0f)).withExtraKerningFactor (0.25f));
    g.drawText ("STEREO TOOLS", 168, 0, 140, 46, juce::Justification::centredLeft);

    // Right control panel backing
    auto panel = juce::Rectangle<int> (getWidth() - 330, 46, 330, getHeight() - 46).toFloat().reduced (8.0f);
    g.setColour (colours::bgPanel.withAlpha (0.6f));
    g.fillRoundedRectangle (panel, 8.0f);

    if (advanced)
    {
        auto adv = juce::Rectangle<int> (0, getHeight() - 200, getWidth(), 200).toFloat().reduced (8.0f, 6.0f);
        g.setColour (colours::bgPanel.withAlpha (0.5f));
        g.fillRoundedRectangle (adv, 8.0f);
        g.setColour (colours::textDim);
        g.setFont (juce::Font (juce::FontOptions (10.0f)).withExtraKerningFactor (0.2f));
        g.drawText ("ADVANCED", (int) adv.getX() + 10, (int) adv.getY() + 4, 120, 14, juce::Justification::centredLeft);
    }
}

// ----------------------------------------------------------------------------
void AnamorphAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (46);

    // Top bar (right-aligned cluster)
    {
        auto bar = top.reduced (8, 9);
        bypassToggle.setBounds (bar.removeFromRight (90));
        advancedToggle.setBounds (bar.removeFromRight (110));
        bar.removeFromRight (16);
        copyToA.setBounds (bar.removeFromRight (44));
        copyToB.setBounds (bar.removeFromRight (44));
        bButton.setBounds (bar.removeFromRight (34));
        aButton.setBounds (bar.removeFromRight (34));
    }

    auto rightPanel = r.removeFromRight (330);
    juce::Rectangle<int> advArea;
    if (advanced)
        advArea = r.removeFromBottom (200);

    // ---- Scope + correlation meters ----
    {
        auto sa = r.reduced (16);
        auto vCol = sa.removeFromRight (24);
        sa.removeFromRight (8);
        auto hRow = sa.removeFromBottom (24);
        sa.removeFromBottom (8);

        // Keep the scope square and centred in the remaining area.
        const int side = juce::jmin (sa.getWidth(), sa.getHeight());
        auto sq = sa.withSizeKeepingCentre (side, side);
        scope->setBounds (sq);
        corrV->setBounds (vCol.withHeight (side).withY (sq.getY()));
        corrH->setBounds (hRow.withWidth (side).withX (sq.getX()));
    }

    // ---- Right control panel ----
    {
        auto p = rightPanel.reduced (18, 14);

        auto algoRow = p.removeFromTop (52);
        algorithmLabel.setBounds (algoRow.removeFromTop (16));
        algorithmBox.setBounds (algoRow.reduced (0, 2));

        p.removeFromTop (8);

        auto knobRow = [&] (juce::Slider& s1, juce::Label& l1, juce::Slider& s2, juce::Label& l2)
        {
            auto row = p.removeFromTop (96);
            auto a = row.removeFromLeft (row.getWidth() / 2);
            l1.setBounds (a.removeFromBottom (16));
            s1.setBounds (a.reduced (4, 2));
            l2.setBounds (row.removeFromBottom (16));
            s2.setBounds (row.reduced (4, 2));
        };

        knobRow (driveK, driveL, widthK, widthL);
        knobRow (mixK, mixL, outputK, outputL);

        // Algorithm-specific row (chorus shows two knobs, others one centred).
        {
            auto row = p.removeFromTop (96);
            auto a = row.removeFromLeft (row.getWidth() / 2);
            auto b = row;
            auto place = [] (juce::Rectangle<int> area, juce::Slider& s, juce::Label& l)
            { l.setBounds (area.removeFromBottom (16)); s.setBounds (area.reduced (4, 2)); };
            place (a, haasDelayK, haasDelayL);
            place (a, velvetK, velvetL);
            place (a, chorusRateK, chorusRateL);
            place (a, dimAmountK, dimAmountL);
            place (b, chorusDepthK, chorusDepthL);
        }

        p.removeFromTop (6);

        // Mono maker
        {
            auto row = p.removeFromTop (84);
            monoMakerToggle.setBounds (row.removeFromLeft (row.getWidth() / 2).removeFromTop (28).reduced (2));
            auto k = row;
            monoFreqL.setBounds (k.removeFromBottom (16));
            monoFreqK.setBounds (k.reduced (4, 2));
        }

        p.removeFromTop (6);

        // Auto gain
        {
            auto row = p.removeFromTop (40);
            autoMatchToggle.setBounds (row.removeFromLeft (110).reduced (2));
            applyGainButton.setBounds (row.removeFromLeft (80).reduced (3));
            matchReadout.setBounds (row.reduced (2));
        }
    }

    // ---- Advanced strip ----
    if (advanced)
    {
        auto a = advArea.reduced (14, 22);

        // Row 1: toggles + selectors
        auto row1 = a.removeFromTop (40);
        msToggle.setBounds   (row1.removeFromLeft (110).reduced (2, 8));
        swapToggle.setBounds (row1.removeFromLeft (110).reduced (2, 8));
        polLToggle.setBounds (row1.removeFromLeft (80).reduced (2, 8));
        polRToggle.setBounds (row1.removeFromLeft (80).reduced (2, 8));
        row1.removeFromLeft (10);

        auto comboCell = [] (juce::Rectangle<int>& src, int w, juce::Label& lab, juce::ComboBox& box)
        {
            auto cell = src.removeFromLeft (w);
            lab.setBounds (cell.removeFromTop (15));
            box.setBounds (cell.reduced (2, 1));
        };
        comboCell (row1, 110, channelModeLabel, channelModeBox);
        comboCell (row1, 90,  soloLabel, soloBox);
        comboCell (row1, 110, oversampleLabel, oversampleBox);

        a.removeFromTop (8);

        // Row 2: balance + haas side / dim mode + multiband
        auto row2 = a;
        auto placeKnob = [] (juce::Rectangle<int>& src, int w, juce::Slider& s, juce::Label& l)
        {
            auto cell = src.removeFromLeft (w);
            l.setBounds (cell.removeFromBottom (16));
            s.setBounds (cell.reduced (4, 0));
        };
        placeKnob (row2, 82, balanceK, balanceL);

        // Haas side / Dim mode share a slot depending on algorithm.
        {
            auto cell = row2.removeFromLeft (110);
            haasSideBox.setBounds (cell.withHeight (26).withY (cell.getCentreY() - 13).reduced (2, 0));
            dimModeBox.setBounds  (cell.withHeight (26).withY (cell.getCentreY() - 13).reduced (2, 0));
        }

        row2.removeFromLeft (10);
        mbEnableToggle.setBounds (row2.removeFromLeft (110).withHeight (26).withY (row2.getCentreY() - 13).reduced (2, 0));
        placeKnob (row2, 78, mbFreqLowK, mbFreqLowL);
        placeKnob (row2, 78, mbFreqHighK, mbFreqHighL);
        placeKnob (row2, 70, mbWLowK, mbWLowL);
        placeKnob (row2, 70, mbWMidK, mbWMidL);
        placeKnob (row2, 70, mbWHighK, mbWHighL);
        placeKnob (row2, 78, scopePersistK, scopePersistL);
    }
}
