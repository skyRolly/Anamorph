#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../dsp/ScopeBuffer.h"
#include "FrameClock.h"

namespace anamorph::gui
{

// ============================================================================
//  SpectrumImager  (Advanced Mode -- the "Multiband" display)
//
//  An Ozone-Imager / FabFilter Pro-Q style band editor: a live FFT spectrum split
//  by up to three draggable crossover handles into 1..4 bands, each with its own
//  stereo width and a per-band solo. Drives the parameters directly so automation,
//  undo and A/B all track. Solo is a mask, so any combination of bands can be
//  auditioned at once (0.6.9 #7).
// ============================================================================
class SpectrumImager : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    SpectrumImager (anamorph::ScopeBuffer& scope, juce::AudioProcessorValueTreeState& apvts);
    ~SpectrumImager() override;

    void paint (juce::Graphics&) override;

    // The cached bottom layer bakes look-dependent drawing, so any look change
    // must drop it; the next paint() rebuilds at the current size/scale (H17).
    void lookAndFeelChanged() override { bottomLayer = {}; blW = 0; }

    void mouseMove      (const juce::MouseEvent&) override;
    void mouseExit      (const juce::MouseEvent&) override;
    void mouseDown      (const juce::MouseEvent&) override;
    void mouseDrag      (const juce::MouseEvent&) override;
    void mouseUp        (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // Release-outside safety net (v0.8.12): called by the editor's 24 Hz reconcile when the
    // physical mouse button is up but a drag is still active (a mouseUp lost outside the plugin
    // window). Ends any open gesture + clears the drag flags without firing on-release actions.
    void cancelActiveDrag();

    // Abandons an in-progress inline crossover-frequency edit WITHOUT applying it -- the Escape-key
    // outcome, reached without the user pressing Escape. `freqEditor->onFocusLost` commits, which is
    // right for "the user clicked elsewhere" but wrong for a focus release the EDITOR decides on
    // (leaving the application with a menu open); that would write a half-typed crossover, complete
    // with a change gesture and a nudge to the neighbouring splits. A caller releasing focus for a
    // reason the user did not initiate must call this first. Harmless when nothing is being edited.
    void cancelInlineEdit() { if (editingHandle >= 0) closeFreqEditor(); }

    // Wired up by the editor. The momentary solo audition is a non-undoable engine
    // override (#8); onSweep / isSweeping share the editor's eased-position window so
    // a reset / preset / A-B / undo travels the split & width lines (#1).
    std::function<void(int)> onSoloPreview;
    std::function<void()>    onClearSoloPreview;
    std::function<void()>    onSweep;
    std::function<bool()>    isSweeping;

    // UI-animation flag now lives in InternalState (host-hidden), injected by the editor.
    void setAnimationSource (const std::atomic<float>* p) noexcept { animOnP = p; }

private:
    void tick (double dt); // FrameClock callback (display-rate; dt-corrected eases/decays)
    void visibilityChanged() override; // Advanced-only: no vblank ticks while hidden (Simple mode)
    bool pushFFT();        // runs the FFT only when the window changed; true = new magnitudes
    void runTransform();   // the unchanged mix + Hann + transform body

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
    int   soloMask() const noexcept;           // 4-bit solo mask
    bool  bandSoloed (int b) const noexcept;

    // ADR-0046: `n` is the topology to answer under. A caller that has already read the count,
    // and is going to stamp an index with it, MUST pass it -- otherwise these re-read the live
    // count and can answer under a topology the caller never proved. -1 means "read it yourself",
    // which is right for the hover/paint callers that prove nothing and stamp nothing.
    int   bandAtX (float x, int n = -1) const noexcept;
    int   handleNearX (float x, int n = -1) const noexcept;
    bool  nearWidthLine (juce::Point<float> p, int b) const noexcept;
    juce::Rectangle<float> deleteBox (int b) const noexcept;   // x to remove a band (bottom-left)
    juce::Rectangle<float> soloBox (int b) const noexcept;     // headphone solo, top-centre
    juce::Rectangle<float> numberChip (int i) const noexcept;
    int   deleteHit (juce::Point<float>) const noexcept;       // band whose x is under the cursor
    int   soloHit (juce::Point<float>) const noexcept;         // band whose headphone is under the cursor

    float magForColumn (float xa, float xb) const noexcept;
    float magCubic (float bin) const noexcept;
    // S12 paint LUTs: xToFreqCached returns the exact xToFreq(x) -- from the LUT
    // when x lands on the cached half-pixel grid at the built geometry, else the
    // live bisection -- so callers are byte-identical. ensurePaintLUTs (re)builds
    // both LUTs when the plot geometry or sample rate changes.
    float xToFreqCached (float x) const noexcept;
    void  ensurePaintLUTs();

    // Minimum band spacing in pixels: a constant on-screen gap (the same width at
    // every point of the warped ruler). The only limit on a split is this gap; a
    // split may otherwise be dragged anywhere, pushing its neighbours aside, and a
    // crowded insert spreads the neighbours apart (0.6.10 #1/#25/#26).
    void  projectGaps (float* xs, int count, int pin) const noexcept;
    // Seeds `dragOrigX` for a gesture that is about to start. EVERY slot, never just the
    // splits in use: the consumers re-read a LIVE `bandCount()`, so a host raising Bands
    // mid-drag makes them read entries a "0 .. splits-in-use" capture never wrote. Call
    // this at every drag start rather than writing the loop again -- one home for the rule.
    void  captureDragOrigins() noexcept;
    void  captureGestureSound() noexcept;
    // ADR-0041. Store, then confirm IN PARAMETER SPACE that this store is what the parameter now
    // holds, and hand back the value to own. False means somebody wrote from inside the store.
    bool  storeOwned (juce::RangedAudioParameter* p, float plain, float& ownedNorm) noexcept;
    // Reversible projection: pin one or two splits at target x, keep every OTHER split
    // as close to its drag-start position (orig) as the min gap allows, so a pushed
    // neighbour springs back when the pin moves away again (0.6.13 #8/#9/#10/#11).
    void  projectFromOrig (float* out, const float* orig, int count,
                           int pinA, float xA, int pinB, float xB) const noexcept;
    // ADR-0040: these return FALSE when the gesture lost ownership part-way through the burst, so
    // the caller abandons the rest of the event and voids the gesture. `true` means every store this
    // pass made was one the gesture still owned.
    bool  writeCrossovers (const float* xs, int count);
    // ADR-0042: the neighbour spread of a reset or a text commit, applied only to the world the plan
    // was computed from. `was` holds each split's normalised value at plan time; a slot that no
    // longer holds it belongs to somebody else and the spread stops there. Returns false then.
    // `pinNorm` is the normalised value the PIN was just confirmed to hold: every position in `xs`
    // was computed to make room for it, so a spread that carries on after the pin has moved applies
    // its plan around a split that is no longer there (ADR-0043).
    bool  spreadSplits (const float* xs, const float* was, int count, int except, float pinNorm);
    // ADR-0046: `n` is the topology the caller proved; it sizes the plan so the burst's extent
    // and the per-store proof inside `writeCrossovers` come from ONE reading. -1 = read it here.
    bool  dragCrossoverTo (int handle, float x, int n = -1);
    bool  bandAddTarget (int b, float x, float& outX) const noexcept;

    // `resultingBands` reports the count this add ESTABLISHED, taken from the same read of
    // `bandCount()` the insertion was computed against -- so the caller's gesture snapshot
    // cannot pick up a different count from a second, later read (ADR-0039).
    int   addBandAt (float hz, int& resultingBands);
    // `expectedBands` is the topology the caller validated its index against. removeBand reads
    // the live count ONCE and refuses unless it is still that one: the check and the operation
    // then share a single read, which is the only way to stop a stale index being applied to a
    // topology the user never saw (ADR-0039).
    void  removeBand (int b, int expectedBands);
    void  resetCrossover (int i);

    // Display-eased positions (#1): split frequencies and band widths the PAINT uses,
    // which travel to a new value on a reset / preset / A-B / undo and otherwise snap.
    float dispCrossover (int i) const noexcept;
    float dispWidth (int b) const noexcept;
    float dispLeftX (int b) const noexcept;
    float dispRightX (int b) const noexcept;

    // Solo (mask) ---------------------------------------------------------
    // ADR-0041: these RETURN whether they committed. A conditional store that refuses silently is
    // indistinguishable from one that succeeded, and both topology transactions used to carry on
    // after the mask store had been refused -- changing the band count with the mask still in the
    // old numbering. A caller that derived state from a precondition must hear the refusal.
    bool  setSoloMask (int mask, int expectedBands = -1, int expectedMask = -1);
    bool  toggleSoloBit (int b, int expectedBands = -1);
    int   effectiveSoloMask() const noexcept; // includes the momentary hold preview
    void  beginBandMove (int b);            // drag a solo handle sideways to move the band (0.6.9 #9)
    bool  moveBand (float mouseX);
    void  endBandMove();

    void beginGesture (juce::RangedAudioParameter*);
    // ADR-0046: `expectedBands` refuses a store whose topology has moved since the caller
    // proved it -- the same contract `resetParam` carries. -1 for the callers that have
    // just proved the count with nothing in between.
    void setParam (juce::RangedAudioParameter*, float plain, int expectedBands = -1);
    void endGesture (juce::RangedAudioParameter*);
    // ADR-0045: `expectedBands` proves the topology between this reset's own gesture open and
    // its store; -1 for callers with no band index to prove.
    void resetParam (juce::RangedAudioParameter*, int expectedBands = -1);
    // ADR-0044: `expectedMask` proves the solo word INSIDE the count store's own gesture bracket.
    // -1 means "do not prove it". The count is what reinterprets the mask, and
    // `beginChangeGesture` dispatches before the guard, so no caller-side check can cover it.
    bool setBands (int n, int expectedBands = -1, int expectedMask = -1);

    void updateHover (juce::Point<float>);
    void setContextTooltip();               // per-control tooltip (0.6.9 #18)
    // A smooth, symmetric glow along a path: many overlapping rounded strokes with a
    // Gaussian-ish falloff -- centred on the path (no offset), continuous (no hard band)
    // (0.6.15 #2/#3).
    void softGlow (juce::Graphics&, const juce::Path&, juce::Colour, float intensity, float maxWidth) const;

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
    const std::atomic<float>* animOnP  { nullptr }; // UI-animation flag (InternalState, host-hidden)
    std::atomic<float>* enableP  { nullptr };

    std::unique_ptr<juce::TextEditor> freqEditor;
    int editingHandle = -1;
    // ADR-0043: did the user actually change the text? `openFreqEditor` seeds the box from the live
    // split, so a commit with neither of these true is a store of a value nobody asked for -- and,
    // if a host moved that split while the box was open, a store of a value from before it did.
    // TWO signals, because one of them is asynchronous: `onTextChange` is delivered through
    // `postCommandMessage` (juce_TextEditor.cpp:594-599), which is reliable in a running message
    // loop and catches even a retyped identical string, but never arrives without one. The text
    // comparison is synchronous and needs no message loop. Either is sufficient.
    bool editTextEdited = false;
    juce::String editOpenText;

    static constexpr int fftOrder = 13;
    static constexpr int fftSize  = 1 << fftOrder; // 8192
    juce::dsp::FFT  fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> fifoL, fifoR, fftData, mags;
    // Per-bin dBFS of the CURRENT fftData (Wave 4): gainToDecibels is a pure
    // function of fftData[k] (norm is a compile-time constant of fftSize), so
    // it is converted once per new transform -- pushFFT() true, the only writer
    // of fftData -- instead of per decay tick. The release tail after audio
    // stops used to re-run ~4k log10s per tick on identical input.
    std::vector<float> magsDb;
    std::vector<float> redLevel; // per-bin clip-glow level, temporally smoothed (#4)
    std::vector<float> redColX;  // per-pixel clip level, horizontally feathered (0.6.16)
    double sampleRate = 48000.0;

    // S12 per-paint cost caches (never change rendered pixels -- see xToFreqCached).
    std::vector<float> xToFreqLUT;      // half-pixel grid: [i] = xToFreq(lutX0 + i*0.5)
    int    lutW = -1;                   // component getWidth() the LUT was built at
    float  lutX0 = 0.0f;                // r.getX() - 0.5, the LUT's first grid position
    std::vector<int>   redColBin;       // clip mapping: [xi] = FFT bin drawn at column xi
    double redBinSR = 0.0;              // sample rate redColBin was built for
    std::vector<float> clipBlurScratch; // reused triangular-blur buffer (was a per-paint vector)
    // Reused paint paths (Wave 4): Path::clear() keeps the allocated storage
    // (Array::clearQuick), so the ~one-point-per-pixel spectrum line and its
    // fill stop growing fresh heap blocks on every active paint. Same points,
    // same rendering -- only the allocation churn goes.
    juce::Path specPath, specFillPath, clipQuadPath;

    // Cached bottom layer (H17, the H2 recipe): the glass panel + band tints +
    // frequency-grid verticals -- everything painted BELOW the spectrum. Unlike
    // the scope/meter caches this layer is keyed on eased-but-snapping inputs
    // (panel hover wash, drawn splits/widths, width-hover washes, solo mask)
    // as well as size/scale/look: every one of those eases converges EXACTLY
    // onto its target (the 0.004 ease snap / the sub-pixel drawnF-drawnW snap),
    // so the key settles and steady-state paints are pure blits with ZERO
    // rebuilds; while something animates it rebuilds per frame, which costs
    // what the direct drawing always cost. Kept ARGB/translucent: the imager
    // sits on the editor's semi-transparent Multiband panel (NOT flat bg), so
    // opacity would need fragile parent replication -- proven unsafe, N2 does
    // not apply here. The image buffer is reused across same-size rebuilds.
    void ensureBottomLayer (juce::Graphics& g, juce::Rectangle<float> r);
    juce::Image bottomLayer;
    int    blW = 0, blH = 0;            // component size the layer was built at
    float  blScale = 0.0f;              // physical pixel scale it was built at
    float  blHover = -1.0f;             // panelHoverA baked into the panel colour
    int    blBands = -1, blMask = -1;   // band count + effective solo mask
    float  blF[3] { -1, -1, -1 };       // drawnF baked into tint edges / grid
    float  blDW[4] { -1, -1, -1, -1 };  // drawnW baked into tint colour/alpha
    float  blWA[4] { -1, -1, -1, -1 };  // widthA hover washes baked into tints

    // S2 idle gate state (message thread only -- see tick()/pushFFT).
    // Same freshness pattern as the Vectorscope's S1 gate, with the fixed
    // fftSize analysis window as the content window.
    std::uint64_t lastSeenCount = 0; // ring write count at the previous tick
    std::uint64_t lastNonZero   = 0; // newest bound on non-zero ring content
    bool lastWindowSilent = false;   // the FFT window was all-zero last tick
    bool magsSettled = false;        // mags smoothing reached its fixpoint
    bool redSettled  = false;        // redLevel decay fully drained
    bool frameDirty  = true;         // force one repaint (re-show, SR change)
    bool wasShowing  = false;        // visibility edge detection

    int   dragHandle = -1;
    int   dragBand   = -1;
    int   hoverHandle = -1;
    int   hoverWidth  = -1;
    int   hoverAdd    = -1;
    int   hoverDelete = -1;  // band the cursor is over (its delete x is dimly shown)
    int   hoverDeleteExact = -1; // band whose delete x is directly under the cursor (bright)
    int   hoverSolo   = -1;  // band whose headphone is hovered
    int   pressDeleteBand = -1; // band whose delete x is being held (delete fires on release) (0.6.16 #1/#2)
    float addX        = 0.0f;
    float dragGrabDX  = 0.0f;     // cursor-to-line offset while dragging a split (#10/#11)
    float dragOrigX[3] { 0, 0, 0 }; // split x positions at drag start, for the reversible projection
    float widthPressY   = 0.0f;   // cursor y at a Width grab, for the 3 px drag-engage threshold (v0.8.12)
    float dragGrabDY    = 0.0f;   // cursor-to-line offset while dragging a Width -> relative, no jump (v0.8.12)
    bool  widthHoldActive = false;// Width drag engaged past the 3 px threshold (click-vs-drag, v0.8.12)

    int   scrollHandle = -1;
    int   scrollBand   = -1;
    // ADR-0045: the band count the two latches above were taken in. A latch is a positional
    // identifier and is only meaningful in its own topology; -1 means nothing is latched.
    int   scrollBands  = -1;
    juce::Point<float> scrollAnchor;

    // Solo press machine (0.6.9 #8/#9): a quick click latches the band's solo
    // bit; a press-and-hold auditions THIS band alone (engine override, restored on
    // release); a hold-and-drag moves the band with rigid, anchor-based translation
    // so the split tracks the cursor 1:1 and a limited band keeps its width (0.6.12).
    // An Alt/Option quick click (0.8.10 exclusive-solo redesign): unsoloed band ->
    // solo THAT band exclusively; soloed band -> clear all solos.
    int   soloPressBand   = -1;
    juce::uint32 soloPressMs = 0;
    bool  soloPressAlt    = false;   // Alt/Option held at press -> all-bands action on release
    bool  soloHoldActive  = false;   // momentary audition engaged
    bool  soloMovedBand   = false;   // turned into a sideways band move
    int   soloMoveLeft    = -1;      // crossover index on the band's left edge (or -1)
    int   soloMoveRight   = -1;      // crossover index on the band's right edge (or -1)
    float soloDownX       = 0.0f;
    float bandAnchorX     = 0.0f;    // cursor x when the move began
    float bandStartLeftX  = 0.0f;    // band edge x positions at move start
    float bandStartRightX = 0.0f;
    float bandTmin = 0.0f, bandTmax = 0.0f; // clamped translation range

    bool  dragRemovePending = false; // dragged a split far outside -> drop it on release (#18)

    // ADR-0038. THE TOPOLOGY A GESTURE WAS DEFINED AGAINST. Every identifier above --
    // dragHandle, dragBand, soloPressBand, soloMoveLeft/Right, pressDeleteBand -- names a
    // split or a band by POSITION, and every one of them is latched when a press begins.
    // `bandCount()` is a LIVE read of mbBands, which a host can move at any instant, so
    // each of those names can stop meaning what it meant. Snapshotting the count once per
    // press, and voiding the whole gesture the moment the live one differs, is what makes
    // the identifiers safe as a set; validating them one consumer at a time provably is
    // not (the band move validated its pins and still wrote the unpinned splits from
    // drag-start origins). -1 = no gesture in progress.
    // ADR-0039 sharpens WHEN this is taken: with the identifiers, not at handler entry. The
    // add-area press CREATES a split and hands itself to it (`addBandAt` raises Bands, then
    // `dragHandle = idx`), so a snapshot taken at the top of mouseDown named a topology the
    // press itself had already left and the first mouseDrag cancelled the brand-new drag.
    // Taking it where the identifiers are latched separates the two classes BY CONSTRUCTION
    // rather than by asking who moved the count: a change the press performs is synchronous,
    // on this thread, and complete before the identifiers exist; anything later is external.
    int   gestureBands = -1;
    // ADR-0039. WHERE THIS GESTURE LAST LEFT EACH SPLIT, in pixels. `gestureBands` catches an
    // authoritative change that moves the COUNT; a whole-sound install at the same count moves
    // nothing it watches, and the drag then keeps projecting from `dragOrigX` -- the positions
    // of the sound that was just replaced -- and writes the unpinned splits back over it. The
    // drag's own writes refresh these (`writeCrossovers` stores THROUGH `gestureX[k]`), so a
    // difference means someone else wrote. TWO OF THE CLASS'S OWN STORES DO NOT REFRESH THEM:
    // `resetCrossover` and `spreadSplits` take the read-back into a local, so a gesture that
    // Alt-click-resets a split then reads its own store as foreign and cancels itself. That is the
    // safe direction and it costs one int store, not a frame of repaints (see the timer's comment);
    // it is recorded here because this paragraph used to say "every write", which is not true.
    // ADR-0041: the NORMALISED parameter value, not a pixel. A pixel was a GUI quantisation
    // tolerance doing an ownership job: half a display pixel is 0.30-0.65 % of the frequency
    // (0.19 Hz at 30 Hz, 32 Hz at 10 kHz, 61 Hz at 20 kHz on the harness's 902 px plot), while the
    // parameter has no interval at all and resolves about 0.00055 Hz at 1 kHz -- so an external,
    // fully automatable change of up to 61 Hz read as the gesture's own. Compared EXACTLY, which is
    // sound because `AudioParameterFloat::setValue` is `value = convertFrom0to1 (newValue)` and
    // `getValue()` is `convertTo0to1 (value)` (juce_AudioParameterFloat.cpp:97-98), so
    // `convertTo0to1 (convertFrom0to1 (norm))` is bit-identical to what a clean store leaves.
    float gestureX[3] { 0.0f, 0.0f, 0.0f };
    // ADR-0040. THE SAME, FOR WIDTHS. `gestureX` records splits only, so a width-only authoritative
    // change moved nothing the gesture watched: a width drag anchors `dragGrabDY` at the 3 px engage
    // and thereafter computes from the CURSOR alone -- the live width is never read again -- so an
    // outside write was overwritten by the next mouse move from an anchor taken before it. Measured:
    // `the installed width 1.700 was overwritten with 0.650`.
    float gestureW[4] { 0.0f, 0.0f, 0.0f, 0.0f }; // normalised too (ADR-0041)
    // ADR-0040. THE ONE QUESTION EVERY GESTURE STORE ASKS, adjacent to the store it guards. Exact
    // equality, not an epsilon: the gesture records the READ-BACK after each of its own stores, so
    // its own write can never read as somebody else's, and there is no epsilon to invent for a path
    // (the width drag) that has no write-suppression threshold of its own. `gestureBands < 0` means
    // nothing is in flight, and then nothing is owned and nothing is refused. ADR-0043: that is no
    // longer the wheel's state while it writes. `mouseWheelMove` ends any drag first, which clears
    // `gestureBands`, but its own burst is up to three stores with a host dispatch between each, so
    // it latches the count around `dragCrossoverTo` and clears it again immediately. The waiver is
    // for having no gesture, not for being the wheel.
    bool  ownsSplit (int k) const noexcept;
    bool  ownsWidth (int b) const noexcept;
    // True once the count has moved under an active gesture. Message thread only; a plain
    // comparison of two ints, no lock and no allocation -- the audio thread is not involved
    // in any of this and must never be.
    bool  topologyMovedUnderGesture() const noexcept
    { return gestureBands >= 0 && bandCount() != gestureBands; }
    // True once any split OR any width has moved under an active gesture. BOTH are compared in the
    // parameter's own normalised units, EXACTLY (ADR-0041) -- this comment used to describe the
    // splits as owned "by more than the amount `writeCrossovers` itself treats as a change", the
    // pixel tolerance that round removed, and a reader arriving here was told the opposite of what
    // `ownsSplit` does. `kSplitMovedPx` keeps its one real job, deciding whether a write is worth
    // making. The drag's own writes, read back through the same conversion and recorded at the
    // store, can never register as somebody else's (ADR-0039 for the splits, ADR-0040 for the
    // widths).
    bool  soundMovedUnderGesture() const noexcept;
    // The one question mouseDrag and mouseUp ask: is this gesture still defined against the
    // world it was latched in?
    bool  gestureIsStale() const noexcept
    { return topologyMovedUnderGesture() || soundMovedUnderGesture(); }

    // Crossover band-pass preview gate (0.8.1): the blue/green band-pass curve is a
    // PRESS-AND-HOLD affordance, exactly like the solo audition. A bare click, double-
    // click (reset), repeated clicks, programmatic/preset/A-B change must NOT flash it.
    // It lights only once a press is HELD past the threshold or turns into a drag.
    juce::uint32 handlePressMs   = 0;
    float        handlePressX    = 0.0f;
    bool         handleHoldActive = false;

    // Eased hover / press / state.
    float handleA[3] { 0, 0, 0 };
    float pressA[3]  { 0, 0, 0 };   // crossover actively dragged (curve + handle feedback)
    float widthA[4]  { 0, 0, 0, 0 };
    float pressW[4]  { 0, 0, 0, 0 }; // width line actively dragged (stronger than hover, #14)
    float delA[4]    { 0, 0, 0, 0 };
    float soloA[4]   { 0, 0, 0, 0 };
    float labelFlipA[4] { 0, 0, 0, 0 }; // 0 = % above the bar, 1 = below (top-collision flip, #3)
    float addA       = 0.0f;
    float enaA       = 1.0f;
    float soloCurveA = 0.0f;          // band-pass overlay while a solo is held/dragged (#15)
    int   soloCurveBand = -1;
    float panelHoverA = 0.0f;         // idle dims the panel; hover lifts it slightly (#23)

    // Display-eased split / width positions (#1).
    float drawnF[3] { 180.0f, 800.0f, 3000.0f };
    float drawnW[4] { 1.0f, 1.0f, 1.0f, 1.0f };
    int   lastBandCount = 4;
    bool  dispEasing = false; // a sweep is still gliding to its target -> keep easing, don't snap (#5)

    // Adaptive refresh (display-rate, capped ~120 Hz). Every temporal decay/ease
    // here (spectrum release, clip-glow rise/fall, hover/press/solo eases,
    // split/width glides) is dt-corrected so its time constant is identical on a
    // 60 or 120 Hz panel and matches the old 60 Hz curves to within the display
    // quantum (Class B) -- the H17 cache key still settles on the same distance-
    // based snaps. Runs while visible via the in-tick isShowing() gate (S2), so
    // no visibility stop/start is needed.
    FrameClock frameClock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumImager)
};

} // namespace anamorph::gui
