#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/Correlation.h"
#include "FrameClock.h"

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
class StereoMeter : public juce::Component
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
    void tick (double dt); // FrameClock callback (display-rate; dt-corrected glide)
    void visibilityChanged() override;
    void ensureStaticLayer (juce::Graphics&, juce::Rectangle<float> bounds);

    anamorph::CorrelationMeter& source;
    Orientation orientation;
    Type        type;
    float value = 0.0f;          // smoothed display value
    float shownValue = 1.0e9f;   // value at the last repaint request (S3 gate)

    // Cached static layer (H13, the H2 recipe; opaque since N2): the glass panel
    // + centre tick -- a pure function of (size, physical scale, look) --
    // rendered once into an opaque RGB image at physical resolution (corners
    // pre-filled with the editor's flat colours::bg backdrop) and copy-blitted
    // 1:1 by paint(). The end labels are NOT cached: the original z-order draws
    // them ON TOP of the pointer (which can reach the track ends), so they stay
    // live to preserve stacking exactly; they cost <1 % of a paint. The pointer
    // (value-dependent colour, glow, gradient core, highlight) is never cached.
    juce::Image staticLayer;
    int   staticW = 0, staticH = 0;
    float staticScale = 0.0f;

    // Adaptive refresh (display-rate, capped ~120 Hz): started only while shown.
    // The pointer glide below is re-expressed in dt form (frameCoeff), so it
    // reaches the same place at the same wall-clock time on a 60 or 120 Hz panel
    // and matches the old 60 Hz glide to within the display quantum (Class B).
    FrameClock frameClock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoMeter)
};

} // namespace anamorph::gui
