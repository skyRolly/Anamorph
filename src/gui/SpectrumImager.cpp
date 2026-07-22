#include "SpectrumImager.h"
#include "LookAndFeel.h"
#include "PluginParameters.h"
#include <cmath>

namespace anamorph::gui
{

static constexpr float kFreqLo = 20.0f, kFreqHi = 20000.0f;
static constexpr float kMinDb = -90.0f, kMaxDb = 0.0f;
static constexpr float kWidthGrab = 8.0f;
static constexpr float kMinGapPx  = 46.0f; // constant on-screen split spacing (#1/#26)

namespace
{
    // The frequency axis is a hand-tuned warp: a table of (Hz -> 0..1 fraction)
    // anchors with 50 Hz the visual midpoint of 20-100 (0.6.11 #1). It is interpolated
    // with a monotone (Fritsch-Carlson) cubic so the axis is C1-smooth -- no slope
    // kink at an anchor, which is what made the band-pass curve look creased (#7).
    struct AnchorPt { float f; float x; };
    const AnchorPt kAxis[] = {
        {    20.0f, 0.000000f }, {    50.0f, 0.083333f }, {   100.0f, 0.166667f },
        {   300.0f, 0.333333f }, {   600.0f, 0.416667f }, {  1000.0f, 0.500000f },
        {  3000.0f, 0.666667f }, {  6000.0f, 0.791667f }, { 10000.0f, 0.875000f },
        { 20000.0f, 1.000000f }
    };
    constexpr int kAxisN = (int) (sizeof (kAxis) / sizeof (kAxis[0]));

    struct AxisMap
    {
        float s[kAxisN], u[kAxisN], m[kAxisN];
        AxisMap()
        {
            for (int i = 0; i < kAxisN; ++i) { s[i] = std::log (kAxis[i].f); u[i] = kAxis[i].x; }
            float d[kAxisN - 1];
            for (int i = 0; i < kAxisN - 1; ++i) d[i] = (u[i + 1] - u[i]) / (s[i + 1] - s[i]);
            m[0] = d[0]; m[kAxisN - 1] = d[kAxisN - 2];
            for (int i = 1; i < kAxisN - 1; ++i) m[i] = 0.5f * (d[i - 1] + d[i]);
            for (int i = 0; i < kAxisN - 1; ++i)            // Fritsch-Carlson monotonicity
            {
                if (! (d[i] > 0.0f)) { m[i] = 0.0f; m[i + 1] = 0.0f; } // anchors strictly increase, so this is the guard branch
                else
                {
                    const float a = m[i] / d[i], b = m[i + 1] / d[i];
                    const float t = a * a + b * b;
                    if (t > 9.0f) { const float k = 3.0f / std::sqrt (t); m[i] = k * a * d[i]; m[i + 1] = k * b * d[i]; }
                }
            }
        }
        float fracS (float ss) const noexcept
        {
            int i = 0; while (i < kAxisN - 2 && ss > s[i + 1]) ++i;
            const float h = s[i + 1] - s[i], t = (ss - s[i]) / h;
            const float t2 = t * t, t3 = t2 * t;
            return (2 * t3 - 3 * t2 + 1) * u[i] + (t3 - 2 * t2 + t) * h * m[i]
                 + (-2 * t3 + 3 * t2) * u[i + 1] + (t3 - t2) * h * m[i + 1];
        }
        float frac (float hz) const noexcept { return fracS (std::log (juce::jlimit (20.0f, 20000.0f, hz))); }
        float freq (float fr) const noexcept
        {
            fr = juce::jlimit (0.0f, 1.0f, fr);
            float lo = s[0], hi = s[kAxisN - 1];
            for (int it = 0; it < 30; ++it) { const float mid = 0.5f * (lo + hi); if (fracS (mid) < fr) lo = mid; else hi = mid; }
            return std::exp (0.5f * (lo + hi));
        }
    };
    const AxisMap kAxisMap;
    float fracForFreq (float hz) noexcept { return kAxisMap.frac (hz); }
    float freqForFrac (float fr) noexcept { return kAxisMap.freq (fr); }

    // Ruler ticks: which frequencies get a labelled number, and which are bright
    // anchors (0.6.10 #21). 20 / 20k are edges, unshown; 60 sits just right of the
    // 50 midpoint (0.6.11 #1).
    struct Tick { float f; const char* label; bool major; };
    const Tick kTicks[] = {
        { 60.0f, "60", false }, { 100.0f, "100", true }, { 300.0f, "300", false },
        { 600.0f, "600", false }, { 1000.0f, "1k", true }, { 3000.0f, "3k", false },
        { 6000.0f, "6k", false }, { 10000.0f, "10k", true }
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
    // UI-animation flag is injected via setAnimationSource() (it lives in InternalState,
    // not the APVTS, so the host can't see it). animOnP == nullptr -> animations on.
    enableP   = apvts.getRawParameterValue (pid::mbEnable);

    fifoL.assign ((size_t) fftSize, 0.0f);
    fifoR.assign ((size_t) fftSize, 0.0f);
    fftData.assign ((size_t) fftSize * 2, 0.0f);
    mags.assign ((size_t) fftSize / 2 + 1, kMinDb);
    magsDb.assign ((size_t) fftSize / 2 + 1, kMinDb); // = gainToDecibels of the all-zero fftData
    redLevel.assign ((size_t) fftSize / 2 + 1, 0.0f);

    // S2 gate init: treat whatever the ring already holds as non-silent, so
    // the first fftSize observed frames always analyse (conservative -- an
    // editor opened mid-playback shows the live spectrum immediately).
    lastSeenCount = lastNonZero = s.writeCount();

    enaA = enabled() ? 1.0f : 0.0f;
    lastBandCount = bandCount();
    for (int i = 0; i < 3; ++i) drawnF[i] = crossover (i);
    for (int b = 0; b < 4; ++b) drawnW[b] = bandWidth (b);

    setInterceptsMouseClicks (true, false);
    // Adaptive refresh: ride the display's vblank (capped ~120 Hz). Armed here and
    // then gated by visibility: the imager is Advanced-only, so its clock is
    // stopped whenever it is hidden (Simple mode -- the default) rather than
    // firing a per-vblank isShowing()-and-return. visibilityChanged() flips it as
    // the mode toggles; the in-tick S2 isShowing() gate still covers a whole-editor
    // hide (own-visibility unchanged). This is strictly less idle work than the old
    // fixed 60 Hz timer, which ran even while hidden.
    frameClock.start (*this, [this] (double dt) { tick (dt); });
}

void SpectrumImager::visibilityChanged()
{
    if (isVisible())
        frameClock.start (*this, [this] (double dt) { tick (dt); });
    else
    {
        frameClock.stop();
        // Force the S2 stale-spectrum reset (mags/redLevel -> floor) to run on the
        // first tick after the next show, exactly as the always-running tick did
        // when it early-returned on !isShowing().
        wasShowing = false;
    }
}

SpectrumImager::~SpectrumImager()
{
    frameClock.stop();
    if (onClearSoloPreview) onClearSoloPreview();
}

// ----------------------------------------------------------------------------
//  Geometry
// ----------------------------------------------------------------------------
juce::Rectangle<float> SpectrumImager::plot() const noexcept { return getLocalBounds().toFloat().reduced (1.0f); }

float SpectrumImager::freqToX (float hz) const noexcept
{
    auto r = plot();
    return r.getX() + fracForFreq (hz) * r.getWidth();
}
float SpectrumImager::xToFreq (float x) const noexcept
{
    auto r = plot();
    return freqForFrac ((x - r.getX()) / r.getWidth());
}
float SpectrumImager::rulerY()  const noexcept { return plot().getBottom() - 14.0f; }
float SpectrumImager::laneTop() const noexcept { return plot().getY() + 24.0f; }   // below the solo / handle row
float SpectrumImager::laneBot() const noexcept { return rulerY() - 24.0f; }        // above the delete x / ruler row

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
int  SpectrumImager::effectiveSoloMask() const noexcept
{
    return (soloHoldActive && soloPressBand >= 0) ? (1 << soloPressBand) : soloMask();
}

// Display (eased) reads: PAINT uses these so a reset / preset / A-B / undo travels.
float SpectrumImager::dispCrossover (int i) const noexcept { return (i >= 0 && i < 3) ? drawnF[i] : kFreqLo; }
float SpectrumImager::dispWidth (int b) const noexcept     { return (b >= 0 && b < 4) ? drawnW[b] : 1.0f; }
float SpectrumImager::dispLeftX  (int b) const noexcept { return b <= 0 ? plot().getX() : freqToX (dispCrossover (b - 1)); }
float SpectrumImager::dispRightX (int b) const noexcept { return b >= bandCount() - 1 ? plot().getRight() : freqToX (dispCrossover (b)); }

float SpectrumImager::bandLeftX  (int b) const noexcept { return b <= 0 ? plot().getX() : freqToX (crossover (b - 1)); }
float SpectrumImager::bandRightX (int b) const noexcept { return b >= bandCount() - 1 ? plot().getRight() : freqToX (crossover (b)); }

juce::Rectangle<float> SpectrumImager::deleteBox (int b) const noexcept
{
    // Sized close to the add "+", nudged up so it clears the freq chip below (#4).
    return { bandLeftX (b) + 5.0f, rulerY() - 22.0f, 14.0f, 14.0f };
}
juce::Rectangle<float> SpectrumImager::soloBox (int b) const noexcept
{
    const float cx = 0.5f * (bandLeftX (b) + bandRightX (b));
    return { cx - 9.0f, plot().getY() + 3.0f, 18.0f, 15.0f }; // balanced headphone proportions (#2)
}
juce::Rectangle<float> SpectrumImager::numberChip (int i) const noexcept
{
    return { freqToX (crossover (i)) - 22.0f, rulerY() - 6.0f, 44.0f, 13.0f };
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
//  Min-gap projection (constant on-screen spacing; pushes neighbours)
// ----------------------------------------------------------------------------
void SpectrumImager::projectGaps (float* xs, int count, int pin) const noexcept
{
    if (count <= 0) return;
    auto r = plot();
    const float lo = r.getX() + kMinGapPx, hi = r.getRight() - kMinGapPx;

    if (pin >= 0 && pin < count)
    {
        xs[pin] = juce::jlimit (lo, hi, xs[pin]);
        for (int i = pin - 1; i >= 0; --i)     xs[i] = juce::jmin (xs[i], xs[i + 1] - kMinGapPx);
        for (int i = pin + 1; i < count; ++i)  xs[i] = juce::jmax (xs[i], xs[i - 1] + kMinGapPx);
    }
    else
    {
        for (int i = 1; i < count; ++i) xs[i] = juce::jmax (xs[i], xs[i - 1] + kMinGapPx);
    }
    // Slide the whole cluster back inside the edges, keeping the gaps it just set.
    if (xs[0] < lo)             { const float d = lo - xs[0];             for (int i = 0; i < count; ++i) xs[i] += d; }
    if (xs[count - 1] > hi)     { const float d = xs[count - 1] - hi;     for (int i = 0; i < count; ++i) xs[i] -= d; }
    for (int i = 0; i < count; ++i) xs[i] = juce::jlimit (lo, hi, xs[i]);
}
void SpectrumImager::writeCrossovers (const float* xs, int count)
{
    for (int k = 0; k < count; ++k)
        if (std::abs (freqToX (crossover (k)) - xs[k]) > 0.5f)
            setParam (freqP[k], juce::jlimit (kFreqLo, kFreqHi, xToFreq (xs[k])));
}
// Pin pinA (and optional pinB) at their target x; every other split is pulled toward
// its drag-start position `orig`, only pushed aside as far as the min gap demands -- so a
// neighbour springs straight back to where it began the moment the pin clears it (#8-#11).
void SpectrumImager::projectFromOrig (float* out, const float* orig, int count,
                                      int pinA, float xA, int pinB, float xB) const noexcept
{
    auto r = plot();
    const float lo = r.getX() + kMinGapPx, hi = r.getRight() - kMinGapPx;
    for (int k = 0; k < count; ++k) out[k] = orig[k];
    if (pinA >= 0 && pinA < count) out[pinA] = juce::jlimit (lo, hi, xA);
    if (pinB >= 0 && pinB < count) out[pinB] = juce::jlimit (lo, hi, xB);

    const int leftPin  = (pinA >= 0 && pinB >= 0) ? juce::jmin (pinA, pinB) : juce::jmax (pinA, pinB);
    const int rightPin = juce::jmax (pinA, pinB);
    if (leftPin < 0) return;

    for (int k = leftPin - 1;  k >= 0;    --k) out[k] = juce::jmin (orig[k], out[k + 1] - kMinGapPx);
    for (int k = rightPin + 1; k < count; ++k) out[k] = juce::jmax (orig[k], out[k - 1] + kMinGapPx);

    // Safety: keep everything ordered and inside the edges (a no-op when the pins fit).
    out[0] = juce::jmax (out[0], lo);
    for (int k = 1; k < count; ++k)         out[k] = juce::jmax (out[k], out[k - 1] + kMinGapPx);
    out[count - 1] = juce::jmin (out[count - 1], hi);
    for (int k = count - 2; k >= 0; --k)    out[k] = juce::jmin (out[k], out[k + 1] - kMinGapPx);
}
void SpectrumImager::dragCrossoverTo (int handle, float x)
{
    const int M = bandCount() - 1;
    if (handle < 0 || handle >= M) return;
    float out[3];
    projectFromOrig (out, dragOrigX, M, handle, x, -1, 0.0f);
    writeCrossovers (out, M);
}
bool SpectrumImager::bandAddTarget (int b, float x, float& outX) const noexcept
{
    const int N = bandCount();
    if (N >= 4) return false;
    auto r = plot();
    // The whole band (minus a small inset) offers an add affordance; the actual
    // split lands at the clamped click and neighbours spread to fit (#25).
    const float lo = (b > 0     ? freqToX (crossover (b - 1)) : r.getX())     + 6.0f;
    const float hi = (b < N - 1 ? freqToX (crossover (b))     : r.getRight()) - 6.0f;
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
    if (p) { if (onSweep) onSweep(); p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); }
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

// --- Solo mask ---------------------------------------------------------------
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
    const int M = N - 1;
    auto r = plot();
    soloMoveLeft  = (b > 0)     ? b - 1 : -1;
    soloMoveRight = (b < N - 1) ? b     : -1;

    // Anchor the move: the band translates RIGIDLY by T = clamp(cursor - anchor) so each
    // split tracks the cursor 1:1, the band keeps its width while it pushes neighbours
    // aside, and a pushed neighbour springs back on the way out (0.6.13 #3/#8/#9/#10).
    bandAnchorX = soloDownX;
    for (int k = 0; k < M; ++k) dragOrigX[k] = freqToX (crossover (k));
    bandStartLeftX  = (soloMoveLeft  >= 0) ? dragOrigX[soloMoveLeft]  : r.getX();
    bandStartRightX = (soloMoveRight >= 0) ? dragOrigX[soloMoveRight] : r.getRight();

    // T range: the band may slide until its edge split (after packing every neighbour on
    // that side at the min gap) reaches the frame edge.
    bandTmin = -1.0e9f; bandTmax = 1.0e9f;
    if (soloMoveLeft >= 0)
        bandTmin = juce::jmax (bandTmin, (r.getX() + (float) (soloMoveLeft + 1) * kMinGapPx) - bandStartLeftX);
    else
        bandTmin = juce::jmax (bandTmin, (r.getX() + kMinGapPx) - bandStartRightX);
    if (soloMoveRight >= 0)
        bandTmax = juce::jmin (bandTmax, (r.getRight() - (float) (M - soloMoveRight) * kMinGapPx) - bandStartRightX);
    else
        bandTmax = juce::jmin (bandTmax, (r.getRight() - kMinGapPx) - bandStartLeftX);
    if (bandTmin > bandTmax) bandTmin = bandTmax = 0.0f;

    if (soloMoveLeft  >= 0) beginGesture (freqP[soloMoveLeft]);
    if (soloMoveRight >= 0) beginGesture (freqP[soloMoveRight]);
}
void SpectrumImager::moveBand (float mouseX)
{
    const int M = bandCount() - 1;
    if (M <= 0) return;
    const float T = juce::jlimit (bandTmin, bandTmax, mouseX - bandAnchorX);
    float out[3];
    projectFromOrig (out, dragOrigX, M, soloMoveLeft, bandStartLeftX + T, soloMoveRight, bandStartRightX + T);
    writeCrossovers (out, M);
}
void SpectrumImager::endBandMove()
{
    if (soloMoveLeft  >= 0) endGesture (freqP[soloMoveLeft]);
    if (soloMoveRight >= 0) endGesture (freqP[soloMoveRight]);
    soloMoveLeft = soloMoveRight = -1;
}

void SpectrumImager::resetCrossover (int i)
{
    auto* p = (i >= 0 && i < 3) ? freqP[i] : nullptr;
    if (p == nullptr) return;
    const int M = bandCount() - 1;
    float xs[3];
    for (int k = 0; k < M; ++k) xs[k] = freqToX (crossover (k));
    xs[i] = freqToX (p->convertFrom0to1 (p->getDefaultValue()));
    projectGaps (xs, M, i);
    if (onSweep) onSweep();
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (kFreqLo, kFreqHi, xToFreq (xs[i]))));
    p->endChangeGesture();
    for (int k = 0; k < M; ++k)
        if (k != i && std::abs (freqToX (crossover (k)) - xs[k]) > 0.5f)
            setParam (freqP[k], juce::jlimit (kFreqLo, kFreqHi, xToFreq (xs[k])));
}

int SpectrumImager::addBandAt (float hz)
{
    const int N = bandCount();
    if (N >= 4) return -1;
    const int M = N - 1; // existing crossovers
    float xs[3];
    for (int k = 0; k < M; ++k) xs[k] = freqToX (crossover (k));
    const float clickX = freqToX (juce::jlimit (kFreqLo, kFreqHi, hz));

    int ins = 0;
    while (ins < M && xs[ins] < clickX) ++ins;

    float nx[3];
    for (int i = 0; i <= M; ++i) nx[i] = (i < ins) ? xs[i] : (i == ins ? clickX : xs[i - 1]);
    projectGaps (nx, M + 1, ins); // spread neighbours so every gap fits (#25)

    float wd[4];
    for (int k = 0; k < 4; ++k) wd[k] = bandWidth (k);
    float nw[4];
    for (int i = 0; i <= N; ++i) nw[i] = (i <= ins) ? wd[i] : wd[i - 1];

    // Solo: splitting band `ins` keeps ONLY the left half soloed (#6).
    const int oldMask = soloMask();
    int nm = 0;
    for (int i = 0; i < N; ++i)
        if (oldMask & (1 << i))
            nm |= (i < ins) ? (1 << i) : (i == ins ? (1 << ins) : (1 << (i + 1)));
    setSoloMask (nm);

    for (int i = 0; i <= N; ++i) setParam (widthP[i], nw[i]);
    for (int i = 0; i < N;  ++i) setParam (freqP[i],  juce::jlimit (kFreqLo, kFreqHi, xToFreq (nx[i])));
    setBands (N + 1);
    return ins;
}

void SpectrumImager::removeBand (int b)
{
    const int N = bandCount();
    if (N <= 1) return;
    b = juce::jlimit (0, N - 1, b);
    const int dropX = (b == 0) ? 0 : (b - 1); // delete the split on this band's left (#12)

    float fr[3], wd[4];
    for (int k = 0; k < 3; ++k) fr[k] = crossover (k);
    for (int k = 0; k < 4; ++k) wd[k] = bandWidth (k);

    float nf[3], nw[4];
    for (int k = 0, j = 0; k < N;     ++k) if (k != b)     nw[j++] = wd[k];
    for (int k = 0, j = 0; k < N - 1; ++k) if (k != dropX) nf[j++] = fr[k];

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
    const int M = bandCount() - 1;
    float xs[3];
    for (int k = 0; k < M; ++k) xs[k] = freqToX (crossover (k));
    xs[i] = freqToX (juce::jlimit (kFreqLo, kFreqHi, parseFreq (freqEditor->getText())));
    projectGaps (xs, M, i);
    if (auto* p = freqP[i])
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (kFreqLo, kFreqHi, xToFreq (xs[i]))));
        p->endChangeGesture();
    }
    for (int k = 0; k < M; ++k)
        if (k != i && std::abs (freqToX (crossover (k)) - xs[k]) > 0.5f)
            setParam (freqP[k], juce::jlimit (kFreqLo, kFreqHi, xToFreq (xs[k])));
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
// The unchanged 0.6.x analysis body: mono mix, Hann window, transform. Leaves
// the magnitude spectrum in fftData[0 .. fftSize/2] (retained between ticks).
void SpectrumImager::runTransform()
{
    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = 0.5f * (fifoL[(size_t) i] + fifoR[(size_t) i]);
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    // ignoreNegativeFreqs (Wave 3): every consumer reads bins 0..fftSize/2
    // only (the mags loop, the silent-path fill, magForColumn), and a real
    // input's negative-frequency magnitudes are a mirror of those -- so let
    // JUCE compute |X[k]| for the fftSize/2+1 consumed bins and zero the rest
    // instead of reconstructing and abs-ing all fftSize bins. The consumed
    // values are the identical std::abs of the identical complex spectrum:
    // no visual change by construction.
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);
}

// S2 idle gate around the FFT. The maths, sizes, window and read are exactly
// the old pushFFT -- only WHEN they execute changes. Freshness follows the
// Vectorscope's S1 pattern (ScopeBuffer::writeCount + scan only the newly
// arrived frames), with the fixed fftSize window as the content window:
//  - ring frozen (host stopped processing)  -> window identical, no work;
//  - window all-zero and was all-zero       -> more zeros change nothing;
//  - window just became all-zero            -> the FFT of zeros is exactly
//    zero, so that result is written analytically, without the transform.
// Returns true when fftData holds new magnitudes (the mags smoothing in
// timerCallback has fresh input).
bool SpectrumImager::pushFFT()
{
    const auto count = scope.writeCount();
    const auto fresh = count - lastSeenCount;
    if (fresh == 0)
        return false; // frozen ring: the analysis window is bit-identical

    lastSeenCount = count;

    if (! lastWindowSilent)
    {
        // Window has (or may have) content: read it in full, exactly as
        // before, scanning just the freshly arrived tail for the silence
        // tracker (frames older than the window can never re-enter it).
        const int got    = scope.readLatest (fifoL.data(), fifoR.data(), fftSize);
        const int freshN = (int) juce::jmin ((std::uint64_t) got, fresh);
        for (int i = got - freshN; i < got; ++i)
            if (std::abs (fifoL[(size_t) i]) > 0.0f || std::abs (fifoR[(size_t) i]) > 0.0f)
            {
                lastNonZero = count;
                break;
            }

        if (count - lastNonZero >= (std::uint64_t) fftSize)
        {
            std::fill (fftData.begin(), fftData.begin() + fftSize / 2 + 1, 0.0f);
            lastWindowSilent = true;
            return true;
        }

        if (got < fftSize)
            return false; // ring not filled once yet (the old early-return)

        runTransform();
        return true;
    }

    // Window was all-zero: peek only at the freshly arrived frames; further
    // zeros keep the window -- and therefore fftData -- unchanged.
    const int n   = (int) juce::jmin (fresh, (std::uint64_t) fftSize);
    const int got = scope.readLatest (fifoL.data(), fifoR.data(), n);
    for (int i = 0; i < got; ++i)
        if (std::abs (fifoL[(size_t) i]) > 0.0f || std::abs (fifoR[(size_t) i]) > 0.0f)
        {
            lastNonZero      = count;
            lastWindowSilent = false;
            if (scope.readLatest (fifoL.data(), fifoR.data(), fftSize) < fftSize)
                return false;
            runTransform(); // audio is back: analyse it this very tick
            return true;
        }
    return false;
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
    const float fa = xToFreqCached (juce::jmin (xa, xb));
    const float fb = xToFreqCached (juce::jmax (xa, xb));
    const float span = (fb - fa) / binHz;
    if (span < 1.5f)
        return magCubic (0.5f * (fa + fb) / binHz);
    int ka = juce::jlimit (0, kmax, (int) std::floor (fa / binHz));
    int kb = juce::jlimit (0, kmax, (int) std::ceil  (fb / binHz));
    float sum = 0.0f;
    for (int k = ka; k <= kb; ++k) sum += mags[(size_t) k];
    return sum / (float) (kb - ka + 1);
}

// S12: the exact xToFreq(x), served from the half-pixel LUT only when x lands
// EXACTLY on the cached grid at the geometry the LUT was built for -- otherwise
// the live bisection. LUT[i] was produced by xToFreq(lutX0 + i*0.5), and an
// on-grid x equals lutX0 + i*0.5 bit-for-bit (integers/half-integers < 2^23),
// so the returned float is identical to calling xToFreq(x) directly. Any miss
// (off-grid, out of range, geometry changed) falls through to the exact path.
float SpectrumImager::xToFreqCached (float x) const noexcept
{
    // Geometry is keyed on the INTEGER component width -- plot() is a pure
    // function of getLocalBounds(), so an unchanged width means an unchanged
    // grid. The only exact float test left is the on-grid integrality of the
    // reconstructed index, which is the correctness guard itself.
    if (getWidth() == lutW)
    {
        const float fi = (x - lutX0) * 2.0f;
        const int   i  = (int) fi;
        JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wfloat-equal")
        const bool onGrid = (fi == (float) i);
        JUCE_END_IGNORE_WARNINGS_GCC_LIKE
        if (onGrid && i >= 0 && i < (int) xToFreqLUT.size())
            return xToFreqLUT[(size_t) i];
    }
    return xToFreq (x);
}

void SpectrumImager::ensurePaintLUTs()
{
    const auto r = plot();

    // Inverse-axis LUT on the half-pixel grid the spectrum + clip loops query.
    // Keyed on the integer width (plot X and WIDTH are fixed by getLocalBounds).
    if (getWidth() != lutW)
    {
        lutW  = getWidth();
        lutX0 = r.getX() - 0.5f;      // the lowest query is xToFreq(getX() - 0.5)
        const int n = juce::jmax (2, (int) std::lround ((r.getRight() + 0.5f - lutX0) * 2.0f) + 1);
        xToFreqLUT.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
            xToFreqLUT[(size_t) i] = xToFreq (lutX0 + (float) i * 0.5f);
        redBinSR = 0.0;              // geometry moved -> the clip bin LUT is stale too
    }

    // Clip bin-index LUT: which FFT bin each pixel column samples (depends on
    // geometry via xToFreq and on sampleRate via binHz). The per-frame clip loop
    // then just gathers redLevel[bin], with no per-pixel bisection.
    const int W = (int) r.getWidth() + 1;
    JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wfloat-equal")
    const bool srChanged = (redBinSR != sampleRate);
    JUCE_END_IGNORE_WARNINGS_GCC_LIKE
    if ((int) redColBin.size() != W || srChanged)
    {
        redBinSR = sampleRate;
        redColBin.resize ((size_t) W);
        const float binHz = (float) sampleRate / (float) fftSize;
        for (int xi = 0; xi < W; ++xi)
            redColBin[(size_t) xi] = juce::jlimit (0, fftSize / 2,
                (int) std::lround (xToFreq (r.getX() + (float) xi) / binHz));
    }
}

void SpectrumImager::tick (double dt)
{
    // S2 idle gate: suspend all analysis and animation while not showing
    // (Simple mode hides the imager; hosts can hide the whole editor). On
    // re-show the stale spectrum drops to the floor: live audio rebuilds it
    // on this same tick (the mags attack is instantaneous) and silence shows
    // the floor -- exactly what the always-running decay converged to before.
    if (! isShowing())
    {
        wasShowing = false;
        return;
    }
    if (! wasShowing)
    {
        wasShowing = true;
        std::fill (mags.begin(), mags.end(), kMinDb);
        std::fill (redLevel.begin(), redLevel.end(), 0.0f);
        magsSettled = false;
        redSettled  = false;
        frameDirty  = true;
    }

    const double sr = apvts.processor.getSampleRate() > 0.0 ? apvts.processor.getSampleRate() : 48000.0;
    if (std::abs (sr - sampleRate) > 0.0)
        frameDirty = true; // the frequency mapping shifts with the sample rate
    sampleRate = sr;

    // Analysis runs only while something can still change: the FFT when the
    // window changed (pushFFT), the two smoothing passes until they settle.
    // Their maths and per-tick rates are the unchanged 0.6.x code -- they run
    // every tick while anything still moves, so attack/decay timing is
    // identical; they just stop being evaluated once provably static.
    bool dataMoved = false;
    if (pushFFT())
    {
        magsSettled = false;
        // dB conversion once per NEW transform (Wave 4), not per decay tick:
        // pushFFT() returning true is the only way fftData changes, so magsDb
        // always holds exactly gainToDecibels(fftData[k] * norm) -- the decay
        // loop below reads the identical values it used to recompute. The
        // multi-second release tail after audio stops (frozen ring, mags still
        // draining) previously re-ran all ~4k log10 conversions every tick.
        const float norm = 2.0f / (float) fftSize;
        for (int k = 0; k <= fftSize / 2; ++k)
            magsDb[(size_t) k] = juce::Decibels::gainToDecibels (fftData[(size_t) k] * norm, kMinDb);
    }

    if (! magsSettled)
    {
        // Release decay per tick, dt-corrected (0.25 at the old fixed 60 Hz):
        // same decay time on any display, matching 60 Hz to within the display
        // quantum. Attack stays instantaneous (db > m). Computed once per tick,
        // shared across all bins (never a per-bin pow).
        const float rRel = frameCoeff (0.25f, dt);
        bool any = false;
        for (int k = 0; k <= fftSize / 2; ++k)
        {
            const float db = magsDb[(size_t) k];
            float& m = mags[(size_t) k];
            const float next = db > m ? db : m + (db - m) * rRel;
            if (std::abs (next - m) > 0.0f) { m = next; any = true; }
        }
        magsSettled = ! any;
        dataMoved |= any;
    }

    // Clip glow level per bin. The Hann window costs ~6 dB of coherent gain, so a 0 dBFS
    // tone only reads ~-6 dB on the analyser -- compensate for that, so reaching full scale
    // actually lights the red (0.6.16 #1). Rises FAST and falls back SLOWLY with an
    // exponential (non-linear) curve so a region lingers as it fades, even while other bands
    // light up (0.6.16 #2). Always animated, independent of the UI-animation switch.
    if (dataMoved || ! redSettled)
    {
        // Rise-fast / fall-slow rates per tick, dt-corrected (0.6 / 0.035 at the
        // old fixed 60 Hz): same rise/fall times on any display, matching 60 Hz
        // to within the display quantum. Both computed once, selected per bin.
        const float rRedRise = frameCoeff (0.6f,   dt);
        const float rRedFall = frameCoeff (0.035f, dt);
        bool any = false;
        for (size_t k = 0; k < redLevel.size(); ++k)
        {
            const float eff = mags[k] + 6.0f;                                   // window-gain compensated dBFS
            const float t = juce::jlimit (0.0f, 1.0f, (eff + 4.0f) / 4.0f);     // onset -4 dBFS, full at 0 dBFS
            float& rl = redLevel[k];
            float next = rl + (t - rl) * (t > rl ? rRedRise : rRedFall);
            // paint() cannot draw levels below 0.012 (the maxRed gate and the
            // per-quad cull), so snapping the tail to zero from 1e-3 -- 12x
            // below the smallest drawable value -- is pixel-identical and ends
            // an otherwise ~half-minute sub-visible float decay.
            if (t <= 0.0f && next < 1.0e-3f) next = 0.0f;
            if (std::abs (next - rl) > 0.0f) { rl = next; any = true; }
        }
        redSettled = ! any;
        dataMoved |= any;
    }

    // Press-and-hold a headphone -> momentary audition of that band (engine override).
    if (soloPressBand >= 0 && ! soloHoldActive && ! soloMovedBand
        && juce::Time::getMillisecondCounter() - soloPressMs > 200u)
    {
        soloHoldActive = true;
        if (onSoloPreview) onSoloPreview (1 << soloPressBand);
    }

    const bool animOn = animOnP == nullptr || animOnP->load() > 0.5f;
    // Real frame dt (was hardcoded 1/60): these UI eases are already in the
    // time-constant form 1 - exp(-dt/tau), so feeding the true dt keeps every
    // tau identical on any display and reproduces the 60 Hz curves exactly.
    const float dtf  = (float) dt;
    const float rIn  = animOn ? 1.0f - std::exp (-dtf / 0.075f) : 1.0f;
    const float rOut = animOn ? 1.0f - std::exp (-dtf / 0.150f) : 1.0f;
    bool uiMoved = false;
    auto ease = [&] (float& v, float t)
    {
        float next = v + (t - v) * (t > v ? rIn : rOut);
        // Converge onto the target inside the display quantum (< 1/255) --
        // the same snap the editor's micro-anims use (stepVal, 0.004) -- so
        // an eased value actually arrives instead of decaying sub-visibly
        // forever; report any movement to the S2 repaint gate.
        if (std::abs (next - t) < 0.004f) next = t;
        if (std::abs (next - v) > 0.0f) { v = next; uiMoved = true; }
    };

    // The band-pass preview curve is a press-and-HOLD affordance: light it only once the
    // grab has been held past the threshold or has become a drag, never on a bare click /
    // double-click / repeated clicks (0.8.1). Hover/handle glow stays click-responsive.
    if (dragHandle >= 0 && ! handleHoldActive
        && juce::Time::getMillisecondCounter() - handlePressMs > 200u)
        handleHoldActive = true;

    const int em = effectiveSoloMask();
    for (int i = 0; i < 3; ++i) ease (handleA[i], (i == dragHandle || i == hoverHandle) ? 1.0f : 0.0f);
    for (int i = 0; i < 3; ++i) ease (pressA[i],  (i == dragHandle && handleHoldActive) ? 1.0f : 0.0f);
    for (int i = 0; i < 4; ++i) ease (widthA[i],  (i == dragBand   || i == hoverWidth)  ? 1.0f : 0.0f);
    for (int i = 0; i < 4; ++i) ease (pressW[i],  (i == dragBand) ? 1.0f : 0.0f);
    // Over the x itself = brightest; merely over the band = dimmer; eased either way (#2).
    for (int i = 0; i < 4; ++i) ease (delA[i], (i == hoverDeleteExact) ? 1.0f : (i == hoverDelete ? 0.5f : 0.0f));
    for (int i = 0; i < 4; ++i) ease (soloA[i],   (em & (1 << i)) ? 1.0f : (i == hoverSolo ? 0.55f : 0.0f));
    for (int i = 0; i < 4; ++i) ease (labelFlipA[i], (widthToY (bandWidth (i)) < laneTop() + 20.0f) ? 1.0f : 0.0f);
    // No add affordance while the cursor is on a delete x or holding one -- a click there
    // deletes, it does not add (0.6.16 #1/#2).
    ease (addA, (hoverAdd >= 0 && hoverDeleteExact < 0 && pressDeleteBand < 0) ? 1.0f : 0.0f);
    ease (enaA, enabled() ? 1.0f : 0.0f);
    ease (panelHoverA, isMouseOverOrDragging (true) ? 1.0f : 0.0f);
    if (soloHoldActive) soloCurveBand = soloPressBand;
    ease (soloCurveA, soloHoldActive ? 1.0f : 0.0f);

    // Display-eased split / width positions: a sweep (reset / preset / A-B / undo) glides
    // them to the new spots; a drag / scroll / automation snaps 1:1. Once a sweep is armed
    // the glide keeps running until it CONVERGES, so it never stops by snapping the last few
    // pixels when the editor's sweep window closes (0.6.15 #5).
    float prevF[3], prevW[4];
    for (int i = 0; i < 3; ++i) prevF[i] = drawnF[i];
    for (int b = 0; b < 4; ++b) prevW[b] = drawnW[b];
    const int prevBands = lastBandCount;

    const int N = bandCount();
    if (N != lastBandCount)
    {
        for (int i = 0; i < 3; ++i) drawnF[i] = crossover (i);
        for (int b = 0; b < 4; ++b) drawnW[b] = bandWidth (b);
        lastBandCount = N;
        dispEasing = false;
    }
    const bool busy = (dragHandle >= 0 || dragBand >= 0 || soloMovedBand);
    if (animOn && ! busy && isSweeping && isSweeping()) dispEasing = true; // (re)arm on a sweep event
    if (busy || ! animOn) dispEasing = false;

    const float rPos = 1.0f - std::exp (-dtf / 0.105f); // gentle, slow-stopping tail (#5)
    if (dispEasing)
    {
        bool anyDiff = false;
        for (int i = 0; i < 3; ++i)
        {
            const float real = crossover (i);
            if (drawnF[i] > 0.0f) drawnF[i] *= std::pow (real / drawnF[i], rPos);
            if (std::abs (freqToX (drawnF[i]) - freqToX (real)) < 0.3f) drawnF[i] = real; // sub-pixel snap
            else anyDiff = true;
        }
        for (int b = 0; b < 4; ++b)
        {
            const float real = bandWidth (b);
            drawnW[b] += (real - drawnW[b]) * rPos;
            if (std::abs (widthToY (drawnW[b]) - widthToY (real)) < 0.3f) drawnW[b] = real;
            else anyDiff = true;
        }
        if (! anyDiff) dispEasing = false; // fully arrived
    }
    else
    {
        for (int i = 0; i < 3; ++i) drawnF[i] = crossover (i);
        for (int b = 0; b < 4; ++b) drawnW[b] = bandWidth (b);
    }

    // Drawn split/width positions and the band count feed paint() directly:
    // any change (glide step, snap to a moved parameter, band add/remove)
    // must render, however it was produced above.
    for (int i = 0; i < 3; ++i) uiMoved |= std::abs (drawnF[i] - prevF[i]) > 0.0f;
    for (int b = 0; b < 4; ++b) uiMoved |= std::abs (drawnW[b] - prevW[b]) > 0.0f;
    uiMoved |= lastBandCount != prevBands;

    // S2 repaint gate: the frame is a pure function of the state advanced
    // above (mags/redLevel, eased alphas, drawn positions) plus mouse-driven
    // fields whose handlers already repaint explicitly -- so when nothing
    // moved this tick, the previous frame is bit-identical and painting it
    // again is pure waste. Interaction, decays and animations repaint at the
    // full 60 Hz exactly as before; the view idles only once truly settled.
    if (dataMoved || uiMoved || frameDirty)
    {
        frameDirty = false;
        repaint();
    }
}

// A smooth, centred glow: several overlapping rounded strokes whose alpha falls off in a
// Gaussian-ish curve, so the halo reads as one continuous bloom (not a few hard bands) and
// sits exactly on the path with no sideways offset (0.6.15 #2/#3).
void SpectrumImager::softGlow (juce::Graphics& g, const juce::Path& p, juce::Colour c,
                               float intensity, float maxWidth) const
{
    if (intensity <= 0.01f) return;
    const auto js = juce::PathStrokeType::curved;
    const auto cs = juce::PathStrokeType::rounded;
    constexpr int N = 9;
    for (int s = N - 1; s >= 0; --s) // widest + faintest first, narrowest + brightest last
    {
        const float t = (float) s / (float) (N - 1);          // 1 .. 0
        const float w = 1.4f + t * (maxWidth - 1.4f);
        const float a = intensity * std::exp (-1.9f * t * t) * 0.40f; // broader falloff -> a real, far glow
        g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, a)));
        g.strokePath (p, juce::PathStrokeType (w, js, cs));
    }
}

// ----------------------------------------------------------------------------
//  Paint
// ----------------------------------------------------------------------------

// Render the bottom layer (H17): the glass panel, the band tints and the
// frequency-grid verticals -- everything painted BELOW the spectrum. The
// drawing code is IDENTICAL to what paint() ran directly before the cache;
// the image is rendered at the destination's PHYSICAL resolution so the blit
// is a 1:1 device-pixel composite of the exact rasterization the direct draw
// produced. ARGB, not RGB: the imager sits on the editor's semi-transparent
// Multiband panel (not flat bg), so transparency must be preserved (the
// rounded corners keep showing the parent) -- the N2 opacity pattern is
// deliberately NOT applied here.
//
// Every key input converges EXACTLY (the 0.004 ease snap for panelHoverA /
// widthA, the sub-pixel drawnF/drawnW snap), so the key settles after any
// interaction and steady-state paints never rebuild. While something is
// easing, the rebuild costs what the direct drawing always cost; the image
// buffer is reused across same-size rebuilds (no per-frame allocation).
void SpectrumImager::ensureBottomLayer (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    const int   N     = bandCount();
    const int   amask = effectiveSoloMask() & ((1 << N) - 1);

    auto same = [] (float a, float b) noexcept { return ! (std::abs (a - b) > 0.0f); }; // exact, S4 idiom
    bool ok = ! bottomLayer.isNull()
           && blW == getWidth() && blH == getHeight()
           && same (blScale, scale) && same (blHover, panelHoverA)
           && blBands == N && blMask == amask;
    for (int i = 0; i < 3 && ok; ++i) ok = same (blF[i],  drawnF[i]);
    for (int b = 0; b < 4 && ok; ++b) ok = same (blDW[b], drawnW[b]) && same (blWA[b], widthA[b]);
    if (ok)
        return;

    const bool sameSize = blW == getWidth() && blH == getHeight() && same (blScale, scale)
                       && ! bottomLayer.isNull();
    blW = getWidth(); blH = getHeight(); blScale = scale;
    blHover = panelHoverA; blBands = N; blMask = amask;
    for (int i = 0; i < 3; ++i) blF[i]  = drawnF[i];
    for (int b = 0; b < 4; ++b) { blDW[b] = drawnW[b]; blWA[b] = widthA[b]; }

    if (sameSize)
        bottomLayer.clear (bottomLayer.getBounds()); // reuse the buffer (unshared -> in place)
    else
        bottomLayer = juce::Image (juce::Image::ARGB,
                                   juce::jmax (1, juce::roundToInt ((float) blW * scale)),
                                   juce::jmax (1, juce::roundToInt ((float) blH * scale)),
                                   true);
    juce::Graphics ig (bottomLayer);
    ig.addTransform (juce::AffineTransform::scale (scale));

    // Idle the panel sits darker so the ruler lines read through; hover lifts it (#23).
    glass::fillPanel (ig, getLocalBounds().toFloat(), 6.0f,
                      colours::bgPanel.darker (0.58f - 0.14f * panelHoverA), 1.0f);

    juce::Graphics::ScopedSaveState save (ig);
    juce::Path clip; clip.addRoundedRectangle (r, 5.0f);
    ig.reduceClipRegion (clip);

    const bool anySolo = amask != 0;
    const juce::Colour bandLo (0xff5aa6ff), bandHi (0xff35d0c0);
    auto bandCol = [&] (int b) { return bandLo.interpolatedWith (bandHi, juce::jlimit (0.0f, 1.0f, dispWidth (b) * 0.5f)); };

    // --- band tints -----------------------------------------------------
    for (int b = 0; b < N; ++b)
    {
        const float x0 = dispLeftX (b), x1 = dispRightX (b);
        float a = 0.04f + 0.05f * juce::jlimit (0.0f, 2.0f, dispWidth (b)) * 0.5f + 0.06f * widthA[b];
        if (anySolo) a = (amask & (1 << b)) ? a + 0.10f : a * 0.4f;
        ig.setColour (bandCol (b).withAlpha (a));
        ig.fillRect (juce::Rectangle<float> (x0, r.getY(), juce::jmax (0.0f, x1 - x0), r.getHeight()));
    }

    // --- frequency grid (brighter when the panel is dark, #21/#23) ------
    for (auto& t : kTicks)
    {
        const float x = freqToX (t.f);
        ig.setColour (colours::outline.withAlpha (t.major ? 0.42f : 0.20f));
        ig.drawVerticalLine (juce::roundToInt (x), r.getY(), rulerY() + 2.0f);
    }
}

void SpectrumImager::paint (juce::Graphics& g)
{
    // Eased split / width positions only lag during a sweep window (reset / preset / A-B /
    // undo); a band-count change or any direct interaction snaps them to the live values
    // BEFORE the first paint, so a split never flashes from a stale spot and every follower
    // (pin, width dot, headphones) stays in step while dragging (0.6.11 #6/#15, 0.6.12 #8).
    {
        const bool animOn = animOnP == nullptr || animOnP->load() > 0.5f;
        const bool busy = (dragHandle >= 0 || dragBand >= 0 || soloMovedBand);
        const bool inSweepWindow = animOn && ! busy && isSweeping && isSweeping();
        const int Nb = bandCount();
        // Snap to live values unless a sweep is open OR still gliding to its target (so the
        // glide is never cut short by a mid-frame snap); a band-count change always snaps.
        if (busy || Nb != lastBandCount || (! inSweepWindow && ! dispEasing))
        {
            for (int i = 0; i < 3; ++i) drawnF[i] = crossover (i);
            for (int b = 0; b < 4; ++b) drawnW[b] = bandWidth (b);
            if (Nb != lastBandCount) dispEasing = false;
        }
        lastBandCount = Nb;
    }

    auto r = plot();

    // Bottom layer (H17): panel + band tints + frequency grid, blitted from the
    // cache and re-rasterized only when one of its (exactly-converging) inputs
    // changed -- see ensureBottomLayer.
    ensureBottomLayer (g, r);
    {
        juce::Graphics::ScopedSaveState blitState (g);
        g.addTransform (juce::AffineTransform::scale (1.0f / blScale));
        g.drawImageAt (bottomLayer, 0, 0);
    }

    juce::Graphics::ScopedSaveState save (g);
    juce::Path clip; clip.addRoundedRectangle (r, 5.0f);
    g.reduceClipRegion (clip);

    const int N = bandCount();
    const int amask = effectiveSoloMask() & ((1 << N) - 1);
    const juce::Colour bandLo (0xff5aa6ff), bandHi (0xff35d0c0);
    const juce::Colour xoverCol = colours::accent;
    const juce::Colour clipCol (0xffff5b4b);

    auto bandCol = [&] (int b) { return bandLo.interpolatedWith (bandHi, juce::jlimit (0.0f, 1.0f, dispWidth (b) * 0.5f)); };

    // --- spectrum (cubic-smoothed) + localized clip-red overlay (#14) ---
    {
        ensurePaintLUTs(); // S12: refresh the paint LUTs on geometry / SR change
        auto dbToY = [&] (float db)
        {
            const float t = (juce::jlimit (kMinDb, kMaxDb, db) - kMinDb) / (kMaxDb - kMinDb);
            return (r.getBottom() + 8.0f) - t * (r.getHeight() + 8.0f);
        };
        // Member paths, cleared not reconstructed (Wave 4): clear() retains the
        // storage, so steady-state active paints add zero path allocations. The
        // point sequence is identical to the former locals (addPath appends the
        // same elements the copy constructor copied).
        juce::Path& spec = specPath;
        spec.clear();
        bool started = false;
        for (float x = r.getX(); x <= r.getRight(); x += 1.0f)
        {
            const float y = dbToY (magForColumn (x - 0.5f, x + 0.5f));
            if (! started) { spec.startNewSubPath (x, y); started = true; }
            else            spec.lineTo (x, y);
        }
        juce::Path& fillPath = specFillPath;
        fillPath.clear();
        fillPath.addPath (spec);
        fillPath.lineTo (r.getRight(), r.getBottom() + 2.0f);
        fillPath.lineTo (r.getX(), r.getBottom() + 2.0f);
        fillPath.closeSubPath();
        g.setGradientFill (juce::ColourGradient (xoverCol.withAlpha (0.20f), 0.0f, r.getY(),
                                                 xoverCol.withAlpha (0.012f), 0.0f, r.getBottom(), false));
        g.fillPath (fillPath);
        g.setColour (xoverCol.withAlpha (0.55f));
        g.strokePath (spec, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Clip: a VERTICAL red gradient band rises at each over-0 dBFS frequency -- brightest
        // on the line and washing down the column (not a circle), confined to the spectrum
        // (line + below). The per-column level is horizontally feathered so it cross-fades
        // smoothly and widely into the surrounding green with no seam or banding (0.6.16 #B3).
        {
            const int W = (int) r.getWidth() + 1;
            if ((int) redColX.size() != W) redColX.assign ((size_t) W, 0.0f);
            float maxRed = 0.0f;
            for (int xi = 0; xi < W; ++xi)
            {
                // S12: redColBin[xi] is the same jlimit(lround(xToFreq/binHz)) index
                // computed per-pixel before, now precomputed in ensurePaintLUTs.
                redColX[(size_t) xi] = redLevel[(size_t) redColBin[(size_t) xi]];
                maxRed = juce::jmax (maxRed, redColX[(size_t) xi]);
            }
            if (maxRed > 0.012f)
            {
                // Wide triangular blur -> a soft, far-reaching horizontal feather.
                // S12: reuse a persistent scratch buffer instead of allocating one
                // per paint; every element is written below before it is read.
                if ((int) clipBlurScratch.size() != W) clipBlurScratch.resize ((size_t) W);
                auto& sm = clipBlurScratch;
                const int RB = 22;
                float norm = 0.0f; for (int d = -RB; d <= RB; ++d) norm += (float) (RB + 1 - std::abs (d));
                for (int xi = 0; xi < W; ++xi)
                {
                    float acc = 0.0f;
                    for (int d = -RB; d <= RB; ++d)
                    {
                        const int j = juce::jlimit (0, W - 1, xi + d);
                        acc += redColX[(size_t) j] * (float) (RB + 1 - std::abs (d));
                    }
                    sm[(size_t) xi] = acc / norm;
                }
                // Quads whose TOP edge slants along the spectrum curve (not a flat-topped
                // rectangle), so the red follows the green edge smoothly with no staircase (#2).
                const float bot = r.getBottom();
                auto topY = [&] (float x) { return dbToY (magForColumn (x - 0.5f, x + 0.5f)) - 2.0f; };
                for (int xi = 0; xi < W - 2; xi += 2)
                {
                    const float rl = sm[(size_t) xi];
                    if (rl < 0.012f) continue;
                    const float x0 = r.getX() + (float) xi, x1 = x0 + 2.0f;
                    const float y0 = topY (x0), y1 = topY (x1);
                    juce::Path& q = clipQuadPath; // reused storage (Wave 4)
                    q.clear();
                    q.startNewSubPath (x0, y0);
                    q.lineTo (x1, y1);
                    q.lineTo (x1, bot);
                    q.lineTo (x0, bot);
                    q.closeSubPath();
                    juce::ColourGradient vg (clipCol.withAlpha (0.78f * rl), x0, juce::jmin (y0, y1),
                                             clipCol.withAlpha (0.14f * rl), x0, bot, false);
                    g.setGradientFill (vg);
                    g.fillPath (q);
                }
            }
        }
    }

    // --- width lane guides: faint floor (0%) + ceiling (max), unity dashed ---
    {
        g.setColour (colours::outline.withAlpha (0.22f));
        g.drawLine (r.getX() + 2.0f, laneBot(), r.getRight() - 2.0f, laneBot(), 1.0f);
        g.drawLine (r.getX() + 2.0f, laneTop(), r.getRight() - 2.0f, laneTop(), 1.0f); // top reference (#24)
        const float y = widthToY (1.0f);
        float d[2] = { 3.0f, 3.0f };
        g.setColour (colours::outline.withAlpha (0.40f));
        g.drawDashedLine ({ { r.getX(), y }, { r.getRight(), y } }, d, 2, 1.0f);
    }

    // --- per-band width lines -------------------------------------------
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    for (int b = 0; b < N; ++b)
    {
        const float w = dispWidth (b);
        const float y = widthToY (w);
        const float x0 = dispLeftX (b), x1 = dispRightX (b);
        const float act = widthA[b];
        const float pr  = pressW[b];
        const auto col = bandCol (b);

        // The soft drop-shadow glow (the look the user preferred) -- reverted from the
        // multi-stroke variant, which changed its appearance too much (0.6.19 #1).
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
            const float flip = labelFlipA[b];
            const float ly = (y - 17.0f) + flip * 22.0f;
            g.setColour (colours::text.withAlpha (juce::jmax (act, pr)));
            g.drawText (juce::String (juce::roundToInt (w * 100.0f)) + "%",
                        juce::Rectangle<float> (cx - 26.0f, ly, 52.0f, 13.0f), juce::Justification::centred);
        }
    }

    // --- frequency ruler numbers + bottom-right "Hz" --------------------
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    for (auto& t : kTicks)
    {
        const float x = freqToX (t.f);
        g.setColour (colours::textDim.withAlpha (t.major ? 0.85f : 0.50f));
        g.drawText (t.label, juce::Rectangle<float> (x - 20.0f, rulerY() - 6.0f, 40.0f, 13.0f), juce::Justification::centred);
    }
    g.setColour (colours::textDim.withAlpha (0.55f));
    g.drawText ("Hz", juce::Rectangle<float> (r.getRight() - 26.0f, rulerY() - 6.0f, 24.0f, 13.0f), juce::Justification::centredRight);

    // --- band-pass response curve(s) ------------------------------------
    // A flat plateau across the band, steep slopes (~LR4) straight down to the very
    // bottom; the outermost edge of the lowest / highest band runs flat to the frame
    // (0.6.10 #11/#17). Shown while a split is dragged (its two neighbours) or while a
    // band is soloed/auditioned (that one band, #15).
    auto bandCurve = [&] (int b, juce::Colour col, float alpha)
    {
        if (b < 0 || b >= N) return;
        const float Lf = (b > 0)     ? dispCrossover (b - 1) : -1.0f;
        const float Rf = (b < N - 1) ? dispCrossover (b)     : -1.0f;
        const float slope = 6.0f;                              // ~36 dB/oct (#13)
        const float yPass  = r.getY() + r.getHeight() * 0.66f; // plateau in the lower third (#13)
        const float yFloor = r.getBottom();
        const float range  = 22.0f;
        auto yAt = [&] (float x)
        {
            const float f = xToFreq (x);
            float amp = 1.0f;
            if (Lf > 0.0f) amp *= 1.0f / (1.0f + std::pow (2.0f, -slope * std::log2 (f / Lf)));
            if (Rf > 0.0f) amp *= 1.0f / (1.0f + std::pow (2.0f,  slope * std::log2 (f / Rf)));
            const float db = 20.0f * std::log10 (juce::jmax (1.0e-4f, amp));
            return yPass + juce::jlimit (0.0f, 1.0f, -db / range) * (yFloor - yPass);
        };
        // Find the contiguous above-floor hump (1 px steps for low jitter), then draw the
        // slopes all the way DOWN to the floor with an explicit floor point at each foot, so
        // the join to the bottom is rock-solid instead of shimmering on/off as the band moves
        // (0.6.16). A slope that ends at the view edge (the lowest / highest band) stays flat.
        float fx = -1.0f, lx = -1.0f;
        for (float x = r.getX(); x <= r.getRight(); x += 1.0f)
            if (yAt (x) < yFloor - 0.5f) { if (fx < 0.0f) fx = x; lx = x; }
        if (fx < 0.0f) return;
        const bool leftEdge  = (fx <= r.getX() + 1.0f);
        const bool rightEdge = (lx >= r.getRight() - 1.0f);
        juce::Path p;
        if (leftEdge) p.startNewSubPath (fx, yAt (fx));
        else        { p.startNewSubPath (fx, yFloor); p.lineTo (fx, yAt (fx)); }
        for (float x = fx + 1.0f; x <= lx; x += 1.0f) p.lineTo (x, yAt (x));
        if (! rightEdge) p.lineTo (lx, yFloor);

        juce::Path f (p);
        f.lineTo (lx, yFloor + 2.0f);
        f.lineTo (fx, yFloor + 2.0f);
        f.closeSubPath();
        g.setGradientFill (juce::ColourGradient (col.withAlpha (0.14f * alpha), 0.0f, yPass,
                                                 col.withAlpha (0.0f), 0.0f, yFloor, false));
        g.fillPath (f);
        // Plain two-stroke line (no glow) -- the 0.6.14 look the user preferred (0.6.17 #1).
        const auto js = juce::PathStrokeType::curved;
        const auto cs = juce::PathStrokeType::rounded;
        g.setColour (col.withAlpha (0.18f * alpha)); g.strokePath (p, juce::PathStrokeType (2.6f, js, cs));
        g.setColour (col.withAlpha (0.90f * alpha)); g.strokePath (p, juce::PathStrokeType (1.3f, js, cs));
    };
    for (int i = 0; i < N - 1; ++i)
        if (pressA[i] > 0.01f)
        {
            bandCurve (i,     bandLo, pressA[i]);
            bandCurve (i + 1, bandHi, pressA[i]);
        }
    if (soloCurveA > 0.01f && soloCurveBand >= 0)
        bandCurve (soloCurveBand, bandCol (soloCurveBand).brighter (0.2f), soloCurveA);

    // --- per-band solo headphones + delete x ----------------------------
    // The headband + earcups composite through one partial-opacity layer so they never
    // double up into a bright seam where they meet (0.6.11 #2) -- see paintHeadphone for
    // the full-opacity direct-draw fast path and the glyph-clip that bounds that layer's
    // offscreen (GPU Wave 6).
    auto paintHeadphone = [&] (juce::Rectangle<float> bx, juce::Colour solid, float alpha)
    {
        // The headband arc + two earcups go through a transparency layer so their overlap
        // can't double-blend into a bright seam at partial opacity (0.6.11 #2). GPU Wave 6
        // (0.8.12): two behaviour-neutral trims to this per-band path, which is NOT
        // interaction-gated -- a band wider than 30 px always shows a headphone (loop below),
        // so it runs every frame the spectrum repaints while Advanced is open and audio plays.
        //   * At FULL opacity the seam cannot occur (opaque same-colour draws overwrite rather
        //     than accumulate), so skip the layer and draw direct -- pixel-identical. Only the
        //     soloed/on band hits alpha == 1.0; every other band eases in [0.4, 0.9], never here.
        //   * Otherwise clip to the glyph's own box BEFORE beginTransparencyLayer: JUCE sizes the
        //     offscreen the layer allocates to the CURRENT clip bounds, which here is the whole
        //     plot rounded-rect (reduceClipRegion at the top of paint) -- so without this each
        //     call allocates a plot-sized offscreen (an FBO on the macOS/Windows GL compositor)
        //     and composites it back. The +4 px margin covers the earcups (which reach ~1 px past
        //     bx) and their AA, so every drawn pixel is inside the clip and the composite is
        //     byte-identical; only the offscreen shrinks (~plot -> ~26x23 px).
        const bool useLayer = alpha < 0.999f;
        juce::Graphics::ScopedSaveState clipToIcon (g);
        if (useLayer)
        {
            g.reduceClipRegion (bx.expanded (4.0f).toNearestInt());
            g.beginTransparencyLayer (alpha);
        }
        auto cup = bx.reduced (1.0f, 0.5f);
        const float cx = cup.getCentreX();
        const float rx = cup.getWidth() * 0.5f;
        const float ry = cup.getHeight() * 0.56f;
        const float cy = cup.getY() + ry + 0.5f;
        juce::Path headband;
        headband.addCentredArc (cx, cy, rx, ry, 0.0f, -1.5f, 1.5f, true);
        g.setColour (solid);
        g.strokePath (headband, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float ex = rx * std::sin (1.5f);
        const float ey = cy - ry * std::cos (1.5f);
        const float cupW = 4.2f, cupH = 6.4f;              // over-ear cups (#2)
        g.fillRoundedRectangle (cx - ex - cupW * 0.5f, ey - 0.5f, cupW, cupH, 1.8f);
        g.fillRoundedRectangle (cx + ex - cupW * 0.5f, ey - 0.5f, cupW, cupH, 1.8f);
        if (useLayer)
            g.endTransparencyLayer();
    };
    for (int b = 0; b < N; ++b)
    {
        // Paint from the eased (disp) band edges so the headphone and x travel in lockstep
        // with the split lines on an A-B / preset / reset sweep, and stay glued while
        // dragging (disp == live during a drag) (0.6.14 #5).
        const float dl = dispLeftX (b), dr = dispRightX (b);
        const float dcx = 0.5f * (dl + dr);
        if ((dr - dl) > 30.0f)
        {
            const bool on = (amask & (1 << b)) != 0;
            const float sa = soloA[b];
            juce::Rectangle<float> hbx { dcx - 9.0f, plot().getY() + 3.0f, 18.0f, 15.0f };
            paintHeadphone (hbx, on ? xoverCol : colours::textDim, on ? 1.0f : 0.4f + 0.5f * sa);
        }
        if (delA[b] > 0.02f)
        {
            juce::Rectangle<float> db { dl + 5.0f, rulerY() - 22.0f, 14.0f, 14.0f };
            const float pad = 2.5f, a = delA[b];
            // Alpha tracks the ease all the way to zero so it fades out fully instead of
            // dropping to a floor and then vanishing (#9).
            g.setColour (colours::textDim.brighter (0.2f * a).withAlpha (0.95f * a));
            g.drawLine (db.getX() + pad, db.getY() + pad, db.getRight() - pad, db.getBottom() - pad, 1.6f);
            g.drawLine (db.getRight() - pad, db.getY() + pad, db.getX() + pad, db.getBottom() - pad, 1.6f);
        }
    }

    // --- crossover splits (line + 4-way glow + marker + number) ---------
    for (int i = 0; i < N - 1; ++i)
    {
        const float x = freqToX (dispCrossover (i));
        const float removeFade = (i == dragHandle && dragRemovePending) ? 0.2f : 1.0f; // drag-out preview (#18)
        const float act = handleA[i] * removeFade, press = pressA[i] * removeFade;
        const bool  editing = (editingHandle == i);
        const float lineBot = r.getBottom() - 2.0f;
        const float lineW = 1.2f + 0.9f * act;
        const float lw = lineW * 0.5f;        // half line width == pin flat-bottom half width
        const float tip = r.getY() + 19.0f;   // junction: pin flat bottom meets the line top

        // Smooth, perfectly centred glow along the line (no sideways offset) -- the pin, drawn
        // after, occludes the top so it can't bleed over it (0.6.15 #2).
        const float glowA = juce::jlimit (0.0f, 1.0f, (0.55f * handleA[i] + 0.3f * pressA[i]) * removeFade); // press a touch lighter (0.6.18 #2)
        if (glowA > 0.01f)
        {
            juce::Path lp; lp.startNewSubPath (x, tip); lp.lineTo (x, lineBot);
            softGlow (g, lp, xoverCol, glowA, 15.0f + 5.0f * press);
        }

        // The line starts exactly at the pin's flat bottom -- no stub above, no overlap (0.6.15 #4).
        const auto lineCol = colours::text.withAlpha (0.45f * removeFade)
                                 .interpolatedWith (xoverCol, juce::jlimit (0.0f, 1.0f, act));
        g.setColour (lineCol);
        g.drawLine (x, tip, x, lineBot, lineW);

        {
            const float hw = 5.0f + 1.1f * act + 0.8f * press;
            const float top = r.getY() + 2.0f;
            const float bodyBot = top + 7.0f + 1.0f * press;
            const float rad = 2.5f;
            // Pin tapers to a flat bottom exactly the line's width, so the line butts onto it.
            juce::Path m;
            m.startNewSubPath (x - hw, top + rad);
            m.quadraticTo (x - hw, top, x - hw + rad, top);
            m.lineTo (x + hw - rad, top);
            m.quadraticTo (x + hw, top, x + hw, top + rad);
            m.lineTo (x + hw, bodyBot);
            m.lineTo (x + lw, tip);
            m.lineTo (x - lw, tip);
            m.lineTo (x - hw, bodyBot);
            m.closeSubPath();

            if (act > 0.02f || press > 0.02f)
                juce::DropShadow (xoverCol.withAlpha (0.45f * act + 0.5f * press), (int) (7.0f + 3.0f * press), {})
                    .drawForPath (g, m);
            g.setGradientFill (juce::ColourGradient (xoverCol.brighter (0.35f + 0.3f * press).withAlpha (removeFade), 0.0f, top,
                                                     xoverCol.withMultipliedBrightness (0.65f).withAlpha (removeFade), 0.0f, bodyBot, false));
            g.fillPath (m);
            // Outline every edge EXCEPT the bottom flat one, so no white stroke lands on the seam.
            juce::Path o;
            o.startNewSubPath (x - lw, tip);
            o.lineTo (x - hw, bodyBot);
            o.lineTo (x - hw, top + rad);
            o.quadraticTo (x - hw, top, x - hw + rad, top);
            o.lineTo (x + hw - rad, top);
            o.quadraticTo (x + hw, top, x + hw, top + rad);
            o.lineTo (x + hw, bodyBot);
            o.lineTo (x + lw, tip);
            g.setColour (juce::Colours::white.withAlpha ((0.25f + 0.45f * juce::jmax (act, press)) * removeFade));
            g.strokePath (o, juce::PathStrokeType (1.0f));
        }

        if (act > 0.05f && ! editing && ! (i == dragHandle && dragRemovePending))
        {
            // Chip position AND value follow the eased split, so the freq readout travels
            // with the line on a reset / preset / A-B sweep instead of jumping (0.6.16 #D).
            // A slightly shorter background box (0.6.17 #3).
            juce::Rectangle<float> nb { x - 18.0f, rulerY() - 6.0f, 36.0f, 13.0f };
            g.setColour (colours::bgPanel.withAlpha (0.9f * act));
            g.fillRoundedRectangle (nb.expanded (1.0f, 1.0f), 3.0f);
            g.setFont (juce::Font (juce::FontOptions (9.5f)));
            g.setColour (colours::text.brighter (0.35f).withAlpha (act));
            g.drawText (freqText (dispCrossover (i)), nb, juce::Justification::centred);
        }
    }

    // --- add-band hint: big "+" at the top, dashed line breaks just below it (#2) ---
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
        g.drawDashedLine ({ { x, py + arm + 4.0f }, { x, rulerY() - 9.0f } }, dl, 2, 1.3f);
        auto nb = numberChip (0).withX (x - 22.0f);
        g.setColour (colours::bgPanel.withAlpha (0.9f * a));
        g.fillRoundedRectangle (nb.expanded (1.0f, 1.0f), 3.0f);
        g.setFont (juce::Font (juce::FontOptions (9.5f)));
        g.setColour (xoverCol.brighter (0.3f).withAlpha (a));
        g.drawText (freqText (xToFreq (x)), nb, juce::Justification::centred);
    }

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
    if (hoverSolo >= 0)            t = "Solo this band";
    else if (hoverHandle >= 0)     t = "Drag to change the split frequency";
    else if (hoverWidth >= 0)      t = "Band width";
    else if (deleteHit (p) >= 0)   t = "Remove this band";
    else if (hoverAdd >= 0)        t = "Click to add a band split";
    // No tooltip over idle areas -- there is nothing to do there (#13).
    setTooltip (t);
}

void SpectrumImager::updateHover (juce::Point<float> p)
{
    const int N = bandCount();
    hoverHandle = hoverWidth = hoverAdd = hoverDelete = hoverDeleteExact = hoverSolo = -1;

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

    // The delete x shows DIMLY whenever the cursor is over the band or its split line, and
    // BRIGHT (with a pointing hand) once it is directly over the x. When a split LINE is
    // hovered, show the x to its RIGHT (the band the split opens), not the left band's (#1).
    if (N > 1)
    {
        hoverDelete = (h >= 0) ? juce::jmin (h + 1, N - 1) : b;
        hoverDeleteExact = deleteHit (p);
        if (hoverDeleteExact >= 0) setMouseCursor (juce::MouseCursor::PointingHandCursor);
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
    hoverHandle = hoverWidth = hoverAdd = hoverDelete = hoverDeleteExact = hoverSolo = -1;
    scrollHandle = scrollBand = -1;
}
void SpectrumImager::mouseDown (const juce::MouseEvent& e)
{
    if (editingHandle >= 0) commitFreqEditor();
    const auto p = e.position;
    const bool alt = e.mods.isAltDown();

    if (const int sh = soloHit (p); sh >= 0)
    {
        soloPressBand = sh;
        soloDownX = p.x;
        soloPressMs = juce::Time::getMillisecondCounter();
        soloPressAlt = alt; // modifier read at press, like every other alt-click here
        soloHoldActive = soloMovedBand = false;
        return;
    }

    // Press the delete x to ARM it; the band is removed on RELEASE (over the same x) -- while
    // held, the add affordance stays hidden (0.6.16 #2).
    if (! alt) if (const int dB = deleteHit (p); dB >= 0) { pressDeleteBand = dB; hoverAdd = -1; addA = 0.0f; repaint(); return; }

    const int h = handleNearX (p.x);
    if (alt)
    {
        if (h >= 0) resetCrossover (h);
        else { const int b = bandAtX (p.x); if (nearWidthLine (p, b)) resetParam (widthP[b]); }
        return;
    }
    if (h >= 0)
    {
        dragHandle = h; dragBand = -1; dragRemovePending = false;
        handlePressMs = juce::Time::getMillisecondCounter(); handlePressX = p.x; handleHoldActive = false;
        const int M = bandCount() - 1;
        for (int k = 0; k < M; ++k) dragOrigX[k] = freqToX (crossover (k));
        dragGrabDX = p.x - freqToX (crossover (h)); // keep the line under the cursor with this offset (#10)
        beginGesture (freqP[h]); repaint(); return;
    }

    const int b = bandAtX (p.x);
    if (nearWidthLine (p, b))
    {
        // Press only BEGINS the width ("Bandwidth") interaction -- the value is written by
        // mouseDrag, never on the press (v0.8.12). The drag is RELATIVE and modelled on the
        // crossover handle: remember the press Y now, and only once the cursor has moved past a
        // 3 px threshold (widthHoldActive -- the crossover's click-vs-drag idiom) does the Width
        // start moving, anchored so it follows the mouse DELTA from the grab rather than jumping
        // to the absolute cursor. So a bare click, or a click a few px off the line (grab
        // tolerance kWidthGrab = 8 px), or a tiny hand jitter, begins+ends an EMPTY gesture --
        // no value change, no divider jump, no automation/undo step. See mouseDrag.
        dragBand = b; dragHandle = -1;
        widthPressY = p.y;          // for the 3 px drag-engage threshold
        widthHoldActive = false;    // value stays put until the threshold is crossed
        beginGesture (widthP[b]);
        repaint();
        return;
    }
    float ax;
    if (bandAddTarget (b, p.x, ax))
    {
        const int idx = addBandAt (xToFreq (ax));
        if (idx >= 0)
        {
            dragHandle = idx; dragBand = -1; dragRemovePending = false;
            handlePressMs = juce::Time::getMillisecondCounter(); handlePressX = p.x; handleHoldActive = false;
            const int M = bandCount() - 1;
            for (int k = 0; k < M; ++k) dragOrigX[k] = freqToX (crossover (k));
            dragGrabDX = p.x - freqToX (crossover (idx));
            beginGesture (freqP[idx]);
        }
        hoverAdd = -1; addA = 0.0f; // snap away the preview so nothing lingers (#5)
        repaint();
    }
}
void SpectrumImager::mouseDrag (const juce::MouseEvent& e)
{
    if (soloPressBand >= 0)
    {
        if (soloMovedBand || std::abs (e.position.x - soloDownX) > 4.0f)
        {
            if (! soloMovedBand)  { soloMovedBand = true; beginBandMove (soloPressBand); }
            if (! soloHoldActive) { soloHoldActive = true; if (onSoloPreview) onSoloPreview (1 << soloPressBand); }
            moveBand ((float) e.position.x);
        }
        return;
    }
    if (dragHandle >= 0)
    {
        // Dragged far outside the box -> mark for removal on release (merge) and FREEZE.
        // When the cursor returns, the split is recomputed purely from the cursor against
        // the drag-start positions, so it jumps straight to the cursor and any pushed
        // neighbour springs back -- no creep, no stuck split (#10/#11/#18).
        const bool out = bandCount() > 1
                      && (e.position.y < -50.0f || e.position.y > (float) getHeight() + 50.0f
                       || e.position.x < -70.0f || e.position.x > (float) getWidth() + 70.0f);
        dragRemovePending = out;
        // A real drag IS a sustained hold -> show the band-pass preview (but a tiny jitter
        // between the two clicks of a double-click must not, hence the small threshold).
        if (! handleHoldActive && std::abs (e.position.x - handlePressX) > 3.0f) handleHoldActive = true;
        if (! out) dragCrossoverTo (dragHandle, (float) e.position.x - dragGrabDX);
    }
    else if (dragBand >= 0)
    {
        // Relative Width drag past a 3 px threshold (mirrors the crossover's handleHoldActive
        // click-vs-drag gate). Below the threshold nothing moves, so a click or hand jitter
        // leaves Width untouched. On crossing it we anchor dragGrabDY to the CURRENT line, so
        // the value starts exactly where it was (no jump to the absolute cursor) and then
        // follows the mouse delta -- the line stays attached to the grabbed point.
        if (! widthHoldActive && std::abs ((float) e.position.y - widthPressY) > 3.0f)
        {
            widthHoldActive = true;
            dragGrabDY = (float) e.position.y - widthToY (bandWidth (dragBand));
        }
        if (widthHoldActive)
            setParam (widthP[dragBand], yToWidth ((float) e.position.y - dragGrabDY));
    }
    repaint();
}
void SpectrumImager::mouseUp (const juce::MouseEvent& e)
{
    if (pressDeleteBand >= 0)
    {
        const int dB = pressDeleteBand;
        pressDeleteBand = -1;
        if (deleteHit (e.position) == dB) removeBand (dB); // released over the same x -> delete
        updateHover (e.position);
        repaint();
        return;
    }
    if (soloPressBand >= 0)
    {
        if (soloHoldActive) { if (onClearSoloPreview) onClearSoloPreview(); if (soloMovedBand) endBandMove(); }
        else if (soloPressAlt) // Alt/Option quick click: inactive band -> EXCLUSIVE solo
            setSoloMask (bandSoloed (soloPressBand) ? 0 // active: all solos off, as before (0.8.9)
                                                    : (1 << soloPressBand)); // 0.8.10: only this band
        else                  toggleSoloBit (soloPressBand);
        soloPressBand = -1;
        soloHoldActive = soloMovedBand = false;
        updateHover (e.position);
        repaint();
        return;
    }
    if (dragHandle >= 0)
    {
        endGesture (freqP[dragHandle]);
        if (dragRemovePending) removeBand (dragHandle + 1); // drop the dragged split, merge (#18)
    }
    if (dragBand >= 0) endGesture (widthP[dragBand]);
    dragHandle = dragBand = -1;
    dragRemovePending = false;
    handleHoldActive = false; // a click that never became a hold leaves the preview dark
    widthHoldActive  = false;
    updateHover (e.position);
    repaint();
}
// Release-outside safety net (v0.8.12): the editor's 24 Hz reconcile calls this when the
// physical mouse button is up but a drag is still active -- i.e. a mouseUp was lost because
// the button was released outside the plugin window. Close any open parameter gesture and
// clear the press/drag flags WITHOUT firing the on-release actions (delete band / toggle solo /
// commit a band move): a lost release is not a deliberate release-over-target. Cheap no-op when
// nothing is active. Runs on the message thread, like every mouse handler; both this and
// mouseUp guard on the same flags, so whichever runs first makes the other a no-op -- a
// parameter's endChangeGesture can never fire twice.
void SpectrumImager::cancelActiveDrag()
{
    if (dragBand < 0 && dragHandle < 0 && soloPressBand < 0 && pressDeleteBand < 0)
        return;
    if (dragBand   >= 0) endGesture (widthP[dragBand]);
    if (dragHandle >= 0) endGesture (freqP[dragHandle]);
    if (soloPressBand >= 0)
    {
        if (soloHoldActive && onClearSoloPreview) onClearSoloPreview();
        if (soloMovedBand) endBandMove();
    }
    dragBand = dragHandle = soloPressBand = pressDeleteBand = -1;
    dragRemovePending = false;
    handleHoldActive  = false;
    widthHoldActive   = false;
    soloHoldActive = soloMovedBand = false;
    updateHover (getMouseXYRelative().toFloat());
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
        const int M = N - 1;
        for (int k = 0; k < M; ++k) dragOrigX[k] = freqToX (crossover (k)); // seed the projection from the live spots
        dragCrossoverTo (scrollHandle, freqToX (crossover (scrollHandle)) + dy * 28.0f);
    }
    else if (scrollBand >= 0 && scrollBand < N)
    {
        const float step = sgn * juce::jmax (0.01f, std::abs (dy) * 0.30f); // velocity-aware (#15 prior)
        setParam (widthP[scrollBand], juce::jlimit (0.0f, 2.0f, bandWidth (scrollBand) + step));
    }
    repaint();
}

} // namespace anamorph::gui
