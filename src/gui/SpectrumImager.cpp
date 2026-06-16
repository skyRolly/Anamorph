#include "SpectrumImager.h"
#include "LookAndFeel.h"
#include "PluginParameters.h"

namespace anamorph::gui
{

static constexpr float kFreqLo = 20.0f, kFreqHi = 20000.0f;
static constexpr float kMinDb = -90.0f, kMaxDb = 0.0f;

SpectrumImager::SpectrumImager (anamorph::ScopeBuffer& s, juce::AudioProcessorValueTreeState& a)
    : scope (s), apvts (a)
{
    freqP[0]  = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (pid::mbFreqLow));
    freqP[1]  = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (pid::mbFreqMid));
    freqP[2]  = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (pid::mbFreqHigh));
    widthP[0] = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (pid::mbWidthLow));
    widthP[1] = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (pid::mbWidthMid));
    widthP[2] = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (pid::mbWidthHiMid));
    widthP[3] = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (pid::mbWidthHigh));

    fifoL.assign ((size_t) fftSize, 0.0f);
    fifoR.assign ((size_t) fftSize, 0.0f);
    fftData.assign ((size_t) fftSize * 2, 0.0f);
    mags.assign ((size_t) fftSize / 2 + 1, kMinDb);

    setInterceptsMouseClicks (true, false);
    startTimerHz (30);
}

SpectrumImager::~SpectrumImager() { stopTimer(); }

// ----------------------------------------------------------------------------
float SpectrumImager::crossover (int i) const noexcept
{
    if (auto* p = freqP[i]) return p->convertFrom0to1 (p->getValue());
    return kFreqLo;
}

float SpectrumImager::bandWidth (int i) const noexcept
{
    if (auto* p = widthP[i]) return p->convertFrom0to1 (p->getValue());
    return 1.0f;
}

juce::Rectangle<float> SpectrumImager::plot() const noexcept
{
    return getLocalBounds().toFloat().reduced (1.0f);
}

float SpectrumImager::freqToX (float hz) const noexcept
{
    auto r = plot();
    const float t = std::log (juce::jlimit (kFreqLo, kFreqHi, hz) / kFreqLo) / std::log (kFreqHi / kFreqLo);
    return r.getX() + t * r.getWidth();
}

float SpectrumImager::xToFreq (float x) const noexcept
{
    auto r = plot();
    const float t = juce::jlimit (0.0f, 1.0f, (x - r.getX()) / r.getWidth());
    return kFreqLo * std::pow (kFreqHi / kFreqLo, t);
}

float SpectrumImager::widthToY (float w) const noexcept
{
    auto r = plot();
    return r.getY() + (1.0f - juce::jlimit (0.0f, 2.0f, w) * 0.5f) * r.getHeight();
}

float SpectrumImager::yToWidth (float y) const noexcept
{
    auto r = plot();
    const float t = juce::jlimit (0.0f, 1.0f, (y - r.getY()) / r.getHeight());
    return (1.0f - t) * 2.0f;
}

int SpectrumImager::bandAtX (float x) const noexcept
{
    const float f = xToFreq (x);
    if (f < crossover (0)) return 0;
    if (f < crossover (1)) return 1;
    if (f < crossover (2)) return 2;
    return 3;
}

int SpectrumImager::handleNearX (float x) const noexcept
{
    int best = -1; float bestD = 7.0f; // px tolerance
    for (int i = 0; i < 3; ++i)
    {
        const float d = std::abs (x - freqToX (crossover (i)));
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

// ----------------------------------------------------------------------------
void SpectrumImager::beginGesture (juce::RangedAudioParameter* p) { if (p) p->beginChangeGesture(); }
void SpectrumImager::endGesture   (juce::RangedAudioParameter* p) { if (p) p->endChangeGesture(); }
void SpectrumImager::setParam (juce::RangedAudioParameter* p, float plain)
{
    if (p) p->setValueNotifyingHost (p->convertTo0to1 (plain));
}

// ----------------------------------------------------------------------------
void SpectrumImager::pushFFT()
{
    // Pull the most-recent window of output samples and fold to mono for the
    // magnitude spectrum.
    const int got = scope.readLatest (fifoL.data(), fifoR.data(), fftSize);
    if (got < fftSize) return; // not enough history yet

    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = 0.5f * (fifoL[(size_t) i] + fifoR[(size_t) i]);
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);

    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const float norm = 2.0f / (float) fftSize;
    for (int k = 0; k <= fftSize / 2; ++k)
    {
        const float db = juce::Decibels::gainToDecibels (fftData[(size_t) k] * norm, kMinDb);
        // Fast rise, slow fall -> a readable analyser.
        float& m = mags[(size_t) k];
        m = db > m ? db : m + (db - m) * 0.30f;
    }
}

void SpectrumImager::timerCallback()
{
    sampleRate = apvts.processor.getSampleRate() > 0.0 ? apvts.processor.getSampleRate() : 48000.0;
    pushFFT();
    repaint();
}

// ----------------------------------------------------------------------------
void SpectrumImager::paint (juce::Graphics& g)
{
    auto r = plot();
    glass::fillPanel (g, getLocalBounds().toFloat(), 6.0f, colours::bgPanel, 0.9f);

    juce::Graphics::ScopedSaveState save (g);
    juce::Path clip; clip.addRoundedRectangle (r, 5.0f);
    g.reduceClipRegion (clip);

    const juce::Colour bandLo (0xff5aa6ff), bandHi (0xff35d0c0);

    // --- band tints (wider band = stronger accent) ----------------------
    float edges[5] = { r.getX(), freqToX (crossover (0)), freqToX (crossover (1)),
                       freqToX (crossover (2)), r.getRight() };
    for (int b = 0; b < 4; ++b)
    {
        auto br = juce::Rectangle<float> (edges[b], r.getY(), juce::jmax (0.0f, edges[b + 1] - edges[b]), r.getHeight());
        const float w = bandWidth (b);
        const float a = 0.05f + 0.06f * juce::jlimit (0.0f, 2.0f, w) * 0.5f;
        const auto c = (b == hoverBand || b == dragBand) ? bandHi.withAlpha (a + 0.05f) : bandLo.withAlpha (a);
        g.setColour (c);
        g.fillRect (br);
    }

    // --- frequency grid (decade lines) ----------------------------------
    g.setColour (colours::outline.withAlpha (0.5f));
    for (float f = 100.0f; f <= kFreqHi; f *= 10.0f)
        g.drawVerticalLine (juce::roundToInt (freqToX (f)), r.getY(), r.getBottom());
    g.setColour (colours::outline.withAlpha (0.22f));
    for (float decade = 100.0f; decade <= kFreqHi; decade *= 10.0f)
        for (int m = 2; m < 10; ++m)
            g.drawVerticalLine (juce::roundToInt (freqToX (decade * (float) m)), r.getY(), r.getBottom());

    // --- spectrum curve --------------------------------------------------
    auto dbToY = [&] (float db) { return r.getBottom() - (juce::jlimit (kMinDb, kMaxDb, db) - kMinDb) / (kMaxDb - kMinDb) * r.getHeight(); };
    juce::Path spec;
    bool started = false;
    const float binHz = (float) sampleRate / (float) fftSize;
    for (float x = r.getX(); x <= r.getRight(); x += 1.5f)
    {
        const float f = xToFreq (x);
        const int k = juce::jlimit (0, fftSize / 2, (int) std::round (f / binHz));
        const float y = dbToY (mags[(size_t) k]);
        if (! started) { spec.startNewSubPath (x, y); started = true; }
        else            spec.lineTo (x, y);
    }
    {
        juce::Path fillPath (spec);
        fillPath.lineTo (r.getRight(), r.getBottom());
        fillPath.lineTo (r.getX(), r.getBottom());
        fillPath.closeSubPath();
        g.setColour (colours::accent.withAlpha (0.10f));
        g.fillPath (fillPath);
        g.setColour (colours::accent.withAlpha (0.55f));
        g.strokePath (spec, juce::PathStrokeType (1.2f));
    }

    // --- per-band width line + handle -----------------------------------
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    for (int b = 0; b < 4; ++b)
    {
        const float w = bandWidth (b);
        const float y = widthToY (w);
        const float x0 = edges[b], x1 = edges[b + 1];
        const bool active = (b == hoverBand || b == dragBand);
        const auto col = bandLo.interpolatedWith (bandHi, juce::jlimit (0.0f, 1.0f, w * 0.5f));

        if (active) { g.setColour (col.withAlpha (0.25f)); juce::DropShadow (col.withAlpha (0.5f), 7, {})
                          .drawForRectangle (g, juce::Rectangle<int> ((int) x0, (int) (y - 2), (int) (x1 - x0), 4)); }
        g.setColour (col.withAlpha (active ? 0.95f : 0.7f));
        g.drawLine (x0 + 2.0f, y, x1 - 2.0f, y, active ? 2.4f : 1.8f);

        // centre grip
        const float cx = 0.5f * (x0 + x1);
        g.setColour (col.brighter (active ? 0.3f : 0.0f));
        g.fillEllipse (cx - 3.0f, y - 3.0f, 6.0f, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (active ? 0.5f : 0.25f));
        g.drawEllipse (cx - 3.0f, y - 3.0f, 6.0f, 6.0f, 1.0f);

        // width readout (only while interacting that band)
        if (active && (x1 - x0) > 36.0f)
        {
            g.setColour (colours::text);
            g.drawText (juce::String (juce::roundToInt (w * 100.0f)) + "%",
                        juce::Rectangle<float> (cx - 26.0f, y - 17.0f, 52.0f, 13.0f), juce::Justification::centred);
        }
    }

    // --- crossover handles ----------------------------------------------
    for (int i = 0; i < 3; ++i)
    {
        const float x = freqToX (crossover (i));
        const bool active = (i == hoverHandle || i == dragHandle);
        g.setColour (active ? colours::accent : colours::text.withAlpha (0.55f));
        g.drawLine (x, r.getY(), x, r.getBottom(), active ? 2.0f : 1.2f);
        // top + bottom grips
        g.fillEllipse (x - 3.5f, r.getY() + 1.0f, 7.0f, 7.0f);
        g.fillEllipse (x - 3.5f, r.getBottom() - 8.0f, 7.0f, 7.0f);
        if (active)
        {
            const float f = crossover (i);
            const juce::String t = f >= 1000.0f ? juce::String (f / 1000.0f, 2) + "k" : juce::String (juce::roundToInt (f));
            g.setColour (colours::text);
            g.setFont (juce::Font (juce::FontOptions (10.0f)));
            g.drawText (t, juce::Rectangle<float> (x - 24.0f, r.getY() + 9.0f, 48.0f, 13.0f), juce::Justification::centred);
        }
    }

    // centre (unity-width) reference line
    g.setColour (colours::outline.withAlpha (0.45f));
    const float yMid = widthToY (1.0f);
    float dashes[2] = { 3.0f, 3.0f };
    g.drawDashedLine ({ { r.getX(), yMid }, { r.getRight(), yMid } }, dashes, 2, 1.0f);
}

// ----------------------------------------------------------------------------
void SpectrumImager::mouseMove (const juce::MouseEvent& e)
{
    const int h = handleNearX ((float) e.x);
    const int b = h >= 0 ? -1 : bandAtX ((float) e.x);
    if (h != hoverHandle || b != hoverBand) { hoverHandle = h; hoverBand = b; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::LeftRightResizeCursor
                           : juce::MouseCursor::UpDownResizeCursor);
}

void SpectrumImager::mouseExit (const juce::MouseEvent&)
{
    if (hoverHandle != -1 || hoverBand != -1) { hoverHandle = hoverBand = -1; repaint(); }
}

void SpectrumImager::mouseDown (const juce::MouseEvent& e)
{
    dragHandle = handleNearX ((float) e.x);
    if (dragHandle >= 0)
    {
        dragBand = -1;
        beginGesture (freqP[dragHandle]);
    }
    else
    {
        dragBand = bandAtX ((float) e.x);
        beginGesture (widthP[dragBand]);
        setParam (widthP[dragBand], yToWidth ((float) e.y));
    }
    repaint();
}

void SpectrumImager::mouseDrag (const juce::MouseEvent& e)
{
    if (dragHandle >= 0)
    {
        // Clamp between the neighbours so the three crossovers stay ordered.
        float f = xToFreq ((float) e.x);
        if (dragHandle > 0) f = juce::jmax (f, crossover (dragHandle - 1) * 1.05f);
        if (dragHandle < 2) f = juce::jmin (f, crossover (dragHandle + 1) * 0.95f);
        setParam (freqP[dragHandle], f);
    }
    else if (dragBand >= 0)
    {
        setParam (widthP[dragBand], yToWidth ((float) e.y));
    }
    repaint();
}

void SpectrumImager::mouseUp (const juce::MouseEvent&)
{
    if (dragHandle >= 0) endGesture (freqP[dragHandle]);
    if (dragBand   >= 0) endGesture (widthP[dragBand]);
    dragHandle = dragBand = -1;
    repaint();
}

void SpectrumImager::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int h = handleNearX ((float) e.x);
    if (h >= 0)
    {
        if (auto* p = freqP[h]) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); }
    }
    else
    {
        const int b = bandAtX ((float) e.x);
        setParam (widthP[b], 1.0f); // reset band to 100% (unity)
    }
    repaint();
}

} // namespace anamorph::gui
