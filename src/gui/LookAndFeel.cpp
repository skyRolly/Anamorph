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

    // Track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - thick, radius - thick, 0.0f,
                         startAngle, endAngle, true);
    g.setColour (colours::outline);
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc with a subtle accent gradient
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - thick, radius - thick, 0.0f,
                         startAngle, angle, true);
    juce::ColourGradient grad (colours::accent2, centre.x - radius, centre.y,
                               colours::accent,  centre.x + radius, centre.y, false);
    g.setGradientFill (grad);
    g.strokePath (value, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Knob face
    const float faceR = radius - thick * 1.6f;
    g.setColour (colours::bgRaised);
    g.fillEllipse (centre.x - faceR, centre.y - faceR, faceR * 2.0f, faceR * 2.0f);
    g.setColour (colours::outline);
    g.drawEllipse (centre.x - faceR, centre.y - faceR, faceR * 2.0f, faceR * 2.0f, 1.0f);

    // Pointer
    juce::Path pointer;
    const float pl = faceR * 0.92f, pr = thick * 0.35f;
    pointer.addRoundedRectangle (-pr, -pl, pr * 2.0f, pl * 0.6f, pr);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    g.setColour (colours::text);
    g.fillPath (pointer);

    juce::ignoreUnused (s);
}

void AnamorphLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                            float pos, float, float,
                                            juce::Slider::SliderStyle style, juce::Slider& s)
{
    const bool horizontal = (style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar);
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);

    const float trackThick = 5.0f;
    juce::Rectangle<float> track = horizontal
        ? juce::Rectangle<float> (bounds.getX(), bounds.getCentreY() - trackThick * 0.5f, bounds.getWidth(), trackThick)
        : juce::Rectangle<float> (bounds.getCentreX() - trackThick * 0.5f, bounds.getY(), trackThick, bounds.getHeight());

    g.setColour (colours::outline);
    g.fillRoundedRectangle (track, trackThick * 0.5f);

    g.setColour (colours::accent);
    if (horizontal)
        g.fillRoundedRectangle (track.withWidth (pos - bounds.getX()), trackThick * 0.5f);
    else
        g.fillRoundedRectangle (track.withTop (pos).withBottom (bounds.getBottom()), trackThick * 0.5f);

    const float r = 7.0f;
    const float cx = horizontal ? pos : bounds.getCentreX();
    const float cy = horizontal ? bounds.getCentreY() : pos;
    g.setColour (colours::text);
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    juce::ignoreUnused (s);
}

void AnamorphLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                            bool highlighted, bool /*down*/)
{
    auto bounds = b.getLocalBounds().toFloat();
    const float h = juce::jmin (20.0f, bounds.getHeight());
    auto pill = juce::Rectangle<float> (bounds.getX(), bounds.getCentreY() - h * 0.5f, h * 1.9f, h);

    const bool on = b.getToggleState();
    g.setColour (on ? colours::accent.withAlpha (0.9f) : colours::bgRaised);
    g.fillRoundedRectangle (pill, h * 0.5f);
    g.setColour (on ? colours::accent : colours::outline);
    g.drawRoundedRectangle (pill, h * 0.5f, 1.0f);

    const float knob = h - 4.0f;
    const float kx = on ? pill.getRight() - knob - 2.0f : pill.getX() + 2.0f;
    g.setColour (on ? colours::bg : colours::text);
    g.fillEllipse (kx, pill.getCentreY() - knob * 0.5f, knob, knob);

    g.setColour (highlighted ? colours::text : colours::textDim);
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawText (b.getButtonText(),
                pill.getRight() + 8, (int) bounds.getY(),
                (int) (bounds.getWidth() - pill.getWidth() - 8), (int) bounds.getHeight(),
                juce::Justification::centredLeft);
}

void AnamorphLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                                const juce::Colour&, bool highlighted, bool down)
{
    auto bounds = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    auto fill = on ? colours::accent.withAlpha (0.9f)
                   : (down ? colours::bgRaised.brighter (0.1f)
                           : (highlighted ? colours::bgRaised.brighter (0.05f) : colours::bgRaised));
    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (on ? colours::accent : colours::outline);
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
}

void AnamorphLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool,
                                        int, int, int, int, juce::ComboBox&)
{
    auto bounds = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
    g.setColour (colours::bgRaised);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

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

} // namespace anamorph::gui
