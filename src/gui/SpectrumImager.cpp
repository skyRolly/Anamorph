#include "SpectrumImager.h"
#include "LookAndFeel.h"
#include "PluginParameters.h"
#include <cmath>

namespace anamorph::gui
{

static constexpr float kFreqLo = 20.0f, kFreqHi = 20000.0f;
static constexpr float kMinDb = -90.0f, kMaxDb = 0.0f;
static constexpr float kWidthGrab = 8.0f;
static constexpr float kMinGapPx  = 48.0f; // smallest band width / edge gap (0.6.9 #1)

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
        return f >= 1000.0f ? juce::String (f / 1000.0f, 2) + "k" : juce::String (juce::roundToInt (f));
    }
}

SpectrumImager::SpectrumImager (anamorph::ScopeBuffer& s, juce::AudioProcessorValueTreeState& a)
    : scope (s), apvts (a)
{
    bandsP    = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (pid::mbBands));
    soloP     = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (pid::mbSolo));
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
float SpectrumImager::laneTop() const noexcept { return plot().getY() + 24.0f; }   // below the solo / handle row
float SpectrumImager::laneBot() const noexcept { return rulerY() - 22.0f; }        // above the delete x / ruler row

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

int SpectrumImager::soloMask() const noexcept
{
    if (auto* p = soloP) return (int) std::lround (p->convertFrom0to1 (p->getValue())) & 0x0F;
    return 0;
}
bool SpectrumImager::bandSoloed (int b) const noexcept { return (soloMask() & (1 << b)) != 0; }

float SpectrumImager::bandLeftX  (int b) const noexcept { return b <= 0 ? plot().getX() : freqToX (crossover (b - 1)); }
float SpectrumImager::bandRightX (int b) const noexcept { return b >= bandCount() - 1 ? plot().getRight() : freqToX (crossover (b)); }

juce::Rectangle<float> SpectrumImager::deleteBox (int b) const noexcept
{
    return { bandLeftX (b) + 4.0f, rulerY() - 18.0f, 12.0f, 12.0f };
}
juce::Rectangle<float> SpectrumImager::soloBox (int b) const noexcept
{
    const float cx = 0.5f * (bandLeftX (b) + bandRightX (b));
    return { cx - 9.0f, plot().getY() + 4.0f, 18.0f, 14.0f };
}
juce::Rectangle<float> SpectrumImager::numberChip (int i) const noexcept
{
    return { freqToX (crossover (i)) - 22.0f, rulerY() - 6.0f, 44.0f, 13.0f }; // share the ruler baseline (#3)
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
    if (N <= 1) return -1;
    for (int b = 0; b < N; ++b)
        if (deleteBox (b).contains (p)) return b;
    return -1;
}
int SpectrumImager::soloHit (juce::Point<float> p) const noexcept
{
    const int N = bandCount();
    for (int b = 0; b < N; ++b)
        if ((bandRightX (b) - bandLeftX (b)) > 30.0f && soloBox (b).contains (p)) return b;
    return -1;
}

// ----------------------------------------------------------------------------
//  Minimum-gap clamps (0.6.9 #1)
// ----------------------------------------------------------------------------
float SpectrumImager::clampHandleX (int i, float x) const noexcept
{
    const int N = bandCount();
    auto r = plot();
    const float lo = (i > 0     ? freqToX (crossover (i - 1)) : r.getX())     + kMinGapPx;
    const float hi = (i < N - 2 ? freqToX (crossover (i + 1)) : r.getRight()) - kMinGapPx;
    if (lo >= hi) return 0.5f * (lo + hi);
    return juce::jlimit (lo, hi, x);
}
float SpectrumImager::clampHandleFreq (int i, float hz) const noexcept
{
    return xToFreq (clampHandleX (i, freqToX (hz)));
}
bool SpectrumImager::bandAddTarget (int b, float x, float& outX) const noexcept
{
    const int N = bandCount();
    if (N >= 4) return false;
    auto r = plot();
    const float lo = (b > 0     ? freqToX (crossover (b - 1)) : r.getX())     + kMinGapPx;
    const float hi = (b < N - 1 ? freqToX (crossover (b))     : r.getRight()) - kMinGapPx;
    if (lo >= hi) return false;
    outX = juce::jlimit (lo, hi, x);
    return true;
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

// --- Solo mask (0.6.9 #7/#8) -------------------------------------------------
void SpectrumImager::setSoloMask (int mask)
{
    if (soloP == nullptr) return;
    mask &= 0x0F;
    soloP->beginChangeGesture();
    soloP->setValueNotifyingHost (soloP->convertTo0to1 ((float) mask));
    soloP->endChangeGesture();
}
void SpectrumImager::toggleSoloBit (int b) { setSoloMask (soloMask() ^ (1 << b)); }

void SpectrumImager::beginBandMove (int b)
{
    const int N = bandCount();
    soloMoveLeft  = (b > 0)     ? b - 1 : -1;
    soloMoveRight = (b < N - 1) ? b     : -1;
    if (soloMoveLeft  >= 0) beginGesture (freqP[soloMoveLeft]);
    if (soloMoveRight >= 0) beginGesture (freqP[soloMoveRight]);
    soloMoveGesture = (soloMoveLeft >= 0 || soloMoveRight >= 0);
}
void SpectrumImager::moveBand (float fromX, float toX)
{
    const float ratio = xToFreq (toX) / xToFreq (fromX);
    if (! (ratio > 0.0f)) return;
    auto moveOne = [&] (int idx)
    {
        if (idx < 0) return;
        const float nf = clampHandleFreq (idx, crossover (idx) * ratio);
        setParam (freqP[idx], juce::jlimit (kFreqLo, kFreqHi, nf));
    };
    // Move the leading edge first so the trailing edge clamps against its new spot.
    if (ratio >= 1.0f) { moveOne (soloMoveRight); moveOne (soloMoveLeft); }
    else               { moveOne (soloMoveLeft);  moveOne (soloMoveRight); }
}
void SpectrumImager::endBandMove()
{
    if (soloMoveLeft  >= 0) endGesture (freqP[soloMoveLeft]);
    if (soloMoveRight >= 0) endGesture (freqP[soloMoveRight]);
    soloMoveLeft = soloMoveRight = -1;
    soloMoveGesture = false;
}

void SpectrumImager::resetCrossover (int i)
{
    auto* p = (i >= 0 && i < 3) ? freqP[i] : nullptr;
    if (p == nullptr) return;
    const float def = clampHandleFreq (i, p->convertFrom0to1 (p->getDefaultValue()));
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

    // Pixel-gap clamp against the band the split lands in -- no room means no add (#1).
    auto r = plot();
    const float xlo = (ins > 0     ? freqToX (fr[ins - 1]) : r.getX())     + kMinGapPx;
    const float xhi = (ins < N - 1 ? freqToX (fr[ins])     : r.getRight()) - kMinGapPx;
    if (xlo >= xhi) return -1;
    hz = xToFreq (juce::jlimit (xlo, xhi, freqToX (hz)));

    float nf[3], nw[4];
    for (int i = 0; i < N;  ++i) nf[i] = (i < ins) ? fr[i] : (i == ins ? hz : fr[i - 1]);
    for (int i = 0; i <= N; ++i) nw[i] = (i <= ins) ? wd[i] : wd[i - 1];

    // Solo mask: splitting band `ins` keeps both halves soloed if it was (#7).
    const int oldMask = soloMask();
    int nm = 0;
    for (int i = 0; i < N; ++i)
        if (oldMask & (1 << i))
        {
            nm |= (1 << (i < ins ? i : i + 1));
            if (i == ins) nm |= (1 << ins);
        }
    setSoloMask (nm);

    for (int i = 0; i <= N; ++i) setParam (widthP[i], nw[i]);
    for (int i = 0; i < N;  ++i) setParam (freqP[i],  nf[i]);
    setBands (N + 1);
    return ins;
}

void SpectrumImager::removeBand (int b)
{
    const int N = bandCount();
    if (N <= 1) return;
    b = juce::jlimit (0, N - 1, b);
    // Delete the crossover on this band's LEFT (the leftmost band drops the first
    // split instead); the opposite region expands over it, keeping its own width /
    // solo (0.6.9 #12).
    const int dropX = (b == 0) ? 0 : (b - 1);

    float fr[3], wd[4];
    for (int k = 0; k < 3; ++k) fr[k] = crossover (k);
    for (int k = 0; k < 4; ++k) wd[k] = bandWidth (k);

    float nf[3], nw[4];
    for (int k = 0, j = 0; k < N;     ++k) if (k != b)     nw[j++] = wd[k]; // drop the deleted band's width
    for (int k = 0, j = 0; k < N - 1; ++k) if (k != dropX) nf[j++] = fr[k];

    // Solo mask: drop bit b, shift higher bits down.
    const int oldMask = soloMask();
    int nm = 0;
    for (int k = 0, j = 0; k < N; ++k)
    {
        if (k == b) continue;
        if (oldMask & (1 << k)) nm |= (1 << j);
        ++j;
    }
    setSoloMask (nm);

    for (int k = 0; k < N - 1; ++k) setParam (widthP[k], nw[k]);
    for (int k = 0; k < N - 2; ++k) setParam (freqP[k],  nf[k]);
    setBands (N - 1);
}

// ----------------------------------------------------------------------------
//  Frequency text editor
// ----------------------------------------------------------------------------
float SpectrumImager::parseFreq (const juce::String& t)
{
    auto s = t.toLowerCase().trim();
    const bool k = s.containsChar ('k');
    const float v = s.removeCharacters ("khz ").getFloatValue();
    if (k) return v * 1000.0f;
    return (v <= 20.0f) ? v * 1000.0f : v;
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
    auto chip = numberChip (i).expanded (6.0f, 4.0f);
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
    const float hz = clampHandleFreq (i, parseFreq (freqEditor->getText()));
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
    return 0.5f * ((2.0f * m1) + (-m0 + m2) * t
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
        return magCubic (0.5f * (fa + fb) / binHz);
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

    // Press-and-hold on a solo headphone -> momentary audition of that band (#8).
    if (soloPressBand >= 0 && ! soloHoldActive && ! soloMovedBand
        && juce::Time::getMillisecondCounter() - soloPressMs > 200u)
    {
        soloHoldActive = true;
        setSoloMask (1 << soloPressBand);
    }

    const bool animOn = animOnP == nullptr || animOnP->load() > 0.5f;
    const float dt   = 1.0f / 60.0f;
    const float rIn  = animOn ? 1.0f - std::exp (-dt / 0.075f) : 1.0f;
    const float rOut = animOn ? 1.0f - std::exp (-dt / 0.150f) : 1.0f;
    auto ease = [&] (float& v, float t) { v += (t - v) * (t > v ? rIn : rOut); };

    for (int i = 0; i < 3; ++i) ease (handleA[i], (i == dragHandle || i == hoverHandle) ? 1.0f : 0.0f);
    for (int i = 0; i < 3; ++i) ease (pressA[i],  (i == dragHandle) ? 1.0f : 0.0f);
    for (int i = 0; i < 4; ++i) ease (widthA[i],  (i == dragBand   || i == hoverWidth)  ? 1.0f : 0.0f);
    for (int i = 0; i < 4; ++i) ease (pressW[i],  (i == dragBand) ? 1.0f : 0.0f);
    for (int i = 0; i < 4; ++i) ease (delA[i],    (i == hoverDelete) ? 1.0f : 0.0f);
    for (int i = 0; i < 4; ++i) ease (soloA[i],   bandSoloed (i) ? 1.0f : (i == hoverSolo ? 0.55f : 0.0f));
    for (int i = 0; i < 4; ++i) ease (labelFlipA[i], (widthToY (bandWidth (i)) < laneTop() + 20.0f) ? 1.0f : 0.0f);
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
    glass::fillPanel (g, getLocalBounds().toFloat(), 6.0f, colours::bgPanel.darker (0.42f), 1.0f);

    juce::Graphics::ScopedSaveState save (g);
    juce::Path clip; clip.addRoundedRectangle (r, 5.0f);
    g.reduceClipRegion (clip);

    const int N = bandCount();
    const int amask = soloMask() & ((1 << N) - 1);
    const bool anySolo = amask != 0;
    const juce::Colour bandLo (0xff5aa6ff), bandHi (0xff35d0c0);
    const juce::Colour xoverCol = colours::accent;

    auto bandCol = [&] (int b) { return bandLo.interpolatedWith (bandHi, juce::jlimit (0.0f, 1.0f, bandWidth (b) * 0.5f)); };

    // --- band tints (soloed bands glow, the muted ones dim) -------------
    for (int b = 0; b < N; ++b)
    {
        const float x0 = bandLeftX (b), x1 = bandRightX (b);
        float a = 0.04f + 0.05f * juce::jlimit (0.0f, 2.0f, bandWidth (b)) * 0.5f + 0.06f * widthA[b];
        if (anySolo) a = (amask & (1 << b)) ? a + 0.10f : a * 0.4f;
        g.setColour (bandCol (b).withAlpha (a));
        g.fillRect (juce::Rectangle<float> (x0, r.getY(), juce::jmax (0.0f, x1 - x0), r.getHeight()));
    }

    // --- frequency grid -------------------------------------------------
    for (auto& t : kTicks)
    {
        const float x = freqToX (t.f);
        g.setColour (colours::outline.withAlpha (t.major ? 0.30f : 0.15f));
        g.drawVerticalLine (juce::roundToInt (x), r.getY(), rulerY() + 2.0f);
    }

    // --- spectrum (cubic-smoothed; floor sunk below the frame) ----------
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
        g.setGradientFill (juce::ColourGradient (xoverCol.withAlpha (0.20f), 0.0f, r.getY(),
                                                 xoverCol.withAlpha (0.012f), 0.0f, r.getBottom(), false));
        g.fillPath (fillPath);
        g.setColour (xoverCol.withAlpha (0.55f));
        g.strokePath (spec, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // --- width lane guides: faint floor + unity dashed line -------------
    {
        g.setColour (colours::outline.withAlpha (0.22f));
        g.drawLine (r.getX() + 2.0f, laneBot(), r.getRight() - 2.0f, laneBot(), 1.0f);
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
        const float act = widthA[b];
        const float pr  = pressW[b];
        const auto col = bandCol (b);

        if (act > 0.01f || pr > 0.01f)
            juce::DropShadow (col.withAlpha (0.4f * act + 0.45f * pr), 8, {})
                .drawForRectangle (g, juce::Rectangle<int> ((int) x0, (int) (y - 2.0f), (int) (x1 - x0), 4));
        g.setColour (col.withAlpha (0.55f + 0.4f * juce::jmax (act, pr)).brighter (0.2f * pr));
        g.drawLine (x0 + 3.0f, y, x1 - 3.0f, y, 1.6f + 0.8f * act + 0.7f * pr);

        const float cx = 0.5f * (x0 + x1);
        g.setColour (col.brighter (0.2f * juce::jmax (act, pr)));
        g.fillEllipse (cx - 3.0f, y - 3.0f, 6.0f, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.2f + 0.35f * juce::jmax (act, pr)));
        g.drawEllipse (cx - 3.0f, y - 3.0f, 6.0f, 6.0f, 1.0f);

        if (juce::jmax (act, pr) > 0.2f && (x1 - x0) > 40.0f)
        {
            // Flip the % readout below the bar when it would clip past the top (#3).
            const float flip = labelFlipA[b];
            const float ly = (y - 17.0f) + flip * 22.0f;
            g.setColour (colours::text.withAlpha (juce::jmax (act, pr)));
            g.drawText (juce::String (juce::roundToInt (w * 100.0f)) + "%",
                        juce::Rectangle<float> (cx - 26.0f, ly, 52.0f, 13.0f), juce::Justification::centred);
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

    // --- crossover-drag band-pass response (#10/#11) --------------------
    // Two mirrored band-pass curves for the bands either side of the dragged split,
    // crossing at -6 dB on the divider; the outermost edges run flat to the frame.
    auto bandCurve = [&] (int b, juce::Colour col, float alpha)
    {
        const float Lf = (b > 0)     ? crossover (b - 1) : -1.0f;
        const float Rf = (b < N - 1) ? crossover (b)     : -1.0f;
        const float slope = 1.9f;       // ~ gentle 12 dB/oct visual roll-off
        const float yPass = laneTop() + 5.0f;
        const float yFloor = laneBot();
        const float range = 26.0f;      // dB mapped across the lane
        auto yAt = [&] (float x)
        {
            const float f = xToFreq (x);
            float amp = 1.0f;
            if (Lf > 0.0f) amp *= 1.0f / (1.0f + std::pow (2.0f, -slope * std::log2 (f / Lf)));
            if (Rf > 0.0f) amp *= 1.0f / (1.0f + std::pow (2.0f,  slope * std::log2 (f / Rf)));
            const float db = 20.0f * std::log10 (juce::jmax (1.0e-4f, amp));
            const float t = juce::jlimit (0.0f, 1.0f, -db / range);
            return yPass + t * (yFloor - yPass);
        };
        juce::Path p;
        bool started = false;
        for (float x = r.getX(); x <= r.getRight(); x += 2.0f)
        {
            const float y = yAt (x);
            if (! started) { p.startNewSubPath (x, y); started = true; } else p.lineTo (x, y);
        }
        juce::Path f (p);
        f.lineTo (r.getRight(), yFloor + 2.0f);
        f.lineTo (r.getX(), yFloor + 2.0f);
        f.closeSubPath();
        g.setGradientFill (juce::ColourGradient (col.withAlpha (0.14f * alpha), 0.0f, yPass,
                                                 col.withAlpha (0.0f), 0.0f, yFloor, false));
        g.fillPath (f);
        g.setColour (col.withAlpha (0.16f * alpha)); g.strokePath (p, juce::PathStrokeType (3.0f));
        g.setColour (col.withAlpha (0.90f * alpha)); g.strokePath (p, juce::PathStrokeType (1.3f));
    };
    for (int i = 0; i < N - 1; ++i)
        if (pressA[i] > 0.01f)        // fades in on grab, out on release, re-grab continues (#10)
        {
            bandCurve (i,     bandLo, pressA[i]);
            bandCurve (i + 1, bandHi, pressA[i]);
        }

    // --- per-band solo headphones + delete x ----------------------------
    auto paintHeadphone = [&] (juce::Rectangle<float> bx, juce::Colour c)
    {
        auto cup = bx.reduced (1.5f, 1.0f);
        const float cx = cup.getCentreX();
        const float rx = cup.getWidth() * 0.5f;
        const float ry = cup.getHeight() * 0.5f;
        const float cy = cup.getY() + ry * 0.9f;
        juce::Path band;
        band.addCentredArc (cx, cy, rx, ry, 0.0f, -1.35f, 1.35f, true); // proper top headband (#6)
        g.setColour (c);
        g.strokePath (band, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float ex = rx * std::sin (1.35f);
        const float ey = cy - ry * std::cos (1.35f);
        const float cupW = 3.4f, cupH = 5.0f;
        g.fillRoundedRectangle (cx - ex - cupW * 0.5f, ey, cupW, cupH, 1.4f);
        g.fillRoundedRectangle (cx + ex - cupW * 0.5f, ey, cupW, cupH, 1.4f);
    };
    for (int b = 0; b < N; ++b)
    {
        if ((bandRightX (b) - bandLeftX (b)) > 30.0f)
        {
            const bool on = (amask & (1 << b)) != 0;
            const float sa = soloA[b];
            const auto c = on ? xoverCol : colours::textDim.withAlpha (0.4f + 0.5f * sa);
            paintHeadphone (soloBox (b), c);
        }
        if (delA[b] > 0.02f)
        {
            auto db = deleteBox (b);
            const float pad = 3.0f, a = delA[b];
            g.setColour (colours::textDim.withAlpha (0.8f * a));
            g.drawLine (db.getX() + pad, db.getY() + pad, db.getRight() - pad, db.getBottom() - pad, 1.4f);
            g.drawLine (db.getRight() - pad, db.getY() + pad, db.getX() + pad, db.getBottom() - pad, 1.4f);
        }
    }

    // --- crossover splits (line + 4-way glow + marker + number) ---------
    for (int i = 0; i < N - 1; ++i)
    {
        const float x = freqToX (crossover (i));
        const float act = handleA[i], press = pressA[i];
        const bool  editing = (editingHandle == i);
        const float lineTop = r.getY() + 18.0f; // line starts at the marker tip
        const float breakY = rulerY() - 9.0f;

        // Even glow in all directions (a blurred capsule), drawn BELOW the marker so
        // the handle occludes it at the top (#13); stronger while held (#16).
        const float glowA = juce::jlimit (0.0f, 0.85f, 0.42f * act + 0.5f * press);
        if (glowA > 0.01f)
        {
            juce::Path ls; ls.addRoundedRectangle (x - 0.9f, lineTop, 1.8f, breakY - lineTop, 0.9f);
            juce::DropShadow (xoverCol.withAlpha (glowA), (int) (6.0f + 4.0f * press), {}).drawForPath (g, ls);
        }

        const auto lineCol = colours::text.withAlpha (0.45f).interpolatedWith (xoverCol, juce::jlimit (0.0f, 1.0f, act));
        g.setColour (lineCol);
        g.drawLine (x, lineTop, x, breakY, 1.2f + 0.9f * act);
        if (act < 0.99f && ! editing)
        {
            g.setColour (lineCol.withMultipliedAlpha (1.0f - act));
            g.drawLine (x, breakY, x, r.getBottom() - 2.0f, 1.2f);
        }

        // Marker handle: a downward "pin" the line runs out of. Press now widens it
        // only a little; the extra feedback comes from the glow instead (#16).
        {
            const float hw = 5.0f + 1.1f * act + 0.8f * press;
            const float top = r.getY() + 2.0f;
            const float bodyBot = top + 7.0f + 1.0f * press;
            const float tip = lineTop + 1.0f;
            const float rad = 2.5f;
            juce::Path m;
            m.startNewSubPath (x - hw, top + rad);
            m.quadraticTo (x - hw, top, x - hw + rad, top);
            m.lineTo (x + hw - rad, top);
            m.quadraticTo (x + hw, top, x + hw, top + rad);
            m.lineTo (x + hw, bodyBot);
            m.lineTo (x, tip);
            m.lineTo (x - hw, bodyBot);
            m.closeSubPath();

            if (act > 0.02f || press > 0.02f)
                juce::DropShadow (xoverCol.withAlpha (0.45f * act + 0.5f * press), (int) (7.0f + 3.0f * press), {})
                    .drawForPath (g, m);
            g.setGradientFill (juce::ColourGradient (xoverCol.brighter (0.35f + 0.3f * press), 0.0f, top,
                                                     xoverCol.withMultipliedBrightness (0.65f), 0.0f, bodyBot, false));
            g.fillPath (m);
            g.setColour (juce::Colours::white.withAlpha (0.25f + 0.45f * juce::jmax (act, press)));
            g.strokePath (m, juce::PathStrokeType (1.0f));
        }

        // Freq readout chip in the bottom break (aligned to the ruler, fades in).
        if (act > 0.05f && ! editing)
        {
            auto nb = numberChip (i);
            g.setColour (colours::bgPanel.withAlpha (0.9f * act));
            g.fillRoundedRectangle (nb.expanded (1.0f, 1.0f), 3.0f);
            g.setFont (juce::Font (juce::FontOptions (9.5f)));
            g.setColour (colours::text.brighter (0.35f).withAlpha (act));
            g.drawText (freqText (crossover (i)), nb, juce::Justification::centred);
        }
    }

    // --- add-band hint: big "+" hugging the top, dashed line breaks just below it (#2/#13) ---
    if (addA > 0.02f && N < 4)
    {
        const float x = juce::jlimit (r.getX(), r.getRight(), addX);
        const float a = addA;
        const float py = r.getY() + 9.0f, arm = 6.0f;
        g.setColour (xoverCol.withAlpha (0.95f * a));
        g.drawLine (x - arm, py, x + arm, py, 2.0f);
        g.drawLine (x, py - arm, x, py + arm, 2.0f);
        float dl[2] = { 3.0f, 3.0f };
        g.setColour (xoverCol.withAlpha (0.55f * a));
        g.drawDashedLine ({ { x, py + arm + 4.0f }, { x, rulerY() - 9.0f } }, dl, 2, 1.3f); // small gap below "+"
        auto nb = numberChip (0).withX (x - 22.0f);
        g.setColour (colours::bgPanel.withAlpha (0.9f * a));
        g.fillRoundedRectangle (nb.expanded (1.0f, 1.0f), 3.0f);
        g.setFont (juce::Font (juce::FontOptions (9.5f)));
        g.setColour (xoverCol.brighter (0.3f).withAlpha (a));
        g.drawText (freqText (xToFreq (x)), nb, juce::Justification::centred);
    }

    // --- disabled wash (greys the whole box) ----------------------------
    if (enaA < 0.999f)
    {
        g.setColour (colours::bg.withAlpha (0.5f * (1.0f - enaA)));
        g.fillRect (r);
    }
}

// ----------------------------------------------------------------------------
//  Interaction
// ----------------------------------------------------------------------------
void SpectrumImager::setContextTooltip()
{
    const auto p = getMouseXYRelative().toFloat();
    juce::String t;
    if (hoverSolo >= 0)            t = "Solo band \xe2\x80\x94 click to latch, hold to audition, drag to move";
    else if (hoverHandle >= 0)     t = "Crossover \xe2\x80\x94 drag to move, double-click to type Hz, Alt-click resets";
    else if (hoverWidth >= 0)      t = "Band width \xe2\x80\x94 drag up to widen, down to narrow";
    else if (deleteHit (p) >= 0)   t = "Remove this band";
    else if (hoverAdd >= 0)        t = "Click to add a band split";
    else                           t = "Per-band stereo width across the spectrum";
    setTooltip (t);
}

void SpectrumImager::updateHover (juce::Point<float> p)
{
    const int N = bandCount();
    hoverHandle = hoverWidth = hoverAdd = hoverDelete = hoverSolo = -1;

    const int h = handleNearX (p.x);
    const int b = bandAtX (p.x);
    const int sh = soloHit (p);

    if (sh >= 0)                   { hoverSolo = sh; setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    else if (h >= 0)               { hoverHandle = h; setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); }
    else if (nearWidthLine (p, b)) { hoverWidth = b; setMouseCursor (juce::MouseCursor::UpDownResizeCursor); }
    else
    {
        float ax;
        if (bandAddTarget (b, p.x, ax)) { hoverAdd = b; addX = ax; setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        else                              setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    // Delete x shows in the lower part of a band (independent of the above).
    if (N > 1)
    {
        const int dh = deleteHit (p);
        const float lowerY = plot().getY() + plot().getHeight() * 0.6f;
        if (dh >= 0) hoverDelete = dh;
        else if (h < 0 && sh < 0 && p.y > lowerY) hoverDelete = b;
    }

    setContextTooltip();
}

void SpectrumImager::mouseMove (const juce::MouseEvent& e)
{
    if ((scrollHandle >= 0 || scrollBand >= 0) && e.position.getDistanceFrom (scrollAnchor) > 3.0f)
        scrollHandle = scrollBand = -1;
    updateHover (e.position);
}
void SpectrumImager::mouseExit (const juce::MouseEvent&)
{
    hoverHandle = hoverWidth = hoverAdd = hoverDelete = hoverSolo = -1;
    scrollHandle = scrollBand = -1;
}
void SpectrumImager::mouseDown (const juce::MouseEvent& e)
{
    if (editingHandle >= 0) commitFreqEditor();
    const auto p = e.position;
    const bool alt = e.mods.isAltDown();

    // Solo headphone: arm the press machine (click vs hold vs drag decided later).
    if (const int sh = soloHit (p); sh >= 0)
    {
        soloPressBand = sh;
        soloDownX = soloDragPrevX = p.x;
        soloPressMs = juce::Time::getMillisecondCounter();
        soloMaskSaved = soloMask();
        soloHoldActive = soloMovedBand = false;
        return;
    }

    if (! alt) if (const int dB = deleteHit (p); dB >= 0) { removeBand (dB); updateHover (p); return; }

    const int h = handleNearX (p.x);
    if (alt)
    {
        if (h >= 0) resetCrossover (h);
        else { const int b = bandAtX (p.x); if (nearWidthLine (p, b)) resetParam (widthP[b]); }
        return;
    }
    if (h >= 0) { dragHandle = h; dragBand = -1; beginGesture (freqP[h]); repaint(); return; }

    const int b = bandAtX (p.x);
    if (nearWidthLine (p, b))
    {
        dragBand = b; dragHandle = -1;
        beginGesture (widthP[b]);
        setParam (widthP[b], yToWidth (p.y));
        repaint();
        return;
    }
    float ax;
    if (bandAddTarget (b, p.x, ax))
    {
        const int idx = addBandAt (xToFreq (ax));
        if (idx >= 0) { dragHandle = idx; dragBand = -1; beginGesture (freqP[idx]); }
        hoverAdd = -1; addA = 0.0f; // snap away the preview so nothing lingers at the old spot (#5)
        repaint();
    }
}
void SpectrumImager::mouseDrag (const juce::MouseEvent& e)
{
    if (soloPressBand >= 0)
    {
        if (std::abs (e.position.x - soloDownX) > 4.0f)
        {
            if (! soloMovedBand)  { soloMovedBand = true; beginBandMove (soloPressBand); }
            if (! soloHoldActive) { soloHoldActive = true; setSoloMask (1 << soloPressBand); }
            moveBand (soloDragPrevX, e.position.x);
            soloDragPrevX = e.position.x;
        }
        return;
    }
    if (dragHandle >= 0)
    {
        const float x = clampHandleX (dragHandle, (float) e.position.x);
        setParam (freqP[dragHandle], juce::jlimit (kFreqLo, kFreqHi, xToFreq (x)));
    }
    else if (dragBand >= 0)
    {
        setParam (widthP[dragBand], yToWidth ((float) e.position.y));
    }
    repaint();
}
void SpectrumImager::mouseUp (const juce::MouseEvent& e)
{
    if (soloPressBand >= 0)
    {
        if (soloMovedBand)        { endBandMove(); setSoloMask (soloMaskSaved); }  // band moved -> restore solo
        else if (soloHoldActive)  { setSoloMask (soloMaskSaved); }                 // momentary audition ended (#8)
        else                      { setSoloMask (soloMaskSaved ^ (1 << soloPressBand)); } // quick click latches
        soloPressBand = -1;
        soloHoldActive = soloMovedBand = false;
        updateHover (e.position);
        repaint();
        return;
    }
    if (dragHandle >= 0) endGesture (freqP[dragHandle]);
    if (dragBand   >= 0) endGesture (widthP[dragBand]);
    dragHandle = dragBand = -1;
    repaint();
}
void SpectrumImager::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto p = e.position;
    const int N = bandCount();
    for (int i = 0; i < N - 1; ++i)
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
        const float f = clampHandleFreq (scrollHandle, crossover (scrollHandle) * std::pow (2.0f, dy * 0.3f));
        setParam (freqP[scrollHandle], juce::jlimit (kFreqLo, kFreqHi, f));
    }
    else if (scrollBand >= 0 && scrollBand < N)
    {
        // Velocity-aware: a slow notch nudges ~1%, a fast flick moves much more (#15).
        const float step = sgn * juce::jmax (0.01f, std::abs (dy) * 0.30f);
        setParam (widthP[scrollBand], juce::jlimit (0.0f, 2.0f, bandWidth (scrollBand) + step));
    }
    repaint();
}

} // namespace anamorph::gui
