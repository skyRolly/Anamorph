#include "LevelMeter.h"
#include "LookAndFeel.h"

namespace anamorph::gui
{

static constexpr float kMinDb = -60.0f, kMaxDb = 0.0f;
static const juce::Colour kClipRed { 0xffe0584a };
static const juce::Colour kRmsOrange { 0xffe0a94a };

LevelMeter::LevelMeter (anamorph::LevelMeters& src) : source (src)
{
    startTimerHz (30);
}

LevelMeter::~LevelMeter() { stopTimer(); }

// Non-uniform scale tuned for mixing: the busy -24..0 dBFS range gets most of
// the bar, the quiet tail is compressed (#17).
static float dbToNorm (float db)
{
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

void LevelMeter::drawBar (juce::Graphics& g, juce::Rectangle<float> r,
                          float dimDb, float briDb, float barDb)
{
    const float rad = 3.0f;
    // Recessed slot: lighter at the top, noticeably DARKER toward the bottom for a
    // richer gradient (#14).
    juce::ColourGradient bgGrad (colours::bg.brighter (0.06f), r.getX(), r.getY(),
                                 juce::Colour (0xff04060a),     r.getX(), r.getBottom(), false);
    bgGrad.addColour (0.45, colours::bg.darker (0.25f));
    g.setGradientFill (bgGrad);
    g.fillRoundedRectangle (r, rad);

    auto track = r.reduced (1.6f);

    const float dimN = dbToNorm (dimDb);
    const float briN = dbToNorm (briDb);
    const float barN = dbToNorm (barDb);

    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip; clip.addRoundedRectangle (track, rad - 1.0f);
        g.reduceClipRegion (clip);

        // Very subtle tick marks (kept faint so the bar doesn't look gridded/cheap).
        g.setColour (colours::outline.withAlpha (0.22f));
        for (float gl : { 0.0f, -6.0f, -18.0f })
            g.fillRect (track.getX(), track.getBottom() - dbToNorm (gl) * track.getHeight(), track.getWidth(), 1.0f);

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

    // Crisp glass frame on top (after the clipped fills) -- top-left / bottom-right
    // highlight edges to match the panels (#17).
    glass::drawEdges (g, r, rad, 0.8f);
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    glass::fillPanel (g, bounds, 4.0f, colours::bgPanel); // iOS-glass frame (#17)

    auto area = bounds.reduced (5.0f);

    // Symmetric layout: the dB ruler sits in the MIDDLE, with the IN pair (L,R) to
    // its left and the OUT pair (L,R) to its right, so the whole module is mirror-
    // balanced (#18). Numbers and bars share the same column geometry.
    const float rulerW = 22.0f;
    const float pairW  = (area.getWidth() - rulerW) * 0.5f;
    const float colW   = pairW * 0.5f;
    auto ruler = area.withX (area.getX() + pairW).withWidth (rulerW);
    auto colX = [&] (int i)
    {
        const float x = (i < 2) ? area.getX() + i * colW
                                : area.getX() + pairW + rulerW + (i - 2) * colW;
        return area.withX (x).withWidth (colW);
    };

    // ---- header: IN | OUT (each centred over its pair) ----
    auto header = area.removeFromTop (11.0f);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (juce::FontOptions (9.0f)).withExtraKerningFactor (0.15f));
    g.drawText ("IN",  header.withX (colX (0).getX()).withWidth (colX (1).getRight() - colX (0).getX()),
                juce::Justification::centred);
    g.drawText ("OUT", header.withX (colX (2).getX()).withWidth (colX (3).getRight() - colX (2).getX()),
                juce::Justification::centred);

    // ---- L / R sub-header ----
    auto sub = area.removeFromTop (10.0f);
    g.setColour (colours::textDim.withAlpha (0.8f));
    g.setFont (juce::Font (juce::FontOptions (8.5f)));
    const char* lr[] = { "L", "R", "L", "R" };
    for (int i = 0; i < 4; ++i)
        g.drawText (lr[i], sub.withX (colX (i).getX()).withWidth (colW), juce::Justification::centred);

    // ---- Peak row (8 numbers total with the RMS row, #17) ----
    auto pkRow = area.removeFromTop (14.0f);
    drawNumber (g, colX (0).withY (pkRow.getY()).withHeight (14.0f), source.input.getPeakHoldL(),  true, source.input.getPeakClipL());
    drawNumber (g, colX (1).withY (pkRow.getY()).withHeight (14.0f), source.input.getPeakHoldR(),  true, source.input.getPeakClipR());
    drawNumber (g, colX (2).withY (pkRow.getY()).withHeight (14.0f), source.output.getPeakHoldL(), true, source.output.getPeakClipL());
    drawNumber (g, colX (3).withY (pkRow.getY()).withHeight (14.0f), source.output.getPeakHoldR(), true, source.output.getPeakClipR());

    // ---- RMS row ----
    auto rmRow = area.removeFromTop (13.0f);
    drawNumber (g, colX (0).withY (rmRow.getY()).withHeight (13.0f), source.input.getRmsNumL(),  false, source.input.getRmsClipL());
    drawNumber (g, colX (1).withY (rmRow.getY()).withHeight (13.0f), source.input.getRmsNumR(),  false, source.input.getRmsClipR());
    drawNumber (g, colX (2).withY (rmRow.getY()).withHeight (13.0f), source.output.getRmsNumL(), false, source.output.getRmsClipL());
    drawNumber (g, colX (3).withY (rmRow.getY()).withHeight (13.0f), source.output.getRmsNumR(), false, source.output.getRmsClipR());

    area.removeFromTop (3.0f);

    // ---- four thin bars, in the same two grouped pairs as the numbers (#16) ----
    const float gap = 6.0f;
    const float bw = juce::jmin (14.0f, colW - gap);
    auto bar = [&] (int i) { return colX (i).withX (colX (i).getX() + (colW - bw) * 0.5f).withWidth (bw); };
    drawBar (g, bar (0), source.input.getDimL(),  source.input.getBriL(),  source.input.getBarL());
    drawBar (g, bar (1), source.input.getDimR(),  source.input.getBriR(),  source.input.getBarR());
    drawBar (g, bar (2), source.output.getDimL(), source.output.getBriL(), source.output.getBarL());
    drawBar (g, bar (3), source.output.getDimR(), source.output.getBriR(), source.output.getBarR());

    // ---- centred non-uniform dB scale, ticks reaching toward both pairs (#18) ----
    const float trackH = area.getHeight() - 3.2f;
    g.setFont (juce::Font (juce::FontOptions (8.0f)));
    for (int db : { 0, -6, -12, -18, -24, -48 })
    {
        const float y = area.getBottom() - 1.6f - dbToNorm ((float) db) * trackH;
        g.setColour (colours::outline.brighter (0.1f));
        g.fillRect (ruler.getX() - 0.5f,        y - 0.5f, 2.5f, 1.0f); // tick toward IN
        g.fillRect (ruler.getRight() - 2.0f,    y - 0.5f, 2.5f, 1.0f); // tick toward OUT
        g.setColour (colours::textDim.withAlpha (0.85f));
        g.drawText (juce::String (db), ruler.withY (y - 6.0f).withHeight (12.0f),
                    juce::Justification::centred);
    }
}

} // namespace anamorph::gui
