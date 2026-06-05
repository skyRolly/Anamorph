#include "PluginEditor.h"

using namespace anamorph::gui;

#ifndef ANAMORPH_VERSION_STRING
 #define ANAMORPH_VERSION_STRING "0.3.0"
#endif
#ifndef ANAMORPH_BUILD_NUMBER
 #define ANAMORPH_BUILD_NUMBER "0"
#endif

// ============================================================================
void AnamorphAudioProcessorEditor::Backdrop::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xcc06080b));

    g.setColour (colours::bgPanel);
    g.fillRoundedRectangle (panel.toFloat(), 12.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (panel.toFloat().reduced (0.5f), 12.0f, 1.0f);

    if (aboutText)
    {
        auto r = panel.reduced (28, 24);
        g.setColour (colours::text);
        g.setFont (juce::Font (juce::FontOptions (27.0f)).withExtraKerningFactor (0.16f));
        g.drawText ("ANAMORPH", r.removeFromTop (36), juce::Justification::centredLeft);

        g.setColour (colours::accent);
        g.setFont (juce::Font (juce::FontOptions (12.0f)).withExtraKerningFactor (0.22f));
        g.drawText ("STEREO TOOLS", r.removeFromTop (20), juce::Justification::centredLeft);

        r.removeFromTop (12);
        g.setColour (colours::textDim);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        const juce::String body =
            juce::String ("Version ") + ANAMORPH_VERSION_STRING + "  —  build " + ANAMORPH_BUILD_NUMBER + "\n"
            "Rolly Tech\n\n"
            "A stereo-field toolkit: turn mono into stereo, shape width, and\n"
            "keep mono compatibility — around a precision vectorscope.";
        g.drawMultiLineText (body, r.getX(), r.getY() + 16, r.getWidth(), juce::Justification::left);

        // Copyright line at the bottom of the panel (#2).
        g.setColour (colours::textDim.withAlpha (0.7f));
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (juce::String ("© 2026 Rolly Tech. All rights reserved."),
                    panel.reduced (28, 16).removeFromBottom (16), juce::Justification::centredLeft);
    }
}

void AnamorphAudioProcessorEditor::ABControl::paint (juce::Graphics& g)
{
    const int active = getActive ? getActive() : 0;
    auto b = getLocalBounds();
    g.setFont (juce::Font (juce::FontOptions (15.0f)).withExtraKerningFactor (0.05f));

    auto aRect = b.removeFromLeft (juce::roundToInt (b.getWidth() * 0.42f));
    auto bRect = b.removeFromRight (juce::roundToInt (getWidth() * 0.42f));

    g.setColour (colours::textDim.withAlpha (0.7f));
    g.drawText ("/", b, juce::Justification::centred);

    g.setColour (active == 0 ? colours::accent : colours::textDim);
    g.drawText ("A", aRect, juce::Justification::centredRight);
    g.setColour (active == 1 ? colours::accent : colours::textDim);
    g.drawText ("B", bRect, juce::Justification::centredLeft);
}

// ============================================================================
AnamorphAudioProcessorEditor::AnamorphAudioProcessorEditor (AnamorphAudioProcessor& p)
    : juce::AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lnf);
    tooltips.setLookAndFeel (&lnf);

    openGLContext.setContinuousRepainting (false);
    openGLContext.attachTo (*this);

    scope = std::make_unique<Vectorscope> (processor.getEngine().getScopeBuffer());
    balanceMeter = std::make_unique<StereoMeter> (processor.getEngine().getCorrelation(),
                       StereoMeter::Orientation::Horizontal, StereoMeter::Type::Balance);
    corrMeter = std::make_unique<StereoMeter> (processor.getEngine().getCorrelation(),
                       StereoMeter::Orientation::Vertical, StereoMeter::Type::Correlation);
    levelMeter = std::make_unique<LevelMeter> (processor.getEngine().getLevels());
    addAndMakeVisible (*scope);
    addAndMakeVisible (*balanceMeter);
    addAndMakeVisible (*corrMeter);
    addChildComponent (*levelMeter);

    // --- Top bar ---
    titleButton.setComponentID ("ghost");
    titleButton.onClick = [this] { showAbout (true); };
    addAndMakeVisible (titleButton);

    abControl.getActive = [this] { return processor.abActiveSlot(); };
    abControl.onToggle  = [this] { processor.abSwitchTo (processor.abActiveSlot() == 0 ? 1 : 0); repaint(); };
    abControl.setTooltip ("A/B compare — click to switch. Copy stores the current sound into the other slot.");
    addAndMakeVisible (abControl);
    copyButton.onClick = [this] { processor.abCopyToOther(); };
    copyButton.setTooltip ("Copy the current settings into the other A/B slot.");
    addAndMakeVisible (copyButton);

    settingsButton.onClick = [this] { showSettings (true); }; // no tooltip (#20)
    addAndMakeVisible (settingsButton);

    undoButton.setButtonText (juce::String::charToString ((juce::juce_wchar) 0x21BA));
    redoButton.setButtonText (juce::String::charToString ((juce::juce_wchar) 0x21BB));
    undoButton.setTooltip ("Undo");
    redoButton.setTooltip ("Redo");
    undoButton.onClick = [this] { processor.getUndoManager().undo(); };
    redoButton.onClick = [this] { processor.getUndoManager().redo(); };
    addAndMakeVisible (undoButton);
    addAndMakeVisible (redoButton);

    metersToggle.setButtonText ("Meters");
    metersToggle.setTooltip ("Show input / output level meters.");
    metersToggle.onClick = [this] { metersOn = metersToggle.getToggleState(); levelMeter->setVisible (metersOn); resized(); };
    addAndMakeVisible (metersToggle);

    setupToggle (advancedToggle, pid::advancedMode, "Adv", "Show more controls."); // #17 / #20
    advanced = advancedToggle.getToggleState();
    advancedToggle.onClick = [this]
    {
        advanced = advancedToggle.getToggleState();
        setSize (kWidth, advanced ? kHeightAdvanced : kHeightSimple);
        updateModeVisibility();
    };
    setupToggle (bypassToggle, pid::bypass, "Bypass", {}); // no tooltip (#20)
    bypassToggle.setComponentID ("bypass"); // red when engaged (#8)

    // --- WIDEN module ---
    setupCombo (algorithmBox, pid::algorithm, "How stereo is created from mono.");
    algorithmBox.onChange = [this] { updateAlgoControls(); resized(); };
    algorithmLabel.setText ("WIDEN", juce::dontSendNotification);
    algorithmLabel.setJustificationType (juce::Justification::centredLeft);
    algorithmLabel.setColour (juce::Label::textColourId, colours::textDim);
    algorithmLabel.setFont (juce::Font (juce::FontOptions (11.0f)).withExtraKerningFactor (0.2f));
    addAndMakeVisible (algorithmLabel);

    setupCombo (haasSideBox, pid::haasSide, "Which side the sound leans toward.");
    setupCombo (dimModeBox,  pid::dimMode,  "Voicing of the Dim D widener (mimics the classic dimensional chorus).");

    setupRotary (driveK,  driveL,  "Drive",  "Adds gentle saturation / density, like driving an analog input.");
    setupRotary (amountK, amountL, "Amount", "How much widening. 0% is fully transparent.");
    setupRotary (widthK,  widthL,  "Width",  "Stereo width. 100% leaves the image unchanged.");
    attachSlider (driveK, pid::drive);
    attachSlider (amountK, pid::amount);
    attachSlider (widthK, pid::width);

    setupRotary (haasDelayK,   haasDelayL,   "Delay",   "Short delay that creates the Haas stereo offset.");
    setupRotary (velvetK,      velvetL,      "Density", "How dense the velvet-noise diffusion is.");
    setupRotary (chorusRateK,  chorusRateL,  "Rate",    "Chorus modulation speed.");
    setupRotary (chorusDepthK, chorusDepthL, "Depth",   "Chorus modulation depth.");
    attachSlider (haasDelayK,   pid::haasDelay);
    attachSlider (velvetK,      pid::velvetDensity);
    attachSlider (chorusRateK,  pid::chorusRate);
    attachSlider (chorusDepthK, pid::chorusDepth);

    // --- OUTPUT module ---
    setupRotary (mixK,        mixL,        "Mix",     "Dry / wet blend.");
    setupRotary (outputK,     outputL,     "Output",  "Output level.");
    setupRotary (outBalanceK, outBalanceL, "Balance", "Output left / right balance.");
    attachSlider (mixK,        pid::mix);
    attachSlider (outputK,     pid::outputGain);
    attachSlider (outBalanceK, pid::outputBalance);

    setupToggle (autoMatchToggle, pid::autoGainMatch, "Level Match",
                 "Match the processed loudness to the input so louder doesn't fool you."); // #5
    applyGainButton.setTooltip ("Bake the measured match into Output as a fixed value.");
    applyGainButton.onClick = [this] { processor.applyAutoGain(); };
    addAndMakeVisible (applyGainButton);
    matchReadout.setJustificationType (juce::Justification::centred);
    matchReadout.setColour (juce::Label::textColourId, colours::textDim);
    matchReadout.setFont (juce::Font (juce::FontOptions (11.0f)));
    addAndMakeVisible (matchReadout);

    // --- MONO MAKER ---
    setupToggle (monoMakerToggle, pid::monoMakerOn, "Mono Maker",
                 "Collapse everything below the frequency to mono (after widening).");
    setupRotary (monoFreqK, monoFreqL, "Freq", "Mono Maker crossover frequency.");
    monoFreqK.setSliderStyle (juce::Slider::LinearHorizontal);
    monoFreqK.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
    attachSlider (monoFreqK, pid::monoMakerFreq);

    // --- INPUT module (advanced) ---
    inputModuleLabel.setText ("INPUT", juce::dontSendNotification);
    inputModuleLabel.setJustificationType (juce::Justification::centredLeft);
    inputModuleLabel.setColour (juce::Label::textColourId, colours::textDim);
    inputModuleLabel.setFont (juce::Font (juce::FontOptions (11.0f)).withExtraKerningFactor (0.2f));
    addAndMakeVisible (inputModuleLabel);

    setupCombo (channelModeBox, pid::channelMode, "Use the full stereo input, or just one side.");
    channelModeLabel.setText ("Input Channel", juce::dontSendNotification);
    channelModeLabel.setJustificationType (juce::Justification::centredLeft);
    channelModeLabel.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (channelModeLabel);

    setupCombo (soloBox, pid::solo, "Listen to just the Mid or just the Side of the input.");
    soloLabel.setText ("M/S Solo", juce::dontSendNotification);
    soloLabel.setJustificationType (juce::Justification::centredLeft);
    soloLabel.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (soloLabel);

    setupToggle (monoToggle, pid::monoSum, "Mono",     "Sum the input to mono.");
    setupToggle (swapToggle, pid::swap,    "Swap L/R", "Swap the left and right channels.");
    setupToggle (msToggle,   pid::msMode,  "M/S Mode", "Process the effect in Mid/Side.");
    const juce::String ph = juce::String::charToString ((juce::juce_wchar) 0x00F8);
    setupToggle (polLToggle, pid::polarityL, ph + " L", "Flip the polarity (phase) of the left channel.");
    setupToggle (polRToggle, pid::polarityR, ph + " R", "Flip the polarity (phase) of the right channel.");

    setupRotary (balanceK, balanceL, "Input Balance", "Balance the input between L and R.");
    attachSlider (balanceK, pid::inputBalance);

    // --- MULTIBAND module (advanced, separate) ---
    multibandLabel.setText ("MULTIBAND", juce::dontSendNotification);
    multibandLabel.setJustificationType (juce::Justification::centredLeft);
    multibandLabel.setColour (juce::Label::textColourId, colours::textDim);
    multibandLabel.setFont (juce::Font (juce::FontOptions (11.0f)).withExtraKerningFactor (0.2f));
    addAndMakeVisible (multibandLabel);

    setupToggle (mbEnableToggle, pid::mbEnable, "On", "Independent width per low / mid / high band.");
    setupRotary (mbFreqLowK,  mbFreqLowL,  "Lo/Mid", "Low / Mid crossover.");
    setupRotary (mbFreqHighK, mbFreqHighL, "Mid/Hi", "Mid / High crossover.");
    setupRotary (mbWLowK,  mbWLowL,  "W Low",  "Low band width.");
    setupRotary (mbWMidK,  mbWMidL,  "W Mid",  "Mid band width.");
    setupRotary (mbWHighK, mbWHighL, "W High", "High band width.");
    attachSlider (mbFreqLowK,  pid::mbFreqLow);
    attachSlider (mbFreqHighK, pid::mbFreqHigh);
    attachSlider (mbWLowK,  pid::mbWidthLow);
    attachSlider (mbWMidK,  pid::mbWidthMid);
    attachSlider (mbWHighK, pid::mbWidthHigh);

    setupRotary (scopePersistK, scopePersistL, "Persist", "Vectorscope afterglow time.");
    attachSlider (scopePersistK, pid::scopePersist);
    scopePersistK.onValueChange = [this] { scope->setPersistence ((float) scopePersistK.getValue()); };
    scope->setPersistence ((float) scopePersistK.getValue());

    // --- Overlays ---
    dimOverlay.setInterceptsMouseClicks (false, false);
    addChildComponent (dimOverlay);

    aboutBackdrop.aboutText = true;
    aboutBackdrop.onDismiss = [this] { showAbout (false); };
    addChildComponent (aboutBackdrop);

    settingsBackdrop.onDismiss = [this] { showSettings (false); };
    addChildComponent (settingsBackdrop);

    settingsTitle.setText ("SETTINGS", juce::dontSendNotification);
    settingsTitle.setColour (juce::Label::textColourId, colours::textDim);
    settingsTitle.setFont (juce::Font (juce::FontOptions (12.0f)).withExtraKerningFactor (0.2f));
    settingsBackdrop.addAndMakeVisible (settingsTitle);

    setupCombo (oversampleBox, pid::oversample, "Oversampling for the nonlinear stages. Off (1x) = no latency.");
    oversampleLabel.setText ("Oversampling", juce::dontSendNotification);
    oversampleLabel.setColour (juce::Label::textColourId, colours::textDim);
    settingsBackdrop.addAndMakeVisible (oversampleLabel);
    settingsBackdrop.addAndMakeVisible (oversampleBox);

    tooltipsToggle.setButtonText ("Tooltips");
    tooltipsToggle.setTooltip ("Show these hover hints on every control."); // #22
    tooltipsToggle.setToggleState (tooltipsOn, juce::dontSendNotification);
    tooltipsToggle.onClick = [this] { tooltipsOn = tooltipsToggle.getToggleState(); applyTooltipsEnabled(); };
    settingsBackdrop.addAndMakeVisible (tooltipsToggle);

    applyTooltipsEnabled();
    updateAlgoControls();
    updateModeVisibility();
    setSize (kWidth, advanced ? kHeightAdvanced : kHeightSimple);
    startTimerHz (12);
}

AnamorphAudioProcessorEditor::~AnamorphAudioProcessorEditor()
{
    stopTimer();
    openGLContext.detach();
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

// ----------------------------------------------------------------------------
void AnamorphAudioProcessorEditor::setupRotary (juce::Slider& s, juce::Label& l,
                                                const juce::String& name, const juce::String& tip)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 15);
    s.setColour (juce::Slider::textBoxTextColourId, colours::text);
    s.setColour (juce::Slider::textBoxHighlightColourId, colours::accent.withAlpha (0.30f));
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setTooltip (tip);
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

void AnamorphAudioProcessorEditor::setupCombo (juce::ComboBox& box, const char* id, const juce::String& tip)
{
    if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (processor.getAPVTS().getParameter (id)))
        box.addItemList (cp->choices, 1);
    box.setTooltip (tip);
    addAndMakeVisible (box);
    comboAtts.add (new ComboBoxAttachment (processor.getAPVTS(), id, box));
}

void AnamorphAudioProcessorEditor::setupToggle (juce::ToggleButton& t, const char* id,
                                                const juce::String& text, const juce::String& tip)
{
    t.setButtonText (text);
    if (tip.isNotEmpty()) t.setTooltip (tip);
    addAndMakeVisible (t);
    buttonAtts.add (new ButtonAttachment (processor.getAPVTS(), id, t));
}

void AnamorphAudioProcessorEditor::applyTooltipsEnabled()
{
    tooltips.setMillisecondsBeforeTipAppears (tooltipsOn ? 600 : 0x3fffffff);
}

// ----------------------------------------------------------------------------
void AnamorphAudioProcessorEditor::updateAlgoControls()
{
    const int algo = algorithmBox.getSelectedItemIndex();
    haasDelayK.setVisible (algo == 0);  haasDelayL.setVisible (algo == 0);
    velvetK.setVisible    (algo == 1);  velvetL.setVisible    (algo == 1);
    chorusRateK.setVisible (algo == 2); chorusRateL.setVisible (algo == 2);
    chorusDepthK.setVisible (algo == 2);chorusDepthL.setVisible (algo == 2);
    haasSideBox.setVisible (algo == 0);
    dimModeBox.setVisible  (algo == 3);
}

void AnamorphAudioProcessorEditor::updateModeVisibility()
{
    juce::Component* adv[] = {
        &inputModuleLabel, &channelModeBox, &channelModeLabel, &soloBox, &soloLabel,
        &monoToggle, &swapToggle, &msToggle, &polLToggle, &polRToggle, &balanceK, &balanceL,
        &multibandLabel, &mbEnableToggle, &mbFreqLowK, &mbFreqLowL, &mbFreqHighK, &mbFreqHighL,
        &mbWLowK, &mbWLowL, &mbWMidK, &mbWMidL, &mbWHighK, &mbWHighL,
        &scopePersistK, &scopePersistL
    };
    for (auto* c : adv) c->setVisible (advanced);
    updateAlgoControls();
    resized();
}

void AnamorphAudioProcessorEditor::showAbout (bool show)    { aboutBackdrop.setVisible (show);    if (show) { aboutBackdrop.toFront (false); resized(); } }
void AnamorphAudioProcessorEditor::showSettings (bool show) { settingsBackdrop.setVisible (show); if (show) { settingsBackdrop.toFront (false); resized(); } }

// ----------------------------------------------------------------------------
void AnamorphAudioProcessorEditor::timerCallback()
{
    static int lastAlgo = -1;
    if (algorithmBox.getSelectedItemIndex() != lastAlgo)
    {
        lastAlgo = algorithmBox.getSelectedItemIndex();
        updateAlgoControls(); resized();
    }
    if (advancedToggle.getToggleState() != advanced)
    {
        advanced = advancedToggle.getToggleState();
        setSize (kWidth, advanced ? kHeightAdvanced : kHeightSimple);
        updateModeVisibility();
    }
    if (dimOverlay.isVisible() != bypassToggle.getToggleState())
    {
        dimOverlay.setVisible (bypassToggle.getToggleState());
        if (dimOverlay.isVisible()) dimOverlay.toFront (false);
    }

    undoButton.setEnabled (processor.getUndoManager().canUndo());
    redoButton.setEnabled (processor.getUndoManager().canRedo());
    matchReadout.setText (juce::String (processor.getEngine().getMatchGainDb(), 1) + " dB", juce::dontSendNotification);
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

    g.setColour (colours::text);
    g.setFont (juce::Font (juce::FontOptions (22.0f)).withExtraKerningFactor (0.18f));
    g.drawText ("ANAMORPH", 18, 0, 240, 46, juce::Justification::centredLeft);
    g.setColour (colours::accent);
    g.setFont (juce::Font (juce::FontOptions (10.0f)).withExtraKerningFactor (0.25f));
    g.drawText ("STEREO TOOLS", 168, 0, 140, 46, juce::Justification::centredLeft);

    auto panel = juce::Rectangle<int> (getWidth() - 300, 46, 300, getHeight() - 46).toFloat().reduced (8.0f);
    g.setColour (colours::bgPanel.withAlpha (0.55f));
    g.fillRoundedRectangle (panel, 10.0f);

    if (advanced)
    {
        auto adv = juce::Rectangle<int> (0, getHeight() - 210, getWidth() - 300, 210).toFloat().reduced (8.0f, 6.0f);
        g.setColour (colours::bgPanel.withAlpha (0.5f));
        g.fillRoundedRectangle (adv, 10.0f);
    }
}

// ----------------------------------------------------------------------------
void AnamorphAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();

    dimOverlay.setBounds (getLocalBounds().withTrimmedTop (46)); // exclude the toolbar (#8)
    aboutBackdrop.setBounds (getLocalBounds());
    settingsBackdrop.setBounds (getLocalBounds());
    {
        aboutBackdrop.panel = getLocalBounds().withSizeKeepingCentre (390, 240);
        auto s = getLocalBounds().withSizeKeepingCentre (340, 190);
        settingsBackdrop.panel = s;
        auto inner = s.reduced (24, 20);
        settingsTitle.setBounds (inner.removeFromTop (20));
        inner.removeFromTop (10);
        auto row1 = inner.removeFromTop (40);
        oversampleLabel.setBounds (row1.removeFromTop (16));
        oversampleBox.setBounds (row1.reduced (0, 1));
        inner.removeFromTop (12);
        tooltipsToggle.setBounds (inner.removeFromTop (28));
    }

    auto top = r.removeFromTop (46);
    {
        auto bar = top.reduced (8, 10);
        titleButton.setBounds (juce::Rectangle<int> (10, 0, 300, 46));
        bypassToggle.setBounds (bar.removeFromRight (80));
        advancedToggle.setBounds (bar.removeFromRight (60));
        bar.removeFromRight (8);
        settingsButton.setBounds (bar.removeFromRight (74));
        metersToggle.setBounds (bar.removeFromRight (74));
        bar.removeFromRight (10);
        redoButton.setBounds (bar.removeFromRight (26));
        undoButton.setBounds (bar.removeFromRight (26));
        bar.removeFromRight (12);
        copyButton.setBounds (bar.removeFromRight (44));   // smaller (#12)
        abControl.setBounds (bar.removeFromRight (52));
    }

    auto rightPanel = r.removeFromRight (300);
    juce::Rectangle<int> advArea;
    if (advanced) advArea = r.removeFromBottom (210);

    // ---- Scope + meters ----
    {
        auto sa = r.reduced (16);
        if (metersOn)
        {
            levelMeter->setBounds (sa.removeFromLeft (76));
            sa.removeFromLeft (10);
        }
        auto vCol = sa.removeFromRight (24);
        sa.removeFromRight (8);
        auto hRow = sa.removeFromBottom (24);
        sa.removeFromBottom (8);
        const int side = juce::jmin (sa.getWidth(), sa.getHeight());
        auto sq = sa.withSizeKeepingCentre (side, side);
        scope->setBounds (sq);
        corrMeter->setBounds (vCol.withHeight (side).withY (sq.getY()));
        balanceMeter->setBounds (hRow.withWidth (side).withX (sq.getX()));
    }

    // ---- Right control column ----
    {
        auto col = rightPanel.reduced (18, 12);
        auto placeKnob = [] (juce::Rectangle<int> area, juce::Slider& s, juce::Label& l)
        { l.setBounds (area.removeFromBottom (15)); s.setBounds (area.reduced (3, 1)); };

        algorithmLabel.setBounds (col.removeFromTop (15));
        auto algoRow = col.removeFromTop (26);
        algorithmBox.setBounds (algoRow.removeFromLeft (algoRow.getWidth() - 96).reduced (0, 1));
        algoRow.removeFromLeft (6);
        haasSideBox.setBounds (algoRow.reduced (0, 1));
        dimModeBox.setBounds  (haasSideBox.getBounds());
        col.removeFromTop (6);

        auto twoKnob = [&] (juce::Slider& s1, juce::Label& l1, juce::Slider& s2, juce::Label& l2)
        {
            auto row = col.removeFromTop (88);
            placeKnob (row.removeFromLeft (row.getWidth() / 2), s1, l1);
            placeKnob (row, s2, l2);
        };

        twoKnob (driveK, driveL, amountK, amountL);
        {
            auto row = col.removeFromTop (88);
            placeKnob (row.removeFromLeft (row.getWidth() / 2), widthK, widthL);
            auto charArea = row;
            placeKnob (charArea, haasDelayK, haasDelayL);
            placeKnob (charArea, velvetK, velvetL);
            placeKnob (charArea.withTrimmedRight (charArea.getWidth() / 2), chorusRateK, chorusRateL);
            placeKnob (charArea.withTrimmedLeft  (charArea.getWidth() / 2), chorusDepthK, chorusDepthL);
        }
        twoKnob (mixK, mixL, outputK, outputL);
        {
            auto row = col.removeFromTop (88);
            placeKnob (row.removeFromLeft (row.getWidth() / 2), outBalanceK, outBalanceL);
            autoMatchToggle.setBounds (row.removeFromTop (26).reduced (2));
            auto ar = row.removeFromTop (28);
            applyGainButton.setBounds (ar.removeFromLeft (64).reduced (3));
            matchReadout.setBounds (ar.reduced (2));
        }
        col.removeFromTop (4);
        auto mm = col.removeFromTop (30);
        monoMakerToggle.setBounds (mm.removeFromLeft (112).reduced (2, 4));
        monoFreqK.setBounds (mm.reduced (2, 4));
    }

    // ---- Advanced strip: INPUT module (top) + MULTIBAND module (bottom) ----
    if (advanced)
    {
        auto a = advArea.reduced (14, 8);
        auto placeKnob = [] (juce::Rectangle<int>& src, int w, juce::Slider& s, juce::Label& l)
        { auto c = src.removeFromLeft (w); l.setBounds (c.removeFromBottom (15)); s.setBounds (c.reduced (3, 0)); };

        // INPUT
        auto inputArea = a.removeFromTop (108);
        inputModuleLabel.setBounds (inputArea.removeFromTop (15));
        auto inRow1 = inputArea.removeFromTop (40);
        auto cell = [&] (juce::Rectangle<int>& src, int w, juce::Label& lab, juce::ComboBox& box)
        { auto c = src.removeFromLeft (w); lab.setBounds (c.removeFromTop (15)); box.setBounds (c.reduced (1, 1)); };
        cell (inRow1, 116, channelModeLabel, channelModeBox);
        cell (inRow1, 92,  soloLabel, soloBox);
        inRow1.removeFromLeft (12);
        placeKnob (inRow1, 96, balanceK, balanceL);
        auto inRow2 = inputArea.removeFromTop (30);
        monoToggle.setBounds (inRow2.removeFromLeft (74).reduced (2, 2));
        swapToggle.setBounds (inRow2.removeFromLeft (92).reduced (2, 2));
        msToggle.setBounds   (inRow2.removeFromLeft (92).reduced (2, 2));
        polLToggle.setBounds (inRow2.removeFromLeft (58).reduced (2, 2));
        polRToggle.setBounds (inRow2.removeFromLeft (58).reduced (2, 2));

        a.removeFromTop (4);
        // MULTIBAND
        multibandLabel.setBounds (a.removeFromTop (15));
        auto mbRow = a;
        mbEnableToggle.setBounds (mbRow.removeFromLeft (60).withHeight (24).withY (mbRow.getCentreY() - 18).reduced (2, 0));
        placeKnob (mbRow, 76, mbFreqLowK, mbFreqLowL);
        placeKnob (mbRow, 76, mbFreqHighK, mbFreqHighL);
        placeKnob (mbRow, 68, mbWLowK, mbWLowL);
        placeKnob (mbRow, 68, mbWMidK, mbWMidL);
        placeKnob (mbRow, 68, mbWHighK, mbWHighL);
        mbRow.removeFromLeft (10);
        placeKnob (mbRow, 76, scopePersistK, scopePersistL);
    }
}
