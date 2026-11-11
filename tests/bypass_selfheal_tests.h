#pragma once

// Test 59 extension, D-02: a wet-path reset must not erase the validated raw
// scratch or the history a later bypass/forced-duck read will validate again.
static void testSelfHealRetainsRawBypass()
{
    std::printf ("Test 59 extension: wet self-heal preserves raw bypass samples and history (D-02)\n");
    juce::ScopedNoDenormals noDenormals;
    const auto sameBits = [] (float a, float b) { return std::memcmp (&a, &b, sizeof (float)) == 0; };
    const float values[] { 1.0e38f, std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::infinity(),
                          -std::numeric_limits<float>::infinity(), 2.0f, -1.0e-20f };
    constexpr double rate = 48000.0;
    for (const int block : { 1, 64, 512 })
        for (const auto factor : { anamorph::OversampleFactor::Off, anamorph::OversampleFactor::x2,
                                  anamorph::OversampleFactor::x8 })
            for (const int badChannel : { 0, 1 })
                for (const float drive : { 0.0f, 6.0f })
                    for (int kind = 0; kind < 6; ++kind)
                    {
                        const float value = values[kind];
                        anamorph::EngineParameters p;
                        p.algorithm = anamorph::Algorithm::Haas;
                        p.outputGainDb = 24.0f;
                        p.driveDb = drive;
                        p.oversample = factor;
                        p.bypass = true;
                        auto activeP = p; activeP.bypass = false;
                        auto actual = std::make_unique<anamorph::AnamorphEngine>();
                        auto active = std::make_unique<anamorph::AnamorphEngine>();
                        actual->primeParameters (p); actual->prepare (rate, block);
                        active->primeParameters (activeP); active->prepare (rate, block);
                        const int latency = actual->getLatencySamples();
                        // 480 ramp samples plus 32 settled samples, also a whole
                        // block for all sizes here. Poison is isolated, not a
                        // reset on every one-sample process() call.
                        constexpr int phaseSamples = 512;
                        const auto inputAt = [badChannel, value] (int sampleChannel, int sample)
                        {
                            if (sample < 0) return 0.0f;
                            const int phase = sample < 4 * phaseSamples ? sample / phaseSamples
                                            : sample < 9 * phaseSamples ? 4 : 5;
                            const int offset = phase < 4 ? sample % phaseSamples
                                             : sample - (phase == 4 ? 4 : 9) * phaseSamples;
                            const bool pulse = phase < 5 && (offset == 7
                                                || (phase == 4 && offset == 5 * phaseSamples - 1));
                            return sampleChannel == badChannel ? (pulse ? value : 0.125f) : 0.25f;
                        };
                        juce::AudioBuffer<float> work (2, block), wet (2, block);
                        juce::SmoothedValue<float> blend;
                        blend.reset (rate, 0.010); blend.setCurrentAndTargetValue (1.0f);
                        int position = 0, badOn = 0, badOff = 0, badInterior = 0;
                        int on = 0, off = 0, interior = 0, firstMuted = 0;
                        bool finite = true, latencySame = true, measured = true, guardObserved = false;
                        bool rise = false, fall = false;
                        double recoveredEnergy = 0.0;
                        // On -> Off -> On -> Off, hold Off through a forced
                        // duck, then On. A bulk bottom snaps smoothers by contract:
                        // keep bypass settled there, including the block that
                        // retires FadeIn, rather than model that bulk adoption as
                        // a ramp. Poison the END of that write-only leg before re-entry.
                        for (int phase = 0; phase < 6; ++phase)
                        {
                            p.bypass = phase == 0 || phase == 2 || phase == 5;
                            blend.setTargetValue (p.bypass ? 1.0f : 0.0f);
                            if (phase == 4) { actual->requestDuck(); active->requestDuck(); }
                            const int blocks = (phase == 4 ? 5 : 1) * phaseSamples / block;
                            for (int b = 0; b < blocks; ++b)
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
                                    else { ++interior; if (p.bypass) rise = true; else fall = true; }
                                    if (position + i == 7 + latency)
                                        guardObserved = juce::exactlyEqual (wet.getSample (badChannel, i), 0.0f);
                                    for (int ch = 0; ch < 2; ++ch)
                                    {
                                        const float input = inputAt (ch, position + i - latency);
                                        const float raw = std::isfinite (input) ? input : 0.0f;
                                        const float processed = wet.getSample (ch, i);
                                        const float expected = atOn ? raw : atOff ? processed
                                            : processed + coefficient * (raw - processed);
                                        const float output = work.getSample (ch, i);
                                        const bool equal = sameBits (output, expected);
                                        if (atOn) badOn += ! equal;
                                        else if (atOff) badOff += ! equal;
                                        else badInterior += ! equal;
                                        finite = finite && std::isfinite (output) && std::isfinite (processed)
                                                        && std::isfinite (expected);
                                        if (phase == 0 && b == 0 && ch != badChannel && atOn
                                            && std::abs (raw) > 0.0f && juce::exactlyEqual (output, 0.0f)) ++firstMuted;
                                        if (phase == 5 && b == blocks - 1)
                                            recoveredEnergy += std::abs ((double) processed);
                                    }
                                }
                                measured = measured && std::isfinite (actual->getMatchGainDb())
                                                    && std::isfinite (active->getMatchGainDb());
                                latencySame = latencySame && actual->getLatencySamples() == latency
                                                          && active->getLatencySamples() == latency;
                                position += block;
                            }
                        }
                        std::printf ("  block %d OS %d channel %d Drive %.0f input %g: first unaffected mute %d, on/off/interior mismatches %d/%d/%d\n",
                                     block, (int) factor, badChannel, (double) drive, (double) value,
                                     firstMuted, badOn, badOff, badInterior);
                        check (on > 0 && badOn == 0, "D-02: settled bypass retains all validated raw bits, including subsequent delay history");
                        check (off > 0 && badOff == 0, "D-02: bypass off retains the active engine output");
                        check (interior > 0 && rise && fall && badInterior == 0, "D-02: both ramps consume validated raw history without a block-wide mute");
                        check (finite, "D-02: finite output includes isolated NaN/Inf controls and finite headroom without a limiter");
                        check (kind != 0 || drive > 0.0f || guardObserved, "D-02 premise: the finite spike really reaches the processed self-heal (Drive off)");
                        check (measured && recoveredEnergy > 0.01, "D-02: processed audio and analysis recover in the clean return leg");
                        check (latencySame && (factor == anamorph::OversampleFactor::Off ? latency == 0 : latency > 0),
                               "D-02: reported and raw delay alignment stay at the selected factor");
                    }
}
