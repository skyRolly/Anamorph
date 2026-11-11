#pragma once

// Test 76 extension, R-01: the wet reference is an actual always-active engine.
// The independent long-double convex sum specifies the exceptional affine blend;
// finite original float results must still match their bits, not a tolerance.
static void testBypassFiniteEndpointOverflow()
{
    std::printf ("Test 76 extension: finite endpoints, exceptional interpolation and unchanged normal bits (R-01)\n");
    juce::ScopedNoDenormals noDenormals;
    const auto sameBits = [] (float a, float b) { return std::memcmp (&a, &b, sizeof (float)) == 0; };
    constexpr double rate = 48000.0;
    constexpr int phaseSamples = 512;
    constexpr long double epsilon = (long double) std::numeric_limits<float>::epsilon();
    for (const int block : { 1, 64, 512 })
        for (const auto factor : { anamorph::OversampleFactor::Off, anamorph::OversampleFactor::x2,
                                  anamorph::OversampleFactor::x8 })
            for (const int invertedChannel : { 0, 1 })
                for (int profile = 0; profile < 6; ++profile)
                {
                    // 0 headroom, 1 tiny normal, 2 original witness, 3 float max,
                    // 4 alternating sides of the subtraction-overflow boundary,
                    // 5 headroom through the ACTUAL selected nonlinear OS path.
                    const float gainDb = profile == 3 ? 0.0f : 6.0f;
                    const float gain = juce::Decibels::decibelsToGain (gainDb);
                    float low = (float) ((double) std::numeric_limits<float>::max() / (1.0 + (double) gain));
                    float high = low;
                    for (int i = 0; i < 16; ++i)
                    {
                        low = std::nextafter (low, 0.0f);
                        high = std::nextafter (high, std::numeric_limits<float>::infinity());
                    }
                    const auto inputAt = [profile, low, high, invertedChannel] (int sampleChannel, int sample)
                    {
                        if (sample < 0) return 0.0f;
                        if (sampleChannel != invertedChannel) return 0.25f;
                        if (profile == 1 && sample % 11 == 0) return -0.0f;
                        const float magnitude = profile == 1 ? 1.0e-20f : profile == 2 ? 1.5e38f
                            : profile == 3 ? std::numeric_limits<float>::max()
                            : profile == 4 ? (sample % 2 == 0 ? low : high) : 2.0f;
                        return (sample / 17) % 2 == 0 ? magnitude : -magnitude;
                    };
                    anamorph::EngineParameters p;
                    p.algorithm = anamorph::Algorithm::Haas;
                    p.outputGainDb = gainDb;
                    p.driveDb = profile == 5 ? 6.0f : 0.0f;
                    p.polarityL = invertedChannel == 0;
                    p.polarityR = invertedChannel == 1;
                    p.oversample = factor;
                    auto actual = std::make_unique<anamorph::AnamorphEngine>();
                    auto active = std::make_unique<anamorph::AnamorphEngine>();
                    actual->primeParameters (p); actual->prepare (rate, block);
                    active->primeParameters (p); active->prepare (rate, block);
                    const auto activeP = p;
                    const int latency = actual->getLatencySamples();
                    juce::AudioBuffer<float> work (2, block), wet (2, block);
                    juce::SmoothedValue<float> blend;
                    blend.reset (rate, 0.010); blend.setCurrentAndTargetValue (0.0f);
                    int position = 0, on = 0, off = 0, interiors = 0;
                    int badOn = 0, badOff = 0, badNormal = 0, badWide = 0;
                    int unsafeTarget = 0, safeTarget = 0;
                    bool finite = true, latencySame = true, largeWet = true, rise = false, fall = false;
                    float previousOutput[2] {};
                    long double previousReference[2] {}, previousScale[2] {};
                    bool previousValid[2] {};
                    long double maxStepError = 0.0L, maxCurveError = 0.0L;
                    for (int phase = 0; phase < 3; ++phase)
                    {
                        p.bypass = phase == 1;
                        blend.setTargetValue (p.bypass ? 1.0f : 0.0f);
                        for (int b = 0; b < phaseSamples / block; ++b)
                        {
                            for (int ch = 0; ch < 2; ++ch)
                                for (int i = 0; i < block; ++i)
                                    work.setSample (ch, i, inputAt (ch, position + i));
                            wet.makeCopyOf (work);
                            actual->setParameters (p); active->setParameters (activeP);
                            actual->process (work); active->process (wet);
                            for (int i = 0; i < block; ++i)
                            {
                                const float coefficient = blend.getNextValue();
                                const bool atOn = juce::exactlyEqual (coefficient, 1.0f);
                                const bool atOff = ! (coefficient > 0.0f);
                                if (atOn) ++on;
                                else if (atOff) ++off;
                                else { ++interiors; if (p.bypass) rise = true; else fall = true; }
                                for (int ch = 0; ch < 2; ++ch)
                                {
                                    const float raw = inputAt (ch, position + i - latency);
                                    const float processed = wet.getSample (ch, i);
                                    const float output = work.getSample (ch, i);
                                    finite = finite && std::isfinite (processed) && std::isfinite (output);
                                    if (profile >= 2 && profile <= 4 && ch == invertedChannel
                                        && position + i >= latency)
                                        largeWet = largeWet && std::abs (processed) > 1.0e37f;
                                    const long double weight = (long double) coefficient;
                                    const long double reference = atOn ? (long double) raw : atOff ? (long double) processed
                                        : (1.0L - weight) * (long double) processed + weight * (long double) raw;
                                    const long double scale = std::max ({ std::abs ((long double) processed),
                                                                        std::abs ((long double) raw), 1.0e-30L });
                                    if (atOn) badOn += ! sameBits (output, raw);
                                    else if (atOff) badOff += ! sameBits (output, processed);
                                    else
                                    {
                                        const float original = processed + coefficient * (raw - processed);
                                        if (std::isfinite (original))
                                        {
                                            badNormal += ! sameBits (output, original);
                                            if (ch == invertedChannel) ++safeTarget;
                                        }
                                        else
                                        {
                                            if (ch == invertedChannel) ++unsafeTarget;
                                            const bool bounded = output >= std::min (processed, raw)
                                                              && output <= std::max (processed, raw);
                                            badWide += ! std::isfinite (output) || ! bounded
                                                || std::abs ((long double) output - reference) > 4.0L * epsilon * scale;
                                        }
                                    }
                                    if (std::isfinite (output))
                                    {
                                        maxCurveError = std::max (maxCurveError, std::abs ((long double) output - reference) / scale);
                                        if (previousValid[ch])
                                        {
                                            const long double observedStep = (long double) output - (long double) previousOutput[ch];
                                            const long double idealStep = reference - previousReference[ch];
                                            maxStepError = std::max (maxStepError, std::abs (observedStep - idealStep)
                                                                 / std::max (scale, previousScale[ch]));
                                        }
                                        previousOutput[ch] = output;
                                        previousReference[ch] = reference;
                                        previousScale[ch] = scale;
                                        previousValid[ch] = true;
                                    }
                                    else previousValid[ch] = false;
                                }
                            }
                            latencySame = latencySame && actual->getLatencySamples() == latency
                                                      && active->getLatencySamples() == latency;
                            position += block;
                        }
                    }
                    const bool expectsOverflow = profile >= 2 && profile <= 4;
                    std::printf ("  block %d OS %d channel %d profile %d: unsafe target interiors %d, bad endpoints/normal/wide %d/%d/%d/%d, curve/step error %.3f/%.3f float eps\n",
                                 block, (int) factor, invertedChannel, profile, unsafeTarget,
                                 badOn, badOff, badNormal, badWide,
                                 (double) (maxCurveError / epsilon), (double) (maxStepError / epsilon));
                    check (on > 0 && badOn == 0, "R-01: settled bypass selects exact delay-aligned raw bits");
                    check (off > 0 && badOff == 0, "R-01: bypass off retains the continuously active processed bits");
                    check (badNormal == 0, "R-01: every finite original interior result retains its float bits");
                    check (finite && badWide == 0, "R-01: finite endpoints yield bounded finite exceptional blends near the affine reference");
                    check (largeWet && (expectsOverflow ? unsafeTarget > 0 : unsafeTarget == 0)
                           && (profile != 4 || safeTarget > 0),
                           "R-01 premise: wet stays finite without being muted; the boundary exercises both arithmetic paths");
                    check (maxCurveError <= 4.0L * epsilon && maxStepError <= 8.0L * epsilon,
                           "R-01: switching arithmetic introduces no excess step relative to the affine curve");
                    check (latencySame && interiors > 0 && rise && fall
                           && (factor == anamorph::OversampleFactor::Off ? latency == 0 : latency > 0),
                           "R-01: both ramps retain their sample timing and the selected factor's latency");
                }
}
