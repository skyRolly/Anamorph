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

    // Value arc with a subtle accent gradient + a self-illuminating glow that
    // strengthens on hover / press.
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - thick, radius - thick, 0.0f,
                         startAngle, angle, true);
    const float glowA = active ? 0.55f : hover ? 0.34f : 0.18f;
    const float glowW = active ? 3.0f  : hover ? 2.5f  : 2.1f;
    g.setColour (colours::accent.withAlpha (glowA)); // glow halo
    g.strokePath (value, juce::PathStrokeType (thick * glowW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::ColourGradient grad (active ? colours::accent2.brighter (0.25f) : colours::accent2, centre.x - radius, centre.y,
                               active ? colours::accent.brighter (0.25f)  : colours::accent,  centre.x + radius, centre.y, false);
    g.setGradientFill (grad);
    g.strokePath (value, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Glassy knob face: a top-lit radial gradient over a dark base.
    const float faceR = radius - thick * 1.6f;
    juce::ColourGradient face (colours::bgRaised.brighter (0.16f), centre.x, centre.y - faceR * 0.7f,
                               colours::bgPanel.darker (0.25f),    centre.x, centre.y + faceR, true);
    face.addColour (0.55, colours::bgRaised);
    g.setGradientFill (face);
    g.fillEllipse (centre.x - faceR, centre.y - faceR, faceR * 2.0f, faceR * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f)); // faint top highlight rim
    g.drawEllipse (centre.x - faceR, centre.y - faceR + 0.6f, faceR * 2.0f, faceR * 2.0f, 1.0f);
    g.setColour (colours::outline);
    g.drawEllipse (centre.x - faceR, centre.y - faceR, faceR * 2.0f, faceR * 2.0f, 1.0f);

    // Pointer (brighter, with a soft glow, while pressed / dragging).
    juce::Path pointer;
    const float pl = faceR * 0.92f, pr = thick * 0.35f;
    pointer.addRoundedRectangle (-pr, -pl, pr * 2.0f, pl * 0.6f, pr);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    if (active)
    {
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.strokePath (pointer, juce::PathStrokeType (3.0f));
    }
    g.setColour (active ? juce::Colours::white : colours::text);
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

    juce::Rectangle<float> fill = horizontal
        ? track.withWidth (juce::jmax (0.0f, pos - bounds.getX()))
        : track.withTop (pos).withBottom (bounds.getBottom());

    // Self-illuminating accent fill: soft glow halo (stronger on hover/drag, #10).
    const bool act = s.isMouseOverOrDragging() || (bool) s.getProperties().getWithDefault ("dragging", false);
    g.setColour (colours::accent.withAlpha (act ? 0.40f : 0.22f));
    g.fillRoundedRectangle (fill.expanded (1.6f), (trackThick + 3.2f) * 0.5f);
    juce::ColourGradient fg (colours::accent2, fill.getX(), fill.getY(),
                             colours::accent, horizontal ? fill.getRight() : fill.getX(),
                             horizontal ? fill.getY() : fill.getBottom(), false);
    g.setGradientFill (fg);
    g.fillRoundedRectangle (fill, trackThick * 0.5f);

    // Glassy thumb with a glow and an accent rim.
    const float r = 8.0f;
    const float cx = horizontal ? pos : bounds.getCentreX();
    const float cy = horizontal ? bounds.getCentreY() : pos;
    g.setColour (colours::accent.withAlpha (act ? 0.45f : 0.28f));
    g.fillEllipse (cx - r - 2.0f, cy - r - 2.0f, (r + 2.0f) * 2.0f, (r + 2.0f) * 2.0f);
    juce::ColourGradient kg (colours::bgRaised.brighter (act ? 0.5f : 0.32f), cx, cy - r,
                             colours::bgPanel.darker (0.18f),     cx, cy + r, false);
    g.setGradientFill (kg);
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour (act ? colours::accent.brighter (0.2f) : colours::accent);
    g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.4f);
}

void AnamorphLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                            bool highlighted, bool /*down*/)
{
    auto bounds = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();

    // Hover feedback for every toggle style (#10): a faint bright wash.
    if (highlighted)
    {
        g.setColour (colours::text.withAlpha (0.07f));
        g.fillRoundedRectangle (bounds.reduced (1.0f), 5.0f);
    }

    // --- Level-meter glyph instead of the word "Meters" (#7) ---
    if (b.getComponentID() == "metersicon")
    {
        const auto col = on ? colours::accent : colours::textDim;
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
        const juce::Colour onCol = colours::accent;
        const float ph = 15.0f, pw = ph * 1.7f;
        auto pill = juce::Rectangle<float> (bounds.getCentreX() - pw * 0.5f, bounds.getY() + 2.0f, pw, ph);
        if (on) { g.setColour (onCol.withAlpha (0.22f)); g.fillRoundedRectangle (pill.expanded (2.0f), (ph + 4.0f) * 0.5f); }
        juce::ColourGradient pg (on ? onCol.brighter (0.10f) : colours::bgRaised.brighter (0.06f), pill.getX(), pill.getY(),
                                 on ? onCol.darker (0.12f)   : colours::bgRaised.darker (0.10f),   pill.getX(), pill.getBottom(), false);
        g.setGradientFill (pg);
        g.fillRoundedRectangle (pill, ph * 0.5f);
        g.setColour (on ? onCol : colours::outline);
        g.drawRoundedRectangle (pill, ph * 0.5f, 1.0f);
        const float knob = ph - 4.0f;
        const float kx = on ? pill.getRight() - knob - 2.0f : pill.getX() + 2.0f;
        g.setColour (on ? colours::bg : colours::text);
        g.fillEllipse (kx, pill.getCentreY() - knob * 0.5f, knob, knob);

        const auto labelArea = bounds.withTop (pill.getBottom() + 1.0f).toNearestInt();
        const juce::Colour tc = (on || highlighted ? colours::text : colours::textDim);
        const auto txt = b.getButtonText();
        if (txt.isNotEmpty() && txt[0] == (juce::juce_wchar) 0x00F8) // polarity "ø L/R": bigger symbol (#30)
        {
            juce::AttributedString as;
            as.setJustification (juce::Justification::centred);
            as.append (txt.substring (0, 1), juce::Font (juce::FontOptions (13.0f)), tc); // slightly smaller -> less line-height skew
            as.append (txt.substring (1),     juce::Font (juce::FontOptions (11.0f)), tc);
            as.draw (g, labelArea.toFloat());
        }
        else
        {
            // Nudge down so Mono/Swap/M/S sit at the same baseline as the taller
            // "ø L/R" line (whose bigger glyph raises it) (#2/#6).
            g.setColour (tc);
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawFittedText (txt, labelArea.translated (0, 3), juce::Justification::centred, 1, 0.8f);
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
    const juce::Colour onCol = (b.getComponentID() == "bypass") ? juce::Colour (0xffd0584e)
                                                                : colours::accent;
    if (on) // soft outer glow (fits inside the pad)
    {
        g.setColour (onCol.withAlpha (0.22f));
        g.fillRoundedRectangle (pill.expanded (2.0f), (h + 4.0f) * 0.5f);
    }
    juce::ColourGradient pg (on ? onCol.brighter (0.10f) : colours::bgRaised.brighter (0.06f),
                             pill.getX(), pill.getY(),
                             on ? onCol.darker (0.12f)   : colours::bgRaised.darker (0.10f),
                             pill.getX(), pill.getBottom(), false);
    g.setGradientFill (pg);
    g.fillRoundedRectangle (pill, h * 0.5f);
    g.setColour (on ? onCol : colours::outline);
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
    if (b.getComponentID() == "apply") return juce::Font (juce::FontOptions (13.5f)); // Apply (#6)
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
    // Hover / open feedback (#10): brighten the body, accent the outline.
    const bool hover = box.isMouseOver (true);
    const bool open  = down || box.isPopupActive();
    const float lift = open ? 0.18f : hover ? 0.12f : 0.05f;
    juce::ColourGradient gr (colours::bgRaised.brighter (lift), bounds.getX(), bounds.getY(),
                             colours::bgRaised.darker (0.10f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (gr);
    g.fillRoundedRectangle (bounds, radius);
    g.setColour (open ? colours::accent.withAlpha (0.8f) : colours::outline);
    g.drawRoundedRectangle (bounds, radius, open ? 1.4f : 1.0f);

    juce::Path arrow;
    const float cx = (float) w - 14.0f, cy = (float) h * 0.5f;
    arrow.startNewSubPath (cx - 4, cy - 2);
    arrow.lineTo (cx, cy + 3);
    arrow.lineTo (cx + 4, cy - 2);
    g.setColour (colours::textDim);
    g.strokePath (arrow, juce::PathStrokeType (1.6f));
}

juce::Font AnamorphLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::Font (juce::FontOptions (13.0f));
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
            const bool left = disp.startsWithIgnoreCase ("L");          // Left -> negative
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
    auto b = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (colours::bgRaised);
    g.fillRoundedRectangle (b.reduced (1.0f), 6.0f);
    g.setColour (colours::accent.withAlpha (0.55f));
    g.drawRoundedRectangle (b.reduced (1.0f), 6.0f, 1.0f);
    layoutTooltip (text, (float) w - 20.0f).draw (g, b.reduced (10.0f, 7.0f));
}

} // namespace anamorph::gui
