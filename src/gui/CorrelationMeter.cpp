#include "CorrelationMeter.h"
#include "LookAndFeel.h"

namespace anamorph::gui
{

CorrelationMeter::CorrelationMeter (anamorph::CorrelationMeter& src, Orientation o)
    : source (src), orientation (o)
{
    startTimerHz (30);
}

CorrelationMeter::~CorrelationMeter() { stopTimer(); }

void CorrelationMeter::timerCallback()
{
    const float target = (orientation == Orientation::Horizontal) ? source.getSlow() : source.getFast();
    value += 0.35f * (target - value); // gentle visual smoothing
    repaint();
}

void CorrelationMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (colours::bgPanel);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    const bool horizontal = (orientation == Orientation::Horizontal);
    auto track = bounds.reduced (4.0f);

    // Map correlation -1..+1 to position; +1 (mono) at the "good" end.
    const float norm = (value + 1.0f) * 0.5f; // 0..1

    // Colour: red-ish for anti-phase (<0), accent for in-phase.
    const auto col = (value < 0.0f)
        ? colours::warn.interpolatedWith (juce::Colour (0xffe0584a), juce::jlimit (0.0f, 1.0f, -value))
        : colours::accent2.interpolatedWith (colours::accent, value);

    // Centre tick (correlation = 0)
    g.setColour (colours::outline.brighter (0.2f));
    if (horizontal)
        g.drawLine (track.getCentreX(), track.getY(), track.getCentreX(), track.getBottom(), 1.0f);
    else
        g.drawLine (track.getX(), track.getCentreY(), track.getRight(), track.getCentreY(), 1.0f);

    g.setColour (col);
    const float thick = 3.0f;
    if (horizontal)
    {
        const float x = track.getX() + norm * track.getWidth();
        g.fillRoundedRectangle (x - thick * 0.5f, track.getY(), thick, track.getHeight(), 1.5f);
    }
    else
    {
        // +1 at the top
        const float y = track.getBottom() - norm * track.getHeight();
        g.fillRoundedRectangle (track.getX(), y - thick * 0.5f, track.getWidth(), thick, 1.5f);
    }

    g.setColour (colours::textDim);
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    if (horizontal)
    {
        g.drawText ("-1", track.getX(), (int) track.getY(), 16, (int) track.getHeight(), juce::Justification::centredLeft);
        g.drawText ("+1", (int) track.getRight() - 18, (int) track.getY(), 18, (int) track.getHeight(), juce::Justification::centredRight);
    }
}

} // namespace anamorph::gui
