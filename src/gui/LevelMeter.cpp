#include "LevelMeter.h"
#include "LookAndFeel.h"
#include <cstring>

namespace anamorph::gui
{

static constexpr float kMinDb = -60.0f, kMaxDb = 0.0f;
static const juce::Colour kClipRed { 0xffe0584a };
static const juce::Colour kRmsOrange { 0xffe0a94a };

LevelMeter::LevelMeter (anamorph::LevelMeters& src) : source (src)
{
    // Default-hidden: the FrameClock is armed on first show (visibilityChanged).

    // Opaque (N2): the cached static layer pre-fills the rounded-panel corners
    // with the editor backdrop colour (flat colours::bg), so paint() covers
    // every pixel -- the parent never re-renders beneath this component and the
    // layer blits as an opaque copy (see Vectorscope for the same pattern).
    // MUST stay in lockstep with the editor's backdrop: if the editor ever
    // paints anything but flat colours::bg beneath the meter strip, this
    // component can no longer be opaque.
    setOpaque (true);
}

LevelMeter::~LevelMeter() { frameClock.stop(); }

void LevelMeter::tick()
{
    if (! isShowing())
        return; // whole-editor hidden: snapshots resume live on re-show (S3)

    // S3 repaint gate: snapshot every published value paint() draws (held
    // peaks, clip colours, RMS numbers, the three bar levels per bar). The
    // audio side settles all of them exactly during silence (envelopes flush
    // to their floors, holds are latched), so a settled meter stops
    // repainting; any change -- audio, decay in flight, or a click's
    // resetHold -- repaints at the full 60 Hz exactly as before. Bitwise
    // comparison: no thresholds, no visual change can ever be skipped.
    const auto& i = source.input;
    const auto& o = source.output;
    const std::array<float, 28> now {
        i.getPeakHoldL(), i.getPeakHoldR(), o.getPeakHoldL(), o.getPeakHoldR(),
        i.getPeakClipL() ? 1.0f : 0.0f, i.getPeakClipR() ? 1.0f : 0.0f,
        o.getPeakClipL() ? 1.0f : 0.0f, o.getPeakClipR() ? 1.0f : 0.0f,
        i.getRmsNumL(), i.getRmsNumR(), o.getRmsNumL(), o.getRmsNumR(),
        i.getRmsClipL() ? 1.0f : 0.0f, i.getRmsClipR() ? 1.0f : 0.0f,
        o.getRmsClipL() ? 1.0f : 0.0f, o.getRmsClipR() ? 1.0f : 0.0f,
        i.getDimL(), i.getDimR(), o.getDimL(), o.getDimR(),
        i.getBriL(), i.getBriR(), o.getBriL(), o.getBriR(),
        i.getBarL(), i.getBarR(), o.getBarL(), o.getBarR() };

    if (! shownValid || std::memcmp (now.data(), shown.data(), sizeof (now)) != 0)
    {
        shown = now;
        shownValid = true;
        repaint();
    }
}

void LevelMeter::visibilityChanged()
{
    // Own-visibility lifecycle (S3): the meter is default-hidden (Show Meters
    // toggle), so its display-rate FrameClock runs only while it is shown --
    // no vblank wakeups at all while hidden, exactly like the old stopTimer().
    if (isVisible())
    {
        shownValid = false;
        frameClock.start (*this, [this] (double) { tick(); });
    }
    else
        frameClock.stop();
}

// Non-uniform scale tuned for mixing: the busy -24..0 dBFS range gets most of
// the bar, the quiet tail is compressed (#17).
static float dbToNorm (float db)
{
    if (! std::isfinite (db)) db = kMinDb; // never propagate a NaN into the bar geometry (Issue 8)
    const float t = juce::jlimit (0.0f, 1.0f, (db - kMinDb) / (kMaxDb - kMinDb));
    return std::pow (t, 2.0f);
}

static juce::String dbText (float db)
{
    if (db <= kMinDb + 0.5f) return juce::String ("-") + juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e"));
    return juce::String (db, 1);
}

void LevelMeter::drawNumber (juce::Graphics& g, juce::Rectangle<float> r, float valueDb,
                             bool peak, bool clip)
{
    // Peak (red on clip) and RMS (amber on clip) share the same small size (#11).
    juce::Colour col = peak ? (clip ? kClipRed : colours::text)
                            : (clip ? kRmsOrange : colours::textDim);
    g.setColour (col);
    g.setFont (juce::Font (juce::FontOptions (10.5f)));
    g.drawText (dbText (valueDb), r, juce::Justification::centred);
}

// Slot geometry shared by the static layer (slot background + ticks) and the
// live fills: identical to the former single drawBar.
static constexpr float kBarRad = 3.0f;

// The signal-dependent half of the former drawBar (Wave 4): the fills and the
// peak block, clipped to the same rounded track as before. The slot background
// + ticks live in the cached static layer; the glass edges are drawn by
// paint() AFTER this, preserving the original draw order exactly.
void LevelMeter::drawBarDynamic (juce::Graphics& g, juce::Rectangle<float> r,
                                 float dimDb, float briDb, float barDb)
{
    const float rad = kBarRad;
    auto track = r.reduced (1.6f);

    const float dimN = dbToNorm (dimDb);
    const float briN = dbToNorm (briDb);
    const float barN = dbToNorm (barDb);

    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip; clip.addRoundedRectangle (track, rad - 1.0f);
        g.reduceClipRegion (clip);

        // Dim fast-riser, sitting above the body (translucent, soft). A touch
        // brighter so it reads clearly against the body (#15).
        auto dimFill = track.withTop (track.getBottom() - dimN * track.getHeight());
        juce::ColourGradient dg (colours::accent2.withAlpha (0.16f), dimFill.getX(), dimFill.getBottom(),
                                 colours::accent2.withAlpha (0.46f), dimFill.getX(), dimFill.getY(), false);
        g.setGradientFill (dg);
        g.fillRect (dimFill);

        // Bright VU body: a 3-stop vertical gradient (deep -> bright, warming to
        // red near 0 dBFS) with a glossy centre sheen and a crisp bright cap (#10).
        const float warm = juce::jlimit (0.0f, 1.0f, (briDb + 6.0f) / 6.0f);
        auto base = colours::accent.interpolatedWith (kClipRed, warm);
        auto fill = track.withTop (track.getBottom() - briN * track.getHeight());
        juce::ColourGradient fg (base.darker (0.22f), fill.getX(), fill.getBottom(),
                                 base.brighter (0.30f), fill.getX(), fill.getY(), false);
        fg.addColour (0.5, base.brighter (0.05f));
        g.setGradientFill (fg);
        g.fillRect (fill);
        // vertical glossy sheen down the centre
        auto sheen = fill.withWidth (fill.getWidth() * 0.34f).withX (fill.getCentreX() - fill.getWidth() * 0.17f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRect (sheen);
        if (briN > 0.01f) // crisp bright cap + glow
        {
            g.setColour (base.brighter (0.55f));
            g.fillRect (fill.getX(), fill.getY(), fill.getWidth(), 1.4f);
            g.setColour (base.withAlpha (0.30f));
            g.fillRect (fill.getX(), fill.getY() - 2.0f, fill.getWidth(), 2.0f);
        }

        // Peak-hold block: rounded bright block with a soft glow, red past 0 dBFS.
        const float py = track.getBottom() - barN * track.getHeight();
        const bool over = barDb > -0.05f;
        const auto pc = over ? kClipRed : colours::text;
        g.setColour (pc.withAlpha (0.30f));
        g.fillRoundedRectangle (track.getX() - 0.5f, py - 3.0f, track.getWidth() + 1.0f, 6.0f, 2.0f);
        g.setColour (pc.brighter (0.1f));
        g.fillRoundedRectangle (track.getX(), py - 1.6f, track.getWidth(), 3.2f, 1.4f);
    }
}

// The former in-paint layout arithmetic, verbatim: same removeFromTop sequence,
// same column / bar / ruler expressions (#18/#16). Pure function of the size.
LevelMeter::Layout LevelMeter::computeLayout() const noexcept
{
    Layout l;
    l.bounds = getLocalBounds().toFloat();
    auto area = l.bounds.reduced (5.0f);

    // Symmetric layout: the dB ruler sits in the MIDDLE, with the IN pair (L,R) to
    // its left and the OUT pair (L,R) to its right, so the whole module is mirror-
    // balanced (#18). Numbers and bars share the same column geometry.
    const float rulerW = 22.0f;
    const float pairW  = (area.getWidth() - rulerW) * 0.5f;
    l.colW = pairW * 0.5f;
    l.ruler = area.withX (area.getX() + pairW).withWidth (rulerW);
    for (int i = 0; i < 4; ++i)
        l.colXs[i] = (i < 2) ? area.getX() + (float) i * l.colW
                             : area.getX() + pairW + rulerW + (float) (i - 2) * l.colW;

    l.header = area.removeFromTop (11.0f);
    l.sub    = area.removeFromTop (10.0f);
    l.pkRow  = area.removeFromTop (14.0f);
    l.rmRow  = area.removeFromTop (13.0f);
    area.removeFromTop (3.0f);
    l.bars   = area;

    const float gap = 6.0f;
    l.barW = juce::jmin (14.0f, l.colW - gap);
    return l;
}

// Render the static layer (Wave 4 -- the H13/H2 recipe): everything that is a
// pure function of (size, physical scale, look) -- the glass panel, the IN/OUT
// and L/R headers, the four recessed bar slots with their faint ticks, and the
// centre dB ruler. The drawing statements are IDENTICAL to what paint() ran
// directly before the cache; the image is rendered at the destination's
// PHYSICAL resolution so the blit in paint() is a 1:1 device-pixel copy. The
// ruler never overlaps a bar or a number (the bars are inset >= 3 px from the
// ruler column and the ticks reach only 0.5 px past its edge), so hoisting it
// beneath the live drawing cannot change the composition; the bar glass edges
// are NOT here -- they stay live in paint() to keep their order over the fills.
void LevelMeter::ensureStaticLayer (juce::Graphics& g, const Layout& l)
{
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (! staticLayer.isNull()
        && staticW == getWidth() && staticH == getHeight()
        && ! (std::abs (staticScale - scale) > 0.0f)) // exact: same idiom as the S4 gate
        return;

    staticW = getWidth();
    staticH = getHeight();
    staticScale = scale;
    // RGB + corner pre-fill (N2): same pattern and same editor-backdrop
    // coupling as the Vectorscope / StereoMeter static layers.
    staticLayer = juce::Image (juce::Image::RGB,
                               juce::jmax (1, juce::roundToInt ((float) staticW * scale)),
                               juce::jmax (1, juce::roundToInt ((float) staticH * scale)),
                               true);
    juce::Graphics ig (staticLayer);
    ig.addTransform (juce::AffineTransform::scale (scale));
    ig.fillAll (colours::bg);

    glass::fillPanel (ig, l.bounds, 4.0f, colours::bgPanel, 0.85f); // gentle 0.5.3-style frame (#1)

    // ---- header: IN | OUT (each centred over its pair) ----
    ig.setColour (colours::textDim);
    ig.setFont (juce::Font (juce::FontOptions (9.0f)).withExtraKerningFactor (0.15f));
    ig.drawText ("IN",  l.header.withX (l.colXs[0]).withWidth (l.colXs[1] + l.colW - l.colXs[0]),
                 juce::Justification::centred);
    ig.drawText ("OUT", l.header.withX (l.colXs[2]).withWidth (l.colXs[3] + l.colW - l.colXs[2]),
                 juce::Justification::centred);

    // ---- L / R sub-header ----
    ig.setColour (colours::textDim.withAlpha (0.8f));
    ig.setFont (juce::Font (juce::FontOptions (8.5f)));
    const char* lr[] = { "L", "R", "L", "R" };
    for (int i = 0; i < 4; ++i)
        ig.drawText (lr[i], l.sub.withX (l.colXs[i]).withWidth (l.colW), juce::Justification::centred);

    // ---- the four recessed bar slots (background gradient + faint ticks) ----
    for (int i = 0; i < 4; ++i)
    {
        const auto r = l.bar (i);
        // Recessed slot: lighter at the top, noticeably DARKER toward the bottom
        // for a richer gradient (#14).
        juce::ColourGradient bgGrad (colours::bg.brighter (0.06f), r.getX(), r.getY(),
                                     juce::Colour (0xff04060a),     r.getX(), r.getBottom(), false);
        bgGrad.addColour (0.45, colours::bg.darker (0.25f));
        ig.setGradientFill (bgGrad);
        ig.fillRoundedRectangle (r, kBarRad);

        auto track = r.reduced (1.6f);
        juce::Graphics::ScopedSaveState save (ig);
        juce::Path clip; clip.addRoundedRectangle (track, kBarRad - 1.0f);
        ig.reduceClipRegion (clip);

        // Very subtle tick marks (kept faint so the bar doesn't look gridded/cheap).
        ig.setColour (colours::outline.withAlpha (0.22f));
        for (float gl : { 0.0f, -6.0f, -18.0f })
            ig.fillRect (track.getX(), track.getBottom() - dbToNorm (gl) * track.getHeight(), track.getWidth(), 1.0f);
    }

    // ---- centred non-uniform dB scale, ticks reaching toward both pairs (#18) ----
    const float trackH = l.bars.getHeight() - 3.2f;
    ig.setFont (juce::Font (juce::FontOptions (8.0f)));
    for (int db : { 0, -6, -12, -18, -24, -48 })
    {
        const float y = l.bars.getBottom() - 1.6f - dbToNorm ((float) db) * trackH;
        ig.setColour (colours::outline.brighter (0.1f));
        ig.fillRect (l.ruler.getX() - 0.5f,     y - 0.5f, 2.5f, 1.0f); // tick toward IN
        ig.fillRect (l.ruler.getRight() - 2.0f, y - 0.5f, 2.5f, 1.0f); // tick toward OUT
        ig.setColour (colours::textDim.withAlpha (0.85f));
        ig.drawText (juce::String (db), l.ruler.withY (y - 6.0f).withHeight (12.0f),
                     juce::Justification::centred);
    }
}

void LevelMeter::paint (juce::Graphics& g)
{
    const auto l = computeLayout();

    // Static layer: blit the cached panel/headers/slots/ruler instead of
    // re-rasterizing them on every meter frame (Wave 4; the H2/H13 measured
    // precedent -- glass::fillPanel alone was the largest single item of the
    // StereoMeter's active paint profile before its cache).
    ensureStaticLayer (g, l);
    {
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::scale (1.0f / staticScale));
        g.drawImageAt (staticLayer, 0, 0);
    }

    // ---- Peak row (8 numbers total with the RMS row, #17) ----
    auto numRect = [&] (int i, juce::Rectangle<float> row, float h)
    { return juce::Rectangle<float> (l.colXs[i], row.getY(), l.colW, h); };
    drawNumber (g, numRect (0, l.pkRow, 14.0f), source.input.getPeakHoldL(),  true, source.input.getPeakClipL());
    drawNumber (g, numRect (1, l.pkRow, 14.0f), source.input.getPeakHoldR(),  true, source.input.getPeakClipR());
    drawNumber (g, numRect (2, l.pkRow, 14.0f), source.output.getPeakHoldL(), true, source.output.getPeakClipL());
    drawNumber (g, numRect (3, l.pkRow, 14.0f), source.output.getPeakHoldR(), true, source.output.getPeakClipR());

    // ---- RMS row ----
    drawNumber (g, numRect (0, l.rmRow, 13.0f), source.input.getRmsNumL(),  false, source.input.getRmsClipL());
    drawNumber (g, numRect (1, l.rmRow, 13.0f), source.input.getRmsNumR(),  false, source.input.getRmsClipR());
    drawNumber (g, numRect (2, l.rmRow, 13.0f), source.output.getRmsNumL(), false, source.output.getRmsClipL());
    drawNumber (g, numRect (3, l.rmRow, 13.0f), source.output.getRmsNumR(), false, source.output.getRmsClipR());

    // ---- four thin bars, in the same two grouped pairs as the numbers (#16):
    // live fills + peak blocks over the cached slots, then the glass frames on
    // top -- the same per-bar order as before (bars never overlap each other).
    drawBarDynamic (g, l.bar (0), source.input.getDimL(),  source.input.getBriL(),  source.input.getBarL());
    drawBarDynamic (g, l.bar (1), source.input.getDimR(),  source.input.getBriR(),  source.input.getBarR());
    drawBarDynamic (g, l.bar (2), source.output.getDimL(), source.output.getBriL(), source.output.getBarL());
    drawBarDynamic (g, l.bar (3), source.output.getDimR(), source.output.getBriR(), source.output.getBarR());

    // Crisp glass frame on top (after the clipped fills) -- top-left / bottom-right
    // highlight edges to match the panels (#17).
    for (int i = 0; i < 4; ++i)
        glass::drawEdges (g, l.bar (i), kBarRad, 0.8f);
}

} // namespace anamorph::gui
