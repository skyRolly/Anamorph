#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../dsp/ScopeBuffer.h"

namespace anamorph::gui
{

// ============================================================================
//  SpectrumImager  (Advanced Mode -- replaces the rotary Multiband)
//
//  A FabFilter Pro-Q 4 / Ozone Imager-style band editor: a live FFT spectrum
//  with THREE draggable crossover handles splitting it into FOUR bands, each
//  band carrying its own stereo width set by dragging UP (wider) / DOWN
//  (narrower) inside the band -- no knobs. Reads the lock-free ScopeBuffer for
//  the analyser and drives the seven Imager parameters directly (so automation
//  and undo work). Double-click a crossover or a band to reset it.
// ============================================================================
class SpectrumImager : public juce::Component,
                       public juce::SettableTooltipClient,
                       private juce::Timer
{
public:
    SpectrumImager (anamorph::ScopeBuffer& scope, juce::AudioProcessorValueTreeState& apvts);
    ~SpectrumImager() override;

    void paint (juce::Graphics&) override;

    void mouseMove      (const juce::MouseEvent&) override;
    void mouseExit      (const juce::MouseEvent&) override;
    void mouseDown      (const juce::MouseEvent&) override;
    void mouseDrag      (const juce::MouseEvent&) override;
    void mouseUp        (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void pushFFT();

    // --- geometry helpers (component-local) ------------------------------
    float freqToX (float hz) const noexcept;
    float xToFreq (float x)  const noexcept;
    float widthToY (float w) const noexcept;
    float yToWidth (float y) const noexcept;
    juce::Rectangle<float> plot() const noexcept; // the graph area inside the frame

    // current plain values pulled from the parameters
    float crossover (int i) const noexcept; // 0..2
    float bandWidth (int i) const noexcept; // 0..3
    int   bandAtX (float x) const noexcept; // which of the 4 bands a column is in
    int   handleNearX (float x) const noexcept; // crossover handle under the cursor, or -1

    void beginGesture (juce::RangedAudioParameter*);
    void setParam (juce::RangedAudioParameter*, float plain);
    void endGesture (juce::RangedAudioParameter*);

    anamorph::ScopeBuffer& scope;
    juce::AudioProcessorValueTreeState& apvts;

    juce::RangedAudioParameter* freqP[3]  { nullptr, nullptr, nullptr };
    juce::RangedAudioParameter* widthP[4] { nullptr, nullptr, nullptr, nullptr };

    // FFT analyser (GUI thread)
    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder; // 2048
    juce::dsp::FFT  fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> fifoL, fifoR, fftData, mags; // mags: 0..fftSize/2 smoothed dB
    double sampleRate = 48000.0;

    int   dragHandle = -1; // crossover 0..2 being dragged, else -1
    int   dragBand   = -1; // band 0..3 being width-dragged, else -1
    int   hoverHandle = -1;
    int   hoverBand   = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumImager)
};

} // namespace anamorph::gui
