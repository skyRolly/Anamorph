#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>

namespace anamorph::gui
{

// ============================================================================
//  FrameClock
//
//  Adaptive refresh driver for the visualizers (Vectorscope, LevelMeter,
//  StereoMeter, SpectrumImager). Replaces their fixed 60 Hz juce::Timers with
//  a juce::VBlankAttachment paced to the display the component actually sits
//  on (the attachment follows the component across monitors):
//
//    * The tick rides the display's vertical blank, so drawing is synchronous
//      with presentation (no 60 Hz-timer beat against a 60/120/144 Hz panel).
//    * The executed rate is the vblank rate divided to an EVEN cadence of at
//      most ~125 Hz: every ceil(rate/126)-th vblank runs the callback
//      (60 -> 60, 120 -> 120, 144 -> 72, 165 -> 82.5, 240 -> 120). Uniform
//      frame pacing at a lower rate reads smoother than dropping every Nth
//      frame to hit a cap exactly, and the cap bounds worst-case paint CPU.
//    * The cadence is measured from the vblank timestamps (EMA of deltas).
//      When it cannot be determined -- implausible or missing deltas -- the
//      clock automatically falls back to pacing ~60 Hz by wall clock.
//
//  The callback receives the REAL elapsed seconds since the tick it last ran
//  (clamped to 50 ms, the editor's meterVBlank idiom), so per-tick ballistics
//  rewritten in dt form behave identically at 60 Hz and stay time-correct at
//  every other rate. Callbacks arrive on the message thread, exactly like the
//  juce::Timer ticks they replace (THREAD_MODEL unchanged).
// ============================================================================
class FrameClock
{
public:
    FrameClock() = default;

    // Attach to a component and start ticking. Restartable (stop()/start()):
    // the pacing state resets so the first tick after a restart uses a
    // neutral 1/60 s dt rather than the whole hidden interval.
    void start (juce::Component& c, std::function<void (double dtSeconds)> cb)
    {
        callback  = std::move (cb);
        lastStamp = 0.0;
        lastRun   = 0.0;
        emaDelta  = kNominalDt;
        haveEma   = false; // MUST reset with emaDelta: a leaked true would EMA from the
                           // stale seed instead of snapping to the first post-restart
                           // delta (defeating the cap on a faster display) and would
                           // pin the plausible branch, never reaching the wall-clock
                           // fallback if only implausible deltas arrive after restart.
        countdown = 1;
        attachment = juce::VBlankAttachment (&c, [this] (double t) { onVBlank (t); });
    }

    void stop()
    {
        attachment = {};
        callback   = nullptr;
    }

    bool isRunning() const noexcept { return ! attachment.isEmpty(); }

private:
    static constexpr double kNominalDt = 1.0 / 60.0;   // fallback pace + first-tick dt
    static constexpr double kMaxDt     = 0.05;          // editor meterVBlank clamp idiom
    static constexpr double kCapHz     = 126.0;         // even-cadence divider target

    void onVBlank (double t)
    {
        // Measure the vblank cadence. Deltas outside (0.5 ms, 50 ms) -- stalls,
        // duplicate/garbage timestamps -- do not update the estimate; if the
        // estimate was never fed, the clock behaves as a 60 Hz wall-clock pacer.
        bool plausible = false;
        if (lastStamp > 0.0)
        {
            const double d = t - lastStamp;
            if (d > 0.0005 && d < 0.05)
            {
                // Fast-attack on a rate INCREASE, smooth otherwise. An upward
                // refresh-rate transition (e.g. the editor dragged onto a faster
                // monitor -- no stop()/start(), the attachment just follows the
                // component) would otherwise run at the full new rate for the
                // whole EMA settle window (~10-30 frames), briefly exceeding the
                // ~125 Hz cap. Snapping emaDelta down the moment a much-shorter
                // delta arrives (>33 % faster) re-caps within a frame. Small
                // fluctuations and rate DECREASES ease via the 0.125 EMA; snapping
                // down over-eagerly only ever runs FEWER frames, never more, so it
                // is CPU-safe (a stray short delta cannot spike the rate).
                if (! haveEma || d < 0.75 * emaDelta) emaDelta = d;
                else                                  emaDelta += 0.125 * (d - emaDelta);
                haveEma   = true;
                plausible = true;
            }
        }
        lastStamp = t;

        if (plausible || haveEma)
        {
            // Even-cadence division: run every N-th vblank so pacing stays
            // uniform. N re-derives each tick, so moving the window between a
            // 144 Hz and a 60 Hz monitor adapts within the EMA's settle time.
            const double rate = 1.0 / juce::jmax (1.0e-4, emaDelta);
            const int    n    = juce::jmax (1, (int) std::ceil (rate / kCapHz));
            if (--countdown > 0)
                return;
            countdown = n;
        }
        else
        {
            // Cadence undeterminable: fall back to ~60 Hz paced by wall clock.
            if (lastRun > 0.0 && t - lastRun < 0.9 * kNominalDt)
                return;
        }

        const double dt = (lastRun > 0.0) ? juce::jlimit (0.0, kMaxDt, t - lastRun)
                                          : kNominalDt;
        lastRun = t;
        if (callback)
            callback (dt);
    }

    juce::VBlankAttachment attachment;
    std::function<void (double)> callback;
    double lastStamp = 0.0, lastRun = 0.0, emaDelta = kNominalDt;
    bool   haveEma   = false;
    int    countdown = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrameClock)
};

// Frame-rate-independent per-tick smoothing coefficient. `c60` is a geometric
// lerp coefficient tuned for the fixed 60 Hz tick these visualizers used to run
// at (`v += c60 * (target - v)`); this re-expresses it for an arbitrary dt so
// the retention (1 - rate) stays geometric in time -- the decay reaches the same
// value at the same wall-clock time on any display. At dt == 1/60 s it returns
// c60 to within ~3e-8 (float pow rounding; the reference exponent is ~1.0), far
// below the 1/255 display quantum, so 60 Hz output is perceptually identical
// (Class B) to the pre-adaptive code -- and the runtime dt jitters around 1/60
// regardless, exactly as the old fixed-coefficient tick's timing did. At 120 Hz
// it is 1 - sqrt(1 - c60), the exact half-step (two of which compose back to
// c60). Cheap enough to call once per tick (never per-bin): the returned rate is
// shared across every element that tick.
inline float frameCoeff (float c60, double dt) noexcept
{
    return 1.0f - std::pow (1.0f - c60, (float) (dt * 60.0));
}

} // namespace anamorph::gui
