#include "LookAndFeel.h"

namespace anamorph::gui
{

AnamorphLookAndFeel::AnamorphLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::bg);
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::ComboBox::backgroundColourId, colours::bgRaised);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::outlineColourId, colours::outline);
    setColour (juce::PopupMenu::backgroundColourId, colours::bgPanel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.25f));
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::TextButton::buttonColourId, colours::bgRaised);
    setColour (juce::TextButton::textColourOnId, colours::bg);
    setColour (juce::TextButton::textColourOffId, colours::text);
}

void AnamorphLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                            float pos, float startAngle, float endAngle,
                                            juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const auto radius  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre  = bounds.getCentre();
    const auto angle   = startAngle + pos * (endAngle - startAngle);
    const float thick  = juce::jmax (3.0f, radius * 0.16f);

    // Interaction state (#10): hover the KNOB (not its value box) glows a little
    // more; pressing/holding -- or dragging the value number -- glows strongly.
    const bool hover  = s.isMouseOver (false);
    const bool active = s.isMouseButtonDown() || (bool) s.getProperties().getWithDefault ("dragging", false);

    // Track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - thick, radius - thick, 0.0f,
                         startAngle, endAngle, true);
    g.setColour (colours::outline);
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc: the softer palette BLUE->TEAL gradient (#1), wrapped in a MANY-
    // layered glow (faint+wide outside, bright+tight inside). Using many thin
    // layers makes the brightness falloff smooth instead of stepped/banded (#4).
    const juce::Colour arcLo (0xff5aa6ff); // soft blue (accent2)
    const juce::Colour arcHi (0xff35d0c0); // teal (accent)
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - thick, radius - thick, 0.0f,
                         startAngle, angle, true);
    // The arc glow keeps its blue->teal gradient but spreads even WIDER around the
    // arc on hover / press, with many thin layers so it stays smooth (#4).
    const float glowPeak   = active ? 0.40f : hover ? 0.24f : 0.13f;
    const float glowSpread = active ? 11.0f : hover ? 8.0f  : 3.4f;
    constexpr int nLayers = 12;
    for (int i = 0; i < nLayers; ++i)
    {
        const float t  = (float) i / (float) (nLayers - 1); // 0 outer .. 1 inner
        const float gw = thick + (1.0f - t) * glowSpread;   // widest outside
        const float a  = glowPeak * std::pow (t, 1.5f);     // smooth brighten toward the arc
        juce::ColourGradient gg (arcLo.withAlpha (a * 0.85f), centre.x - radius, centre.y,
                                 arcHi.withAlpha (a),         centre.x + radius, centre.y, false);
        g.setGradientFill (gg);
        g.strokePath (value, juce::PathStrokeType (gw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    juce::ColourGradient grad (active ? arcLo.brighter (0.12f) : arcLo, centre.x - radius, centre.y,
                               active ? arcHi.brighter (0.12f) : arcHi, centre.x + radius, centre.y, false);
    grad.addColour (0.5, arcLo.interpolatedWith (arcHi, 0.5f));
    g.setGradientFill (grad);
    g.strokePath (value, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Glassy knob face: a top-lit radial gradient over a dark base. Hover / press
    // lift the face only SLIGHTLY now -- the previous lift was too bright (#3).
    const float faceR  = radius - thick * 1.6f;
    const float lift   = active ? 0.12f : hover ? 0.06f : 0.0f;
    juce::ColourGradient face (colours::bgRaised.brighter (0.16f + lift), centre.x, centre.y - faceR * 0.7f,
                               colours::bgPanel.darker (0.25f), centre.x, centre.y + faceR, true);
    face.addColour (0.55, colours::bgRaised.brighter (lift));
    g.setGradientFill (face);
    g.fillEllipse (centre.x - faceR, centre.y - faceR, faceR * 2.0f, faceR * 2.0f);
    // Subtle glass rim: bright top-left arc + faint opposite glow (#16).
    glass::drawCircleEdge (g, centre.x, centre.y, faceR, hover || active ? 1.0f : 0.85f);
    g.setColour (colours::outline.brighter (hover || active ? 0.12f : 0.0f));
    g.drawEllipse (centre.x - faceR, centre.y - faceR, faceR * 2.0f, faceR * 2.0f, 1.0f);

    // Pointer: the glow is a REAL feathered halo AROUND the pointer (a blurred
    // drop-shadow of the pointer shape), not a thick white stroke band on top of it
    // (#2/#8). The solid pointer sits on the glow.
    juce::Path pointer;
    const float pl = faceR * 0.92f, pr = thick * 0.35f;
    pointer.addRoundedRectangle (-pr, -pl, pr * 2.0f, pl * 0.6f, pr);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    if (active || hover)
    {
        // A WIDE, soft feather (a big-radius blurred halo) rather than a tight bright
        // band -- so it reads like the blue/teal arc glow, not a hard white outline
        // (#5). Two radii: a broad soft wash + a closer one.
        const float a = active ? 0.42f : 0.22f;
        juce::DropShadow (juce::Colours::white.withAlpha (a),        active ? 13 : 9, {}).drawForPath (g, pointer);
        juce::DropShadow (juce::Colours::white.withAlpha (a * 0.7f), active ? 6  : 4, {}).drawForPath (g, pointer);
    }
    g.setColour (active ? juce::Colours::white : (hover ? colours::text.brighter (0.2f) : colours::text));
    g.fillPath (pointer);
}

void AnamorphLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                            float pos, float, float,
                                            juce::Slider::SliderStyle style, juce::Slider& s)
{
    const bool horizontal = (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar);
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);

    // Recessed track with an inner gradient (#12: premium, not a flat line).
    const float trackThick = 6.0f;
    juce::Rectangle<float> track = horizontal
        ? juce::Rectangle<float> (bounds.getX(), bounds.getCentreY() - trackThick * 0.5f, bounds.getWidth(), trackThick)
        : juce::Rectangle<float> (bounds.getCentreX() - trackThick * 0.5f, bounds.getY(), trackThick, bounds.getHeight());

    juce::ColourGradient tg (colours::bg.darker (0.25f), track.getX(), track.getY(),
                             colours::bgRaised, track.getX(), track.getBottom(), false);
    g.setGradientFill (tg);
    g.fillRoundedRectangle (track, trackThick * 0.5f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (track.reduced (0.5f), trackThick * 0.5f, 1.0f);

    // The thumb travels on `pos` directly (1:1 with the cursor); the inset that
    // keeps it on the track is done in getSliderLayout, so cursor / value / thumb
    // all stay in sync (#5).
    const float r = 8.0f;
    juce::Rectangle<float> fill = horizontal
        ? track.withWidth (juce::jmax (0.0f, pos - bounds.getX()))
        : track.withTop (pos).withBottom (bounds.getBottom());

    // Filled portion: the softer palette blue->teal gradient (#1) with a MANY-
    // layered glow so the halo's brightness falls off smoothly instead of in
    // visible steps when zoomed in (#4).
    const juce::Colour fillLo (0xff5aa6ff), fillHi (0xff35d0c0);
    const bool act = s.isMouseOverOrDragging() || (bool) s.getProperties().getWithDefault ("dragging", false);
    const float glowPeak   = act ? 0.34f : 0.16f;
    const float glowSpread = act ? 4.6f  : 2.8f;
    constexpr int nLayers = 9;
    for (int i = 0; i < nLayers; ++i)
    {
        const float t  = (float) i / (float) (nLayers - 1); // 0 outer .. 1 inner
        const float ex = (1.0f - t) * glowSpread;
        const float a  = glowPeak * std::pow (t, 1.5f);     // smooth brighten inward
        juce::ColourGradient gg (fillLo.withAlpha (a), fill.getX(), fill.getY(),
                                 fillHi.withAlpha (a),
                                 horizontal ? fill.getRight() : fill.getX(),
                                 horizontal ? fill.getY() : fill.getBottom(), false);
        g.setGradientFill (gg);
        g.fillRoundedRectangle (fill.expanded (ex), (trackThick + ex * 2.0f) * 0.5f);
    }
    juce::ColourGradient fg (fillLo, fill.getX(), fill.getY(),
                             fillHi, horizontal ? fill.getRight() : fill.getX(),
                             horizontal ? fill.getY() : fill.getBottom(), false);
    g.setGradientFill (fg);
    g.fillRoundedRectangle (fill, trackThick * 0.5f);

    // Glassy thumb: a neutral gray-white rim normally; on hover/drag it gets a
    // REAL feathered cyan glow (a blurred drop-shadow halo around the circle, not a
    // hard flat ring) (#8).
    const float cx = horizontal ? pos : bounds.getCentreX();
    const float cy = horizontal ? bounds.getCentreY() : pos;
    juce::Path thumbPath; thumbPath.addEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    if (act)
    {
        juce::DropShadow (fillHi.withAlpha (0.65f), 9, {}).drawForPath (g, thumbPath);
        juce::DropShadow (fillHi.withAlpha (0.40f), 4, {}).drawForPath (g, thumbPath);
    }
    juce::ColourGradient kg (colours::bgRaised.brighter (act ? 0.45f : 0.30f), cx, cy - r,
                             colours::bgPanel.darker (0.18f),     cx, cy + r, false);
    g.setGradientFill (kg);
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour (act ? fillHi : juce::Colour (0xffb8c2cf)); // gray-white rim, cyan on hover
    g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.4f);
    // Glass rim ON TOP, clearly visible: bright top-left arc + faint opposite (#16).
    glass::drawCircleEdge (g, cx, cy, r, 1.5f);
}

juce::Slider::SliderLayout AnamorphLookAndFeel::getSliderLayout (juce::Slider& s)
{
    auto layout = juce::LookAndFeel_V4::getSliderLayout (s);
    // Inset the interactive track by the thumb radius: the thumb then maps 1:1 to
    // the cursor within the track and clamps a radius in from each end, so it never
    // hangs off the edge (#4) and never lags the cursor (#5).
    const int rad = 8;
    if (s.isHorizontal())    layout.sliderBounds = layout.sliderBounds.reduced (rad, 0);
    else if (s.isVertical()) layout.sliderBounds = layout.sliderBounds.reduced (0, rad);
    return layout;
}

void AnamorphLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                            bool highlighted, bool /*down*/)
{
    auto bounds = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    // Hover brightens only the switch + text, never the whole block (#4).
    const float hi = highlighted ? 0.18f : 0.0f;

    // --- Level-meter glyph instead of the word "Meters" (#7) ---
    if (b.getComponentID() == "metersicon")
    {
        const auto col = (on ? colours::accent : colours::textDim).brighter (hi);
        const float barW = 5.0f, gap = 4.0f;
        const float totalW = barW * 2.0f + gap;
        const float barH = juce::jmin (bounds.getHeight() - 6.0f, 16.0f);
        const float x0 = bounds.getCentreX() - totalW * 0.5f;
        const float y0 = bounds.getCentreY() - barH * 0.5f;
        if (on) { g.setColour (col.withAlpha (0.18f)); g.fillRoundedRectangle (bounds.reduced (3.0f), 4.0f); }
        for (int k = 0; k < 2; ++k)
        {
            auto bar = juce::Rectangle<float> (x0 + k * (barW + gap), y0, barW, barH);
            g.setColour (col.withAlpha (0.55f));
            g.drawRoundedRectangle (bar, 1.5f, 1.0f);                 // hollow top
            const float ratio = (k == 0) ? 0.55f : 0.80f;            // unequal fills
            g.setColour (col);
            g.fillRoundedRectangle (bar.withTop (bar.getBottom() - barH * ratio).reduced (0.6f), 1.2f);
        }
        return;
    }

    // --- Compact vertical toggle: pill on top, label centred below, for tight
    //     module rows where a right-side label would clip (#11 / #14) ---
    if (b.getComponentID() == "vtoggle")
    {
        const juce::Colour onCol = colours::accent.brighter (hi);
        const float ph = 15.0f, pw = ph * 1.7f;
        auto pill = juce::Rectangle<float> (bounds.getCentreX() - pw * 0.5f, bounds.getY() + 2.0f, pw, ph);
        if (on) { g.setColour (onCol.withAlpha (0.22f)); g.fillRoundedRectangle (pill.expanded (2.0f), (ph + 4.0f) * 0.5f); }
        const auto pillBase = on ? onCol : colours::bgRaised.brighter (hi);
        juce::ColourGradient pg (pillBase.brighter (on ? 0.10f : 0.06f), pill.getX(), pill.getY(),
                                 pillBase.darker (on ? 0.12f : 0.10f),   pill.getX(), pill.getBottom(), false);
        g.setGradientFill (pg);
        g.fillRoundedRectangle (pill, ph * 0.5f);
        g.setColour (on ? onCol : colours::outline.brighter (hi));
        g.drawRoundedRectangle (pill, ph * 0.5f, 1.0f);
        const float knob = ph - 4.0f;
        const float kx = on ? pill.getRight() - knob - 2.0f : pill.getX() + 2.0f;
        g.setColour (on ? colours::bg : colours::text);
        g.fillEllipse (kx, pill.getCentreY() - knob * 0.5f, knob, knob);

        // Labels render with a shared 11 px baseline so Mono/Swap/M/S all line up
        // (#8). The polarity toggles are the exception: their "ø" is drawn LARGER
        // than the trailing letter but on the SAME baseline, so it reads bold
        // without dragging the letter's baseline around (#5).
        const juce::Colour tc = (on || highlighted ? colours::text : colours::textDim);
        g.setColour (tc);
        const auto labelArea = bounds.withTop (pill.getBottom() + 1.0f);
        const juce::juce_wchar phi = (juce::juce_wchar) 0x00F8;
        const juce::String txt = b.getButtonText();
        if (txt.startsWithChar (phi))
        {
            const juce::Font fBig (juce::FontOptions (15.0f));
            const juce::Font fSm  (juce::FontOptions (11.0f));
            juce::GlyphArrangement ga;
            ga.addLineOfText (fBig, juce::String::charToString (phi), 0.0f, 0.0f);
            const float headW = ga.getBoundingBox (0, -1, true).getRight();
            ga.addLineOfText (fSm, txt.substring (1), headW, 0.0f); // shared baseline at y = 0
            const auto bb = ga.getBoundingBox (0, -1, true);
            // Centre the group horizontally; place the shared baseline so the small
            // letter is vertically centred just like the other toggles' labels.
            const float tx = labelArea.getCentreX() - bb.getCentreX();
            const float by = labelArea.getCentreY() + (fSm.getAscent() - fSm.getDescent()) * 0.5f;
            ga.draw (g, juce::AffineTransform::translation (tx, by));
        }
        else
        {
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawFittedText (txt, labelArea.toNearestInt(), juce::Justification::centred, 1, 0.85f);
        }
        return;
    }

    // Leave a uniform inset so the outer glow can never be clipped at the left /
    // top / bottom edges (feedback #16). The pill is a fixed, compact size.
    const float pad = 3.0f;
    const float h   = juce::jlimit (12.0f, 18.0f, bounds.getHeight() - pad * 2.0f);
    const float pw  = h * 1.8f;
    auto pill = juce::Rectangle<float> (bounds.getX() + pad,
                                        bounds.getCentreY() - h * 0.5f, pw, h);

    // Bypass uses a controlled red when engaged so it reads as "off/abnormal".
    const juce::Colour onCol = ((b.getComponentID() == "bypass") ? juce::Colour (0xffd0584e)
                                                                  : colours::accent).brighter (hi);
    if (on) // soft outer glow (fits inside the pad)
    {
        g.setColour (onCol.withAlpha (0.22f));
        g.fillRoundedRectangle (pill.expanded (2.0f), (h + 4.0f) * 0.5f);
    }
    const auto pillBase = on ? onCol : colours::bgRaised.brighter (hi);
    juce::ColourGradient pg (pillBase.brighter (on ? 0.10f : 0.06f), pill.getX(), pill.getY(),
                             pillBase.darker (on ? 0.12f : 0.10f),   pill.getX(), pill.getBottom(), false);
    g.setGradientFill (pg);
    g.fillRoundedRectangle (pill, h * 0.5f);
    g.setColour (on ? onCol : colours::outline.brighter (hi));
    g.drawRoundedRectangle (pill, h * 0.5f, 1.0f);

    const float knob = h - 4.0f;
    const float kx = on ? pill.getRight() - knob - 2.0f : pill.getX() + 2.0f;
    g.setColour (on ? colours::bg : colours::text);
    g.fillEllipse (kx, pill.getCentreY() - knob * 0.5f, knob, knob);

    // Label: fit-to-width so nothing is ever truncated to an ellipsis (#9).
    const float tx = pill.getRight() + 7.0f;
    const float tw = bounds.getRight() - tx - 1.0f;
    if (tw > 4.0f)
    {
        g.setColour (highlighted ? colours::text : colours::textDim);
        g.setFont (juce::Font (juce::FontOptions (12.5f)));
        g.drawFittedText (b.getButtonText(), (int) tx, (int) bounds.getY(),
                          (int) tw, (int) bounds.getHeight(),
                          juce::Justification::centredLeft, 1, 0.85f);
    }
}

void AnamorphLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                                const juce::Colour&, bool highlighted, bool down)
{
    if (b.getComponentID() == "ghost") return; // invisible hit-area (e.g. title)

    auto bounds = b.getLocalBounds().toFloat().reduced (1.0f);
    const float radius = juce::jmin (9.0f, bounds.getHeight() * 0.5f); // rounder (#13)
    const bool on = b.getToggleState();
    const auto base = down ? colours::bgRaised.brighter (0.12f)
                           : (highlighted ? colours::bgRaised.brighter (0.06f) : colours::bgRaised);
    if (on)
    {
        g.setColour (colours::accent.withAlpha (0.85f));
        g.fillRoundedRectangle (bounds, radius);
    }
    else
    {
        juce::ColourGradient gr (base.brighter (0.05f), bounds.getX(), bounds.getY(),
                                 base.darker (0.10f), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (gr);
        g.fillRoundedRectangle (bounds, radius);
    }
    g.setColour (on ? colours::accent : colours::outline);
    g.drawRoundedRectangle (bounds, radius, 1.0f);
}

juce::Font AnamorphLookAndFeel::getTextButtonFont (juce::TextButton& b, int buttonHeight)
{
    if (b.getComponentID() == "apply") return juce::Font (juce::FontOptions (12.0f)); // Apply, a touch smaller (#21)
    if (b.getComponentID() == "icon")  return juce::Font (juce::FontOptions (21.0f)); // bigger glyph (#7)
    return juce::Font (juce::FontOptions ((float) juce::jmin (13, juce::jmax (10, buttonHeight - 12))));
}

juce::Font AnamorphLookAndFeel::getComboBoxFont (juce::ComboBox&)  { return juce::Font (juce::FontOptions (13.5f)); }
juce::Font AnamorphLookAndFeel::getPopupMenuFont()                 { return juce::Font (juce::FontOptions (13.5f)); }

void AnamorphLookAndFeel::getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                                     int, int& idealWidth, int& idealHeight)
{
    if (isSeparator) { idealWidth = 60; idealHeight = 8; return; }
    auto f = getPopupMenuFont();
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, text, 0.0f, 0.0f);
    idealWidth  = (int) std::ceil (ga.getBoundingBox (0, -1, true).getWidth()) + 30;
    idealHeight = 23; // uniform across every combo regardless of its on-screen height (#3)
}

// Undo/Redo glyphs: render them larger AND rotated 180 degrees, which the user
// found more comfortable (feedback #7). All other buttons use the default text.
void AnamorphLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b,
                                          bool /*highlighted*/, bool /*down*/)
{
    if (b.getComponentID() == "icon")
    {
        auto area = b.getLocalBounds().toFloat();
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (colours::text.withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.4f));
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::pi,
                                                         area.getCentreX(), area.getCentreY()));
        // Nudge the (rotated) glyph so it reads as optically centred (#8): shifting
        // the pre-rotation box UP moves the visible glyph DOWN.
        g.drawText (b.getButtonText(), area.translated (0.0f, -2.0f), juce::Justification::centred, false);
        return;
    }
    juce::LookAndFeel_V4::drawButtonText (g, b, false, false);
}

// Pop-up rows: a clean, FLAT modern list -- the highlighted row is a single
// solid accent tint with no gradient, sheen or bevel (the previous glassy version
// read as dated "Vista aero", #3).
void AnamorphLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                             bool isSeparator, bool /*isActive*/, bool isHighlighted,
                                             bool isTicked, bool hasSubMenu, const juce::String& text,
                                             const juce::String& shortcutKeyText,
                                             const juce::Drawable* /*icon*/, const juce::Colour* /*textColour*/)
{
    if (isSeparator)
    {
        auto r = area.toFloat().reduced (8.0f, 0.0f);
        g.setColour (colours::outline.withAlpha (0.7f));
        g.fillRect (r.withHeight (1.0f).withY (r.getCentreY()));
        return;
    }

    auto r = area.toFloat();
    if (isHighlighted)
    {
        // One flat accent tint, lightly rounded -- clean and modern (#3).
        g.setColour (colours::accent.withAlpha (0.18f));
        g.fillRoundedRectangle (r.reduced (3.0f, 1.0f), 4.0f);
    }

    g.setColour (isHighlighted ? colours::text : colours::text.withMultipliedAlpha (0.88f));
    g.setFont (getPopupMenuFont());

    auto textArea = r.reduced (12.0f, 0.0f);
    if (isTicked)
    {
        auto tick = textArea.removeFromLeft (14.0f);
        g.setColour (colours::accent);
        g.fillEllipse (tick.getCentreX() - 2.0f, tick.getCentreY() - 2.0f, 4.0f, 4.0f);
        g.setColour (isHighlighted ? colours::text : colours::text.withMultipliedAlpha (0.88f));
    }
    else
    {
        textArea.removeFromLeft (14.0f);
    }

    g.drawText (text, textArea, juce::Justification::centredLeft);

    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour (colours::textDim);
        g.setFont (getPopupMenuFont().withHeight (11.0f));
        g.drawText (shortcutKeyText, textArea, juce::Justification::centredRight);
    }

    if (hasSubMenu)
    {
        const float h = (float) area.getHeight();
        juce::Path p;
        const float x = r.getRight() - 12.0f, cy = r.getCentreY();
        p.startNewSubPath (x, cy - h * 0.12f);
        p.lineTo (x + h * 0.12f, cy);
        p.lineTo (x, cy + h * 0.12f);
        g.setColour (colours::textDim);
        g.strokePath (p, juce::PathStrokeType (1.4f));
    }
}

void AnamorphLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    // Square, fully-opaque list: rounded corners on an opaque menu window leave
    // bright corner/edge artefacts on some hosts, so we keep the popup square and
    // clean (feedback #3). The flat fill + hairline still reads as premium.
    g.fillAll (colours::bgPanel);
    g.setColour (colours::outline);
    g.drawRect (0, 0, width, height, 1);
}

void AnamorphLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool down,
                                        int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
    const float radius = juce::jmin (5.0f, bounds.getHeight() * 0.5f); // a touch squarer (#8)
    // Hover / open feedback (#10): brighten the WHOLE body, accent the outline.
    // The editor's timer publishes an authoritative single-combo hover flag (#20),
    // falling back to a live cursor test before the first timer tick.
    const bool hover = box.getProperties().contains ("hov")
                     ? (bool) box.getProperties()["hov"]
                     : box.getLocalBounds().contains (box.getMouseXYRelative());
    const bool open  = down || box.isPopupActive();
    const float lift = open ? 0.18f : hover ? 0.12f : 0.05f;
    juce::ColourGradient gr (colours::bgRaised.brighter (lift), bounds.getX(), bounds.getY(),
                             colours::bgRaised.darker (0.10f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (gr);
    g.fillRoundedRectangle (bounds, radius);
    if (open)
    {
        // Open state: a faint accent bloom just inside the rim plus a thin
        // gradient border -- the box reads as lit by its list, instead of the
        // previous flat thick ring, which looked dated (#2).
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (colours::accent.withAlpha (0.05f * (float) i));
            g.drawRoundedRectangle (bounds.reduced ((float) (4 - i)),
                                    juce::jmax (1.5f, radius - (float) (4 - i)), 1.8f);
        }
        juce::ColourGradient og (colours::accent.brighter (0.22f), bounds.getX(), bounds.getY(),
                                 colours::accent.withAlpha (0.55f), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (og);
        g.drawRoundedRectangle (bounds, radius, 1.0f);
    }
    else
    {
        g.setColour (colours::outline);
        g.drawRoundedRectangle (bounds, radius, 1.0f);
    }

    juce::Path arrow;
    const float cx = (float) w - 14.0f, cy = (float) h * 0.5f;
    arrow.startNewSubPath (cx - 4, cy - 2);
    arrow.lineTo (cx, cy + 3);
    arrow.lineTo (cx + 4, cy - 2);
    g.setColour (colours::textDim);
    g.strokePath (arrow, juce::PathStrokeType (1.6f));
}

void AnamorphLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    // Start the text a little further from the left edge so it isn't cramped
    // against the border (#13).
    label.setBounds (9, 1, box.getWidth() - 9 - 20, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font AnamorphLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::Font (juce::FontOptions (13.0f));
}

// Default LookAndFeel::drawLabel routes through getLabelFont(), which forced a
// single size and silently overrode every per-label setFont() -- which is why the
// larger Simple-mode Widen text never appeared. Honour the label's own font here.
void AnamorphLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.fillAll (label.findColour (juce::Label::backgroundColourId));

    if (! label.isBeingEdited())
    {
        const float alpha = label.isEnabled() ? 1.0f : 0.5f;
        const juce::Font font (label.getFont()); // <- explicit per-label font wins
        g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
        g.setFont (font);

        auto area = label.getBorderSize().subtractedFrom (label.getLocalBounds());
        g.drawFittedText (label.getText(), area, label.getJustificationType(),
                          juce::jmax (1, (int) ((float) area.getHeight() / font.getHeight())),
                          label.getMinimumHorizontalScale());
    }

    g.setColour (label.findColour (juce::Label::outlineColourId));
    g.drawRect (label.getLocalBounds());
}

// A slider value box you can DRAG (up/down) to change the value (#28), while a
// double-click opens an editor pre-filled with the RAW number (no %, dB, Hz),
// which still parses unit/k-suffixed input (#36 / #37 / #29).
namespace
{
    static juce::Slider* rotaryParent (juce::Component* c) noexcept
    {
        auto* s = dynamic_cast<juce::Slider*> (c);
        if (s == nullptr) return nullptr;
        const auto st = s->getSliderStyle();
        const bool rotary = (st == juce::Slider::Rotary || st == juce::Slider::RotaryHorizontalDrag
                          || st == juce::Slider::RotaryVerticalDrag || st == juce::Slider::RotaryHorizontalVerticalDrag);
        return rotary ? s : nullptr;
    }

    static juce::String rawEditText (juce::Slider& s)
    {
        // Mirror the displayed value exactly (what's outside is what you edit),
        // just without the unit (#3/#4/#5). Derive from the display string.
        const juce::String unit = s.getProperties().getWithDefault ("unit", "").toString();
        juce::String disp = s.getTextFromValue (s.getValue()).trim();

        if (unit == "bal")
        {
            if (disp.startsWithIgnoreCase ("C")) return "0";
            // Left (L) or Mid (M) read as the negative side; Right (R) / Side (S) positive.
            const bool left = disp.startsWithIgnoreCase ("L") || disp.startsWithIgnoreCase ("M");
            const juce::String num = disp.retainCharacters ("0123456789.");
            return (left ? "-" : "") + num;
        }
        if (disp.endsWithIgnoreCase ("kHz"))                            // "5.55 kHz" -> "5.55k"
            return disp.dropLastCharacters (3).trim() + "k";
        const int sp = disp.lastIndexOfChar (' ');                      // strip a trailing unit word
        if (sp > 0 && ! disp.substring (sp + 1).containsAnyOf ("0123456789"))
            return disp.substring (0, sp).trim();
        return disp;
    }

    struct ValueBox : public juce::Label
    {
        double downProp = 0.0;

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (auto* s = rotaryParent (getParentComponent()); s != nullptr && e.getNumberOfClicks() < 2 && ! isBeingEdited())
            {
                downProp = s->valueToProportionOfLength (s->getValue());
                s->getProperties().set ("dragging", true); // knob shows press feedback (#10)
                s->repaint();
            }
            juce::Label::mouseDown (e); // double-click still opens the editor
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (auto* s = dynamic_cast<juce::Slider*> (getParentComponent()))
            {
                s->getProperties().set ("dragging", false);
                s->repaint();
            }
            juce::Label::mouseUp (e);
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            // Directly map vertical drag to the value (respecting any skew). This
            // is robust regardless of event routing -- the previous forwarding
            // approach didn't take (feedback #28).
            if (auto* s = rotaryParent (getParentComponent()); s != nullptr && ! isBeingEdited())
            {
                const double prop = juce::jlimit (0.0, 1.0, downProp + (-e.getDistanceFromDragStartY()) / 180.0);
                s->setValue (s->proportionOfLengthToValue (prop), juce::sendNotificationSync);
            }
            else
                juce::Label::mouseDrag (e);
        }
        void editorShown (juce::TextEditor* ed) override
        {
            if (auto* s = dynamic_cast<juce::Slider*> (getParentComponent()))
            {
                ed->setText (rawEditText (*s), false); // raw number only (#36)
                ed->selectAll();
            }
        }
    };
}

juce::Label* AnamorphLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = new ValueBox();
    l->setJustificationType (juce::Justification::centred);
    l->setFont (juce::Font (juce::FontOptions (13.0f))); // explicit default; Simple mode enlarges it (#A)
    l->setKeyboardType (juce::TextInputTarget::decimalKeyboard);
    l->setColour (juce::Label::textColourId,            s.findColour (juce::Slider::textBoxTextColourId));
    l->setColour (juce::Label::backgroundColourId,      juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId,         juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundWhenEditingColourId, colours::bg);
    l->setColour (juce::Label::textWhenEditingColourId, colours::text);
    l->setColour (juce::TextEditor::highlightColourId,  colours::accent.withAlpha (0.30f));
    l->setColour (juce::TextEditor::textColourId,       colours::text);
    l->setEditable (false, s.isTextBoxEditable(), false); // double-click to type
    return l;
}

// ---- Tooltips: rounded dark capsule, accent hairline, soft text (#20) ----
static juce::TextLayout layoutTooltip (const juce::String& text, float maxWidth)
{
    juce::AttributedString s;
    s.append (text, juce::Font (juce::FontOptions (12.5f)), colours::text);
    s.setJustification (juce::Justification::centredLeft);
    juce::TextLayout tl;
    tl.createLayout (s, maxWidth);
    return tl;
}

juce::Rectangle<int> AnamorphLookAndFeel::getTooltipBounds (const juce::String& tip, juce::Point<int> pos,
                                                            juce::Rectangle<int> parentArea)
{
    auto tl = layoutTooltip (tip, 260.0f);
    const int w = (int) std::ceil (tl.getWidth())  + 20;
    const int h = (int) std::ceil (tl.getHeight()) + 14;
    return juce::Rectangle<int> (pos.x > parentArea.getCentreX() ? pos.x - (w + 12) : pos.x + 14,
                                 pos.y > parentArea.getCentreY() ? pos.y - (h + 8)  : pos.y + 16,
                                 w, h).constrainedWithin (parentArea);
}

void AnamorphLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    auto b = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
    // Subtle glass capsule -- the previous version was too white / contrasty (#7).
    glass::fillPanel (g, b, 6.0f, colours::bgRaised, 0.7f);
    g.setColour (colours::accent.withAlpha (0.28f)); // faint accent hairline
    g.drawRoundedRectangle (b, 6.0f, 1.0f);
    layoutTooltip (text, (float) w - 20.0f).draw (g, b.reduced (10.0f, 7.0f));
}

// ---- Glass surfaces (#7/#17) -----------------------------------------------
namespace glass
{
    void drawEdges (juce::Graphics& g, juce::Rectangle<float> bounds, float radius, float strength)
    {
        // A restrained, 0.5.4-style micro-glow: a dim hairline plus a soft top-left
        // highlight that fades gently toward the centre, and a fainter bottom-right
        // one. Deliberately NOT a bright, thick rim -- that read as grey and stiff
        // (#1). A hint of corner detail remains.
        auto r = bounds.reduced (0.5f);
        const auto c = r.getCentre();

        g.setColour (colours::outline);
        g.drawRoundedRectangle (r, radius, 1.0f);

        {
            juce::ColourGradient gr (juce::Colours::white.withAlpha (0.20f * strength), r.getX(), r.getY(),
                                     juce::Colours::white.withAlpha (0.0f), c.x, c.y, false);
            g.setGradientFill (gr);
            g.drawRoundedRectangle (r, radius, 1.3f);
        }
        {
            juce::ColourGradient gr (juce::Colours::white.withAlpha (0.09f * strength), r.getRight(), r.getBottom(),
                                     juce::Colours::white.withAlpha (0.0f), c.x, c.y, false);
            g.setGradientFill (gr);
            g.drawRoundedRectangle (r, radius, 1.1f);
        }
    }

    void fillPanel (juce::Graphics& g, juce::Rectangle<float> bounds, float radius,
                    juce::Colour base, float strength)
    {
        // Gentle VERTICAL depth (0.5.3 feel): a little brighter at the top, darker
        // toward the bottom -- no bright grey diagonal wash (#1/#7).
        juce::ColourGradient gr (base.brighter (0.04f * strength), bounds.getCentreX(), bounds.getY(),
                                 base.darker  (0.20f * strength), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (gr);
        g.fillRoundedRectangle (bounds, radius);
        drawEdges (g, bounds, radius, strength);
    }

    void drawCircleEdge (juce::Graphics& g, float cx, float cy, float radius, float strength)
    {
        auto box = juce::Rectangle<float> (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f).reduced (0.4f);
        // One corner (top-left) catches a clear glass highlight; the opposite edge
        // gets a faint glow -- subtle, like the panels (#16).
        {
            juce::ColourGradient gr (juce::Colours::white.withAlpha (0.42f * strength), box.getX(), box.getY(),
                                     juce::Colours::white.withAlpha (0.0f), cx, cy, false);
            g.setGradientFill (gr);
            g.drawEllipse (box, 1.4f);
        }
        {
            juce::ColourGradient gr (juce::Colours::white.withAlpha (0.16f * strength), box.getRight(), box.getBottom(),
                                     juce::Colours::white.withAlpha (0.0f), cx, cy, false);
            g.setGradientFill (gr);
            g.drawEllipse (box, 1.1f);
        }
    }
} // namespace glass

} // namespace anamorph::gui
