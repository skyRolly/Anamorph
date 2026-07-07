#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/LevelMeters.h"
#include <array>

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

    // Click any number to reset the held peak / clip colours (#16).
    void mouseDown (const juce::MouseEvent&) override { source.resetHold(); }

private:
    void timerCallback() override;
    void visibilityChanged() override;
    void drawBar (juce::Graphics&, juce::Rectangle<float>,
                  float dimDb, float briDb, float barDb);
    void drawNumber (juce::Graphics&, juce::Rectangle<float>, float valueDb,
                     bool peak, bool clip);

    anamorph::LevelMeters& source;

    // S3 repaint gate: everything paint() reads from the audio-side meters, as
    // last drawn. The component holds no other dynamic state, so a tick whose
    // snapshot matches the last one cannot change the frame.
    std::array<float, 28> shown {};
    bool shownValid = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
};

} // namespace anamorph::gui
