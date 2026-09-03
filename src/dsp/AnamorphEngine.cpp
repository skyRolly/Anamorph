#include "AnamorphEngine.h"
#include "MidSide.h"
#include <cstdint>
#include <cstring>

namespace anamorph
{

using juce::dsp::Oversampling;

// ---------------------------------------------------------------------------
//  Is oversampling actually doing WORK? Only when wrapping a nonlinear /
//  modulation stage (Drive, or Chorus / Dimension-D). Linear-only chains skip
//  the whole resampling round trip -- the CPU saving this predicate exists for,
//  and the largest single one in the engine (spec section 2.2 / 9).
//
//  IT NO LONGER DECIDES THE LATENCY (ADR-0034). It used to answer both "does the
//  wrap run?" and "does the chain have delay?" with one bit, so an ordinary Drive
//  move changed the number reported to the host and the host restarted the graph
//  for it. The delay now belongs to the SELECTED FACTOR: when this predicate is
//  false but a factor is selected, `osCompDelayBuffer` stands in for the wrap's
//  group delay, so the chain carries the same latency either way.
//
//  AND IT NO LONGER FORCES A DUCK. It is now the TARGET of `osBlend`, a click-free
//  crossfade between the two paths, so this predicate may flip live and mid-block:
//  nothing is latched from it any more. The duck it used to force could not mask
//  the swap anyway -- the duck's gain is applied downstream of Haas and Velvet, so
//  the handover's discontinuity entered their 12-35 ms delay lines at full level
//  and re-emerged after the fade had finished. See `osBlend` in the header.
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
    velvet.prepare (sr, maxBlock);
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
    latency2.store ((int) std::round (os2->getLatencyInSamples()), std::memory_order_relaxed);
    latency4.store ((int) std::round (os4->getLatencyInSamples()), std::memory_order_relaxed);
    latency8.store ((int) std::round (os8->getLatencyInSamples()), std::memory_order_relaxed);

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
    const int maxLat = juce::jmax (latency2.load (std::memory_order_relaxed),
                                   latency4.load (std::memory_order_relaxed),
                                   latency8.load (std::memory_order_relaxed));
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

    // Oversampling latency stand-in, sized like the rest: the read offset is the
    // same `maxLat` at most, and a block may write up to `maxBlock` before the
    // oldest sample it needs is read back. Allocated HERE, on the host's prepare
    // thread -- the audio path only ever reads and writes it (REALTIME_AUDIO_POLICY).
    osCompDelayBuffer.setSize (2, maxLat + maxBlock + 1);
    osCompDelayBuffer.clear();
    osCompDelayWrite = 0;

    // OS-path crossfade: the wrapped path's working buffer, and the blend itself.
    // 12 ms, the same sample-safe ramp Multiband Enable uses -- long enough to be
    // inaudible on a path swap, short enough that the wrap goes cold promptly.
    osPathScratch.setSize (2, maxBlock);
    osBlend.reset (sr, 0.012);
    osBlend.setCurrentAndTargetValue (osActiveFor (p) ? 1.0f : 0.0f);
    osRunning = osActiveFor (p); // prepare() reset the oversamplers: warm iff engaged
    bypassBlend.reset (sr, 0.010); // ~10 ms sample-safe crossfade
    bypassBlend.setCurrentAndTargetValue (p.bypass ? 1.0f : 0.0f);

    // Multiband Enable crossfade: same short, click-free ramp as Bypass.
    preMbScratch.setSize (2, maxBlock);
    mbEnableBlend.reset (sr, 0.012); // ~12 ms sample-safe crossfade
    mbEnableBlend.setCurrentAndTargetValue (p.mbEnable ? 1.0f : 0.0f);
    mbRunning = p.mbEnable; // prepare() just reset the bank, so it is clean + warm if on

    dryScratch.setSize (2, maxBlock);
    loudnessRefScratch.setSize (2, maxBlock);

    updateDerived();
    reset();

    // Settle every continuous smoother at the target updateDerived() just armed
    // from the live snapshot p. Without this, the neutral constants written
    // above are the smoothers' CURRENT values, so the first ~5-20 ms after
    // every prepareToPlay of a non-default session GLIDE from neutral (a Mix=0
    // session opened wet, Output Gain -24 dB opened hot, inverted polarity
    // ramped through +1) -- violating DSP_POLICY invariants 7/8 in the first
    // blocks. prepare() has just cleared all delay/filter state, so snapping
    // is inaudible; the two blend crossfades were already settled from p above
    // (the same treatment the continuous set was missing). matchGainSmooth is
    // deliberately excluded by snapSmoothers (its own glide/injection).
    snapSmoothers();

    // ...and the same treatment for the MODULES' own internal smoothers, which
    // snapSmoothers does not reach (ER-DSP-09, round 20). Each of these modules
    // already snaps in its OWN prepare() -- but that necessarily ran further up
    // this function, BEFORE updateDerived() pushed the restored snapshot in, so it
    // snapped to whatever targets existed beforehand: a fresh instance's defaults,
    // or the previous session's on a reused one. reset() above then re-zeroed the
    // chorus blend outright. The result was that a restored non-default session
    // GLIDED into its own sound over the first ~10-100 ms instead of opening in
    // it. Measured before this line existed (--restore-fade-probe, first block vs
    // the twelfth): Haas 0.17, Velvet 0.09, Chorus 0.29, Dimension-D 0.39 and the
    // Mono Maker crossover 0.35 of their settled values.
    //
    // Placed HERE, not inside reset() and not inside snapSmoothers(): reset() also
    // runs at the silent bottom of a switch duck and on the NaN self-heal, and
    // snapSmoothers() is called from that duck path too (see the switch handler),
    // so folding this in would change how live edits settle. prepare() is the one
    // moment where snapping is unambiguously right -- all delay and filter state
    // has just been cleared, so there is nothing for a glide to protect.
    haas.snapToTargets();
    velvet.snapToTargets();
    chorus.snapToTargets();
    monoMaker.snapToTargets();
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
    osCompDelayBuffer.clear();
    osCompDelayWrite = 0;
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
    dryDuck = false;
    dryDuckLat = 0;
    // ...and the forced-duck flag, which belongs to the same group and was the one
    // member this flush used to miss (ER-DSP-07). A FORCED duck still fading when the
    // host re-prepares would otherwise leave it latched true underneath a Normal
    // switchState -- and the Level-Match consumer at the end of process() runs only
    // `if (! pendingForced)`, so an injected trim was dropped for the rest of the
    // session rather than adopted (#23). Clearing it here is what makes the flushed
    // state actually steady: the duck it described has just been resolved above.
    pendingForced = false;
    bypassBlend.setCurrentAndTargetValue (p.bypass ? 1.0f : 0.0f); // settle the crossfade
    mbEnableBlend.setCurrentAndTargetValue (p.mbEnable ? 1.0f : 0.0f); // settle the multiband crossfade
    mbRunning = p.mbEnable; // reset() above cleaned the bank: warm iff multiband is on
    // Settle the OS-path crossfade for the state we have just flushed to, exactly
    // as the Bypass and Multiband Enable crossfades are settled above.
    osBlend.setCurrentAndTargetValue (osActiveFor (p) ? 1.0f : 0.0f);
    osRunning = osActiveFor (p); // reset() cleared the oversamplers: warm iff engaged
}

// ---------------------------------------------------------------------------
//  Discrete controls: changing any of these mid-stream would step the signal,
//  so they are swapped in only while the output is ducked to silence. Polarity
//  is excluded (it already ramps through zero via its own smoother).
// ---------------------------------------------------------------------------
// Bitwise whole-snapshot equality (Wave 5). Floats compare by BIT PATTERN
// (NaN == NaN, +0 != -0): the gate's question is "is this snapshot the very
// same bytes the engine already adopted?", so identical bits must gate and
// any bit difference must fall through to the normal adopt path. Like
// discreteDiffers/copyContinuous, this list must name EVERY EngineParameters
// field -- a field added there and forgotten here would have its edits
// ignored whenever nothing else moves.
bool AnamorphEngine::sameParameters (const EngineParameters& a, const EngineParameters& b) noexcept
{
    auto sameF = [] (float x, float y) noexcept
    {
        return std::memcmp (&x, &y, sizeof (float)) == 0;
    };
    return a.channelMode == b.channelMode
        && a.monoSum     == b.monoSum
        && a.swapLR      == b.swapLR
        && sameF (a.inputBalance, b.inputBalance)
        && a.polarityL   == b.polarityL
        && a.polarityR   == b.polarityR
        && a.msMode      == b.msMode
        && sameF (a.driveDb, b.driveDb)
        && a.algorithm   == b.algorithm
        && sameF (a.algoAmount, b.algoAmount)
        && sameF (a.haasDelayMs, b.haasDelayMs)
        && a.haasSide    == b.haasSide
        && sameF (a.velvetDensity, b.velvetDensity)
        && sameF (a.chorusRate, b.chorusRate)
        && sameF (a.chorusDepth, b.chorusDepth)
        && a.dimMode     == b.dimMode
        && sameF (a.width, b.width)
        && a.mbEnable    == b.mbEnable
        && a.mbBands     == b.mbBands
        && a.mbSolo      == b.mbSolo
        && sameF (a.mbFreqLow, b.mbFreqLow)
        && sameF (a.mbFreqMid, b.mbFreqMid)
        && sameF (a.mbFreqHigh, b.mbFreqHigh)
        && sameF (a.mbWidthLow, b.mbWidthLow)
        && sameF (a.mbWidthMid, b.mbWidthMid)
        && sameF (a.mbWidthHiMid, b.mbWidthHiMid)
        && sameF (a.mbWidthHigh, b.mbWidthHigh)
        && a.monoMakerEnable == b.monoMakerEnable
        && sameF (a.monoMakerFreq, b.monoMakerFreq)
        && sameF (a.mix, b.mix)
        && sameF (a.outputGainDb, b.outputGainDb)
        && sameF (a.outputBalance, b.outputBalance)
        && a.autoGainMatch == b.autoGainMatch
        && a.solo        == b.solo
        && a.oversample  == b.oversample
        && a.bypass      == b.bypass;
}

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
        // Engaging / disengaging the OS WRAP (Drive crossing 0.01 dB, or Algorithm
        // crossing the mod boundary, with a factor selected) is NOT listed either,
        // for the same reason and since 0.9.7: it is a click-free crossfade between
        // the two paths (`osBlend`), which is a mechanism ADR-0034 made available by
        // giving both paths the same latency. Ducking it was worse than useless --
        // the duck's gain lands downstream of Haas and Velvet, so it masked the
        // output while letting the handover's discontinuity into their delay lines
        // at full level, to re-emerge 12-35 ms later with the fade already over.
        // An OS FACTOR change (`oversample`) IS listed above and still ducks: that
        // one moves the reported latency, so the two paths are not aligned.
        ;
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

    // Begin (or re-begin) a forced duck: mark it forced and latch the dry-fill
    // decision against the state being heard RIGHT NOW (getLatencySamples() tracks
    // the latched p.oversample; since ADR-0034 that -- not the wrap's engagement --
    // is what the number follows, so a swap that merely crosses the Drive threshold
    // with a factor selected is latency-NEUTRAL and now KEEPS its dry fill where it
    // used to dip to silence). dryDuckLat is fixed for this duck -- the state heard
    // through its fade-out equals the one heard through its fade-in, so a single
    // read offset is valid and can never jump mid-fade. Dry-fill is engaged only
    // when the swap keeps the reported latency (else the offset would step by the
    // latency delta at full dry weight; the host is re-aligning its PDC anyway).
    // Called at every FRESH fade-out entry, so a second swap never inherits the
    // previous swap's stale dryDuck/dryDuckLat.
    auto beginForcedDuck = [this] (const EngineParameters& target)
    {
        pendingForced = true;
        dryDuckLat    = getLatencySamples();
        dryDuck       = (predictLatency (target) == dryDuckLat);
        // Present the fill at the output-stage level being heard RIGHT NOW
        // (Output Gain -- or the Match gain that replaces it -- and Output
        // Balance), latched for the duck's lifetime like dryDuckLat: the
        // smoothers snap at the silent bottom where the fill carries full
        // weight, so a live gain would step there. Without this the fill
        // played the raw ring at unity and burst in up to |Output Gain| dB
        // louder than the surrounding audio (undo/redo Mix toggle at -24 dB).
        const float fg = p.autoGainMatch ? matchGainSmooth.getCurrentValue()
                                         : outGainSmooth.getCurrentValue();
        const float fb = outBalanceSmooth.getCurrentValue();
        dryDuckGainL = fg * ((fb > 0.0f) ? (1.0f - fb) : 1.0f);
        dryDuckGainR = fg * ((fb < 0.0f) ? (1.0f + fb) : 1.0f);
    };

    if (switchState == SwitchState::Normal)
    {
        if (forceDuck)
        {
            // Keep the OLD state live through the fade-out; swap it all at the bottom.
            pendingP = np;
            pendingAlgoReset = (np.algorithm != p.algorithm);
            beginForcedDuck (np);
            switchState = SwitchState::FadeOut;
        }
        else if (discreteDiffers (np, p))
        {
            // A Drive move across 0.01 dB no longer reaches here at all: the OS-path
            // swap it causes is a crossfade in process(), not a discrete change, so
            // an ordinary knob move opens no duck of any kind. What is left here is
            // the genuinely discrete set -- routing, algorithm, band count, the OS
            // FACTOR -- and it keeps its duck-to-silence behaviour unchanged.
            pendingP = np;
            pendingAlgoReset = (np.algorithm != p.algorithm);
            copyContinuous (p, np);          // knobs respond immediately
            dryDuck = false;                 // ordinary discrete duck: duck-to-silence (unchanged)
            switchState = SwitchState::FadeOut;
            updateDerived();
        }
        else
        {
            // Steady-state no-change gate (Wave 5): during normal playback the
            // wrapper rebuilds and hands in a snapshot EVERY block; when it is
            // bit-identical to the adopted state, re-adopting it and re-running
            // updateDerived (2 decibelsToGain pow calls + ~25 module setters +
            // ~12 smoother-target stores, all producing the already-set values)
            // is pure per-block waste. updateDerived's one non-pure input --
            // loudness.getMatchGainDb() for the matchGainSmooth target -- is
            // re-applied FRESH by process() every block (the matchTarget
            // refresh), so skipping it here loses nothing. Bitwise field
            // compare: an unchanged snapshot (even a pathological NaN one)
            // behaves exactly as its previous block did.
            if (! sameParameters (np, p))
            {
                p = np;                      // continuous-only change
                updateDerived();
            }
        }
    }
    else
    {
        // Mid-duck: remember the latest target.
        pendingP = np;
        if (switchState == SwitchState::FadeIn && (forceDuck || discreteDiffers (np, p)))
        {
            // A new discrete change (or a forced bulk swap) arrived as we were
            // fading back in: duck again. pendingForced was cleared at the previous
            // bottom, so this duck's forced-ness is exactly the new forceDuck.
            pendingAlgoReset = (np.algorithm != p.algorithm);
            if (forceDuck) beginForcedDuck (np);      // re-latch against the state heard now
            else         { pendingForced = false; dryDuck = false; } // ordinary discrete re-duck
            switchState = SwitchState::FadeOut;
        }
        else if (pendingForced)
        {
            // Still fading on a forced duck and the target moved (a retarget during
            // fade-out, or a forced swap during fade-out). Only TIGHTEN dry-fill: a
            // new target that turns the swap latency-crossing disables it; never
            // re-enable mid-fade (that would jump the offset) and never move
            // dryDuckLat (its L_out is fixed for the duck).
            dryDuck = dryDuck && (predictLatency (pendingP) == dryDuckLat);
        }
        else if (forceDuck)
        {
            // A forced bulk swap arrived while an ORDINARY duck is still fading
            // OUT (the FadeIn case re-ducks above; a forced fade-out is the tighten
            // branch). The request was already consumed from duckRequest, so it
            // must be captured here or the swap would finish with normal-duck
            // semantics: no wholesale swap at the bottom, no smoother snap, no
            // clean-slate reset -- stale delay-line/oversampler contents would
            // replay as the fade lifts (the A/B "weird sound" this path exists to
            // prevent). Upgrade the in-flight duck in place: same fade, forced
            // bottom. Dry-fill stays OFF -- it was never on for this duck (ordinary
            // entries set dryDuck = false) and engaging it mid-fade would step the
            // fill in at the current dry weight, the same no-mid-fade-enable rule
            // as the tighten above; this narrow window keeps plain duck-to-silence.
            pendingForced = true;
            dryDuck       = false;
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
    // The OS-path crossfade is deliberately NOT snapped here. snapSmoothers() runs
    // from the duck bottom, which is BEFORE the OS stage sets `osBlend`'s target for
    // the adopted state -- so snapping would land it on the outgoing target and the
    // stage would immediately start a fresh blend anyway (measured: no change at
    // all). The forced-swap branch settles it explicitly instead, where the new `p`
    // is already in force.
    // matchGainSmooth is left to the injection / loudness re-measure (its own glide).
}

// The oversampler for the SELECTED FACTOR, or nullptr when no factor is selected.
// It carries no engagement gate of its own since 0.9.7 -- whether the wrap RUNS
// this block is `osRunning`, decided by the crossfade in process(), and the caller
// applies it. `p.oversample` is discrete, so this can only change at a silent duck
// bottom or a reset.
juce::dsp::Oversampling<float>* AnamorphEngine::currentOversampler() noexcept
{
    switch (p.oversample)
    {
        case OversampleFactor::x2: return os2.get();
        case OversampleFactor::x4: return os4.get();
        case OversampleFactor::x8: return os8.get();
        default:                   return nullptr;
    }
}

// THE LATENCY OF A FACTOR, NOT OF A PARAMETER STATE (ADR-0034). Both accessors
// read the oversampling SELECTION and nothing else -- deliberately NOT
// `osEngaged` / `osActiveFor`, which say whether the wrap is RUNNING. Those two
// questions used to share one answer, and that is precisely the defect: with a
// factor selected, Drive crossing 0.01 dB or Algorithm crossing into a mod
// algorithm moved the reported PDC between 0 and the factor's latency, and hosts
// answer a latency change by restarting the graph -- heard as a dropout on an
// ordinary knob move. The wrap is still skipped in that state (the CPU saving is
// untouched); `osCompDelayBuffer` supplies its delay instead, so the chain really
// does carry this number whenever the factor is selected. Latched, because
// `oversample` is a discrete control: it changes only at a silent duck bottom or
// in reset(), never mid-block.
static int osLatencyFor (OversampleFactor f,
                         const std::atomic<int>& l2,
                         const std::atomic<int>& l4,
                         const std::atomic<int>& l8) noexcept
{
    switch (f)
    {
        case OversampleFactor::x2: return l2.load (std::memory_order_relaxed);
        case OversampleFactor::x4: return l4.load (std::memory_order_relaxed);
        case OversampleFactor::x8: return l8.load (std::memory_order_relaxed);
        case OversampleFactor::Off:
        default:                   return 0;
    }
}

int AnamorphEngine::getLatencySamples() const noexcept
{
    return osLatencyFor (p.oversample, latency2, latency4, latency8);
}

int AnamorphEngine::predictLatency (const EngineParameters& e) const noexcept
{
    return osLatencyFor (e.oversample, latency2, latency4, latency8);
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
    // Settled identity fast path (H10, 0.8.9): in the default routing (Stereo,
    // no swap, no M/S, no mono-sum) with the balance smoother settled at
    // exactly 0 (-> gL = gR = 1) and both polarity smoothers settled at
    // exactly +1, every sample computes l = L[i] * 1 * 1 -- a bitwise
    // identity -- and a settled SmoothedValue::getNextValue() is
    // mutation-free, so skipping the loop is state-identical. Exact
    // compares, no epsilon (the S4 idiom); any smoothing or non-default
    // routing keeps the exact per-sample path.
    if (p.channelMode == ChannelMode::Stereo && ! p.swapLR && ! p.msMode && ! p.monoSum
        && ! balanceSmooth.isSmoothing() && ! (std::abs (balanceSmooth.getCurrentValue()) > 0.0f)
        && ! polLSmooth.isSmoothing() && ! (std::abs (polLSmooth.getCurrentValue() - 1.0f) > 0.0f)
        && ! polRSmooth.isSmoothing() && ! (std::abs (polRSmooth.getCurrentValue() - 1.0f) > 0.0f))
        return;

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

// Rational tanh for the drive waveshaper (H3, Wave 2). Odd minimax rational
// x*P(x^2)/Q(x^2) (degree 9/8, coefficients fitted for RELATIVE error over
// [0, 9.2]) replacing the two per-sample libm tanh calls (~55 % of every
// oversampling delta; their internal range reduction owned 35.8 % of engine
// branch mispredicts). Call-free straight-line arithmetic; the two clamps
// compare against fixed thresholds that real audio essentially never crosses,
// so their branches stay predicted (GCC keeps the loop scalar without
// fast-math -- measured 15.2 -> 3.9 ns/sample, 3.9x, on the kernel bench;
// packed 4/8-wide would need intrinsics and is left for a future round).
// Properties the drive stage relies on, all verified against double std::tanh
// on a 4M-point sweep: exact 0 at 0 (identity-at-silence), hard saturation to
// exactly +/-1 beyond the clamp (never overshoots full scale), odd symmetry,
// max relative error 3.5e-7 (~3 ulp; class B per the Round-2 report), and
// peak preservation within 1 ulp when paired with the same-kernel makeup
// 1/driveTanh(g). Using the SAME kernel for the makeup keeps full-scale
// mapping exact by construction: driveTanh(g*1) * (1/driveTanh(g)) == 1.
static inline float driveTanh (float x) noexcept
{
    x = juce::jlimit (-9.2f, 9.2f, x);
    const float t = x * x;
    const float num = (((1.30678566e-08f * t + 2.04071515e-05f) * t
                        + 3.48355893e-03f) * t + 1.33709871e-01f) * t + 1.0f;
    const float den = (((7.66062875e-07f * t + 3.26578727e-04f) * t
                        + 2.58314903e-02f) * t + 4.67043060e-01f) * t + 1.0f;
    return juce::jlimit (-1.0f, 1.0f, x * (num / den));
}

void AnamorphEngine::processNonlinearRegion (float* L, float* R, int n, double rate,
                                             bool runMod, int envStride) noexcept
{
    // Run the drive maths while Drive is engaged OR while the blend is still
    // gliding back to zero, so disengaging Drive fades out instead of stepping.
    if (driveActive || driveBlendSmooth.isSmoothing())
    {
        // Smoothed tanh drive with PEAK-preserving makeup (1/tanh(g)): a
        // full-scale input still maps to full scale, so driving harder adds
        // saturation/density without dropping the level (feedback #23). The blend
        // crossfades from the clean signal so 0 dB is identity (feedback #13).
        if (! driveSmooth.isSmoothing() && ! driveBlendSmooth.isSmoothing())
        {
            // Settled: g and blend are constants for the whole block (a settled
            // SmoothedValue returns its target without mutating), so the makeup
            // 1/tanh(g) is too -- computed once instead of per sample (S6a);
            // any glide takes the original per-sample path below untouched.
            const float g     = juce::jmax (1.0f, driveSmooth.getNextValue());
            const float blend = driveBlendSmooth.getNextValue();
            const float c = 1.0f / driveTanh (g);
            // One channel per pass: every sample is independent, so splitting
            // the interleaved loop is bit-identical -- and it removes the L/R
            // aliasing question that blocked auto-vectorizing the kernel.
            for (float* ch : { L, R })
                for (int i = 0; i < n; ++i)
                {
                    const float s = driveTanh (g * ch[i]) * c;
                    ch[i] += blend * (s - ch[i]);
                }
        }
        else if (envStride <= 1)
        {
            for (int i = 0; i < n; ++i)
            {
                const float g     = juce::jmax (1.0f, driveSmooth.getNextValue());
                const float blend = driveBlendSmooth.getNextValue();
                const float c = 1.0f / driveTanh (g);
                const float sl = driveTanh (g * L[i]) * c;
                const float sr2 = driveTanh (g * R[i]) * c;
                L[i] += blend * (sl  - L[i]);
                R[i] += blend * (sr2 - R[i]);
            }
        }
        else
        {
            // OS-path crossfade only. One envelope step per BASE sample, held across
            // the oversampled group: the ramp then advances at the same wall-clock
            // rate as the base-rate path's, which is what makes the two paths
            // mixable. Ticking per OVERSAMPLED sample -- what the loop above does,
            // correctly, when it is the only path running -- would run the ramp
            // `factor` times faster here, and the two paths would diverge by however
            // far it had got. Measured before this branch, on an instantaneous
            // 0 -> 6 dB step, as a multiple of the settled sample-to-sample step:
            // 2.0x at 2x, 4.0x at 4x, 7.3x at 8x -- scaling with the factor.
            float g = 1.0f, blend = 0.0f, c = 1.0f;
            int hold = 0;
            for (int i = 0; i < n; ++i)
            {
                if (hold == 0)
                {
                    g     = juce::jmax (1.0f, driveSmooth.getNextValue());
                    blend = driveBlendSmooth.getNextValue();
                    c     = 1.0f / driveTanh (g);
                    hold  = envStride;
                }
                --hold;
                const float sl  = driveTanh (g * L[i]) * c;
                const float sr2 = driveTanh (g * R[i]) * c;
                L[i] += blend * (sl  - L[i]);
                R[i] += blend * (sr2 - R[i]);
            }
        }
    }

    if (runMod && isModAlgorithm (p.algorithm))
    {
        chorus.setWorkingRate (rate);
        chorus.processBlock (L, R, n);
    }
}

// ---------------------------------------------------------------------------
// ANAMORPH_NONBLOCKING IS REPEATED HERE, on the definition, and not only on the
// declaration in the header. Clang merges function effects across
// redeclarations, so the enforcement reached this body either way -- the
// seeded-allocation run that aborts at exit 43 from inside it proves that. Two
// things are wrong with relying on the merge. It is a compiler behaviour rather
// than a property of this code, and `-Wfunction-effect-redeclarations` exists
// precisely because Clang reserves the right to complain about the split; a
// future major tightening it would surface a new first-party diagnostic in the
// `linux-clang` warning gate, for a file that had not changed. And a reader of
// this translation unit, which is where the DSP chain actually lives, saw no
// sign that the body is under a realtime contract at all. The macro is inert on
// every compiler that lacks the attribute and changes no codegen on the one that
// has it (ADR-0029 Evidence), so saying it twice costs nothing.
void AnamorphEngine::process (juce::AudioBuffer<float>& buffer) noexcept ANAMORPH_NONBLOCKING
{
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n <= 0) return;

    // A host that exceeds the prepared maximum block size (JUCE documents this
    // host class as real: prepareToPlay's samplesPerBlock is "a strong hint",
    // to be handled defensively) would overrun every maxBlock-sized scratch
    // buffer below and the oversamplers' initProcessing size. Split such a
    // block into <= maxBlock slices: each slice takes the identical path a
    // conforming host block takes, so behaviour for in-contract hosts is
    // bit-unchanged (single slice), and out-of-contract hosts get correct
    // audio instead of a heap overflow. Stack views only -- no allocation
    // (AudioBuffer's preallocated-pointer constructor, 2 <= 32 channels).
    if (n > maxBlock)
    {
        for (int start = 0; start < n; start += maxBlock)
        {
            float* slicePtrs[2] = { buffer.getWritePointer (0) + start,
                                    buffer.getWritePointer (1) + start };
            juce::AudioBuffer<float> slice (slicePtrs, 2, juce::jmin (maxBlock, n - start));
            process (slice); // depth-1 recursion: every slice is <= maxBlock
        }
        return;
    }

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
        // Only a FACTOR change now reaches this: engaging/disengaging the wrap is a
        // crossfade and never opens a duck. A factor change moves the reported
        // latency and swaps to a different filter, so both the oversamplers and the
        // stand-in ring are restarted here, at silence.
        const bool osPathChanged = pendingP.oversample != p.oversample;
        p = pendingP;
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
            // The stand-in ring is the other half of this swap -- the delay moves
            // between the wrap and the ring here -- so it holds audio from the last
            // time IT ran for exactly the same reason, and would replay it as the
            // duck lifts. Clearing at the silent bottom is inaudible, and the ring
            // refills within `lat` samples (4-6), far inside the ~28 ms fade-in.
            osCompDelayBuffer.clear();
            osCompDelayWrite = 0;
            // AND LAND THE PATH CROSSFADE ON THE STATE JUST ADOPTED. `osBlend` is a
            // crossfade between two paths that ADR-0034 made sample-aligned; a FACTOR
            // change is the one OS transition that breaks that alignment (it moves the
            // reported latency), which is why it ducks instead. A blend left in flight
            // across this bottom therefore spans two states it was never valid for --
            // and in the ->Off direction it weights a wrapped path that no longer
            // exists at all, so the mix hands back the RAW input and Drive and the mod
            // algorithms vanish for the 12 ms of the ramp, at full level into Haas's
            // and Velvet's delay lines. Measured with a 1 kHz probe, third-harmonic
            // ratio H3/H1 (gain-invariant, so the duck cannot move it): 0.289 settled,
            // 0.103 at the bottom, 12 ms to recover -- against a flat 0.288 through
            // the identical duck on the 2x->4x control (Test 54). The duck already gives us
            // silence and every path was just reset two lines up, so the correct
            // handover here is the same one the forced swap below takes: land it.
            osBlend.setCurrentAndTargetValue (osActiveFor (p) ? 1.0f : 0.0f);
            osRunning = osActiveFor (p);
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
            // The stand-in ring belongs to this list for the same reason the three
            // oversamplers do -- it is stateful and holds pre-swap audio. Unlike the
            // osPathChanged clear above this one runs on EVERY forced duck, including
            // ones where the ring keeps running; that is deliberate and matches the
            // oversamplers beside it, and the resulting `lat` samples of zeros (4-6)
            // are emitted at switchPhase 0 with a ~28 ms fade-in ahead of them.
            osCompDelayBuffer.clear();
            osCompDelayWrite = 0;
            // Land the OS-path crossfade on the adopted state, like every other
            // control at this bottom. Without it the fade-in mixes IN from the
            // base-rate path -- which, with the drive smoothers snapped to a large
            // new value one line above, means ~12 ms of the nonlinear stage running
            // UNDERSAMPLED, the one thing the wrap exists to avoid. The blend is a
            // click-free mechanism for a LIVE flip; a forced swap already has
            // silence, so it does not need one, and taking it makes the fade-in play
            // a single settled path. Safe here for the usual reason: the wrap and
            // the ring were both cleared two lines up, and every delay line
            // downstream of them was emptied too.
            osBlend.setCurrentAndTargetValue (osActiveFor (p) ? 1.0f : 0.0f);
            osRunning = osActiveFor (p);
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
        dryDuck = false; // the dry fill ends with the duck (weight already 0 at phase 1)
    }
    const bool fading = (switchState != SwitchState::Normal);
    // Dry-filled forced duck: while fading, stage 5 blends the output toward the
    // delay-aligned raw input (bypassDryScratch) instead of toward silence.
    const bool duckDry = fading && dryDuck;

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
        // H9 (0.8.9): the delay-aligned read-back feeds exactly TWO consumers --
        // the Bypass crossfade at the end of the chain (its own gate below) and
        // the dry-filled forced duck in stage 5 (gate: duckDry) -- so the fill
        // runs under the UNION of those two gates, and with Bypass fully off and
        // settled and no forced duck in flight (the normal state) nothing ever
        // reads bypassDryScratch this block. The ring WRITES always happen
        // (history must stay warm so a later Bypass engage / forced duck reads
        // valid delay-aligned input); only the dead read-back is skipped. The
        // conditions cannot change between here and the consumers: the blend
        // target is set once per block in setParameters, isSmoothing() only
        // advances inside the crossfade's own getNextValue calls, and duckDry is
        // latched above for the whole block.
        // Read offset: a dry-filled duck reads at the offset latched when the duck
        // began (dryDuckLat). Dry-fill is engaged ONLY when the swap keeps the
        // reported latency -- since ADR-0034 that test reduces to "the swap keeps
        // the oversampling FACTOR", which is the honest statement of it
        // (setParameters gates dryDuck on predictLatency(target)
        // == getLatencySamples(), and a same-duck retarget that turns the swap
        // latency-crossing ANDs dryDuck back to false -- it is never re-enabled
        // mid-fade). So whenever duckDry is true here, the heard latency has not
        // changed across the duck and dryDuckLat == lat: the dry read is always
        // aligned, and the shared-offset case with the Bypass crossfade (both
        // reading at the same offset) can never carry a latency mismatch. Outside a
        // dry duck the Bypass crossfade reads at the live latency as before.
        const bool bypassAudible = bypassBlend.isSmoothing()
                                || bypassBlend.getTargetValue() > 0.0f;
        float* bxL = bypassDryScratch.getWritePointer (0);
        float* bxR = bypassDryScratch.getWritePointer (1);
        if (bypassAudible || duckDry)
        {
            const int readLat = duckDry ? dryDuckLat : lat;
            for (int i = 0; i < n; ++i)
            {
                bdL[bypassDelayWrite] = L[i];
                bdR[bypassDelayWrite] = R[i];
                int rp = bypassDelayWrite - readLat; if (rp < 0) rp += bdSize;
                bxL[i] = bdL[rp]; bxR[i] = bdR[rp];
                // Wrap by branch, not %: the index advances by exactly 1 from
                // within [0, size), so this is integer-identical and avoids a
                // hardware division per sample (S6b; matches the read wrap above).
                if (++bypassDelayWrite >= bdSize) bypassDelayWrite = 0;
            }
        }
        else
        {
            // Write-only fill (the normal state, every block): identical ring
            // bytes as the per-sample loop, in at most two contiguous copies
            // (Wave 4). The read-back branch above is left per-sample: its
            // reads can overlap this block's own writes when readLat < n.
            int i = 0;
            while (i < n)
            {
                const int seg = juce::jmin (n - i, bdSize - bypassDelayWrite);
                juce::FloatVectorOperations::copy (bdL + bypassDelayWrite, L + i, seg);
                juce::FloatVectorOperations::copy (bdR + bypassDelayWrite, R + i, seg);
                bypassDelayWrite += seg;
                if (bypassDelayWrite >= bdSize) bypassDelayWrite = 0;
                i += seg;
            }
        }
    }

    // -------- Input conditioning --------------------------------------------
    applyInputConditioning (L, R, n);

    // M/S Solo lives in the INPUT module: it isolates Mid or Side BEFORE the
    // widening engine (feedback #15), so soloing Side on mono content stays
    // silent even as Amount is raised, and Output Balance still applies.
    if (p.solo == SoloMode::Mid)
        for (int i = 0; i < n; ++i) { const float m = (L[i] + R[i]) * 0.5f; L[i] = m; R[i] = m; }
    else if (p.solo == SoloMode::Side)
        for (int i = 0; i < n; ++i) { const float s = (L[i] - R[i]) * 0.5f; L[i] = s; R[i] = -s; }

    // DRY for the dry/wet Mix = the full conditioned input. Mono Maker now runs
    // POST-Mix, so nothing is peeled off here; the widener and Multiband see the
    // whole signal. This same buffer doubles as the silence-edge scan's view of
    // the conditioned input (#25): it is written once here and only READ
    // afterwards (the Mix loop and the scan), so the former separate
    // inputScratch copy was byte-identical dead weight (H9, 0.8.9).
    dryScratch.copyFrom (0, 0, L, n);
    dryScratch.copyFrom (1, 0, R, n);

    // -------- Oversampled nonlinear / modulation region ---------------------
    //
    // TWO PATHS, CROSSFADED, NOT SWAPPED (0.9.7). The wrap runs only when it has
    // nonlinear/modulation work to do -- the largest CPU saving in the engine, and
    // untouched here. What changed is the HANDOVER. It used to be a latched swap at
    // the silent bottom of the switch duck, and the duck could not mask it: the
    // duck's gain is applied at the output stage, DOWNSTREAM of Haas (12-35 ms) and
    // Velvet (~21 ms), so the discontinuity went into their delay lines at full
    // level and came back out after the ~28 ms fade-in was over. Measured at the
    // Drive threshold with a 220 Hz tone, as a multiple of the settled
    // sample-to-sample step: 2.6x (Haas) and 5.8x (Velvet), arriving at duck bottom
    // + the widener's own delay, identically at 2x, 4x and 8x.
    //
    // So the two paths are mixed instead. Both are `lat` samples long -- the wrap's
    // group delay on one side, `osCompDelayBuffer` on the other -- which is exactly
    // what ADR-0034 established, and is what makes them sample-aligned and safe to
    // mix. Before ADR-0034 they differed by 4-6 samples and this would have combed.
    //
    // THE POINTER IS THE AUTHORITY ON WHETHER A WRAPPED PATH EXISTS, NOT THE BLEND.
    // `currentOversampler()` is null for exactly one state -- Oversampling Off -- and
    // in that state there is no wrapped buffer to fade from, so a non-zero blend
    // weight would mix toward `osPathScratch` holding nothing but the raw input.
    // Forcing the blend to agree makes that unrepresentable rather than merely
    // unreached: below, `wrapAudible` implies `os != nullptr`, and if the weight were
    // ever stale the output degrades to the correctly-processed base-rate path, never
    // to unprocessed audio. `p.oversample` is discrete, so this can only ever fire at
    // a silent duck bottom -- the same instant the branch above lands it deliberately.
    auto* const os = currentOversampler();
    if (os == nullptr) osBlend.setCurrentAndTargetValue (0.0f);
    osBlend.setTargetValue (osActiveFor (p) ? 1.0f : 0.0f);
    const bool osBlending  = osBlend.isSmoothing();
    const bool wrapAudible = osBlending || osBlend.getCurrentValue() > 0.0f;
    const bool baseAudible = osBlending || osBlend.getCurrentValue() < 1.0f;

    // Cold -> warm, the mbRunning pattern: the wrap's polyphase IIR state is stale
    // (or zero) the instant it starts running again, and its settle is the OTHER
    // half of the defect above -- removing the reset does not help, because a wrap
    // that has not run is already at zero state and still ramps in over its group
    // delay. Starting it HERE, while the blend is still ~0, is what masks that: by
    // the time the blend has risen the filters are warm. The reset makes the start
    // defined rather than a replay of whatever it last held (#3).
    if (wrapAudible && ! osRunning)
    {
        if (os2) os2->reset();
        if (os4) os4->reset();
        if (os8) os8->reset();
        chorus.reset();                 // runs at the OS rate, so it restarts with it
    }
    osRunning = wrapAudible;

    // The wrapped path needs the UNDELAYED input, and the base-rate path overwrites
    // it in place, so snapshot it while a crossfade is in flight.
    if (osBlending)
    {
        osPathScratch.copyFrom (0, 0, L, n);
        osPathScratch.copyFrom (1, 0, R, n);
    }

    // ---- base-rate path: the stand-in delay, then the region ----------------
    // THE RING IS WRITTEN ON EVERY BLOCK, whichever path is audible, and read back
    // only when the base-rate path is. That is what makes the handover continuous
    // in this direction: a ring that were cleared (or left cold) at the swap would
    // hand back `lat` samples of ZEROS, which is precisely the hole that used to
    // reach Haas and Velvet. Write-only costs two vector copies, the same trade the
    // true-bypass ring already makes. DELAY THEN REGION, not the reverse: the ring
    // must carry the raw input so its history means the same thing whether or not
    // the region ran, and at the crossing the region is identity anyway.
    if (lat > 0)
    {
        float* cdL = osCompDelayBuffer.getWritePointer (0);
        float* cdR = osCompDelayBuffer.getWritePointer (1);
        const int cdSize = osCompDelayBuffer.getNumSamples();
        if (baseAudible)
        {
            for (int i = 0; i < n; ++i)
            {
                cdL[osCompDelayWrite] = L[i];
                cdR[osCompDelayWrite] = R[i];
                // lat >= 1 here, so the read index is never the one just written:
                // correct in place, no scratch copy. Wrap by branch, not % (S6b,
                // see the bypass ring): the index advances by exactly 1 from
                // within [0, size), so this is integer-identical and avoids a
                // hardware division per sample.
                int rp = osCompDelayWrite - lat; if (rp < 0) rp += cdSize;
                L[i] = cdL[rp]; R[i] = cdR[rp];
                if (++osCompDelayWrite >= cdSize) osCompDelayWrite = 0;
            }
        }
        else
        {
            // Write-only fill: identical ring bytes as the loop above, in at most
            // two contiguous copies (the Wave 4 trade the bypass ring documents).
            int i = 0;
            while (i < n)
            {
                const int seg = juce::jmin (n - i, cdSize - osCompDelayWrite);
                juce::FloatVectorOperations::copy (cdL + osCompDelayWrite, L + i, seg);
                juce::FloatVectorOperations::copy (cdR + osCompDelayWrite, R + i, seg);
                osCompDelayWrite += seg;
                if (osCompDelayWrite >= cdSize) osCompDelayWrite = 0;
                i += seg;
            }
        }
    }
    // BOTH PATHS MUST SEE THE SAME DRIVE ENVELOPE. `processNonlinearRegion` ADVANCES
    // `driveSmooth` and `driveBlendSmooth` -- once per sample, so the wrapped call
    // advances them `factor` times as far as the base-rate call over the same block.
    // With only ever one path running that was invisible; running both in one block
    // makes it a real desynchronisation, and the two paths then differ by however
    // far the drive ramp has diverged. Measured before this save/restore, as a
    // multiple of the settled sample-to-sample step at a 0 -> 6 dB Drive step:
    // 2.84x at 2x, 4.05x at 4x, 8.69x at 8x -- scaling with the factor, which is
    // the signature. Both paths therefore run from the SAME smoother state, and the
    // state that survives the block is the one belonging to the path the blend is
    // heading TO, since that is the path that will still be running when it settles.
    const auto driveEntry = driveSmooth;
    const auto blendEntry = driveBlendSmooth;

    if (baseAudible)
        processNonlinearRegion (L, R, n, sr, ! osBlending);

    // ---- wrapped path -------------------------------------------------------
    if (wrapAudible)
    {
        // Rewind to the same entry state the base-rate path started from. With
        // `envStride` set to the factor below, the wrapped call then advances the
        // envelope exactly `n` times too, so the two paths end the block in the same
        // place and no end-state arbitration is needed.
        if (osBlending) { driveSmooth = driveEntry; driveBlendSmooth = blendEntry; }
        // Non-null whenever `wrapAudible` is -- the invariant established above.
        {
            const double factor = (p.oversample == OversampleFactor::x2) ? 2.0
                                : (p.oversample == OversampleFactor::x4) ? 4.0 : 8.0;
            float* wL = osBlending ? osPathScratch.getWritePointer (0) : L;
            float* wR = osBlending ? osPathScratch.getWritePointer (1) : R;
            float* wch[2] = { wL, wR };
            juce::dsp::AudioBlock<float> block (wch, 2, (size_t) n);
            auto osBlock = os->processSamplesUp (block);
            processNonlinearRegion (osBlock.getChannelPointer (0),
                                    osBlock.getChannelPointer (1),
                                    (int) osBlock.getNumSamples(), sr * factor,
                                    true, osBlending ? (int) factor : 1);
            os->processSamplesDown (block);
        }
    }

    // ---- mix them ----------------------------------------------------------
    if (osBlending)
    {
        const float* wL = osPathScratch.getReadPointer (0);
        const float* wR = osPathScratch.getReadPointer (1);
        for (int i = 0; i < n; ++i)
        {
            const float b = osBlend.getNextValue();
            L[i] += b * (wL[i] - L[i]);
            R[i] += b * (wR[i] - R[i]);
        }
    }

    // -------- Linear algorithm at base rate ---------------------------------
    if (p.algorithm == Algorithm::Haas)        haas.processBlock (L, R, n);
    else if (p.algorithm == Algorithm::Velvet) velvet.processBlock (L, R, n);

    // -------- Global Width (MS-domain) --------------------------------------
    // Settled hoist (Wave 5): a settled SmoothedValue's getNextValue() returns
    // `target` without mutating anything (the same library semantics H1/H10
    // already rely on), so hoisting the value feeds applyWidth the identical
    // float every sample and lets the ~9-flop kernel vectorize. A gliding
    // width keeps the original per-sample call verbatim.
    if (! widthSmooth.isSmoothing())
    {
        const float w = widthSmooth.getTargetValue();
        for (int i = 0; i < n; ++i)
            applyWidth (L[i], R[i], w);
    }
    else
        for (int i = 0; i < n; ++i)
            applyWidth (L[i], R[i], widthSmooth.getNextValue());

    // -------- Multiband Width (Advanced) ------------------------------------
    // Reconstruct the dry through the SAME crossover banks as the wet, at unit
    // width -- a phase-matched A(dry) -- so a partial Mix never combs the mono sum
    // (Known Issue #1). Solo-agnostic: the wet always sums every band.
    // Multiband Enable is a short click-free OUTPUT crossfade (the bypassBlend model),
    // NOT a duck: the crossover bank stays WARM across the toggle and its output is
    // faded against the pre-multiband signal, so enabling/disabling never mutes or
    // settles audibly. mbActive keeps the bank running while the blend is non-zero, so
    // a disable fades the multiband OUT over ~12 ms before the bank goes cold.
    bool dryAligned = false;
    // Hoisted from the Level-Match snap gate below (same expression, one value per
    // block): the H4 dry-align gate must also see "Match is about to engage" so the
    // dry bank re-warms THROUGH the engage duck, exactly like the S7 energy scan.
    const bool matchEngaging = switchState != SwitchState::Normal && pendingP.autoGainMatch;
    bool fullWetIdle = false;
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

        // Settled-full-wet dry-align gate (H4, Wave 2). A(dry) has exactly two
        // consumers: the dry/wet blend and the Level-Match reference. With the Mix
        // glide parked at exactly 1 the blend is one LSB-level rounding pass of the
        // wet (out = dry + 1*(wet-dry)); with Match off AND not mid-engage the match
        // target is never read. So when no enable/bypass crossfade is in flight
        // either, the dry bank (6 LR4/sample -- half the multiband cost) and the
        // blend loop are skipped. Class B by design: the gated output is the EXACT
        // wet instead of its m=1 float re-blend, and the live Measure readout
        // follows the delay-aligned CLEAN dry while gated. Both dry delay rings
        // keep being written below, so a Mix dip re-engages against warm lock-step
        // history; the dry bank's cutoffs can never drift while gated -- they are
        // fixed per crossover bank and always assigned with the wet's at a fade
        // start (0.8.10, KI #1). Exact compares, no epsilon.
        fullWetIdle = ! blending
                   && ! mixSmooth.isSmoothing()
                   && ! (std::abs (mixSmooth.getCurrentValue() - 1.0f) > 0.0f)
                   && ! p.autoGainMatch && ! matchEngaging
                   && ! bypassBlend.isSmoothing()
                   && ! (bypassBlend.getCurrentValue() > 0.0f);

        // Keep the pre-multiband signal -- the "off" side of the enable crossfade (the
        // chain output with the multiband NOT applied) -- before processBlock overwrites it.
        if (blending)
        {
            juce::FloatVectorOperations::copy (preMbScratch.getWritePointer (0), L, n);
            juce::FloatVectorOperations::copy (preMbScratch.getWritePointer (1), R, n);
        }

        if (fullWetIdle)
            multiband.processBlock (L, R, n); // dry bank skipped (null dry pointers)
        else
        {
            multiband.processBlock (L, R, n,
                dryScratch.getReadPointer (0), dryScratch.getReadPointer (1),
                dryAlignScratch.getWritePointer (0), dryAlignScratch.getWritePointer (1));
            dryAligned = true;
        }

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

    // ======================== DRY / WET MIX =================================
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

    if (fullWetIdle)
    {
        // H4 gated state: Mix parked at exactly 1, Match off, no crossfade in
        // flight. The output already IS the wet, so the m=1 blend below (one
        // LSB-level rounding pass) is skipped along with the smoothstep -- and a
        // settled mixSmooth tick is mutation-free, so not calling getNextValue()
        // is state-identical (the H1/H10 argument). Everything stateful still
        // runs: both dry delay rings advance in lockstep (warm re-engage) and the
        // Level-Match reference is filled with the delay-aligned dry so the
        // Measure readout keeps tracking while gated.
        for (int i = 0; i < n; ++i)
        {
            ddL[dryDelayWrite] = dL[i];
            ddR[dryDelayWrite] = dR[i];
            adL[dryDelayWrite] = aL[i];
            adR[dryDelayWrite] = aR[i];
            int rp = dryDelayWrite - lat; if (rp < 0) rp += ddSize;
            lrL[i] = adL[rp]; lrR[i] = adR[rp];
            if (++dryDelayWrite >= ddSize) dryDelayWrite = 0;
        }
    }
    else if (! mixSmooth.isSmoothing())
    {
        // Settled Mix (Wave 3): m -- and therefore the dry-source smoothstep --
        // are block constants (a settled getCurrentValue equals the value every
        // settled getNextValue tick would return, and the tick is
        // mutation-free). The per-sample ring writes/reads and the blend are
        // the IDENTICAL expressions of the general loop below evaluated with
        // the same m/ts, so this path is bit-exact; only the per-sample
        // recomputation of those constants goes. This is the steady
        // partial-Mix listening state, where the dry bank also runs (H4
        // cannot gate it) -- the most expensive settled state on record.
        const float m  = mixSmooth.getCurrentValue();
        const float t  = juce::jlimit (0.0f, 1.0f, m * (1.0f / kAlignMix));
        const float ts = t * t * (3.0f - 2.0f * t); // smoothstep: clean -> A(dry)
        for (int i = 0; i < n; ++i)
        {
            ddL[dryDelayWrite] = dL[i];
            ddR[dryDelayWrite] = dR[i];
            adL[dryDelayWrite] = aL[i];
            adR[dryDelayWrite] = aR[i];
            int rp = dryDelayWrite - lat; if (rp < 0) rp += ddSize;
            const float cleanL = ddL[rp], cleanR = ddR[rp];
            const float alignL = adL[rp], alignR = adR[rp];
            if (++dryDelayWrite >= ddSize) dryDelayWrite = 0;

            lrL[i] = alignL; lrR[i] = alignR;

            float dryL = cleanL, dryR = cleanR;
            if (dryAligned)
            {
                dryL = cleanL + ts * (alignL - cleanL);
                dryR = cleanR + ts * (alignR - cleanR);
            }
            L[i] = dryL + m * (L[i] - dryL);
            R[i] = dryR + m * (R[i] - dryR);
        }
    }
    else
    for (int i = 0; i < n; ++i)
    {
        ddL[dryDelayWrite] = dL[i];
        ddR[dryDelayWrite] = dR[i];
        adL[dryDelayWrite] = aL[i];
        adR[dryDelayWrite] = aR[i];
        int rp = dryDelayWrite - lat; if (rp < 0) rp += ddSize;
        const float cleanL = ddL[rp], cleanR = ddR[rp];
        const float alignL = adL[rp], alignR = adR[rp];
        // Wrap by branch, not % (S6b, see the bypass ring): integer-identical
        // for an index in [0, size) advancing by 1. dryDelayBuffer and
        // dryAlignDelayBuffer keep sharing this ONE index, staying in lockstep.
        if (++dryDelayWrite >= ddSize) dryDelayWrite = 0;

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

    // ======================== MONO MAKER (post-Mix) =========================
    // Collapse the low band of the MIXED signal to mono in place, so the final low
    // end is mono whatever the Mix amount. The Mid stays allpass-flat (LP + HP), so
    // the mono sum has no low-frequency cancellation.
    if (p.monoMakerEnable)
        monoMaker.process (L, R, n);

    // ======================== OUTPUT STAGE ==================================
    // Level Match measures the post-Mono-Maker signal (the real processed output)
    // against the conditioned input, BEFORE Output gain / balance (feedback #25).
    // L/R are passed to the matcher directly: nothing modifies them between here
    // and the loudness call, so the former wetScratch copy-then-read-once was
    // byte-identical dead weight (H9, 0.8.9).
    // Feed the predict BOTH big-gain controls so it pre-ducks the instant Drive OR Mix
    // is raised (Drive maxed + Mix 0 -> no boost; Mix to 100% -> pre-duck) (#14/#19).
    loudness.setDriveDb (p.driveDb);
    loudness.setMix     (p.mix);
    // Dry reference = the delay-aligned reconstruction (loudnessRefScratch), NOT the raw
    // input, so the Multiband allpass-reconstruction ripple cancels -> Measure ~0 at
    // unit width with Multiband on (Issue 2). Falls back to the delay-aligned input when
    // Multiband is off.
    loudness.process (loudnessRefScratch.getReadPointer (0), loudnessRefScratch.getReadPointer (1),
                      L, R, n);
    const float matchTarget = p.autoGainMatch
        ? juce::Decibels::decibelsToGain (loudness.getMatchGainDb()) : 1.0f;
    matchGainSmooth.setTargetValue (matchTarget);

    // Silence -> audio edge: SNAP the applied match gain to its (already pre-ducked)
    // target so the first audible block is compensated even if the host never ran the
    // plugin while paused. Click-free: the previous block's output was silent. This is
    // the safety net that makes the predict effective across Transport Stop->Play and
    // Silence->Audio, not just when the host keeps processing during a pause.
    // The energy scan below only feeds the Level-Match snap, so it is skipped
    // while Match is off (S7) -- EXCEPT while a duck that will turn Match ON is
    // in flight (Match is a discrete, always-ducked switch): running the scan
    // through the engage fade refreshes prevInputSilent one block before the
    // swapped-in p.autoGainMatch can first read it, so the snap decision sees
    // exactly the state the always-computed original would have seen -- the
    // silence->audio edge landing on the engage block included.
    //
    // INVARIANT this gate depends on: enabling Match is a discrete change that
    // ALWAYS routes through the switch-duck state machine (autoGainMatch is in
    // discreteDiffers(), so pendingP holds the new value while switchState !=
    // Normal). That is what lets `matchEngaging` warm prevInputSilent before the
    // swap. If Match is ever made to engage WITHOUT a duck (removed from
    // discreteDiffers, or applied live), this gate must be revisited -- the
    // scan would then miss the pre-engage block and the silence->audio snap
    // could be wrong on the first engaged block.
    if (p.autoGainMatch || matchEngaging) // matchEngaging hoisted above the multiband stage (H4)
    {
        double inSq = 0.0;
        {
            const float* il = dryScratch.getReadPointer (0); // == the conditioned input (H9)
            const float* ir = dryScratch.getReadPointer (1);
            for (int i = 0; i < n; ++i) inSq += (double) il[i] * il[i] + (double) ir[i] * ir[i];
        }
        const bool inSilentNow = inSq < 1.0e-6 * (double) juce::jmax (1, n); // ~ -60 dBFS mean-square
        if (prevInputSilent && ! inSilentNow && p.autoGainMatch)
            matchGainSmooth.setCurrentAndTargetValue (matchTarget);
        prevInputSilent = inSilentNow;
    }

    // -------- Output Gain / Auto Gain / Output Balance ----------------------
    {
    // Dry fill for a FORCED duck: blend the ducked output toward the delay-
    // aligned raw input (same source and unity presentation as the true-bypass
    // crossfade) so an undo / redo / A/B / preset swap dips to the DRY signal,
    // never to silence. The processed weight still reaches exactly 0 at the
    // bottom, so the silent-bottom swap semantics above are untouched; with
    // duckDry false this adds exactly nothing (bit-exact original arithmetic).
    const float* bxL = bypassDryScratch.getReadPointer (0);
    const float* bxR = bypassDryScratch.getReadPointer (1);
    // Settled fast path (Wave 3): with no switch duck in flight and all three
    // output smoothers parked, g / gL / gR are block constants and sg == 1, so
    // the constant stereo gain (g * gL) -- the identical product the per-sample
    // path computes, since x * 1.0f is exact -- can be hoisted; the settled
    // getNextValue ticks are mutation-free, so skipping them is
    // state-identical (the H1/H10 argument). At exact unity gain the multiply
    // is skipped outright (x * 1.0f is bitwise x for every finite value; a
    // non-finite sample is zeroed by the scrub below in both variants).
    // duckDry is in the gate defensively: it implies a duck in flight, but if
    // it were ever latched without one the original loop still runs.
    if (! fading && ! duckDry
        && ! outGainSmooth.isSmoothing()
        && ! matchGainSmooth.isSmoothing()
        && ! outBalanceSmooth.isSmoothing())
    {
        const float g  = p.autoGainMatch ? matchGainSmooth.getCurrentValue()
                                         : outGainSmooth.getCurrentValue();
        const float b  = outBalanceSmooth.getCurrentValue();
        const float gL = (b > 0.0f) ? (1.0f - b) : 1.0f;
        const float gR = (b < 0.0f) ? (1.0f + b) : 1.0f;
        const float cL = g * gL;
        const float cR = g * gR;
        if (std::abs (cL - 1.0f) > 0.0f || std::abs (cR - 1.0f) > 0.0f)
            for (int i = 0; i < n; ++i) { L[i] *= cL; R[i] *= cR; }
    }
    else
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
        if (duckDry)
        {
            // Fill at the latched output-stage presentation gain (see the
            // dryDuckGain comment in the header): unity-latched this is the
            // original arithmetic; at extreme Output Gain it stops the fill
            // bursting in louder than the surrounding processed audio.
            const float dw = 1.0f - sg;
            L[i] += dw * dryDuckGainL * bxL[i]; R[i] += dw * dryDuckGainR * bxR[i];
        }
    }
    }

    // ======================== BAND SOLO MONITOR =============================
    // POST-EVERYTHING audition: band-pass the already-produced output to the soloed
    // band(s). No effect stage changed its behaviour for solo -- this only filters what
    // is heard. mask == 0 -> the monitor settles to passGain 1 == BIT-EXACT true output.
    //
    // It MUST be CALLED every block: the monitor is click-free only because its passGain/
    // bandGain crossfade advances on every block in which any gain is unsettled (SoloMonitor's
    // design invariant -- its internal H1 fast path may skip work ONLY in the fully-settled
    // passthrough state, where the output is provably the input). Hard-gating the CALL
    // on the instantaneous p.mbEnable -- which flips with NO duck on the continuous path --
    // bypassed that crossfade and inserted/removed the whole band-pass in a single sample
    // whenever Multiband Enable was toggled with a band soloed (an amplitude + phase step =
    // the click, on both edges). Driving the MASK from p.mbEnable instead (the solo applies
    // only while Multiband is on) lets the monitor MORPH solo<->passthrough over its own
    // ~12 ms ramp, so the toggle is click-free. The mbSolo parameter is untouched; only its
    // application is gated, and at mask 0 the settled monitor is a bit-exact passthrough.
    soloMonitor.process (L, R, p.mbEnable ? p.mbSolo : 0, n);

    // -------- Defensive NaN / Inf self-heal --------------------------------
    // This is NOT a level limiter: it touches ONLY non-finite samples, so valid audio
    // (however loud) is passed through untouched -- no 0 dBFS clipper, dynamics and
    // headroom fully preserved (Issue 1). The crossover Nyquist clamp already prevents
    // the multiband blow-up at the source; if anything ever still went non-finite it
    // would latch a dead channel / poison the meters, so any non-finite sample is
    // replaced with 0 and the stateful nodes are reset to stop the source (self-heal).
    // Detection first (Wave 4): a float is non-finite iff its exponent field is
    // all-ones, and that masked field's numeric MAX over the block reaches
    // 0x7f800000 iff at least one sample is non-finite -- so a branch-free,
    // auto-vectorizable max-reduction replaces the per-sample isfinite branches
    // on the (always-taken) clean path. No false positives, no false negatives;
    // the zeroing pass below is the unchanged original and runs only when the
    // detector fired, so healing behaviour is bit-identical.
    bool nonFinite = false;
    {
        uint32_t worst = 0;
        for (int i = 0; i < n; ++i)
        {
            uint32_t bl, br;
            std::memcpy (&bl, L + i, sizeof (bl));
            std::memcpy (&br, R + i, sizeof (br));
            bl &= 0x7f800000u;
            br &= 0x7f800000u;
            const uint32_t m = bl > br ? bl : br;
            worst = worst > m ? worst : m;
        }
        if (worst == 0x7f800000u)
            for (int i = 0; i < n; ++i)
            {
                if (! std::isfinite (L[i])) { L[i] = 0.0f; nonFinite = true; }
                if (! std::isfinite (R[i])) { R[i] = 0.0f; nonFinite = true; }
            }
    }
    if (nonFinite)
    {
        multiband.reset(); mbRunning = p.mbEnable; monoMaker.reset(); soloMonitor.reset();
        haas.reset(); velvet.reset(); chorus.reset();
        if (os2) os2->reset(); if (os4) os4->reset(); if (os8) os8->reset();
        loudness.reset();
        dryDelayBuffer.clear(); dryAlignDelayBuffer.clear(); bypassDelayBuffer.clear();
        // The oversampling latency stand-in is a delay line on the MAIN path and was
        // written further up this very block, so a non-finite sample is already
        // inside it and would be handed back `lat` samples later -- exactly the
        // re-entry the three rings above are cleared to prevent. Contents only, like
        // them: the write index may keep advancing, since every read now returns 0.
        osCompDelayBuffer.clear();
        // Also flush this block's delay-aligned dry scratch, so the Bypass crossfade
        // below can't re-introduce a non-finite sample from pathological host input.
        bypassDryScratch.clear();
    }

    // ======================== BYPASS CROSSFADE ==============================
    // Click-free, sample-safe Bypass: a short crossfade between the fully processed
    // output and the delay-aligned RAW input -- no mute, no dropout, imperceptible
    // switch (Issue 3). bypassBlend settles to exactly 1 -> bit-exact true bypass; to
    // exactly 0 -> the untouched processed output. Because the chain + analysis already
    // ran above, toggling Bypass never stops Level Match and never re-engages stale DSP.
    //
    // INVARIANT (H9, 0.8.9): the ring fill above -- the only writer of
    // bypassDryScratch -- runs under the UNION of this gate and the dry-duck gate
    // (`bypassAudible || duckDry`), so every consumer's gate must stay a subset of
    // that union: outside it the scratch holds STALE samples. Widening a consumer
    // without the fill reads garbage into the output; widening the fill without a
    // consumer burns a dead per-sample read-back. The ring WRITES themselves are
    // unconditional in both branches, so history is always valid the moment Bypass
    // -- or a dry-filled forced duck -- engages.
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
    // Correlation keeps its per-sample integration (ballistics unchanged); the
    // scope ring is filled in one pass and published with a single release-
    // store per block instead of one per sample (S9, ScopeBuffer::pushBlock).
    for (int i = 0; i < n; ++i)
        correlation.process (L[i], R[i]);
    scope.pushBlock (L, R, n);
    levels.output.process (L, R, n);
    correlation.publish();
    levels.publish();
}

} // namespace anamorph
