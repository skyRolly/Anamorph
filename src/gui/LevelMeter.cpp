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

// Non-uniform scale: more pixels-per-dB near 0 dBFS (#17).
static float dbToNorm (float db)
{
    const float t = juce::jlimit (0.0f, 1.0f, (db - kMinDb) / (kMaxDb - kMinDb));
    return std::pow (t, 1.7f);
}

static juce::String dbText (float db)
{
    if (db <= kMinDb + 0.5f) return juce::String ("-") + juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e"));
    return juce::String (db, 1);
}

void LevelMeter::drawNumber (juce::Graphics& g, juce::Rectangle<float> r, float valueDb,
                             bool peak, bool clip)
{
    // Peak is bright (red when it has clipped); RMS is dimmer (orange on clip) (#14).
    juce::Colour col = peak ? (clip ? kClipRed : colours::text)
                            : (clip ? kRmsOrange : colours::textDim);
    g.setColour (col);
    g.setFont (juce::Font (juce::FontOptions (peak ? 11.5f : 10.5f)));
    g.drawText (dbText (valueDb), r, juce::Justification::centred);
}

void LevelMeter::drawBar (juce::Graphics& g, juce::Rectangle<float> r,
                          float dimDb, float briDb, float barDb)
{
    // Recessed slot with an inner gradient (#22).
    juce::ColourGradient bgGrad (colours::bg.darker (0.3f), r.getX(), r.getY(),
                                 colours::bgPanel, r.getX(), r.getBottom(), false);
    g.setGradientFill (bgGrad);
    g.fillRoundedRectangle (r, 2.5f);
    g.setColour (colours::outline.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), 2.5f, 1.0f);

    auto track = r.reduced (1.5f);

    // Faint non-uniform gridlines.
    g.setColour (colours::outline.withAlpha (0.4f));
    for (float gl : { 0.0f, -6.0f, -18.0f, -36.0f })
        g.fillRect (track.getX(), track.getBottom() - dbToNorm (gl) * track.getHeight(), track.getWidth(), 1.0f);

    const float dimN = dbToNorm (dimDb);
    const float briN = dbToNorm (briDb);
    const float barN = dbToNorm (barDb);

    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (track.toNearestInt());

    // Dim fast-rise/slow-fall envelope rides above the bright body (#15/#18).
    g.setColour (colours::accent2.withAlpha (0.30f));
    g.fillRoundedRectangle (track.withTop (track.getBottom() - dimN * track.getHeight()), 1.5f);

    // Bright RMS body: gradient that warms toward 0 dBFS, with a soft top glow.
    const float warm = juce::jlimit (0.0f, 1.0f, (briDb + 6.0f) / 6.0f);
    auto col = colours::accent.interpolatedWith (kClipRed, warm);
    auto fill = track.withTop (track.getBottom() - briN * track.getHeight());
    juce::ColourGradient fg (col.darker (0.10f), fill.getX(), fill.getBottom(),
                             col.brighter (0.28f), fill.getX(), fill.getY(), false);
    g.setGradientFill (fg);
    g.fillRoundedRectangle (fill, 1.5f);
    g.setColour (col.brighter (0.4f).withAlpha (0.5f)); // top edge highlight
    g.fillRect (fill.getX(), fill.getY(), fill.getWidth(), 1.0f);

    // Peak-hold block (#24): a small bright block with a glow, red past 0 dBFS.
    const float py = track.getBottom() - barN * track.getHeight();
    const bool over = barDb > -0.05f;
    g.setColour ((over ? kClipRed : colours::text).withAlpha (0.30f));
    g.fillRect (track.getX(), py - 3.0f, track.getWidth(), 6.0f);
    g.setColour (over ? kClipRed : colours::text);
    g.fillRoundedRectangle (track.getX(), py - 1.5f, track.getWidth(), 3.0f, 1.0f);
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::bgPanel);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    auto area = bounds.reduced (5.0f);
    const float colW = area.getWidth() / 4.0f;
    auto colX = [&] (int i) { return area.withX (area.getX() + i * colW).withWidth (colW); };

    // ---- header: IN | OUT ----
    auto header = area.removeFromTop (11.0f);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (juce::FontOptions (9.0f)).withExtraKerningFactor (0.15f));
    g.drawText ("IN",  header.removeFromLeft (area.getWidth() * 0.5f), juce::Justification::centred);
    g.drawText ("OUT", header, juce::Justification::centred);

    // ---- L / R sub-header ----
    auto sub = area.removeFromTop (10.0f);
    g.setColour (colours::textDim.withAlpha (0.8f));
    g.setFont (juce::Font (juce::FontOptions (8.5f)));
    const char* lr[] = { "L", "R", "L", "R" };
    for (int i = 0; i < 4; ++i)
        g.drawText (lr[i], sub.withX (area.getX() + i * colW).withWidth (colW), juce::Justification::centred);

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

    // ---- four thin bars under the columns ----
    const float gap = 6.0f;
    const float bw = juce::jmin (14.0f, colW - gap);
    auto bar = [&] (int i) { return area.withX (area.getX() + i * colW + (colW - bw) * 0.5f).withWidth (bw); };
    drawBar (g, bar (0), source.input.getDimL(),  source.input.getBriL(),  source.input.getBarL());
    drawBar (g, bar (1), source.input.getDimR(),  source.input.getBriR(),  source.input.getBarR());
    drawBar (g, bar (2), source.output.getDimL(), source.output.getBriL(), source.output.getBarL());
    drawBar (g, bar (3), source.output.getDimR(), source.output.getBriR(), source.output.getBarR());
}

} // namespace anamorph::gui
