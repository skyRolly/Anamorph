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
    // Gate init: treat whatever the ring already holds as non-silent, so the
    // first windowFrames() of observed frames always paint (conservative --
    // an editor reopened mid-playback shows the live picture immediately).
    lastSeenCount = lastNonZero = buffer.writeCount();
    // Opaque (N2): paint() covers every pixel of the bounds -- the cached static
    // layer pre-fills the rounded-rect corners with the editor backdrop colour
    // (colours::bg, the flat fillAll behind the whole scope/meter row), so the
    // parent never needs to re-render beneath this component and the cached
    // layer can blit as an opaque copy instead of a per-pixel alpha composite.
    setOpaque (true);
    // Adaptive refresh: ride the display's vblank (capped ~120 Hz) instead of a
    // fixed 60 Hz timer. The idle gate below keeps a quiescent tick near-free.
    frameClock.start (*this, [this] (double) { tick(); });
}

Vectorscope::~Vectorscope() { frameClock.stop(); }

void Vectorscope::tick()
{
    // Whole-editor hidden (host hid the window without destroying the editor):
    // skip the freshness scan entirely, like the LevelMeter / StereoMeter S3
    // gates (Wave 4 -- this was the one visualizer ticking while unseen). On
    // re-show the very next tick sees the accumulated `fresh` delta, scans the
    // (capacity-capped) newly arrived frames and repaints -- the same
    // conservative catch-up the constructor's gate init performs.
    if (! isShowing())
        return;

    // Idle repaint gate. paint() is a pure function of (window content,
    // persistence, size) -- the trail's age-alpha is positional, not clocked --
    // so a frame only needs re-rendering when that content can have changed.
    // Two static cases are skipped: the ring is FROZEN (the host stopped
    // calling processBlock), or every frame in the visible window is exactly
    // zero and the previously painted frame was that same all-zero image
    // (digital silence: the engine keeps pushing zeros, so the write counter
    // advances while the picture stays put). While ANY non-zero frame remains
    // in the window the tick repaints as before, so the trail scrolls out and
    // fades exactly as it always did; the final all-zero frame paints once,
    // then the view goes idle. A persistence change re-arms via setPersistence
    // (frameDirty); resize/expose repaints bypass the timer entirely.
    const auto count = scope.writeCount();
    if (const auto fresh = count - lastSeenCount; fresh > 0)
    {
        // Scan only the newly arrived frames (order-of-800 samples per tick,
        // far cheaper than one paint). Frames older than the scratch size can
        // never re-enter any window (max window < capacity), so capping is safe.
        const int n   = (int) juce::jmin (fresh, (std::uint64_t) bufL.size());
        const int got = scope.readLatest (bufL.data(), bufR.data(), n);
        for (int i = 0; i < got; ++i)
            if (std::abs (bufL[(size_t) i]) > 0.0f || std::abs (bufR[(size_t) i]) > 0.0f)
            {
                lastNonZero = count;
                break;
            }
        lastSeenCount = count;

        const bool windowSilent = count - lastNonZero >= (std::uint64_t) windowFrames();
        if (! (windowSilent && lastFrameSilent))
            frameDirty = true;
        lastFrameSilent = windowSilent;
    }

    if (frameDirty)
    {
        frameDirty = false;
        repaint();
    }
}

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

// Render the static layer (H2): everything that is a pure function of (size,
// physical scale, look). The drawing code is IDENTICAL to what paint() ran
// directly before the cache -- only the destination changed. The image is
// rendered at the destination's PHYSICAL resolution so the blit in paint() is
// a 1:1 device-pixel copy of the exact rasterization the direct draw produced.
// (Exactly 1:1 whenever size x scale is integral -- measured byte-identical;
// at fractional physical sizes the blit takes JUCE's interpolating path and
// AA border pixels can wobble slightly, the setBufferedToImage behaviour.)
void Vectorscope::ensureStaticLayer (juce::Graphics& g, juce::Rectangle<float> area)
{
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (! staticLayer.isNull()
        && staticW == getWidth() && staticH == getHeight()
        && ! (std::abs (staticScale - scale) > 0.0f)) // exact: same idiom as the S4 gate
        return;

    staticW = getWidth();
    staticH = getHeight();
    staticScale = scale;
    // RGB, not ARGB (N2): the layer is fully covered below (corner pre-fill +
    // panel), so it carries no alpha and the per-frame blit is an opaque COPY
    // instead of a per-pixel source-over blend (measured 0.8.9: the ARGB blend
    // was the largest single item of the active default-view GUI profile).
    staticLayer = juce::Image (juce::Image::RGB,
                               juce::jmax (1, juce::roundToInt ((float) staticW * scale)),
                               juce::jmax (1, juce::roundToInt ((float) staticH * scale)),
                               true);
    juce::Graphics ig (staticLayer);
    ig.addTransform (juce::AffineTransform::scale (scale));

    // Corner pre-fill (N2): exactly what the editor's flat fillAll backdrop
    // showed through the rounded corners while this component was translucent.
    // MUST stay in lockstep with the editor's backdrop colour -- if the editor
    // ever paints anything but flat colours::bg beneath the scope/meter row,
    // this component can no longer be opaque.
    ig.fillAll (colours::bg);

    // Background: the 0.5.3 look -- a little brighter at the top, clearly dark
    // toward the bottom (down-dark / up-bright), with just a subtle glass edge so
    // it no longer reads grey and washed out (#1).
    juce::ColourGradient bgGrad (colours::bgPanel.brighter (0.03f), area.getCentreX(), area.getY(),
                                 colours::bg, area.getCentreX(), area.getBottom(), false);
    ig.setGradientFill (bgGrad);
    ig.fillRoundedRectangle (area, 8.0f);
    glass::drawEdges (ig, area, 8.0f, 0.9f);

    const auto plot = area.reduced (18.0f);
    drawGrid (ig, plot, juce::jmin (plot.getWidth(), plot.getHeight()) * 0.5f);
}

void Vectorscope::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    // Static layer (H2): blit the cached background/panel/edges/grid/labels
    // instead of re-rasterizing them at 60 Hz (measured 0.8.8+H1: ~70 % of this
    // component's paint cost, ~2/3 of the active default-view GUI profile).
    // The inverse transform makes the total device mapping identity, so the
    // software renderer performs a straight 1:1 blit -- no resampling.
    ensureStaticLayer (g, area);
    {
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::scale (1.0f / staticScale));
        g.drawImageAt (staticLayer, 0, 0);
    }

    const auto plot = area.reduced (18.0f);
    const float radius = juce::jmin (plot.getWidth(), plot.getHeight()) * 0.5f;
    const auto centre = plot.getCentre();

    // --- read a decimated window from the lock-free ring buffer ---
    const int wantFrames = windowFrames();
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
