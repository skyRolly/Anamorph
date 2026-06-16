#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../dsp/ScopeBuffer.h"

namespace anamorph::gui
{

// ============================================================================
//  SpectrumImager  (Advanced Mode -- the "Multiband" display)
//
//  An Ozone-Imager / FabFilter Pro-Q style band editor: a live FFT spectrum on a
//  long horizontal panel, split by up to THREE draggable crossover handles into
//  1..4 bands, each carrying its own stereo width (drag UP = wider / DOWN =
//  narrower inside the band). Bands are added by clicking the top "add" strip and
//  removed via the per-band x; scrolling fine-tunes a split (on a line) or a
//  band's width (off a line). Reads the lock-free ScopeBuffer for the analyser
//  and drives the parameters directly, so automation, undo and A/B all track.
//
//  Hover / press states ease in and out (the same non-linear fade as the rest of
//  the UI) when UI Animations are on, and snap when they are off.
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
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;
    void pushFFT();

    // --- geometry helpers (component-local) ------------------------------
    juce::Rectangle<float> plot() const noexcept; // graph area inside the frame
    float freqToX (float hz) const noexcept;
    float xToFreq (float x)  const noexcept;
    float widthToY (float w) const noexcept;
    float yToWidth (float y) const noexcept;
    float yThird()  const noexcept;   // the "add band" line, 1/3 down
    float rulerY()  const noexcept;   // baseline for the frequency numbers
    float laneTop() const noexcept;   // width = 2.0 maps here
    float laneBot() const noexcept;   // width = 0.0 maps here

    // current plain values pulled from the parameters
    int   bandCount()       const noexcept; // 1..4
    float crossover (int i) const noexcept; // 0..2
    float bandWidth (int i) const noexcept; // 0..3
    float bandLeftX (int b) const noexcept;
    float bandRightX (int b) const noexcept;
    juce::Rectangle<float> deleteRect (int b) const noexcept;

    int   bandAtX (float x) const noexcept;     // which of the active bands a column is in
    int   handleNearX (float x) const noexcept; // active crossover under the cursor, or -1
    int   deleteHit (juce::Point<float>) const noexcept; // band whose x is under the cursor, or -1

    // spectrum magnitude (dB) for the pixel column [xa, xb], averaging / interpolating bins
    float magForColumn (float xa, float xb) const noexcept;

    // structural edits (write the parameters, engine ducks the swap)
    void addBandAt (float hz);
    void deleteBand (int b);

    void beginGesture (juce::RangedAudioParameter*);
    void setParam (juce::RangedAudioParameter*, float plain);
    void endGesture (juce::RangedAudioParameter*);
    void resetParam (juce::RangedAudioParameter*);
    void setBands (int n);

    void updateHover (juce::Point<float>);

    anamorph::ScopeBuffer& scope;
    juce::AudioProcessorValueTreeState& apvts;

    juce::RangedAudioParameter* bandsP  { nullptr };
    juce::RangedAudioParameter* freqP[3]  { nullptr, nullptr, nullptr };
    juce::RangedAudioParameter* widthP[4] { nullptr, nullptr, nullptr, nullptr };
    std::atomic<float>* animOnP { nullptr };

    // FFT analyser (GUI thread). A large window keeps the LOW end resolved so the
    // bottom octaves stop looking blocky / stair-stepped (0.6.6 #8).
    static constexpr int fftOrder = 13;
    static constexpr int fftSize  = 1 << fftOrder; // 8192
    juce::dsp::FFT  fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> fifoL, fifoR, fftData, mags; // mags: 0..fftSize/2 smoothed dB
    double sampleRate = 48000.0;

    int   dragHandle = -1;   // crossover 0..2 being dragged, else -1
    int   dragBand   = -1;   // band 0..3 being width-dragged, else -1
    int   hoverHandle = -1;
    int   hoverBand   = -1;
    int   hoverDelete = -1;  // band whose x affordance is showing
    bool  hoverAdd    = false; // cursor is in the top "add" strip (and bands < 4)
    float addX        = 0.0f;  // cursor x for the add hint

    // Scroll latches a target (the split or band under the cursor) and KEEPS it
    // until the pointer next moves, so a split scrolled out from under the cursor
    // still responds to the wheel (0.6.6 #5).
    int   scrollHandle = -1;
    int   scrollBand   = -1;

    // Eased hover/press activity (0.6.6 #11): one per crossover, per band, plus the
    // add strip and each band's delete affordance.
    float handleA[3] { 0, 0, 0 };
    float bandA[4]   { 0, 0, 0, 0 };
    float delA[4]    { 0, 0, 0, 0 };
    float addA       = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumImager)
};

} // namespace anamorph::gui
