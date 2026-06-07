#include "Vectorscope.h"
#include "LookAndFeel.h"
#include <cmath>

namespace anamorph::gui
{

static constexpr float kInvSqrt2 = 0.70710678118654752440f;

Vectorscope::Vectorscope (anamorph::ScopeBuffer& buffer) : scope (buffer)
{
    bufL.resize (anamorph::ScopeBuffer::capacity);
    bufR.resize (anamorph::ScopeBuffer::capacity);
    setOpaque (false);
    startTimerHz (60);
}

Vectorscope::~Vectorscope() { stopTimer(); }

void Vectorscope::drawGrid (juce::Graphics& g, juce::Rectangle<float> area, float radius)
{
    const auto c = area.getCentre();

    // Concentric reference rings
    g.setColour (colours::outline.withAlpha (0.5f));
    for (float frac : { 0.33f, 0.66f, 1.0f })
        g.drawEllipse (c.x - radius * frac, c.y - radius * frac,
                       radius * 2.0f * frac, radius * 2.0f * frac, 1.0f);

    // Axes: vertical = Mono(Mid), horizontal = Side; plus the L/R diagonals.
    g.setColour (colours::outline.withAlpha (0.6f));
    g.drawLine (c.x, c.y - radius, c.x, c.y + radius, 1.0f);          // M
    g.drawLine (c.x - radius, c.y, c.x + radius, c.y, 1.0f);          // S
    g.setColour (colours::outline.withAlpha (0.35f));
    const float d = radius * kInvSqrt2;
    g.drawLine (c.x - d, c.y - d, c.x + d, c.y + d, 1.0f);            // L axis
    g.drawLine (c.x - d, c.y + d, c.x + d, c.y - d, 1.0f);            // R axis

    g.setColour (colours::textDim.withAlpha (0.7f));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText ("M", (int) (c.x - 10), (int) (c.y - radius - 16), 20, 14, juce::Justification::centred);
    g.drawText ("L", (int) (c.x - d - 16), (int) (c.y - d - 14), 16, 14, juce::Justification::centred);
    g.drawText ("R", (int) (c.x + d), (int) (c.y - d - 14), 16, 14, juce::Justification::centred);
}

void Vectorscope::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    // Background: subtle vertical gradient inside a rounded panel.
    juce::ColourGradient bgGrad (colours::bgPanel, area.getCentreX(), area.getY(),
                                 colours::bg, area.getCentreX(), area.getBottom(), false);
    g.setGradientFill (bgGrad);
    g.fillRoundedRectangle (area, 8.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (area.reduced (0.5f), 8.0f, 1.0f);

    const auto plot = area.reduced (18.0f);
    const float radius = juce::jmin (plot.getWidth(), plot.getHeight()) * 0.5f;
    const auto centre = plot.getCentre();

    drawGrid (g, plot, radius);

    // --- read a decimated window from the lock-free ring buffer ---
    const int wantFrames = (int) juce::jmap (persistence, 0.0f, 1.0f, 1200.0f, 8000.0f);
    const int got = scope.readLatest (bufL.data(), bufR.data(),
                                      juce::jmin (wantFrames, (int) bufL.size()));
    if (got <= 0)
        return;

    // Fixed scale calibrated so 0 dBFS on one channel lands near the rim, with a
    // little visual gain so normal tracks fill the scope nicely (#11). Points
    // that exceed the rim are pinned to it (they pile up like Imager 2).
    const float effScale = radius * 1.4f;
    const float baseAlpha = juce::jmap (persistence, 0.0f, 1.0f, 0.22f, 0.5f);

    const int maxPoints = 3000;
    const int step = juce::jmax (1, got / maxPoints);

    float clip = 0.0f; // peak magnitude for the clip indicator

    {
        juce::Graphics::ScopedSaveState save (g);
        // Expand the clip a touch so points pinned to the rim aren't shaved at the
        // top/bottom/left/right (feedback #12).
        g.reduceClipRegion (plot.expanded (3.0f).toNearestInt());

        for (int i = 0; i < got; i += step)
        {
            const float L = bufL[(size_t) i];
            const float R = bufR[(size_t) i];
            clip = juce::jmax (clip, std::abs (L), std::abs (R));

            // Rotate 45 deg: vertical = Mid (mono up), horizontal = Side.
            // L leans LEFT, R leans RIGHT (so dx uses -(L-R) = R-L) -- fixes the
            // previously mirrored image (feedback #24).
            const float side = (L - R) * kInvSqrt2;
            const float mid  = (L + R) * kInvSqrt2;

            float dx = -side * effScale;
            float dy = -mid * effScale;
            const float mag = std::sqrt (dx * dx + dy * dy);
            if (mag > radius && mag > 0.0f) { const float k = radius / mag; dx *= k; dy *= k; }

            const float px = centre.x + dx;
            const float py = centre.y + dy;

            const float age = (float) i / (float) got;        // 0 oldest .. 1 newest
            const float a = baseAlpha * (0.15f + 0.85f * age);
            const float spread = juce::jlimit (0.0f, 1.0f, std::abs (side) * 1.6f);
            const auto col = colours::accent2.interpolatedWith (colours::accent, spread).withAlpha (a);
            g.setColour (col);
            g.fillRect (px - 0.8f, py - 0.8f, 1.6f, 1.6f);
        }
    }

    // Clip indicator: drawn OUTSIDE the plot clip region so the ring's stroke
    // isn't shaved at the top/bottom/left/right (feedback #2). The surrounding
    // 18 px margin gives the stroke room.
    if (clip > 0.95f)
    {
        const float amt = juce::jlimit (0.0f, 1.0f, (clip - 0.95f) / 0.35f);
        g.setColour (juce::Colour (0xffe0584a).withAlpha (0.15f + 0.55f * amt));
        g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.5f + 2.5f * amt);
    }
}

} // namespace anamorph::gui
