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
    // Draw at the EASED visual position when the micro-anim driver is publishing
    // one (preset / A-B sweep, #5); during a hand drag use the live position so
    // the pointer tracks 1:1 with no lag.
    // A reset sweep (double-click / alt-click) draws at the eased vpos even while the
    // button is still held; a hand drag draws at the live pos so it tracks 1:1.
    const bool resetSweep = (bool) s.getProperties().getWithDefault ("resetSweep", false);
    const bool dragging = ! resetSweep
                       && (s.isMouseButtonDown()
                           || (bool) s.getProperties().getWithDefault ("dragging", false));
    if (! dragging)
        if (const auto* v = s.getProperties().getVarPointer ("vpos"))
            pos = juce::jlimit (0.0f, 1.0f, (float) (double) *v);
    const auto angle   = startAngle + pos * (endAngle - startAngle);
    const float thick  = juce::jmax (3.0f, radius * 0.16f);

    // Interaction state (#10), now as EASED 0..1 levels from the micro-anim
    // driver (F3): hA ramps with hover, aA with press / value-number drag; hi is
    // "interacting at all". Falls back to the old binary feel when not animated.
    const bool hover  = s.isMouseOver (false);
    const bool active = s.isMouseButtonDown() || (bool) s.getProperties().getWithDefault ("dragging", false);
    const float hA = animOr (s, "hovA", hover);
    const float aA = animOr (s, "actA", active);
    const float hi = juce::jmax (hA, aA);

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
    // arc on hover / press, with many thin layers so it stays smooth (#4). The
    // eased levels make the spread glide instead of stepping (F3).
    const float glowPeak   = 0.13f + 0.11f * hi + 0.16f * aA; // idle .13 / hover .24 / press .40
    const float glowSpread = 3.4f  + 4.6f  * hi + 3.0f  * aA; // idle 3.4 / hover 8 / press 11
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
    juce::ColourGradient grad (arcLo.brighter (0.12f * aA), centre.x - radius, centre.y,
                               arcHi.brighter (0.12f * aA), centre.x + radius, centre.y, false);
    grad.addColour (0.5, arcLo.interpolatedWith (arcHi, 0.5f));
    g.setGradientFill (grad);
    g.strokePath (value, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Glassy knob face: a top-lit radial gradient over a dark base. Hover / press
    // lift the face only SLIGHTLY now -- the previous lift was too bright (#3).
    const float faceR  = radius - thick * 1.6f;
    const float lift   = 0.06f * hi + 0.06f * aA;
    juce::ColourGradient face (colours::bgRaised.brighter (0.16f + lift), centre.x, centre.y - faceR * 0.7f,
                               colours::bgPanel.darker (0.25f), centre.x, centre.y + faceR, true);
    face.addColour (0.55, colours::bgRaised.brighter (lift));
    g.setGradientFill (face);
    g.fillEllipse (centre.x - faceR, centre.y - faceR, faceR * 2.0f, faceR * 2.0f);
    // Subtle glass rim: bright top-left arc + faint opposite glow (#16).
    glass::drawCircleEdge (g, centre.x, centre.y, faceR, 0.85f + 0.15f * hi);
    g.setColour (colours::outline.brighter (0.12f * hi));
    g.drawEllipse (centre.x - faceR, centre.y - faceR, faceR * 2.0f, faceR * 2.0f, 1.0f);

    // Pointer: the glow is a REAL feathered halo AROUND the pointer (a blurred
    // drop-shadow of the pointer shape), not a thick white stroke band on top of it
    // (#2/#8). The solid pointer sits on the glow.
    juce::Path pointer;
    const float pl = faceR * 0.92f, pr = thick * 0.35f;
    pointer.addRoundedRectangle (-pr, -pl, pr * 2.0f, pl * 0.6f, pr);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    // A WIDE, soft feather (a big-radius blurred halo) rather than a tight bright
    // band -- so it reads like the blue/teal arc glow, not a hard white outline
    // (#5). Two radii: a broad soft wash + a closer one. The eased levels fade it
    // in/out and widen it smoothly (F3).
    const float pa = (0.22f + 0.20f * aA) * hi; // hover .22 / press .42, faded by hi
    if (pa > 0.02f)
    {
        const int r1 = 9 + juce::roundToInt (4.0f * aA); // hover 9 / press 13
        juce::DropShadow (juce::Colours::white.withAlpha (pa),        r1,     {}).drawForPath (g, pointer);
        juce::DropShadow (juce::Colours::white.withAlpha (pa * 0.7f), r1 / 2, {}).drawForPath (g, pointer);
    }
    g.setColour (colours::text.brighter (0.2f * hi).interpolatedWith (juce::Colours::white, aA));
    g.fillPath (pointer);
}

void AnamorphLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                            float pos, float, float,
                                            juce::Slider::SliderStyle style, juce::Slider& s)
{
    const bool horizontal = (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar);
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);

    // Hover and press are DISTINCT eased levels, like the knob arc: hover glows,
    // press glows MORE (#5). hi = "lit at all", aA = the extra press amount.
    const bool hovB = s.isMouseOver (false);
    const bool interacting = s.isMouseButtonDown()
                          || (bool) s.getProperties().getWithDefault ("dragging", false);
    const float hA = animOr (s, "hovA", hovB);
    const float aA = animOr (s, "actA", interacting);
    const float hi = juce::jmax (hA, aA);

    // Recessed track. The un-filled (dark) portion lifts a touch on hover, more on
    // press -- the same two-level brightening the knob face has (#6).
    const float trackThick = 6.0f;
    const float trackLift = 0.05f * hi + 0.07f * aA;
    juce::Rectangle<float> track = horizontal
        ? juce::Rectangle<float> (bounds.getX(), bounds.getCentreY() - trackThick * 0.5f, bounds.getWidth(), trackThick)
        : juce::Rectangle<float> (bounds.getCentreX() - trackThick * 0.5f, bounds.getY(), trackThick, bounds.getHeight());

    juce::ColourGradient tg (colours::bg.darker (0.25f).brighter (trackLift), track.getX(), track.getY(),
                             colours::bgRaised.brighter (trackLift), track.getX(), track.getBottom(), false);
    g.setGradientFill (tg);
    g.fillRoundedRectangle (track, trackThick * 0.5f);
    g.setColour (colours::outline.brighter (0.10f * hi));
    g.drawRoundedRectangle (track.reduced (0.5f), trackThick * 0.5f, 1.0f);

    // Eased visual position (#7): a preset / A-B switch sweeps the fill + thumb
    // instead of teleporting. During a hand drag we keep the real `pos` so the
    // thumb tracks the cursor exactly 1:1 (the inset lives in getSliderLayout).
    // A reset sweep draws at the eased vpos even while the button is held (see the
    // rotary path); a live hand drag keeps the real pos so the thumb tracks 1:1.
    if (! interacting || (bool) s.getProperties().getWithDefault ("resetSweep", false))
        if (const auto* v = s.getProperties().getVarPointer ("vpos"))
        {
            const float vp = juce::jlimit (0.0f, 1.0f, (float) (double) *v);
            pos = horizontal ? bounds.getX() + vp * bounds.getWidth()
                             : bounds.getBottom() - vp * bounds.getHeight();
        }

    const float r = 8.0f;
    juce::Rectangle<float> fill = horizontal
        ? track.withWidth (juce::jmax (0.0f, pos - bounds.getX()))
        : track.withTop (pos).withBottom (bounds.getBottom());

    // Filled portion: blue->teal gradient with a MANY-layered gradient-opacity
    // halo (NOT a hard widened stroke). The halo is brighter + wider on hover and
    // brighter + wider STILL on press, so the two levels clearly differ (#5).
    const juce::Colour fillLo (0xff5aa6ff), fillHi (0xff35d0c0);
    const float glowPeak   = 0.14f + 0.10f * hi + 0.16f * aA; // idle .14 / hover .24 / press .40
    const float glowSpread = 2.6f  + 1.8f  * hi + 2.0f  * aA; // idle 2.6 / hover 4.4 / press 6.4
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

    // Glassy thumb: a neutral gray-white rim at rest; on hover a feathered cyan
    // halo, on press a stronger one -- a blurred drop-shadow (gradient opacity),
    // never a hard ring (#5). Two clearly-different levels.
    const float cx = horizontal ? pos : bounds.getCentreX();
    const float cy = horizontal ? bounds.getCentreY() : pos;
    juce::Path thumbPath; thumbPath.addEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    const float thumbGlow = 0.30f * hi + 0.35f * aA; // hover .30 / press .65
    if (thumbGlow > 0.02f)
    {
        juce::DropShadow (fillHi.withAlpha (thumbGlow),        9, {}).drawForPath (g, thumbPath);
        juce::DropShadow (fillHi.withAlpha (thumbGlow * 0.6f), 4, {}).drawForPath (g, thumbPath);
    }
    // Thumb body fill brightens in TWO distinct levels like the knob face: a
    // little on hover, more on press (#2).
    juce::ColourGradient kg (colours::bgRaised.brighter (0.30f + 0.10f * hi + 0.14f * aA), cx, cy - r,
                             colours::bgPanel.darker (0.18f),     cx, cy + r, false);
    g.setGradientFill (kg);
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour (juce::Colour (0xffb8c2cf).interpolatedWith (fillHi, hi)); // gray rim -> cyan on touch
    g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.4f);
    // Glass rim micro-glow on the thumb ring, always faintly present and a touch
    // brighter on interaction (#5).
    glass::drawCircleEdge (g, cx, cy, r, 1.2f + 0.5f * hi);
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
    // Hover brightens only the switch + text, never the whole block (#4). Both
    // levels are EASED by the micro-anim driver, so the knob slides and the
    // glow fades instead of stepping (F3).
    const float hovA = animOr (b, "hovA", highlighted);
    const float onAv = animOr (b, "onA",  on);
    const float hi = 0.18f * hovA;

    // --- Level-meter glyph instead of the word "Meters" (#7) ---
    if (b.getComponentID() == "metersicon")
    {
        const auto col = colours::textDim.interpolatedWith (colours::accent, onAv).brighter (hi);
        const float barW = 5.0f, gap = 4.0f;
        const float totalW = barW * 2.0f + gap;
        const float barH = juce::jmin (bounds.getHeight() - 6.0f, 16.0f);
        const float x0 = bounds.getCentreX() - totalW * 0.5f;
        const float y0 = bounds.getCentreY() - barH * 0.5f;
        if (onAv > 0.02f) { g.setColour (col.withAlpha (0.18f * onAv)); g.fillRoundedRectangle (bounds.reduced (3.0f), 4.0f); }
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
        if (onAv > 0.02f) { g.setColour (onCol.withAlpha (0.22f * onAv)); g.fillRoundedRectangle (pill.expanded (2.0f), (ph + 4.0f) * 0.5f); }
        const auto pillBase = colours::bgRaised.brighter (hi).interpolatedWith (onCol, onAv);
        juce::ColourGradient pg (pillBase.brighter (0.06f + 0.04f * onAv), pill.getX(), pill.getY(),
                                 pillBase.darker (0.10f + 0.02f * onAv),   pill.getX(), pill.getBottom(), false);
        g.setGradientFill (pg);
        g.fillRoundedRectangle (pill, ph * 0.5f);
        g.setColour (colours::outline.brighter (hi).interpolatedWith (onCol, onAv));
        g.drawRoundedRectangle (pill, ph * 0.5f, 1.0f);
        const float knob = ph - 4.0f;
        const float kx = pill.getX() + 2.0f + (pill.getWidth() - knob - 4.0f) * onAv; // slides (F3)
        g.setColour (colours::text.interpolatedWith (colours::bg, onAv));
        g.fillEllipse (kx, pill.getCentreY() - knob * 0.5f, knob, knob);

        // Labels render with a shared 11 px baseline so Mono/Swap/M/S all line up
        // (#8). The polarity toggles are the exception: their "ø" is drawn LARGER
        // than the trailing letter but on the SAME baseline, so it reads bold
        // without dragging the letter's baseline around (#5).
        const juce::Colour tc = colours::textDim.interpolatedWith (colours::text, juce::jmax (onAv, hovA));
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
    if (onAv > 0.02f) // soft outer glow (fits inside the pad)
    {
        g.setColour (onCol.withAlpha (0.22f * onAv));
        g.fillRoundedRectangle (pill.expanded (2.0f), (h + 4.0f) * 0.5f);
    }
    const auto pillBase = colours::bgRaised.brighter (hi).interpolatedWith (onCol, onAv);
    juce::ColourGradient pg (pillBase.brighter (0.06f + 0.04f * onAv), pill.getX(), pill.getY(),
                             pillBase.darker (0.10f + 0.02f * onAv),   pill.getX(), pill.getBottom(), false);
    g.setGradientFill (pg);
    g.fillRoundedRectangle (pill, h * 0.5f);
    g.setColour (colours::outline.brighter (hi).interpolatedWith (onCol, onAv));
    g.drawRoundedRectangle (pill, h * 0.5f, 1.0f);

    const float knob = h - 4.0f;
    const float kx = pill.getX() + 2.0f + (pill.getWidth() - knob - 4.0f) * onAv; // slides (F3)
    g.setColour (colours::text.interpolatedWith (colours::bg, onAv));
    g.fillEllipse (kx, pill.getCentreY() - knob * 0.5f, knob, knob);

    // Label: fit-to-width so nothing is ever truncated to an ellipsis (#9).
    const float tx = pill.getRight() + 7.0f;
    const float tw = bounds.getRight() - tx - 1.0f;
    if (tw > 4.0f)
    {
        g.setColour (colours::textDim.interpolatedWith (colours::text, hovA));
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
    const float hovA = animOr (b, "hovA", highlighted); // eased hover wash (F3)

    // Preset bar (F2): the name + nav chevrons sit flat on the top bar and only
    // get a quiet rounded wash on hover/press -- FabFilter-style.
    if (b.getComponentID() == "presetname" || b.getComponentID() == "presetnav")
    {
        const float a = 0.55f * hovA + (down ? 0.25f : 0.0f);
        if (a > 0.02f)
        {
            g.setColour (colours::bgRaised.brighter (0.18f).withAlpha (a));
            g.fillRoundedRectangle (bounds, radius);
            g.setColour (colours::outline.withAlpha (a * 0.8f));
            g.drawRoundedRectangle (bounds, radius, 1.0f);
        }
        return;
    }

    const bool on = b.getToggleState();
    const auto base = down ? colours::bgRaised.brighter (0.12f)
                           : colours::bgRaised.brighter (0.06f * hovA);
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
    if (b.getComponentID() == "apply")      return juce::Font (juce::FontOptions (12.0f)); // Apply, a touch smaller (#21)
    if (b.getComponentID() == "icon")       return juce::Font (juce::FontOptions (21.0f)); // bigger glyph (#7)
    if (b.getComponentID() == "presetname") return juce::Font (juce::FontOptions (13.0f)); // preset display (F2)
    if (b.getComponentID() == "presetnav")  return juce::Font (juce::FontOptions (19.0f)); // chevrons (F2)
    return juce::Font (juce::FontOptions ((float) juce::jmin (13, juce::jmax (10, buttonHeight - 12))));
}

juce::Font AnamorphLookAndFeel::getComboBoxFont (juce::ComboBox&)  { return juce::Font (juce::FontOptions (13.5f)); }
juce::Font AnamorphLookAndFeel::getPopupMenuFont()                 { return juce::Font (juce::FontOptions (13.5f)); }

// Position the combo pop-up BELOW the box. The JUCE default adds withItemThatMustBeVisible +
// withInitiallySelectedItem, which centre the popup on the selected row so it COVERS the box (the
// native-macOS look). Targeting the box's screen bounds -- and omitting those two options -- makes
// the menu open flush below the box (or above if there's no room), restoring the drop-down.
juce::PopupMenu::Options AnamorphLookAndFeel::getOptionsForComboBoxPopupMenu (juce::ComboBox& box,
                                                                             juce::Label& label)
{
    return juce::PopupMenu::Options()
             .withTargetComponent (&box)
             .withTargetScreenArea (box.getScreenBounds())
             .withMinimumWidth (box.getWidth())
             .withMaximumNumColumns (1)
             .withStandardItemHeight (label.getHeight());
}

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
    if (b.getComponentID() == "presetname") // preset display (F2)
    {
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (colours::text);
        g.drawText (b.getButtonText(), b.getLocalBounds().reduced (6, 0),
                    juce::Justification::centred, true);
        return;
    }
    if (b.getComponentID() == "presetnav") // ‹ › chevrons brighten on hover (F2/F3)
    {
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (colours::textDim.interpolatedWith (colours::text, animOr (b, "hovA", b.isOver())));
        g.drawText (b.getButtonText(), b.getLocalBounds().translated (0, -1),
                    juce::Justification::centred, false);
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

// FACTORY / USER section headers in the preset menu: small dim caps (F2).
void AnamorphLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area,
                                                      const juce::String& sectionName)
{
    g.setColour (colours::textDim.withAlpha (0.85f));
    g.setFont (juce::Font (juce::FontOptions (10.5f)).withExtraKerningFactor (0.18f));
    g.drawText (sectionName, area.reduced (12, 0), juce::Justification::centredLeft);
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
    // Hover lift is EASED by the micro-anim driver; the open lift stays instant
    // so the box visibly anchors its list (F3).
    const float lift = 0.05f + 0.07f * animOr (box, "hovA", hover) + (open ? 0.06f : 0.0f);
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

// Text fields carrying a "glow" property (the Save-Preset name box) get a
// rounded, faintly accent-lit border when focused -- the same micro-glow as an
// open combo, so it reads premium rather than a thin default rectangle (#11).
void AnamorphLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height,
                                                    juce::TextEditor& ed)
{
    if (! (bool) ed.getProperties().getWithDefault ("glow", false))
    {
        juce::LookAndFeel_V4::fillTextEditorBackground (g, width, height, ed);
        return;
    }
    auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (ed.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (b, 5.0f);
}

void AnamorphLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height,
                                                 juce::TextEditor& ed)
{
    if (! (bool) ed.getProperties().getWithDefault ("glow", false))
    {
        juce::LookAndFeel_V4::drawTextEditorOutline (g, width, height, ed); // value boxes unchanged
        return;
    }

    auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    const float radius = 5.0f;

    if (ed.hasKeyboardFocus (true))
    {
        // Faint accent bloom just inside the rim + a thin vertical-gradient border.
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (colours::accent.withAlpha (0.045f * (float) i));
            g.drawRoundedRectangle (b.reduced ((float) (4 - i)),
                                    juce::jmax (1.5f, radius - (float) (4 - i)), 1.8f);
        }
        juce::ColourGradient og (colours::accent.brighter (0.20f), b.getX(), b.getY(),
                                 colours::accent.withAlpha (0.55f), b.getX(), b.getBottom(), false);
        g.setGradientFill (og);
        g.drawRoundedRectangle (b, radius, 1.2f);
    }
    else
    {
        g.setColour (colours::outline);
        g.drawRoundedRectangle (b, radius, 1.0f);
    }
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
    const auto full = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    auto b = full.reduced (1.0f);
    // On platforms WITHOUT per-pixel window alpha (notably Linux/X11 with no compositor),
    // juce::TooltipWindow cannot be semi-transparent, so the area OUTSIDE the rounded capsule
    // renders the window's opaque fill -> black corners (KI-006). Pre-fill the whole bounds with
    // the capsule colour so the corners match the capsule instead of rendering black. Where
    // transparent windows ARE available (macOS / Windows / compositing Linux) the corners stay
    // genuinely transparent -- no visual change there.
    if (! juce::Desktop::getInstance().canUseSemiTransparentWindows())
    {
        g.setColour (colours::bgRaised);
        g.fillRect (full);
    }
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
