#include "AnamorphEngine.h"
#include "MidSide.h"

namespace anamorph
{

using juce::dsp::Oversampling;

// ---------------------------------------------------------------------------
void AnamorphEngine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = juce::jmax (1, maxBlockSize);

    // --- sub-modules ---
    sat.setDriveDb (0.0f);
    haas.prepare (sr, maxBlock);
    velvet.prepare (sr);
    chorus.prepare (sr * 8.0);          // sized for the highest OS rate (8x)
    multiband.prepare (sr, maxBlock);
    monoMaker.prepare (sr, maxBlock);
    loudness.prepare (sr);
    correlation.prepare (sr);

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
    widthSmooth   .reset (sr, ramp);
    mixSmooth     .reset (sr, ramp);
    outGainSmooth .reset (sr, ramp);
    matchGainSmooth.reset (sr, 0.20); // slower for loudness match
    balanceSmooth .reset (sr, ramp);
    widthSmooth   .setCurrentAndTargetValue (1.0f);
    mixSmooth     .setCurrentAndTargetValue (1.0f);
    outGainSmooth .setCurrentAndTargetValue (1.0f);
    matchGainSmooth.setCurrentAndTargetValue (1.0f);
    balanceSmooth .setCurrentAndTargetValue (0.0f);

    // --- dry-path delay (aligns dry to the wet OS latency) ---
    const int maxLat = juce::jmax (latency2, latency4, latency8);
    dryDelayBuffer.setSize (2, maxLat + maxBlock + 1);
    dryDelayBuffer.clear();
    dryDelayWrite = 0;

    dryScratch.setSize (2, maxBlock);
    wetScratch.setSize (2, maxBlock);

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
    if (os2) os2->reset();
    if (os4) os4->reset();
    if (os8) os8->reset();
    dryDelayBuffer.clear();
    dryDelayWrite = 0;
}

// ---------------------------------------------------------------------------
//  Is oversampling actually doing work? Only when wrapping a nonlinear /
//  modulation stage (Drive, or Chorus / Dimension-D). Linear-only chains skip
//  oversampling entirely so they add ZERO latency (spec section 2.2 / 9).
// ---------------------------------------------------------------------------
static bool isModAlgorithm (Algorithm a) noexcept
{
    return a == Algorithm::Chorus || a == Algorithm::DimensionD;
}

juce::dsp::Oversampling<float>* AnamorphEngine::currentOversampler() noexcept
{
    if (p.oversample == OversampleFactor::Off) return nullptr;
    if (! (driveActive || isModAlgorithm (p.algorithm))) return nullptr;

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
    if (p.oversample == OversampleFactor::Off) return 0;
    if (! (driveActive || isModAlgorithm (p.algorithm))) return 0;
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
    sat.setDriveDb (p.driveDb);
    driveActive = p.driveDb > 0.01f;

    haas.setDelayMs (p.haasDelayMs);
    haas.setSide (p.haasSide == HaasSide::Right);

    velvet.setDensity (p.velvetDensity);

    if (p.algorithm == Algorithm::Chorus)
    {
        chorus.setVoice (ChorusEngine::Voice::Chorus);
        chorus.setRate  (p.chorusRate);
        chorus.setDepth (p.chorusDepth);
        chorus.setAmount (0.5f);
    }
    else if (p.algorithm == Algorithm::DimensionD)
    {
        chorus.setVoice (ChorusEngine::Voice::DimensionD);
        chorus.setDimMode (p.dimMode);
        chorus.setAmount (p.dimAmount);
    }

    multiband.setCrossovers (p.mbFreqLow, p.mbFreqHigh);
    multiband.setWidths (p.mbWidthLow, p.mbWidthMid, p.mbWidthHigh);

    monoMaker.setFrequency (p.monoMakerFreq);

    widthSmooth  .setTargetValue (p.width);
    mixSmooth    .setTargetValue (p.mix);
    balanceSmooth.setTargetValue (p.inputBalance);
    outGainSmooth.setTargetValue (juce::Decibels::decibelsToGain (p.outputGainDb));
    matchGainSmooth.setTargetValue (p.autoGainMatch
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
            case ChannelMode::Mono:      { const float m = (l + r) * 0.5f; l = m; r = m; } break;
            case ChannelMode::LeftOnly:  r = 0.0f; break;   // keep L, kill R
            case ChannelMode::RightOnly: l = 0.0f; break;   // keep R, kill L
            case ChannelMode::Stereo:    default: break;
        }

        if (p.swapLR) { const float t = l; l = r; r = t; }

        // Balance: centre is unity, turning attenuates the opposite channel.
        const float b  = balanceSmooth.getNextValue();
        const float gL = (b > 0.0f) ? (1.0f - b) : 1.0f;
        const float gR = (b < 0.0f) ? (1.0f + b) : 1.0f;
        l *= gL; r *= gR;

        if (p.polarityL) l = -l;
        if (p.polarityR) r = -r;

        L[i] = l; R[i] = r;
    }
}

void AnamorphEngine::processNonlinearRegion (float* L, float* R, int n, double rate) noexcept
{
    if (driveActive)
        for (int i = 0; i < n; ++i)
        {
            L[i] = sat.processSample (L[i]);
            R[i] = sat.processSample (R[i]);
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

    const int lat = getLatencySamples();
    const int ddSize = dryDelayBuffer.getNumSamples();
    float* ddL = dryDelayBuffer.getWritePointer (0);
    float* ddR = dryDelayBuffer.getWritePointer (1);

    // -------- True bypass: latency-aligned passthrough + meter the output ----
    if (p.bypass)
    {
        for (int i = 0; i < n; ++i)
        {
            ddL[dryDelayWrite] = L[i];
            ddR[dryDelayWrite] = R[i];
            int rp = dryDelayWrite - lat; if (rp < 0) rp += ddSize;
            const float ol = ddL[rp], orr = ddR[rp];
            L[i] = ol; R[i] = orr;
            correlation.process (ol, orr);
            scope.push (ol, orr);
            dryDelayWrite = (dryDelayWrite + 1) % ddSize;
        }
        correlation.publish();
        return;
    }

    // -------- 1. Input conditioning -----------------------------------------
    applyInputConditioning (L, R, n);

    // Capture conditioned DRY for the mix + loudness reference.
    dryScratch.copyFrom (0, 0, L, n);
    dryScratch.copyFrom (1, 0, R, n);

    // -------- 2. MS encode (only the Drive + algorithm run in M/S) ----------
    if (p.msMode)
        for (int i = 0; i < n; ++i)
        {
            float m, s; MidSide::encode (L[i], R[i], m, s);
            L[i] = m; R[i] = s;
        }

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

    // -------- 6. MS decode --------------------------------------------------
    if (p.msMode)
        for (int i = 0; i < n; ++i)
        {
            float l, r; MidSide::decode (L[i], R[i], l, r);
            L[i] = l; R[i] = r;
        }

    // -------- 3c. Global Width (MS-domain) ----------------------------------
    for (int i = 0; i < n; ++i)
        applyWidth (L[i], R[i], widthSmooth.getNextValue());

    // -------- 4. Multiband Width (Advanced) ---------------------------------
    if (p.mbEnable)
        multiband.processBlock (L, R, n);

    // -------- 5. Mono Maker -------------------------------------------------
    if (p.monoMakerEnable)
        monoMaker.processBlock (L, R, n);

    // -------- 7. Mix (dry delay-compensated to the wet latency) -------------
    const float* dL = dryScratch.getReadPointer (0);
    const float* dR = dryScratch.getReadPointer (1);
    for (int i = 0; i < n; ++i)
    {
        ddL[dryDelayWrite] = dL[i];
        ddR[dryDelayWrite] = dR[i];
        int rp = dryDelayWrite - lat; if (rp < 0) rp += ddSize;
        const float dryL = ddL[rp], dryR = ddR[rp];
        dryDelayWrite = (dryDelayWrite + 1) % ddSize;

        const float m = mixSmooth.getNextValue();
        L[i] = dryL + m * (L[i] - dryL);
        R[i] = dryR + m * (R[i] - dryR);
    }

    // Capture WET (post-mix, pre-gain) for perceptual loudness matching.
    wetScratch.copyFrom (0, 0, L, n);
    wetScratch.copyFrom (1, 0, R, n);
    loudness.process (dryScratch.getReadPointer (0), dryScratch.getReadPointer (1),
                      wetScratch.getReadPointer (0), wetScratch.getReadPointer (1), n);
    matchGainSmooth.setTargetValue (p.autoGainMatch
        ? juce::Decibels::decibelsToGain (loudness.getMatchGainDb()) : 1.0f);

    // -------- 8. Output Gain / Auto Gain ------------------------------------
    for (int i = 0; i < n; ++i)
    {
        const float g = outGainSmooth.getNextValue() * matchGainSmooth.getNextValue();
        L[i] *= g; R[i] *= g;
    }

    // -------- Solo Mid / Side ----------------------------------------------
    if (p.solo == SoloMode::Mid)
        for (int i = 0; i < n; ++i) { const float m = (L[i] + R[i]) * 0.5f; L[i] = m; R[i] = m; }
    else if (p.solo == SoloMode::Side)
        for (int i = 0; i < n; ++i) { const float s = (L[i] - R[i]) * 0.5f; L[i] = s; R[i] = s; }

    // -------- 9. Metering tap (FINAL output) --------------------------------
    for (int i = 0; i < n; ++i)
    {
        correlation.process (L[i], R[i]);
        scope.push (L[i], R[i]);
    }
    correlation.publish();
}

} // namespace anamorph
