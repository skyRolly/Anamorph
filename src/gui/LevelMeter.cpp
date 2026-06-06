#include "LevelMeter.h"
#include "LookAndFeel.h"

namespace anamorph::gui
{

static constexpr float kMinDb = -60.0f, kMaxDb = 0.0f;

LevelMeter::LevelMeter (anamorph::LevelMeters& src) : source (src)
{
    startTimerHz (30);
}

LevelMeter::~LevelMeter() { stopTimer(); }

// Non-uniform scale (#17): more pixels-per-dB near 0 dBFS, where mastering work
// happens, and a compressed tail toward -60. A gentle power curve does this
// smoothly without piecewise kinks.
static float dbToNorm (float db)
{
    const float t = juce::jlimit (0.0f, 1.0f, (db - kMinDb) / (kMaxDb - kMinDb));
    return std::pow (t, 1.7f);
}

static juce::String dbText (float db)
{
    if (db <= kMinDb + 0.5f) return juce::String ("-") + juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x9e")); // -inf
    return juce::String (db, 1);
}

void LevelMeter::drawReadout (juce::Graphics& g, juce::Rectangle<float> r,
                              const juce::String& lab, float valueDb, bool dim)
{
    // Label (always dim) + value. RMS uses a dimmer value colour than Peak (#17).
    g.setColour (colours::textDim.withAlpha (0.8f));
    g.setFont (juce::Font (juce::FontOptions (9.5f)).withExtraKerningFactor (0.06f));
    g.drawText (lab, r.removeFromLeft (30.0f), juce::Justification::centredLeft);

    g.setColour (dim ? colours::textDim : colours::text);
    g.setFont (juce::Font (juce::FontOptions (12.5f)));
    g.drawText (dbText (valueDb), r, juce::Justification::centredRight);
}

void LevelMeter::drawBar (juce::Graphics& g, juce::Rectangle<float> r,
                          float fastDb, float rmsDb, float peakDb, const juce::String& lab)
{
    g.setColour (colours::bg);
    g.fillRoundedRectangle (r, 2.0f);

    auto track = r.reduced (1.0f).withTrimmedBottom (12.0f);

    // Faint non-uniform gridlines so the eye reads the scale (#17).
    g.setColour (colours::outline.withAlpha (0.45f));
    for (float gl : { 0.0f, -3.0f, -6.0f, -12.0f, -24.0f, -48.0f })
    {
        const float y = track.getBottom() - dbToNorm (gl) * track.getHeight();
        g.fillRect (track.getX(), y, track.getWidth(), 1.0f);
    }

    const float fastN = dbToNorm (fastDb);
    const float rmsN  = dbToNorm (rmsDb);
    const float peakN = dbToNorm (peakDb);

    // Fast/dim layer: a quicker envelope that rides ABOVE the steady RMS, drawn
    // FIRST (taller) so it shows over the bright body, not hidden behind it (#15).
    g.setColour (colours::accent2.withAlpha (0.32f));
    g.fillRect (track.withTop (track.getBottom() - fastN * track.getHeight()));

    // Slow/bright RMS: the steady body, warming toward 0 dBFS, with a soft
    // vertical gradient consistent with the rest of the UI (#18). Drawn on top.
    const float topWarn = juce::jlimit (0.0f, 1.0f, (peakDb + 6.0f) / 6.0f);
    auto col = colours::accent.interpolatedWith (juce::Colour (0xffd8584a), topWarn);
    auto fill = track.withTop (track.getBottom() - rmsN * track.getHeight());
    juce::ColourGradient grad (col.withAlpha (0.95f), fill.getX(), fill.getBottom(),
                               col.brighter (0.25f).withAlpha (0.95f), fill.getX(), fill.getY(), false);
    g.setGradientFill (grad);
    g.fillRect (fill);

    // Peak: a small bright block (not a hairline) sitting on top (#18).
    const float py = track.getBottom() - peakN * track.getHeight();
    const bool clip = peakDb > -0.1f;
    g.setColour (clip ? juce::Colour (0xffe0584a) : colours::text);
    g.fillRect (track.getX(), juce::jmax (track.getY(), py - 2.0f), track.getWidth(), 3.0f);
    if (clip) // glow when slamming 0 dBFS
    {
        g.setColour (juce::Colour (0xffe0584a).withAlpha (0.35f));
        g.fillRect (track.getX(), juce::jmax (track.getY(), py - 3.5f), track.getWidth(), 6.0f);
    }

    g.setColour (colours::textDim);
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.drawText (lab, r.withTop (r.getBottom() - 12.0f), juce::Justification::centred);
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (colours::bgPanel);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    auto area = bounds.reduced (5.0f);

    // Fixed-position numeric Peak / RMS readouts for the OUTPUT (#17): Peak is
    // the louder channel's peak; RMS the louder channel's fast RMS (dimmer).
    const float outPeak = juce::jmax (source.output.getPeakL(), source.output.getPeakR());
    const float outRms  = juce::jmax (source.output.getRmsL(),  source.output.getRmsR());
    drawReadout (g, area.removeFromTop (15.0f), "Peak", outPeak, false);
    drawReadout (g, area.removeFromTop (15.0f), "RMS",  outRms,  true);
    area.removeFromTop (3.0f);

    // Header labels IN | OUT
    auto header = area.removeFromTop (11.0f);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (juce::FontOptions (9.0f)).withExtraKerningFactor (0.15f));
    g.drawText ("IN",  header.removeFromLeft (header.getWidth() * 0.5f), juce::Justification::centred);
    g.drawText ("OUT", header, juce::Justification::centred);

    const float gap = 3.0f;
    const float bw = (area.getWidth() - 3.0f * gap) / 4.0f;
    auto next = [&] { auto b = area.removeFromLeft (bw); area.removeFromLeft (gap); return b; };

    drawBar (g, next(), source.input.getFastL(),  source.input.getRmsL(),  source.input.getPeakL(),  "L");
    drawBar (g, next(), source.input.getFastR(),  source.input.getRmsR(),  source.input.getPeakR(),  "R");
    drawBar (g, next(), source.output.getFastL(), source.output.getRmsL(), source.output.getPeakL(), "L");
    drawBar (g, area,   source.output.getFastR(), source.output.getRmsR(), source.output.getPeakR(), "R");
}

} // namespace anamorph::gui
