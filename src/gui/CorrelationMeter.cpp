#include "CorrelationMeter.h"
#include "LookAndFeel.h"

namespace anamorph::gui
{

StereoMeter::StereoMeter (anamorph::CorrelationMeter& src, Orientation o, Type t)
    : source (src), orientation (o), type (t)
{
    startTimerHz (60); // match the vectorscope's frame rate -- 30 Hz juddered (#2)
}

StereoMeter::~StereoMeter() { stopTimer(); }

void StereoMeter::timerCallback()
{
    // When the input goes silent (or before any audio at all), both the phase
    // correlation and the L/R balance lose all meaning, so the pointer glides
    // gently back to centre (0) rather than holding at an extreme or jumping to
    // +1 on open (#1/#2). Active motion stays snappy; the return-to-centre is a
    // slow ~700 ms damped release so it reads as relaxing, never a snap.
    const bool silent = source.getEnergy() < 6.0e-9f; // ~ -82 dBFS (sum of L^2 + R^2)
    const float target = silent ? 0.0f
                                : (type == Type::Balance ? source.getBalance() : source.getSlow());
    const float rate = silent ? 0.030f : 0.165f;
    value += rate * (target - value);
    repaint();
}

void StereoMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    glass::fillPanel (g, bounds, 4.0f, colours::bgPanel, 0.85f); // gentle 0.5.3-style frame (#1)

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

    // Pointer with a layered glow + glass gradient core, matching the panels'
    // refraction/diffraction language (#8).
    const float thick = 3.0f;
    juce::Rectangle<float> pr = horizontal
        ? juce::Rectangle<float> (track.getX() + norm * track.getWidth() - thick * 0.5f, track.getY(), thick, track.getHeight())
        : juce::Rectangle<float> (track.getX(), track.getBottom() - norm * track.getHeight() - thick * 0.5f, track.getWidth(), thick);

    for (int i = 0; i < 5; ++i) // soft layered glow (subtle, #1)
    {
        const float t  = (float) i / 4.0f;
        const float ex = (1.0f - t) * 3.0f;
        auto e = pr.expanded (ex);
        g.setColour (col.withAlpha (0.18f * t * t));
        g.fillRoundedRectangle (e, juce::jmin (e.getWidth(), e.getHeight()) * 0.5f);
    }
    juce::ColourGradient pg (col.brighter (0.22f), pr.getX(), pr.getY(),
                             col.darker (0.14f),   pr.getRight(), pr.getBottom(), false);
    g.setGradientFill (pg);
    g.fillRoundedRectangle (pr, juce::jmin (pr.getWidth(), pr.getHeight()) * 0.5f);
    g.setColour (juce::Colours::white.withAlpha (0.30f)); // leading-edge glass highlight
    if (horizontal) g.fillRoundedRectangle (pr.withWidth (1.2f).translated (0.4f, 0.0f), 0.6f);
    else            g.fillRoundedRectangle (pr.withHeight (1.2f).translated (0.0f, 0.4f), 0.6f);

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
