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

    // 0..1: longer trails + slower fade.
    void setPersistence (float p) noexcept { persistence = juce::jlimit (0.0f, 1.0f, p); }

private:
    void timerCallback() override { repaint(); }
    void drawGrid (juce::Graphics&, juce::Rectangle<float> area, float radius);

    anamorph::ScopeBuffer& scope;
    float persistence = 0.6f;

    std::vector<float> bufL, bufR; // scratch read from the ring buffer

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Vectorscope)
};

} // namespace anamorph::gui
