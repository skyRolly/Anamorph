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
//  1..4 bands, each carrying its own stereo width.
//
//  Interaction (0.6.7):
//    * Drag a split to move it; click-drag in empty band space ADDS a split and
//      keeps dragging it.
//    * Hover a band (away from its width line) -> a "+" add hint; hover the width
//      line -> drag it up/down for the band width.
//    * Hover a split -> a x appears to its right to remove it.
//    * Scroll a split = frequency; scroll a band = width (1 %/notch); the wheel
//      latches its target until the pointer really moves.
//    * Double-click a split's number to TYPE a frequency; double-click / Alt-click
//      elsewhere resets.
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

    // current plain values pulled from the parameters
    int   bandCount()       const noexcept; // 1..4
    float crossover (int i) const noexcept; // 0..2
    float bandWidth (int i) const noexcept; // 0..3
    float bandLeftX (int b) const noexcept;
    float bandRightX (int b) const noexcept;
    bool  enabled() const noexcept;

    int   bandAtX (float x) const noexcept;
    int   handleNearX (float x) const noexcept;
    bool  nearWidthLine (juce::Point<float> p, int b) const noexcept;
    juce::Rectangle<float> deleteBox (int i) const noexcept;   // the x to a split's right
    juce::Rectangle<float> numberChip (int i) const noexcept;  // the freq readout / edit box
    int   deleteHit (juce::Point<float>) const noexcept;

    // spectrum magnitude (dB) for the pixel column [xa, xb]
    float magForColumn (float xa, float xb) const noexcept;
    float magCubic (float bin) const noexcept;

    // structural / value edits (engine ducks the swap)
    int   addBandAt (float hz);     // returns the NEW crossover index, or -1
    void  removeCrossover (int i);
    void  resetCrossover (int i);

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
    juce::RangedAudioParameter* freqP[3]  { nullptr, nullptr, nullptr };
    juce::RangedAudioParameter* widthP[4] { nullptr, nullptr, nullptr, nullptr };
    std::atomic<float>* animOnP  { nullptr };
    std::atomic<float>* enableP  { nullptr };

    std::unique_ptr<juce::TextEditor> freqEditor;
    int editingHandle = -1;

    // FFT analyser (GUI thread). A large window + cubic interpolation keeps the LOW
    // end smooth (0.6.7 #11).
    static constexpr int fftOrder = 13;
    static constexpr int fftSize  = 1 << fftOrder; // 8192
    juce::dsp::FFT  fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> fifoL, fifoR, fftData, mags;
    double sampleRate = 48000.0;

    int   dragHandle = -1;
    int   dragBand   = -1;
    int   hoverHandle = -1;
    int   hoverWidth  = -1;  // band whose width line is hovered
    int   hoverAdd    = -1;  // band showing the add hint
    int   hoverDelete = -1;  // crossover showing its delete x
    float addX        = 0.0f;

    // Scroll latches its target and holds it until the pointer really moves (#4).
    int   scrollHandle = -1;
    int   scrollBand   = -1;
    juce::Point<float> scrollAnchor;

    // Eased hover/press activity (#11 / 0.6.6 #11).
    float handleA[3] { 0, 0, 0 };
    float widthA[4]  { 0, 0, 0, 0 };
    float delA[3]    { 0, 0, 0 };
    float addA       = 0.0f;
    float enaA       = 1.0f; // eased enabled (1) / disabled (0) wash (#20)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumImager)
};

} // namespace anamorph::gui
