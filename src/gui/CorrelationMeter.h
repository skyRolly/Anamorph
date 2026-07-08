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

private:
    void timerCallback() override;
    void visibilityChanged() override;

    anamorph::CorrelationMeter& source;
    Orientation orientation;
    Type        type;
    float value = 0.0f;          // smoothed display value
    float shownValue = 1.0e9f;   // value at the last repaint request (S3 gate)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoMeter)
};

} // namespace anamorph::gui
