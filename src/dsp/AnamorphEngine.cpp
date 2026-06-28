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
    soloMonitor.prepare (sr, maxBlock);
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

    // True-bypass raw-input delay line + per-block scratch, same size as the dry delay.
    bypassDelayBuffer.setSize (2, maxLat + maxBlock + 1);
    bypassDelayBuffer.clear();
    bypassDryScratch.setSize (2, maxBlock);
    bypassDelayWrite = 0;
    bypassBlend.reset (sr, 0.010); // ~10 ms sample-safe crossfade
    bypassBlend.setCurrentAndTargetValue (p.bypass ? 1.0f : 0.0f);

    // Multiband Enable crossfade: same short, click-free ramp as Bypass.
    preMbScratch.setSize (2, maxBlock);
    mbEnableBlend.reset (sr, 0.012); // ~12 ms sample-safe crossfade
    mbEnableBlend.setCurrentAndTargetValue (p.mbEnable ? 1.0f : 0.0f);
    mbRunning = p.mbEnable; // prepare() just reset the bank, so it is clean + warm if on

    dryScratch.setSize (2, maxBlock);
    wetScratch.setSize (2, maxBlock);
    inputScratch.setSize (2, maxBlock);
    loudnessRefScratch.setSize (2, maxBlock);

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
    soloMonitor.reset();
    loudness.reset();
    correlation.reset();
    levels.reset();
    if (os2) os2->reset();
    if (os4) os4->reset();
    if (os8) os8->reset();
    dryDelayBuffer.clear();
    dryAlignDelayBuffer.clear();
    dryDelayWrite = 0;
    bypassDelayBuffer.clear();
    bypassDelayWrite = 0;
    prevInputSilent = true;

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
    bypassBlend.setCurrentAndTargetValue (p.bypass ? 1.0f : 0.0f); // settle the crossfade
    mbEnableBlend.setCurrentAndTargetValue (p.mbEnable ? 1.0f : 0.0f); // settle the multiband crossfade
    mbRunning = p.mbEnable; // reset() above cleaned the bank: warm iff multiband is on
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
        || a.mbBands          != b.mbBands
        // Multiband Enable is NOT listed: like Bypass it is now a click-free OUTPUT
        // crossfade (mbEnableBlend) with the crossover bank kept warm, NOT a duck-to-
        // silence -- so toggling it no longer mutes/drops the output. A BAND-COUNT
        // change (mbBands) still ducks: that is a true structural rewire of the bank.
        // Band Solo is NOT listed: it is a post-everything monitor with its own
        // click-free crossfade (SoloMonitor), so a solo change needs no output duck
        // -- ducking it was the source of the engage tick / pause-time ghost (0.8.1).
        || a.monoMakerEnable  != b.monoMakerEnable
        || a.autoGainMatch    != b.autoGainMatch
        || a.oversample       != b.oversample
        // Bypass is NOT listed: it is now a click-free OUTPUT crossfade (bypassBlend),
        // not a ducked switch -- the chain + analysis run regardless, so toggling it
        // never stops Level Match and never needs a duck (Issues 2/3).
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
    // Keep dst's discrete fields; pull every smoothed/continuous field from src. Every
    // state field is preserved here (merge consistency): mbBands so a deferred band-count
    // change is still detected at the silent duck bottom (structural-change fix), and
    // bypass for state consistency -- its click-free transition is the bypassBlend
    // crossfade in process(), so preserving it here is neutral (a bypass-only change goes
    // through the continuous path and never reaches copyContinuous).
    const auto cm = dst.channelMode; const auto ms = dst.monoSum; const auto sw = dst.swapLR;
    const auto md = dst.msMode;      const auto so = dst.solo;    const auto al = dst.algorithm;
    const auto hs = dst.haasSide;    const auto dm = dst.dimMode; const auto mb = dst.mbEnable;
    const auto nb = dst.mbBands;
    const auto mm = dst.monoMakerEnable; const auto ov = dst.oversample; const auto by = dst.bypass;
    const auto ag = dst.autoGainMatch;

    dst = src;

    dst.channelMode = cm; dst.monoSum = ms; dst.swapLR = sw; dst.msMode = md; dst.solo = so;
    dst.algorithm = al;   dst.haasSide = hs; dst.dimMode = dm; dst.mbEnable = mb;
    dst.mbBands = nb;
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
    snap (polLSmooth);    snap (polRSmooth);
    // Snap the Bypass crossfade too, so a forced swap that also flips Bypass lands bit-exact
    // (1 -> true bypass, 0 -> processed) at the silent duck bottom rather than ramping (#8).
    snap (bypassBlend);
    // Same for the Multiband Enable crossfade: a forced swap that flips it lands settled.
    snap (mbEnableBlend);
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
    multiband.setCrossovers (p.mbFreqLow, p.mbFreqMid, p.mbFreqHigh);
    multiband.setWidths (p.mbWidthLow, p.mbWidthMid, p.mbWidthHiMid, p.mbWidthHigh);

    // Band Solo is a post-everything MONITOR: the SoloMonitor mirrors the Multiband's
    // band split (same count + crossovers) so soloing band b auditions exactly band b
    // of the FINAL output. The solo mask itself is read per block in process().
    soloMonitor.setBandCount (p.mbBands);
    soloMonitor.setCrossovers (p.mbFreqLow, p.mbFreqMid, p.mbFreqHigh);

    monoMaker.setFrequency (p.monoMakerFreq);

    widthSmooth     .setTargetValue (p.width);
    mixSmooth       .setTargetValue (p.mix);
    balanceSmooth   .setTargetValue (p.inputBalance);
    outBalanceSmooth.setTargetValue (p.outputBalance);
    outGainSmooth   .setTargetValue (juce::Decibels::decibelsToGain (p.outputGainDb));
    matchGainSmooth .setTargetValue (p.autoGainMatch
        ? juce::Decibels::decibelsToGain (loudness.getMatchGainDb())
        : 1.0f);
    bypassBlend     .setTargetValue (p.bypass ? 1.0f : 0.0f); // click-free Bypass crossfade
    mbEnableBlend   .setTargetValue (p.mbEnable ? 1.0f : 0.0f); // click-free Multiband Enable crossfade
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
        // (Bypass is no longer handled here -- it is a continuous output crossfade with
        //  the chain always running, so there is never any stale bypass state to clear.)
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
            mbRunning = p.mbEnable; // bank just cleaned; warm iff multiband is on
            monoMaker.reset();
            soloMonitor.reset();
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
        else
        {
            // A non-forced structural Multiband edit (band added/removed, or the module
            // toggled) reached the silent bottom: clear the crossover state so the new
            // topology starts clean. The post-everything Band Solo monitor mirrors the
            // band split, so re-point + clear it on the same structural change. A pure
            // solo change is NOT ducked -- the SoloMonitor crossfades it click-free.
            if (mbStructuralChange) { multiband.reset(); mbRunning = p.mbEnable; soloMonitor.reset(); }
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

    // ---- True-bypass dry source: the RAW input, delay-aligned to the wet latency.
    //      Captured HERE, before any conditioning, so Bypass can crossfade to the exact
    //      unprocessed signal at the very END of the chain (Issue 3). The processing AND
    //      the Level-Match analysis ALWAYS run below -- Bypass only changes the audio
    //      output path, never the analysis path, so Measure + Predict keep running while
    //      bypassed (Issue 2). Bypass is therefore a click-free crossfade, not a mute,
    //      and needs no duck (it is no longer a discrete switch).
    {
        float* bdL = bypassDelayBuffer.getWritePointer (0);
        float* bdR = bypassDelayBuffer.getWritePointer (1);
        const int bdSize = bypassDelayBuffer.getNumSamples();
        float* bxL = bypassDryScratch.getWritePointer (0);
        float* bxR = bypassDryScratch.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            bdL[bypassDelayWrite] = L[i];
            bdR[bypassDelayWrite] = R[i];
            int rp = bypassDelayWrite - lat; if (rp < 0) rp += bdSize;
            bxL[i] = bdL[rp]; bxR[i] = bdR[rp];
            bypassDelayWrite = (bypassDelayWrite + 1) % bdSize;
        }
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

    // DRY for the dry/wet Mix = the full conditioned input (same content as the
    // Level-Match reference). Mono Maker now runs POST-Mix, so nothing is peeled off
    // here; the widener and Multiband see the whole signal.
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

    // -------- Multiband Width (Advanced) ------------------------------------
    // Reconstruct the dry through the SAME gliding crossovers as the wet, at unit
    // width -- a phase-matched A(dry) -- so a partial Mix never combs the mono sum
    // (Known Issue #1). Solo-agnostic: the wet always sums every band.
    // Multiband Enable is a short click-free OUTPUT crossfade (the bypassBlend model),
    // NOT a duck: the crossover bank stays WARM across the toggle and its output is
    // faded against the pre-multiband signal, so enabling/disabling never mutes or
    // settles audibly. mbActive keeps the bank running while the blend is non-zero, so
    // a disable fades the multiband OUT over ~12 ms before the bank goes cold.
    bool dryAligned = false;
    const bool mbActive = p.mbEnable || mbEnableBlend.isSmoothing()
                       || mbEnableBlend.getCurrentValue() > 0.0f;
    if (mbActive)
    {
        // The instant the bank begins running again (after being fully disabled) it is
        // cold; clear it now, while the blend is still ~0, so its settle is masked and
        // an enable can never click (the old reset-at-silent-duck-bottom, made local).
        if (! mbRunning) { multiband.reset(); mbRunning = true; }

        // Only crossfade while the blend is actually mid-transition. Settled at 1 the
        // output IS the multiband result, so we skip the mix and stay BIT-EXACT with the
        // plain processed path (the common, fully-enabled case + every existing test).
        const bool blending = mbEnableBlend.isSmoothing()
                           || mbEnableBlend.getCurrentValue() < 1.0f;

        // Keep the pre-multiband signal -- the "off" side of the enable crossfade (the
        // chain output with the multiband NOT applied) -- before processBlock overwrites it.
        if (blending)
        {
            juce::FloatVectorOperations::copy (preMbScratch.getWritePointer (0), L, n);
            juce::FloatVectorOperations::copy (preMbScratch.getWritePointer (1), R, n);
        }

        multiband.processBlock (L, R, n,
            dryScratch.getReadPointer (0), dryScratch.getReadPointer (1),
            dryAlignScratch.getWritePointer (0), dryAlignScratch.getWritePointer (1));
        dryAligned = true;

        // Fade the multiband contribution in/out. At blend 1 the output is exactly the
        // multiband result; at 0 it is exactly the pre-multiband signal -- so a settled
        // toggle is bit-exact either way and the transition is imperceptible.
        if (blending)
        {
            const float* pmL = preMbScratch.getReadPointer (0);
            const float* pmR = preMbScratch.getReadPointer (1);
            for (int i = 0; i < n; ++i)
            {
                const float b = mbEnableBlend.getNextValue();
                L[i] = pmL[i] + b * (L[i] - pmL[i]);
                R[i] = pmR[i] + b * (R[i] - pmR[i]);
            }
        }
    }
    else
    {
        mbRunning = false; // fully disabled: the bank is idle and may go cold
    }

    // ======================== 2. DRY / WET MIX ==============================
    // Delay-compensated dry. The dry source crossfades from the CLEAN dry (bit-exact
    // at Mix=0 -> an exact null) to the phase-ALIGNED A(dry) as Mix leaves 0, so
    // 0<Mix<1 never combs yet Mix=0 stays sample-exact; the fade completes by
    // kAlignMix and smoothstep (zero slope at 0) keeps the departure click-free (KI #1).
    const float* dL = dryScratch.getReadPointer (0);
    const float* dR = dryScratch.getReadPointer (1);
    const float* aL = dryAligned ? dryAlignScratch.getReadPointer (0) : dL;
    const float* aR = dryAligned ? dryAlignScratch.getReadPointer (1) : dR;
    float* adL = dryAlignDelayBuffer.getWritePointer (0);
    float* adR = dryAlignDelayBuffer.getWritePointer (1);
    float* lrL = loudnessRefScratch.getWritePointer (0);
    float* lrR = loudnessRefScratch.getWritePointer (1);
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

        // Level-Match reference (#Issue2): the delay-aligned reconstruction A(dry) --
        // the dry pushed through the SAME crossovers at unit width. It carries the
        // Multiband's allpass-reconstruction magnitude ripple, so comparing the wet
        // against IT (not the raw input) cancels that ripple: Measure reads ~0 at unit
        // width whether Multiband is on or off, and still measures the real loudness
        // change once a band's width moves. When Multiband is off this is just the
        // delay-aligned input, which also fixes the old OS-latency misalignment.
        lrL[i] = alignL; lrR[i] = alignR;

        const float m = mixSmooth.getNextValue();
        float dryL = cleanL, dryR = cleanR;
        if (dryAligned)
        {
            const float t  = juce::jlimit (0.0f, 1.0f, m * (1.0f / kAlignMix));
            const float ts = t * t * (3.0f - 2.0f * t); // smoothstep: clean -> A(dry)
            dryL = cleanL + ts * (alignL - cleanL);
            dryR = cleanR + ts * (alignR - cleanR);
        }
        L[i] = dryL + m * (L[i] - dryL);
        R[i] = dryR + m * (R[i] - dryR);
    }

    // ======================== 3. MONO MAKER (post-Mix) ======================
    // Collapse the low band of the MIXED signal to mono in place, so the final low
    // end is mono whatever the Mix amount. The Mid stays allpass-flat (LP + HP), so
    // the mono sum has no low-frequency cancellation.
    if (p.monoMakerEnable)
        monoMaker.process (L, R, n);

    // ======================== 4. OUTPUT STAGE ===============================
    // Level Match measures the post-Mono-Maker signal (the real processed output)
    // against the conditioned input, BEFORE Output gain / balance (feedback #25).
    wetScratch.copyFrom (0, 0, L, n);
    wetScratch.copyFrom (1, 0, R, n);
    // Feed the predict BOTH big-gain controls so it pre-ducks the instant Drive OR Mix
    // is raised (Drive maxed + Mix 0 -> no boost; Mix to 100% -> pre-duck) (#14/#19).
    loudness.setDriveDb (p.driveDb);
    loudness.setMix     (p.mix);
    // Dry reference = the delay-aligned reconstruction (loudnessRefScratch), NOT the raw
    // input, so the Multiband allpass-reconstruction ripple cancels -> Measure ~0 at
    // unit width with Multiband on (Issue 2). Falls back to the delay-aligned input when
    // Multiband is off.
    loudness.process (loudnessRefScratch.getReadPointer (0), loudnessRefScratch.getReadPointer (1),
                      wetScratch.getReadPointer (0), wetScratch.getReadPointer (1), n);
    const float matchTarget = p.autoGainMatch
        ? juce::Decibels::decibelsToGain (loudness.getMatchGainDb()) : 1.0f;
    matchGainSmooth.setTargetValue (matchTarget);

    // Silence -> audio edge: SNAP the applied match gain to its (already pre-ducked)
    // target so the first audible block is compensated even if the host never ran the
    // plugin while paused. Click-free: the previous block's output was silent. This is
    // the safety net that makes the predict effective across Transport Stop->Play and
    // Silence->Audio, not just when the host keeps processing during a pause.
    double inSq = 0.0;
    {
        const float* il = inputScratch.getReadPointer (0);
        const float* ir = inputScratch.getReadPointer (1);
        for (int i = 0; i < n; ++i) inSq += (double) il[i] * il[i] + (double) ir[i] * ir[i];
    }
    const bool inSilentNow = inSq < 1.0e-6 * (double) juce::jmax (1, n); // ~ -60 dBFS mean-square
    if (prevInputSilent && ! inSilentNow && p.autoGainMatch)
        matchGainSmooth.setCurrentAndTargetValue (matchTarget);
    prevInputSilent = inSilentNow;

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

    // ======================== 5. BAND SOLO MONITOR ==========================
    // POST-EVERYTHING audition: band-pass the already-produced output to the soloed
    // band(s). No effect stage changed its behaviour for solo -- this only filters
    // what is heard. mask == 0 (or Multiband off) -> the true output passes through.
    if (p.mbEnable)
        soloMonitor.process (L, R, p.mbSolo, n);

    // -------- Defensive NaN / Inf self-heal --------------------------------
    // This is NOT a level limiter: it touches ONLY non-finite samples, so valid audio
    // (however loud) is passed through untouched -- no 0 dBFS clipper, dynamics and
    // headroom fully preserved (Issue 1). The crossover Nyquist clamp already prevents
    // the multiband blow-up at the source; if anything ever still went non-finite it
    // would latch a dead channel / poison the meters, so any non-finite sample is
    // replaced with 0 and the stateful nodes are reset to stop the source (self-heal).
    bool nonFinite = false;
    for (int i = 0; i < n; ++i)
    {
        if (! std::isfinite (L[i])) { L[i] = 0.0f; nonFinite = true; }
        if (! std::isfinite (R[i])) { R[i] = 0.0f; nonFinite = true; }
    }
    if (nonFinite)
    {
        multiband.reset(); mbRunning = p.mbEnable; monoMaker.reset(); soloMonitor.reset();
        haas.reset(); velvet.reset(); chorus.reset();
        if (os2) os2->reset(); if (os4) os4->reset(); if (os8) os8->reset();
        loudness.reset();
        dryDelayBuffer.clear(); dryAlignDelayBuffer.clear(); bypassDelayBuffer.clear();
        // Also flush this block's delay-aligned dry scratch, so the Bypass crossfade
        // below can't re-introduce a non-finite sample from pathological host input.
        bypassDryScratch.clear();
    }

    // ======================== 6. BYPASS CROSSFADE ===========================
    // Click-free, sample-safe Bypass: a short crossfade between the fully processed
    // output and the delay-aligned RAW input -- no mute, no dropout, imperceptible
    // switch (Issue 3). bypassBlend settles to exactly 1 -> bit-exact true bypass; to
    // exactly 0 -> the untouched processed output. Because the chain + analysis already
    // ran above, toggling Bypass never stops Level Match and never re-engages stale DSP.
    if (bypassBlend.isSmoothing() || bypassBlend.getTargetValue() > 0.0f)
    {
        const float* bxL = bypassDryScratch.getReadPointer (0);
        const float* bxR = bypassDryScratch.getReadPointer (1);
        for (int i = 0; i < n; ++i)
        {
            const float bb = bypassBlend.getNextValue();
            L[i] += bb * (bxL[i] - L[i]);
            R[i] += bb * (bxR[i] - R[i]);
        }
    }

    // -------- Metering tap (the monitored output) ---------------------------
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
