#include "PluginEditor.h"
#include <cmath>

using namespace anamorph::gui;

#ifndef ANAMORPH_VERSION_STRING
 #define ANAMORPH_VERSION_STRING "0.4.0"
#endif
#ifndef ANAMORPH_BUILD_NUMBER
 #define ANAMORPH_BUILD_NUMBER "0"
#endif

// ============================================================================
void AnamorphAudioProcessorEditor::Backdrop::paint (juce::Graphics& g)
{
    // While revealing (Persist drag), only partially dim and partially fade the
    // panel so the live vectorscope shows through -- the bar/controls on top stay
    // fully opaque (#26).
    const float dimA   = juce::jmap (reveal, 0.0f, 1.0f, 0.80f, 0.34f);
    const float panelA = juce::jmap (reveal, 0.0f, 1.0f, 1.0f, 0.30f);
    g.fillAll (juce::Colour (0x06080b).withAlpha (dimA));

    if (panelA >= 0.999f) // solid (normal) state: full iOS-glass panel (#17)
    {
        glass::fillPanel (g, panel.toFloat(), 12.0f, colours::bgPanel);
    }
    else // mid-reveal (Persist drag): fade the panel so the live scope shows through
    {
        g.setColour (colours::bgPanel.withAlpha (panelA));
        g.fillRoundedRectangle (panel.toFloat(), 12.0f);
        g.setColour (colours::outline.withAlpha (juce::jmap (reveal, 0.0f, 1.0f, 1.0f, 0.5f)));
        g.drawRoundedRectangle (panel.toFloat().reduced (0.5f), 12.0f, 1.0f);
    }

    if (aboutText)
    {
        const juce::String copyright = juce::String::charToString ((juce::juce_wchar) 0x00A9); // © (no mojibake, #3)
        const juce::String emdash    = juce::String::charToString ((juce::juce_wchar) 0x2014); // —

        auto r = panel.reduced (30, 26);
        g.setColour (colours::text);
        g.setFont (juce::Font (juce::FontOptions (28.0f)).withExtraKerningFactor (0.16f));
        g.drawText ("ANAMORPH", r.removeFromTop (38), juce::Justification::topLeft);

        g.setColour (colours::accent);
        g.setFont (juce::Font (juce::FontOptions (12.0f)).withExtraKerningFactor (0.22f));
        g.drawText ("STEREO TOOLS", r.removeFromTop (20), juce::Justification::topLeft);

        r.removeFromTop (14);
        g.setColour (colours::textDim);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawText (juce::String ("Version ") + ANAMORPH_VERSION_STRING + "   build " + ANAMORPH_BUILD_NUMBER,
                    r.removeFromTop (18), juce::Justification::topLeft);
        g.drawText ("RollyTech", r.removeFromTop (18), juce::Justification::topLeft); // RollyTech (#5)

        r.removeFromTop (10);
        // One flowing sentence that word-wraps naturally (#3): no hard break
        // before "keep mono compatibility".
        const juce::String desc =
            "A stereo-field toolkit: turn mono into stereo, shape width, and keep "
            "mono compatibility " + emdash + " all around a precision vectorscope.";
        g.drawFittedText (desc, r.removeFromTop (60), juce::Justification::topLeft, 4);

        // Copyright line at the bottom of the panel (#3).
        g.setColour (colours::textDim.withAlpha (0.7f));
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (copyright + " 2026 RollyTech. All rights reserved.",
                    panel.reduced (30, 18).removeFromBottom (16), juce::Justification::bottomLeft);
    }
}

void AnamorphAudioProcessorEditor::ABControl::paint (juce::Graphics& g)
{
    // Racetrack / stadium frame: micro-gradient fill + soft accent edge glow (#6).
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const float rad = b.getHeight() * 0.5f;

    g.setColour (colours::accent.withAlpha (0.10f));
    g.fillRoundedRectangle (b.expanded (1.6f), rad + 1.6f);

    const float lift = hovered ? 0.16f : 0.06f; // hover wash (#10)
    juce::ColourGradient gr (colours::bgRaised.brighter (lift), b.getX(), b.getY(),
                             colours::bgRaised.darker (0.12f),   b.getX(), b.getBottom(), false);
    g.setGradientFill (gr);
    g.fillRoundedRectangle (b, rad);
    g.setColour (hovered ? colours::accent.withAlpha (0.6f) : colours::outline.brighter (0.12f));
    g.drawRoundedRectangle (b, rad, 1.0f);

    const int active = getActive ? getActive() : 0;
    auto inner = getLocalBounds();
    g.setFont (juce::Font (juce::FontOptions (14.0f)).withExtraKerningFactor (0.04f));

    auto aRect = inner.removeFromLeft (juce::roundToInt (inner.getWidth() * 0.40f));
    auto bRect = inner.removeFromRight (juce::roundToInt (getWidth() * 0.40f));

    g.setColour (colours::textDim.withAlpha (0.7f));
    g.drawText ("/", inner, juce::Justification::centred);
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
    abControl.setTooltip ("A/B Compare"); // #17 (no period)
    addAndMakeVisible (abControl);
    copyButton.onClick = [this] { processor.abCopyToOther(); };
    copyButton.setTooltip ("Copy the current settings into the other A/B slot.");
    addAndMakeVisible (copyButton);

    settingsButton.onClick = [this] { showSettings (true); };
    addAndMakeVisible (settingsButton);

    undoButton.setButtonText (juce::String::charToString ((juce::juce_wchar) 0x21BA));
    redoButton.setButtonText (juce::String::charToString ((juce::juce_wchar) 0x21BB));
    undoButton.setComponentID ("icon"); // bigger, rotated glyph (#7)
    redoButton.setComponentID ("icon");
    undoButton.setTooltip ("Undo");
    redoButton.setTooltip ("Redo");
    undoButton.onClick = [this] { processor.undo(); };
    redoButton.onClick = [this] { processor.redo(); };
    addAndMakeVisible (undoButton);
    addAndMakeVisible (redoButton);

    setupToggle (metersToggle, pid::metersOn, "", "Show level meters."); // #32
    metersToggle.setComponentID ("metersicon"); // level-meter glyph, not the word (#7)
    metersToggle.onClick = [this] { metersOn = metersToggle.getToggleState(); if (metersOn) levelMeter->setVisible (true); };

    setupToggle (advancedToggle, pid::advancedMode, "Adv", "Advanced mode"); // #17
    advancedToggle.onClick = [this] { advanced = advancedToggle.getToggleState(); updateModeVisibility(); };

    setupToggle (bypassToggle, pid::bypass, "Bypass", {});
    bypassToggle.setComponentID ("bypass");

    // --- WIDEN module (the Simple-mode core) ---
    setupCombo (algorithmBox, pid::algorithm, "The stereo-widening algorithm."); // #4
    algorithmBox.onChange = [this] { updateAlgoControls(); resized(); };
    algorithmLabel.setText ("WIDEN", juce::dontSendNotification);
    algorithmLabel.setJustificationType (juce::Justification::centredLeft);
    algorithmLabel.setColour (juce::Label::textColourId, colours::textDim);
    algorithmLabel.setFont (juce::Font (juce::FontOptions (11.0f)).withExtraKerningFactor (0.2f));
    addAndMakeVisible (algorithmLabel);

    algoOptLabel.setJustificationType (juce::Justification::centredLeft); // left-aligned with the combo (#13)
    algoOptLabel.setColour (juce::Label::textColourId, colours::textDim);
    algoOptLabel.setFont (juce::Font (juce::FontOptions (10.0f)).withExtraKerningFactor (0.18f));
    addAndMakeVisible (algoOptLabel);

    setupCombo (haasSideBox, pid::haasSide, "Which side the sound leans toward.");
    setupCombo (dimModeBox,  pid::dimMode,  "Voicing of the Dim-D widener."); // #5

    setupRotary (driveK,  driveL,  "Drive",  "Adds gentle saturation / density. 0 dB is clean.");
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

    // --- OUTPUT module (Advanced, #24) ---
    outputModuleLabel.setText ("OUTPUT", juce::dontSendNotification);
    outputModuleLabel.setJustificationType (juce::Justification::centredLeft);
    outputModuleLabel.setColour (juce::Label::textColourId, colours::textDim);
    outputModuleLabel.setFont (juce::Font (juce::FontOptions (11.0f)).withExtraKerningFactor (0.2f));
    addAndMakeVisible (outputModuleLabel);

    setupRotary (mixK,        mixL,        "Mix",     "Dry / wet blend.");
    setupRotary (outputK,     outputL,     "Output",  "Output level.");
    setupRotary (outBalanceK, outBalanceL, "Balance", "Output left / right balance.");
    attachSlider (mixK,        pid::mix);
    attachSlider (outputK,     pid::outputGain);
    attachSlider (outBalanceK, pid::outputBalance);

    setupToggle (autoMatchToggle, pid::autoGainMatch, "Level Match",
                 "Match the processed loudness to the input so louder doesn't fool you.");
    applyGainButton.setComponentID ("apply"); // bigger Apply text (#23)
    applyGainButton.setTooltip ("Bake the measured match into Output as a fixed value.");
    applyGainButton.onClick = [this] { processor.applyAutoGain(); };
    addAndMakeVisible (applyGainButton);
    matchReadout.setJustificationType (juce::Justification::centredRight); // align with the Hz readout below (#11)
    matchReadout.setColour (juce::Label::textColourId, colours::textDim);
    matchReadout.setFont (juce::Font (juce::FontOptions (11.0f)));
    addAndMakeVisible (matchReadout);

    // Mono Maker now lives inside the Output module (#24).
    setupToggle (monoMakerToggle, pid::monoMakerOn, "Mono Maker",
                 "Collapse everything below the frequency to mono (before widening).");
    setupRotary (monoFreqK, monoFreqL, "Freq", "Mono Maker crossover frequency.");
    monoFreqK.setSliderStyle (juce::Slider::LinearHorizontal);
    monoFreqK.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
    // Right-justify the Hz readout so its right edge lines up with the Level Match
    // dB readout above it (#11).
    for (auto* c : monoFreqK.getChildren())
        if (auto* lab = dynamic_cast<juce::Label*> (c)) lab->setJustificationType (juce::Justification::centredRight);
    attachSlider (monoFreqK, pid::monoMakerFreq);

    // --- INPUT module (advanced) ---
    inputModuleLabel.setText ("INPUT", juce::dontSendNotification);
    inputModuleLabel.setJustificationType (juce::Justification::centredLeft);
    inputModuleLabel.setColour (juce::Label::textColourId, colours::textDim);
    inputModuleLabel.setFont (juce::Font (juce::FontOptions (11.0f)).withExtraKerningFactor (0.2f));
    addAndMakeVisible (inputModuleLabel);

    setupCombo (channelModeBox, pid::channelMode, "Use the full stereo input, or just one side.");
    channelModeBox.setLookAndFeel (&compactCombo); // smaller list (#12)
    passComboHoverThrough (channelModeBox); // setLookAndFeel rebuilt the label -> re-let hover through (recurring)
    channelModeLabel.setText ("Input Channel", juce::dontSendNotification);
    channelModeLabel.setColour (juce::Label::textColourId, colours::textDim);
    channelModeLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    addAndMakeVisible (channelModeLabel);

    setupCombo (soloBox, pid::solo, "Listen to just the Mid or just the Side of the input.");
    soloBox.setLookAndFeel (&compactCombo); // smaller list (#12)
    passComboHoverThrough (soloBox);
    soloLabel.setText ("M/S Solo", juce::dontSendNotification);
    soloLabel.setColour (juce::Label::textColourId, colours::textDim);
    soloLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    addAndMakeVisible (soloLabel);

    // Compact vertical toggles (pill on top, label centred below) so the Input
    // row labels always fit and never truncate (#11 / #14).
    const juce::String ph = juce::String::charToString ((juce::juce_wchar) 0x00F8);
    setupToggle (monoToggle, pid::monoSum, "Mono", "Sum to mono (after M/S decode)."); // #14
    setupToggle (swapToggle, pid::swap,    "Swap", "Swap the Left / Right channels, or Mid / Side when M/S is on."); // #3
    setupToggle (msToggle,   pid::msMode,  "M/S",  "M/S decoder: treat the input as Mid / Side and decode to Left / Right."); // #4
    msToggle.onClick = [this] { updateMsLabels(); };
    setupToggle (polLToggle, pid::polarityL, ph + " L", "Flip the polarity of the Left channel.");   // M/S-aware (#13)
    setupToggle (polRToggle, pid::polarityR, ph + " R", "Flip the polarity of the Right channel.");
    for (auto* t : { &monoToggle, &swapToggle, &msToggle, &polLToggle, &polRToggle })
        t->setComponentID ("vtoggle");

    setupRotary (balanceK, balanceL, "Balance", "Input balance: Left / Right (Mid / Side in M/S mode)."); // #12
    attachSlider (balanceK, pid::inputBalance);
    // M/S-aware value readout: shows L/R normally, M/S when the decoder is on (#12).
    balanceK.textFromValueFunction = [this] (double v) -> juce::String
    {
        const bool ms = msToggle.getToggleState();
        const float val = (float) v;
        if (std::abs (val) < 0.0005f) return "C";
        if (val < 0.0f) return juce::String (ms ? "M -" : "L -") + juce::String (-val * 100.0f, 1) + "%";
        return juce::String (ms ? "S " : "R ") + juce::String (val * 100.0f, 1) + "%";
    };
    balanceK.updateText();

    // --- MULTIBAND module (advanced) ---
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

    // --- Overlays ---
    dimOverlay.setInterceptsMouseClicks (false, false);
    addChildComponent (dimOverlay);

    aboutBackdrop.aboutText = true;
    aboutBackdrop.onDismiss = [this] { showAbout (false); };
    addChildComponent (aboutBackdrop);
    aboutLink.setColour (juce::HyperlinkButton::textColourId, colours::accent);
    aboutLink.setFont (juce::Font (juce::FontOptions (13.0f)), false, juce::Justification::centredLeft);
    aboutLink.setJustificationType (juce::Justification::centredLeft);
    aboutLink.setTooltip (juce::String()); // no tooltip on the URL (#2)
    aboutBackdrop.addAndMakeVisible (aboutLink); // clickable www.rolly.tech

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

    // Scope Persistence is now a Settings bar (#21).
    persistLabel.setText ("Vectorscope Persist", juce::dontSendNotification);
    persistLabel.setColour (juce::Label::textColourId, colours::textDim);
    settingsBackdrop.addAndMakeVisible (persistLabel);
    scopePersistK.setSliderStyle (juce::Slider::LinearHorizontal);
    scopePersistK.setDoubleClickReturnValue (true, 0.5); // double-click resets to default (#7)
    scopePersistK.setColour (juce::Slider::textBoxTextColourId, colours::textDim);
    scopePersistK.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    scopePersistK.setTooltip ("Vectorscope afterglow time " // #5
                              + juce::String::charToString ((juce::juce_wchar) 0x2014)
                              + " longer trails fade more slowly.");
    scopePersistK.setRepaintsOnMouseActivity (true); // hover glow (#10)
    settingsBackdrop.addAndMakeVisible (scopePersistK);
    scopePersistK.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18); // box built with our LnF
    attachSlider (scopePersistK, pid::scopePersist);
    scopePersistK.onValueChange = [this] { applyScopePersist(); };
    // While dragging Persist, fade the Settings overlay so the live vectorscope
    // behind it is visible (#9).
    scopePersistK.onDragStart = [this] { persistDragging = true; };
    scopePersistK.onDragEnd   = [this] { persistDragging = false; };

    tooltipsToggle.setButtonText ("Tooltips");
    tooltipsToggle.setTooltip ("Show these hover hints on every control.");
    settingsBackdrop.addAndMakeVisible (tooltipsToggle);
    buttonAtts.add (new ButtonAttachment (processor.getAPVTS(), pid::tooltipsOn, tooltipsToggle));

    // Initial cached view-state from the (recalled) parameters.
    advanced   = advancedToggle.getToggleState();
    metersOn   = metersToggle.getToggleState();
    tooltipsOn = tooltipsToggle.getToggleState();
    meterAnim  = metersOn ? 1.0f : 0.0f;
    levelMeter->setVisible (metersOn);

    applyTooltipsEnabled();
    applyScopePersist();
    updateAlgoControls();
    updateMsLabels();
    updateModeVisibility();
    setSize (kWidth, kHeight);    // single fixed size for both modes (#20)
    setResizable (false, false);
    startTimerHz (24);
}

AnamorphAudioProcessorEditor::~AnamorphAudioProcessorEditor()
{
    stopTimer();
    openGLContext.detach();
    channelModeBox.setLookAndFeel (nullptr);
    soloBox.setLookAndFeel (nullptr);
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

// ----------------------------------------------------------------------------
void AnamorphAudioProcessorEditor::setupRotary (juce::Slider& s, juce::Label& l,
                                                const juce::String& name, const juce::String& tip)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setColour (juce::Slider::textBoxTextColourId, colours::text);
    s.setColour (juce::Slider::textBoxHighlightColourId, colours::accent.withAlpha (0.30f));
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setTooltip (tip);
    s.setRepaintsOnMouseActivity (true); // hover glow (#10)
    addAndMakeVisible (s);
    // Create the value box AFTER parenting, so it's built with our LookAndFeel
    // (the draggable/raw-editing ValueBox) -- reparenting doesn't recreate it,
    // which is why drag/edit "didn't take" (feedback #28/#29).
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 15);

    l.setText (name, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setColour (juce::Label::textColourId, colours::textDim);
    l.setFont (juce::Font (juce::FontOptions (11.5f)));
    addAndMakeVisible (l);
}

void AnamorphAudioProcessorEditor::attachSlider (juce::Slider& s, const char* id)
{
    sliderAtts.add (new SliderAttachment (processor.getAPVTS(), id, s));

    auto* p = processor.getAPVTS().getParameter (id);
    if (auto* k = dynamic_cast<Knob*> (&s); k != nullptr && p != nullptr)
        k->resetValue = p->getNormalisableRange().convertFrom0to1 (p->getDefaultValue()); // #6

    // Tag the unit so the value box can show a bare number while editing (#36).
    const juce::String sid (id);
    juce::String unit;
    if      (sid == pid::drive || sid == pid::outputGain) unit = "db";
    else if (sid == pid::haasDelay) unit = "ms";
    else if (sid == pid::amount || sid == pid::velvetDensity || sid == pid::chorusDepth
          || sid == pid::width || sid == pid::mix || sid == pid::scopePersist
          || sid == pid::mbWidthLow || sid == pid::mbWidthMid || sid == pid::mbWidthHigh) unit = "pct";
    else if (sid == pid::monoMakerFreq || sid == pid::mbFreqLow || sid == pid::mbFreqHigh) unit = "hz";
    else if (sid == pid::inputBalance || sid == pid::outputBalance) unit = "bal";
    if (unit.isNotEmpty()) s.getProperties().set ("unit", unit);
}

// The combo's internal text label otherwise eats hover events over most of the
// box, so brightening only showed on the border/arrow. Let events fall through to
// the ComboBox so the whole control lights up. Must be re-applied after any
// setLookAndFeel(), which rebuilds the label fresh (recurring hover request).
void AnamorphAudioProcessorEditor::passComboHoverThrough (juce::ComboBox& box)
{
    for (auto* child : box.getChildren())
        if (dynamic_cast<juce::Label*> (child) != nullptr)
            child->setInterceptsMouseClicks (false, false);
}

void AnamorphAudioProcessorEditor::setupCombo (juce::ComboBox& box, const char* id, const juce::String& tip)
{
    if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (processor.getAPVTS().getParameter (id)))
        box.addItemList (cp->choices, 1);
    box.setTooltip (tip);
    box.setRepaintsOnMouseActivity (true); // hover feedback (#10)
    passComboHoverThrough (box);
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

void AnamorphAudioProcessorEditor::applyScopePersist()
{
    // Remap so the new default 50% reproduces the old 60% afterglow (#21):
    // pow(0.5, 0.737) ~= 0.6, endpoints preserved.
    if (scope != nullptr)
        scope->setPersistence (std::pow ((float) scopePersistK.getValue(), 0.737f));
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

    // Caption over the side/voicing combo (#9): one intuitive word per algorithm.
    algoOptLabel.setVisible (algo == 0 || algo == 3);
    algoOptLabel.setText (algo == 0 ? "FOCUS" : algo == 3 ? "STYLE" : juce::String(), // #13
                          juce::dontSendNotification);
}

void AnamorphAudioProcessorEditor::updateModeVisibility()
{
    juce::Component* adv[] = {
        &outputModuleLabel, &mixK, &mixL, &outputK, &outputL, &outBalanceK, &outBalanceL,
        &autoMatchToggle, &applyGainButton, &matchReadout, &monoMakerToggle, &monoFreqK, &monoFreqL,
        &inputModuleLabel, &channelModeBox, &channelModeLabel, &soloBox, &soloLabel,
        &monoToggle, &swapToggle, &msToggle, &polLToggle, &polRToggle, &balanceK, &balanceL,
        &multibandLabel, &mbEnableToggle, &mbFreqLowK, &mbFreqLowL, &mbFreqHighK, &mbFreqHighL,
        &mbWLowK, &mbWLowL, &mbWMidK, &mbWMidL, &mbWHighK, &mbWHighL
    };
    for (auto* c : adv) c->setVisible (advanced);

    // Simple mode is just the Widen core, so ALL its text (names AND the value
    // numbers) is noticeably larger for presence; Advanced packs more in, so it
    // shrinks back (recurring Simple-font request).
    const float widenLabelFont = advanced ? 11.5f : 17.0f;
    const float widenValueFont = advanced ? 12.0f : 15.0f;
    auto setValueFont = [] (juce::Slider& s, float size)
    {
        for (auto* c : s.getChildren())
            if (auto* l = dynamic_cast<juce::Label*> (c))
                l->setFont (juce::Font (juce::FontOptions (size)));
    };
    for (auto* l : { &driveL, &amountL, &widthL, &haasDelayL, &velvetL, &chorusRateL, &chorusDepthL })
        l->setFont (juce::Font (juce::FontOptions (widenLabelFont)));
    for (auto* k : { &driveK, &amountK, &widthK, &haasDelayK, &velvetK, &chorusRateK, &chorusDepthK })
        setValueFont (*k, widenValueFont);
    algorithmLabel.setFont (juce::Font (juce::FontOptions (advanced ? 11.0f : 13.5f)).withExtraKerningFactor (0.2f));

    updateAlgoControls();
    resized();
    repaint();
}

void AnamorphAudioProcessorEditor::updateMsLabels()
{
    msState = msToggle.getToggleState();
    const juce::String ph = juce::String::charToString ((juce::juce_wchar) 0x00F8);
    polLToggle.setButtonText (ph + (msState ? " M" : " L")); // ø M / ø L  (#13)
    polRToggle.setButtonText (ph + (msState ? " S" : " R")); // ø S / ø R
    polLToggle.setTooltip (msState ? "Flip the polarity of the Mid channel."  : "Flip the polarity of the Left channel.");
    polRToggle.setTooltip (msState ? "Flip the polarity of the Side channel." : "Flip the polarity of the Right channel.");
    balanceK.updateText(); // re-derive the L/R vs M/S balance readout (#12)
}

void AnamorphAudioProcessorEditor::showAbout (bool show)    { aboutBackdrop.setVisible (show);    if (show) { aboutBackdrop.toFront (false); resized(); } }
void AnamorphAudioProcessorEditor::showSettings (bool show)
{
    if (show) { persistDragging = false; settingsBackdrop.reveal = 0.0f; }
    settingsBackdrop.setVisible (show);
    if (show) { settingsBackdrop.toFront (false); resized(); }
}

// ----------------------------------------------------------------------------
void AnamorphAudioProcessorEditor::timerCallback()
{
    // Sync cached view-state with the (possibly externally changed) parameters.
    if (advancedToggle.getToggleState() != advanced)
    {
        advanced = advancedToggle.getToggleState();
        updateModeVisibility();
    }
    if (metersToggle.getToggleState() != metersOn)
    {
        metersOn = metersToggle.getToggleState();
        if (metersOn) levelMeter->setVisible (true);
    }
    if (tooltipsToggle.getToggleState() != tooltipsOn)
    {
        tooltipsOn = tooltipsToggle.getToggleState();
        applyTooltipsEnabled();
    }
    if (msToggle.getToggleState() != msState) // external / preset / automation change (#12/#13)
        updateMsLabels();

    // Ease the level-meter reveal: vectorscope slides right, meter grows in.
    // Shorter, snappier than before (#27).
    const float target = metersOn ? 1.0f : 0.0f;
    if (std::abs (meterAnim - target) > 0.001f)
    {
        meterAnim += (target - meterAnim) * 0.55f;
        if (std::abs (meterAnim - target) < 0.01f)
        {
            meterAnim = target;
            if (! metersOn) levelMeter->setVisible (false);
        }
        resized();
    }

    // Settings overlay becomes see-through (panel + dim only; the bar stays
    // opaque) while the Persist bar is dragged -- shorter, gentler reveal (#26).
    // A short hold delay before revealing means a double-click-to-reset no longer
    // flashes the window transparent then opaque (#7).
    if (settingsBackdrop.isVisible())
    {
        persistHold = persistDragging ? persistHold + 1 : 0;
        const bool revealNow = persistDragging && persistHold >= 4; // ~165 ms at 24 Hz
        const float revTarget = revealNow ? 1.0f : 0.0f;
        if (std::abs (settingsBackdrop.reveal - revTarget) > 0.004f)
        {
            settingsBackdrop.reveal += (revTarget - settingsBackdrop.reveal) * 0.45f;
            settingsBackdrop.repaint();
        }
    }

    if (dimOverlay.isVisible() != bypassToggle.getToggleState())
    {
        dimOverlay.setVisible (bypassToggle.getToggleState());
        if (dimOverlay.isVisible()) dimOverlay.toFront (false);
    }

    processor.pollUndoCoalesce(); // fold settled sound edits into undo steps (#10-12)
    undoButton.setEnabled (processor.canUndo());
    redoButton.setEnabled (processor.canRedo());
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

    // Right control panel
    auto right = juce::Rectangle<int> (getWidth() - 300, 46, 300, getHeight() - 46).toFloat().reduced (8.0f);
    g.setColour (colours::bgPanel.withAlpha (0.55f));
    g.fillRoundedRectangle (right, 10.0f);

    if (advanced)
    {
        // Divider between the Widen and Output modules in the right panel, to
        // match the Input/Multiband divider in the bottom strip (#11). Y is set by
        // resized() so it always tracks the (now looser) Output module position.
        const float dy = (float) widenOutputDividerY;
        g.setColour (colours::outline.withAlpha (0.6f));
        g.drawLine (right.getX() + 12.0f, dy, right.getRight() - 12.0f, dy, 1.0f);

        // Bottom strip (INPUT + MULTIBAND) under the scope.
        auto strip = juce::Rectangle<int> (0, getHeight() - kStripHeight, getWidth() - 300, kStripHeight)
                         .toFloat().reduced (8.0f, 6.0f);
        g.setColour (colours::bgPanel.withAlpha (0.5f));
        g.fillRoundedRectangle (strip, 10.0f);
        g.setColour (colours::outline.withAlpha (0.6f));
        g.drawLine (strip.getX() + strip.getWidth() * 0.55f, strip.getY() + 10.0f,
                    strip.getX() + strip.getWidth() * 0.55f, strip.getBottom() - 10.0f, 1.0f);
    }
}

// ----------------------------------------------------------------------------
void AnamorphAudioProcessorEditor::resized()
{
    dimOverlay.setBounds (getLocalBounds().withTrimmedTop (46));
    aboutBackdrop.setBounds (getLocalBounds());
    settingsBackdrop.setBounds (getLocalBounds());

    // About panel + clickable link
    aboutBackdrop.panel = getLocalBounds().withSizeKeepingCentre (440, 290);
    {
        auto pnl = aboutBackdrop.panel.reduced (30, 26);
        aboutLink.setBounds (pnl.getX(), aboutBackdrop.panel.getBottom() - 50, 170, 20);
    }

    // Settings panel
    {
        auto s = getLocalBounds().withSizeKeepingCentre (360, 250);
        settingsBackdrop.panel = s;
        auto inner = s.reduced (24, 20);
        settingsTitle.setBounds (inner.removeFromTop (20));
        inner.removeFromTop (12);
        oversampleLabel.setBounds (inner.removeFromTop (16));
        oversampleBox.setBounds (inner.removeFromTop (24).reduced (0, 1));
        inner.removeFromTop (14);
        persistLabel.setBounds (inner.removeFromTop (16));
        scopePersistK.setBounds (inner.removeFromTop (24));
        inner.removeFromTop (14);
        tooltipsToggle.setBounds (inner.removeFromTop (26));
    }

    auto r = getLocalBounds();

    // ---- Top bar ----
    auto top = r.removeFromTop (46);
    {
        auto bar = top.reduced (8, 9);
        titleButton.setBounds (juce::Rectangle<int> (10, 0, 300, 46));
        bypassToggle.setBounds   (bar.removeFromRight (84));
        advancedToggle.setBounds (bar.removeFromRight (66)); // wider so "Adv" fits (#7)
        bar.removeFromRight (6);
        settingsButton.setBounds (bar.removeFromRight (74));
        bar.removeFromRight (8);
        metersToggle.setBounds   (bar.removeFromRight (34)); // compact icon (#7)
        bar.removeFromRight (12);
        redoButton.setBounds (bar.removeFromRight (30));
        undoButton.setBounds (bar.removeFromRight (30));
        bar.removeFromRight (12);
        copyButton.setBounds (bar.removeFromRight (46));
        abControl.setBounds (bar.removeFromRight (46).reduced (0, 1)); // shorter oval (#4)
    }

    auto content = r;
    auto rightPanel = content.removeFromRight (300);
    auto leftArea = content;

    juce::Rectangle<int> stripArea;
    if (advanced) stripArea = leftArea.removeFromBottom (kStripHeight);

    // ---- Scope + meters (with the reveal animation) ----
    {
        auto sa = leftArea.reduced (16);
        const int meterFull = 156; // fits 8 numbers + dB ruler; bars stay thin (#11/#17)
        const int reserve = juce::roundToInt (meterFull * meterAnim);
        if (reserve > 2)
        {
            levelMeter->setBounds (sa.removeFromLeft (reserve));
            sa.removeFromLeft (juce::roundToInt (12.0f * meterAnim));
        }

        auto vCol = sa.removeFromRight (26);
        sa.removeFromRight (8);
        auto hRow = sa.removeFromBottom (26);
        sa.removeFromBottom (8);
        const int side = juce::jmin (sa.getWidth(), sa.getHeight());
        auto sq = sa.withSizeKeepingCentre (side, side);
        scope->setBounds (sq);
        corrMeter->setBounds (vCol.withHeight (side).withY (sq.getY()));
        balanceMeter->setBounds (hRow.withWidth (side).withX (sq.getX()));
    }

    // ---- Right column: WIDEN (both modes) + OUTPUT (advanced) ----
    {
        auto placeKnob = [] (juce::Rectangle<int> area, juce::Slider& s, juce::Label& l)
        { l.setBounds (area.removeFromBottom (15)); s.setBounds (area.reduced (3, 1)); };
        auto twoKnob = [&] (juce::Rectangle<int> row, juce::Slider& s1, juce::Label& l1,
                            juce::Slider& s2, juce::Label& l2)
        { placeKnob (row.removeFromLeft (row.getWidth() / 2), s1, l1); placeKnob (row, s2, l2); };
        auto layoutAlgoRow = [&] (juce::Rectangle<int> algoRow)
        {
            algorithmBox.setBounds (algoRow.removeFromLeft (algoRow.getWidth() - 100).reduced (0, 1));
            algoRow.removeFromLeft (6);
            haasSideBox.setBounds (algoRow.reduced (0, 1));
            dimModeBox.setBounds  (haasSideBox.getBounds());
        };
        auto layoutCharacter = [&] (juce::Rectangle<int> cell, juce::Slider& wK, juce::Label& wL)
        {
            placeKnob (cell.removeFromLeft (cell.getWidth() / 2), wK, wL);
            placeKnob (cell, haasDelayK, haasDelayL);
            placeKnob (cell, velvetK, velvetL);
            placeKnob (cell.withTrimmedRight (cell.getWidth() / 2), chorusRateK, chorusRateL);
            placeKnob (cell.withTrimmedLeft  (cell.getWidth() / 2), chorusDepthK, chorusDepthL);
        };

        if (! advanced)
        {
            // SIMPLE: the Widen core, large and vertically centred so it no longer
            // hugs the top with an empty void below (#10).
            const int blockH = 16 + 8 + 30 + 26 + 148 + 18 + 148; // ~394
            auto col = rightPanel.reduced (22, 0);
            col.removeFromTop (juce::jmax (16, (col.getHeight() - blockH) / 2));

            { auto lr = col.removeFromTop (16); algoOptLabel.setBounds (lr.removeFromRight (94)); algorithmLabel.setBounds (lr); }
            col.removeFromTop (8);
            layoutAlgoRow (col.removeFromTop (30));
            col.removeFromTop (26);
            twoKnob (col.removeFromTop (148), driveK, driveL, amountK, amountL);
            col.removeFromTop (18);
            layoutCharacter (col.removeFromTop (148), widthK, widthL);
        }
        else
        {
            // ADVANCED: Widen + Output spread to fill the WHOLE column with even,
            // generous gaps instead of leaving a void at the bottom (#10).
            auto col = rightPanel.reduced (20, 18);

            { auto lr = col.removeFromTop (16); algoOptLabel.setBounds (lr.removeFromRight (94)); algorithmLabel.setBounds (lr); }
            col.removeFromTop (10);
            layoutAlgoRow (col.removeFromTop (30));
            col.removeFromTop (30);
            twoKnob (col.removeFromTop (120), driveK, driveL, amountK, amountL);
            col.removeFromTop (24);
            layoutCharacter (col.removeFromTop (120), widthK, widthL);

            const int gapTop = col.getY();
            col.removeFromTop (46);                  // breathing room before the Output module
            widenOutputDividerY = gapTop + 22;        // divider sits in the middle of that gap (#11)
            outputModuleLabel.setBounds (col.removeFromTop (16));
            col.removeFromTop (10);
            {
                auto row = col.removeFromTop (118);
                const int w = row.getWidth() / 3;
                placeKnob (row.removeFromLeft (w), mixK, mixL);
                placeKnob (row.removeFromLeft (w), outputK, outputL);
                placeKnob (row, outBalanceK, outBalanceL);
            }
            col.removeFromTop (22);
            // Level Match + Mono Maker share one X and one toggle size, vertically
            // aligned (#16). Their right-hand NUMBERS (dB readout / Hz) right-align
            // to the same edge; the Mono Maker bar starts at Apply's left edge (#11).
            const int togW = 118, numW = 56;
            auto lm = col.removeFromTop (30);
            autoMatchToggle.setBounds (lm.removeFromLeft (togW).reduced (2, 4));
            lm.removeFromLeft (4);
            matchReadout.setBounds (lm.removeFromRight (numW).reduced (2, 4)); // right-aligned dB readout
            applyGainButton.setBounds (lm.reduced (2, 4));                     // Apply fills the gap between
            col.removeFromTop (12);
            auto mm = col.removeFromTop (30);
            monoMakerToggle.setBounds (mm.removeFromLeft (togW).reduced (2, 4));
            mm.removeFromLeft (4);
            monoFreqK.setBounds (mm.reduced (2, 4)); // bar aligns under Apply, Hz box right-aligned under the dB
        }
    }

    // ---- Advanced bottom strip: INPUT (left) + MULTIBAND (right) ----
    if (advanced)
    {
        auto strip = stripArea.reduced (8, 6);
        auto input = strip.removeFromLeft (juce::roundToInt (strip.getWidth() * 0.55f));
        auto multi = strip;

        auto placeKnob = [] (juce::Rectangle<int> area, juce::Slider& s, juce::Label& l)
        { l.setBounds (area.removeFromBottom (15)); s.setBounds (area.reduced (4, 1)); };

        // INPUT
        {
            auto a = input.reduced (12, 8);
            inputModuleLabel.setBounds (a.removeFromTop (15));
            a.removeFromTop (2);

            // Input Balance: a compact knob block (knob + value + name packed
            // together, vertically centred) so the number/label sit right under
            // the knob instead of drifting to the bottom (#3).
            auto bal = a.removeFromRight (98);
            auto blk = bal.withSizeKeepingCentre (bal.getWidth(), juce::jmin (bal.getHeight(), 90));
            balanceL.setBounds (blk.removeFromBottom (14));
            balanceK.setBounds (blk.reduced (8, 0));
            a.removeFromRight (6);

            auto cmRow = a.removeFromTop (36);
            channelModeLabel.setBounds (cmRow.removeFromTop (14));
            channelModeBox.setBounds (cmRow.reduced (0, 1));
            a.removeFromTop (4);
            auto soRow = a.removeFromTop (36);
            soloLabel.setBounds (soRow.removeFromTop (14));
            soloBox.setBounds (soRow.reduced (0, 1));
            a.removeFromTop (6);
            // Five compact vertical toggles spread evenly across the row (#11/#14).
            auto tog = a.removeFromTop (36);
            const int tw = tog.getWidth() / 5;
            monoToggle.setBounds (tog.removeFromLeft (tw));
            swapToggle.setBounds (tog.removeFromLeft (tw));
            msToggle.setBounds   (tog.removeFromLeft (tw));
            polLToggle.setBounds (tog.removeFromLeft (tw));
            polRToggle.setBounds (tog);
        }

        // MULTIBAND
        {
            auto a = multi.reduced (12, 8);
            auto head = a.removeFromTop (18);
            multibandLabel.setBounds (head.removeFromLeft (head.getWidth() - 56));
            mbEnableToggle.setBounds (head.reduced (0, 0));
            a.removeFromTop (2);
            {
                auto row = a.removeFromTop (76);
                placeKnob (row.removeFromLeft (row.getWidth() / 2), mbFreqLowK, mbFreqLowL);
                placeKnob (row, mbFreqHighK, mbFreqHighL);
            }
            {
                auto row = a.removeFromTop (76);
                const int w = row.getWidth() / 3;
                placeKnob (row.removeFromLeft (w), mbWLowK, mbWLowL);
                placeKnob (row.removeFromLeft (w), mbWMidK, mbWMidL);
                placeKnob (row, mbWHighK, mbWHighL);
            }
        }
    }
}
