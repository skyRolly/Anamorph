#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/Correlation.h"

namespace anamorph::gui
{

// ============================================================================
//  StereoMeter
//
//  A slim -1..+1 readout placed around the vectorscope (Insight / Imager 2
//  layout, spec feedback #9):
//    * Balance     (horizontal, under the scope): L .. C .. R energy balance.
//    * Correlation (vertical, right of the scope): phase correlation -1..+1.
//  Reads the audio-thread anamorph::CorrelationMeter via atomics on a timer.
// ============================================================================
class StereoMeter : public juce::Component,
                    private juce::Timer
{
public:
    enum class Orientation { Horizontal, Vertical };
    enum class Type        { Balance, Correlation };

    StereoMeter (anamorph::CorrelationMeter& src, Orientation, Type);
    ~StereoMeter() override;

    void paint (juce::Graphics&) override;

    // The cached static layer bakes look-dependent drawing, so any look change
    // must drop it; the next paint() rebuilds at the current size/scale (H13).
    void lookAndFeelChanged() override { staticLayer = {}; }

private:
    void timerCallback() override;
    void visibilityChanged() override;
    void ensureStaticLayer (juce::Graphics&, juce::Rectangle<float> bounds);

    anamorph::CorrelationMeter& source;
    Orientation orientation;
    Type        type;
    float value = 0.0f;          // smoothed display value
    float shownValue = 1.0e9f;   // value at the last repaint request (S3 gate)

    // Cached static layer (H13, the H2 recipe): the glass panel + centre tick --
    // a pure function of (size, physical scale, look) -- rendered once into an
    // ARGB image at physical resolution and blitted 1:1 by paint(). The end
    // labels are NOT cached: the original z-order draws them ON TOP of the
    // pointer (which can reach the track ends), so they stay live to preserve
    // stacking exactly; they cost <1 % of a paint. The pointer (value-dependent
    // colour, glow, gradient core, highlight) is never cached.
    juce::Image staticLayer;
    int   staticW = 0, staticH = 0;
    float staticScale = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoMeter)
};

} // namespace anamorph::gui
