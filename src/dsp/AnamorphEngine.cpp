#include "AnamorphEngine.h"
#include "MidSide.h"

namespace anamorph
{

using juce::dsp::Oversampling;

// ---------------------------------------------------------------------------
//  Is oversampling actually doing work? Only when wrapping a nonlinear /
//  modulation stage (Drive, or Chorus / Dimension-D). Linear-only chains skip
//  oversampling entirely so they add ZERO latency (spec section 2.2 / 9).
// ---------------------------------------------------------------------------
static bool isModAlgorithm (Algorithm a) noexcept
{
    return a == Algorithm::Chorus || a == Algorithm::DimensionD;
}

static bool osActiveFor (const EngineParameters& e) noexcept
{
    return e.oversample != OversampleFactor::Off
        && (e.driveDb > 0.01f || isModAlgorithm (e.algorithm));
}

// ---------------------------------------------------------------------------
void AnamorphEngine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = juce::jmax (1, maxBlockSize);

    // --- sub-modules ---
    haas.prepare (sr, maxBlock);
    velvet.prepare (sr);
    chorus.prepare (sr * 8.0);          // sized for the highest OS rate (8x)
    multiband.prepare (sr, maxBlock);
    monoMaker.prepare (sr, maxBlock);
    loudness.prepare (sr);
    correlation.prepare (sr);
    levels.prepare (sr);

    // --- oversamplers (2x / 4x / 8x). Minimum-phase polyphase IIR: low latency,
    //     and crucially NO linear-phase pre-ringing / waveform misalignment.
    //     Integer latency requested so PDC is exact. ---
    using FT = Oversampling<float>::FilterType;
    os2 = std::make_unique<Oversampling<float>> (2, 1, FT::filterHalfBandPolyphaseIIR, true, true);
    os4 = std::make_unique<Oversampling<float>> (2, 2, FT::filterHalfBandPolyphaseIIR, true, true);
    os8 = std::make_unique<Oversampling<float>> (2, 3, FT::filterHalfBandPolyphaseIIR, true, true);
    os2->initProcessing ((size_t) maxBlock);
    os4->initProcessing ((size_t) maxBlock);
    os8->initProcessing ((size_t) maxBlock);
    latency2 = (int) std::round (os2->getLatencyInSamples());
    latency4 = (int) std::round (os4->getLatencyInSamples());
    latency8 = (int) std::round (os8->getLatencyInSamples());

    // --- smoothers ---
    const double ramp = 0.02; // 20 ms
    widthSmooth     .reset (sr, ramp);
    mixSmooth       .reset (sr, ramp);
    outGainSmooth   .reset (sr, ramp);
    matchGainSmooth .reset (sr, 0.12); // gentle so an A/B level-match swap glides (#16)
    balanceSmooth   .reset (sr, ramp);
    outBalanceSmooth.reset (sr, ramp);
    driveSmooth     .reset (sr, ramp);
    driveBlendSmooth.reset (sr, 0.015);
    monoDriveSmooth     .reset (sr, ramp);
    monoDriveBlendSmooth.reset (sr, 0.015);
    monoDriveSmooth     .setCurrentAndTargetValue (1.0f);
    monoDriveBlendSmooth.setCurrentAndTargetValue (0.0f);
    polLSmooth      .reset (sr, 0.005);
    polRSmooth      .reset (sr, 0.005);
    polLSmooth      .setCurrentAndTargetValue (1.0f);
    polRSmooth      .setCurrentAndTargetValue (1.0f);
    switchIncOut = 1.0f / (float) std::max (1.0, 0.006 * sr); // ~6 ms fade-out
    switchIncIn  = 1.0f / (float) std::max (1.0, 0.028 * sr); // ~28 ms fade-in
    widthSmooth     .setCurrentAndTargetValue (1.0f);
    mixSmooth       .setCurrentAndTargetValue (1.0f);
    outGainSmooth   .setCurrentAndTargetValue (1.0f);
    matchGainSmooth .setCurrentAndTargetValue (1.0f);
    balanceSmooth   .setCurrentAndTargetValue (0.0f);
    outBalanceSmooth.setCurrentAndTargetValue (0.0f);
    driveSmooth     .setCurrentAndTargetValue (1.0f);
    driveBlendSmooth.setCurrentAndTargetValue (0.0f);

    // --- dry-path delay (aligns dry to the wet OS latency) ---
    const int maxLat = juce::jmax (latency2, latency4, latency8);
    dryDelayBuffer.setSize (2, maxLat + maxBlock + 1);
    dryDelayBuffer.clear();
    dryDelayWrite = 0;

    // Phase-matched dry: same-size delay line + per-block scratch (Known Issue #1).
    dryAlignDelayBuffer.setSize (2, maxLat + maxBlock + 1);
    dryAlignDelayBuffer.clear();
    dryAlignScratch.setSize (2, maxBlock);

    dryScratch.setSize (2, maxBlock);
    wetScratch.setSize (2, maxBlock);
    inputScratch.setSize (2, maxBlock);
    monoLow.setSize (1, maxBlock);
    monoLowDry.setSize (1, maxBlock);
    monoLowDelay.setSize (1, maxLat + maxBlock + 1);
    monoLowDryDelay.setSize (1, maxLat + maxBlock + 1);
    monoLowDelay.clear();
    monoLowDryDelay.clear();
    monoLowWrite = 0;

    updateDerived();
    reset();
}

void AnamorphEngine::reset()
{
    haas.reset();
    velvet.reset();
    chorus.reset();
    multiband.reset();
    monoMaker.reset();
    loudness.reset();
    correlation.reset();
    levels.reset();
    if (os2) os2->reset();
    if (os4) os4->reset();
    if (os8) os8->reset();
    dryDelayBuffer.clear();
    dryAlignDelayBuffer.clear();
    dryDelayWrite = 0;
    monoLowDelay.clear();
    monoLowDryDelay.clear();
    monoLowWrite = 0;

    // Flush any in-flight switch duck straight to its target so a host reset
    // lands in a clean steady state (bit-exact transparent from sample 0).
    if (switchState != SwitchState::Normal)
    {
        p = pendingP;
        updateDerived();
    }
    pendingP = p;
    pendingAlgoReset = false;
    switchState = SwitchState::Normal;
    switchPhase = 1.0f;
    osEngaged = osActiveFor (p); // re-latch the OS wrap for the settled state (#3)
}

// ---------------------------------------------------------------------------
//  Discrete controls: changing any of these mid-stream would step the signal,
//  so they are swapped in only while the output is ducked to silence. Polarity
//  is excluded (it already ramps through zero via its own smoother).
// ---------------------------------------------------------------------------
bool AnamorphEngine::discreteDiffers (const EngineParameters& a, const EngineParameters& b) noexcept
{
    return a.channelMode      != b.channelMode
        || a.monoSum          != b.monoSum
        || a.swapLR           != b.swapLR
        || a.msMode           != b.msMode
        || a.solo             != b.solo
        || a.algorithm        != b.algorithm
        || a.haasSide         != b.haasSide
        || a.dimMode          != b.dimMode
        || a.mbEnable         != b.mbEnable
        || a.mbBands          != b.mbBands
        || a.mbSolo           != b.mbSolo
        || a.monoMakerEnable  != b.monoMakerEnable
        || a.autoGainMatch    != b.autoGainMatch
        || a.oversample       != b.oversample
        || a.bypass           != b.bypass
        // Engaging / disengaging the OS wrap (Drive crossing 0 with OS selected)
        // inserts/removes its group delay -- a discrete, duck-worthy change (#3).
        || osActiveFor (a)    != osActiveFor (b);
}

bool AnamorphEngine::processingDiffers (const EngineParameters& a, const EngineParameters& b) noexcept
{
    return a.channelMode != b.channelMode || a.monoSum  != b.monoSum  || a.swapLR   != b.swapLR
        || a.msMode      != b.msMode      || a.solo     != b.solo     || a.algorithm != b.algorithm
        || a.haasSide    != b.haasSide    || a.dimMode  != b.dimMode  || a.mbEnable  != b.mbEnable
        || a.mbBands     != b.mbBands
        || a.monoMakerEnable != b.monoMakerEnable || a.oversample != b.oversample;
}

void AnamorphEngine::copyContinuous (EngineParameters& dst, const EngineParameters& src) noexcept
{
    // Keep dst's discrete fields; pull every smoothed/continuous field from src.
    const auto cm = dst.channelMode; const auto ms = dst.monoSum; const auto sw = dst.swapLR;
    const auto md = dst.msMode;      const auto so = dst.solo;    const auto al = dst.algorithm;
    const auto hs = dst.haasSide;    const auto dm = dst.dimMode; const auto mb = dst.mbEnable;
    const auto mm = dst.monoMakerEnable; const auto ov = dst.oversample; const auto by = dst.bypass;
    const auto ag = dst.autoGainMatch;

    dst = src;

    dst.channelMode = cm; dst.monoSum = ms; dst.swapLR = sw; dst.msMode = md; dst.solo = so;
    dst.algorithm = al;   dst.haasSide = hs; dst.dimMode = dm; dst.mbEnable = mb;
    dst.monoMakerEnable = mm; dst.oversample = ov; dst.bypass = by; dst.autoGainMatch = ag;
}

void AnamorphEngine::setParameters (const EngineParameters& np) noexcept
{
    // A bulk swap (A/B, preset, undo) asks for a masking duck even when only
    // continuous controls move, and -- crucially -- is applied ENTIRELY at the
    // silent bottom (continuous included, smoothers snapped) so NOTHING can pop
    // mid-fade, not even an un-smoothed control or the Level-Match re-injection
    // (#1, 0.6.4/0.6.5 feedback).
    const bool forceDuck = duckRequest.exchange (0, std::memory_order_relaxed) != 0;

    if (switchState == SwitchState::Normal)
    {
        if (forceDuck)
        {
            // Keep the OLD state live through the fade-out; swap it all at silence.
            pendingP = np;
            pendingForced = true;
            pendingAlgoReset = (np.algorithm != p.algorithm);
            switchState = SwitchState::FadeOut;
        }
        else if (discreteDiffers (np, p))
        {
            pendingP = np;
            pendingAlgoReset = (np.algorithm != p.algorithm);
            copyContinuous (p, np);          // knobs respond immediately
            switchState = SwitchState::FadeOut;
            updateDerived();
        }
        else
        {
            p = np;                          // continuous-only change
            updateDerived();
        }
    }
    else
    {
        // Mid-duck: remember the latest target.
        pendingP = np;
        if (switchState == SwitchState::FadeIn && (forceDuck || discreteDiffers (np, p)))
        {
            // A new discrete change (or a forced bulk swap) arrived as we were
            // fading back in: duck again.
            pendingForced = pendingForced || forceDuck;
            pendingAlgoReset = (np.algorithm != p.algorithm);
            switchState = SwitchState::FadeOut;
        }
        // A forced swap defers everything to the silent bottom; otherwise keep
        // continuous controls live during the duck.
        if (! pendingForced)
        {
            copyContinuous (p, np);
            updateDerived();
        }
    }
}

// Snap every continuous smoother straight to its (new) target. Only called at the
// silent bottom of a forced duck, where it's inaudible -- so the post-fade-in
// state is already settled and a big level change never swells (#1).
void AnamorphEngine::snapSmoothers() noexcept
{
    auto snap = [] (juce::SmoothedValue<float>& s) { s.setCurrentAndTargetValue (s.getTargetValue()); };
    snap (widthSmooth);   snap (mixSmooth);        snap (outGainSmooth);
    snap (balanceSmooth); snap (outBalanceSmooth); snap (driveSmooth); snap (driveBlendSmooth);
    snap (monoDriveSmooth); snap (monoDriveBlendSmooth);
    snap (polLSmooth);    snap (polRSmooth);
    // matchGainSmooth is left to the injection / loudness re-measure (its own glide).
}

// The OS wrap follows the LATCHED engagement, not the live driveDb: both the
// path and its latency may only change at the silent duck bottom (#3).
juce::dsp::Oversampling<float>* AnamorphEngine::currentOversampler() noexcept
{
    if (! osEngaged) return nullptr;

    switch (p.oversample)
    {
        case OversampleFactor::x2: return os2.get();
        case OversampleFactor::x4: return os4.get();
        case OversampleFactor::x8: return os8.get();
        default:                   return nullptr;
    }
}

int AnamorphEngine::getLatencySamples() const noexcept
{
    if (! osEngaged) return 0;
    switch (p.oversample)
    {
        case OversampleFactor::x2: return latency2;
        case OversampleFactor::x4: return latency4;
        case OversampleFactor::x8: return latency8;
        default:                   return 0;
    }
}

int AnamorphEngine::predictLatency (const EngineParameters& e) const noexcept
{
    if (e.oversample == OversampleFactor::Off) return 0;
    if (! (e.driveDb > 0.01f || isModAlgorithm (e.algorithm))) return 0;
    switch (e.oversample)
    {
        case OversampleFactor::x2: return latency2;
        case OversampleFactor::x4: return latency4;
        case OversampleFactor::x8: return latency8;
        default:                   return 0;
    }
}

void AnamorphEngine::updateDerived()
{
    driveActive = p.driveDb > 0.01f;
    // Pre-gain for the waveshaper, plus a separate 0..1 blend that crossfades
    // from the clean signal over the first ~2 dB. This makes Drive identity at
    // 0 dB and removes the step that used to click when Drive first engaged
    // (feedback #13) -- the peak-preserving makeup is only mixed in gradually.
    driveSmooth.setTargetValue (juce::Decibels::decibelsToGain (juce::jmax (0.0f, p.driveDb)));
    driveBlendSmooth.setTargetValue (juce::jlimit (0.0f, 1.0f, p.driveDb / 2.0f));
    monoDriveSmooth.setTargetValue (driveSmooth.getTargetValue());
    monoDriveBlendSmooth.setTargetValue (driveBlendSmooth.getTargetValue());

    // Unified widening intensity -> every algorithm is identity at amount 0.
    // Each algorithm smooths the amount internally (click-free, #1).
    haas.setAmount   (p.algoAmount);
    velvet.setAmount (p.algoAmount);
    chorus.setAmount (p.algoAmount);
    haas.setDelayMs (p.haasDelayMs);
    // Haas side now means the PERCEIVED side (precedence): the sound leans to the
    // chosen side, so we delay the OPPOSITE channel (feedback #25).
    haas.setSide (p.haasSide == HaasSide::Left);
    velvet.setDensity (p.velvetDensity);

    polLSmooth.setTargetValue (p.polarityL ? -1.0f : 1.0f);
    polRSmooth.setTargetValue (p.polarityR ? -1.0f : 1.0f);

    if (p.algorithm == Algorithm::Chorus)
    {
        chorus.setVoice (ChorusEngine::Voice::Chorus);
        chorus.setRate  (p.chorusRate);
        chorus.setDepth (p.chorusDepth);
    }
    else if (p.algorithm == Algorithm::DimensionD)
    {
        chorus.setVoice (ChorusEngine::Voice::DimensionD);
        chorus.setDimMode (p.dimMode);
    }

    multiband.setBandCount (p.mbBands);
    multiband.setSolo (p.mbSolo);
    multiband.setCrossovers (p.mbFreqLow, p.mbFreqMid, p.mbFreqHigh);
    multiband.setWidths (p.mbWidthLow, p.mbWidthMid, p.mbWidthHiMid, p.mbWidthHigh);

    monoMaker.setFrequency (p.monoMakerFreq);

    widthSmooth     .setTargetValue (p.width);
    mixSmooth       .setTargetValue (p.mix);
    balanceSmooth   .setTargetValue (p.inputBalance);
    outBalanceSmooth.setTargetValue (p.outputBalance);
    outGainSmooth   .setTargetValue (juce::Decibels::decibelsToGain (p.outputGainDb));
    matchGainSmooth .setTargetValue (p.autoGainMatch
        ? juce::Decibels::decibelsToGain (loudness.getMatchGainDb())
        : 1.0f);
}

// ---------------------------------------------------------------------------
void AnamorphEngine::applyInputConditioning (float* L, float* R, int n) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        float l = L[i], r = R[i];

        switch (p.channelMode)
        {
            case ChannelMode::LeftOnly:  r = 0.0f; break;   // keep L, kill R
            case ChannelMode::RightOnly: l = 0.0f; break;   // keep R, kill L
            case ChannelMode::Stereo:    default: break;
        }

        // Swap acts on the raw input channels: L/R, or Mid<->Side when M/S is on.
        if (p.swapLR) { const float t = l; l = r; r = t; }

        // Advance the per-sample smoothers exactly once, whatever the routing.
        const float b  = balanceSmooth.getNextValue();   // centre = unity, turning attenuates the far side
        const float gL = (b > 0.0f) ? (1.0f - b) : 1.0f;
        const float gR = (b < 0.0f) ? (1.0f + b) : 1.0f;
        const float pL = polLSmooth.getNextValue();      // smoothed polarity sign, ramps +1<->-1 (no click)
        const float pR = polRSmooth.getNextValue();

        if (p.msMode)
        {
            // M/S DECODER (feedback #6): the input IS Mid/Side (Ch1 = Mid, Ch2 =
            // Side). Balance and Polarity act on Mid & Side IN the M/S domain
            // (#12/#13), then we decode to L/R, and only THEN does Mono sum the
            // decoded L/R (#14).
            l *= gL; r *= gR;                  // balance Mid vs Side
            l *= pL; r *= pR;                  // polarity of Mid / Side
            // Decode with the power-preserving 1/sqrt2 convention so the level is
            // balanced and clip-safe (reverted from the louder M+S form, #9).
            const float le = (l + r) * 0.70710678f, re = (l - r) * 0.70710678f;
            l = le; r = re;
            if (p.monoSum) { const float m = (l + r) * 0.5f; l = m; r = m; }
        }
        else
        {
            // L/R domain: Mono first so Balance still pans the summed signal (#14),
            // then Balance and Polarity on L/R.
            if (p.monoSum) { const float m = (l + r) * 0.5f; l = m; r = m; }
            l *= gL; r *= gR;                  // balance L vs R
            l *= pL; r *= pR;                  // polarity of L / R
        }

        L[i] = l; R[i] = r;
    }
}

void AnamorphEngine::processNonlinearRegion (float* L, float* R, int n, double rate) noexcept
{
    // Run the drive maths while Drive is engaged OR while the blend is still
    // gliding back to zero, so disengaging Drive fades out instead of stepping.
    if (driveActive || driveBlendSmooth.isSmoothing())
    {
        // Smoothed tanh drive with PEAK-preserving makeup (1/tanh(g)): a
        // full-scale input still maps to full scale, so driving harder adds
        // saturation/density without dropping the level (feedback #23). The blend
        // crossfades from the clean signal so 0 dB is identity (feedback #13).
        for (int i = 0; i < n; ++i)
        {
            const float g     = juce::jmax (1.0f, driveSmooth.getNextValue());
            const float blend = driveBlendSmooth.getNextValue();
            const float c = 1.0f / std::tanh (g);
            const float sl = std::tanh (g * L[i]) * c;
            const float sr2 = std::tanh (g * R[i]) * c;
            L[i] += blend * (sl  - L[i]);
            R[i] += blend * (sr2 - R[i]);
        }
    }

    if (isModAlgorithm (p.algorithm))
    {
        chorus.setWorkingRate (rate);
        chorus.processBlock (L, R, n);
    }
}

// ---------------------------------------------------------------------------
void AnamorphEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n <= 0) return;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    // ---- Click-free switch machine: once the duck has reached silence, adopt
    //      the deferred discrete change (clearing stale algorithm tails) and
    //      fade back in. Pure continuous edits never enter here (#10 / #11). ----
    if (switchState == SwitchState::FadeOut && switchPhase <= 0.0f)
    {
        const bool procChanged = processingDiffers (pendingP, p);
        // A change to the Multiband topology (band added/removed, or the module
        // toggled) needs its crossover filters cleared, captured before p is moved.
        const bool mbStructuralChange = (pendingP.mbBands != p.mbBands)
                                     || (pendingP.mbEnable != p.mbEnable);
        // Compare the incoming OS path against what was actually RUNNING (the
        // latch) -- p's driveDb was already overwritten by copyContinuous.
        const bool osPathChanged = pendingP.oversample != p.oversample
                                || osActiveFor (pendingP) != osEngaged;
        p = pendingP;
        osEngaged = osActiveFor (p);
        if (pendingAlgoReset) { haas.reset(); velvet.reset(); chorus.reset(); pendingAlgoReset = false; }
        if (osPathChanged)
        {
            // The incoming oversampler -- and the chorus, which runs at the OS
            // rate -- still hold audio from the last time that path ran. Left
            // alone it replays as a garbled burst right as the duck fades back
            // in, which is the "weird sound" on an Oversampling switch (#3).
            if (os2) os2->reset();
            if (os4) os4->reset();
            if (os8) os8->reset();
            chorus.reset();
        }
        // Re-arm the loudness match ONLY when the processing actually changed (A/B
        // swap, algorithm, ...). Toggling Level Match / Bypass must NOT re-measure,
        // or enabling Match with a big boost slams loud for a moment (#1).
        if (procChanged) loudness.softReset();
        updateDerived();

        // A forced bulk swap (A/B / preset / undo) finishes HERE, while silent: snap
        // the continuous smoothers to their new targets so the fade-in plays the
        // settled new state with no swell, and adopt any remembered Level-Match gain
        // now (masked) instead of jumping it at full level -- the A/B pop (#1).
        if (pendingForced)
        {
            snapSmoothers();
            // Clear EVERY stateful node so the fade-in plays the new state from a
            // clean slate. Even a SAME-algorithm A/B can move Haas delay / Chorus
            // rate / Drive far enough that the old delay-line + filter + oversampler
            // contents replay as a short glitch right as the duck lifts -- the
            // intermittent A/B "weird sound" (0.6.7 #22). At the silent bottom these
            // resets are inaudible.
            multiband.reset();
            monoMaker.reset();
            haas.reset();
            velvet.reset();
            chorus.reset();
            if (os2) os2->reset();
            if (os4) os4->reset();
            if (os8) os8->reset();
            pendingAlgoReset = false; // already handled by the wholesale reset above
            const float inj = matchInject.exchange (kNoInject, std::memory_order_relaxed);
            if (inj > kNoInject + 1.0f)
            {
                loudness.setDisplayedGainDb (inj);
                matchGainSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (inj));
            }
            pendingForced = false;
        }
        else if (mbStructuralChange)
        {
            // A non-forced structural Multiband edit (band added/removed) reached the
            // silent bottom: clear the crossover state so the new topology starts clean.
            multiband.reset();
        }
        switchState = SwitchState::FadeIn;
    }
    else if (switchState == SwitchState::FadeIn && switchPhase >= 1.0f)
    {
        switchState = SwitchState::Normal;
    }
    const bool fading = (switchState != SwitchState::Normal);

    // A Level-Match injection that arrived WITHOUT a forced duck (defensive: every
    // A/B switch forces one, so normally this is consumed at the silent bottom
    // above) still gets applied so it isn't lost.
    if (! pendingForced)
    {
        const float inj = matchInject.exchange (kNoInject, std::memory_order_relaxed);
        if (inj > kNoInject + 1.0f)
        {
            loudness.setDisplayedGainDb (inj);
            matchGainSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (inj));
        }
    }

    const int lat = getLatencySamples();
    const int ddSize = dryDelayBuffer.getNumSamples();
    float* ddL = dryDelayBuffer.getWritePointer (0);
    float* ddR = dryDelayBuffer.getWritePointer (1);

    levels.input.process (L, R, n); // tap the raw plugin input (#10)

    // -------- True bypass: latency-aligned passthrough + meter the output ----
    if (p.bypass)
    {
        for (int i = 0; i < n; ++i)
        {
            ddL[dryDelayWrite] = L[i];
            ddR[dryDelayWrite] = R[i];
            int rp = dryDelayWrite - lat; if (rp < 0) rp += ddSize;
            float ol = ddL[rp], orr = ddR[rp];
            dryDelayWrite = (dryDelayWrite + 1) % ddSize;

            if (fading)
            {
                if (switchState == SwitchState::FadeOut) { switchPhase -= switchIncOut; if (switchPhase < 0.0f) switchPhase = 0.0f; }
                else                                     { switchPhase += switchIncIn;  if (switchPhase > 1.0f) switchPhase = 1.0f; }
                const float sg = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * switchPhase);
                ol *= sg; orr *= sg;
            }

            L[i] = ol; R[i] = orr;
            correlation.process (ol, orr);
            scope.push (ol, orr);
        }
        levels.output.process (L, R, n);
        correlation.publish();
        levels.publish();
        return;
    }

    // -------- 1. Input conditioning -----------------------------------------
    applyInputConditioning (L, R, n);

    // M/S Solo lives in the INPUT module: it isolates Mid or Side BEFORE the
    // widening engine (feedback #15), so soloing Side on mono content stays
    // silent even as Amount is raised, and Output Balance still applies.
    if (p.solo == SoloMode::Mid)
        for (int i = 0; i < n; ++i) { const float m = (L[i] + R[i]) * 0.5f; L[i] = m; R[i] = m; }
    else if (p.solo == SoloMode::Side)
        for (int i = 0; i < n; ++i) { const float s = (L[i] - R[i]) * 0.5f; L[i] = s; R[i] = -s; }

    // Capture the full conditioned INPUT as the loudness-match reference (#25):
    // Level Match compares the FINAL recombined output against this.
    inputScratch.copyFrom (0, 0, L, n);
    inputScratch.copyFrom (1, 0, R, n);

    // -------- Mono Maker split (feedback #25) -------------------------------
    // Peel off a mono low band; only the HIGH band enters the widener and the
    // dry/wet mix. The mono lows go straight to the output (added back before
    // Output gain/balance), so the low end is unaffected by widening OR Mix.
    const bool monoMakerActive = p.monoMakerEnable;
    if (monoMakerActive)
    {
        monoMaker.processSplit (L, R, monoLow.getWritePointer (0), n);

        // Keep the un-driven mono low aside as the DRY low: Mix=0 must yield this,
        // not the driven low (#5).
        monoLowDry.copyFrom (0, 0, monoLow, 0, 0, n);

        // Drive the mono low band with the SAME saturation as the high band, so
        // raising Drive doesn't just boost the highs and make Mono Maker sound
        // like a low cut (feedback #1). Low-frequency mono -> base rate is fine.
        if (driveActive || monoDriveBlendSmooth.isSmoothing())
        {
            float* ml = monoLow.getWritePointer (0);
            for (int i = 0; i < n; ++i)
            {
                const float g = juce::jmax (1.0f, monoDriveSmooth.getNextValue());
                const float blend = monoDriveBlendSmooth.getNextValue();
                const float sat = std::tanh (g * ml[i]) * (1.0f / std::tanh (g));
                ml[i] += blend * (sat - ml[i]);
            }
        }
    }

    // DRY for the dry/wet mix = the band that actually enters the widener
    // (the high band when Mono Maker is on, otherwise the full signal).
    dryScratch.copyFrom (0, 0, L, n);
    dryScratch.copyFrom (1, 0, R, n);

    // -------- 3a. Oversampled nonlinear / modulation region -----------------
    if (auto* os = currentOversampler())
    {
        const double factor = (p.oversample == OversampleFactor::x2) ? 2.0
                            : (p.oversample == OversampleFactor::x4) ? 4.0 : 8.0;
        juce::dsp::AudioBlock<float> block (buffer);
        auto osBlock = os->processSamplesUp (block);
        processNonlinearRegion (osBlock.getChannelPointer (0),
                                osBlock.getChannelPointer (1),
                                (int) osBlock.getNumSamples(), sr * factor);
        os->processSamplesDown (block);
    }
    else
    {
        processNonlinearRegion (L, R, n, sr);
    }

    // -------- 3b. Linear algorithm at base rate -----------------------------
    if (p.algorithm == Algorithm::Haas)        haas.processBlock (L, R, n);
    else if (p.algorithm == Algorithm::Velvet) velvet.processBlock (L, R, n);

    // -------- 3c. Global Width (MS-domain) ----------------------------------
    for (int i = 0; i < n; ++i)
        applyWidth (L[i], R[i], widthSmooth.getNextValue());

    // -------- 4. Multiband Width (Advanced) ---------------------------------
    // A Multiband solo monitors AFTER the dry/wet Mix and forces full wet, so the
    // phase-matched dry is only needed when NOT soloing. When mixing in dry, we ask
    // MultibandWidth to also reconstruct A(dry) in lockstep with the wet's gliding
    // crossovers, so a partial Mix recombines without combing the mono sum (#1-KI).
    const bool mbSoloMonitor = p.mbEnable && ((p.mbSolo & ((1 << p.mbBands) - 1)) != 0);
    bool dryAligned = false;
    if (p.mbEnable)
    {
        if (mbSoloMonitor)
        {
            multiband.processBlock (L, R, n);
        }
        else
        {
            multiband.processBlock (L, R, n,
                dryScratch.getReadPointer (0), dryScratch.getReadPointer (1),
                dryAlignScratch.getWritePointer (0), dryAlignScratch.getWritePointer (1));
            dryAligned = true;
        }
    }

    // -------- 7. Mix (dry/wet), delay-compensated. The Mono Maker low band is
    //          folded INTO the same per-sample Mix so its DRIVE obeys Mix too:
    //          Mix=0 -> the un-driven mono low; Mix=1 -> the driven mono low. The
    //          low band's mono-ing itself stays present at all Mix values, but it
    //          no longer roars through driven when Mix=0 (#5). -------------------
    const float* dL  = dryScratch.getReadPointer (0);
    const float* dR  = dryScratch.getReadPointer (1);
    // Phase-matched dry source (A(dry)); falls back to the clean dry when the
    // Multiband isn't aligning, so the delay line stays coherent either way.
    const float* aL  = dryAligned ? dryAlignScratch.getReadPointer (0) : dL;
    const float* aR  = dryAligned ? dryAlignScratch.getReadPointer (1) : dR;
    float* adL = dryAlignDelayBuffer.getWritePointer (0);
    float* adR = dryAlignDelayBuffer.getWritePointer (1);
    const float* ml  = monoMakerActive ? monoLow.getReadPointer (0)    : nullptr; // wet (driven) low
    const float* mld = monoMakerActive ? monoLowDry.getReadPointer (0) : nullptr; // dry (un-driven) low
    float* md  = monoLowDelay.getWritePointer (0);
    float* mdd = monoLowDryDelay.getWritePointer (0);
    const int mdSize = monoLowDelay.getNumSamples();

    // The dry source crossfades from the CLEAN dry (bit-exact at Mix=0 -> an exact
    // null) to the phase-ALIGNED A(dry) as Mix leaves 0, so 0<Mix<1 never combs yet
    // Mix=0 stays sample-exact. The fade completes by kAlignMix; below it the wet is
    // barely present, so the brief blend's residual comb is negligible. Smoothstep
    // (zero slope at 0) keeps the departure click-free (Known Issue #1).
    constexpr float kAlignMix = 0.05f;

    for (int i = 0; i < n; ++i)
    {
        ddL[dryDelayWrite] = dL[i];
        ddR[dryDelayWrite] = dR[i];
        adL[dryDelayWrite] = aL[i];
        adR[dryDelayWrite] = aR[i];
        int rp = dryDelayWrite - lat; if (rp < 0) rp += ddSize;
        const float cleanL = ddL[rp], cleanR = ddR[rp];
        const float alignL = adL[rp], alignR = adR[rp];
        dryDelayWrite = (dryDelayWrite + 1) % ddSize;

        const float m = mbSoloMonitor ? 1.0f : mixSmooth.getNextValue();
        if (mbSoloMonitor) mixSmooth.getNextValue(); // keep the smoother advancing in lock-step

        float dryL = cleanL, dryR = cleanR;
        if (dryAligned)
        {
            const float t  = juce::jlimit (0.0f, 1.0f, m * (1.0f / kAlignMix));
            const float ts = t * t * (3.0f - 2.0f * t); // smoothstep: clean -> A(dry)
            dryL = cleanL + ts * (alignL - cleanL);
            dryR = cleanR + ts * (alignR - cleanR);
        }
        float outL = dryL + m * (L[i] - dryL);
        float outR = dryR + m * (R[i] - dryR);

        if (monoMakerActive)
        {
            md[monoLowWrite]  = ml[i];
            mdd[monoLowWrite] = mld[i];
            int mp = monoLowWrite - lat; if (mp < 0) mp += mdSize;
            const float wetLow = md[mp], dryLow = mdd[mp];
            monoLowWrite = (monoLowWrite + 1) % mdSize;
            if (! mbSoloMonitor)
            {
                const float lowOut = dryLow + m * (wetLow - dryLow); // lows obey Mix (#5)
                outL += lowOut; outR += lowOut;
            }
        }

        L[i] = outL; R[i] = outR;
    }

    // Level Match detects the FULL recombined output (lows + highs) against the
    // full input, BEFORE Output gain / balance (feedback #25).
    wetScratch.copyFrom (0, 0, L, n);
    wetScratch.copyFrom (1, 0, R, n);
    loudness.setDriveDb (driveActive ? p.driveDb : 0.0f); // anticipate Drive boost on silence (#19)
    loudness.process (inputScratch.getReadPointer (0), inputScratch.getReadPointer (1),
                      wetScratch.getReadPointer (0), wetScratch.getReadPointer (1), n);
    matchGainSmooth.setTargetValue (p.autoGainMatch
        ? juce::Decibels::decibelsToGain (loudness.getMatchGainDb()) : 1.0f);

    // -------- 8. Output Gain / Auto Gain / Output Balance -------------------
    for (int i = 0; i < n; ++i)
    {
        // When Level Match is engaged the matched gain REPLACES Output Gain, so
        // the Output knob no longer shifts the matched level (feedback #1). Both
        // smoothers advance every sample so toggling Match (a ducked switch) is
        // seamless. Match's smoother is slow, so an A/B swap glides (feedback #16).
        const float og = outGainSmooth.getNextValue();
        const float mg = matchGainSmooth.getNextValue();
        const float g  = p.autoGainMatch ? mg : og;

        // Whole-plugin output balance (centre = unity, turns down one side).
        const float b  = outBalanceSmooth.getNextValue();
        const float gL = (b > 0.0f) ? (1.0f - b) : 1.0f;
        const float gR = (b < 0.0f) ? (1.0f + b) : 1.0f;

        // Click-free switch duck (raised cosine, zero slope at the seam, #10/#11).
        float sg = 1.0f;
        if (fading)
        {
            if (switchState == SwitchState::FadeOut) { switchPhase -= switchIncOut; if (switchPhase < 0.0f) switchPhase = 0.0f; }
            else                                     { switchPhase += switchIncIn;  if (switchPhase > 1.0f) switchPhase = 1.0f; }
            sg = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * switchPhase);
        }

        L[i] *= g * gL * sg; R[i] *= g * gR * sg;
    }

    // -------- 9. Metering tap (FINAL output) --------------------------------
    for (int i = 0; i < n; ++i)
    {
        correlation.process (L[i], R[i]);
        scope.push (L[i], R[i]);
    }
    levels.output.process (L, R, n);
    correlation.publish();
    levels.publish();
}

} // namespace anamorph
