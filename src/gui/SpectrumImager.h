#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../dsp/ScopeBuffer.h"

namespace anamorph::gui
{

// ============================================================================
//  SpectrumImager  (Advanced Mode -- the "Multiband" display)
//
//  An Ozone-Imager / FabFilter Pro-Q style band editor: a live FFT spectrum split
//  by up to three draggable crossover handles into 1..4 bands, each with its own
//  stereo width and a per-band solo. Drives the parameters directly so automation,
//  undo and A/B all track.
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
    juce::Rectangle<float> plot() const noexcept;
    float freqToX (float hz) const noexcept;
    float xToFreq (float x)  const noexcept;
    float widthToY (float w) const noexcept;
    float yToWidth (float y) const noexcept;
    float rulerY()  const noexcept;
    float laneTop() const noexcept;
    float laneBot() const noexcept;

    int   bandCount()       const noexcept;
    float crossover (int i) const noexcept;
    float bandWidth (int i) const noexcept;
    float bandLeftX (int b) const noexcept;
    float bandRightX (int b) const noexcept;
    bool  enabled() const noexcept;
    int   soloBand() const noexcept;        // 0-based soloed band, or -1

    int   bandAtX (float x) const noexcept;
    int   handleNearX (float x) const noexcept;
    bool  nearWidthLine (juce::Point<float> p, int b) const noexcept;
    juce::Rectangle<float> deleteBox (int b) const noexcept;   // x to remove a band (bottom-left)
    juce::Rectangle<float> soloBox (int b) const noexcept;     // headphone solo, top-centre
    juce::Rectangle<float> numberChip (int i) const noexcept;
    int   deleteHit (juce::Point<float>) const noexcept;       // band whose x is under the cursor
    int   soloHit (juce::Point<float>) const noexcept;         // band whose headphone is under the cursor

    float magForColumn (float xa, float xb) const noexcept;
    float magCubic (float bin) const noexcept;

    int   addBandAt (float hz);
    void  removeBand (int b);
    void  resetCrossover (int i);
    void  toggleSolo (int b);

    void beginGesture (juce::RangedAudioParameter*);
    void setParam (juce::RangedAudioParameter*, float plain);
    void endGesture (juce::RangedAudioParameter*);
    void resetParam (juce::RangedAudioParameter*);
    void setBands (int n);

    void updateHover (juce::Point<float>);

    void openFreqEditor (int i);
    void commitFreqEditor();
    void closeFreqEditor();
    static float parseFreq (const juce::String&);

    anamorph::ScopeBuffer& scope;
    juce::AudioProcessorValueTreeState& apvts;

    juce::RangedAudioParameter* bandsP  { nullptr };
    juce::RangedAudioParameter* soloP   { nullptr };
    juce::RangedAudioParameter* freqP[3]  { nullptr, nullptr, nullptr };
    juce::RangedAudioParameter* widthP[4] { nullptr, nullptr, nullptr, nullptr };
    std::atomic<float>* animOnP  { nullptr };
    std::atomic<float>* enableP  { nullptr };

    std::unique_ptr<juce::TextEditor> freqEditor;
    int editingHandle = -1;

    static constexpr int fftOrder = 13;
    static constexpr int fftSize  = 1 << fftOrder; // 8192
    juce::dsp::FFT  fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> fifoL, fifoR, fftData, mags;
    double sampleRate = 48000.0;

    int   dragHandle = -1;
    int   dragBand   = -1;
    int   hoverHandle = -1;
    int   hoverWidth  = -1;
    int   hoverAdd    = -1;
    int   hoverDelete = -1;  // band whose delete x is showing
    int   hoverSolo   = -1;  // band whose headphone is hovered
    float addX        = 0.0f;

    int   scrollHandle = -1;
    int   scrollBand   = -1;
    juce::Point<float> scrollAnchor;

    // Eased hover / press / state.
    float handleA[3] { 0, 0, 0 };
    float pressA[3]  { 0, 0, 0 };   // extra feedback while actually dragging a split (#5)
    float widthA[4]  { 0, 0, 0, 0 };
    float delA[4]    { 0, 0, 0, 0 };
    float soloA[4]   { 0, 0, 0, 0 };
    float addA       = 0.0f;
    float enaA       = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumImager)
};

} // namespace anamorph::gui
