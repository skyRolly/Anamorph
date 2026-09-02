#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include "../dsp/ScopeBuffer.h"
#include "FrameClock.h"

namespace anamorph::gui
{

// ============================================================================
//  Vectorscope
//
//  Diamond / Lissajous polar vectorscope: the L/R axes are rotated 45 deg so a
//  pure mono signal draws a VERTICAL line and pure "side" draws HORIZONTAL --
//  the familiar goniometer the spec asks for (section 8).
//
//  The audio thread only writes into a lock-free ScopeBuffer; this component
//  reads a decimated window of recent samples on a 60 fps timer and draws them
//  with an age-based alpha falloff (the "phosphor" afterglow). When the editor
//  attaches an OpenGL context, all of this painting is GPU-composited, keeping
//  CPU usage low. Nothing is ever drawn on the audio thread.
// ============================================================================
class Vectorscope : public juce::Component
{
public:
    explicit Vectorscope (anamorph::ScopeBuffer& buffer);
    ~Vectorscope() override;

    void paint (juce::Graphics&) override;

    // The cached static layer bakes look-dependent drawing, so any look change
    // must drop it; the next paint() rebuilds at the current size/scale (H2).
    void lookAndFeelChanged() override { staticLayer = {}; }

    // 0..1: longer trails + slower fade.
    //
    // The finiteness guard is not defensive padding, it closes a reachable
    // undefined conversion (ER-STATE-21, round 17). `juce::jlimit` returns its
    // argument when NEITHER comparison is true, which is exactly what a NaN does,
    // so the clamp on its own is transparent to one -- and `windowFrames()` below
    // then evaluates `(int)` of it, which is UNDEFINED for a non-finite float
    // ([conv.fpint]), the same class round 12 fixed on the legacy Settings path.
    // Two inputs reach here non-finite, both from a hand-edited or corrupted
    // session's `int_scopePersist`, which nothing on the restore path rejects and
    // opening the editor does not repair:
    //   * "nan" travels the whole way -- Value -> Slider -> pow -> here; and
    //   * ANY NEGATIVE value arrives as a NaN, because the editor's
    //     `applyScopePersist()` raises it to a fractional power first
    //     (`pow(-1.0f, 0.737f)` is NaN), so a perfectly finite out-of-range value
    //     becomes the non-finite one.
    // Substituting the member's own initialiser is the recovery the meters and the
    // correlation display already apply to a non-finite sample (ADR-0009): the
    // control keeps working at its default rather than propagating the poison.
    void setPersistence (float p) noexcept
    {
        persistence = std::isfinite (p) ? juce::jlimit (0.0f, 1.0f, p) : kDefaultPersistence;
        frameDirty = true; // window length + point alpha depend on persistence
    }

    // The clamped, always-finite value in force. Exists so the guard above is
    // testable through the real editor (State test 32) rather than by inspection.
    float getPersistence() const noexcept { return persistence; }

private:
    void tick(); // FrameClock callback (display-rate; no dt-dependent state here)
    void drawGrid (juce::Graphics&, juce::Rectangle<float> area, float radius);
    void ensureStaticLayer (juce::Graphics&, juce::Rectangle<float> area);

    // Frames the current persistence makes visible. Shared by paint() and the
    // timer's idle gate so the two can never disagree about the window.
    int windowFrames() const noexcept
    {
        return (int) juce::jmap (persistence, 0.0f, 1.0f, 1200.0f, 8000.0f);
    }

    anamorph::ScopeBuffer& scope;
    // 0.6 is the remapped 50 % default (applyScopePersist's pow(0.5, 0.737)).
    static constexpr float kDefaultPersistence = 0.6f;
    float persistence = kDefaultPersistence;

    std::vector<float> bufL, bufR; // scratch read from the ring buffer

    // Cached static layer (H2, opaque since N2): background gradient + rounded
    // panel + glass edges + grid + axis labels -- everything that is a pure
    // function of (size, physical scale, look) -- rendered ONCE into an opaque
    // RGB image at physical resolution (corners pre-filled with the editor's
    // flat colours::bg backdrop) and copy-blitted 1:1 by paint(). Rebuilt only
    // when size, scale or look changes; a normal repaint never re-rasterizes
    // it. Signal-dependent drawing (point cloud, clip ring) is never cached.
    juce::Image staticLayer;
    int   staticW = 0, staticH = 0;
    float staticScale = 0.0f;

    // Idle repaint gate state (message thread only -- see tick()):
    std::uint64_t lastSeenCount = 0; // ring write count at the previous tick
    std::uint64_t lastNonZero   = 0; // newest bound on non-zero ring content
    bool frameDirty      = true;     // displayed frame is stale -> repaint
    bool lastFrameSilent = false;    // last painted frame was the all-zero image

    // Adaptive refresh (display-rate, capped ~120 Hz): the freshness scan + idle
    // gate below are delta-based (writeCount) and the trail age is positional, so
    // nothing here depends on the tick rate -- the FrameClock's dt is ignored.
    FrameClock frameClock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Vectorscope)
};

} // namespace anamorph::gui
