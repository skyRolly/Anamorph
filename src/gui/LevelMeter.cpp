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

static float dbToNorm (float db)
{
    return juce::jlimit (0.0f, 1.0f, (db - kMinDb) / (kMaxDb - kMinDb));
}

void LevelMeter::drawBar (juce::Graphics& g, juce::Rectangle<float> r,
                          float rmsDb, float peakDb, const juce::String& lab)
{
    g.setColour (colours::bg);
    g.fillRoundedRectangle (r, 2.0f);

    auto track = r.reduced (1.0f).withTrimmedBottom (12.0f);
    const float rmsN  = dbToNorm (rmsDb);
    const float peakN = dbToNorm (peakDb);

    // RMS fill: accent that warms toward the top, red near 0 dBFS.
    const float topWarn = juce::jlimit (0.0f, 1.0f, (peakDb + 6.0f) / 6.0f);
    auto col = colours::accent.interpolatedWith (juce::Colour (0xffd8584a), topWarn);
    g.setColour (col.withAlpha (0.9f));
    g.fillRect (track.withTop (track.getBottom() - rmsN * track.getHeight()));

    // Peak tick
    const float py = track.getBottom() - peakN * track.getHeight();
    g.setColour (peakDb > -0.1f ? juce::Colour (0xffe0584a) : colours::text);
    g.fillRect (track.getX(), py - 1.0f, track.getWidth(), 1.5f);

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
    // Header labels IN | OUT
    auto header = area.removeFromTop (12.0f);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (juce::FontOptions (9.0f)).withExtraKerningFactor (0.15f));
    g.drawText ("IN",  header.removeFromLeft (header.getWidth() * 0.5f), juce::Justification::centred);
    g.drawText ("OUT", header, juce::Justification::centred);

    const float gap = 3.0f;
    const float bw = (area.getWidth() - 3.0f * gap) / 4.0f;
    auto next = [&] { auto b = area.removeFromLeft (bw); area.removeFromLeft (gap); return b; };

    drawBar (g, next(), source.input.getRmsL(),  source.input.getPeakL(),  "L");
    drawBar (g, next(), source.input.getRmsR(),  source.input.getPeakR(),  "R");
    drawBar (g, next(), source.output.getRmsL(), source.output.getPeakL(), "L");
    drawBar (g, area,   source.output.getRmsR(), source.output.getPeakR(), "R");
}

} // namespace anamorph::gui
