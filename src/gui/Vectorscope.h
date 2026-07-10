#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/ScopeBuffer.h"

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
class Vectorscope : public juce::Component,
                    private juce::Timer
{
public:
    explicit Vectorscope (anamorph::ScopeBuffer& buffer);
    ~Vectorscope() override;

    void paint (juce::Graphics&) override;

    // The cached static layer bakes look-dependent drawing, so any look change
    // must drop it; the next paint() rebuilds at the current size/scale (H2).
    void lookAndFeelChanged() override { staticLayer = {}; }

    // 0..1: longer trails + slower fade.
    void setPersistence (float p) noexcept
    {
        persistence = juce::jlimit (0.0f, 1.0f, p);
        frameDirty = true; // window length + point alpha depend on persistence
    }

private:
    void timerCallback() override;
    void drawGrid (juce::Graphics&, juce::Rectangle<float> area, float radius);
    void ensureStaticLayer (juce::Graphics&, juce::Rectangle<float> area);

    // Frames the current persistence makes visible. Shared by paint() and the
    // timer's idle gate so the two can never disagree about the window.
    int windowFrames() const noexcept
    {
        return (int) juce::jmap (persistence, 0.0f, 1.0f, 1200.0f, 8000.0f);
    }

    anamorph::ScopeBuffer& scope;
    float persistence = 0.6f;

    std::vector<float> bufL, bufR; // scratch read from the ring buffer

    // Cached static layer (H2): background gradient + rounded panel + glass edges
    // + grid + axis labels -- everything that is a pure function of (size,
    // physical scale, look) -- rendered ONCE into an ARGB image at physical
    // resolution and blitted 1:1 by paint(). Rebuilt only when size, scale or
    // look changes; a normal repaint never re-rasterizes it. Signal-dependent
    // drawing (point cloud, clip ring) is never cached.
    juce::Image staticLayer;
    int   staticW = 0, staticH = 0;
    float staticScale = 0.0f;

    // Idle repaint gate state (message thread only -- see timerCallback):
    std::uint64_t lastSeenCount = 0; // ring write count at the previous tick
    std::uint64_t lastNonZero   = 0; // newest bound on non-zero ring content
    bool frameDirty      = true;     // displayed frame is stale -> repaint
    bool lastFrameSilent = false;    // last painted frame was the all-zero image

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Vectorscope)
};

} // namespace anamorph::gui
