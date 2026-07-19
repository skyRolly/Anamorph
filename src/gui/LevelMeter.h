#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/LevelMeters.h"
#include "FrameClock.h"
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
class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (anamorph::LevelMeters& src);
    ~LevelMeter() override;

    void paint (juce::Graphics&) override;

    // Click any number to reset the held peak / clip colours (#16).
    void mouseDown (const juce::MouseEvent&) override { source.resetHold(); }

    // The cached static layer bakes look-dependent drawing, so any look change
    // must drop it; the next paint() rebuilds at the current size/scale (H2/H13).
    void lookAndFeelChanged() override { staticLayer = {}; }

private:
    // Shared geometry (Wave 4): one source of truth for the row/column/bar/ruler
    // maths that the static-layer renderer and the live paint() both consume, so
    // the two can never drift. Values reproduce the former in-paint arithmetic
    // exactly (same removeFromTop sequence, same column expressions).
    struct Layout
    {
        juce::Rectangle<float> bounds;               // full component
        juce::Rectangle<float> header, sub, pkRow, rmRow;
        juce::Rectangle<float> bars;                 // remaining area: the four bars
        juce::Rectangle<float> ruler;
        float colW = 0.0f;
        float colXs[4] { 0.0f, 0.0f, 0.0f, 0.0f };  // column left edges
        float barW = 0.0f;
        juce::Rectangle<float> bar (int i) const noexcept
        {
            return { colXs[i] + (colW - barW) * 0.5f, bars.getY(), barW, bars.getHeight() };
        }
    };
    Layout computeLayout() const noexcept;

    void tick(); // FrameClock callback (display-rate; ballistics are all audio-side)
    void visibilityChanged() override;
    void ensureStaticLayer (juce::Graphics&, const Layout&);
    void drawBarDynamic (juce::Graphics&, juce::Rectangle<float>,
                         float dimDb, float briDb, float barDb);
    void drawNumber (juce::Graphics&, juce::Rectangle<float>, float valueDb,
                     bool peak, bool clip);

    anamorph::LevelMeters& source;

    // Cached static layer (Wave 4 -- the H13/H2 recipe; opaque per N2): the
    // glass panel, IN/OUT + L/R headers, the four recessed bar slots (gradient
    // + faint ticks) and the centre dB ruler -- pure functions of (size,
    // physical scale, look) -- rendered once into an opaque RGB image at
    // physical resolution (corners pre-filled with the editor's flat
    // colours::bg backdrop) and copy-blitted by paint(). Signal-dependent
    // drawing (numbers, bar fills, peak blocks) is never cached, and the bar
    // glass edges stay live so their draw order over the fills is preserved.
    juce::Image staticLayer;
    int   staticW = 0, staticH = 0;
    float staticScale = 0.0f;

    // S3 repaint gate: everything paint() reads from the audio-side meters, as
    // last drawn. The component holds no other dynamic state, so a tick whose
    // snapshot matches the last one cannot change the frame.
    std::array<float, 28> shown {};
    bool shownValid = false;

    // Adaptive refresh (display-rate, capped ~120 Hz): started only while shown
    // (default-hidden meter), exactly like the 60 Hz timer it replaces. The
    // bitwise snapshot gate is rate-independent, so no dt correction is needed.
    FrameClock frameClock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
};

} // namespace anamorph::gui
