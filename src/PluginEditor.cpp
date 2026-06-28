#include "PluginEditor.h"
#include <cmath>

using namespace anamorph::gui;

// Tooltips read terser without a trailing full stop (#3). Applied centrally so
// every control set up through the helpers is covered.
static juce::String tidyTip (const juce::String& tip)
{
    auto t = tip.trim();
    while (t.endsWithChar ('.')) t = t.dropLastCharacters (1).trimEnd();
    return t;
}

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

    const auto pf = panel.toFloat();

    // Soft, feathered outer shadow just outside the panel -- subtle, not too dark
    // or too long (Settings, #14). Drawn before the panel so it sits behind it.
    if (dropShadow && panelA >= 0.999f)
    {
        for (int i = 5; i >= 0; --i)
        {
            const float t  = (float) i / 5.0f;            // 0 inner .. 1 outer
            const float ex = 1.5f + t * 13.0f;            // moderate feather range
            g.setColour (juce::Colours::black.withAlpha (0.13f * (1.0f - t)));
            g.fillRoundedRectangle (pf.expanded (ex).translated (0.0f, 2.5f), 12.0f + ex);
        }
    }

    if (panelA >= 0.999f) // solid (normal) state
    {
        if (lensFlare)
        {
            // About panel: the brighter 0.5.5 glass (background + edges) plus the
            // 0.5.5 anamorphic flare, now STATIC in the top-left corner -- exactly
            // the look from the reference screenshot (#3).
            const auto base = colours::bgPanel;
            juce::ColourGradient gr (base.brighter (0.13f), pf.getRight(), pf.getY(),
                                     base.darker  (0.32f), pf.getX(),     pf.getBottom(), false);
            gr.addColour (0.5, base.darker (0.04f));
            g.setGradientFill (gr);
            g.fillRoundedRectangle (pf, 12.0f);
            paintBrightEdges (g, pf, 12.0f);
            paintFlare (g, pf);
        }
        else
        {
            glass::fillPanel (g, pf, 12.0f, colours::bgPanel); // Settings: subtle glass
        }
    }
    else // mid-reveal (Persist drag): the SAME glass, just composited see-through,
    {    // so it never switches to a different "faded" style when revealed (#1).
        juce::ColourGradient gr (colours::bgPanel.brighter (0.04f).withAlpha (panelA), pf.getCentreX(), pf.getY(),
                                 colours::bgPanel.darker (0.20f).withAlpha (panelA), pf.getCentreX(), pf.getBottom(), false);
        g.setGradientFill (gr);
        g.fillRoundedRectangle (pf, 12.0f);
        glass::drawEdges (g, pf, 12.0f, panelA);
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

// The 0.5.5 bright glass EDGES, kept only for the About panel so it matches the
// reference screenshot while the rest of the UI uses the calmer glass (#3).
void AnamorphAudioProcessorEditor::Backdrop::paintBrightEdges (juce::Graphics& g, juce::Rectangle<float> bounds, float radius)
{
    auto r = bounds.reduced (0.5f);
    const float W = r.getWidth(), H = r.getHeight();
    g.setColour (anamorph::gui::colours::outline.withAlpha (0.85f));
    g.drawRoundedRectangle (r, radius, 1.0f);
    {
        juce::ColourGradient gr (juce::Colours::white.withAlpha (0.62f), r.getX(), r.getY(),
                                 juce::Colours::white.withAlpha (0.0f), r.getX() + W * 0.34f, r.getY() + H * 0.34f, false);
        g.setGradientFill (gr); g.drawRoundedRectangle (r, radius, 1.9f);
    }
    {
        juce::ColourGradient gr (juce::Colours::white.withAlpha (0.32f), r.getRight(), r.getBottom(),
                                 juce::Colours::white.withAlpha (0.0f), r.getRight() - W * 0.24f, r.getBottom() - H * 0.24f, false);
        g.setGradientFill (gr); g.drawRoundedRectangle (r, radius, 1.5f);
    }
    {
        auto ri = r.reduced (1.8f);
        const float rr = juce::jmax (0.5f, radius - 1.6f);
        juce::ColourGradient gr (juce::Colours::white.withAlpha (0.13f), r.getX(), r.getY(),
                                 juce::Colours::white.withAlpha (0.0f), ri.getCentreX(), ri.getCentreY(), false);
        g.setGradientFill (gr); g.drawRoundedRectangle (ri, rr, 1.4f);
    }
}

// The exact 0.5.5 anamorphic flare, but with the "cursor" pinned STATIC in the
// top-left corner -- the reference screenshot was that flare with the mouse there
// (#3). No longer follows the cursor, so the URL-hover restyle is gone too (#9).
void AnamorphAudioProcessorEditor::Backdrop::paintFlare (juce::Graphics& g, juce::Rectangle<float> pf)
{
    using anamorph::gui::colours::accent;
    using anamorph::gui::colours::accent2;

    juce::Graphics::ScopedSaveState save (g);
    juce::Path clip; clip.addRoundedRectangle (pf, 12.0f);
    g.reduceClipRegion (clip);

    // Pinned to the EXACT top-left corner -- where the 0.5.5 cursor flare clamped
    // (jlimit) when the mouse went to/beyond the corner. That is the look the
    // reference screenshot captured (#6).
    const float mx   = pf.getX();
    const float my   = pf.getY();
    const float frac = juce::jlimit (0.05f, 0.95f, (mx - pf.getX()) / pf.getWidth());
    const juce::Colour hot (0xfff4f8ff);

    // Wide-angle corner distortion: radial vignettes toward TL and BR corners.
    for (auto corner : { pf.getTopLeft(), pf.getBottomRight() })
    {
        juce::ColourGradient vig (juce::Colours::transparentBlack, pf.getCentreX(), pf.getCentreY(),
                                  juce::Colours::black.withAlpha (0.22f), corner.x, corner.y, true);
        g.setGradientFill (vig);
        g.fillRect (pf);
    }

    // Horizontal anamorphic streak: three stacked layers, brightest at the flare.
    for (int layer = 0; layer < 3; ++layer)
    {
        const float hh = (float) (3 - layer) * 6.0f + 1.0f;
        const float a  = (layer == 2) ? 0.55f : (layer == 1 ? 0.20f : 0.09f);
        const juce::Colour core = (layer == 2) ? hot : juce::Colour (0xffdfeaff);
        juce::ColourGradient hg (core.withAlpha (0.0f), pf.getX(), my,
                                 core.withAlpha (0.0f), pf.getRight(), my, false);
        hg.addColour (juce::jlimit (0.01, 0.99, (double) frac - 0.30), accent2.withAlpha (a * 0.35f));
        hg.addColour (frac, core.withAlpha (a));
        hg.addColour (juce::jlimit (0.01, 0.99, (double) frac + 0.30), accent.withAlpha (a * 0.35f));
        g.setGradientFill (hg);
        g.fillRect (pf.getX(), my - hh * 0.5f, pf.getWidth(), hh);
    }

    // Chromatic fringe lines: cool blue above, teal below.
    g.setColour (accent2.withAlpha (0.16f)); g.fillRect (pf.getX(), my - 3.0f, pf.getWidth(), 1.0f);
    g.setColour (accent .withAlpha (0.16f)); g.fillRect (pf.getX(), my + 2.0f, pf.getWidth(), 1.0f);

    // Hot radial core + a tight white centre.
    {
        juce::ColourGradient core (hot.withAlpha (0.60f), mx, my,
                                   juce::Colours::transparentBlack, mx + 34.0f, my, true);
        g.setGradientFill (core);
        g.fillEllipse (mx - 34.0f, my - 34.0f, 68.0f, 68.0f);
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.fillEllipse (mx - 2.6f, my - 2.6f, 5.2f, 5.2f);
    }

    // Glass refraction where the streak meets the left / right edges.
    g.setColour (hot.withAlpha (0.50f));
    g.fillEllipse (pf.getX() - 3.0f,     my - 3.0f, 6.0f, 6.0f);
    g.fillEllipse (pf.getRight() - 3.0f, my - 3.0f, 6.0f, 6.0f);
}

void AnamorphAudioProcessorEditor::ABControl::paint (juce::Graphics& g)
{
    // Racetrack / stadium frame: micro-gradient fill + soft accent edge glow (#6).
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const float rad = b.getHeight() * 0.5f;

    g.setColour (colours::accent.withAlpha (0.10f));
    g.fillRoundedRectangle (b.expanded (1.6f), rad + 1.6f);

    const float hovA = animOr (*this, "hovA", hovered); // eased hover wash (#10/F3)
    const float lift = 0.06f + 0.10f * hovA;
    juce::ColourGradient gr (colours::bgRaised.brighter (lift), b.getX(), b.getY(),
                             colours::bgRaised.darker (0.12f),   b.getX(), b.getBottom(), false);
    g.setGradientFill (gr);
    g.fillRoundedRectangle (b, rad);
    g.setColour (colours::outline.brighter (0.12f).interpolatedWith (colours::accent.withAlpha (0.6f), hovA));
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

    setOpaque (true); // fill our bounds every paint -> no see-through flash on a scale resize (#13)
    openGLContext.setContinuousRepainting (false);
   #if ! (JUCE_LINUX || JUCE_BSD)
    // GPU-composite the vectorscope on macOS / Windows. NOT on Linux/X11: attaching an
    // OpenGL context adds an extra embedded child X11 window, which multiplies the
    // ConfigureNotify events the host's XEmbedComponent turns into async lambdas that
    // capture a raw `this`; under a host that rapidly opens/closes the editor (pluginval
    // "Editor Automation", and real Linux DAWs), one of those lambdas can fire after the
    // editor window is gone -> use-after-free segfault inside JUCE's X11 embedding. The
    // CPU paint path is identical visually, so Linux simply renders without GL.
    openGLContext.attachTo (*this);
   #endif

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
    abControl.onToggle  = [this] { processor.abSwitchTo (processor.abActiveSlot() == 0 ? 1 : 0); knobSweepTime = 0.45; refreshPresetDisplay(); repaint(); };
    abControl.setTooltip ("A/B Compare"); // #17 (no period)
    addAndMakeVisible (abControl);
    copyButton.onClick = [this] { processor.abCopyToOther(); };
    copyButton.setTooltip (tidyTip ("Copy the current settings into the other A/B slot."));
    addAndMakeVisible (copyButton);

    settingsButton.onClick = [this] { showSettings (true); };
    addAndMakeVisible (settingsButton);

    // --- Preset browser (F2): ‹ name › -------------------------------------
    presetPrev.setButtonText (juce::String::charToString ((juce::juce_wchar) 0x2039)); // ‹
    presetNext.setButtonText (juce::String::charToString ((juce::juce_wchar) 0x203A)); // ›
    presetPrev.setComponentID ("presetnav");
    presetNext.setComponentID ("presetnav");
    presetName.setComponentID ("presetname");
    presetPrev.setTooltip ("Previous preset");
    presetNext.setTooltip ("Next preset");
    presetName.setTooltip ("Presets"); // short, no period (#12)
    presetPrev.onClick = [this] { processor.getEngine().requestDuck(); processor.getPresets().step (-1); knobSweepTime = 0.45; refreshPresetDisplay(); };
    presetNext.onClick = [this] { processor.getEngine().requestDuck(); processor.getPresets().step (+1); knobSweepTime = 0.45; refreshPresetDisplay(); };
    presetName.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (presetPrev);
    addAndMakeVisible (presetNext);
    addAndMakeVisible (presetName);

    // Save-preset overlay (F2): a small glass panel with a name field.
    savePresetBackdrop.dropShadow = true;
    savePresetBackdrop.onDismiss = [this] { showSavePreset (false); };
    addChildComponent (savePresetBackdrop);
    saveTitle.setText ("SAVE PRESET", juce::dontSendNotification);
    saveTitle.setColour (juce::Label::textColourId, colours::textDim);
    saveTitle.setFont (juce::Font (juce::FontOptions (12.0f)).withExtraKerningFactor (0.2f));
    savePresetBackdrop.addAndMakeVisible (saveTitle);
    saveNameEditor.setFont (juce::Font (juce::FontOptions (14.0f)));
    saveNameEditor.setColour (juce::TextEditor::backgroundColourId, colours::bg);
    saveNameEditor.setColour (juce::TextEditor::textColourId, colours::text);
    saveNameEditor.setColour (juce::TextEditor::outlineColourId, colours::outline);
    saveNameEditor.setColour (juce::TextEditor::focusedOutlineColourId, colours::accent.withAlpha (0.6f));
    saveNameEditor.setColour (juce::TextEditor::highlightColourId, colours::accent.withAlpha (0.3f));
    saveNameEditor.getProperties().set ("glow", true); // accent micro-glow border (#11)
    saveNameEditor.setSelectAllWhenFocused (true);
    saveNameEditor.onReturnKey = [this] { saveOkButton.triggerClick(); };
    saveNameEditor.onEscapeKey = [this] { showSavePreset (false); };
    savePresetBackdrop.addAndMakeVisible (saveNameEditor);
    saveOkButton.onClick = [this]
    {
        if (processor.getPresets().saveUser (saveNameEditor.getText()))
        {
            showSavePreset (false);
            refreshPresetDisplay();
        }
    };
    saveCancelButton.onClick = [this] { showSavePreset (false); };
    savePresetBackdrop.addAndMakeVisible (saveOkButton);
    savePresetBackdrop.addAndMakeVisible (saveCancelButton);

    undoButton.setButtonText (juce::String::charToString ((juce::juce_wchar) 0x21BA));
    redoButton.setButtonText (juce::String::charToString ((juce::juce_wchar) 0x21BB));
    undoButton.setComponentID ("icon"); // bigger, rotated glyph (#7)
    redoButton.setComponentID ("icon");
    undoButton.setTooltip ("Undo");
    redoButton.setTooltip ("Redo");
    undoButton.onClick = [this] { processor.undo(); knobSweepTime = 0.45; refreshPresetDisplay(); };
    redoButton.onClick = [this] { processor.redo(); knobSweepTime = 0.45; refreshPresetDisplay(); };
    addAndMakeVisible (undoButton);
    addAndMakeVisible (redoButton);

    setupToggleInternal (metersToggle, "", "Show level meters.", processor.getInternal().metersValue()); // #32, host-hidden
    metersToggle.setComponentID ("metersicon"); // level-meter glyph, not the word (#7)
    metersToggle.onClick = [this] { metersOn = metersToggle.getToggleState(); }; // visibility via layoutScopeArea (#2)

    setupToggle (advancedToggle, pid::advancedMode, "Adv", "Advanced mode"); // #17
    advancedToggle.onClick = [this] { advanced = advancedToggle.getToggleState(); applyUiScale(); updateModeVisibility(); };

    setupToggle (bypassToggle, pid::bypass, "Bypass", {});
    bypassToggle.setComponentID ("bypass");

    // --- WIDEN module (the Simple-mode core) ---
    setupCombo (algorithmBox, pid::algorithm, "The stereo-widening algorithm."); // #4
    algorithmBox.onChange = [this] { knobSweepTime = 0.45; updateAlgoControls(); resized(); };
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

    setupRotary (driveK,  driveL,  "Drive",  "Adds gentle saturation / density - 0 dB is clean");
    setupRotary (amountK, amountL, "Amount", "How much widening - 0% is fully transparent");
    setupRotary (widthK,  widthL,  "Width",  "Stereo width - 100% leaves the image unchanged");
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
    applyGainButton.setButtonText ("Apply Gain");  // clearer label (0.6.12 #7)
    applyGainButton.setComponentID ("apply");
    applyGainButton.setTooltip (tidyTip ("Apply the matched gain to Output Gain"));
    applyGainButton.onClick = [this] { processor.applyAutoGain(); };
    addAndMakeVisible (applyGainButton);
    matchReadout.setJustificationType (juce::Justification::centredRight); // align with the Hz readout below (#11)
    matchReadout.setColour (juce::Label::textColourId, colours::textDim);
    matchReadout.setFont (juce::Font (juce::FontOptions (11.0f)));
    addAndMakeVisible (matchReadout);

    // Mono Maker now lives inside the Output module (#24).
    setupToggle (monoMakerToggle, pid::monoMakerOn, "Mono Maker",
                 "Collapse everything below the frequency to mono.");
    setupRotary (monoFreqK, monoFreqL, "Freq", "Mono Maker crossover frequency.");
    monoFreqK.setSliderStyle (juce::Slider::LinearHorizontal);
    monoFreqK.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 18);
    // Right-justify the Hz readout (right edge lines up with the Level Match dB
    // above) and use a slightly smaller font for it (#11).
    for (auto* c : monoFreqK.getChildren())
        if (auto* lab = dynamic_cast<juce::Label*> (c))
        {
            lab->setJustificationType (juce::Justification::centredRight);
            lab->setFont (juce::Font (juce::FontOptions (11.0f)));
        }
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
    setupToggle (monoToggle, pid::monoSum, "Mono", "Sum to mono."); // #2
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

    // --- MULTIBAND module (advanced): drag-to-split spectral band editor ---
    multibandLabel.setText ("MULTIBAND", juce::dontSendNotification);
    multibandLabel.setJustificationType (juce::Justification::centredLeft);
    multibandLabel.setColour (juce::Label::textColourId, colours::textDim);
    multibandLabel.setFont (juce::Font (juce::FontOptions (11.0f)).withExtraKerningFactor (0.2f));
    addAndMakeVisible (multibandLabel);

    setupToggle (mbEnableToggle, pid::mbEnable, "On", "Apply the per-band stereo widths to the sound");
    imager = std::make_unique<anamorph::gui::SpectrumImager> (processor.getEngine().getScopeBuffer(),
                                                              processor.getAPVTS());
    imager->setAnimationSource (processor.getInternal().animationsFloatPtr()); // host-hidden UI-anim flag
    // Per-control tooltips live in the imager; idle areas show none (0.6.10 #13).
    imager->setTooltip ({});
    // Momentary solo audition is a non-undoable engine override (#8); the split / width
    // lines travel on a reset / preset / A-B / undo via the shared sweep window (#1).
    imager->onSoloPreview      = [this] (int mask) { processor.setSoloPreview (mask); };
    imager->onClearSoloPreview = [this] { processor.clearSoloPreview(); };
    imager->onSweep            = [this] { if (uiAnimOn) knobSweepTime = 0.45; };
    imager->isSweeping         = [this] { return uiAnimOn && knobSweepTime > 0.0; };
    addAndMakeVisible (*imager);

    // --- Overlays ---
    dimOverlay.setInterceptsMouseClicks (false, false);
    addChildComponent (dimOverlay);

    aboutBackdrop.aboutText = true;
    aboutBackdrop.lensFlare = true; // mouse-following anamorphic flare (#13)
    aboutBackdrop.onDismiss = [this] { showAbout (false); };
    addChildComponent (aboutBackdrop);
    aboutLink.setColour (juce::HyperlinkButton::textColourId, colours::accent);
    aboutLink.setFont (juce::Font (juce::FontOptions (13.0f)), false, juce::Justification::centredLeft);
    aboutLink.setJustificationType (juce::Justification::centredLeft);
    aboutLink.setTooltip (juce::String()); // no tooltip on the URL (#2)
    aboutBackdrop.addAndMakeVisible (aboutLink); // clickable www.rolly.tech

    settingsBackdrop.dropShadow = true; // soft feathered outer shadow (#14)
    settingsBackdrop.onDismiss = [this] { showSettings (false); };
    addChildComponent (settingsBackdrop);

    settingsTitle.setText ("SETTINGS", juce::dontSendNotification);
    settingsTitle.setColour (juce::Label::textColourId, colours::textDim);
    settingsTitle.setFont (juce::Font (juce::FontOptions (12.0f)).withExtraKerningFactor (0.2f));
    settingsBackdrop.addAndMakeVisible (settingsTitle);

    setupComboInternal (oversampleBox, { "Off (1x)", "2x", "4x", "8x" },
                        "Oversampling for the nonlinear stages - Off (1x) = no latency",
                        processor.getInternal().oversampleValue()); // host-hidden engine config
    oversampleLabel.setText ("Oversampling", juce::dontSendNotification);
    oversampleLabel.setColour (juce::Label::textColourId, colours::textDim);
    settingsBackdrop.addAndMakeVisible (oversampleLabel);
    settingsBackdrop.addAndMakeVisible (oversampleBox);

    // Whole-window scale (F4): vectors redraw at the new size, stays crisp.
    setupComboInternal (uiScaleBox, { "XS", "S", "M", "L", "XL" },
                        "Window size - M is the original; everything scales in proportion",
                        processor.getInternal().uiScaleValue());
    uiScaleLabel.setText ("Window Size", juce::dontSendNotification);
    uiScaleLabel.setColour (juce::Label::textColourId, colours::textDim);
    settingsBackdrop.addAndMakeVisible (uiScaleLabel);
    settingsBackdrop.addAndMakeVisible (uiScaleBox);

    // Scope Persistence is now a Settings bar (#21).
    persistLabel.setText ("Vectorscope Persist", juce::dontSendNotification);
    persistLabel.setColour (juce::Label::textColourId, colours::textDim);
    settingsBackdrop.addAndMakeVisible (persistLabel);
    scopePersistK.setSliderStyle (juce::Slider::LinearHorizontal);
    scopePersistK.setColour (juce::Slider::textBoxTextColourId, colours::textDim);
    scopePersistK.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    scopePersistK.setTooltip (tidyTip ("Vectorscope afterglow time " // #5
                              + juce::String::charToString ((juce::juce_wchar) 0x2014)
                              + " longer trails fade more slowly."));
    scopePersistK.setRepaintsOnMouseActivity (true); // hover glow (#10)
    settingsBackdrop.addAndMakeVisible (scopePersistK);
    scopePersistK.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18); // box built with our LnF
    // Host-hidden: bind to InternalState (juce::Value), not the APVTS. Set the range first
    // so the bound value lands in [0,1]; tag the unit for the bare-number value box (#36).
    scopePersistK.setRange (0.0, 1.0, 0.001); // match the old parameter's step (#36)
    // The SliderAttachment used to supply the parameter's percentage formatter; restore
    // it here so the value box shows "50%" (not a raw decimal) and parses bare numbers (#36).
    scopePersistK.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    scopePersistK.valueFromTextFunction = [] (const juce::String& t) { return t.removeCharacters ("% ").getDoubleValue() / 100.0; };
    scopePersistK.getValueObject().referTo (processor.getInternal().scopePersistValue());
    scopePersistK.getProperties().set ("unit", "pct");
    // attachSlider (which used to seed Knob::resetValue from the parameter default) is no
    // longer called, so set the double-click / Alt-click reset target explicitly (#7).
    scopePersistK.resetValue = 0.5;
    // ...and the eased reset sweep attachSlider also wired, so a double-click / Alt-click
    // reset animates like every other Knob (#7).
    scopePersistK.onSweep = [this] { if (uiAnimOn) { knobSweepTime = 0.45; scopePersistK.getProperties().set ("resetSweep", true); } };
    scopePersistK.onValueChange = [this] { applyScopePersist(); };
    // Listen for the mouse wheel ON the Persist bar so a sustained scroll reveals the
    // window (handled in mouseWheelMove, the single source of truth, so a single
    // notch never triggers it) (#1).
    scopePersistK.addMouseListener (this, false);
    // While dragging Persist, fade the Settings overlay so the live vectorscope
    // behind it is visible (#9).
    scopePersistK.onDragStart = [this] { persistDragging = true; };
    scopePersistK.onDragEnd   = [this] { persistDragging = false; };

    tooltipsToggle.setButtonText ("Tooltips");
    tooltipsToggle.setTooltip (tidyTip ("Show these hover hints on every control."));
    settingsBackdrop.addAndMakeVisible (tooltipsToggle);
    tooltipsToggle.getToggleStateValue().referTo (processor.getInternal().tooltipsValue()); // host-hidden

    animToggle.setButtonText ("UI Animations");
    animToggle.setTooltip (tidyTip ("Smooth micro-animations on hovers, presses and switches")); // no F3 ref (#4)
    settingsBackdrop.addAndMakeVisible (animToggle);
    animToggle.getToggleStateValue().referTo (processor.getInternal().animationsValue()); // host-hidden
    // Adopt the new state IMMEDIATELY on click (not on the next 24 Hz tick): so
    // turning it ON makes uiAnimOn true before the next frame and the toggle's own
    // slide plays, while turning it OFF makes it false first so the toggle snaps --
    // the user feels the on/off difference on the switch itself (#1).
    animToggle.onClick = [this] { uiAnimOn = animToggle.getToggleState(); };

    // Initial cached view-state from the (recalled) parameters.
    advanced   = advancedToggle.getToggleState();
    metersOn   = metersToggle.getToggleState();
    tooltipsOn = tooltipsToggle.getToggleState();
    meterAnim  = metersOn ? 1.0f : 0.0f;
    levelMeter->setVisible (metersOn);

    // Components not built through the setup helpers still get micro-anims (F3).
    for (auto* c : std::initializer_list<juce::Component*> {
             &copyButton, &settingsButton, &undoButton, &redoButton, &applyGainButton,
             &presetPrev, &presetNext, &presetName, &saveOkButton, &saveCancelButton,
             &scopePersistK, &tooltipsToggle, &animToggle, &abControl })
        registerAnimated (*c);

    applyTooltipsEnabled();
    applyScopePersist();
    updateAlgoControls();
    updateMsLabels();
    updateModeVisibility();
    uiAnimOn = animToggle.getToggleState();
    refreshPresetDisplay();
    setResizable (false, false);
    applyUiScale();               // sets the base size (mode-dependent height) + XS..XL scale
    startTimerHz (24);
    // One vblank drives every per-frame animation: the meter reveal (#6) and the
    // micro-animations (F3); both early-out to a few compares when idle.
    meterVBlank = juce::VBlankAttachment (this, [this] (double t)
    {
        const double dt = juce::jlimit (0.0, 0.05, t - lastFrameTime);
        lastFrameTime = t;
        stepMeterReveal (dt);
        stepMicroAnims (dt);
    });
}

AnamorphAudioProcessorEditor::~AnamorphAudioProcessorEditor()
{
    // Release the per-frame callbacks FIRST, before anything they touch is torn down:
    // the VBlank lambda captures `this` and can trigger a repaint, so it must be stopped
    // before the timer / GL / look-and-feels go away (defensive editor-lifecycle hygiene).
    meterVBlank = {};
    stopTimer();
    openGLContext.detach();
    channelModeBox.setLookAndFeel (nullptr);
    soloBox.setLookAndFeel (nullptr);
    for (auto* box : { &algorithmBox, &haasSideBox, &dimModeBox })
        box->setLookAndFeel (nullptr); // detach simpleCombo before it's destroyed (#17)
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
    s.setTooltip (tidyTip (tip));
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
    registerAnimated (s); // eased hover/press glow (F3)
}

void AnamorphAudioProcessorEditor::attachSlider (juce::Slider& s, const char* id)
{
    sliderAtts.add (new SliderAttachment (processor.getAPVTS(), id, s));

    auto* p = processor.getAPVTS().getParameter (id);
    if (auto* k = dynamic_cast<Knob*> (&s); k != nullptr && p != nullptr)
    {
        k->resetValue = p->getNormalisableRange().convertFrom0to1 (p->getDefaultValue()); // #6
        // A RESET (double-click / Option-click) sweeps the eased position (0.6.7 #21).
        // resetSweep lets that travel play even while the reset's mouse button is still
        // held (alt-click / a double-click's 2nd press); only flagged when animations
        // are on, so with them off the knob snaps exactly as before.
        k->onSweep = [this, k] { if (uiAnimOn) { knobSweepTime = 0.45; k->getProperties().set ("resetSweep", true); } };
    }

    // A TEXT-box value entry should also sweep, but a drag, the scroll wheel, and
    // host AUTOMATION must NOT: text entry is the only one of those that changes the
    // value while the control holds keyboard focus and the mouse is up (0.6.7 #21).
    if (! s.onValueChange)
        s.onValueChange = [this, &s] {
            if (uiAnimOn && ! s.isMouseButtonDown() && s.hasKeyboardFocus (true))
                knobSweepTime = 0.45;
        };

    // Tag the unit so the value box can show a bare number while editing (#36).
    const juce::String sid (id);
    juce::String unit;
    if      (sid == pid::drive || sid == pid::outputGain) unit = "db";
    else if (sid == pid::haasDelay) unit = "ms";
    else if (sid == pid::amount || sid == pid::velvetDensity || sid == pid::chorusDepth
          || sid == pid::width || sid == pid::mix
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
    box.setTooltip (tidyTip (tip));
    box.setRepaintsOnMouseActivity (true); // hover feedback (#10)
    passComboHoverThrough (box);
    allCombos.add (&box); // timer drives the hover repaint (#20)
    addAndMakeVisible (box);
    comboAtts.add (new ComboBoxAttachment (processor.getAPVTS(), id, box));
    registerAnimated (box); // eased hover lift (F3)
}

void AnamorphAudioProcessorEditor::setupToggle (juce::ToggleButton& t, const char* id,
                                                const juce::String& text, const juce::String& tip)
{
    t.setButtonText (text);
    if (tip.isNotEmpty()) t.setTooltip (tidyTip (tip));
    addAndMakeVisible (t);
    buttonAtts.add (new ButtonAttachment (processor.getAPVTS(), id, t));
    registerAnimated (t); // eased switch slide + hover (F3)
}

// Same as setupCombo/setupToggle, but bound to a host-HIDDEN InternalState value via
// juce::Value (two-way) instead of an APVTS parameter -- for the Settings / view controls
// that must not appear in the host's parameter list. ComboBox IDs are 1-based, matching
// the InternalState convention (index + 1).
void AnamorphAudioProcessorEditor::setupComboInternal (juce::ComboBox& box, const juce::StringArray& items,
                                                       const juce::String& tip, juce::Value value)
{
    box.addItemList (items, 1);
    box.setTooltip (tidyTip (tip));
    box.setRepaintsOnMouseActivity (true);
    passComboHoverThrough (box);
    allCombos.add (&box);
    addAndMakeVisible (box);
    box.getSelectedIdAsValue().referTo (value);
    registerAnimated (box);
}

void AnamorphAudioProcessorEditor::setupToggleInternal (juce::ToggleButton& t, const juce::String& text,
                                                        const juce::String& tip, juce::Value value)
{
    t.setButtonText (text);
    if (tip.isNotEmpty()) t.setTooltip (tidyTip (tip));
    addAndMakeVisible (t);
    t.getToggleStateValue().referTo (value);
    registerAnimated (t);
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

    // The bottom-right Widen slot hosts a DIFFERENT knob per algorithm (Haas
    // Delay <-> Velvet Density). When the algorithm swaps which knob lives there,
    // hand the OUTGOING knob's visual position to the INCOMING one so the slot's
    // knob sweeps continuously to its new value instead of teleporting (#8). The
    // Chorus layout has two knobs in that area and is deliberately left to their
    // own separate vpos registers (#8).
    auto getVp = [] (juce::Slider& s)
    {
        return (float) (double) s.getProperties().getWithDefault (
                   "vpos", (double) s.valueToProportionOfLength (s.getValue()));
    };
    float oldBR = -1.0f;
    if (algo != brPrevAlgo && brPrevAlgo >= 0)
    {
        if (haasDelayK.isVisible())    oldBR = getVp (haasDelayK);   // leaving Haas
        else if (velvetK.isVisible())  oldBR = getVp (velvetK);      // leaving Velvet
    }

    haasDelayK.setVisible (algo == 0);  haasDelayL.setVisible (algo == 0);
    velvetK.setVisible    (algo == 1);  velvetL.setVisible    (algo == 1);
    chorusRateK.setVisible (algo == 2); chorusRateL.setVisible (algo == 2);
    chorusDepthK.setVisible (algo == 2);chorusDepthL.setVisible (algo == 2);
    haasSideBox.setVisible (algo == 0);
    dimModeBox.setVisible  (algo == 3);

    if (oldBR >= 0.0f)
    {
        if (algo == 0)      haasDelayK.getProperties().set ("vpos", (double) oldBR); // -> Haas Delay
        else if (algo == 1) velvetK.getProperties().set ("vpos", (double) oldBR);    // -> Velvet Density
    }
    brPrevAlgo = algo;

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
        &multibandLabel, &mbEnableToggle
    };
    for (auto* c : adv) c->setVisible (advanced);
    if (imager) imager->setVisible (advanced);

    // The Widen fonts AND the Simple-mode combo LookAndFeel are applied inside resized() (see
    // applyWidenFonts), so they change in the SAME layout pass as the resize with no lag (#F/#4).

    updateAlgoControls();
    resized();
    repaint();
}

// Mode-dependent Widen fonts. Called at the START of resized() so that when the ADV toggle
// resizes the window the font sizes update in the very same layout pass -- no lag (#F).
void AnamorphAudioProcessorEditor::applyWidenFonts()
{
    const float widenLabelFont = advanced ? 11.5f : 15.0f;
    const float widenValueFont = advanced ? 12.0f : 14.5f;
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
    const auto capFont = juce::Font (juce::FontOptions (advanced ? 11.0f : 13.5f)).withExtraKerningFactor (0.2f);
    algorithmLabel.setFont (capFont);
    algoOptLabel.setFont (capFont);

    // The Widen combos (Algorithm + Style/Focus) get a larger-text LookAndFeel in Simple mode.
    // Swapping it here -- inside the resize pass -- keeps their text size changing in step with
    // the layout, not a frame later (0.6.17 #4). Only swap on a real mode change (the swap
    // rebuilds the combo's label, so it must not run every resized()).
    const int mode = advanced ? 1 : 0;
    if (comboFontMode != mode)
    {
        comboFontMode = mode;
        for (auto* box : { &algorithmBox, &haasSideBox, &dimModeBox })
        {
            box->setLookAndFeel (advanced ? nullptr : &simpleCombo);
            passComboHoverThrough (*box);
        }
    }
}

void AnamorphAudioProcessorEditor::updateMsLabels()
{
    msState = msToggle.getToggleState();
    const juce::String ph = juce::String::charToString ((juce::juce_wchar) 0x00F8);
    polLToggle.setButtonText (ph + (msState ? " M" : " L")); // ø M / ø L  (#13)
    polRToggle.setButtonText (ph + (msState ? " S" : " R")); // ø S / ø R
    polLToggle.setTooltip (msState ? "Flip the polarity of the Mid channel"  : "Flip the polarity of the Left channel");
    polRToggle.setTooltip (msState ? "Flip the polarity of the Side channel" : "Flip the polarity of the Right channel");
    balanceK.updateText(); // re-derive the L/R vs M/S balance readout (#12)
}

void AnamorphAudioProcessorEditor::showAbout (bool show)    { aboutBackdrop.setVisible (show);    if (show) { aboutBackdrop.toFront (false); resized(); } }
void AnamorphAudioProcessorEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails&)
{
    // Sustained scroll of the Persist bar reveals the window: the first notch arms a
    // short window, a SECOND notch inside it reveals (so a single notch doesn't),
    // and a ~0.5 s dwell holds it after the last notch. Drag is handled separately
    // and is unchanged (#1).
    if (e.eventComponent == &scopePersistK && ! persistDragging)
    {
        if (persistScrollWindow > 0.0) persistRevealTimer = 0.5; // sustained -> reveal + dwell
        persistScrollWindow = 0.30;                              // (re)arm the recent-change window
    }
}

void AnamorphAudioProcessorEditor::showSettings (bool show)
{
    if (show)
    {
        // Fully reset the see-through state so opening Settings never flashes
        // transparent from a stale reveal/scroll timer (#8).
        persistDragging = false;
        persistHold = 0;
        persistScrollWindow = 0.0;
        persistRevealTimer = 0.0;
        settingsBackdrop.reveal = 0.0f;
    }
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
        applyUiScale();              // resize for / against the Multiband bar (#2)
        updateModeVisibility();
    }
    if (metersToggle.getToggleState() != metersOn)
        metersOn = metersToggle.getToggleState(); // layoutScopeArea owns visibility (#2)
    if (tooltipsToggle.getToggleState() != tooltipsOn)
    {
        tooltipsOn = tooltipsToggle.getToggleState();
        applyTooltipsEnabled();
    }
    if (msToggle.getToggleState() != msState) // external / preset / automation change (#12/#13)
        updateMsLabels();

    uiAnimOn = animToggle.getToggleState(); // micro-anims follow the Settings switch (F3)

    // Whole-window scale: follow the parameter (Settings combo, state recall, F4).
    if (uiScaleBox.getSelectedItemIndex() != lastScaleIdx)
        applyUiScale();

    refreshPresetDisplay(); // preset name + dirty dot track outside edits (F2)

    // Drive combo hover from the actual cursor position so exactly ONE box is ever
    // lit and a stale highlight always clears -- the enter/exit events through the
    // child label were unreliable (sometimes stuck on, sometimes never lit) (#20).
    for (auto* box : allCombos)
    {
        const bool hov = box->isShowing()
                       && box->getLocalBounds().contains (box->getMouseXYRelative());
        if ((bool) box->getProperties().getWithDefault ("hov", false) != hov)
        {
            box->getProperties().set ("hov", hov);
            box->repaint();
        }
    }

    // (The level-meter reveal animation itself runs per display frame in
    //  stepMeterReveal, driven by the vblank attachment -- #6.)

    // Settings overlay becomes see-through (panel + dim only; the bar stays
    // opaque) while the Persist bar is dragged -- shorter, gentler reveal (#26).
    // A short hold delay before revealing means a double-click-to-reset no longer
    // flashes the window transparent then opaque (#7).
    if (settingsBackdrop.isVisible())
    {
        constexpr double dt = 1.0 / 24.0;
        // Drag reveal: normally waits a short anti-flicker hold (so a double-click
        // reset doesn't flash). BUT if the window is ALREADY see-through (e.g. from a
        // recent scroll), grabbing the bar keeps it transparent IMMEDIATELY and holds
        // it there until release -- no recover-then-retransparent flicker (#3).
        const bool alreadyRevealed = settingsBackdrop.reveal > 0.5f;
        persistHold = persistDragging ? persistHold + 1 : 0;
        const bool dragReveal = persistDragging && (persistHold >= 4 || alreadyRevealed);
        // Scroll / type reveal: sustained change, then a ~0.5 s dwell after stopping (#1).
        if (persistScrollWindow > 0.0) persistScrollWindow -= dt;
        if (persistRevealTimer  > 0.0) persistRevealTimer  -= dt;
        const bool scrollReveal = (persistRevealTimer > 0.0) && ! persistDragging;

        const float revTarget = (dragReveal || scrollReveal) ? 1.0f : 0.0f;
        if (std::abs (settingsBackdrop.reveal - revTarget) > 0.004f)
        {
            settingsBackdrop.reveal += (revTarget - settingsBackdrop.reveal) * 0.45f;
            // Snap exactly onto the target when close: otherwise reveal settles at
            // ~0.004 and panelA stays < 1, drawing the FADED panel style instead of
            // the glass one after the bar restores (#10).
            if (std::abs (settingsBackdrop.reveal - revTarget) <= 0.004f)
                settingsBackdrop.reveal = revTarget;
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

// Vsync-stepped meter reveal: the v0.5.9 exponential ease (factor 0.55 per
// 1/24 s, time-corrected to whatever the display rate is) -- the prettier curve
// the user preferred (#1). Stateless, so it always converges and can never stall
// part-open (#2). Follows the UI-animation switch: off = snap instantly (#9).
void AnamorphAudioProcessorEditor::stepMeterReveal (double dt)
{
    const float target = metersOn ? 1.0f : 0.0f;
    const float diff = std::abs (meterAnim - target);
    if (diff < 1.0e-4f)
    {
        if (diff > 0.0f) { meterAnim = target; layoutScopeArea(); } // one final snap
        return; // idle otherwise
    }

    if (uiAnimOn)
        meterAnim += (target - meterAnim) * (1.0f - (float) std::pow (0.45, dt * 24.0));
    else
        meterAnim = target;

    if (std::abs (meterAnim - target) < 0.01f)
        meterAnim = target;
    layoutScopeArea();
}

void AnamorphAudioProcessorEditor::registerAnimated (juce::Component& c)
{
    animated.addIfNotAlreadyThere (&c);
    // Seed the eased properties so they ALWAYS exist: the LookAndFeel then reads a
    // real eased value rather than ever falling back to the binary state (the
    // fallback/property swap is what let a click flicker), and a toggle that loads
    // in the ON state eases correctly from its real position on the first click
    // instead of snapping (#1).
    auto& p = c.getProperties();
    p.set ("hovA", 0.0);
    p.set ("actA", 0.0);
    if (auto* t = dynamic_cast<juce::ToggleButton*> (&c))
        p.set ("onA", t->getToggleState() ? 1.0 : 0.0);
}

// Micro-animation driver (F3): eases per-component "hovA" (hover), "actA"
// (press), "onA" (toggle position) and "vpos" (knob value travel) properties
// every display frame; the LookAndFeel blends its glows/lifts/knob angle with
// them. Fast in, gentler out -- the Apple-feel non-linearity -- and when the
// Settings switch is off the rates snap to 1 so everything behaves exactly as
// before. Idle cost is a handful of compares per control; repaints only fire
// while a value is actually moving.
void AnamorphAudioProcessorEditor::stepMicroAnims (double dt)
{
    static const juce::Identifier hovA ("hovA"), actA ("actA"), onA ("onA"), vpos ("vpos");

    // Exponential ease-out follows (non-linear, the curve the toggle slide used
    // and the user liked). Hover/press eased a touch more deliberately so the
    // highlight transition clearly reads as an animation, not a snap (#4); the
    // toggle slide keeps its liked timing (#1).
    const float rIn  = uiAnimOn ? 1.0f - std::exp (-(float) dt / 0.075f) : 1.0f;
    const float rOut = uiAnimOn ? 1.0f - std::exp (-(float) dt / 0.150f) : 1.0f;
    const float rAct = uiAnimOn ? 1.0f - std::exp (-(float) dt / 0.045f) : 1.0f;
    const float rOn  = uiAnimOn ? 1.0f - std::exp (-(float) dt / 0.055f) : 1.0f;
    const float rPos = uiAnimOn ? 1.0f - std::exp (-(float) dt / 0.090f) : 1.0f;

    // Knob/slider position only EASES while a sweep window is open (preset / A-B /
    // undo / algorithm change); outside it, a value jump from the scroll wheel or
    // host automation snaps instantly so it never lags or misleads (#3).
    if (knobSweepTime > 0.0) knobSweepTime -= dt;
    const bool sweeping = uiAnimOn && knobSweepTime > 0.0;

    for (auto* c : animated)
    {
        auto& props = c->getProperties();

        // Hover is hit-tested against the live cursor rather than read from
        // enter/exit events: those fire unreliably across clicks, relayouts and
        // pop-ups, which is what made a click occasionally flicker (#10).
        const bool over = c->isShowing()
                        && c->getLocalBounds().contains (c->getMouseXYRelative());
        float hovT = over ? 1.0f : 0.0f;
        float actT = -1.0f, onT = -1.0f;

        // The animated properties are SEEDED at registration (registerAnimated),
        // so the default here is only a fallback; it must NOT be the target, or
        // the very first transition starts already-arrived and never plays -- the
        // bug that made the switch animation vanish last version (#1).
        auto stepVal = [&props] (const juce::Identifier& key, float target, float up, float down) -> bool
        {
            const float curr = (float) (double) props.getWithDefault (key, 0.0);
            float next = curr + (target - curr) * (target > curr ? up : down);
            if (std::abs (next - target) < 0.004f) next = target;
            if (std::abs (next - curr) < 0.0015f) return false;
            props.set (key, juce::jlimit (0.0f, 1.0f, next));
            return true;
        };

        if (auto* s = dynamic_cast<juce::Slider*> (c))
        {
            const bool buttonHeld = s->isMouseButtonDown()
                                  || (bool) props.getWithDefault ("dragging", false);
            actT = buttonHeld ? 1.0f : 0.0f;

            // ROTARY knobs AND LINEAR sliders ease their drawn position toward the
            // live value, so a preset / A-B switch SWEEPS them instead of teleporting
            // (#5/#7). While the user turns the control, snap so it stays 1:1 (no
            // lag). vpos is kept current at rest so there's a real "from" position to
            // ease out of the instant the value jumps. A RESET (double-click /
            // alt-click) must sweep even while the button is still physically held,
            // so it opts out of the button-down snap until its travel converges.
            const bool resetSweep = (bool) props.getWithDefault ("resetSweep", false);
            const float realPos = (float) s->valueToProportionOfLength (s->getValue());
            const float curr = (float) (double) props.getWithDefault (vpos, (double) realPos);
            const bool snapPos = (buttonHeld && ! resetSweep) || ! sweeping;
            float vp = snapPos ? realPos : curr + (realPos - curr) * rPos;
            if (std::abs (vp - realPos) < 0.0015f)
            {
                vp = realPos;
                if (resetSweep) props.set ("resetSweep", false); // travel finished
            }
            if (std::abs (vp - curr) > 0.0004f) c->repaint();
            props.set (vpos, vp);
        }
        else if (auto* t = dynamic_cast<juce::ToggleButton*> (c))
            onT = t->getToggleState() ? 1.0f : 0.0f;

        bool changed = stepVal (hovA, hovT, rIn, rOut);
        if (actT >= 0.0f) changed = stepVal (actA, actT, rAct, rOut) || changed;
        if (onT  >= 0.0f) changed = stepVal (onA,  onT,  rOn,  rOn)  || changed;
        if (changed) c->repaint();
    }
}

// Whole-window scale (F4): the layout stays at its logical 940x720 and a plain
// transform scales the editor; every control is vector-drawn, so the result is
// crisp at any step and the composition cannot drift. The wrapper resizes the
// host window from the transformed bounds automatically. Applied as a single
// instant step -- a resize doesn't want an animation (#3).
void AnamorphAudioProcessorEditor::applyUiScale()
{
    static constexpr float scales[] = { 0.75f, 0.85f, 1.0f, 1.25f, 1.5f };
    const int idx = juce::jlimit (0, 4, uiScaleBox.getSelectedItemIndex());
    lastScaleIdx = idx;

    // Advanced extends the window downward for the full-width Multiband bar instead
    // of compressing the scope (0.6.7 #2). The base (unscaled) size depends on the
    // mode; the XS..XL transform scales it, COMPOSED with the host's display/DPI
    // scale so the user scale and the host DPI cooperate instead of overwriting each
    // other (the Windows window-size bug). hostScale is 1.0 unless the host set it.
    setSize (kWidth, advanced ? kAdvHeight : kHeight);
    setTransform (juce::AffineTransform::scale (hostScale * scales[idx]));
    if (openGLContext.isAttached())
        openGLContext.triggerRepaint();
}

// The host calls this with its display/DPI scale (notably Windows hosts on a
// scaled display). JUCE's default would overwrite our transform with scale(newScale)
// and so wipe out the user's Window-Size choice; instead we remember it and re-apply
// the COMPOSED transform, so DPI and the UI scale multiply correctly (#window-size).
void AnamorphAudioProcessorEditor::setScaleFactor (float newScale)
{
    hostScale = (newScale > 0.0f) ? newScale : 1.0f;
    applyUiScale();
}

// ----------------------------------------------------------------------------
//  Preset browser (F2)
// ----------------------------------------------------------------------------
namespace
{
    // Pixel width of a string in a font, via GlyphArrangement (no deprecated API).
    static float textWidth (const juce::Font& f, const juce::String& s)
    {
        if (s.isEmpty()) return 0.0f;
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, s, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }

    // Pro Tools-style track-name abbreviation: keep each word's first letter, drop
    // the vowels from the rest ("Drum" -> "Drm", "Stereo" -> "Stro"), so a long
    // preset name still reads when the slot is narrow (#7).
    static juce::String abbreviate (const juce::String& name)
    {
        auto words = juce::StringArray::fromTokens (name, " ", "");
        juce::String out;
        for (const auto& word : words)
        {
            if (word.isEmpty()) continue;
            if (out.isNotEmpty()) out << ' ';
            out << word[0];
            for (int i = 1; i < word.length(); ++i)
                if (! juce::String ("aeiouAEIOU").containsChar (word[i]))
                    out << word[i];
        }
        return out;
    }
}

void AnamorphAudioProcessorEditor::refreshPresetDisplay()
{
    auto& pm = processor.getPresets();
    // A small asterisk marks an edited preset -- lighter than the old bullet (#6).
    const juce::String marker = pm.isDirty() ? " *" : juce::String();

    const juce::Font font (juce::FontOptions (13.0f)); // matches the presetname button font
    const float avail = (float) presetName.getWidth() - 12.0f - textWidth (font, marker);

    juce::String name = pm.currentName();
    if (textWidth (font, name) > avail)
    {
        name = abbreviate (pm.currentName());          // consonant skeleton (#7)
        while (name.isNotEmpty() && textWidth (font, name) > avail)
            name = name.dropLastCharacters (1);        // hard-clip if still too wide
    }

    const juce::String shown = name + marker;
    if (presetName.getButtonText() != shown)
        presetName.setButtonText (shown);
}

void AnamorphAudioProcessorEditor::showPresetMenu()
{
    auto& pm = processor.getPresets();
    pm.refresh();

    juce::PopupMenu m;
    m.setLookAndFeel (&lnf);
    const int cur = pm.currentIndex();
    m.addSectionHeader ("FACTORY");
    bool userHeader = false;
    for (int i = 0; i < pm.entries().size(); ++i)
    {
        const auto& e = pm.entries().getReference (i);
        if (! e.isFactory && ! userHeader) { m.addSectionHeader ("USER"); userHeader = true; }
        m.addItem (i + 1, e.name, true, i == cur);
    }
    const juce::String ellip = juce::String::charToString ((juce::juce_wchar) 0x2026);
    m.addSeparator();
    m.addItem (10001, "Save Preset" + ellip);
    m.addItem (10002, "Load Preset" + ellip); // OS file chooser (#3)

    // Widen the list so the longest factory name ("Synth Dimension") shows in
    // full -- the slot itself is narrow (#8).
    m.showMenuAsync (juce::PopupMenu::Options()
                         .withTargetComponent (presetName)
                         .withMinimumWidth (228),
        [this] (int r)
        {
            if (r == 0) return;
            if (r == 10001) { showSavePreset (true); return; }
            if (r == 10002) { showLoadPreset(); return; }
            processor.getEngine().requestDuck();   // mask the level jump (#1, 0.6.4)
            processor.getPresets().load (r - 1);
            knobSweepTime = 0.45; // sweep the knobs to the preset (#3)
            refreshPresetDisplay();
        });
}

void AnamorphAudioProcessorEditor::showLoadPreset()
{
    auto dir = anamorph::PresetManager::presetDirectory();
    dir.createDirectory(); // so the chooser opens somewhere sensible even when empty
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load Anamorph Preset", dir, "*" + anamorph::PresetManager::fileSuffix());

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file.existsAsFile())
            {
                processor.getEngine().requestDuck(); // mask the level jump (#1, 0.6.4)
                if (processor.getPresets().loadFile (file))
                {
                    knobSweepTime = 0.45; // sweep the knobs to the preset (#3)
                    refreshPresetDisplay();
                }
            }
        });
}

void AnamorphAudioProcessorEditor::showSavePreset (bool show)
{
    savePresetBackdrop.setVisible (show);
    if (show)
    {
        savePresetBackdrop.toFront (false);
        resized();
        saveNameEditor.setText (processor.getPresets().currentName(), false);
        saveNameEditor.grabKeyboardFocus();
    }
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

    // Right control panel (WIDEN) -- spans the scope row only.
    const int rowH = advanced ? kScopeRowH : (kHeight - 46);
    auto right = juce::Rectangle<int> (getWidth() - 300, 46, 300, rowH).toFloat().reduced (8.0f);
    g.setColour (colours::bgPanel.withAlpha (0.55f));
    g.fillRoundedRectangle (right, 10.0f);

    if (advanced)
    {
        // Full-width MULTIBAND bar, then a full-width INPUT|OUTPUT block with a
        // central vertical divider in the Widen/Output style (0.6.8 #7).
        const int multiTop = 46 + kScopeRowH;
        const int ioTop     = multiTop + kMultiBarH;
        auto multiPanel = juce::Rectangle<int> (0, multiTop, getWidth(), kMultiBarH).toFloat().reduced (16.0f, 6.0f);
        auto ioPanel    = juce::Rectangle<int> (0, ioTop,     getWidth(), kIoH).toFloat().reduced (8.0f, 6.0f);
        g.setColour (colours::bgPanel.withAlpha (0.5f));
        g.fillRoundedRectangle (multiPanel, 10.0f);
        g.fillRoundedRectangle (ioPanel, 10.0f);

        g.setColour (colours::outline.withAlpha (0.6f));
        const float cx = ioPanel.getCentreX();
        g.drawLine (cx, ioPanel.getY() + 12.0f, cx, ioPanel.getBottom() - 12.0f, 1.0f);
    }
}

// ----------------------------------------------------------------------------
// The scope/meter block, separated out so the meter-reveal animation can re-run
// just this part per frame instead of the whole resized() (#6).
void AnamorphAudioProcessorEditor::layoutScopeArea()
{
    // The scope/meter block fills the scope+Widen row; INPUT/OUTPUT and the
    // Multiband bar are full-width tiers BELOW it (0.6.8 #7).
    auto leftArea = juce::Rectangle<int> (0, 46, getWidth() - 300, (advanced ? kScopeRowH : kHeight - 46));

    auto sa = leftArea.reduced (16);
    // 154, not 156: at 156 the meters-open scope was WIDTH-limited at 406 px
    // while the height cap is 408 px, so toggling Meters shaved 2 px off the
    // scope's (and the correlation meter's) top/bottom edges (#1).
    const int meterFull = 154; // fits 8 numbers + dB ruler; bars stay thin (#11/#17)
    const int reserve = juce::roundToInt (meterFull * meterAnim);
    const bool showMeter = reserve > 2;
    // Visibility is decided HERE, every animation frame: the moment the reserve
    // collapses past a couple of px the meter is hidden, so it can never be left
    // as a 1-2 px sliver on the left (the stuck strip, #2).
    if (levelMeter->isVisible() != showMeter)
        levelMeter->setVisible (showMeter);
    if (showMeter)
    {
        levelMeter->setBounds (sa.removeFromLeft (reserve));
        sa.removeFromLeft (juce::roundToInt (12.0f * meterAnim));
    }

    // Phase meter (right) + balance meter (bottom) hug the scope with an EQUAL gap,
    // and the whole scope+meter cluster is centred in the area -- so the
    // scope-to-phase and scope-to-balance gaps always match (0.6.7 #3).
    const int gap = 8, barW = 26;
    const int side = juce::jmin (sa.getWidth() - barW - gap, sa.getHeight() - barW - gap);
    auto cluster = sa.withSizeKeepingCentre (side + gap + barW, side + gap + barW);
    auto sq = juce::Rectangle<int> (cluster.getX(), cluster.getY(), side, side);
    scope->setBounds (sq);
    corrMeter->setBounds (sq.getRight() + gap, sq.getY(), barW, side);
    balanceMeter->setBounds (sq.getX(), sq.getBottom() + gap, side, barW);
}

void AnamorphAudioProcessorEditor::resized()
{
    applyWidenFonts(); // keep the Widen font sizes in lockstep with the (mode-dependent) layout (#F)
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
        auto s = getLocalBounds().withSizeKeepingCentre (360, 318);
        settingsBackdrop.panel = s;
        auto inner = s.reduced (24, 20);
        settingsTitle.setBounds (inner.removeFromTop (20));
        inner.removeFromTop (12);
        oversampleLabel.setBounds (inner.removeFromTop (16));
        inner.removeFromTop (4); // a little more air below the label (#7)
        oversampleBox.setBounds (inner.removeFromTop (25).reduced (0, 1));
        inner.removeFromTop (12);
        uiScaleLabel.setBounds (inner.removeFromTop (16)); // window scale (F4)
        inner.removeFromTop (4);
        uiScaleBox.setBounds (inner.removeFromTop (25).reduced (0, 1));
        inner.removeFromTop (12);
        persistLabel.setBounds (inner.removeFromTop (16));
        inner.removeFromTop (4);
        // Extend 8 px left so the 8 px track inset lands the bar's left edge on the
        // same line as the labels / Oversampling combo above (#4).
        { auto pb = inner.removeFromTop (24); scopePersistK.setBounds (pb.withLeft (pb.getX() - 8)); }
        inner.removeFromTop (12);
        // Nudge the toggles right so their pills line up with the labels above (#5).
        tooltipsToggle.setBounds (inner.removeFromTop (26).withTrimmedLeft (4));
        inner.removeFromTop (6);
        animToggle.setBounds (inner.removeFromTop (26).withTrimmedLeft (4)); // (F3)
    }

    // Save-preset overlay (F2)
    {
        savePresetBackdrop.setBounds (getLocalBounds());
        auto sp = getLocalBounds().withSizeKeepingCentre (340, 148);
        savePresetBackdrop.panel = sp;
        auto in = sp.reduced (24, 18);
        saveTitle.setBounds (in.removeFromTop (20));
        in.removeFromTop (10);
        saveNameEditor.setBounds (in.removeFromTop (28));
        in.removeFromTop (14);
        auto btns = in.removeFromTop (26);
        saveCancelButton.setBounds (btns.removeFromRight (72));
        btns.removeFromRight (8);
        saveOkButton.setBounds (btns.removeFromRight (72));
    }

    auto r = getLocalBounds();

    // Reserve the two full-width bottom tiers (advanced): the INPUT|OUTPUT block at
    // the very bottom, the MULTIBAND bar above it (0.6.8 #7).
    juce::Rectangle<int> ioBlock, multiBar;
    if (advanced)
    {
        ioBlock  = r.removeFromBottom (kIoH);
        multiBar = r.removeFromBottom (kMultiBarH);
    }

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

        // Preset browser between the title and the A/B group (F2): ‹ name ›.
        auto pr = juce::Rectangle<int> (318, 9, bar.getRight() - 12 - 318, 28);
        presetPrev.setBounds (pr.removeFromLeft (22));
        presetNext.setBounds (pr.removeFromRight (22));
        presetName.setBounds (pr.reduced (2, 0));
    }

    auto content = r;
    auto rightPanel = content.removeFromRight (300);

    // ---- Scope + meters (with the reveal animation) ----
    layoutScopeArea();

    auto placeKnob = [] (juce::Rectangle<int> area, juce::Slider& s, juce::Label& l)
    { l.setBounds (area.removeFromBottom (15)); s.setBounds (area.reduced (3, 1)); };

    // ---- Right column: WIDEN (both modes) ----
    {
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
            // ADVANCED: WIDEN alone now fills the right column; OUTPUT moved to the
            // bottom block. The four knobs sit a touch higher for a more balanced
            // column (0.6.9 #17).
            auto col = rightPanel.reduced (20, 14);
            { auto lr = col.removeFromTop (16); algoOptLabel.setBounds (lr.removeFromRight (94)); algorithmLabel.setBounds (lr); }
            col.removeFromTop (8);
            layoutAlgoRow (col.removeFromTop (30));
            col.removeFromTop (40);
            twoKnob (col.removeFromTop (140), driveK, driveL, amountK, amountL);
            col.removeFromTop (44);
            layoutCharacter (col.removeFromTop (140), widthK, widthL);
        }
    }

    // ---- MULTIBAND: full-width spectral band editor (slightly inset) ----
    if (advanced && ! multiBar.isEmpty())
    {
        auto m = multiBar.reduced (16, 6);
        auto head = m.removeFromTop (18).reduced (8, 0);
        multibandLabel.setBounds (head.removeFromLeft (head.getWidth() - 56));
        mbEnableToggle.setBounds (head.removeFromRight (52));
        m.removeFromTop (2);
        if (imager) imager->setBounds (m.reduced (2, 0));
    }

    // ---- INPUT | OUTPUT horizontal block (full width, vertical divider) ----
    if (advanced && ! ioBlock.isEmpty())
    {
        auto block = ioBlock.reduced (8, 6);
        const int half = block.getWidth() / 2;
        auto inputHalf  = block.removeFromLeft (half);
        auto outputHalf = block; // right half (divider drawn in paint)

        // INPUT (left): combos + five toggles, vertically centred so the lower half
        // no longer sits empty; Balance knob centred on the right (0.6.9 #19/#21).
        {
            auto a = inputHalf.reduced (14, 8);
            inputModuleLabel.setBounds (a.removeFromTop (15));
            a.removeFromTop (4);

            auto bal = a.removeFromRight (96);
            {
                auto blk = bal.withSizeKeepingCentre (bal.getWidth(), 112);
                balanceL.setBounds (blk.removeFromBottom (15));
                balanceK.setBounds (blk.reduced (12, 2));
            }
            a.removeFromRight (12);

            const int comboH  = 14 + 3 + 26;
            const int togH     = 50;
            const int gapCT    = 18;
            const int contentH = comboH + gapCT + togH;
            a.removeFromTop (juce::jmax (0, (a.getHeight() - contentH) / 2));

            auto combos = a.removeFromTop (comboH);
            {
                auto cm = combos.removeFromLeft (combos.getWidth() / 2 - 6);
                channelModeLabel.setBounds (cm.removeFromTop (14)); cm.removeFromTop (3);
                channelModeBox.setBounds (cm.removeFromTop (26));
                combos.removeFromLeft (12);
                soloLabel.setBounds (combos.removeFromTop (14)); combos.removeFromTop (3);
                soloBox.setBounds (combos.removeFromTop (26));
            }
            a.removeFromTop (gapCT);
            auto tog = a.removeFromTop (togH);
            const int tw = tog.getWidth() / 5;
            monoToggle.setBounds (tog.removeFromLeft (tw));
            swapToggle.setBounds (tog.removeFromLeft (tw));
            msToggle.setBounds   (tog.removeFromLeft (tw));
            polLToggle.setBounds (tog.removeFromLeft (tw));
            polRToggle.setBounds (tog);
        }

        // OUTPUT (right): Mix / Balance / Output knobs (sized like Input Balance, tight
        // and pushed left) + a Level-Match column wide enough to show the full label and
        // put Apply on the same line as the readout; Mono Maker is the bottom row, the
        // whole block centred with only a small knob-to-Mono gap (0.6.11 #8/#10/#11).
        {
            auto a = outputHalf.reduced (14, 8);
            outputModuleLabel.setBounds (a.removeFromTop (15));
            a.removeFromTop (2);

            const int knobH = 86, gap = 12, monoH = 22;
            a.removeFromTop (juce::jmax (0, (a.getHeight() - (knobH + gap + monoH)) / 2));

            auto knobRow = a.removeFromTop (knobH);
            // Centre [3 knobs | gap | Level-Match column] so the empty band between the
            // knobs and Level Match shrinks while the spacing stays balanced (#7).
            const int kw = 74, kg = 6, gapLM = 22, lmW = 134;
            const int clusterW = 3 * kw + 2 * kg + gapLM + lmW;
            knobRow.removeFromLeft (juce::jmax (0, (knobRow.getWidth() - clusterW) / 2));
            placeKnob (knobRow.removeFromLeft (kw), mixK, mixL);
            knobRow.removeFromLeft (kg);
            placeKnob (knobRow.removeFromLeft (kw), outBalanceK, outBalanceL);
            knobRow.removeFromLeft (kg);
            placeKnob (knobRow.removeFromLeft (kw), outputK, outputL);
            knobRow.removeFromLeft (gapLM);
            auto lmCol = knobRow.removeFromLeft (lmW);

            {
                auto c = lmCol;
                c.removeFromTop (juce::jmax (0, (c.getHeight() - 58) / 2));
                autoMatchToggle.setBounds (c.removeFromTop (22).reduced (0, 1)); // same height as Mono Maker
                c.removeFromTop (14); // a touch more air: Level Match up, Apply Gain + readout down (0.6.14 #7)
                auto ar = c.removeFromTop (22);
                applyGainButton.setBounds (ar.removeFromLeft (76).reduced (0, 1));
                ar.removeFromLeft (6);
                matchReadout.setBounds (ar);
            }

            a.removeFromTop (gap);
            auto mono = a.removeFromTop (monoH);
            monoMakerToggle.setBounds (mono.removeFromLeft (98).reduced (0, 1));
            mono.removeFromLeft (4);
            monoFreqK.setBounds (mono.reduced (0, 1));
        }
    }
}
