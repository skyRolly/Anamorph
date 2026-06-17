#include "SpectrumImager.h"
#include "LookAndFeel.h"
#include "PluginParameters.h"
#include <cmath>

namespace anamorph::gui
{

static constexpr float kFreqLo = 20.0f, kFreqHi = 20000.0f;
static constexpr float kMinDb = -90.0f, kMaxDb = 0.0f;
static constexpr float kWidthGrab = 8.0f;   // px tolerance to grab a width line (#15)

namespace
{
    struct Tick { float f; const char* label; bool major; };
    const Tick kTicks[] = {
        { 30.0f, "30", false }, { 50.0f, "50", false }, { 100.0f, "100", true },
        { 200.0f, "200", false }, { 500.0f, "500", false }, { 1000.0f, "1k", true },
        { 2000.0f, "2k", false }, { 5000.0f, "5k", false }, { 10000.0f, "10k", true },
        { 20000.0f, "20k", false }
    };

    juce::String freqText (float f)
    {
        return f >= 1000.0f ? juce::String (f / 1000.0f, 2) + "k"
                            : juce::String (juce::roundToInt (f));
    }
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
    enableP   = apvts.getRawParameterValue (pid::mbEnable);

    fifoL.assign ((size_t) fftSize, 0.0f);
    fifoR.assign ((size_t) fftSize, 0.0f);
    fftData.assign ((size_t) fftSize * 2, 0.0f);
    mags.assign ((size_t) fftSize / 2 + 1, kMinDb);

    enaA = enabled() ? 1.0f : 0.0f;
    setInterceptsMouseClicks (true, false);
    startTimerHz (60);
}

SpectrumImager::~SpectrumImager() { stopTimer(); }

// ----------------------------------------------------------------------------
//  Geometry
// ----------------------------------------------------------------------------
juce::Rectangle<float> SpectrumImager::plot() const noexcept { return getLocalBounds().toFloat().reduced (1.0f); }

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

float SpectrumImager::rulerY()  const noexcept { return plot().getBottom() - 14.0f; }
float SpectrumImager::laneTop() const noexcept { return plot().getY() + 8.0f; }
float SpectrumImager::laneBot() const noexcept { return rulerY() - 10.0f; }

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
bool SpectrumImager::enabled() const noexcept { return enableP == nullptr || enableP->load() > 0.5f; }

float SpectrumImager::bandLeftX  (int b) const noexcept { return b <= 0 ? plot().getX() : freqToX (crossover (b - 1)); }
float SpectrumImager::bandRightX (int b) const noexcept { return b >= bandCount() - 1 ? plot().getRight() : freqToX (crossover (b)); }

juce::Rectangle<float> SpectrumImager::deleteBox (int i) const noexcept
{
    return { freqToX (crossover (i)) + 6.0f, plot().getY() + 4.0f, 13.0f, 13.0f };
}
juce::Rectangle<float> SpectrumImager::numberChip (int i) const noexcept
{
    return { freqToX (crossover (i)) - 22.0f, rulerY() - 7.0f, 44.0f, 14.0f };
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
    int best = -1; float bestD = 7.0f;
    for (int i = 0; i < N - 1; ++i)
    {
        const float d = std::abs (x - freqToX (crossover (i)));
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

bool SpectrumImager::nearWidthLine (juce::Point<float> p, int b) const noexcept
{
    return std::abs (p.y - widthToY (bandWidth (b))) < kWidthGrab;
}

int SpectrumImager::deleteHit (juce::Point<float> p) const noexcept
{
    const int N = bandCount();
    for (int i = 0; i < N - 1; ++i)
        if (deleteBox (i).contains (p)) return i;
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

// Reset a split to its default, but CLAMPED between its neighbours so it can never
// jump across an adjacent split (0.6.7 #18).
void SpectrumImager::resetCrossover (int i)
{
    auto* p = (i >= 0 && i < 3) ? freqP[i] : nullptr;
    if (p == nullptr) return;
    const int N = bandCount();
    float def = p->convertFrom0to1 (p->getDefaultValue());
    if (i > 0)     def = juce::jmax (def, crossover (i - 1) * 1.05f);
    if (i < N - 2) def = juce::jmin (def, crossover (i + 1) * 0.95f);
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (kFreqLo, kFreqHi, def)));
    p->endChangeGesture();
}

int SpectrumImager::addBandAt (float hz)
{
    const int N = bandCount();
    if (N >= 4) return -1;
    hz = juce::jlimit (kFreqLo, kFreqHi, hz);

    float fr[3], wd[4];
    for (int i = 0; i < 3; ++i) fr[i] = crossover (i);
    for (int i = 0; i < 4; ++i) wd[i] = bandWidth (i);

    int ins = 0;
    while (ins < N - 1 && fr[ins] < hz) ++ins;
    if (ins > 0)     hz = juce::jmax (hz, fr[ins - 1] * 1.05f);
    if (ins < N - 1) hz = juce::jmin (hz, fr[ins]     * 0.95f);

    float nf[3], nw[4];
    for (int i = 0; i < N; ++i)  nf[i] = (i < ins) ? fr[i] : (i == ins ? hz : fr[i - 1]);
    for (int i = 0; i <= N; ++i) nw[i] = (i <= ins) ? wd[i] : wd[i - 1];

    for (int i = 0; i <= N; ++i) setParam (widthP[i], nw[i]);
    for (int i = 0; i < N;  ++i) setParam (freqP[i],  nf[i]);
    setBands (N + 1);
    return ins;
}

void SpectrumImager::removeCrossover (int i)
{
    const int N = bandCount();
    if (N <= 1 || i < 0 || i > N - 2) return;

    float fr[3], wd[4];
    for (int k = 0; k < 3; ++k) fr[k] = crossover (k);
    for (int k = 0; k < 4; ++k) wd[k] = bandWidth (k);

    float nf[3], nw[4];
    for (int k = 0; k < N - 2; ++k) nf[k] = (k < i)  ? fr[k] : fr[k + 1];
    for (int k = 0; k < N - 1; ++k) nw[k] = (k <= i) ? wd[k] : wd[k + 1];

    for (int k = 0; k < N - 1; ++k) setParam (widthP[k], nw[k]);
    for (int k = 0; k < N - 2; ++k) setParam (freqP[k],  nf[k]);
    setBands (N - 1);
}

// ----------------------------------------------------------------------------
//  Frequency text editor (#5)
// ----------------------------------------------------------------------------
float SpectrumImager::parseFreq (const juce::String& t)
{
    auto s = t.toLowerCase().trim();
    const bool k = s.containsChar ('k');
    const float v = s.removeCharacters ("khz ").getFloatValue();
    if (k) return v * 1000.0f;                 // "7.7k" -> 7700
    return (v <= 20.0f) ? v * 1000.0f : v;     // bare <= 20 means kHz: "0.5" -> 500, "7.7" -> 7700
}

void SpectrumImager::openFreqEditor (int i)
{
    if (i < 0 || i >= bandCount() - 1) return;
    editingHandle = i;

    if (freqEditor == nullptr)
    {
        freqEditor = std::make_unique<juce::TextEditor>();
        freqEditor->setJustification (juce::Justification::centred);
        freqEditor->setBorder (juce::BorderSize<int> (1));
        freqEditor->setColour (juce::TextEditor::backgroundColourId, colours::bgPanel);
        freqEditor->setColour (juce::TextEditor::outlineColourId, colours::accent.withAlpha (0.7f));
        freqEditor->setColour (juce::TextEditor::focusedOutlineColourId, colours::accent);
        freqEditor->setColour (juce::TextEditor::textColourId, colours::text);
        freqEditor->setColour (juce::TextEditor::highlightColourId, colours::accent.withAlpha (0.4f));
        freqEditor->setFont (juce::Font (juce::FontOptions (11.0f)));
        freqEditor->setSelectAllWhenFocused (true);
        freqEditor->onReturnKey  = [this] { commitFreqEditor(); };
        freqEditor->onEscapeKey  = [this] { closeFreqEditor(); };
        freqEditor->onFocusLost  = [this] { commitFreqEditor(); };
        addAndMakeVisible (*freqEditor);
    }

    auto chip = numberChip (i).expanded (6.0f, 3.0f);
    freqEditor->setBounds (chip.toNearestInt());
    freqEditor->setText (freqText (crossover (i)), juce::dontSendNotification);
    freqEditor->setVisible (true);
    freqEditor->grabKeyboardFocus();
    freqEditor->selectAll();
}

void SpectrumImager::commitFreqEditor()
{
    if (editingHandle < 0) return;
    const int i = editingHandle;
    const int N = bandCount();
    float hz = parseFreq (freqEditor->getText());
    if (i > 0)     hz = juce::jmax (hz, crossover (i - 1) * 1.05f);
    if (i < N - 2) hz = juce::jmin (hz, crossover (i + 1) * 0.95f);
    if (auto* p = freqP[i])
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (kFreqLo, kFreqHi, hz)));
        p->endChangeGesture();
    }
    closeFreqEditor();
}

void SpectrumImager::closeFreqEditor()
{
    editingHandle = -1;
    if (freqEditor) freqEditor->setVisible (false);
}

// ----------------------------------------------------------------------------
//  Analyser
// ----------------------------------------------------------------------------
void SpectrumImager::pushFFT()
{
    const int got = scope.readLatest (fifoL.data(), fifoR.data(), fftSize);
    if (got < fftSize) return;

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
        m = db > m ? db : m + (db - m) * 0.25f;
    }
}

float SpectrumImager::magCubic (float binPos) const noexcept
{
    const int kmax = fftSize / 2;
    const int i = (int) std::floor (binPos);
    const float t = binPos - (float) i;
    auto m = [&] (int j) { return mags[(size_t) juce::jlimit (0, kmax, j)]; };
    const float m0 = m (i - 1), m1 = m (i), m2 = m (i + 1), m3 = m (i + 2);
    // Catmull-Rom: smooth curve THROUGH the bin points (kills the low-end stairs, #11).
    return 0.5f * ((2.0f * m1)
                   + (-m0 + m2) * t
                   + (2.0f * m0 - 5.0f * m1 + 4.0f * m2 - m3) * t * t
                   + (-m0 + 3.0f * m1 - 3.0f * m2 + m3) * t * t * t);
}

float SpectrumImager::magForColumn (float xa, float xb) const noexcept
{
    const float binHz = (float) sampleRate / (float) fftSize;
    const int   kmax  = fftSize / 2;
    const float fa = xToFreq (juce::jmin (xa, xb));
    const float fb = xToFreq (juce::jmax (xa, xb));
    const float span = (fb - fa) / binHz;

    if (span < 1.5f)
        return magCubic (0.5f * (fa + fb) / binHz); // few bins per column -> interpolate

    int ka = juce::jlimit (0, kmax, (int) std::floor (fa / binHz));
    int kb = juce::jlimit (0, kmax, (int) std::ceil  (fb / binHz));
    float sum = 0.0f;
    for (int k = ka; k <= kb; ++k) sum += mags[(size_t) k];
    return sum / (float) (kb - ka + 1);
}

void SpectrumImager::timerCallback()
{
    sampleRate = apvts.processor.getSampleRate() > 0.0 ? apvts.processor.getSampleRate() : 48000.0;
    pushFFT();

    const bool animOn = animOnP == nullptr || animOnP->load() > 0.5f;
    const float dt   = 1.0f / 60.0f;
    const float rIn  = animOn ? 1.0f - std::exp (-dt / 0.075f) : 1.0f;
    const float rOut = animOn ? 1.0f - std::exp (-dt / 0.150f) : 1.0f;
    auto ease = [&] (float& v, float t) { v += (t - v) * (t > v ? rIn : rOut); };

    for (int i = 0; i < 3; ++i) ease (handleA[i], (i == dragHandle || i == hoverHandle) ? 1.0f : 0.0f);
    for (int i = 0; i < 4; ++i) ease (widthA[i],  (i == dragBand   || i == hoverWidth)  ? 1.0f : 0.0f);
    for (int i = 0; i < 3; ++i) ease (delA[i],    (i == hoverDelete) ? 1.0f : 0.0f);
    ease (addA, hoverAdd >= 0 ? 1.0f : 0.0f);
    ease (enaA, enabled() ? 1.0f : 0.0f);

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
    const juce::Colour xoverCol = colours::accent;

    // --- band tints -----------------------------------------------------
    for (int b = 0; b < N; ++b)
    {
        const float x0 = bandLeftX (b), x1 = bandRightX (b);
        const float w  = bandWidth (b);
        const float a  = 0.04f + 0.05f * juce::jlimit (0.0f, 2.0f, w) * 0.5f + 0.06f * widthA[b];
        g.setColour (bandLo.interpolatedWith (bandHi, juce::jlimit (0.0f, 1.0f, w * 0.5f)).withAlpha (a));
        g.fillRect (juce::Rectangle<float> (x0, r.getY(), juce::jmax (0.0f, x1 - x0), r.getHeight()));
    }

    // --- frequency grid -------------------------------------------------
    for (auto& t : kTicks)
    {
        const float x = freqToX (t.f);
        g.setColour (colours::outline.withAlpha (t.major ? 0.30f : 0.15f));
        g.drawVerticalLine (juce::roundToInt (x), r.getY(), rulerY() + 2.0f);
    }

    // --- spectrum curve + fill (floor SUNK below the frame so a silent signal
    //     leaves no green line, #10; cubic-smoothed low end, #11) -------------
    {
        auto dbToY = [&] (float db)
        {
            const float t = (juce::jlimit (kMinDb, kMaxDb, db) - kMinDb) / (kMaxDb - kMinDb);
            return (r.getBottom() + 8.0f) - t * (r.getHeight() + 8.0f);
        };
        juce::Path spec;
        bool started = false;
        for (float x = r.getX(); x <= r.getRight(); x += 1.0f)
        {
            const float y = dbToY (magForColumn (x - 0.5f, x + 0.5f));
            if (! started) { spec.startNewSubPath (x, y); started = true; }
            else            spec.lineTo (x, y);
        }
        juce::Path fillPath (spec);
        fillPath.lineTo (r.getRight(), r.getBottom() + 2.0f);
        fillPath.lineTo (r.getX(), r.getBottom() + 2.0f);
        fillPath.closeSubPath();
        g.setGradientFill (juce::ColourGradient (xoverCol.withAlpha (0.22f), 0.0f, r.getY(),
                                                 xoverCol.withAlpha (0.015f), 0.0f, r.getBottom(), false));
        g.fillPath (fillPath);
        g.setColour (xoverCol.withAlpha (0.6f));
        g.strokePath (spec, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // --- unity-width reference ------------------------------------------
    {
        const float y = widthToY (1.0f);
        float d[2] = { 3.0f, 3.0f };
        g.setColour (colours::outline.withAlpha (0.40f));
        g.drawDashedLine ({ { r.getX(), y }, { r.getRight(), y } }, d, 2, 1.0f);
    }

    // --- per-band width lines (the look the user liked: feathered glow) ---
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    for (int b = 0; b < N; ++b)
    {
        const float w = bandWidth (b);
        const float y = widthToY (w);
        const float x0 = bandLeftX (b), x1 = bandRightX (b);
        const float act = widthA[b];
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

    // --- crossover-drag band-pass hint (#19) ----------------------------
    if (dragHandle >= 0 && dragHandle < N - 1)
    {
        const int i = dragHandle;
        const float fx = freqToX (crossover (i));
        const float lx = bandLeftX (i), rx = bandRightX (i + 1);
        const float yHi = r.getCentreY() + 6.0f;   // band "shelf" level (lower half)
        const float yLo = laneBot() + 2.0f;        // crossover dip
        juce::Path lo, hi;
        lo.startNewSubPath (lx, yHi);
        lo.quadraticTo (juce::jmax (lx, fx - (fx - lx) * 0.5f), yHi, fx, yLo);
        hi.startNewSubPath (fx, yLo);
        hi.quadraticTo (juce::jmin (rx, fx + (rx - fx) * 0.5f), yHi, rx, yHi);
        g.setColour (bandLo.withAlpha (0.5f));
        g.strokePath (lo, juce::PathStrokeType (1.6f));
        g.setColour (bandHi.withAlpha (0.5f));
        g.strokePath (hi, juce::PathStrokeType (1.6f));
    }

    // --- crossover splits ------------------------------------------------
    for (int i = 0; i < N - 1; ++i)
    {
        const float x = freqToX (crossover (i));
        const float act = handleA[i];
        const bool  editing = (editingHandle == i);
        // Connected to the bottom edge at rest; breaks for the number on hover (#12).
        const float bottomY = (act > 0.05f || editing) ? rulerY() - 9.0f : r.getBottom() - 2.0f;
        const float capTop = r.getY() + 1.0f, capH = 12.0f;

        // Feathered glow like the width line (#7).
        if (act > 0.01f)
            juce::DropShadow (xoverCol.withAlpha (0.55f * act), 9, {})
                .drawForRectangle (g, juce::Rectangle<int> ((int) x - 1, (int) capTop, 3, (int) (bottomY - capTop)));

        g.setColour (colours::text.withAlpha (0.45f).interpolatedWith (xoverCol, juce::jlimit (0.0f, 1.0f, act)));
        g.drawLine (x, capTop + capH * 0.4f, x, bottomY, 1.2f + 0.9f * act);

        // Integrated cap: a rounded "bead" the line runs into, not a floating dot (#9).
        auto cap = juce::Rectangle<float> (x - 5.0f, capTop, 10.0f, capH);
        g.setGradientFill (juce::ColourGradient (xoverCol.brighter (0.25f + 0.3f * act), 0.0f, cap.getY(),
                                                 xoverCol.withMultipliedBrightness (0.7f), 0.0f, cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.25f + 0.4f * act));
        g.drawRoundedRectangle (cap, 4.0f, 1.0f);

        // Freq readout chip in the bottom break (hidden while typing).
        if (act > 0.05f && ! editing)
        {
            auto nb = numberChip (i);
            g.setColour (colours::bgPanel.withAlpha (0.85f * act));
            g.fillRoundedRectangle (nb, 3.0f);
            g.setFont (juce::Font (juce::FontOptions (10.0f)));
            g.setColour (colours::text.brighter (0.3f).withAlpha (act));
            g.drawText (freqText (crossover (i)), nb, juce::Justification::centred);
        }

        // Delete x to the split's right (#6).
        const float da = delA[i];
        if (da > 0.02f)
        {
            auto db = deleteBox (i);
            g.setColour (colours::bgPanel.withAlpha (0.6f * da));
            g.fillEllipse (db);
            g.setColour (colours::textDim.withAlpha (0.9f * da));
            const float pad = 3.5f;
            g.drawLine (db.getX() + pad, db.getY() + pad, db.getRight() - pad, db.getBottom() - pad, 1.4f);
            g.drawLine (db.getRight() - pad, db.getY() + pad, db.getX() + pad, db.getBottom() - pad, 1.4f);
        }
    }

    // --- add-band hint (#13: full-height dashed line + big "+" at the top) ---
    if (addA > 0.02f && N < 4)
    {
        const float x = juce::jlimit (r.getX(), r.getRight(), addX);
        const float a = addA;
        float dl[2] = { 3.0f, 3.0f };
        g.setColour (xoverCol.withAlpha (0.55f * a));
        g.drawDashedLine ({ { x, r.getY() + 2.0f }, { x, rulerY() - 9.0f } }, dl, 2, 1.3f);

        const float py = r.getY() + 9.0f, arm = 6.0f; // big "+" hugging the top edge
        g.setColour (xoverCol.withAlpha (0.95f * a));
        g.drawLine (x - arm, py, x + arm, py, 2.0f);
        g.drawLine (x, py - arm, x, py + arm, 2.0f);

        auto nb = numberChip (0).withX (x - 22.0f);
        g.setColour (colours::bgPanel.withAlpha (0.85f * a));
        g.fillRoundedRectangle (nb, 3.0f);
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.setColour (xoverCol.brighter (0.3f).withAlpha (a));
        g.drawText (freqText (xToFreq (x)), nb, juce::Justification::centred);
    }

    // --- disabled wash (#20) --------------------------------------------
    if (enaA < 0.999f)
    {
        g.setColour (colours::bg.withAlpha (0.5f * (1.0f - enaA)));
        g.fillRect (r);
    }
}

// ----------------------------------------------------------------------------
//  Interaction
// ----------------------------------------------------------------------------
void SpectrumImager::updateHover (juce::Point<float> p)
{
    const int N = bandCount();
    hoverHandle = hoverWidth = hoverAdd = hoverDelete = -1;

    const int overX = deleteHit (p);
    const int h = handleNearX (p.x);
    const int b = bandAtX (p.x);

    if (overX >= 0)            { hoverHandle = overX; hoverDelete = overX; }
    else if (h >= 0)           { hoverHandle = h; if (N > 1) hoverDelete = h; }
    else if (nearWidthLine (p, b)) hoverWidth = b;
    else if (N < 4)            { hoverAdd = b; addX = p.x; }

    if (hoverHandle >= 0)      setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else if (hoverWidth >= 0)  setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    else if (hoverAdd >= 0)    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    else                       setMouseCursor (juce::MouseCursor::NormalCursor);
}

void SpectrumImager::mouseMove (const juce::MouseEvent& e)
{
    if ((scrollHandle >= 0 || scrollBand >= 0) && e.position.getDistanceFrom (scrollAnchor) > 3.0f)
        scrollHandle = scrollBand = -1; // a real move releases the scroll latch (#4)
    updateHover (e.position);
}

void SpectrumImager::mouseExit (const juce::MouseEvent&)
{
    hoverHandle = hoverWidth = hoverAdd = hoverDelete = -1;
    scrollHandle = scrollBand = -1;
}

void SpectrumImager::mouseDown (const juce::MouseEvent& e)
{
    if (editingHandle >= 0) commitFreqEditor();

    const auto p = e.position;
    const int N = bandCount();
    const bool alt = e.mods.isAltDown();

    if (! alt)
        if (const int overX = deleteHit (p); overX >= 0) { removeCrossover (overX); updateHover (p); return; }

    const int h = handleNearX (p.x);

    if (alt) // Option/Alt-click resets, like the knobs (#6 prior / #18 clamp)
    {
        if (h >= 0) resetCrossover (h);
        else { const int b = bandAtX (p.x); if (nearWidthLine (p, b)) resetParam (widthP[b]); }
        return;
    }

    if (h >= 0) { dragHandle = h; dragBand = -1; beginGesture (freqP[h]); repaint(); return; }

    const int b = bandAtX (p.x);
    if (nearWidthLine (p, b)) // width drag only near the line (#15)
    {
        dragBand = b; dragHandle = -1;
        beginGesture (widthP[b]);
        setParam (widthP[b], yToWidth (p.y));
        repaint();
        return;
    }

    if (N < 4) // anywhere else in a band = add a split, and keep dragging it (#14/#16)
    {
        const int idx = addBandAt (xToFreq (p.x));
        if (idx >= 0) { dragHandle = idx; dragBand = -1; beginGesture (freqP[idx]); }
        repaint();
    }
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
    const auto p = e.position;
    const int N = bandCount();

    for (int i = 0; i < N - 1; ++i)        // double-click the number -> type a frequency (#5)
        if (numberChip (i).contains (p)) { openFreqEditor (i); return; }

    const int h = handleNearX (p.x);
    if (h >= 0) resetCrossover (h);
    else { const int b = bandAtX (p.x); if (nearWidthLine (p, b)) resetParam (widthP[b]); }
}

void SpectrumImager::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int N = bandCount();
    if (scrollHandle < 0 && scrollBand < 0)
    {
        const int h = handleNearX ((float) e.position.x);
        if (h >= 0) scrollHandle = h;
        else        scrollBand = bandAtX ((float) e.position.x);
        scrollAnchor = e.position;
    }

    const float dy = (wheel.isReversed ? -1.0f : 1.0f) * wheel.deltaY;
    if (std::abs (dy) < 1.0e-4f) return;
    const float sgn = dy > 0.0f ? 1.0f : -1.0f;

    if (scrollHandle >= 0 && scrollHandle < N - 1)
    {
        float f = crossover (scrollHandle) * std::pow (2.0f, dy * 0.3f);
        if (scrollHandle > 0)     f = juce::jmax (f, crossover (scrollHandle - 1) * 1.05f);
        if (scrollHandle < N - 2) f = juce::jmin (f, crossover (scrollHandle + 1) * 0.95f);
        setParam (freqP[scrollHandle], juce::jlimit (kFreqLo, kFreqHi, f));
    }
    else if (scrollBand >= 0 && scrollBand < N) // 1 %/notch (#17)
    {
        setParam (widthP[scrollBand], juce::jlimit (0.0f, 2.0f, bandWidth (scrollBand) + sgn * 0.01f));
    }
    repaint();
}

} // namespace anamorph::gui
