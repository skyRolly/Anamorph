#include "SpectrumImager.h"
#include "LookAndFeel.h"
#include "PluginParameters.h"
#include <cmath>

namespace anamorph::gui
{

static constexpr float kFreqLo = 20.0f, kFreqHi = 20000.0f;
static constexpr float kMinDb = -90.0f, kMaxDb = 0.0f;

namespace
{
    struct Tick { float f; const char* label; bool major; };
    const Tick kTicks[] = {
        { 30.0f, "30", false }, { 50.0f, "50", false }, { 100.0f, "100", true },
        { 200.0f, "200", false }, { 500.0f, "500", false }, { 1000.0f, "1k", true },
        { 2000.0f, "2k", false }, { 5000.0f, "5k", false }, { 10000.0f, "10k", true },
        { 20000.0f, "20k", false }
    };
}

SpectrumImager::SpectrumImager (anamorph::ScopeBuffer& s, juce::AudioProcessorValueTreeState& a)
    : scope (s), apvts (a)
{
    bandsP    = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (pid::mbBands));
    freqP[0]  = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (pid::mbFreqLow));
    freqP[1]  = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (pid::mbFreqMid));
    freqP[2]  = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (pid::mbFreqHigh));
    widthP[0] = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (pid::mbWidthLow));
    widthP[1] = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (pid::mbWidthMid));
    widthP[2] = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (pid::mbWidthHiMid));
    widthP[3] = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (pid::mbWidthHigh));
    animOnP   = apvts.getRawParameterValue (pid::uiAnimations);

    fifoL.assign ((size_t) fftSize, 0.0f);
    fifoR.assign ((size_t) fftSize, 0.0f);
    fftData.assign ((size_t) fftSize * 2, 0.0f);
    mags.assign ((size_t) fftSize / 2 + 1, kMinDb);

    setInterceptsMouseClicks (true, false);
    startTimerHz (60);
}

SpectrumImager::~SpectrumImager() { stopTimer(); }

// ----------------------------------------------------------------------------
//  Geometry
// ----------------------------------------------------------------------------
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

float SpectrumImager::yThird()  const noexcept { auto r = plot(); return r.getY() + r.getHeight() * (1.0f / 3.0f); }
float SpectrumImager::rulerY()  const noexcept { return plot().getBottom() - 14.0f; }
float SpectrumImager::laneTop() const noexcept { return plot().getY() + 6.0f; }
float SpectrumImager::laneBot() const noexcept { return rulerY() - 8.0f; }

float SpectrumImager::widthToY (float w) const noexcept
{
    const float top = laneTop(), bot = laneBot();
    return bot - juce::jlimit (0.0f, 2.0f, w) * 0.5f * (bot - top);
}

float SpectrumImager::yToWidth (float y) const noexcept
{
    const float top = laneTop(), bot = laneBot();
    return juce::jlimit (0.0f, 1.0f, (bot - y) / (bot - top)) * 2.0f;
}

// ----------------------------------------------------------------------------
//  Parameter reads
// ----------------------------------------------------------------------------
int SpectrumImager::bandCount() const noexcept
{
    if (auto* p = bandsP) return juce::jlimit (1, 4, (int) std::lround (p->convertFrom0to1 (p->getValue())));
    return 4;
}

float SpectrumImager::crossover (int i) const noexcept
{
    if (i >= 0 && i < 3) if (auto* p = freqP[i]) return p->convertFrom0to1 (p->getValue());
    return kFreqLo;
}

float SpectrumImager::bandWidth (int i) const noexcept
{
    if (i >= 0 && i < 4) if (auto* p = widthP[i]) return p->convertFrom0to1 (p->getValue());
    return 1.0f;
}

float SpectrumImager::bandLeftX (int b) const noexcept
{
    return b <= 0 ? plot().getX() : freqToX (crossover (b - 1));
}

float SpectrumImager::bandRightX (int b) const noexcept
{
    return b >= bandCount() - 1 ? plot().getRight() : freqToX (crossover (b));
}

juce::Rectangle<float> SpectrumImager::deleteRect (int b) const noexcept
{
    return { bandLeftX (b) + 4.0f, rulerY() - 20.0f, 14.0f, 14.0f };
}

int SpectrumImager::bandAtX (float x) const noexcept
{
    const int N = bandCount();
    const float f = xToFreq (x);
    for (int i = 0; i < N - 1; ++i)
        if (f < crossover (i)) return i;
    return N - 1;
}

int SpectrumImager::handleNearX (float x) const noexcept
{
    const int N = bandCount();
    int best = -1; float bestD = 7.0f; // px tolerance
    for (int i = 0; i < N - 1; ++i)
    {
        const float d = std::abs (x - freqToX (crossover (i)));
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

int SpectrumImager::deleteHit (juce::Point<float> p) const noexcept
{
    const int N = bandCount();
    if (N <= 1) return -1;
    for (int b = 0; b < N; ++b)
        if (deleteRect (b).contains (p)) return b;
    return -1;
}

// ----------------------------------------------------------------------------
//  Parameter writes
// ----------------------------------------------------------------------------
void SpectrumImager::beginGesture (juce::RangedAudioParameter* p) { if (p) p->beginChangeGesture(); }
void SpectrumImager::endGesture   (juce::RangedAudioParameter* p) { if (p) p->endChangeGesture(); }
void SpectrumImager::setParam (juce::RangedAudioParameter* p, float plain)
{
    if (p) p->setValueNotifyingHost (p->convertTo0to1 (plain));
}
void SpectrumImager::resetParam (juce::RangedAudioParameter* p)
{
    if (p) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); }
}
void SpectrumImager::setBands (int n)
{
    if (auto* p = bandsP)
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::jlimit (1, 4, n)));
        p->endChangeGesture();
    }
}

void SpectrumImager::addBandAt (float hz)
{
    const int N = bandCount();
    if (N >= 4) return;
    hz = juce::jlimit (kFreqLo, kFreqHi, hz);

    float fr[3], wd[4];
    for (int i = 0; i < 3; ++i) fr[i] = crossover (i);
    for (int i = 0; i < 4; ++i) wd[i] = bandWidth (i);

    int ins = 0;                                  // insertion index among the active crossovers
    while (ins < N - 1 && fr[ins] < hz) ++ins;
    if (ins > 0)     hz = juce::jmax (hz, fr[ins - 1] * 1.05f);
    if (ins < N - 1) hz = juce::jmin (hz, fr[ins]     * 0.95f);

    float nf[3], nw[4];
    for (int i = 0; i < N; ++i)     nf[i] = (i < ins) ? fr[i] : (i == ins ? hz : fr[i - 1]); // N crossovers
    for (int i = 0; i <= N; ++i)    nw[i] = (i <= ins) ? wd[i] : wd[i - 1];                  // N+1 widths (dup split band)

    for (int i = 0; i <= N; ++i)    setParam (widthP[i], nw[i]);
    for (int i = 0; i < N;  ++i)    setParam (freqP[i],  nf[i]);
    setBands (N + 1);
}

void SpectrumImager::deleteBand (int b)
{
    const int N = bandCount();
    if (N <= 1) return;
    b = juce::jlimit (0, N - 1, b);
    const int c = (b == N - 1) ? b - 1 : b;       // crossover to remove (merge with the neighbour)

    float fr[3], wd[4];
    for (int i = 0; i < 3; ++i) fr[i] = crossover (i);
    for (int i = 0; i < 4; ++i) wd[i] = bandWidth (i);

    float nf[3], nw[4];
    for (int i = 0; i < N - 2; ++i) nf[i] = (i < c)  ? fr[i] : fr[i + 1];  // drop crossover c
    for (int i = 0; i < N - 1; ++i) nw[i] = (i <= c) ? wd[i] : wd[i + 1];  // drop width c+1, keep left band

    for (int i = 0; i < N - 1; ++i) setParam (widthP[i], nw[i]);
    for (int i = 0; i < N - 2; ++i) setParam (freqP[i],  nf[i]);
    setBands (N - 1);
}

// ----------------------------------------------------------------------------
//  Analyser
// ----------------------------------------------------------------------------
void SpectrumImager::pushFFT()
{
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
        float& m = mags[(size_t) k];
        m = db > m ? db : m + (db - m) * 0.25f; // fast rise, gentle fall
    }
}

float SpectrumImager::magForColumn (float xa, float xb) const noexcept
{
    const float binHz = (float) sampleRate / (float) fftSize;
    const int   kmax  = fftSize / 2;
    const float fa = xToFreq (juce::jmin (xa, xb));
    const float fb = xToFreq (juce::jmax (xa, xb));

    int ka = (int) std::floor (fa / binHz);
    int kb = (int) std::ceil  (fb / binHz);
    ka = juce::jlimit (0, kmax, ka);
    kb = juce::jlimit (0, kmax, kb);

    if (kb <= ka)
    {
        // Sub-bin column (the LOW end): interpolate so the curve slopes between
        // bins instead of stair-stepping (0.6.6 #8).
        const float bin = 0.5f * (fa + fb) / binHz;
        const int   i0  = juce::jlimit (0, kmax - 1, (int) std::floor (bin));
        const float fr  = juce::jlimit (0.0f, 1.0f, bin - (float) i0);
        return mags[(size_t) i0] + (mags[(size_t) (i0 + 1)] - mags[(size_t) i0]) * fr;
    }

    // Several bins per column (the HIGH end): average for a clean, smooth line.
    float sum = 0.0f;
    for (int k = ka; k <= kb; ++k) sum += mags[(size_t) k];
    return sum / (float) (kb - ka + 1);
}

void SpectrumImager::timerCallback()
{
    sampleRate = apvts.processor.getSampleRate() > 0.0 ? apvts.processor.getSampleRate() : 48000.0;
    pushFFT();

    // Ease the hover/press activity the same way the rest of the UI does: a faster
    // fade in than out, snapping when UI Animations are off (0.6.6 #11).
    const bool animOn = animOnP == nullptr || animOnP->load() > 0.5f;
    const float dt   = 1.0f / 60.0f;
    const float rIn  = animOn ? 1.0f - std::exp (-dt / 0.075f) : 1.0f;
    const float rOut = animOn ? 1.0f - std::exp (-dt / 0.150f) : 1.0f;
    auto ease = [&] (float& v, float t) { v += (t - v) * (t > v ? rIn : rOut); };

    for (int i = 0; i < 3; ++i) ease (handleA[i], (i == dragHandle || i == hoverHandle) ? 1.0f : 0.0f);
    for (int i = 0; i < 4; ++i) ease (bandA[i],   (i == dragBand   || i == hoverBand)   ? 1.0f : 0.0f);
    for (int i = 0; i < 4; ++i) ease (delA[i],    (i == hoverDelete) ? 1.0f : 0.0f);
    ease (addA, hoverAdd ? 1.0f : 0.0f);

    repaint();
}

// ----------------------------------------------------------------------------
//  Paint
// ----------------------------------------------------------------------------
void SpectrumImager::paint (juce::Graphics& g)
{
    auto r = plot();
    glass::fillPanel (g, getLocalBounds().toFloat(), 6.0f, colours::bgPanel, 0.92f);

    juce::Graphics::ScopedSaveState save (g);
    juce::Path clip; clip.addRoundedRectangle (r, 5.0f);
    g.reduceClipRegion (clip);

    const int N = bandCount();
    const juce::Colour bandLo (0xff5aa6ff), bandHi (0xff35d0c0);

    // --- band tints -----------------------------------------------------
    for (int b = 0; b < N; ++b)
    {
        const float x0 = bandLeftX (b), x1 = bandRightX (b);
        const float w  = bandWidth (b);
        const float a  = 0.04f + 0.05f * juce::jlimit (0.0f, 2.0f, w) * 0.5f + 0.06f * bandA[b];
        g.setColour (bandLo.interpolatedWith (bandHi, juce::jlimit (0.0f, 1.0f, w * 0.5f)).withAlpha (a));
        g.fillRect (juce::Rectangle<float> (x0, r.getY(), juce::jmax (0.0f, x1 - x0), r.getHeight()));
    }

    // --- frequency grid (ruler ticks) -----------------------------------
    for (auto& t : kTicks)
    {
        const float x = freqToX (t.f);
        g.setColour (colours::outline.withAlpha (t.major ? 0.30f : 0.15f));
        g.drawVerticalLine (juce::roundToInt (x), r.getY(), rulerY() + 2.0f);
    }

    // --- the 1/3 "add" reference line -----------------------------------
    {
        const float y = yThird();
        float dl[2] = { 2.0f, 3.0f };
        g.setColour (colours::outline.withAlpha (0.22f));
        g.drawDashedLine ({ { r.getX(), y }, { r.getRight(), y } }, dl, 2, 1.0f);
    }

    // --- spectrum curve + fill ------------------------------------------
    {
        auto dbToY = [&] (float db) { return r.getBottom() - (juce::jlimit (kMinDb, kMaxDb, db) - kMinDb) / (kMaxDb - kMinDb) * r.getHeight(); };
        juce::Path spec;
        bool started = false;
        for (float x = r.getX(); x <= r.getRight(); x += 1.0f)
        {
            const float y = dbToY (magForColumn (x - 0.5f, x + 0.5f));
            if (! started) { spec.startNewSubPath (x, y); started = true; }
            else            spec.lineTo (x, y);
        }
        juce::Path fillPath (spec);
        fillPath.lineTo (r.getRight(), r.getBottom());
        fillPath.lineTo (r.getX(), r.getBottom());
        fillPath.closeSubPath();
        g.setGradientFill (juce::ColourGradient (colours::accent.withAlpha (0.22f), 0.0f, r.getY(),
                                                 colours::accent.withAlpha (0.015f), 0.0f, r.getBottom(), false));
        g.fillPath (fillPath);
        g.setColour (colours::accent.withAlpha (0.62f));
        g.strokePath (spec, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // --- unity-width reference ------------------------------------------
    {
        const float y = widthToY (1.0f);
        float d[2] = { 3.0f, 3.0f };
        g.setColour (colours::outline.withAlpha (0.40f));
        g.drawDashedLine ({ { r.getX(), y }, { r.getRight(), y } }, d, 2, 1.0f);
    }

    // --- per-band width lines -------------------------------------------
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    for (int b = 0; b < N; ++b)
    {
        const float w = bandWidth (b);
        const float y = widthToY (w);
        const float x0 = bandLeftX (b), x1 = bandRightX (b);
        const float act = bandA[b];
        const auto col = bandLo.interpolatedWith (bandHi, juce::jlimit (0.0f, 1.0f, w * 0.5f));

        if (act > 0.01f)
            juce::DropShadow (col.withAlpha (0.5f * act), 8, {})
                .drawForRectangle (g, juce::Rectangle<int> ((int) x0, (int) (y - 2.0f), (int) (x1 - x0), 4));

        g.setColour (col.withAlpha (0.55f + 0.4f * act));
        g.drawLine (x0 + 3.0f, y, x1 - 3.0f, y, 1.6f + 1.0f * act);

        const float cx = 0.5f * (x0 + x1);
        g.setColour (col.brighter (0.2f * act));
        g.fillEllipse (cx - 3.0f, y - 3.0f, 6.0f, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.2f + 0.35f * act));
        g.drawEllipse (cx - 3.0f, y - 3.0f, 6.0f, 6.0f, 1.0f);

        if (act > 0.2f && (x1 - x0) > 40.0f)
        {
            g.setColour (colours::text.withAlpha (act));
            g.drawText (juce::String (juce::roundToInt (w * 100.0f)) + "%",
                        juce::Rectangle<float> (cx - 26.0f, y - 17.0f, 52.0f, 13.0f), juce::Justification::centred);
        }
    }

    // --- frequency ruler numbers ----------------------------------------
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    for (auto& t : kTicks)
    {
        const float x = freqToX (t.f);
        g.setColour (colours::textDim.withAlpha (t.major ? 0.85f : 0.50f));
        g.drawText (t.label, juce::Rectangle<float> (x - 20.0f, rulerY() - 6.0f, 40.0f, 13.0f), juce::Justification::centred);
    }

    // --- crossover handles (active only) --------------------------------
    for (int i = 0; i < N - 1; ++i)
    {
        const float x = freqToX (crossover (i));
        const float act = handleA[i];
        g.setColour (colours::text.withAlpha (0.5f).interpolatedWith (colours::accent, juce::jlimit (0.0f, 1.0f, act)));
        g.drawLine (x, r.getY(), x, rulerY() - 9.0f, 1.2f + 0.9f * act);
        g.setColour (act > 0.5f ? colours::accent : colours::text.withAlpha (0.6f));
        g.fillEllipse (x - 3.5f, r.getY() + 1.0f, 7.0f, 7.0f);

        if (act > 0.05f)
        {
            const float f = crossover (i);
            const juce::String t = f >= 1000.0f ? juce::String (f / 1000.0f, 2) + "k" : juce::String (juce::roundToInt (f));
            auto nb = juce::Rectangle<float> (x - 22.0f, rulerY() - 7.0f, 44.0f, 14.0f);
            g.setColour (colours::bgPanel.withAlpha (0.85f * act));
            g.fillRoundedRectangle (nb, 3.0f);
            g.setFont (juce::Font (juce::FontOptions (10.0f)));
            g.setColour (colours::text.brighter (0.3f).withAlpha (act));
            g.drawText (t, nb, juce::Justification::centred);
        }
    }

    // --- add-band hint (top strip, bands < 4) ---------------------------
    if (addA > 0.02f)
    {
        const float x  = juce::jlimit (r.getX(), r.getRight(), addX);
        const float a  = addA;
        float dl[2] = { 3.0f, 3.0f };
        g.setColour (colours::accent.withAlpha (0.55f * a));
        g.drawDashedLine ({ { x, yThird() }, { x, rulerY() - 9.0f } }, dl, 2, 1.2f);

        const float py = yThird() - 9.0f;        // the "+" above the line
        g.setColour (colours::accent.withAlpha (0.9f * a));
        g.drawLine (x - 4.0f, py, x + 4.0f, py, 1.6f);
        g.drawLine (x, py - 4.0f, x, py + 4.0f, 1.6f);

        const float f = xToFreq (x);
        const juce::String t = f >= 1000.0f ? juce::String (f / 1000.0f, 2) + "k" : juce::String (juce::roundToInt (f));
        auto nb = juce::Rectangle<float> (x - 22.0f, rulerY() - 7.0f, 44.0f, 14.0f);
        g.setColour (colours::bgPanel.withAlpha (0.85f * a));
        g.fillRoundedRectangle (nb, 3.0f);
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.setColour (colours::accent.brighter (0.3f).withAlpha (a));
        g.drawText (t, nb, juce::Justification::centred);
    }

    // --- delete affordances (dim, like a reference line) ----------------
    for (int b = 0; b < N; ++b)
    {
        const float a = delA[b];
        if (a < 0.02f) continue;
        auto dr = deleteRect (b);
        const float pad = 3.0f;
        g.setColour (colours::textDim.withAlpha (0.75f * a));
        g.drawLine (dr.getX() + pad, dr.getY() + pad, dr.getRight() - pad, dr.getBottom() - pad, 1.4f);
        g.drawLine (dr.getRight() - pad, dr.getY() + pad, dr.getX() + pad, dr.getBottom() - pad, 1.4f);
    }
}

// ----------------------------------------------------------------------------
//  Interaction
// ----------------------------------------------------------------------------
void SpectrumImager::updateHover (juce::Point<float> p)
{
    const int N = bandCount();
    const int h = handleNearX (p.x);
    const int dHit = deleteHit (p);
    const bool inAddZone = (N < 4) && (p.y < yThird()) && h < 0 && dHit < 0;

    int delShow = dHit;
    if (delShow < 0 && N > 1 && h < 0 && p.y > plot().getY() + plot().getHeight() * (2.0f / 3.0f))
        delShow = bandAtX (p.x);

    hoverHandle = h;
    hoverBand   = (h >= 0 || inAddZone) ? -1 : bandAtX (p.x);
    hoverDelete = delShow;
    hoverAdd    = inAddZone;
    addX        = p.x;

    if (h >= 0)            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else if (dHit >= 0)    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    else if (inAddZone)    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    else                   setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void SpectrumImager::mouseMove (const juce::MouseEvent& e)
{
    scrollHandle = scrollBand = -1; // a real pointer move releases the scroll latch (#5)
    updateHover (e.position);
}

void SpectrumImager::mouseExit (const juce::MouseEvent&)
{
    hoverHandle = hoverBand = hoverDelete = -1;
    hoverAdd = false;
    scrollHandle = scrollBand = -1;
}

void SpectrumImager::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position;
    const int N = bandCount();
    const int h = handleNearX (p.x);

    // Option/alt-click resets, the same gesture the knobs and sliders use (#6).
    if (e.mods.isAltDown())
    {
        if (h >= 0) resetParam (freqP[h]);
        else        resetParam (widthP[bandAtX (p.x)]);
        repaint();
        return;
    }

    if (const int dB = deleteHit (p); dB >= 0) { deleteBand (dB); return; }

    if (h >= 0) { dragHandle = h; dragBand = -1; beginGesture (freqP[h]); repaint(); return; }

    if (N < 4 && p.y < yThird()) { addBandAt (xToFreq (p.x)); return; }

    const int b = bandAtX (p.x);
    dragBand = b; dragHandle = -1;
    beginGesture (widthP[b]);
    setParam (widthP[b], yToWidth (p.y));
    repaint();
}

void SpectrumImager::mouseDrag (const juce::MouseEvent& e)
{
    const int N = bandCount();
    if (dragHandle >= 0)
    {
        float f = xToFreq ((float) e.position.x);
        if (dragHandle > 0)     f = juce::jmax (f, crossover (dragHandle - 1) * 1.05f);
        if (dragHandle < N - 2) f = juce::jmin (f, crossover (dragHandle + 1) * 0.95f);
        setParam (freqP[dragHandle], juce::jlimit (kFreqLo, kFreqHi, f));
    }
    else if (dragBand >= 0)
    {
        setParam (widthP[dragBand], yToWidth ((float) e.position.y));
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
    const int h = handleNearX ((float) e.position.x);
    if (h >= 0) resetParam (freqP[h]);
    else        resetParam (widthP[bandAtX ((float) e.position.x)]);
    repaint();
}

void SpectrumImager::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int N = bandCount();

    // Latch the target on the first notch; the latch holds (even as the split
    // slides out from under the cursor) until the pointer next moves (#5).
    if (scrollHandle < 0 && scrollBand < 0)
    {
        const int h = handleNearX ((float) e.position.x);
        if (h >= 0) scrollHandle = h;
        else        scrollBand = bandAtX ((float) e.position.x);
    }

    const float dy = (wheel.isReversed ? -1.0f : 1.0f) * wheel.deltaY;
    if (std::abs (dy) < 1.0e-4f) return;

    if (scrollHandle >= 0 && scrollHandle < N - 1)
    {
        float f = crossover (scrollHandle) * std::pow (2.0f, dy * 0.5f); // ~half octave / notch
        if (scrollHandle > 0)     f = juce::jmax (f, crossover (scrollHandle - 1) * 1.05f);
        if (scrollHandle < N - 2) f = juce::jmin (f, crossover (scrollHandle + 1) * 0.95f);
        setParam (freqP[scrollHandle], juce::jlimit (kFreqLo, kFreqHi, f));
    }
    else if (scrollBand >= 0 && scrollBand < N)
    {
        setParam (widthP[scrollBand], juce::jlimit (0.0f, 2.0f, bandWidth (scrollBand) + dy * 0.05f));
    }
    repaint();
}

} // namespace anamorph::gui
