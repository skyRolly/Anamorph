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
    if (! isShowing())
        return; // whole-editor hidden: the glide resumes live on re-show (S3)

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

    // Land exactly on the target once within 1e-3 of it: that final step moves
    // the pointer < 0.2 px and shifts the colour blend by under a quarter of a
    // display quantum (1/255), so the snap itself is invisible -- it just ends
    // the asymptotic float tail so a settled meter can go idle (S3). The
    // ~700 ms relax animation above runs to visual completion first.
    if (std::abs (target - value) < 1.0e-3f)
        value = target;

    // The frame is a pure function of `value` (+ bounds): repaint only when the
    // drawn value actually changed. Resize/expose repaints bypass the timer.
    if (std::abs (value - shownValue) > 0.0f)
    {
        shownValue = value;
        repaint();
    }
}

void StereoMeter::visibilityChanged()
{
    // Own-visibility lifecycle (S3): no timer wakeups at all while explicitly
    // hidden; re-showing restarts the tick and forces one repaint.
    if (isVisible())
    {
        shownValue = 1.0e9f;
        startTimerHz (60);
    }
    else
        stopTimer();
}

// Render the static layer (H13, the H2 recipe): the glass panel and the centre
// tick -- everything drawn BENEATH the pointer that is a pure function of
// (size, physical scale, look). The drawing code is IDENTICAL to what paint()
// ran directly before the cache; the image is rendered at the destination's
// PHYSICAL resolution so the blit in paint() is a 1:1 device-pixel copy. The
// end labels are deliberately NOT here: the original z-order paints them on
// top of the pointer, so they stay live in paint() to preserve stacking.
//
// Exactness: byte-identical whenever size x scale is integral (measured). At
// FRACTIONAL physical sizes (e.g. 18 px at 125 % DPI = 22.5) the blit takes
// JUCE's interpolating path, wobbling AA border pixels by <= ~25 % of a
// channel and interior transitions by <= ~5 % -- the exact behaviour of
// juce::Component::setBufferedToImage, whose construction this mirrors.
void StereoMeter::ensureStaticLayer (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (! staticLayer.isNull()
        && staticW == getWidth() && staticH == getHeight()
        && ! (std::abs (staticScale - scale) > 0.0f)) // exact: same idiom as the S4 gate
        return;

    staticW = getWidth();
    staticH = getHeight();
    staticScale = scale;
    staticLayer = juce::Image (juce::Image::ARGB,
                               juce::jmax (1, juce::roundToInt ((float) staticW * scale)),
                               juce::jmax (1, juce::roundToInt ((float) staticH * scale)),
                               true);
    juce::Graphics ig (staticLayer);
    ig.addTransform (juce::AffineTransform::scale (scale));

    glass::fillPanel (ig, bounds, 4.0f, colours::bgPanel, 0.85f); // gentle 0.5.3-style frame (#1)

    const bool horizontal = (orientation == Orientation::Horizontal);
    auto track = bounds.reduced (4.0f);

    // Centre tick
    ig.setColour (colours::outline.brighter (0.25f));
    if (horizontal) ig.drawLine (track.getCentreX(), track.getY(), track.getCentreX(), track.getBottom(), 1.0f);
    else            ig.drawLine (track.getX(), track.getCentreY(), track.getRight(), track.getCentreY(), 1.0f);
}

void StereoMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Static layer (H13): blit the cached panel + centre tick instead of
    // re-rasterizing them at 60 Hz (measured Wave 1.2: glass::fillPanel from
    // this component = 14.6 % of the active default-view GUI profile).
    ensureStaticLayer (g, bounds);
    {
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::scale (1.0f / staticScale));
        g.drawImageAt (staticLayer, 0, 0);
    }

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
