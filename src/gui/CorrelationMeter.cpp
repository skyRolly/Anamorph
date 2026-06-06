#include "CorrelationMeter.h"
#include "LookAndFeel.h"

namespace anamorph::gui
{

StereoMeter::StereoMeter (anamorph::CorrelationMeter& src, Orientation o, Type t)
    : source (src), orientation (o), type (t)
{
    startTimerHz (30);
}

StereoMeter::~StereoMeter() { stopTimer(); }

void StereoMeter::timerCallback()
{
    const float target = (type == Type::Balance) ? source.getBalance() : source.getSlow();
    value += 0.3f * (target - value); // gentle visual smoothing
    repaint();
}

void StereoMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (colours::bgPanel);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    const bool horizontal = (orientation == Orientation::Horizontal);
    auto track = bounds.reduced (4.0f);
    const float norm = (value + 1.0f) * 0.5f; // 0..1

    // Colour language: stay on the accent near centre, drift to a warm tone
    // toward the extremes -- consistent and high-end, no garish red except clip.
    const float extremity = juce::jlimit (0.0f, 1.0f, std::abs (value));
    juce::Colour col;
    if (type == Type::Correlation && value < 0.0f)
        col = colours::accent2.interpolatedWith (juce::Colour (0xffd8704a), extremity); // anti-phase warning
    else
        col = colours::accent.interpolatedWith (colours::warn, extremity * 0.85f);

    // Centre tick
    g.setColour (colours::outline.brighter (0.25f));
    if (horizontal) g.drawLine (track.getCentreX(), track.getY(), track.getCentreX(), track.getBottom(), 1.0f);
    else            g.drawLine (track.getX(), track.getCentreY(), track.getRight(), track.getCentreY(), 1.0f);

    g.setColour (col);
    const float thick = 3.0f;
    if (horizontal)
    {
        const float x = track.getX() + norm * track.getWidth();
        g.fillRoundedRectangle (x - thick * 0.5f, track.getY(), thick, track.getHeight(), 1.5f);
    }
    else
    {
        const float y = track.getBottom() - norm * track.getHeight(); // +1 at top
        g.fillRoundedRectangle (track.getX(), y - thick * 0.5f, track.getWidth(), thick, 1.5f);
    }

    // End labels
    g.setColour (colours::textDim);
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    if (horizontal)
    {
        g.drawText (type == Type::Balance ? "L" : "-1", (int) track.getX(), (int) track.getY(),
                    16, (int) track.getHeight(), juce::Justification::centredLeft);
        g.drawText (type == Type::Balance ? "R" : "+1", (int) track.getRight() - 18, (int) track.getY(),
                    18, (int) track.getHeight(), juce::Justification::centredRight);
    }
    else
    {
        // Vertical phase meter: +1 at the top, -1 at the bottom, matching the
        // L/R balance meter's labelled ends (feedback #8).
        g.drawText ("+1", (int) track.getX(), (int) track.getY() + 1,
                    (int) track.getWidth(), 10, juce::Justification::centred);
        g.drawText ("-1", (int) track.getX(), (int) track.getBottom() - 11,
                    (int) track.getWidth(), 10, juce::Justification::centred);
    }
}

} // namespace anamorph::gui
