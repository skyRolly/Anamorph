#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/Correlation.h"

namespace anamorph::gui
{

// ============================================================================
//  CorrelationMeter
//
//  A -1..+1 phase-correlation meter, drawn either horizontally (placed under
//  the vectorscope) or vertically (placed to its right) per the Insight/Ozone
//  Imager reference layout. Reads the audio-thread CorrelationMeter via atomics
//  on a timer; the horizontal meter shows the slow average, the vertical shows
//  the fast value (so you get both a stable and a responsive read).
// ============================================================================
class CorrelationMeter : public juce::Component,
                         private juce::Timer
{
public:
    enum class Orientation { Horizontal, Vertical };

    CorrelationMeter (anamorph::CorrelationMeter& src, Orientation o);
    ~CorrelationMeter() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    anamorph::CorrelationMeter& source;
    Orientation orientation;
    float value = 1.0f;   // smoothed display value

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CorrelationMeter)
};

} // namespace anamorph::gui
