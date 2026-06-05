#include "PluginEditor.h"

using namespace anamorph::gui;

#ifndef ANAMORPH_VERSION_STRING
 #define ANAMORPH_VERSION_STRING "0.2.0"
#endif
#ifndef ANAMORPH_BUILD_NUMBER
 #define ANAMORPH_BUILD_NUMBER "0"
#endif

// ============================================================================
void AnamorphAudioProcessorEditor::Backdrop::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xcc06080b)); // dim the rest of the UI

    g.setColour (colours::bgPanel);
    g.fillRoundedRectangle (panel.toFloat(), 10.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (panel.toFloat().reduced (0.5f), 10.0f, 1.0f);

    if (aboutText)
    {
        auto r = panel.reduced (26, 22);
        g.setColour (colours::text);
        g.setFont (juce::Font (juce::FontOptions (26.0f)).withExtraKerningFactor (0.16f));
        g.drawText ("ANAMORPH", r.removeFromTop (34), juce::Justification::centredLeft);

        g.setColour (colours::accent);
        g.setFont (juce::Font (juce::FontOptions (12.0f)).withExtraKerningFactor (0.22f));
        g.drawText ("STEREO TOOLS", r.removeFromTop (20), juce::Justification::centredLeft);

        r.removeFromTop (10);
        g.setColour (colours::textDim);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        const juce::String body =
            juce::String ("Version ") + ANAMORPH_VERSION_STRING + "  (build " + ANAMORPH_BUILD_NUMBER + ")\n"
            "by Rolly Tech\n\n"
            "A stereo-field toolkit: turn mono into stereo, shape width,\n"
            "and keep mono compatibility -- around a precision vectorscope.\n\n"
            "Click anywhere to close.";
        g.drawMultiLineText (body, r.getX(), r.getY() + 16, r.getWidth(), juce::Justification::left);
    }
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
    addAndMakeVisible (*scope);
    addAndMakeVisible (*balanceMeter);
    addAndMakeVisible (*corrMeter);

    // --- Top bar ---
    titleButton.setComponentID ("ghost");
    titleButton.onClick = [this] { showAbout (true); };
    addAndMakeVisible (titleButton);

    abButton.setTooltip ("A / B compare: click to switch slot. Copy stores the current settings into the other slot.");
    abButton.onClick = [this] { switchTo (activeSlot == 0 ? 1 : 0); };
    addAndMakeVisible (abButton);
    copyButton.onClick = [this] { copyCurrentToOther(); };
    copyButton.setTooltip ("Copy the current settings into the other A/B slot.");
    addAndMakeVisible (copyButton);

    settingsButton.setTooltip ("Oversampling, zero-latency live mode and tooltips.");
    settingsButton.onClick = [this] { showSettings (true); };
    addAndMakeVisible (settingsButton);

    undoButton.setTooltip ("Undo");
    redoButton.setTooltip ("Redo");
    undoButton.onClick = [this] { processor.getUndoManager().undo(); };
    redoButton.onClick = [this] { processor.getUndoManager().redo(); };
    addAndMakeVisible (undoButton);
    addAndMakeVisible (redoButton);

    setupToggle (advancedToggle, pid::advancedMode, "Advanced",
                 "Reveal advanced controls: the Input module and multiband width.");
    advanced = advancedToggle.getToggleState();
    advancedToggle.onClick = [this]
    {
        advanced = advancedToggle.getToggleState();
        setSize (kWidth, advanced ? kHeightAdvanced : kHeightSimple);
        updateModeVisibility();
    };
    setupToggle (bypassToggle, pid::bypass, "Bypass",
                 "Bypass the whole plug-in (the UI dims; controls stay live).");

    // --- WIDEN module ---
    setupCombo (algorithmBox, pid::algorithm, "Widening method used to create stereo from mono.");
    algorithmBox.onChange = [this] { updateAlgoControls(); resized(); };
    algorithmLabel.setText ("WIDEN", juce::dontSendNotification);
    algorithmLabel.setJustificationType (juce::Justification::centredLeft);
    algorithmLabel.setColour (juce::Label::textColourId, colours::textDim);
    algorithmLabel.setFont (juce::Font (juce::FontOptions (11.0f)).withExtraKerningFactor (0.2f));
    addAndMakeVisible (algorithmLabel);

    // Per-algorithm mode/side selector sits right with the algorithm (#24).
    setupCombo (haasSideBox, pid::haasSide, "Which side the Haas delay is applied to.");
    setupCombo (dimModeBox,  pid::dimMode,  "Dimension-D voicing: Subtle / Classic / Wide / Lush.");

    setupRotary (driveK,  driveL,  "Drive",  "Pre-saturation drive. 0 dB is clean.");
    setupRotary (amountK, amountL, "Amount", "Widening intensity. 0% is fully transparent.");
    setupRotary (widthK,  widthL,  "Width",  "Stereo width. 100% leaves the image unchanged.");
    attachSlider (driveK, pid::drive);
    attachSlider (amountK, pid::amount);
    attachSlider (widthK, pid::width);

    setupRotary (haasDelayK,   haasDelayL,   "Delay",   "Haas inter-channel delay (1-35 ms).");
    setupRotary (velvetK,      velvetL,      "Density", "Velvet-noise diffusion density.");
    setupRotary (chorusRateK,  chorusRateL,  "Rate",    "Chorus modulation rate.");
    setupRotary (chorusDepthK, chorusDepthL, "Depth",   "Chorus modulation depth.");
    attachSlider (haasDelayK,   pid::haasDelay);
    attachSlider (velvetK,      pid::velvetDensity);
    attachSlider (chorusRateK,  pid::chorusRate);
    attachSlider (chorusDepthK, pid::chorusDepth);

    // --- OUTPUT module ---
    setupRotary (mixK,        mixL,        "Mix",     "Dry/Wet blend (parallel processing).");
    setupRotary (outputK,     outputL,     "Output",  "Output gain trim.");
    setupRotary (outBalanceK, outBalanceL, "Balance", "Output L/R balance.");
    attachSlider (mixK,        pid::mix);
    attachSlider (outputK,     pid::outputGain);
    attachSlider (outBalanceK, pid::outputBalance);

    setupToggle (autoMatchToggle, pid::autoGainMatch, "Match",
                 "Loudness-match wet to dry (perceptual, ITU-R BS.1770) for fair A/B.");
    applyGainButton.setTooltip ("Lock the measured match gain into Output as a fixed value.");
    applyGainButton.onClick = [this] { processor.applyAutoGain(); };
    addAndMakeVisible (applyGainButton);
    matchReadout.setJustificationType (juce::Justification::centred);
    matchReadout.setColour (juce::Label::textColourId, colours::textDim);
    matchReadout.setFont (juce::Font (juce::FontOptions (11.0f)));
    addAndMakeVisible (matchReadout);

    // --- MONO MAKER (slim bar) ---
    setupToggle (monoMakerToggle, pid::monoMakerOn, "Mono Maker",
                 "Sum everything below the frequency to mono (placed after widening).");
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

    setupCombo (channelModeBox, pid::channelMode, "Input channel: stereo, or solo one side.");
    channelModeLabel.setText ("Input Channel", juce::dontSendNotification);
    channelModeLabel.setJustificationType (juce::Justification::centredLeft);
    channelModeLabel.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (channelModeLabel);

    setupCombo (soloBox, pid::solo, "Solo the Mid or the Side to check your MS balance.");
    soloLabel.setText ("M/S Solo", juce::dontSendNotification);
    soloLabel.setJustificationType (juce::Justification::centredLeft);
    soloLabel.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (soloLabel);

    setupToggle (monoToggle, pid::monoSum, "Mono",       "Sum the input to mono.");
    setupToggle (swapToggle, pid::swap,    "Swap L/R",   "Swap the left and right channels.");
    setupToggle (msToggle,   pid::msMode,  "M/S Mode",   "Run the effect chain in Mid/Side.");
    const juce::String phi = juce::String::charToString ((juce::juce_wchar) 0x00F8); // 'oe slash' as a phase-ish glyph
    setupToggle (polLToggle, pid::polarityL, phi + " L", "Invert the polarity (phase) of the left channel.");
    setupToggle (polRToggle, pid::polarityR, phi + " R", "Invert the polarity (phase) of the right channel.");

    setupRotary (balanceK, balanceL, "Input Balance", "Balance the input between L and R.");
    attachSlider (balanceK, pid::inputBalance);

    // --- MULTIBAND (advanced) ---
    setupToggle (mbEnableToggle, pid::mbEnable, "Multiband", "Independent width per low/mid/high band.");
    setupRotary (mbFreqLowK,  mbFreqLowL,  "Lo/Mid", "Low/Mid crossover.");
    setupRotary (mbFreqHighK, mbFreqHighL, "Mid/Hi", "Mid/High crossover.");
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

    // --- Bypass dim layer (painted on top, mouse passes through) ---
    dimOverlay.setInterceptsMouseClicks (false, false);
    addChildComponent (dimOverlay);

    // --- About / Settings overlays (top of z-order) ---
    aboutBackdrop.aboutText = true;
    aboutBackdrop.onDismiss = [this] { showAbout (false); };
    addChildComponent (aboutBackdrop);

    settingsBackdrop.onDismiss = [this] { showSettings (false); };
    addChildComponent (settingsBackdrop);

    settingsTitle.setText ("SETTINGS", juce::dontSendNotification);
    settingsTitle.setColour (juce::Label::textColourId, colours::textDim);
    settingsTitle.setFont (juce::Font (juce::FontOptions (12.0f)).withExtraKerningFactor (0.2f));
    settingsBackdrop.addAndMakeVisible (settingsTitle);

    setupCombo (oversampleBox, pid::oversample, "Oversampling for the nonlinear stages. Off = 1x (no latency).");
    oversampleLabel.setText ("Oversampling", juce::dontSendNotification);
    oversampleLabel.setColour (juce::Label::textColourId, colours::textDim);
    settingsBackdrop.addAndMakeVisible (oversampleLabel);
    settingsBackdrop.addAndMakeVisible (oversampleBox); // re-parent into the panel

    setupToggle (zeroLatencyToggle, pid::zeroLatency, "Zero Latency (Live)",
                 "Force zero latency for live/tracking (disables oversampling latency).");
    settingsBackdrop.addAndMakeVisible (zeroLatencyToggle);

    tooltipsToggle.setButtonText ("Tooltips");
    tooltipsToggle.setToggleState (true, juce::dontSendNotification);
    tooltipsToggle.onClick = [this] { tooltipsOn = tooltipsToggle.getToggleState(); applyTooltipsEnabled(); };
    settingsBackdrop.addAndMakeVisible (tooltipsToggle);

    // Seed A/B from the current state.
    stateA = processor.getAPVTS().copyState();
    stateB = stateA.createCopy();
    abButton.setButtonText ("A");

    applyTooltipsEnabled();
    updateAlgoControls();
    updateModeVisibility();
    setSize (kWidth, advanced ? kHeightAdvanced : kHeightSimple);
    startTimerHz (8);
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
    t.setTooltip (tip);
    addAndMakeVisible (t);
    buttonAtts.add (new ButtonAttachment (processor.getAPVTS(), id, t));
}

void AnamorphAudioProcessorEditor::applyTooltipsEnabled()
{
    // Disable globally by pushing the appear-delay out of reach (#20).
    tooltips.setMillisecondsBeforeTipAppears (tooltipsOn ? 600 : 0x3fffffff);
}

// ----------------------------------------------------------------------------
void AnamorphAudioProcessorEditor::updateAlgoControls()
{
    const int algo = algorithmBox.getSelectedItemIndex(); // 0 Haas,1 Velvet,2 Chorus,3 Dim-D

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
        &mbEnableToggle, &mbFreqLowK, &mbFreqLowL, &mbFreqHighK, &mbFreqHighL,
        &mbWLowK, &mbWLowL, &mbWMidK, &mbWMidL, &mbWHighK, &mbWHighL,
        &scopePersistK, &scopePersistL
    };
    for (auto* c : adv) c->setVisible (advanced);

    updateAlgoControls();
    resized();
}

// ----------------------------------------------------------------------------
void AnamorphAudioProcessorEditor::showAbout (bool show)
{
    aboutBackdrop.setVisible (show);
    if (show) { aboutBackdrop.toFront (false); resized(); }
}

void AnamorphAudioProcessorEditor::showSettings (bool show)
{
    settingsBackdrop.setVisible (show);
    if (show) { settingsBackdrop.toFront (false); resized(); }
}

// ----------------------------------------------------------------------------
void AnamorphAudioProcessorEditor::timerCallback()
{
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

    if (dimOverlay.isVisible() != bypassToggle.getToggleState())
    {
        dimOverlay.setVisible (bypassToggle.getToggleState());
        if (dimOverlay.isVisible()) dimOverlay.toFront (false);
    }

    undoButton.setEnabled (processor.getUndoManager().canUndo());
    redoButton.setEnabled (processor.getUndoManager().canRedo());

    const float mg = processor.getEngine().getMatchGainDb();
    matchReadout.setText (juce::String (mg, 1) + " dB", juce::dontSendNotification);
}

// ----------------------------------------------------------------------------
//  A/B compare (FabFilter-style)
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
    abButton.setButtonText (slot == 1 ? "B" : "A");
    repaint();
}

void AnamorphAudioProcessorEditor::copyCurrentToOther()
{
    captureTo (activeSlot);
    const int other = activeSlot == 0 ? 1 : 0;
    (other == 1 ? stateB : stateA) = processor.getAPVTS().copyState().createCopy();
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

    // Right control column backing
    auto panel = juce::Rectangle<int> (getWidth() - 300, 46, 300, getHeight() - 46).toFloat().reduced (8.0f);
    g.setColour (colours::bgPanel.withAlpha (0.55f));
    g.fillRoundedRectangle (panel, 8.0f);

    if (advanced)
    {
        auto adv = juce::Rectangle<int> (0, getHeight() - 210, getWidth(), 210).toFloat().reduced (8.0f, 6.0f);
        g.setColour (colours::bgPanel.withAlpha (0.5f));
        g.fillRoundedRectangle (adv, 8.0f);
    }
}

// ----------------------------------------------------------------------------
void AnamorphAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();

    // Full-window overlays
    dimOverlay.setBounds (getLocalBounds());
    aboutBackdrop.setBounds (getLocalBounds());
    settingsBackdrop.setBounds (getLocalBounds());
    {
        auto a = getLocalBounds().withSizeKeepingCentre (380, 230);
        aboutBackdrop.panel = a;
        auto s = getLocalBounds().withSizeKeepingCentre (360, 220);
        settingsBackdrop.panel = s;
        auto inner = s.reduced (22, 18);
        settingsTitle.setBounds (inner.removeFromTop (20));
        inner.removeFromTop (8);
        auto row1 = inner.removeFromTop (40);
        oversampleLabel.setBounds (row1.removeFromTop (16));
        oversampleBox.setBounds (row1.reduced (0, 1));
        inner.removeFromTop (10);
        zeroLatencyToggle.setBounds (inner.removeFromTop (28));
        inner.removeFromTop (6);
        tooltipsToggle.setBounds (inner.removeFromTop (28));
    }

    auto top = r.removeFromTop (46);
    {
        auto bar = top.reduced (8, 9);
        titleButton.setBounds (juce::Rectangle<int> (10, 0, 300, 46)); // over the wordmark
        bypassToggle.setBounds (bar.removeFromRight (84));
        advancedToggle.setBounds (bar.removeFromRight (98));
        settingsButton.setBounds (bar.removeFromRight (84));
        bar.removeFromRight (12);
        redoButton.setBounds (bar.removeFromRight (30));
        undoButton.setBounds (bar.removeFromRight (30));
        bar.removeFromRight (12);
        copyButton.setBounds (bar.removeFromRight (54));
        abButton.setBounds (bar.removeFromRight (38));
    }

    auto rightPanel = r.removeFromRight (300);
    juce::Rectangle<int> advArea;
    if (advanced) advArea = r.removeFromBottom (210);

    // ---- Scope + meters ----
    {
        auto sa = r.reduced (16);
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

        // Width + the contextual character control(s).
        {
            auto row = col.removeFromTop (88);
            placeKnob (row.removeFromLeft (row.getWidth() / 2), widthK, widthL);
            auto charArea = row;
            // Single-knob algorithms fill the slot; Chorus splits it into Rate+Depth.
            placeKnob (charArea, haasDelayK, haasDelayL);
            placeKnob (charArea, velvetK, velvetL);
            placeKnob (charArea.withTrimmedRight (charArea.getWidth() / 2), chorusRateK, chorusRateL);
            placeKnob (charArea.withTrimmedLeft  (charArea.getWidth() / 2), chorusDepthK, chorusDepthL);
        }

        twoKnob (mixK, mixL, outputK, outputL);

        // Output Balance + Auto-Gain cluster
        {
            auto row = col.removeFromTop (88);
            placeKnob (row.removeFromLeft (row.getWidth() / 2), outBalanceK, outBalanceL);
            autoMatchToggle.setBounds (row.removeFromTop (26).reduced (2));
            auto ar = row.removeFromTop (28);
            applyGainButton.setBounds (ar.removeFromLeft (64).reduced (3));
            matchReadout.setBounds (ar.reduced (2));
        }

        // MONO MAKER slim bar (#13)
        col.removeFromTop (4);
        auto mm = col.removeFromTop (30);
        monoMakerToggle.setBounds (mm.removeFromLeft (108).reduced (2, 4));
        monoFreqK.setBounds (mm.reduced (2, 4));
    }

    // ---- Advanced strip: INPUT module + multiband ----
    if (advanced)
    {
        auto a = advArea.reduced (14, 10);
        inputModuleLabel.setBounds (a.removeFromTop (16));

        auto row1 = a.removeFromTop (40);
        auto cell = [&] (juce::Rectangle<int>& src, int w, juce::Label& lab, juce::ComboBox& box)
        {
            auto c = src.removeFromLeft (w);
            lab.setBounds (c.removeFromTop (15));
            box.setBounds (c.reduced (1, 1));
        };
        cell (row1, 120, channelModeLabel, channelModeBox);
        cell (row1, 90,  soloLabel, soloBox);
        row1.removeFromLeft (10);
        monoToggle.setBounds (row1.removeFromLeft (78).reduced (2, 8));
        swapToggle.setBounds (row1.removeFromLeft (96).reduced (2, 8));
        msToggle.setBounds   (row1.removeFromLeft (96).reduced (2, 8));
        polLToggle.setBounds (row1.removeFromLeft (62).reduced (2, 8));
        polRToggle.setBounds (row1.removeFromLeft (62).reduced (2, 8));

        a.removeFromTop (6);
        auto row2 = a;
        auto placeKnob = [] (juce::Rectangle<int>& src, int w, juce::Slider& s, juce::Label& l)
        { auto c = src.removeFromLeft (w); l.setBounds (c.removeFromBottom (16)); s.setBounds (c.reduced (4, 0)); };

        placeKnob (row2, 96, balanceK, balanceL);
        row2.removeFromLeft (12);
        mbEnableToggle.setBounds (row2.removeFromLeft (110).withHeight (26).withY (row2.getCentreY() - 13).reduced (2, 0));
        placeKnob (row2, 78, mbFreqLowK, mbFreqLowL);
        placeKnob (row2, 78, mbFreqHighK, mbFreqHighL);
        placeKnob (row2, 70, mbWLowK, mbWLowL);
        placeKnob (row2, 70, mbWMidK, mbWMidL);
        placeKnob (row2, 70, mbWHighK, mbWHighL);
        placeKnob (row2, 78, scopePersistK, scopePersistL);
    }
}
