#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/LevelMeters.h"

namespace anamorph::gui
{

// ============================================================================
//  LevelMeter
//
//  Default-hidden Input/Output L/R level meter (#10): four vertical bars showing
//  RMS (filled) with a held PEAK tick, on a dBFS scale. Reads the audio-thread
//  anamorph::LevelMeters via atomics on a timer.
// ============================================================================
class LevelMeter : public juce::Component,
                   private juce::Timer
{
public:
    explicit LevelMeter (anamorph::LevelMeters& src);
    ~LevelMeter() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    void drawBar (juce::Graphics&, juce::Rectangle<float>,
                  float slowDb, float rmsDb, float peakDb, const juce::String& lab);
    void drawReadout (juce::Graphics&, juce::Rectangle<float>,
                      const juce::String& lab, float valueDb, bool dim);

    anamorph::LevelMeters& source;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
};

} // namespace anamorph::gui
