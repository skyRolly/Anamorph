#include "SpectrumImager.h"
#include "LookAndFeel.h"
#include "PluginParameters.h"
#include <iterator>
#include <cmath>

namespace anamorph::gui
{

static constexpr float kFreqLo = 20.0f, kFreqHi = 20000.0f;
static constexpr float kMinDb = -90.0f, kMaxDb = 0.0f;
static constexpr float kWidthGrab = 8.0f;
static constexpr float kMinGapPx  = 46.0f; // constant on-screen split spacing (#1/#26)
// The distance at which a split counts as WORTH WRITING -- `writeCrossovers` and `spreadSplits`,
// and nothing else. This paragraph also said `soundMovedUnderGesture` compared against it and that
// "the two must be the same number" (ADR-0039); ADR-0041 removed that coupling -- ownership is a
// parameter question, compared exactly, see `ownsSplit` -- and this was its third stale copy.
static constexpr float kSplitMovedPx = 0.5f;
// ADR-0053: the per-notch magnitudes of a mouse-wheel edit, in ONE place, because a notch inside a
// press and a notch with no button held must move the same control by the same amount. The pixel
// rate serves the split and the rigid band translation (both are horizontal pixel spaces); the
// width pair is the velocity-aware step the wheel has used since 0.6.x -- a per-unit rate with a
// floor, so a very small delta still moves something.
static constexpr float kWheelSplitPx  = 28.0f;
static constexpr float kWheelWidthMin = 0.01f;
static constexpr float kWheelWidthPer = 0.30f;

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
int  SpectrumImager::effectiveSoloMask() const noexcept
{
    return (soloHoldActive && soloPressBand >= 0) ? (1 << soloPressBand) : soloMask();
}

// Display (eased) reads: PAINT uses these so a reset / preset / A-B / undo travels.
float SpectrumImager::dispCrossover (int i) const noexcept { return (i >= 0 && i < 3) ? drawnF[i] : kFreqLo; }
float SpectrumImager::dispWidth (int b) const noexcept     { return (b >= 0 && b < 4) ? drawnW[b] : 1.0f; }
float SpectrumImager::dispLeftX  (int b) const noexcept { return b <= 0 ? plot().getX() : freqToX (dispCrossover (b - 1)); }
float SpectrumImager::dispRightX (int b) const noexcept { return b >= bandCount() - 1 ? plot().getRight() : freqToX (dispCrossover (b)); }

// ADR-0046/0051. THE EDGES ANSWER UNDER THE TOPOLOGY AND THE ROW THEY ARE GIVEN. `bandRightX` read
// `bandCount()` for itself and both read `crossover()` for themselves, so a hit-test that had already
// read the count and the row measured its boxes against LATER readings of both -- and `soloHit` calls
// these four times per band. `n < 0` / `fHz == nullptr` keep the live read for the callers that stamp
// nothing. `splitAt (i, nullptr)` IS `crossover (i)`, so the live path is unchanged to the digit.
float SpectrumImager::bandLeftX  (int b, int, const float* fHz) const noexcept { return b <= 0 ? plot().getX() : freqToX (splitAt (b - 1, fHz)); }
float SpectrumImager::bandRightX (int b, int n, const float* fHz) const noexcept
{
    const int N = n >= 0 ? juce::jlimit (1, 4, n) : bandCount();
    return b >= N - 1 ? plot().getRight() : freqToX (splitAt (b, fHz));
}

juce::Rectangle<float> SpectrumImager::deleteBox (int b, int n, const float* fHz) const noexcept
{
    // Sized close to the add "+", nudged up so it clears the freq chip below (#4).
    return { bandLeftX (b, n, fHz) + 5.0f, rulerY() - 22.0f, 14.0f, 14.0f };
}
juce::Rectangle<float> SpectrumImager::soloBox (int b, int n, const float* fHz) const noexcept
{
    const float cx = 0.5f * (bandLeftX (b, n, fHz) + bandRightX (b, n, fHz));
    return { cx - 9.0f, plot().getY() + 3.0f, 18.0f, 15.0f }; // balanced headphone proportions (#2)
}
juce::Rectangle<float> SpectrumImager::numberChip (int i) const noexcept
{
    return { freqToX (crossover (i)) - 22.0f, rulerY() - 6.0f, 44.0f, 13.0f };
}

// ADR-0046. A DERIVATION ANSWERS UNDER THE TOPOLOGY IT IS GIVEN, NOT THE ONE IT FINDS.
// Both of these used to re-read `bandCount()` for themselves. That is right for a caller that
// only wants to know what is under the cursor RIGHT NOW (hover, paint) and stamps nothing. It is
// wrong for a caller that reads the count, derives an index, and then stamps that index with a
// count: the derivation's read and the caller's read are two different reads of a value THREE
// threads write, so the index could be answered under one topology and stamped with another --
// and a stamp that names a topology the index was never derived in cannot detect anything.
// Passing `n` in makes the two the same read by construction, which is the only way to close the
// window without a lock: `SpectrumImager.cpp:2292` already relies on "handleNearX and addBandAt
// both return an index inside the count they read", and this is what makes that true of the
// count the CALLER read rather than of some later one.
// ADR-0051. AND THE SPLIT ROW IS ONE READING TOO. ADR-0046 made the COUNT one reading per pass and
// ADR-0048 made a band's edges answer under the count its index was derived from; both left the
// split VALUES re-read per call. `bandAtX` compares the cursor against `crossover (i)`, `handleNearX`
// measures the distance to `freqToX (crossover (i))`, and `bandAddTarget` clamps between two more
// `crossover ()` calls -- three readings of a three-element row that the audio thread writes through
// the format wrapper, inside one press. So an index derived against one row could be clamped against
// a later one, which is ADR-0048's defect with the count held fixed. `fHz` is that row, captured
// once by the caller; `nullptr` keeps the live read for the callers that stamp nothing.
void SpectrumImager::captureSplits (float* fHz) const noexcept
{
    for (int i = 0; i < 3; ++i) fHz[i] = crossover (i);
}
float SpectrumImager::splitAt (int i, const float* fHz) const noexcept
{
    return (fHz != nullptr && i >= 0 && i < 3) ? fHz[i] : crossover (i);
}
int SpectrumImager::bandAtX (float x, int n, const float* fHz) const noexcept
{
    const int N = n >= 0 ? juce::jlimit (1, 4, n) : bandCount();
    const float f = xToFreq (x);
    for (int i = 0; i < N - 1; ++i)
        if (f < splitAt (i, fHz)) return i;
    return N - 1;
}
int SpectrumImager::handleNearX (float x, int n, const float* fHz) const noexcept
{
    const int N = n >= 0 ? juce::jlimit (1, 4, n) : bandCount();
    int best = -1; float bestD = 7.0f;
    for (int i = 0; i < N - 1; ++i)
    {
        const float d = std::abs (x - freqToX (splitAt (i, fHz)));
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}
bool SpectrumImager::nearWidthLine (juce::Point<float> p, int b, const float* wNorm) const noexcept
{
    const float w = (wNorm != nullptr && b >= 0 && b < (int) std::size (gestureW) && widthP[b] != nullptr)
                  ? widthP[b]->convertFrom0to1 (wNorm[b]) : bandWidth (b);
    return std::abs (p.y - widthToY (w)) < kWidthGrab;
}
// ADR-0046, THE TWO DERIVATIONS IT NAMED AND LEFT OUT. Both took the count for themselves and both
// let their boxes take it AGAIN, once per `bandRightX` call -- so even a live caller's hit-test was
// several readings of a value three threads write. `N` is resolved ONCE here and handed down, which
// closes that half for every caller including the ones that pass nothing.
//
// THE HALF THAT WAS OPEN IS `soloHit`. Its index is latched as `soloPressBand` and consumed by three
// places -- `tick`'s hold audition, `mouseDrag`'s band move, `mouseUp`'s toggle -- none of which
// re-derives it. Each is guarded by `gestureIsStale()`, which proves the world still MATCHES the
// press; it cannot prove the index was DERIVED in that world. So an ABA return -- the count (or the
// row) moves while this runs and is back before the release -- leaves every guard satisfied over an
// index the press's own layout never had: ADR-0046's amendment made exactly that argument for
// `beginBandMove` while this ADR's "What was NOT done" called the same shape fail-safe here.
//
// `deleteHit` was NOT open, for a reason that ADR does not state: `mouseUp` re-runs it and requires
// `deleteHit (release) == dB`, so a transient index cannot survive an ABA return, and a persistent
// change is refused by `gestureIsStale()`. It is threaded anyway because one reading per pass is the
// rule -- it removes reads rather than adding them -- and that half is NOT claimed as a defect fixed.
int SpectrumImager::deleteHit (juce::Point<float> p, int n, const float* fHz) const noexcept
{
    const int N = n >= 0 ? juce::jlimit (1, 4, n) : bandCount();
    if (N <= 1) return -1;
    for (int b = 0; b < N; ++b)
        if (deleteBox (b, N, fHz).contains (p)) return b;
    return -1;
}
int SpectrumImager::soloHit (juce::Point<float> p, int n, const float* fHz) const noexcept
{
    const int N = n >= 0 ? juce::jlimit (1, 4, n) : bandCount();
    for (int b = 0; b < N; ++b)
        if ((bandRightX (b, N, fHz) - bandLeftX (b, N, fHz)) > 30.0f && soloBox (b, N, fHz).contains (p)) return b;
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
// ADR-0040. THE CHECK AND THE STORE ARE ADJACENT, AND THE RECORD IS MADE AT THE STORE.
//
// This used to be a bare write loop followed by one blanket `captureGestureSound()`. Both halves
// were wrong, and for the same reason: `setParam` is `setValueNotifyingHost`, which stores and then
// dispatches every listener SYNCHRONOUSLY on this thread (juce_AudioProcessorParameter.cpp:59-63,
// :111-121), and one of those listeners is `AudioProcessor::ParameterChangeForwarder`
// (juce_AudioProcessor.cpp:1467), which every format wrapper uses to tell the host. So a host that
// writes a crossover back -- a linked-parameter macro, an automation write-back, a control surface
// echo -- re-enters HERE, between two iterations of this loop.
//
//  * The loop then OVERWROTE it: the old predicate asked "does the live value differ from MY
//    target?", which a large foreign move answers more emphatically, not less. Measured: `the split
//    written from inside the burst (15000.0 Hz) was reclaimed as 10000.0 Hz`.
//  * And `captureGestureSound()` afterwards LAUNDERED it: that function is branch-free and
//    provenance-free, so whatever survived became the ownership baseline and the next
//    `soundMovedUnderGesture()` could not see it either.
//
// Now: `ownsSplit (k)` is compared and the store follows it with nothing in between, so a write
// nested inside store k-1 is seen by iteration k. ADR-0047 sharpens what "nothing in between" has to
// mean: nothing DISPATCHES between them, and nothing RE-READS the parameter either. It used to
// re-read -- the write-worth test below took its own `crossover (k)` -- so a cross-thread write
// landing between the proof and that test was proved absent and then measured against, and a foreign
// value farther from the plan than half a pixel was written over. One reading now serves both; the record is taken from the read-back of THIS
// slot immediately, so nothing a later iteration lets in can be adopted; and slots past `count`
// are left alone rather than adopted wholesale -- the write loop is bounded by the live split count
// while the record used to be bounded by the array size, which adopted the unused slots for free.
// Returns false the moment a slot is not ours: the caller abandons the rest of the event.
bool SpectrumImager::writeCrossovers (const float* xs, int count)
{
    for (int k = 0; k < count; ++k)
    {
        // ADR-0040, round-3 correction 1: the COUNT is re-proved too, not only the value. `count`
        // was read once before this loop and a listener can move mbBands from inside any store.
        if (gestureBands >= 0 && bandCount() != gestureBands) return false;
        // ADR-0047: the proof and the write-worth test are ONE reading. They were two, and a write
        // landing between them was proved absent and then measured against -- so a foreign value
        // FARTHER from the plan than half a pixel was written over, which is the ADR-0040 failure
        // in its cross-thread half.
        const float nk = (freqP[k] != nullptr) ? freqP[k]->getValue() : 0.0f;
        if (! ownsSplit (k, nk)) return false;
        if (std::abs (freqToX ((freqP[k] != nullptr) ? freqP[k]->convertFrom0to1 (nk) : kFreqLo)
                      - xs[k]) <= kSplitMovedPx)
            continue;                                  // nothing to write; the record already stands
        // ADR-0040 round-3 correction 2, sharpened by ADR-0041: confirm the store landed before
        // owning it, in PARAMETER space and exactly. A listener writing this same parameter from
        // inside this very store is caught whatever the size of its write.
        if (! storeOwned (freqP[k], juce::jlimit (kFreqLo, kFreqHi, xToFreq (xs[k])), gestureX[k]))
            return false;
    }
    return true;
}
// ADR-0042. A SPREAD IS A PLAN, AND A PLAN IS APPLIED ONLY TO THE WORLD IT WAS COMPUTED FROM.
// A reset and a text commit both move their neighbours aside to make room for one split, and both
// used to ask the question ADR-0040 removed from `writeCrossovers` (see its header) -- "does the live
// value differ from MY target?" -- which a large foreign move answers MORE emphatically, not less.
// So a host that moved a neighbour from inside the primary store had the plan written over it.
// Measured: `5000.0 Hz was installed and 2000.0 Hz was written over it`, on both paths.
// `was[k]` is the normalised value the plan was computed from; a slot that no longer holds it is
// somebody else's, and the spread stops there exactly as `writeCrossovers` stops. `kSplitMovedPx`
// keeps its one real job -- deciding whether a write is worth making at all.
bool SpectrumImager::spreadSplits (const float* xs, const float* was, int count, int except,
                                   float pinNorm)
{
    for (int k = 0; k < count && k < (int) std::size (freqP); ++k)
    {
        // ADR-0043: THE PIN IS RE-PROVED TOO. Every position in `xs` was computed to make room for
        // it, and each neighbour store dispatches to the host, so a host that moves the pin from
        // inside the FIRST neighbour's store leaves the rest of the plan being applied around a
        // split that is no longer there. Measured: `300.0 / 11407.5 / 15122.0` -- the pin dragged
        // to 300 Hz and the neighbours still spread for a pin at 8440.
        if (except >= 0 && except < (int) std::size (freqP) && freqP[except] != nullptr
            && ! juce::exactlyEqual (freqP[except]->getValue(), pinNorm))
            return false;
        // ADR-0042, the same correction ADR-0040's round 3 made to `writeCrossovers`: the COUNT is
        // re-proved as well as the value. `count` was read before the plan was computed and three
        // dispatches follow it -- the primary store, the gesture close and each neighbour store --
        // any of which a host can answer by writing mbBands. `was[k]` cannot see that, because
        // mbBands is a different parameter.
        if (bandCount() - 1 != count) return false;
        if (k == except || freqP[k] == nullptr) continue;
        // ADR-0047: one reading for the proof and for the write-worth test below.
        const float nk = freqP[k]->getValue();
        if (! juce::exactlyEqual (nk, was[k])) return false;
        if (std::abs (freqToX (freqP[k]->convertFrom0to1 (nk)) - xs[k]) <= kSplitMovedPx) continue;
        float owned = 0.0f;
        if (! storeOwned (freqP[k], juce::jlimit (kFreqLo, kFreqHi, xToFreq (xs[k])), owned))
            return false;
    }
    return true;
}
// The two halves of the one rule, so a future consumer has one place to read it. `gestureBands < 0`
// is "nothing in flight", and then there is nothing to own and nothing to refuse. ADR-0043: the
// WHEEL is no longer an example of that state while it writes -- it ends any drag (clearing
// `gestureBands`) and then latches the count around its own burst, because that burst is up to
// three stores with a host dispatch between each. The waiver is for having no gesture, not for
// being a particular caller. BOTH halves compare the normalised value EXACTLY: this paragraph used to say a split was
// owned "within the same half pixel `writeCrossovers` uses to decide a write is worth making", which
// is what ADR-0041 removed and what the paragraph below replaces it with -- a reader arriving here
// was told the opposite of what the code does.
// ADR-0041. OWNERSHIP IS A PARAMETER QUESTION, NOT A PIXEL ONE. `kSplitMovedPx` decides whether a
// write is worth making; it was also deciding whether a value was ours, and those are different
// questions with different units. Half a display pixel is 0.30-0.65 % of the frequency -- 0.19 Hz at
// 30 Hz, 3.5 Hz at 1 kHz, 32 Hz at 10 kHz, 61 Hz at the top -- against a parameter with no interval
// that resolves about 0.00055 Hz at 1 kHz, so an external change up to five orders of magnitude
// above the parameter's own resolution read as the gesture's own and was overwritten. Exact
// comparison of the normalised value has no such gap and costs one float compare.
bool SpectrumImager::ownsSplit (int k) const noexcept
{
    if (gestureBands < 0) return true;
    if (k < 0 || k >= (int) std::size (gestureX) || freqP[k] == nullptr) return true;
    return juce::exactlyEqual (freqP[k]->getValue(), gestureX[k]);
}
bool SpectrumImager::ownsWidth (int b) const noexcept
{
    if (gestureBands < 0) return true;
    if (b < 0 || b >= (int) std::size (gestureW) || widthP[b] == nullptr) return true;
    return juce::exactlyEqual (widthP[b]->getValue(), gestureW[b]);
}
// ADR-0047. THE SAME QUESTION, ASKED ABOUT A READING THE CALLER ALREADY HAS. Every caller that
// proves ownership and then USES the value was reading the parameter twice: once here and once for
// the plan, the anchor or the write-worth test. Both reads are pure, so nothing can dispatch
// between them on this thread -- but the audio thread writes these parameters through the format
// wrapper, and a write that lands between the two passes the proof while the plan still carries the
// world from before it. Measured on the drag capture against a continuously moving automation lane:
// 92 splits in 1200 drags written back to their pre-automation position, inside the user's own
// change gesture, and 200/200 with the window widened to 200 us -- 0 for both after this change.
// Taking the caller's reading makes the pair one measurement and the class unreachable by
// construction, at no cost: it removes a read rather than adding one (`--split-snapshot-probe`).
bool SpectrumImager::ownsSplit (int k, float norm) const noexcept
{
    if (gestureBands < 0) return true;
    if (k < 0 || k >= (int) std::size (gestureX) || freqP[k] == nullptr) return true;
    return juce::exactlyEqual (norm, gestureX[k]);
}
bool SpectrumImager::ownsWidth (int b, float norm) const noexcept
{
    if (gestureBands < 0) return true;
    if (b < 0 || b >= (int) std::size (gestureW) || widthP[b] == nullptr) return true;
    return juce::exactlyEqual (norm, gestureW[b]);
}
// Store, then confirm the parameter holds THIS store's result before owning it. `expect` is computed
// by the same two conversions the parameter itself performs (juce_AudioParameterFloat.cpp:97-98), so
// with nothing else writing it is bit-identical to the read-back -- and any difference is somebody
// else's hand, at any magnitude, not merely one bigger than a display pixel.
bool SpectrumImager::storeOwned (juce::RangedAudioParameter* p, float plain, float& ownedNorm) noexcept
{
    if (p == nullptr) return false;
    const float norm   = p->convertTo0to1 (plain);
    const float expect = p->convertTo0to1 (p->convertFrom0to1 (norm));
    p->setValueNotifyingHost (norm);
    if (! juce::exactlyEqual (p->getValue(), expect)) return false;
    ownedNorm = expect;
    return true;
}
// Seed the drag-start positions for a gesture that is about to start. EVERY slot of
// dragOrigX, not just the `bandCount() - 1` splits in use at the press -- because the two
// consumers (dragCrossoverTo, moveBand) re-read a LIVE `bandCount()`, and a host write of
// mbBands that RAISES Bands part-way through the gesture makes them ask projectFromOrig
// for more origins than a "splits in use" capture ever wrote. Those slots then still held
// the {0,0,0} initialiser or a previous drag's positions, and projectFromOrig's copy loop
// pulled the new splits toward them: x = 0 is left of the plot, so the min-gap pass packed
// them hard against the dragged split and writeCrossovers pushed that to the host, inside
// the change gesture the drag had opened -- a split jumping to a frequency the user never
// chose, into the automation lane and the undo stack. Every slot is a real split's live
// position (freqP[0..2] all exist whatever Bands says; only some are in USE), so seeding
// all of them costs two extra reads once per gesture and makes the class impossible.
void SpectrumImager::captureDragOrigins() noexcept
{
    // ADR-0047. THE STAMP IS TAKEN FIRST AND THE ORIGIN IS DERIVED FROM IT, because the two used to
    // be separate reads of the same parameter: `crossover (k)` here and `freqP[k]->getValue()` a
    // line later inside `captureGestureSound`. An automation write landing between them left the
    // ORIGIN holding the old position and the STAMP holding the new value -- so `ownsSplit` said the
    // gesture owned a world it had never measured, and the drag wrote the split back to where it was
    // before the automation moved it, inside its own change gesture. Measured 92 times in 1200 drags
    // against a moving lane, and 200/200 with the window widened. Derived, they are one
    // measurement: `convertFrom0to1` is pure arithmetic, so with nothing racing this is bit-identical
    // to what the second read returned.
    captureGestureSound(); // the gesture starts owning exactly what it just measured
    seedDragOrigins();
}
// ADR-0051. THE ORIGIN HALF ON ITS OWN, for the callers that have already stamped -- TWO of them
// since 2026-09-09: `mouseDown`'s handle-press branch, and `beginBandMove`, which is reached
// mid-gesture with a record the drag handler's gate has just proved. `mouseDown` stamps
// at the TOP of the handler, for every branch (ADR-0038/0039), and the handle-press branch then
// called `captureDragOrigins()` -- which stamps AGAIN. Two stamps in one press are two readings of
// the row this press is about to steer: the index came from the first and the anchor from the
// second, so a write landing between them put the drag's grab offset on a split the press had not
// measured. Deriving the origins from the stamp the press already holds removes a reading rather
// than adding one, which is ADR-0047's own test for this shape. The ADD branch still re-stamps, and
// must: `addBandAt` has just changed the count AND written the row, so the press's first stamp
// describes a layout that no longer exists.
void SpectrumImager::seedDragOrigins() noexcept
{
    for (int k = 0; k < (int) std::size (dragOrigX); ++k)
        dragOrigX[k] = (freqP[k] != nullptr) ? freqToX (freqP[k]->convertFrom0to1 (gestureX[k]))
                                             : freqToX (kFreqLo);
}
// The positions this gesture last left behind. Called at every gesture START (through
// captureDragOrigins) and after every write the gesture makes (through writeCrossovers), so
// the array always answers "where did I leave the splits?" and never "where were they once".
//
// AT A START, AND NOWHERE ELSE. This is a blanket, provenance-free copy of the live row into the
// record, so it is correct exactly where no record is in force -- or where this class has just
// changed the row itself, synchronously, before any identifier existed (the add branch). Called
// with a live record it does the opposite of its job: it launders somebody else's write into the
// gesture's own ownership claim. `beginBandMove` did that until 2026-09-09; it now calls
// `seedDragOrigins` instead. A future caller reaching for this function mid-gesture is the same
// defect again.
void SpectrumImager::captureGestureSound() noexcept
{
    for (int k = 0; k < (int) std::size (gestureX); ++k)
        gestureX[k] = (freqP[k] != nullptr) ? freqP[k]->getValue() : 0.0f;
    // ADR-0040: the widths belong to the gesture's world too. Seeded here at every gesture start,
    // and refreshed at the width store itself rather than in any blanket pass. ADR-0041: normalised.
    for (int b = 0; b < (int) std::size (gestureW); ++b)
        gestureW[b] = (widthP[b] != nullptr) ? widthP[b]->getValue() : 0.0f;
}
// ADR-0039. THE COUNT IS NOT THE WHOLE TOPOLOGY. A restore, a preset, an A/B apply, an undo or
// an automation lane can install a different sound at the SAME band count; `gestureBands` sees
// nothing, the drag continues, and `projectFromOrig` pulls every unpinned split back toward
// `dragOrigX` -- the sound that has just been replaced -- with `writeCrossovers` pushing the
// difference to the host inside the change gesture the drag opened. Measured: a restore of
// mbFreqHigh to 15 kHz under a drag of the FIRST split was pulled back to 10 kHz.
//
// Detected by self-comparison rather than by a cross-component counter, and ADR-0042 corrects
// WHY. The reason recorded here (and in ADR-0041's option table) was that the processor's
// counter is unreachable and that a silent restore advances nothing. Both are wrong:
// `soundGeneration()` is public (PluginProcessor.h:159) and `apvts.processor` is a public
// member, and `reassertParameters (..., notifyHost = false)` DOES bump `soundParamGen`
// (PluginProcessor.cpp:779-780, with a comment saying so, added a month before ADR-0041). The
// real reasons are three: the processor listens to every non-view parameter (:43-47) and bumps
// unconditionally (PluginProcessor.h:171-174), so this class's OWN stores move the counter it
// would be watching; one global counter conflates all eight multiband parameters with every
// other sound parameter; and the bump is once per PASS and relaxed, so it is invisible in the
// window between two adjacent stores, which is the only window that matters here. The parameter's
// own normalised value is the stronger version stamp: per-parameter, content-addressed, needing
// no writer cooperation, and self-stamped at the store. Every writer of these parameters is
// either this class or somebody outside it -- the engine only reads them
// (AnamorphEngine.cpp:609) -- so "it moved and I did not move it" is exact and needs no plumbing.
bool SpectrumImager::soundMovedUnderGesture() const noexcept
{
    if (gestureBands < 0) return false;
    // ADR-0045 CONSIDERED AND REJECTED. This round's audit proposed narrowing these two loops to
    // the slots the LATCHED count uses -- `gestureBands - 1` splits, `gestureBands` widths -- on the
    // grounds that a foreign write to a slot the live layout does not read voids a gesture for
    // nothing. It is a FALSE POSITIVE, and State test 71 leg G is the test that says so: at two
    // bands a foreign write to slot 2 must stop the drag, deliberately. The gesture's plan is
    // `projectFromOrig` over ALL slots from a `captureDragOrigins` that seeded ALL of them, so a
    // slot this pass did not write is still part of the world the plan was computed in -- and the
    // count can rise at any moment and start reading it. Voiding is the conservative half of
    // ADR-0038, and the narrowing was measured to break that leg.
    for (int k = 0; k < (int) std::size (gestureX); ++k) if (! ownsSplit (k)) return true;
    for (int b = 0; b < (int) std::size (gestureW); ++b) if (! ownsWidth (b)) return true;
    return false;
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

    // A PIN CAN BE STALE, and only `out[0 .. count - 1]` was written above. `beginBandMove`
    // latches soloMoveLeft/soloMoveRight from the band count at the press; `moveBand` then
    // re-reads a LIVE `bandCount()`, which a host write of `mbBands` -- an automation lane,
    // or the sound half of a state restore -- can lower part-way through the gesture. The
    // two pin guards already rejected an index past `count`; `leftPin`/`rightPin` were
    // computed from the RAW arguments and did not, so a stale pin reached the pull loops
    // below and `out[k + 1]` read a slot the copy loop never wrote -- an indeterminate
    // value (UB) that then steered a crossover write the user never made, inside the
    // gesture the move had opened. Validate ONCE and use the same values for the pins and
    // for the loop bounds, so the two can never disagree again.
    const int pA = (pinA >= 0 && pinA < count) ? pinA : -1;
    const int pB = (pinB >= 0 && pinB < count) ? pinB : -1;
    if (pA >= 0) out[pA] = juce::jlimit (lo, hi, xA);
    if (pB >= 0) out[pB] = juce::jlimit (lo, hi, xB);

    const int leftPin  = (pA >= 0 && pB >= 0) ? juce::jmin (pA, pB) : juce::jmax (pA, pB);
    const int rightPin = juce::jmax (pA, pB);
    if (leftPin < 0) return;

    for (int k = leftPin - 1;  k >= 0;    --k) out[k] = juce::jmin (orig[k], out[k + 1] - kMinGapPx);
    for (int k = rightPin + 1; k < count; ++k) out[k] = juce::jmax (orig[k], out[k - 1] + kMinGapPx);

    // Safety: keep everything ordered and inside the edges (a no-op when the pins fit).
    out[0] = juce::jmax (out[0], lo);
    for (int k = 1; k < count; ++k)         out[k] = juce::jmax (out[k], out[k - 1] + kMinGapPx);
    out[count - 1] = juce::jmin (out[count - 1], hi);
    for (int k = count - 2; k >= 0; --k)    out[k] = juce::jmin (out[k], out[k + 1] - kMinGapPx);
}
// ADR-0046: `n` is the topology the CALLER proved, and it decides how many splits the plan
// covers. Reading it here instead would make the plan's extent a different reading from the one
// `writeCrossovers` proves each store against -- which cannot write a wrong value (the first
// store's `bandCount() != gestureBands` refuses the whole burst) but does leave the burst's
// extent and the burst's proof disagreeing, and an ABA return to the stamped count between the
// two reads would let a plan sized under the wrong topology through. -1 keeps the live read.
bool SpectrumImager::dragCrossoverTo (int handle, float x, int n)
{
    const int M = (n >= 0 ? juce::jlimit (1, 4, n) : bandCount()) - 1;
    if (handle < 0 || handle >= M) return true; // nothing to steer is not a loss of ownership
    float out[3];
    projectFromOrig (out, dragOrigX, M, handle, x, -1, 0.0f);
    return writeCrossovers (out, M);
}
// ADR-0048. THE TARGET AND ITS EDGES ANSWER UNDER ONE TOPOLOGY. `b` is derived by the caller --
// `bandAtX (p.x, gestureBands)` in `mouseDown` -- and this used to read `mbBands` again to work out
// where that band ENDS. Two readings, three threads: a count raised between them made `b < N - 1`
// flip, so the click was clamped against a split edge that belongs to a layout the press never saw.
// Measured before this argument existed: a click at 15 kHz in the top band of a two-band layout was
// written as a split just under the 8 kHz edge the raised count introduced -- 55 misplacements in
// 4800 clicks against a lane moving the count (`--add-target-probe`: 6, 28 and 21 per 1600), against
// a control that places the split at 15030.7 Hz every time, and 0 in 4800 after. Same shape and same
// fix as ADR-0046's for the count itself.
//
// WHAT THAT DID NOT CLOSE, AND ADR-0051 DOES, because the count is only half of a topology: `lo` and
// `hi` used to read `crossover()` LIVE, so the edges could be split VALUES a same-count layout change
// had already repositioned -- the same mismatch one level down, with the count held fixed. It is
// closed with ADR-0047's instrument rather than ADR-0046's: ONE capture of the split row, taken by
// the caller and shared by the derivation (`bandAtX`) and by the target's boundaries. `mouseDown`
// pays nothing for it -- the press already stamps the row at the top (`captureGestureSound`), so the
// row is derived from that stamp rather than read again. Only the RISING count direction misplaced,
// too: a count falling in the window widens the clamp (safe), and one rising past four drops the
// click entirely (fail-safe but lossy, in ADR-0046's own vocabulary).
bool SpectrumImager::bandAddTarget (int b, float x, float& outX, int n, const float* fHz) const noexcept
{
    const int N = n >= 0 ? juce::jlimit (1, 4, n) : bandCount();
    if (N >= 4) return false;
    auto r = plot();
    // The whole band (minus a small inset) offers an add affordance; the actual
    // split lands at the clamped click and neighbours spread to fit (#25).
    const float lo = (b > 0     ? freqToX (splitAt (b - 1, fHz)) : r.getX())     + 6.0f;
    const float hi = (b < N - 1 ? freqToX (splitAt (b,     fHz)) : r.getRight()) - 6.0f;
    if (lo >= hi) return false;
    outX = juce::jlimit (lo, hi, x);
    return true;
}

// ----------------------------------------------------------------------------
//  Parameter writes
// ----------------------------------------------------------------------------
void SpectrumImager::beginGesture (juce::RangedAudioParameter* p) { if (p) p->beginChangeGesture(); }
void SpectrumImager::endGesture   (juce::RangedAudioParameter* p) { if (p) p->endChangeGesture(); }
namespace
{
// Holds the ownership claim for a scope and releases it on every exit, including an early return
// from inside a branch. RE-ENTRANT-AWARE, unlike the `bool` this started as: it has two users now
// (`mouseUp`'s release action and `beginBandMove`'s startup) and the second dispatches from inside
// `mouseDrag`, so the two CAN nest through a host that pumps the message loop. A depth releases the
// claim when the OUTERMOST scope exits, which is the only correct answer once nesting is possible.
struct ScopedGestureAction
{
    explicit ScopedGestureAction (int& d) noexcept : depth (d) { ++depth; }
    ~ScopedGestureAction() noexcept { --depth; }
    ScopedGestureAction (const ScopedGestureAction&) = delete;
    ScopedGestureAction& operator= (const ScopedGestureAction&) = delete;
    int& depth;
};
// ADR-0053: names the control a standalone scroll is editing for the length of its change gesture
// and un-names it on every exit path, so a later gesture of some other kind can never inherit the
// name and be merged into the scroll's undo step.
struct ScopedWheelName
{
    using Fn = std::function<void (const juce::AudioProcessorParameter*)>;
    ScopedWheelName (const Fn& f, const juce::AudioProcessorParameter* p) : fn (f) { if (fn) fn (p); }
    ~ScopedWheelName() { if (fn) fn (nullptr); }
    ScopedWheelName (const ScopedWheelName&) = delete;
    ScopedWheelName& operator= (const ScopedWheelName&) = delete;
    const Fn& fn;
};
} // namespace
// ADR-0046: the same contract `resetParam` has carried since ADR-0045, for the one store that
// had no proof at all. `expectedBands` is checked with NOTHING between it and the store -- this
// function opens no gesture, so unlike `resetParam` there is no dispatch to step over, and the
// only thing that can move the count in the gap is another thread. -1 keeps the four stores
// inside `addBandAt` / `removeBand` exactly as they were: they prove the count in their own loops
// one line above, and doubling that up would say the same thing twice.
void SpectrumImager::setParam (juce::RangedAudioParameter* p, float plain, int expectedBands)
{
    if (p == nullptr) return;
    if (expectedBands >= 0 && bandCount() != expectedBands) return;
    p->setValueNotifyingHost (p->convertTo0to1 (plain));
}
void SpectrumImager::resetParam (juce::RangedAudioParameter* p, int expectedBands)
{
    if (p == nullptr) return;
    if (onSweep) onSweep();
    // ADR-0045. The band index its callers pass is derived from `bandAtX` OUTSIDE this call, and
    // `beginChangeGesture` below dispatches to every listener before the store -- so a host lane
    // answering the gesture open by dropping Bands left this resetting the width of a band the
    // topology no longer has: an automation touch and an undo step for a band that is not there,
    // and a value that pops into the sound if the count ever rises again. The same shape
    // `setBands` and `setSoloMask` have carried since ADR-0041: the check and the store it guards
    // have nothing between them. -1 means the caller has no topology to prove (the non-multiband
    // resets).
    p->beginChangeGesture();
    if (expectedBands < 0 || bandCount() == expectedBands)
        p->setValueNotifyingHost (p->getDefaultValue());
    p->endChangeGesture();
}
// ADR-0040, round-3 correction 3: THE COMMIT POINT OPENS A GESTURE FIRST, so a check in the caller
// is NOT adjacent to this store. `beginChangeGesture` dispatches parameterGestureChanged(idx, true)
// to every listener SYNCHRONOUSLY before the value goes out (juce_AudioProcessorParameter.cpp:65-86),
// and one of those listeners is the wrapper that tells the host -- so a host answering the gesture
// open by writing mbBands had the commit written straight over it. Measured: `Bands was moved to 2
// from inside the gesture that opens the commit, and the commit wrote 3 over it`. `expectedBands`
// re-proves the topology in the only place that is adjacent: after the open, before the store.
// -1 means the caller has no topology to prove (a plain solo toggle).
// ADR-0042: AND THE FAR SIDE TOO. ADR-0041's rule -- a conditional store reports whether it
// COMMITTED -- was implemented as far as the precondition and no further: `stored` used to be set
// the instant the store was issued. `setValueNotifyingHost` dispatches every listener synchronously
// from inside itself (juce_AudioProcessorParameter.cpp:59-63, :111-121) and `endChangeGesture`
// dispatches again, so a host answering either one by writing mbBands took the count away and this
// still reported success. Measured: `the count store did not stand (Bands 2) and the press still
// latched the add and opened 1 gesture(s) on the new split`. The count is re-read once, after the
// gesture closes, which is the last instant this function can still be believed.
bool SpectrumImager::setBands (int n, int expectedBands, int expectedMask)
{
    auto* p = bandsP;
    if (p == nullptr) return false;
    const int want = juce::jlimit (1, 4, n);
    bool stored = false;
    p->beginChangeGesture();
    // ADR-0044. `expectedMask` is why this guard is more than a repeat of the caller's. The count
    // is the store that REINTERPRETS the solo word (`SoloMonitor::process` masks it with
    // ((1 << bands) - 1)), and `beginChangeGesture` above dispatches to every listener BEFORE this
    // line -- so a host answering the gesture open by writing mbSolo is invisible to any check the
    // caller makes outside. Same shape `setSoloMask` has carried since ADR-0041, same reason: the
    // check and the store it guards must have nothing between them.
    if ((expectedBands < 0 || bandCount() == expectedBands)
        && (expectedMask < 0 || soloMask() == expectedMask))
    {
        p->setValueNotifyingHost (p->convertTo0to1 ((float) want));
        stored = true;
    }
    p->endChangeGesture();
    // WHAT THIS FAR SIDE DOES NOT PROVE, and why that is enough (review round 2026-09-08).
    // It re-reads the COUNT only. A listener that moves mbSolo from inside the store's own
    // dispatch, or from inside `endChangeGesture`, is invisible here and this still returns true --
    // the mirror of the same gap in `setSoloMask` below. Two things cover it, neither of them in
    // this function, which is why the answer is a documented invariant and a test rather than a
    // wider re-read:
    //   1. the only caller that ACTS on this result is `addBandAt`, and it uses it for
    //      `resultingBands` and the returned split index -- both mask-independent. `removeBand`
    //      discards it, and both are the LAST store of their transaction, so there is nothing left
    //      to abandon on a mask that moved after the count already committed;
    //   2. the state that reaches is ADR-0039's PARKED SOLO BIT -- a bit above the live count,
    //      inert while hidden (`SoloMonitor.cpp:85` and the painter both mask with
    //      ((1 << bands) - 1)) and exact when the count returns. State test 79 leg A measures that
    //      against a control that uses no reentrancy at all and finds the two states identical,
    //      before and after the round trip.
    // CROSS-READING THE MASK HERE WAS EVALUATED AND REJECTED: it would return false when the count
    // DID commit, so `addBandAt` would abandon a successful add because a foreign writer touched
    // the mask afterwards. That is the "aborting at a leaf is worse than completing" ADR-0042
    // measured at `Bands 3 mask 0x5 wLo 1.750` -- a more precise report bought with worse
    // behaviour.
    return stored && bandCount() == want;
}

// --- Solo mask ---------------------------------------------------------------
// Same shape, same reason (ADR-0040 round-3 correction 3). `expectedMask` additionally proves the
// word this plan was computed from is still the word being replaced; -1 waives both.
bool SpectrumImager::setSoloMask (int mask, int expectedBands, int expectedMask)
{
    if (soloP == nullptr) return false;
    mask &= 0x0F;
    bool stored = false;
    soloP->beginChangeGesture();
    // ADR-0045, THE SOUND HALF OF ITS OWN SECOND SENTENCE: "a store whose gesture bracket
    // dispatches before it proves the topology inside the bracket, never outside it." That is what
    // the two clauses below do for the COUNT -- and ADR-0039 settled that the count is not the whole
    // topology. `beginChangeGesture()` one line up notifies every listener SYNCHRONOUSLY, so a host
    // answering it with a same-count sound install lands INSIDE this bracket, after the caller's
    // gate and before this store; the band index the caller latched then names a band whose
    // boundaries the user never saw, and the solo bit is written to it anyway.
    //
    // Reentrant, therefore deterministic -- no thread and no probe are needed to reach it. The
    // predicate self-disables when no gesture is in force (`gestureBands < 0`), so `removeBand`'s
    // own mask remap and every record-less caller are unaffected; and with nothing racing the
    // caller's gate has just proved these values, so nothing that used to commit stops committing.
    if ((expectedBands < 0 || bandCount() == expectedBands)
        && (expectedMask < 0 || soloMask() == expectedMask)
        && ! soundMovedUnderGesture())
    {
        soloP->setValueNotifyingHost (soloP->convertTo0to1 ((float) mask));
        stored = true;
    }
    soloP->endChangeGesture();
    // ADR-0042: confirmed on the far side as well -- see setBands. Measured on the value dispatch
    // rather than the gesture open: `the mask store was overwritten from inside its dispatch and the
    // transaction carried on: Bands 3 with mask 0x9`. `soloMask()` rounds through std::lround, so
    // the store's own round trip cannot move the word this decodes to.
    // AND THE SAME LIMIT AS `setBands`, mirrored: this re-reads the MASK only, so a listener that
    // moves mbBands from inside this store's dispatch leaves this returning true with the topology
    // it was handed already gone. What covers it is the CALLER'S NEXT LINE -- `addBandAt:839` and
    // `removeBand:953` both re-prove the count as the first thing they do after this returns, with
    // only pure reads in between -- plus `setBands`' own `expectedBands` at the end of the burst.
    // The solo-click callers discard the result entirely and store nothing after it. State test 79
    // leg C holds that cover: it drives an add whose count moves inside THIS store and asserts the
    // transaction abandons. Mutation M1 (that one re-proof removed) survives, because
    // `setBands`' near-side guard is a second layer; M2 (every layer removed) kills leg C with
    // `the add committed its count (Bands 3) after the topology it proved had already moved to 4`.
    // The cover is defence in depth, not one line, and leg C pins the combination.
    return stored && soloMask() == mask;
}
// ADR-0041: the toggle reads the word it is about to replace and names it, so the store proves both
// halves of what it assumed -- the topology the band index belongs to, and the mask it is toggling.
bool SpectrumImager::toggleSoloBit (int b, int expectedBands)
{
    const int m = soloMask();
    return setSoloMask (m ^ (1 << b), expectedBands, m);
}

// ADR-0046, COMPLETED HERE -- and this function is the sibling that ADR named and did not convert.
// Its own comment on `dragCrossoverTo`, one screen below, states the mechanism and the failure:
// taking the extent from a live read "cannot write a wrong value (the first store's
// `bandCount() != gestureBands` refuses the whole burst) but does leave the burst's extent and the
// burst's proof disagreeing, and an ABA return to the stamped count between the two reads would let
// a plan sized under the wrong topology through."
//
// A BAND MOVE TOOK THREE READINGS AND PROVED ONE OF THEM:
//   1. here, `bandCount()` -- the two pins (`soloMoveLeft`/`soloMoveRight`) and the T range;
//   2. `moveBand`, `bandCount() - 1` -- the plan's EXTENT;
//   3. `writeCrossovers`, `bandCount() != gestureBands` -- the per-store proof, against the PRESS.
// Reading 1 is separated from reading 2 by this function's own `beginGesture` calls, which dispatch,
// so a host answering the gesture open moves them apart deterministically. Nothing proves reading 2
// at all, so a count read HIGH there sizes the plan for a layout the press never saw, and an ABA
// return to the stamped count lets every per-store check pass over it. MEASURED at three bands
// against a lane alternating mbBands 3/4: 11 band moves in 1200 wrote `mbFreqHigh` -- a split a
// three-band layout does not use -- inside the user's own change gesture, and so into the host's
// automation lane and undo history. 0 after (`--band-move-probe`).
//
// Both this function and `moveBand` now take the caller's proved topology, and they are ONE change:
// fixing the extent while the pins stay derived under an unproved reading leaves the plan proved and
// its pins still not.
void SpectrumImager::beginBandMove (int b, int n)
{
    // ADR-0050, APPLIED TO THE ONE STARTUP THAT OPENS TWO GESTURES. This function is the only place
    // in the class that brackets MORE THAN ONE parameter, and the two opens are two statements:
    // the first one DISPATCHES, and `mouseDrag` published `soloMovedBand = true` before calling in.
    // So a host that pumps the message loop from that first `beginChangeGesture` lands `tick`'s
    // reconcile -- `if (gestureIsStale()) cancelActiveDrag();` -- in `cancelActiveDrag` with the
    // move's members set and only half its gestures open. That call ran `endBandMove()`, which
    // closes BOTH pins
    // from the members: an `endChangeGesture` on a parameter that was never opened, i.e. a NEGATIVE
    // open-gesture count in the processor and a spurious undo boundary. It also cleared the
    // members, so the statement below never opened the second pin at all, and it cleared
    // `soloPressBand` and `gestureBands`, so the handler this returns into then auditioned
    // `1 << -1` -- undefined behaviour, reaching the processor as a mask the press never named --
    // and called `moveBand` with every ownership predicate self-disabled, writing the pre-press
    // split positions back over whatever the host had just installed, outside any change gesture.
    //
    // AND THE FIRST PIN IS LEFT OPEN FOR GOOD, which is the worst of it and the one an early draft
    // of this comment missed. JUCE walks `listeners` in REVERSE
    // (`for (int i = listeners.size(); --i >= 0;)`, juce_AudioProcessorParameter.cpp), and the
    // processor registers itself at construction, so a host listener added later is notified FIRST.
    // The whole nested cancellation therefore runs -- and both of its `endChangeGesture`s reach
    // `AnamorphAudioProcessor::parameterGestureChanged` while `openGestures` is still 0, where its
    // `else if (openGestures > 0 ...)` makes them no-ops -- BEFORE the outer `beginChangeGesture`
    // reaches the processor and takes the count to 1. Nothing can bring it down: every identifier
    // is already cleared, so no later `endBandMove`, `mouseUp` or `cancelActiveDrag` closes that
    // pin. `pollUndoCoalesce` refuses to commit while `openGestures > 0`, so UNDO SILENTLY STOPS
    // RECORDING every subsequent sound edit until an A/B switch, preset load, undo or redo zeroes
    // the count. Measured: a properly bracketed Width edit after one interrupted startup records no
    // undo step at all.
    //
    // THE RECONCILE THAT IS LIVE HERE IS `tick`'S, AND ONLY `tick`'S, which is the opposite of the
    // release-side windows. During a DRAG the button is genuinely down, so the editor's stuck-drag
    // reconcile (`isMouseButtonDownAnywhere() && ! anyPhysicalMouseButtonDown()`) is inert -- KI-013
    // was resolved in round 4 by giving that predicate the OS's real button state, which is exactly
    // what makes it inert here. And `tick`'s gate is FALSE on entry: `mouseDrag`'s first statement
    // has just proved the record, and everything between that proof and this dispatch is a pure
    // computation. So the sequence is not "a reconcile can arrive", it is: the host writes a
    // parameter from inside the gesture open, which makes the gate true, and the loop it pumps then
    // runs the reconcile. Stated rather than glossed, because it is a precondition and not a
    // free-standing re-entry.
    //
    // Measured, all four, in that order: State test 83 leg E -- the unopened pin closed once, mask
    // 0x80000000, a host's 6500 Hz split written back to 2000.0 Hz with the gesture count at -1, and
    // a later bracketed edit recording no undo step.
    //
    // The claim below is the same one `mouseUp` takes, and for the same reason ADR-0050 gives: an
    // action that has published its identifiers owns the record until it has finished establishing
    // them. Nothing here needs the cancellation to happen NOW -- `moveBand`, one statement later,
    // re-proves the count and every split it writes, and `tick`'s next reconcile still fires -- so
    // declining costs a few instructions of latency and nothing else.
    const ScopedGestureAction ownStartup (gestureActionDepth);
    const int N = (n >= 0 ? juce::jlimit (1, 4, n) : bandCount());
    const int M = N - 1;
    auto r = plot();
    soloMoveLeft  = (b > 0)     ? b - 1 : -1;
    soloMoveRight = (b < N - 1) ? b     : -1;

    // Anchor the move: the band translates RIGIDLY by T = clamp(cursor - anchor) so each
    // split tracks the cursor 1:1, the band keeps its width while it pushes neighbours
    // aside, and a pushed neighbour springs back on the way out (0.6.13 #3/#8/#9/#10).
    bandAnchorX = soloDownX;
    // ADR-0051, THE SECOND CALLER OF ITS OWN RULE. This line was `captureDragOrigins()`, which is
    // `captureGestureSound(); seedDragOrigins();` -- and the stamping half had no business here.
    //
    // A BAND MOVE IS NOT A GESTURE START. The press stamped the record at the top of `mouseDown`,
    // and `mouseDrag`'s first statement -- `if (gestureIsStale()) { cancelActiveDrag(); return; }`
    // -- has just PROVED that stamp still describes the world, all three splits and all four widths,
    // compared exactly. This function is reached a few instructions later, having written nothing.
    // So there was a live, freshly-proved record here, and the old line overwrote it wholesale from
    // whatever the parameters happened to hold at that instant. A same-count write landing in that
    // gap was not merely missed, it was ADOPTED: copied into the very record every later check
    // proves against, after which `ownsSplit`/`ownsWidth` compare the foreign value with itself and
    // answer "mine" for the rest of the gesture. That is the exact inverse of what the record is
    // for -- `exactlyEqual (freqP[k]->getValue(), gestureX[k])` is a decision procedure for "it
    // moved and I did not move it" only while nothing but this gesture's own confirmed stores
    // (`storeOwned`) ever writes it.
    //
    // THE WIDTH HALF IS THE WORSE ONE, and it is the reason this is not merely tidier. A band move
    // never writes a width, so `writeCrossovers` has no per-store check that could catch a laundered
    // one; `gestureW` is proved ONLY by the per-event gate. Adopted there, a foreign width change is
    // invisible to that gate, to `mouseUp`'s gate and to `tick`'s reconcile for the whole rest of
    // the drag -- ADR-0040's width half, silently disarmed.
    //
    // `seedDragOrigins()` is the half this function actually needs: it derives `dragOrigX` FROM the
    // record and writes nothing back to it. Everything below is computed from `dragOrigX`
    // (`bandStartLeftX`/`bandStartRightX`, and the T range from those), so after this change the
    // function reads no live parameter at all -- the window does not narrow, it stops existing.
    //
    // INERT OUTSIDE THE RACE, and provably so rather than by inspection: the gate one step earlier
    // is `soundMovedUnderGesture`, which compares every slot of both rows with `juce::exactlyEqual`
    // in normalised units, so past a PASSING gate `gestureX[k] == freqP[k]->getValue()` bit-for-bit.
    // `captureGestureSound` would have written those same bits back. `convertFrom0to1` is pure
    // arithmetic on them, so `dragOrigX` is identical. ADR-0051's Decision says this in general --
    // "a pass that has already stamped the row derives from the stamp rather than stamping again" --
    // and names ONE exception, the add branch, because `addBandAt` has just changed the count and
    // written the row. This is not that branch, so this is the rule applied, not a new one.
    //
    // WHAT IT DOES NOT DO, said here rather than left to be discovered: it makes the race
    // ADOPTION-free, not WRITE-free. A foreign width landing in the old window is now refused --
    // but at the NEXT event's gate, so the crossover burst of the event already in flight still
    // goes out. That is unchanged in kind from any other foreign write during a drag, and it is
    // what `--band-move-adopt-probe` measures as the post-fix behaviour.
    seedDragOrigins();
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
bool SpectrumImager::moveBand (float mouseX, int n)
{
    // ADR-0046: the EXTENT is the topology the caller proved, not a fourth reading of it.
    const int M = (n >= 0 ? juce::jlimit (1, 4, n) : bandCount()) - 1;
    if (M <= 0) return true;
    const float T = juce::jlimit (bandTmin, bandTmax, mouseX - bandAnchorX);
    float out[3];
    projectFromOrig (out, dragOrigX, M, soloMoveLeft, bandStartLeftX + T, soloMoveRight, bandStartRightX + T);
    return writeCrossovers (out, M);
}
// ADR-0050, the third and last site it escalated. The pins are latched and the members cleared
// before either dispatch, so this function is self-protecting instead of relying on its callers:
// reached twice, the second call closes nothing.
//
// MEASURED, AND THE MEASUREMENT IS NOT THE ONE THIS COMMENT FIRST CLAIMED. Reverting these two
// lines ALONE leaves every check green -- `cancelActiveDrag`'s cheap exit stands in front of the
// only reentrant path, and `mouseUp` clears `soloPressBand`/`soloMovedBand` before calling here --
// so a first draft of this note called it defence in depth with no reachable test. Reverting BOTH
// sites fails State test 83 leg C, while reverting `cancelActiveDrag` alone fails only legs A and
// B. So this clear is not dead weight: it is the layer that still refuses the double close when the
// other one is gone. That is the same "no single layer is measurable, only the ensemble" shape the
// `removeBand` count proof already carries, and the reason a surviving single-line mutation here is
// not evidence of a hole.
void SpectrumImager::endBandMove()
{
    const int l = soloMoveLeft, r = soloMoveRight;
    soloMoveLeft = soloMoveRight = -1;
    if (l >= 0) endGesture (freqP[l]);
    if (r >= 0) endGesture (freqP[r]);
}

void SpectrumImager::resetCrossover (int i)
{
    auto* p = (i >= 0 && i < 3) ? freqP[i] : nullptr;
    if (p == nullptr) return;
    if (i >= bandCount() - 1) return;   // same rule as commitFreqEditor: the handle must still be live
    if (onSweep) onSweep();
    // ADR-0043: the plan is computed AFTER the gesture opens, for the reason spelled out in
    // commitFreqEditor -- `projectGaps` slides the pin by an amount derived from the NEIGHBOURS, and
    // `beginChangeGesture` dispatches to the host before the store. ADR-0042: the reset is then
    // confirmed before anything is moved to make room for it.
    p->beginChangeGesture();
    const int M = bandCount() - 1;
    float xs[3] {}, was[3] {}, owned = 0.0f;
    bool ok = (i < M);
    if (ok)
    {
        for (int k = 0; k < M && k < (int) std::size (freqP); ++k)
        {
            // ADR-0047: the plan and the world it is computed from are ONE reading, not two.
            was[k] = (freqP[k] != nullptr) ? freqP[k]->getValue() : 0.0f;   // the world the plan is computed from
            xs[k]  = freqToX ((freqP[k] != nullptr) ? freqP[k]->convertFrom0to1 (was[k]) : kFreqLo);
        }
        xs[i] = freqToX (p->convertFrom0to1 (p->getDefaultValue()));
        projectGaps (xs, M, i);
        ok = storeOwned (p, juce::jlimit (kFreqLo, kFreqHi, xToFreq (xs[i])), owned);
    }
    p->endChangeGesture();
    if (! ok || ! juce::exactlyEqual (p->getValue(), owned)) return;
    (void) spreadSplits (xs, was, M, i, owned);
}

int SpectrumImager::addBandAt (float hz, int& resultingBands)
{
    const int N = bandCount();
    resultingBands = N;          // nothing added -> the caller's gesture keeps the count it had
    // ADR-0048 CONSIDERED AND REJECTED A REFUSAL HERE, and the reason is the ADR-0044/0045 asymmetry
    // rather than a scope decision. `removeBand` has taken `expectedBands` since ADR-0039 because its
    // input is a BAND INDEX: a count change RETARGETS it onto a different band, so acting under an
    // unvalidated topology performs a different operation. This function's input is a FREQUENCY, and
    // a frequency means the same thing under every topology -- exactly what ADR-0045 ruled for
    // `commitFreqEditor`, which needs no topology stamp for the same reason. Adding the refusal was
    // measured against `--add-target-probe` and closed nothing the clamp argument had not already
    // closed (0 misplacements either way, across 4800 clicks), while turning a click the user made
    // into a no-op whenever a lane moved the count in the same instant. The stores below already
    // prove the count before each of them, which is what protects the TRANSACTION; what this round
    // fixed is the TARGET, one line up in the caller.
    if (N >= 4) return -1;
    const int M = N - 1; // existing crossovers
    float xs[3];
    float fr[3] {};      // the same splits in PARAMETER space, for the ownership compare below
    // ADR-0047: one reading, converted twice -- these were two separate reads of the same split,
    // the first becoming the plan and the second the value every store below proves itself against.
    for (int k = 0; k < M; ++k) { fr[k] = crossover (k); xs[k] = freqToX (fr[k]); }
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
    // ADR-0040. THE BURST IS RE-VALIDATED BEFORE EVERY STORE. A topology edit is not one write, it is
    // a plan computed from a snapshot and then applied as six or more of them -- the solo word, the
    // widths, the splits, and the count last. Every one of those stores is a `setValueNotifyingHost`
    // that reaches the host SYNCHRONOUSLY, inside the call (juce_AudioProcessorParameter.cpp:59-63,
    // :111-121 -> juce_AudioProcessor.cpp:1467, which every format wrapper listens to), so a host
    // that writes mbBands back from inside the FIRST store had the remaining five written over it and
    // the old count restored on top. Measured: `Bands was moved to 2 from inside the burst and the
    // rest of it wrote 3 back`. Validating once at entry cannot see that; validating before each
    // store does, because between the comparison and the store that follows it nothing runs.
    //
    // Each store also checks that its own target still holds the value the plan was computed from, so
    // the plan is never applied to a value it did not account for.
    //
    // ABANDONING MID-BURST LEAVES THE STORES ALREADY ISSUED. That is the right trade and not a
    // half-measure: the alternative -- the completed operation -- puts the OLD count back over the
    // newer one and rewrites the whole layout under it, while abandoning leaves the newer topology
    // standing. A legitimately begun operation truncated by a newer authority is ADR-0036 section
    // 25's rule, not a stale overwrite.
    //
    // ADR-0042 CORRECTION, ITSELF CORRECTED (ADR-0044). This used to say "at most ONE already-issued
    // store behind", which was simply wrong; ADR-0042 replaced it with "an add at N = 3 issues nine
    // stores and a removal at N = 4 issues seven, so the largest residue is eight and six", and that
    // is one too high for the add. Re-counted from this code: the first `ins + 1` width slots are
    // ALWAYS elided (`nw[i] == wd[i]` below the insertion), so an add at N = 3 issues EIGHT value
    // stores and the largest residue is SEVEN. The number is bounded and knowable, not one, and it
    // is counted honestly in the worklog rather than understated here. What DOES reduce it is eliding
    // the stores whose plan equals the snapshot: below the insertion (or above the removal) most of
    // the plan is the world it was computed from, and re-writing those slots bought nothing but
    // reentrancy surface -- plus, for the splits, a pixel round trip that MOVED them. Measured on a
    // two-band add: `split0 200.000015259 -> 199.999847412, delta -1.678e-04` for a split the user
    // never touched, reported to the host as an automation and undo entry.
    // `N` is this transaction's expected topology, read once at entry (ADR-0039). A caller that
    // sees -1 knows nothing it planned was completed as planned.
    // ADR-0041: a refusal is heard. This used to add "the transaction cannot go on to change the
    // band count with the mask still in the old numbering", which was FALSE and is corrected by
    // ADR-0044: this guard tests a change landing BEFORE the mask store, and said nothing about one
    // landing after it. That is what the `soloMask() != nm` checks below and `setBands`'s
    // `expectedMask` now close, measured as `Bands 3 with mask 0x8`.
    //
    // ADR-0044: AND THE MASK IS RE-PROVED ALL THE WAY TO THE COUNT. The paragraph above looks
    // FORWARD -- it proves the store about to happen, against the snapshot its plan came from. The
    // solo word needs the other direction too, because the COUNT is what reinterprets it:
    // `SoloMonitor::process` masks it with ((1 << bands) - 1), so a word written for the old
    // numbering, committed under the new count, silently drops or moves a soloed band. `soloMask()`
    // used to be read exactly twice in this function -- the snapshot and the guard before
    // `setSoloMask` -- and never again, while up to six further synchronous dispatches ran before
    // the count store. Measured on a removal at N = 4 with the probe firing from inside a LATER
    // store: `Bands 3 with mask 0x8`, a word for four bands that three bands then mask to nothing.
    //
    // The widths and the splits deliberately do NOT get this treatment. ADR-0042 measured aborting
    // at a width leaf and found it WORSE -- `Bands 3 mask 0x5 wLo 1.750`, the intended layout with
    // the newer authority's width standing -- because the mask is stored first and is only correct
    // once the count changes. `mbWidthLow` means band 0's width under either topology; `mbSolo`
    // does not mean the same thing under both. That asymmetry is the whole of this decision, and
    // State test 76 legs B and C are the controls that keep it.
    if (bandCount() != N || soloMask() != oldMask) return -1;
    if (! setSoloMask (nm, N, oldMask)) return -1;

    for (int i = 0; i <= N; ++i)
    {
        if (bandCount() != N || ! juce::exactlyEqual (bandWidth (i), wd[i])) return -1;
        if (soloMask() != nm) return -1;                   // ADR-0044
        if (juce::exactlyEqual (nw[i], wd[i])) continue;   // the plan IS the world here
        setParam (widthP[i], nw[i]);
    }
    for (int i = 0; i < N;  ++i)
    {
        // The plan was computed from the M = N - 1 splits that EXIST; slot M is the one this add
        // creates and there is nothing there to own, so only the existing ones are re-proved.
        if (bandCount() != N) return -1;
        // ADR-0041 ruled ownership a PARAMETER question, not a pixel one, and converted the gesture
        // paths; this guard was left in pixels. Half a display pixel is 0.30-0.65 % of the frequency
        // -- 32 Hz at 10 kHz -- so a foreign move that size read as "unchanged" and the burst wrote
        // its own plan over it. `removeBand`'s split guard has compared exactly since ADR-0040;
        // this now matches it. Found by an adversarial pass over the shipped ADR-0042 code.
        // ADR-0047 supplied the other half of what makes it sound: exactness is only worth having if
        // `fr[i]` and the plan `xs[i]` came from the SAME reading. They did not -- the capture called
        // `crossover (k)` twice per slot -- so a write landing between the two left this comparing
        // the post-write world with itself while the plan carried the pre-write one, and passing.
        if (i < M && ! juce::exactlyEqual (crossover (i), fr[i])) return -1;
        // ...and the split it did not move is not written at all. `xToFreq (freqToX (f))` is a
        // 30-iteration bisection over a monotone-spline log axis, not the identity, so storing an
        // unmoved split MOVED it -- measured at -1.678e-04 Hz on a two-band add, into the host's
        // automation lane and the undo stack for a split the user never touched.
        if (soloMask() != nm) return -1;                  // ADR-0044
        if (i < M && juce::exactlyEqual (nx[i], xs[i])) continue;
        setParam (freqP[i],  juce::jlimit (kFreqLo, kFreqHi, xToFreq (nx[i])));
    }
    // ADR-0044: the mask is proved INSIDE the count store's own gesture bracket, not out here --
    // `setBands`'s `beginChangeGesture` dispatches before its guard, so a host answering the
    // gesture open is invisible to any check the caller makes. The `bandCount()` test that used to
    // sit here was already dominated by `setBands`'s own, with nothing dispatching in between.
    if (! setBands (N + 1, N, nm)) return -1;
    resultingBands = N + 1;      // from THIS read of the count, not a second one (ADR-0039)
    return ins;
}

void SpectrumImager::removeBand (int b, int expectedBands)
{
    const int N = bandCount();
    if (N <= 1) return;
    // REFUSE, NEVER CLAMP (ADR-0038). This used to be `b = juce::jlimit (0, N - 1, b)`, and
    // that clamp was the mechanism that turned a stale index into a WRONG TARGET: a caller
    // holding a band number from before a host write of mbBands had its request silently
    // retargeted onto whichever live band the number now landed on.
    //
    // A RANGE CHECK IS NOT ENOUGH (ADR-0039, review finding 1). Refusing an out-of-range index
    // still let a stale one through whenever it happened to LAND inside the new range: two
    // bands, drag split 0 outside, the count rises to four inside mouseUp -- `endGesture` on the
    // dragged split runs first and any listener on that parameter can move mbBands there -- and
    // `removeBand (1)` was in range, so it merged bands out of a four-band layout the user had
    // never seen and left Bands at 3. Measured as `Bands 4 -> 3` by State test 69 leg (c).
    //
    // So the caller names the topology it validated its index against and this reads the live
    // count ONCE: the check and the operation share a single read, which is the only shape that
    // holds when the count can move between two of them. `bandCount()` is a live read of a
    // parameter a host can write at any instant; a check in the CALLER can never close that,
    // because the operation reads again afterwards.
    //
    // RE-AUDITED 2026-09-09 (release-time removal review round) AND MEASURED, because the review
    // asked the question this paragraph answers and a comment is not evidence. The mechanism is
    // real and REENTRANT, which makes it the most reachable window this series has examined:
    // `mouseUp` proves the count, latches `pressBands`, then calls `endGesture` on the dragged
    // split -- which DISPATCHES, so a host recording automation can move mbBands from inside it --
    // and only then reaches this function.
    //
    // WHAT IS LOAD-BEARING IS THE ARGUMENT, NOT ANY ONE COMPARISON. Replacing this line, the
    // `bandCount() != expectedBands` before the solo store and `setSoloMask`'s own precondition --
    // all three at once -- leaves the whole suite green, because the width loop, the split loop and
    // `setBands` still each refuse. But changing the CALL SITE to pass a live `bandCount()` instead
    // of `pressBands` kills State test 69 leg C twice over, with the diagnostic that round measured:
    // `Bands 4 -> 3: the release read a count the check never saw`. So the five comparisons are
    // individually redundant BY DESIGN and the contract they enforce is covered; do not read a
    // survived single-line mutation here as a coverage hole. That is exactly the misreading this
    // round made first and corrected by peeling further.
    //
    // WHICH CALL SITE, EXACTLY -- because the answer is not "all of them", and the round's audit had
    // to measure per site to get it right. Replacing `pressBands` with a live read at the OUTWARD-DRAG
    // site alone killed those two checks; at the delete-x site alone, or at the two solo sites alone,
    // it killed NOTHING. That is not one guard and three holes: the outward-drag site is the only tail
    // store with a synchronous DISPATCH in front of it (`endGesture` on the dragged split, which is
    // exactly what leg C's listener fires inside), so it is the only one whose window a
    // single-threaded harness can enter. The others' windows are cross-thread-only.
    //
    // ...AND THAT MUTATION NO LONGER FIRES, WHICH IS A STRENGTHENING AND NOT A REGRESSION. Re-measured
    // 2026-09-09: passing a live `bandCount()` at the outward-drag site now kills NOTHING on its own,
    // because ADR-0050 put `! gestureIsStale()` in front of the call and `topologyMovedUnderGesture`
    // compares the live count against the STILL-LATCHED `gestureBands` -- so leg C's Bands move is
    // refused one level earlier, whatever this argument says. Remove BOTH and leg C fails twice and
    // leg G once, which is the measurement that now stands. `pressBands` is therefore defence in
    // depth here rather than the sole guard; it is kept because it is the only thing covering the
    // window if a future change moves the staleness gate again, and because the DELETE-X and SOLO
    // call sites have no gate in front of them at all.
    if (N != expectedBands) return;
    if (b < 0 || b >= N) return;
    const int dropX = (b == 0) ? 0 : (b - 1); // delete the split on this band's left (#12)

    // ADR-0047, AT THE TRANSACTION BOUNDARY. The count arrives proved -- `expectedBands` makes the
    // caller's check and this function's own read ONE reading, which is why a stale INDEX cannot
    // land here. The VALUES had no such treatment: the caller proves them (`gestureIsStale()` reads
    // the press's `gestureX`/`gestureW`), and then this function read them AGAIN, a few instructions
    // later, and planned from that second reading. Two readings where the rule says one -- and the
    // second is the one every later guard compares against, so a same-count install landing between
    // them is baked into the snapshot and no guard in the transaction can see it. The delete x then
    // merges a band whose boundaries the press never saw, reported to the host as a real edit.
    //
    // Cross-thread only: from the caller's gate to here every statement is a pure read, so nothing
    // on this thread can dispatch into the gap -- only the audio thread's automation or the host
    // state thread.
    //
    // ONE READING, and the plan AND the proof are derived from it. The row is taken in the
    // parameter's own normalised units -- which is what `gestureX`/`gestureW` hold and what
    // `ownsSplit (k, norm)` / `ownsWidth (b, norm)` compare, the overloads ADR-0047 added for
    // exactly "prove the reading the caller already has" -- and the Hz the plan needs is derived
    // from it by `convertFrom0to1`, pure arithmetic on the value that read returned.
    float fr[3], wd[4], nfr[3], nwd[4];
    for (int k = 0; k < 3; ++k)
    {
        nfr[k] = (freqP[k] != nullptr) ? freqP[k]->getValue() : 0.0f;
        fr[k]  = (freqP[k] != nullptr) ? freqP[k]->convertFrom0to1 (nfr[k]) : kFreqLo;
    }
    for (int k = 0; k < 4; ++k)
    {
        nwd[k] = (widthP[k] != nullptr) ? widthP[k]->getValue() : 0.0f;
        wd[k]  = (widthP[k] != nullptr) ? widthP[k]->convertFrom0to1 (nwd[k]) : 1.0f;
    }
    // ...and the press must still own it. Both predicates answer `true` when no record is in force
    // (`gestureBands < 0`), so a caller without a gesture is unaffected -- and with nothing racing
    // the caller's gate has just proved these same values, so this refuses nothing it did not
    // already refuse. ADR-0039's direction: a removal is REFUSED, never applied to a layout it
    // cannot vouch for.
    for (int k = 0; k < 3; ++k) if (! ownsSplit (k, nfr[k])) return;
    for (int w = 0; w < 4; ++w) if (! ownsWidth (w, nwd[w])) return;   // `w`, not `b`: `b` is the parameter

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
    // ADR-0040. THE BURST IS RE-VALIDATED BEFORE EVERY STORE. A topology edit is not one write, it is
    // a plan computed from a snapshot and then applied as six or more of them -- the solo word, the
    // widths, the splits, and the count last. Every one of those stores is a `setValueNotifyingHost`
    // that reaches the host SYNCHRONOUSLY, inside the call (juce_AudioProcessorParameter.cpp:59-63,
    // :111-121 -> juce_AudioProcessor.cpp:1467, which every format wrapper listens to), so a host
    // that writes mbBands back from inside the FIRST store had the remaining five written over it and
    // the old count restored on top. Measured: `Bands was moved to 2 from inside the burst and the
    // rest of it wrote 3 back`. Validating once at entry cannot see that; validating before each
    // store does, because between the comparison and the store that follows it nothing runs.
    //
    // Each store also checks that its own target still holds the value the plan was computed from, so
    // the plan is never applied to a value it did not account for.
    //
    // ABANDONING MID-BURST LEAVES THE STORES ALREADY ISSUED. That is the right trade and not a
    // half-measure: the alternative -- the completed operation -- puts the OLD count back over the
    // newer one and rewrites the whole layout under it, while abandoning leaves the newer topology
    // standing. A legitimately begun operation truncated by a newer authority is ADR-0036 section
    // 25's rule, not a stale overwrite.
    //
    // ADR-0042 CORRECTION, ITSELF CORRECTED (ADR-0044). This used to say "at most ONE already-issued
    // store behind", which was simply wrong; ADR-0042 replaced it with "an add at N = 3 issues nine
    // stores and a removal at N = 4 issues seven, so the largest residue is eight and six", and that
    // is one too high for the add. Re-counted from this code: the first `ins + 1` width slots are
    // ALWAYS elided (`nw[i] == wd[i]` below the insertion), so an add at N = 3 issues EIGHT value
    // stores and the largest residue is SEVEN. The number is bounded and knowable, not one, and it
    // is counted honestly in the worklog rather than understated here. What DOES reduce it is eliding
    // the stores whose plan equals the snapshot: below the insertion (or above the removal) most of
    // the plan is the world it was computed from, and re-writing those slots bought nothing but
    // reentrancy surface -- plus, for the splits, a pixel round trip that MOVED them. Measured on a
    // two-band add: `split0 200.000015259 -> 199.999847412, delta -1.678e-04` for a split the user
    // never touched, reported to the host as an automation and undo entry.
    // ADR-0041: a refusal is heard -- see addBandAt.
    // ADR-0044: the mask is re-proved all the way to the count -- see addBandAt for the reasoning
    // and the measurement, and for why the widths and splits deliberately keep ADR-0042's
    // disposition instead.
    if (bandCount() != expectedBands || soloMask() != oldMask) return;
    if (! setSoloMask (nm, expectedBands, oldMask)) return;

    // ADR-0049. A TRANSACTION PROVES BOTH ENDS OF EVERY VALUE IT MOVES. A removal does not write
    // values; it MOVES them -- slot k is written with what slot k + 1 was holding when the plan was
    // computed (`nw[j] = wd[j + 1]` and `nf[j] = fr[j + 1]` above, for every j at or past the
    // removal). ADR-0040 proved the DESTINATION before each store and ADR-0042 gave the rule -- a
    // plan is applied only to the world it was computed from -- but only half of each store's world
    // was being proved. The SOURCE was proved late or not at all:
    //
    //   * a mid source (slot k + 1, k + 1 <= N - 2) is proved by the NEXT iteration's destination
    //     check -- one store AFTER the store that already consumed it, with that store's dispatch in
    //     between. The abandon happens, but the stale value has landed;
    //   * the TOP source -- slot N - 1 for the widths, slot N - 2 for the splits -- is never a
    //     destination in either loop, so it is proved NOWHERE.
    //
    // THE CHARGE IS MISATTRIBUTION, NOT LOSS, and this round's audit corrected the first draft of
    // this paragraph for saying otherwise. Nothing destroys `widthP[N - 1]` or `freqP[N - 2]`: the
    // count store puts them ABOVE the live topology, where `MultibandWidth` and `SoloMonitor` simply
    // do not read them (`MultibandWidth.h:53-56`, `MultibandWidth.cpp:140`), so the host's value is
    // PARKED -- inert while hidden and exact if the count comes back, the same disposition ADR-0039
    // records for the parked solo bit. What is wrong is what the SURVIVING band gets: the pre-write
    // snapshot, moved down under a host edit that had already replaced it. Saying "discarded" makes
    // the defect sound worse than the evidence supports, which is the failure mode this file's
    // measurements exist to avoid.
    //
    // So the source is proved HERE, in the same window as the store that consumes it, with nothing
    // between the comparison and `setParam`. It is asked only where a store actually happens: below
    // the removal the plan IS the world (`nw[k] == wd[k]`), the store is elided, and there is no
    // source to speak of -- which is also why `k + 1` is the right index whenever this line is
    // reached at all.
    //
    // THE SIBLING IS DELIBERATELY LEFT ALONE, and NOT because it is structurally safe -- the audit
    // refuted that reading of it. `addBandAt` shifts UP and its loop runs up, so iteration i's
    // destination guard proves slot i at the last instant slot i still holds the value iteration
    // i + 1 will take from it; the source is covered, at the only moment it can be. What is NOT
    // covered there is a host write landing on slot i - 1 AFTER the transaction's own store to it:
    // `nw[i] = (i <= ins) ? wd[i] : wd[i - 1]` re-attributes that slot to a different band, so the
    // newer value stands on a band the user did not aim it at. The symmetric guard cannot be added:
    // `bandWidth (i - 1) == wd[i - 1]` fails on the transaction's OWN store and would abandon every
    // non-elided add. So it stays ADR-0042's measured disposition -- an accepted residual, named
    // here rather than implied by silence.
    //
    // THIS DOES NOT REOPEN ADR-0044's ASYMMETRY, and State test 76 legs B and C are what say so.
    // Those legs are controls: a width or a split replaced mid-burst must NOT abandon, and the newer
    // value must stand. They write a slot the transaction has ALREADY written -- the prefix -- and
    // re-proving the prefix is exactly what ADR-0044's audit removed as measured-worse. A source is
    // the opposite end: a slot the plan has not touched yet and is about to READ. Proving it is the
    // destination rule, not the prefix rule, and both legs still pass unchanged.
    for (int k = 0; k < N - 1; ++k)
    {
        if (bandCount() != expectedBands || ! juce::exactlyEqual (bandWidth (k), wd[k])) return;
        if (soloMask() != nm) return;                      // ADR-0044
        if (juce::exactlyEqual (nw[k], wd[k])) continue;   // the plan IS the world here
        if (! juce::exactlyEqual (bandWidth (k + 1), wd[k + 1])) return;   // ADR-0049: and the source
        setParam (widthP[k], nw[k]);
    }
    for (int k = 0; k < N - 2; ++k)
    {
        if (bandCount() != expectedBands || ! juce::exactlyEqual (crossover (k), fr[k])) return;
        if (soloMask() != nm) return;                      // ADR-0044
        if (juce::exactlyEqual (nf[k], fr[k])) continue;
        if (! juce::exactlyEqual (crossover (k + 1), fr[k + 1])) return;   // ADR-0049: and the source
        setParam (freqP[k],  nf[k]);
    }
    (void) setBands (N - 1, expectedBands, nm); // last store: nothing follows it to abandon
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
        freqEditor->onTextChange = [this] { editTextEdited = true; };
        freqEditor->onReturnKey  = [this] { commitFreqEditor(); };
        freqEditor->onEscapeKey  = [this] { closeFreqEditor(); };
        freqEditor->onFocusLost  = [this] { commitFreqEditor(); };
        addAndMakeVisible (*freqEditor);
    }
    auto chip = numberChip (i).expanded (6.0f, 4.0f);
    freqEditor->setBounds (chip.toNearestInt());
    // `false` is the argument this overload takes -- juce::TextEditor::setText is
    // (const String&, bool sendTextChangeMessage), NOT the (String, NotificationType) of
    // juce::Label. `juce::dontSendNotification` compiled here only because it is 0.
    freqEditor->setText (freqText (crossover (i)), false);
    editTextEdited = false;              // seeded, not typed: no change message, so onTextChange never fires
    editOpenText   = freqEditor->getText();
    freqEditor->setVisible (true);
    freqEditor->grabKeyboardFocus();
    freqEditor->selectAll();
}
void SpectrumImager::commitFreqEditor()
{
    if (editingHandle < 0) return;
    const int i = editingHandle;
    // ADR-0039's rule, at this commit point too: the handle names a split by POSITION, and
    // `openFreqEditor` proved it live when the editor OPENED. Nothing closes the editor when the
    // band count moves -- not the 24 Hz reconcile, not `cancelActiveDrag` -- so a host lane that
    // drops Bands while the user is typing leaves this committing a split the topology no longer
    // uses, and spreading the live ones around a pin that is not there. REFUSE, never clamp.
    // ADR-0045 RULED THIS SUFFICIENT, having been asked whether the editor needs a topology stamp
    // like the drag and the wheel do. It does not, and the reason is the ADR-0044 asymmetry: a
    // SPLIT parameter means the same thing under every topology. `mbFreqLow` is split 0 whether
    // there are two bands or four, so an index that survives names the same parameter the user
    // opened the chip on -- unlike `removeBand`'s band index, which ADR-0039 had to refuse because
    // a surviving index there RETARGETED the operation onto a different band. What must be proved
    // is only that the split still EXISTS, which is this line, and that the plan is computed from
    // the live layout, which ADR-0043 moved inside the gesture bracket below (`M` is re-read there
    // and `ok = (i < M)` proves the handle a second time, after the open has dispatched).
    if (i >= bandCount() - 1) { closeFreqEditor(); return; }
    // ADR-0043. A COMMIT CARRIES INTENT, OR IT CARRIES NOTHING. The box is SEEDED from the live
    // split when the chip opens, and this function is reached by Return, by FOCUS LOSS and by any
    // mouseDown in the component -- so opening the chip and clicking away used to re-commit the
    // value the split had at open time. Measured: `5000.0 Hz was installed and 200.0 Hz was written
    // over it`, and, with nothing moving at all, one automation touch and one undo step for an edit
    // that never happened. A typed value is a different matter entirely and still wins: the user's
    // own action is the newer authority (ADR-0036 section 25).
    if (! editTextEdited && freqEditor->getText() == editOpenText) { closeFreqEditor(); return; }
    auto* p = freqP[i];
    if (p == nullptr) { closeFreqEditor(); return; }
    const float want = juce::jlimit (kFreqLo, kFreqHi, parseFreq (freqEditor->getText()));

    // ADR-0043. THE PLAN IS COMPUTED AFTER THE GESTURE OPENS. `projectGaps` pins the edited split
    // and then slides the WHOLE cluster -- the pin included -- back inside the plot edges by an
    // amount derived from the NEIGHBOURS, and `beginChangeGesture` dispatches to the host before the
    // store. Computed before the open, the stored value could therefore be a projection of a world
    // that had already moved: measured `8440.1 / 3000.0 / 19500.0`, the first split above the
    // second. Computed here, `projectGaps` is pure and nothing dispatches between the snapshot and
    // the store, so the projection is a function of the world one statement earlier.
    p->beginChangeGesture();
    const int M = bandCount() - 1;
    float xs[3] {}, was[3] {}, owned = 0.0f;
    bool ok = (i < M);
    if (ok)
    {
        for (int k = 0; k < M && k < (int) std::size (freqP); ++k)
        {
            // ADR-0047: one reading answers both -- the plan below and the ownership baseline
            // `spreadSplits` proves each neighbour against.
            was[k] = (freqP[k] != nullptr) ? freqP[k]->getValue() : 0.0f;
            xs[k]  = freqToX ((freqP[k] != nullptr) ? freqP[k]->convertFrom0to1 (was[k]) : kFreqLo);
        }
        xs[i] = freqToX (want);
        projectGaps (xs, M, i);
        ok = storeOwned (p, juce::jlimit (kFreqLo, kFreqHi, xToFreq (xs[i])), owned);
    }
    p->endChangeGesture();
    if (ok && juce::exactlyEqual (p->getValue(), owned))
        (void) spreadSplits (xs, was, M, i, owned);
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

    // ADR-0043. THE THIRD CONSUMER ASKS THE SAME QUESTION THE OTHER TWO DO. `mouseDrag` and
    // `mouseUp` both refuse to act on a gesture whose world has moved; this promotion acted on
    // `soloPressBand` -- a band index by POSITION, latched at mouseDown -- on a purely TIME-BASED
    // condition, so a host lane that changed the topology during the hold auditioned a band the
    // user never pressed: press band 3 of four, Bands drops to two, and `SoloMonitor` masks
    // `0x8 & 0x3` to nothing, so the user holds a solo button and hears no solo at all. Fires once
    // rather than every frame, because `cancelActiveDrag` clears `gestureBands`; and it returns
    // before any repaint when no identifier is latched, so an Alt-click reset costs one int store.
    if (gestureIsStale()) cancelActiveDrag();

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
    // Snapshot everything this function may change that paint() reads, so the S2 repaint gate
    // (end of tick()) can see a hover-only move.
    //
    // The gate repaints when the spectrum data moved, an eased alpha moved, or a drawn split /
    // width position moved -- on the stated assumption that "mouse-driven fields [have] handlers
    // [that] already repaint explicitly". This handler was the one that did not, and `addX` -- the
    // X of the "click to add a split" preview line, drawn in paint() -- is none of the three things
    // the gate watches. So while the cursor moved WITHIN one band's add zone the line froze: every
    // alpha had already arrived at its target (addA == 1 throughout), and on a settled view (silence,
    // decays finished) nothing else moved either, so the gate stayed shut and the last painted X
    // stayed on screen. Moving onto some other hotspot changed an alpha (e.g. soloA), the gate
    // opened, and the line jumped to the cursor -- which is exactly how the bug was reported.
    //
    // `frameDirty` rather than repaint(): the imager's FrameClock runs whenever it is visible
    // (visibilityChanged() starts/stops it), and a hidden component receives no mouse events, so the
    // flag is always consumed by the next vblank tick. That keeps painting paced at one frame per
    // vblank instead of one per mouse event, which is what the gate is for.
    const int   wasHandle = hoverHandle, wasWidth = hoverWidth, wasAdd = hoverAdd;
    const int   wasDelete = hoverDelete, wasDeleteExact = hoverDeleteExact, wasSolo = hoverSolo;
    const float wasAddX   = addX;

    // ADR-0048: ONE READING FOR THE WHOLE PASS. `N` was already read here and used for the delete
    // target, while `handleNearX` and `bandAtX` re-read for themselves -- so a hover could offer a
    // delete on one layout and an add on another. Threading `N` into the add target ALONE would have
    // recreated that mismatch one line up rather than closing it, which is the correction this round
    // took from its own audit: the derivation and every boundary derived from it answer under the
    // same reading, exactly as `mouseDown` has since ADR-0046. Display only, so nothing here fails
    // open -- but a cursor that offers one band's affordance while naming another's is the same
    // defect one severity band down, and it costs two reads to remove rather than to reason about.
    const int N = bandCount();
    // ADR-0051: and the SPLIT ROW is read once here too, for the same reason the count is. Three
    // calls below each used to read it for themselves, so the hover could measure the handle
    // distance against one row and clamp the add target against another.
    float fx[3]; captureSplits (fx);
    hoverHandle = hoverWidth = hoverAdd = hoverDelete = hoverDeleteExact = hoverSolo = -1;

    const int h = handleNearX (p.x, N, fx);
    const int b = bandAtX (p.x, N, fx);
    // ADR-0046/0051: ...and these two, which this pass hoisted its reading for and then did not hand
    // it to. Display only, so it cannot fail open -- but the cursor could offer a solo affordance
    // derived under one layout while the delete target beside it named another.
    const int sh = soloHit (p, N, fx);

    if (sh >= 0)                   { hoverSolo = sh; setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    else if (h >= 0)               { hoverHandle = h; setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); }
    else if (nearWidthLine (p, b)) { hoverWidth = b; setMouseCursor (juce::MouseCursor::UpDownResizeCursor); }
    else
    {
        float ax;
        // ADR-0048: this pass already read the count at the top; the add target answers under it too.
        if (bandAddTarget (b, p.x, ax, N, fx)) { hoverAdd = b; addX = ax; setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        else                              setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    // The delete x shows DIMLY whenever the cursor is over the band or its split line, and
    // BRIGHT (with a pointing hand) once it is directly over the x. When a split LINE is
    // hovered, show the x to its RIGHT (the band the split opens), not the left band's (#1).
    if (N > 1)
    {
        hoverDelete = (h >= 0) ? juce::jmin (h + 1, N - 1) : b;
        hoverDeleteExact = deleteHit (p, N, fx);
        if (hoverDeleteExact >= 0) setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    // `addX` only matters while hoverAdd >= 0, and it is written only on that branch, so a move
    // with no add target leaves it untouched and cannot mark the frame dirty spuriously.
    if (hoverHandle != wasHandle || hoverWidth != wasWidth || hoverAdd != wasAdd
        || hoverDelete != wasDelete || hoverDeleteExact != wasDeleteExact || hoverSolo != wasSolo
        || ! juce::exactlyEqual (addX, wasAddX))
        frameDirty = true;

    setContextTooltip();
}

void SpectrumImager::mouseMove (const juce::MouseEvent& e)
{
    if ((scrollHandle >= 0 || scrollBand >= 0) && e.position.getDistanceFrom (scrollAnchor) > 3.0f)
        scrollHandle = scrollBand = scrollBands = -1;
    updateHover (e.position);
}
void SpectrumImager::mouseExit (const juce::MouseEvent&)
{
    hoverHandle = hoverWidth = hoverAdd = hoverDelete = hoverDeleteExact = hoverSolo = -1;
    scrollHandle = scrollBand = scrollBands = -1;
}
void SpectrumImager::mouseDown (const juce::MouseEvent& e)
{
    if (editingHandle >= 0) commitFreqEditor();
    // ADR-0038: the topology this gesture is about to be defined against, and (ADR-0039) the
    // sound with it. Taken once, at the top, so every branch below -- solo press, delete press,
    // handle drag, width drag -- is covered by the one snapshot rather than each latching its
    // own. The ADD branch is the exception and re-takes it: see there.
    // ADR-0046: and it is the topology every derivation below ANSWERS UNDER, not merely the one
    // they are compared against afterwards. `handleNearX` and `bandAtX` used to re-read the count
    // for themselves; with three threads writing mbBands that is a second read, and a second read
    // can differ. Here the difference was always safe -- the stamp is the OLDER of the two, so a
    // disagreement makes `gestureIsStale()` refuse -- but a refusal is a user edit dropped for no
    // reason the user can see.
    //
    // `soloHit` AND `deleteHit` NOW TAKE IT TOO, and the sentence that used to stand here --
    // "six signatures for a window that already fails safe" -- was **false for `soloHit`**,
    // corrected 2026-09-10 with the ADR amended to match. Its premise was that `gestureBands` is
    // the OLDER reading so a disagreement makes `gestureIsStale()` refuse. But `gestureIsStale()`
    // compares the live world against the STAMP; the reading `soloHit` actually used is recorded
    // nowhere, so an ABA return erases the disagreement before any guard runs, and `soloPressBand`
    // -- consumed by `tick`'s audition, `mouseDrag`'s band move and `mouseUp`'s toggle, none of
    // which re-derives it -- then names a band the press's own layout has not got. `setSoloMask`
    // cannot catch it either: a bit above the live count is a legitimate PARKED bit by design.
    // MEASURED at 491 aliased presses in 1200 against a lane alternating mbBands 2/4, 0 after
    // (`--solo-alias-probe`). ADR-0046's own `beginBandMove` amendment made this ABA argument
    // already, one screen up, and reached the opposite conclusion here.
    //
    // `deleteHit` was NOT open, for a reason that sentence never gave: `deleteBox` depends on
    // `bandLeftX` alone, which reads no count, so a count rise can only APPEND candidates above the
    // first match; `removeBand` rejects `b >= expectedBands` outright; and `mouseUp` re-runs the
    // hit-test and demands it name the same band. It is threaded because one reading per pass is
    // the rule and it REMOVES reads -- not as a defect fixed, and it is not claimed as one.
    gestureBands = bandCount();
    captureGestureSound();
    // ADR-0051: the SPLIT ROW this press answers under, derived from the stamp that was just taken
    // rather than read again. `convertFrom0to1` is pure arithmetic on the value `captureGestureSound`
    // has already read, so this is the SAME measurement rather than a second one -- the point of
    // ADR-0047's instrument, and it costs no parameter reads at all. Every derivation below --
    // `handleNearX`, `bandAtX`, and the add target's boundaries -- answers under this row.
    float pressF[3];
    for (int k = 0; k < 3; ++k)
        pressF[k] = (freqP[k] != nullptr) ? freqP[k]->convertFrom0to1 (gestureX[k]) : kFreqLo;
    const auto p = e.position;
    const bool alt = e.mods.isAltDown();

    // ADR-0046 COMPLETED HERE. This was the one derivation in this handler still reading the count
    // and the row for itself, three lines after the press stamped both. `soloPressBand` is latched
    // from it and never re-derived; see `soloHit` for why that is the half that was open.
    if (const int sh = soloHit (p, gestureBands, pressF); sh >= 0)
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
    if (! alt) if (const int dB = deleteHit (p, gestureBands, pressF); dB >= 0) { pressDeleteBand = dB; hoverAdd = -1; addA = 0.0f; repaint(); return; }

    const int h = handleNearX (p.x, gestureBands, pressF);
    if (alt)
    {
        if (h >= 0) resetCrossover (h);
        // ADR-0045, tightened: the count is read BEFORE the index is derived from it, so the
        // topology `resetParam` proves is the one `bandAtX` answered under. Reading it after would
        // leave a window a few instructions wide in which an AUDIO-THREAD automation write moves
        // the count between the two, making the guard agree with a live count while `b` was
        // derived under the old one -- the reentrancy fix closes the dispatch window, not that one.
        // Ordering the two reads costs nothing and closes it in the safe direction (a refusal).
        // ADR-0046: ordering NARROWED that window; passing `n` into `bandAtX` CLOSES it. Ordered
        // but re-reading, the derivation could still answer under a count newer than the one being
        // proved -- harmless here (the guard then refuses) but a refusal is still a lost edit, and
        // the same shape one line down in `mouseWheelMove` was not harmless at all. One reading,
        // used by the derivation and by the proof, has neither failure.
        else { const int n = gestureBands; const int b = bandAtX (p.x, n, pressF);
               if (b >= 0 && b < n && nearWidthLine (p, b, gestureW)) resetParam (widthP[b], n); }
        return;
    }
    if (h >= 0)
    {
        dragHandle = h; dragBand = -1; dragRemovePending = false;
        handlePressMs = juce::Time::getMillisecondCounter(); handlePressX = p.x; handleHoldActive = false;
        // ADR-0051: the press stamped at the top of this handler; seed the origins FROM that stamp
        // instead of taking a second one. `h` and this anchor are now one reading.
        seedDragOrigins();
        // ADR-0047: the anchor comes from the capture, not from a third read of the same parameter.
        dragGrabDX = p.x - dragOrigX[h]; // keep the line under the cursor with this offset (#10)
        beginGesture (freqP[h]); repaint(); return;
    }

    const int b = bandAtX (p.x, gestureBands, pressF);
    // ADR-0051: the last derivation in this handler that read a sound parameter for itself. Bounded
    // in consequence -- a wrong answer starts (or fails to start) a width drag on the band the
    // proved row put under the cursor, and every store it then makes is refused by `ownsWidth`
    // against this same record -- so this half is rule-completion, NOT a defect claimed as fixed.
    if (nearWidthLine (p, b, gestureW))
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
    // ADR-0048: the target's edges answer under the topology `b` was derived in. `addedBands` used to
    // be seeded from a THIRD reading of mbBands, which `addBandAt` overwrites with its own before the
    // caller can use it -- a read that was dead on arrival and read like a latch.
    if (bandAddTarget (b, p.x, ax, gestureBands, pressF))
    {
        int addedBands = gestureBands;
        const int idx = addBandAt (xToFreq (ax), addedBands);
        if (idx >= 0)
        {
            // ADR-0039, review finding 2. THE PRESS'S OWN ADD IS PART OF THE GESTURE. `addBandAt`
            // has just raised Bands and `dragHandle` is latched HERE, against the topology it
            // established -- so the snapshot belongs here too. Taken at the top of the handler it
            // named the count from BEFORE the add, and the first mouseDrag compared 3 against 2,
            // called the gesture void and cancelled it: click-to-add-and-drag stopped following
            // the cursor the moment the split appeared. `captureDragOrigins` below re-seeds the
            // sound half the same way. Nothing here asks WHO moved the count: a change the press
            // performs is synchronous on this thread and complete before the identifiers exist,
            // so taking the snapshot with the identifiers separates the two classes by
            // construction. State test 69 legs (a) and (b) hold both ends of that line.
            gestureBands = addedBands;
            dragHandle = idx; dragBand = -1; dragRemovePending = false;
            handlePressMs = juce::Time::getMillisecondCounter(); handlePressX = p.x; handleHoldActive = false;
            captureDragOrigins();
            dragGrabDX = p.x - dragOrigX[idx]; // ADR-0047: from the capture, not a third read
            beginGesture (freqP[idx]);
        }
        hoverAdd = -1; addA = 0.0f; // snap away the preview so nothing lingers (#5)
        repaint();
    }
}
void SpectrumImager::mouseDrag (const juce::MouseEvent& e)
{
    // ADR-0038. A gesture is defined against the topology it began in; once that has moved
    // the gesture is VOID, and the one safe thing to do with it is what a release lost
    // outside the window already does -- close the open parameter gestures, clear the
    // flags, fire no on-release action. Checked HERE, at the entry of the handler that
    // acts, rather than inside each consumer: this is the point where the identifiers stop
    // being trustworthy as a SET.
    if (gestureIsStale()) { cancelActiveDrag(); return; }
    if (soloPressBand >= 0)
    {
        if (soloMovedBand || std::abs (e.position.x - soloDownX) > 4.0f)
        {
            if (! soloMovedBand)  { soloMovedBand = true; beginBandMove (soloPressBand, gestureBands); }
            if (! soloHoldActive) { soloHoldActive = true; if (onSoloPreview) onSoloPreview (1 << soloPressBand); }
            // ADR-0040: the move abandons its burst the moment a split stops being ours, and a
            // gesture that has lost ownership is void -- the same answer the entry guard gives.
            if (! moveBand ((float) e.position.x, gestureBands)) { cancelActiveDrag(); return; }
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
        if (! out && ! dragCrossoverTo (dragHandle, (float) e.position.x - dragGrabDX, gestureBands))
        { cancelActiveDrag(); return; }
    }
    else if (dragBand >= 0)
    {
        // Relative Width drag past a 3 px threshold (mirrors the crossover's handleHoldActive
        // click-vs-drag gate). Below the threshold nothing moves, so a click or hand jitter
        // leaves Width untouched. On crossing it we anchor dragGrabDY to the CURRENT line, so
        // the value starts exactly where it was (no jump to the absolute cursor) and then
        // follows the mouse delta -- the line stays attached to the grabbed point.
        // ADR-0040. OWNERSHIP FIRST, AND IT COVERS THE ANCHOR AS WELL AS THE STORE. The old code was
        // inconsistent purely by threshold timing: an outside width write landing AFTER the 3 px
        // engage was overwritten (the value below is computed from `dragGrabDY` and the cursor, and
        // the live width is never read again), while one landing BEFORE it was silently adopted as
        // the anchor and re-emitted inside the user's own change gesture. Both are the gesture
        // writing state it does not own; both now void it, which is the answer ADR-0038 gives for
        // the count and ADR-0039 for the splits.
        // ADR-0047: ONE READING PROVES AND ANCHORS. The proof below and the anchor two lines down
        // were two reads of the same width, so a write landing between them was refused by neither
        // and then adopted as the anchor -- the very failure the paragraph above says ADR-0040
        // closed, surviving in the cross-thread half of the window.
        const float wNorm = (dragBand >= 0 && dragBand < (int) std::size (gestureW)
                             && widthP[dragBand] != nullptr) ? widthP[dragBand]->getValue() : 0.0f;
        if (! ownsWidth (dragBand, wNorm)) { cancelActiveDrag(); return; }
        if (! widthHoldActive && std::abs ((float) e.position.y - widthPressY) > 3.0f)
        {
            widthHoldActive = true;
            const float wPlain = (dragBand >= 0 && dragBand < (int) std::size (gestureW)
                                  && widthP[dragBand] != nullptr)
                               ? widthP[dragBand]->convertFrom0to1 (wNorm) : 1.0f;
            dragGrabDY = (float) e.position.y - widthToY (wPlain);
        }
        if (widthHoldActive)
        {
            if (! storeOwned (widthP[dragBand], yToWidth ((float) e.position.y - dragGrabDY),
                              gestureW[dragBand]))
            { cancelActiveDrag(); return; }
        }
    }
    repaint();
}
void SpectrumImager::mouseUp (const juce::MouseEvent& e)
{
    // ADR-0038, and it matters most here: mouseUp is where the ON-RELEASE ACTIONS live --
    // remove a band, toggle a solo bit, commit a band move. A gesture whose topology moved
    // must fire none of them, exactly as a release lost outside the window fires none.
    if (gestureIsStale()) { cancelActiveDrag(); updateHover (e.position); return; }
    // The press is over on EVERY path out of this handler, so the snapshot is dropped on each of
    // them. It is KEPT in a local first: the two removal paths below still need to name the
    // topology this press was made in, and once the member is cleared it no longer says what that
    // was.
    //
    // ADR-0050. THE CLEAR USED TO HAPPEN HERE, ONE LINE BELOW, FOR ALL THREE EXITS AT ONCE -- and
    // that was one dispatch too early. `gestureIsStale()` answers `false` the instant
    // `gestureBands` is negative (SpectrumImager.h:416 -> `topologyMovedUnderGesture` and
    // `soundMovedUnderGesture`, both of which return early on `gestureBands < 0`), so clearing it
    // here disarmed every ownership question for the rest of the handler -- including
    // `ownsSplit`/`ownsWidth`, which are the only things that see a SAME-COUNT sound change. The
    // outward-drag branch then calls `endGesture` on the dragged split, which DISPATCHES, and only
    // afterwards decides to remove a band. A host recording automation, a control surface echo or
    // the sound half of a state restore answering that dispatch installs a different layout at the
    // same count; `removeBand`'s `expectedBands` compares COUNTS and sees nothing; and the removal
    // merges a band of a layout the press never saw. The press's ownership has to last as long as
    // the on-release actions that depend on it, which is what this shape says and the old one did
    // not.
    //
    // APPLIED TO ALL THREE BRANCHES, and the first implementation of this ADR applied it to ONE.
    // It moved the clear off the shared line and then put it straight back at the top of the delete
    // and solo branches, which is the same defect in two more places and is exactly what this ADR's
    // own title forbids. The review found it; the argument was already written down here. Those two
    // windows hold no dispatch IN THIS HANDLER'S BODY -- `deleteHit` is a pure read, and the solo
    // store paths are the `else` of the branch that calls `endBandMove`. **That was true of the body
    // and wrong as the whole answer**, corrected 2026-09-10: the delete window really is
    // cross-thread-only all the way into `removeBand`'s snapshot, but the solo window does not end
    // in this handler -- it continues into `setSoloMask`, whose `beginChangeGesture()` DISPATCHES
    // ahead of its guard. So the solo half is REENTRANT and a deterministic test does enter it --
    // State test 79 leg C, which the ADR-0045 clause inside that bracket now kills. The DELETE half
    // is the cross-thread-only class ADR-0046, ADR-0047, ADR-0048 and ADR-0051
    // all CLOSED rather than accepted, and the reason is the same here: with the latch cleared, the
    // question is not merely unasked, it is unanswerable, so a later reader cannot add the check
    // without also finding this line.
    // ADR-0050: FROM HERE TO THE END OF THIS HANDLER, THIS FRAME OWNS THE RECORD. Every branch
    // below clears its identifiers before it dispatches, which is what sends a re-entrant reconcile
    // down `cancelActiveDrag`'s cheap exit -- and that exit used to drop `gestureBands` on the way
    // past. The flag makes the nested call decline outright, so the ownership this handler is still
    // proving against survives until the branch drops it itself, one line before each return.
    // Scoped so every exit path clears it, including the ones that return from inside a branch.
    const ScopedGestureAction ownRecord (gestureActionDepth);
    const int pressBands = gestureBands;
    // ADR-0051: the press's split row, derived from the stamp `mouseDown` took rather than read
    // again. `convertFrom0to1` is pure arithmetic on the value `captureGestureSound` already read,
    // so this is the SAME measurement -- it costs no parameter reads at all. Used by the delete
    // branch's release-time hit-test below; every other branch here proves the row through
    // `gestureIsStale()` and derives no index from it.
    float relF[3];
    for (int k = 0; k < 3; ++k)
        relF[k] = (freqP[k] != nullptr) ? freqP[k]->convertFrom0to1 (gestureX[k]) : kFreqLo;
    // ...AND THE COUNT AND THE ROW TRAVEL TOGETHER OR NOT AT ALL. With no record in force
    // `pressBands` is -1, which makes the callee read the count LIVE -- and pairing a live count with
    // `gestureX`'s leftover row would be a worse split reading than the one this change removes.
    // `pressDeleteBand >= 0` implies `gestureBands >= 0` today (only `mouseDown` sets it, and
    // `cancelActiveDrag` clears both), but that is an invariant three handlers away; making the pair
    // inseparable here costs one pointer and removes the need to re-derive it.
    const float* const relRow = (pressBands >= 0) ? relF : nullptr;
    if (pressDeleteBand >= 0)
    {
        // The identifier goes first (ADR-0050, the drag branch's reasoning), so a reentrant
        // `cancelActiveDrag` -- `tick`'s staleness reconcile -- takes its cheap exit rather than
        // re-running this branch's action underneath it.
        const int dB = pressDeleteBand;
        pressDeleteBand = -1;
        // ...AND THE SOUND IS PROVED AT THE ACTION, not only at the top of the handler. `pressBands`
        // gives `removeBand` the COUNT this press was made in; nothing gave it the VALUES, and a
        // same-count install between the gate above and this line retargets the delete onto a band
        // whose boundaries the user never saw.
        // ADR-0051: the release's confirmation answers under the PRESS's row and count, the same one
        // `dB` was derived in, so "released over the same x" is one measurement rather than two. With
        // nothing racing this is the live row to the digit; with something racing, a persistent change
        // is still refused by `gestureIsStale()` beside it, and an ABA return now agrees rather than
        // accidentally disagreeing -- the press index is no longer derivable from a transient layout,
        // so the disagreement this comparison used to rely on has nothing left to catch.
        if (deleteHit (e.position, pressBands, relRow) == dB && ! gestureIsStale())
            removeBand (dB, pressBands);                 // released over the same x -> delete
        gestureBands = -1;                               // ADR-0050: after the action, not before it
        updateHover (e.position);
        repaint();
        return;
    }
    if (soloPressBand >= 0)
    {
        // Same shape: every latched identifier is taken into locals and cleared BEFORE anything
        // dispatches. `endBandMove` closes two change gestures, and `cancelActiveDrag` re-runs both
        // `onClearSoloPreview` and `endBandMove` when `soloPressBand`/`soloMovedBand` are still set
        // -- so a reconcile reaching this branch mid-dispatch would close the same two parameters
        // twice.
        const int  sb    = soloPressBand;
        const bool alt2  = soloPressAlt;
        const bool held  = soloHoldActive;
        const bool moved = soloMovedBand;
        soloPressBand = -1;
        soloHoldActive = soloMovedBand = false;
        // ADR-0041: the solo click names the topology it was aimed at. Without `pressBands` these
        // stores ran at the `expectedBands = -1` default and a Bands change inside the store applied
        // a stale band index to a layout that never had that band. Measured: `the click soloed band
        // 3 of a four-band layout, Bands became 2 inside the store, and the mask was written as 0x8
        // anyway`. ADR-0050 adds the other half: the SOUND is proved here too, so a same-count
        // install landing after the handler's gate does not get the click applied to it.
        if (held)
        {
            if (onClearSoloPreview) onClearSoloPreview();
            if (moved) endBandMove();
        }
        else if (! gestureIsStale())
        {
            if (alt2) // Alt/Option quick click: inactive band -> EXCLUSIVE solo
            {
                // ONE read, and the decision is made from it. Asking a "is band b soloed?" helper
                // here would re-read mbSolo, so the word the branch chose from and the word named
                // as `expectedMask` could differ -- the same "two reads where the rule needs one"
                // this series has been about. There used to be such a helper, `bandSoloed`; it lost
                // its last caller when this branch was rewritten to one reading and was removed
                // rather than left as a pattern for someone to reach for.
                const int m = soloMask();
                (void) setSoloMask (((m >> sb) & 1) != 0 ? 0 // active: all solos off (0.8.9)
                                                         : (1 << sb), // 0.8.10: only this band
                                    pressBands, m);
            }
            else (void) toggleSoloBit (sb, pressBands);
        }
        gestureBands = -1;                               // ADR-0050: after the action, not before it
        updateHover (e.position);
        repaint();
        return;
    }
    if (dragHandle >= 0)
    {
        // ADR-0050: THE IDENTIFIER IS LATCHED AND THE MEMBER CLEARED BEFORE THE DISPATCH, because
        // keeping `gestureBands` alive across `endGesture` has a second consequence the round's audit
        // named. `tick`'s 24 Hz reconcile is `if (gestureIsStale()) cancelActiveDrag();`, and it now
        // has something to find -- so a host that pumps the message loop from inside
        // `endChangeGesture` can reach it while this call is still on the stack. With `dragHandle`
        // still set, that reconcile calls `endGesture` on the SAME parameter a second time: a
        // negative open-gesture count in the processor and a spurious undo entry, which is the
        // reentrant-double-close class already escalated for three sites in this file. Cleared first,
        // the reconcile takes `cancelActiveDrag`'s cheap exit instead and closes nothing twice.
        const int h = dragHandle;
        dragHandle = -1;
        endGesture (freqP[h]);
        // ONLY IF THE TOPOLOGY IS STILL THE ONE THIS PRESS WAS MADE IN. `dragHandle` is latched
        // at mouseDown and names a split by POSITION, not identity; a host write of mbBands --
        // an automation lane, or the sound half of a state restore -- can move Bands while the
        // drag is held or, as review finding 1 showed, inside this very handler: `endGesture`
        // above notifies every listener on the dragged split's parameter, and a host recording
        // automation can write mbBands from there. This used to be a liveness check
        // (`dragHandle < bandCount() - 1`) which the rise then satisfied, because a stale index
        // can land inside a NEW range perfectly well. The condition is gone rather than doubled
        // up: `removeBand` is handed the topology this press was made in and owns the decision,
        // so there is ONE place that decides whether a removal is legitimate and one read of the
        // count behind it. `pressBands` guarantees dragHandle <= pressBands - 2 by construction
        // (handleNearX and addBandAt both return an index inside the count they read).
        // ADR-0050: ...AND THE SOUND IS RE-PROVED AFTER THAT DISPATCH, not only before it. The gate
        // at the top of this handler ran before `endGesture` above; `pressBands` carries the count
        // forward but nothing carried the VALUES, and a same-count install is precisely what
        // `gestureBands` alone cannot see (ADR-0039). Asking `gestureIsStale()` again here reads the
        // seven-slot stamp the drag's own stores keep current (`writeCrossovers` stores THROUGH
        // `gestureX[k]`, see `storeOwned`), so an uninterrupted release still removes exactly as
        // before -- the stamp equals the world it just wrote.
        //
        // `gestureBands == pressBands` IS NOW REDUNDANT DEFENCE IN DEPTH, and that is a CHANGE OF
        // STATUS recorded rather than left to rot. It was this branch's LOCAL defence against a
        // GENERAL defect: clearing `dragHandle` above sends a reentrant reconcile down
        // `cancelActiveDrag`, which used to clear `gestureBands` on its first line -- in front of the
        // cheap exit -- so the line below would have removed a band under a gesture something else
        // had just cancelled, with `gestureIsStale()` answering `false` because the latch was gone.
        //
        // That is fixed at the source as of 2026-09-10: a release action now OWNS the record until it
        // finishes and `cancelActiveDrag` declines while it does (ADR-0050, applied again; the solo
        // branch had no equivalent compare and `removeBand`'s per-store proofs had none either, which
        // is why the fix belongs there and not in three more copies of this line). Measured: removing
        // this comparison now fails NOTHING, where State test 79 leg E fails the moment either half
        // of that fix is reverted.
        //
        // It stays -- one integer compare on every release, and a second layer costs nothing -- but it
        // is no longer what holds the line and is not presented as though it were. It was labelled
        // unmeasured before and it is still unmeasured; what changed is that there is now a measured
        // guard behind it.
        if (dragRemovePending && gestureBands == pressBands && ! gestureIsStale())
            removeBand (h + 1, pressBands); // drop the dragged split, merge (#18)
    }
    // ADR-0050, the same two lines as the split-drag branch and for the same reason -- found by
    // walking the class rather than by a later review. The WIDTH drag fires no on-release action, so
    // it needs no staleness re-proof; what it does need is not to be closed twice. Keeping the latch
    // alive to here (which this ADR does) makes `tick`'s reconcile reachable inside this dispatch,
    // and with `dragBand` still set `cancelActiveDrag` would call `endGesture (widthP[dragBand])` a
    // second time while the first is on the stack. Cleared first, that reconcile takes the cheap
    // exit. This window was NOT open before this ADR -- the clear used to happen at the top of the
    // handler -- so closing it here is paying for what the ADR opened, not fixing an old defect.
    if (dragBand >= 0) { const int wb = dragBand; dragBand = -1; endGesture (widthP[wb]); }
    gestureBands = -1;                 // ADR-0050: after the on-release actions, not before them
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
    // BEFORE EVERYTHING ELSE (ADR-0050, applied to this function). A release action that has already
    // taken its identifiers into locals owns the record until it finishes, and re-entering here
    // while it runs must change NOTHING -- not the identifiers, which are already gone, and not
    // `gestureBands`, which every ownership predicate self-disables on. The clear below sat in front
    // of the cheap exit, so a nested call "that closes nothing" still disarmed `ownsSplit`,
    // `ownsWidth`, `topologyMovedUnderGesture` and `soundMovedUnderGesture` for the remainder of the
    // action -- and `setSoloMask`'s `! soundMovedUnderGesture()` runs AFTER `beginChangeGesture`
    // dispatches, which is precisely where a host pumping the message loop lands the editor's
    // stuck-drag reconcile. Measured: the solo bit written to a layout whose split had moved
    // 2000 -> 6500 Hz inside the bracket (State test 79 leg E), which leg C refuses without the
    // nested cancel. The handle-drag branch already defended itself with a bespoke
    // `gestureBands == pressBands` compare and named this mechanism in its own comment; this makes
    // the record survive instead, so the other branches need no such compare.
    if (gestureActionDepth > 0) return;
    // BEFORE the cheap exit (ADR-0039). The four flags below are the only real gestures, but
    // `gestureBands` is latched at the TOP of mouseDown -- including on the branches that latch
    // no identifier at all (an Alt-click reset, an add the count refused). Left set, the next
    // authoritative change made `gestureIsStale()` true with nothing in flight, and this
    // function's early return meant nothing ever cleared it. Clearing an int costs nothing and
    // makes the predicate mean exactly "a gesture is in progress and its world moved".
    gestureBands = -1;
    if (dragBand < 0 && dragHandle < 0 && soloPressBand < 0 && pressDeleteBand < 0)
        return;
    // ADR-0050, THE SECOND OF THE TWO SITES IT ESCALATED. Everything this function is about to
    // close is latched into locals and every member is cleared BEFORE anything dispatches, for the
    // reason `mouseUp` already gives at each of its four exits: `endGesture` is
    // `endChangeGesture()`, which notifies every listener SYNCHRONOUSLY and reaches the host, and a
    // host that pumps the message loop from there re-enters the editor while this call is still on
    // the stack.
    //
    // `gestureBands = -1` above is NOT enough cover, which is what the escalation was about. It
    // protects the reconcile in THIS class -- `tick`'s `if (gestureIsStale()) cancelActiveDrag();`,
    // whose predicate is false the moment the latch is gone -- and nothing else. The editor's
    // stuck-drag reconcile (PluginEditor.cpp, `isMouseButtonDownAnywhere() &&
    // ! anyPhysicalMouseButtonDown()`) calls straight in here and never reads `gestureBands` at
    // all; it reads the MOUSE, and under KI-013 the macOS realtime query does not refresh JUCE's
    // cached button state, so that gate can still be true on the re-entry. Reached with the
    // identifiers still live, the nested call closed the same parameter's gesture a second time --
    // a negative open-gesture count in the processor and a spurious undo boundary -- and, worse,
    // cleared the identifiers, so the OUTER call then skipped the sibling gesture it had not
    // reached yet and left it open. State test 83 legs A, B and C hold all three shapes.
    const int  wb    = dragBand;
    const int  h     = dragHandle;
    const int  sb    = soloPressBand;
    const bool held  = soloHoldActive;
    const bool moved = soloMovedBand;
    dragBand = dragHandle = soloPressBand = pressDeleteBand = -1;
    dragRemovePending = false;
    handleHoldActive  = false;
    widthHoldActive   = false;
    soloHoldActive = soloMovedBand = false;
    // The same calls in the same order as before, from the locals. A reentrant cancel now takes the
    // cheap exit above and closes nothing; leg D is the control that an UNINTERRUPTED cancel still
    // closes exactly once, because clearing first must not turn this function into a no-op.
    if (wb >= 0) endGesture (widthP[wb]);
    if (h  >= 0) endGesture (freqP[h]);
    if (sb >= 0)
    {
        if (held && onClearSoloPreview) onClearSoloPreview();
        if (moved) endBandMove();
    }
    updateHover (getMouseXYRelative().toFloat());
    repaint();
}
void SpectrumImager::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto p = e.position;
    const int N = bandCount();
    for (int i = 0; i < N - 1; ++i)
        if (numberChip (i).contains (p)) { openFreqEditor (i); return; }
    // ADR-0045, tightened -- see mouseDown for why the count is read first.
    // ADR-0046: and read ONCE. `N` already bounds the chip loop above; the derivation and the
    // reset's proof now use the same `N` rather than taking a second and a third reading of a
    // parameter three threads write.
    // ADR-0051: one split row for the pass, as for the count -- this handler decides WHICH split a
    // double click resets, and it was deriving that from two readings of the row.
    float fx[3]; captureSplits (fx);
    const int h = handleNearX (p.x, N, fx);
    if (h >= 0) resetCrossover (h);
    else { const int b = bandAtX (p.x, N, fx);
           if (b >= 0 && b < N && nearWidthLine (p, b)) resetParam (widthP[b], N); }
}
void SpectrumImager::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // AN EVENT THAT PERFORMS NO EDIT IS NOT A WHEEL EDIT (ADR-0052), and it is still the first
    // question this handler asks. `deltaX` is read nowhere below, so a horizontal-only trackpad
    // scroll, a sub-threshold delta or a host that delivers a zero-delta wheel event has nothing to
    // contribute -- and so must cost nothing: no press change, no gesture, no undo step.
    const float dy  = (wheel.isReversed ? -1.0f : 1.0f) * wheel.deltaY;
    if (std::abs (dy) < 1.0e-4f) return;
    const float sgn = dy > 0.0f ? 1.0f : -1.0f;

    // =============================================================================================
    //  A NOTCH INSIDE A PRESS BELONGS TO THAT PRESS -- ADR-0053, which SUPERSEDES ADR-0041's
    //  consequence "a wheel tick during a drag ends the drag" and PRESERVES ADR-0041's decision.
    //
    //  WHY THE OLD ANSWER WAS "END THE PRESS". ADR-0041 measured T1: a wheel tick called
    //  `captureDragOrigins()`, which re-seeds the projection origins AND the ownership record but
    //  CANNOT re-seed `dragGrabDY` / `dragGrabDX` -- the anchors the held press computes its NEXT
    //  write from. The press then owned a value its anchor predated and overwrote it on the next
    //  mouse move: `a wheel tick adopted the installed width 1.700, and the drag then wrote 0.650
    //  from an anchor taken before it`. ADR-0041's decision is that a refresh which cannot bring
    //  every piece of state the next write depends on to the same authoritative sound refreshes NONE
    //  of it, and finishing the press was one way to obey it.
    //
    //  WHY THIS ONE OBEYS IT TOO. The branches below perform NO REFRESH AT ALL. They call no
    //  `captureDragOrigins`, re-stamp no `gestureX` / `gestureW`, and read no parameter the press
    //  has not already proved. A notch moves the press's OWN anchor by the notch's own amount and
    //  then writes through the press's own owned-store path, inside the change gesture the press
    //  already opened. There is no second owner to reconcile, so the T1 rule has nothing to refuse.
    //
    //  AND MOVING THE ANCHOR IS THE WHOLE MECHANISM. Every drag here recomputes its target from the
    //  CURSOR and an anchor on each mouse move, and never reads the live parameter again -- so
    //  writing the value alone would be erased by the very next mouse move, which is T1 arriving
    //  from the other side. Moving the anchor is what makes the notch survive, and "the drag
    //  continues from the value the wheel produced" is exactly that sentence.
    //
    //  NO DEAD TRAVEL, deliberately: each anchor is re-derived from a target that has ALREADY been
    //  clamped to the limit the store itself clamps to, so scrolling past the end of a control's
    //  travel banks nothing the drag afterwards has to unwind.
    //
    //  AND THE UNDO STEP IS THE PRESS'S. Nothing here opens or closes a gesture, so `openGestures`
    //  never returns to zero mid-press and the whole drag-plus-notch interaction commits as ONE
    //  step at the release, exactly as an uninterrupted drag does.
    // =============================================================================================

    // A PENDING DELETE CLICK IS THE ONE PRESS WITH NO VALUE TO ADD TO -- it holds no parameter and
    // no anchor. The old handler cancelled it, swallowing the click (ADR-0041's "a pending click is
    // a press"); ADR-0053 keeps presses alive, and with nothing to add the notch to the honest
    // answer is to do nothing at all, so the click still fires on release as the user intended.
    if (pressDeleteBand >= 0) return;

    if (soloPressBand >= 0 || dragHandle >= 0 || dragBand >= 0)
    {
        // The same entry gate every other event of a live press passes first (ADR-0038/ADR-0039): a
        // press whose world has moved is VOID, and a notch must not be the one event that acts on
        // one. This is also what keeps State test 73 leg A green under the new rule -- the width an
        // outside hand installed makes the record stale, so the press ends here instead of writing
        // over it.
        if (gestureIsStale()) { cancelActiveDrag(); return; }
        const auto r = plot();

        if (soloPressBand >= 0)
        {
            // HOLDING A BAND'S SOLO BUTTON AND SCROLLING MOVES THE BAND. The wheel does what a
            // sideways drag of that same button does, rather than editing the band's width the way a
            // notch with no button held still does. A notch therefore STARTS the move when the 4 px
            // sideways threshold has not been crossed yet -- the notch IS the user asking for one --
            // and promotes the hold audition with it, exactly as `mouseDrag` does at that threshold.
            //
            // ...BUT ONLY IF THERE IS A BAND TO MOVE (ADR-0052, applied to this new branch). A band
            // move needs at least one EDGE SPLIT: at ONE band `beginBandMove` leaves both pins at -1
            // and `moveBand` returns at `M <= 0` having written nothing and opened nothing -- so a
            // notch there performs NO EDIT, and an event that performs no edit must have no side
            // effects. Converting the press regardless would have been the worst kind of side
            // effect: the release would take the move's branch instead of the toggle's and the solo
            // click would be silently swallowed, with nothing whatsoever gained. Found by this
            // round's own adversarial pass, against a one-band layout -- which is the configuration
            // State test 80 leg A itself uses.
            if (gestureBands < 2) return;
            if (! soloMovedBand)  { soloMovedBand = true; beginBandMove (soloPressBand, gestureBands); }
            if (! soloHoldActive) { soloHoldActive = true; if (onSoloPreview) onSoloPreview (1 << soloPressBand); }
            // `moveBand` translates the band by T = clamp (cursor - bandAnchorX, bandTmin, bandTmax),
            // so the anchor IS the accumulator: moving it adds this notch now and keeps it for every
            // later mouse move. Derived from the CLAMPED T, so a notch past the end of the travel
            // banks nothing -- and the clamp itself needs no adjustment, because the absolute pin
            // limits `bandStartLeftX + bandTmin` and `bandStartRightX + bandTmax` do not depend on T.
            const float t = juce::jlimit (bandTmin, bandTmax,
                                          ((float) e.position.x - bandAnchorX) + dy * kWheelSplitPx);
            bandAnchorX = (float) e.position.x - t;
            if (! moveBand ((float) e.position.x, gestureBands)) { cancelActiveDrag(); return; }
        }
        else if (dragHandle >= 0)
        {
            // The split drag steers `cursor - dragGrabDX` and nothing else -- `dragOrigX[handle]` is
            // consumed once, at the press, to compute that offset and never re-enters the pinned
            // split's target. So `dragGrabDX` is the single variable, and clamping the target to the
            // same travel `projectFromOrig` clamps a pin to means the anchor cannot bank travel the
            // store would have refused anyway.
            const float lo = r.getX() + kMinGapPx, hi = r.getRight() - kMinGapPx;
            const float want = juce::jlimit (lo, hi,
                                             ((float) e.position.x - dragGrabDX) + dy * kWheelSplitPx);
            dragGrabDX = (float) e.position.x - want;
            if (! dragCrossoverTo (dragHandle, want, gestureBands)) { cancelActiveDrag(); return; }
        }
        else if (dragBand >= 0 && dragBand < (int) std::size (gestureW))
        {
            // ADR-0047: ONE READING PROVES, PLANS AND ANCHORS. The ownership proof, the value the
            // notch adds to and the anchor it leaves behind are the same measurement, so a foreign
            // write cannot pass the proof and then be carried into the plan.
            const float wNorm = (widthP[dragBand] != nullptr) ? widthP[dragBand]->getValue() : 0.0f;
            if (! ownsWidth (dragBand, wNorm)) { cancelActiveDrag(); return; }
            const float base = (widthP[dragBand] != nullptr)
                             ? widthP[dragBand]->convertFrom0to1 (wNorm) : 1.0f;
            const float want = juce::jlimit (0.0f, 2.0f,
                                             base + sgn * juce::jmax (kWheelWidthMin,
                                                                      std::abs (dy) * kWheelWidthPer));
            // The width drag computes `yToWidth (cursorY - dragGrabDY)`, so anchoring from the
            // notch's own target is what makes the drag continue from it -- and this is the exact
            // form the 3 px engage itself uses. It also ENGAGES the drag: a press that has not
            // crossed that threshold has made no edit yet, and a notch is one the user has just
            // asked for.
            widthHoldActive = true;
            dragGrabDY = (float) e.position.y - widthToY (want);
            if (! storeOwned (widthP[dragBand], want, gestureW[dragBand])) { cancelActiveDrag(); return; }
        }
        repaint();
        return;
    }

    // NOTHING OF THIS CLASS'S IS IN FLIGHT, so this is a STANDALONE scroll -- the case every rule
    // from here down is about.
    //
    // AND IT IS NOT GATED ON THE MOUSE BUTTON, deliberately, though the branches above make that
    // possible for the first time. JUCE routes a wheel event to whatever is under the POINTER, not
    // to whatever captured the press, so a knob drag that has carried the cursor over this display
    // delivers its notches here -- and a `! e.mods.isAnyMouseButtonDown()` gate would make them
    // inert. That is a BEHAVIOUR CHANGE this task did not ask for (today such a notch edits the
    // multiband control under the pointer, and it still does), and it would also silence a notch
    // during a press of this class's own that latched no identifier at all -- an Alt-click reset, an
    // add the count refused. The branches above own every press this class actually has, so what
    // reaches here is a scroll with nothing of ours in flight, whatever some other component's
    // button is doing.
    //
    // ...and this next line is the one thing `cancelActiveDrag()` still did on this path, which is all it did:
    // with no identifier latched its whole body is this store and a cheap exit. `mouseDown` stamps
    // the record at the top for EVERY branch, including ones that latch no identifier (an Alt-click
    // reset, an add the count refused), so a stale stamp can outlive such a press and must not be
    // left to arm the split branch's own bracket below.
    gestureBands = -1;
    // ADR-0050's guard, for the burst this handler is about to own. The gesture opens below DISPATCH
    // -- they are new here, and they are what makes a scroll undoable at all -- and a host that pumps
    // the message loop from one of them lands `tick`'s reconcile in `cancelActiveDrag`, whose first
    // act is the clear one line above. That clear is the record `writeCrossovers` proves every store
    // against. Declining for the length of the burst costs nothing, because nothing is in flight for
    // a cancellation to cancel.
    const ScopedGestureAction ownBurst (gestureActionDepth);
    // ADR-0046. ONE TOPOLOGY READING DECIDES THE WHOLE TICK. This handler used to take THREE --
    // this one, a second inside the staleness test below, a third at the stamp -- and let
    // `handleNearX` / `bandAtX` take a fourth of their own. Between any two of them an
    // AUDIO-THREAD automation write of mbBands can land: there is no dispatch anywhere in this
    // stretch, so reentrancy cannot cross it, but three threads write this parameter and a
    // cross-thread write needs no seam. The stamp was the last of the four, which is the one
    // ordering that cannot work: an index derived at count 3 and stamped with the count 4 that
    // arrived a few instructions later claims a topology it was never derived in, and every
    // later tick of the burst then compares against that claim and passes. `mouseDown` has always
    // read the count FIRST, at the top, before any branch derives anything from it, and
    // `SpectrumImager.cpp:2292` relies on exactly that: "handleNearX and addBandAt both return an
    // index inside the count they read" (`SpectrumImager.cpp:2362`). The wheel is now the same
    // shape, and `N` is threaded into
    // the derivation so the count the index is derived under and the count it is stamped with are
    // ONE READ rather than two that usually agree. Bound, stamp, staleness test and the width
    // store all use it, so the tick has a single topology or it has none.
    const int N = bandCount();
    // ADR-0045. THE WHEEL'S LATCH IS STAMPED WITH THE TOPOLOGY IT WAS TAKEN IN. `scrollHandle` and
    // `scrollBand` are positional identifiers latched at the FIRST tick of a burst and reused by
    // every later one; they are dropped by a >3 px `mouseMove` or by `mouseExit`, and by nothing
    // else. A band count that moves between two ticks -- a host lane, an undo, a preset -- re-lays
    // the whole display out under a hand that has not moved, so the next tick steered a split the
    // pointer was no longer over. Bounds alone do not close that: an index that LANDS inside the
    // new range is exactly the case ADR-0039 refused to accept for `removeBand`. Every other
    // positional identifier in this class is stamped by `gestureBands` and tested by
    // `gestureIsStale()`; this one was not. Dropping the latch re-derives it from the cursor on the
    // very next tick, which is what a `mouseMove` already did -- so an uninterrupted burst behaves
    // exactly as before.
    // ADR-0051: ONE reading of the split row per tick, used by the staleness test below AND by any
    // re-derivation it causes. It was taken inside the re-derivation branch, which is right as far
    // as it goes, but the test that decides whether to re-derive needs the same row -- and two
    // readings of it would be the defect that ADR its own subject.
    // ...IN THE PARAMETER'S OWN UNITS, so this ONE reading serves every consumer in the tick: the
    // staleness test and the hit-test want Hz, the ownership stamp wants normalised, and
    // `convertFrom0to1` is pure arithmetic, so deriving one from the other is the same measurement
    // rather than a second one (ADR-0047).
    float nx[3], fx[3];
    for (int k = 0; k < 3; ++k)
    {
        nx[k] = (freqP[k] != nullptr) ? freqP[k]->getValue() : 0.0f;
        fx[k] = (freqP[k] != nullptr) ? freqP[k]->convertFrom0to1 (nx[k]) : kFreqLo;
    }
    // ...AND THE ROW IS HALF OF THE TOPOLOGY, which is the half this latch did not stamp.
    // ADR-0045's rule is that a positional identifier is void once the topology it was taken in
    // MOVES, and ADR-0039 settled that the count is not the whole topology -- ADR-0051 made the
    // split row the other half explicitly. The paragraph above argues the count case in exactly the
    // terms that apply here: a change "re-lays the whole display out under a hand that has not
    // moved, so the next tick steered a split the pointer was no longer over". A same-count split
    // move does precisely that. `scrollAnchor` cannot see it -- that is the >3 px test against the
    // POINTER, and the pointer has not moved; only the layout under it has. So a burst latched over
    // band 1 kept editing band 1's width after automation slid a split across the cursor, and a
    // burst latched on handle 1 kept steering handle 1 after the row put a different handle there.
    // State test 77 leg E is that, deterministically: the window is BETWEEN two ticks, i.e. user
    // time, so unlike the ADR-0046/0047/0051 windows it needs no thread and no probe.
    if (scrollBands >= 0)
    {
        bool moved = (N != scrollBands);
        for (int k = 0; ! moved && k < 3; ++k) moved = ! juce::exactlyEqual (fx[k], scrollFx[k]);
        if (moved) scrollHandle = scrollBand = scrollBands = -1;
    }
    if (scrollHandle < 0 && scrollBand < 0)
    {
        // ADR-0051: the latch is derived from ONE reading of the split row as well as one of the
        // count. It is stamped with `scrollBands = N` and reused by every later tick of the burst,
        // so a row read twice here names a handle the pointer was never over.
        const int h = handleNearX ((float) e.position.x, N, fx);
        if (h >= 0) scrollHandle = h;
        else        scrollBand = bandAtX ((float) e.position.x, N, fx);
        scrollAnchor = e.position;
        scrollBands  = N;   // ADR-0045/0046: the topology this latch was DERIVED in
        for (int k = 0; k < 3; ++k) scrollFx[k] = fx[k];  // ...and the ROW it was derived in
    }
    if (scrollHandle >= 0 && scrollHandle < N - 1)
    {
        // ADR-0047/0051, THE WITHIN-TICK HALF. This was `captureDragOrigins()`, which is
        // `captureGestureSound(); seedDragOrigins();` -- and the stamping half took a SECOND reading
        // of the row this tick had already read and already proved. `scrollFx` closes the window
        // BETWEEN two ticks; this one is inside a single tick, between the reading at the top and
        // this line. A same-count split write landing there was proved absent by the first reading
        // and then ADOPTED by the second: `gestureX` became the new row, `dragOrigX` was seeded from
        // it, and the tick steered the latched handle FROM ITS NEW POSITION -- a handle the pointer
        // is no longer over -- with `writeCrossovers` proving each store against the row it had just
        // adopted, so nothing could refuse it. UNMEASURED, and deliberately so: `--wheel-adopt-probe`
        // was built for this window, read 38 in 1200 ticks before and 34 after -- not a signal --
        // and a diagnostic showed it counting benign ticks, because its detector gated on a flag the
        // lane set AFTER its store returned with no ordering against the message-thread store. The
        // probe was WITHDRAWN rather than shipped as a gate that cannot fail; the attempt is in
        // `docs/procedures/TESTING.md` and in the worklog. This guard is defence in depth on the
        // same footing as `removeBand`'s entry proof: correct by ADR-0047's rule, inert with nothing
        // racing, and with no reachable test.
        //
        // The stamp is the row this tick PROVED, and `seedDragOrigins` derives the origins from it,
        // so the plan, the stamp and the per-store proof are one measurement. A split that moves
        // after this point is refused by `ownsSplit` inside the store, and the NEXT tick's `scrollFx`
        // test drops the latch and re-derives at the pointer -- the ADR-0045 half already in place,
        // and the reason re-hit-testing every tick is still the wrong answer: the wheel's own edits
        // move the handle it is steering.
        for (int k = 0; k < 3; ++k) gestureX[k] = nx[k];
        // The widths are not part of this race -- a wheel tick never writes one, and nothing in the
        // bracket below reads `gestureW` -- but they are stamped exactly as before, so the record is
        // whole rather than half-fresh.
        for (int b = 0; b < (int) std::size (gestureW); ++b)
            gestureW[b] = (widthP[b] != nullptr) ? widthP[b]->getValue() : 0.0f;
        seedDragOrigins();
        // ADR-0043. THE WHEEL OWNS THE BURST IT ISSUES. `cancelActiveDrag()` above clears
        // `gestureBands`, which is right -- no press is in flight -- but it also waives `ownsSplit`
        // and the count re-proof inside `writeCrossovers` for the burst that follows, and that burst
        // is up to three stores with a host dispatch between each. So the one path ADR-0040 did not
        // cover had no per-store ownership at all: measured `5000.0 Hz was installed and 1476.4 Hz
        // was written over it`. `captureDragOrigins` has just recorded exactly what this burst is
        // about to steer, so naming the topology alongside it costs one int and makes both checks
        // live. Cleared again immediately: the wheel still leaves nothing in flight.
        // ADR-0046: and the topology it names is `N`, the one `scrollHandle` was derived under --
        // not a fresh read. A fresh read would hand `writeCrossovers` a count to prove against
        // that the latch had never been checked against, which is the whole defect one branch up.
        gestureBands = N;
        // ADR-0053. THE BURST IS ONE UNDOABLE EDIT. Every store below used to be a bare
        // `setValueNotifyingHost` outside any change gesture, so `openGestures` never left zero,
        // the poll took its NON-gesture branch and folded the new split into the committed baseline
        // with no undo entry to reverse it -- the imager half of KI-010. Opened AFTER the stamp two
        // lines up, never before: `beginChangeGesture` DISPATCHES, and a host answering it by
        // writing a split would otherwise be copied into the very record every store below proves
        // against (ADR-0047/ADR-0051). `writeCrossovers` re-proves the count and every slot after
        // the dispatch, so the open widens no window it does not also guard.
        //
        // ONE GESTURE, UP TO THREE PARAMETERS -- said plainly, because it would be easy to read the
        // paragraph above as claiming more. `dragCrossoverTo` can push NEIGHBOURING splits aside,
        // and those stores are outside any bracket of their own, so a host recording touch/latch
        // sees them as automation rather than as part of this edit. UNDO is unaffected and this is
        // not the KI-010 shape returning: `openGestures` is one global count, so the whole burst --
        // neighbours included -- lands in the single step this bracket commits. It is also exactly
        // what the DRAG path has done since 0.6.x, where `mouseDown` opens a gesture on the grabbed
        // split alone and the same projection moves its neighbours. Matching it is deliberate; a
        // gesture per pushed neighbour would be a change to how this plug-in reports automation,
        // which is not what this round was asked for.
        const ScopedWheelName wheelName (onWheelStep, freqP[scrollHandle]);
        beginGesture (freqP[scrollHandle]);
        // ADR-0047: the tick's target starts from the position `captureDragOrigins` just stamped,
        // not from a fresh read of the same split. A fresh read failed SAFE -- `ownsSplit` refuses a
        // value the stamp does not know -- but a refusal is a wheel tick the user loses for no
        // reason they can see, which is the same trade ADR-0046 closed one branch up for the count.
        dragCrossoverTo (scrollHandle, dragOrigX[scrollHandle] + dy * kWheelSplitPx, N);
        endGesture (freqP[scrollHandle]);
        gestureBands = -1;
        // THE STAMP FOLLOWS THE BURST'S OWN EDITS, and without this the row half above would drop
        // the latch on the second tick of every ordinary split burst -- because a split burst
        // writes the very row that test watches. DERIVED, not re-read (ADR-0047): `writeCrossovers`
        // stores THROUGH `gestureX[k]` via `storeOwned`, so that array already holds what this
        // burst's own stores CONFIRMED, and `convertFrom0to1` is pure arithmetic on it. Reading the
        // parameters again here would take a second reading of a row this pass has already
        // measured, and would quietly adopt a foreign write landing in between -- which is the
        // defect the previous round closed one function away. A refused store leaves the stamp at
        // the pre-store row, so the next tick sees the difference and retargets: correct, because a
        // store this burst did not land is not a row this burst owns. State test 77 leg F is the
        // control that an uninterrupted split burst keeps its latch across all of this.
        for (int k = 0; k < 3; ++k)
            scrollFx[k] = (freqP[k] != nullptr) ? freqP[k]->convertFrom0to1 (gestureX[k]) : kFreqLo;
    }
    else if (scrollBand >= 0 && scrollBand < N)
    {
        const float step = sgn * juce::jmax (kWheelWidthMin, std::abs (dy) * kWheelWidthPer); // velocity-aware (#15 prior)
        // ADR-0046: the split branch above proves the topology at every store, through
        // `gestureBands` inside `writeCrossovers`. This one had no proof at all -- one bare store,
        // reached from a latch proved at the top of the handler, with the whole tick in between.
        // WHAT A WIDTH STORE FOR A VANISHED BAND ACTUALLY COSTS, which is NOT what ADR-0045 says
        // at `resetParam`: `setParam` opens no gesture, so `parameterGestureChanged` never fires
        // and `openGestures` never returns to zero (`PluginProcessor.cpp:812-825`) -- there is no
        // undo step. That is worse, not better. The value is forwarded to the host as a parameter
        // change all the same, and it is folded into the committed baseline by the next poll with
        // no undo entry to reverse it; and because `MultibandWidth` glides only the widths the
        // live count uses, a width parked on a hidden band is inert until the count RISES, when it
        // is adopted at full magnitude instead of gliding in. (`resetParam` does open a gesture,
        // so ADR-0045's wording is right there and wrong here -- the two stores are not the same
        // shape, and the audit that copied the sentence across was corrected on this point.)
        // ADR-0053 GAVE THIS BRANCH A CHANGE GESTURE, AND WITH IT THE PROOF IT HAD NEVER HAD. The
        // paragraph above is still the whole argument for the count check; what it could not say
        // then is that `setParam` is a bare store with no VALUE ownership behind it at all, because
        // `gestureBands` was -1 across it and `ownsWidth` self-disables there. That was sound while
        // nothing dispatched between the reading and the store. It is not sound now: opening the
        // gesture that makes this notch undoable DISPATCHES, and a host answering it by writing this
        // width would have had `base + step` written straight over it.
        //
        // So the branch takes the same shape the split branch has carried since ADR-0043: ONE
        // reading of the width, taken BEFORE anything dispatches, stamped into the record, and a
        // store that proves the record after the dispatch and refuses if it has moved (ADR-0047).
        // `setParam`'s count check survives inside `bandCount() == N`, adjacent to the store exactly
        // as it was. `storeOwned` returns whether it committed; nothing follows this store that
        // could act on a refusal, so the result is discarded deliberately rather than dropped.
        const float wNorm = (widthP[scrollBand] != nullptr) ? widthP[scrollBand]->getValue() : 0.0f;
        const float base  = (widthP[scrollBand] != nullptr)
                          ? widthP[scrollBand]->convertFrom0to1 (wNorm) : 1.0f;
        gestureW[scrollBand] = wNorm;
        gestureBands = N;
        {
            const ScopedWheelName wheelName (onWheelStep, widthP[scrollBand]);
            beginGesture (widthP[scrollBand]);
            if (ownsWidth (scrollBand) && bandCount() == N)
                (void) storeOwned (widthP[scrollBand], juce::jlimit (0.0f, 2.0f, base + step),
                                   gestureW[scrollBand]);
            endGesture (widthP[scrollBand]);
        }
        gestureBands = -1;
    }
    repaint();
}

} // namespace anamorph::gui
