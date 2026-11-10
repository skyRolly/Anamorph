#pragma once

// Test 59 extension, T-01 / historical ER-DSP-04. The original burst matrix
// reaches the processed-output guard. Routing can instead discard the bad
// channel while the raw delay retains it, including history written with bypass
// off. Test final engine output, not just the guard's pre-bypass location.
static void testNonFiniteBypassBoundary()
{
    std::printf ("Test 59 extension: finite bypass readback through routing, endpoints and transitions (T-01)\n");
    juce::ScopedNoDenormals noDenormals;
    const auto sameBits = [] (float a, float b) { return std::memcmp (&a, &b, sizeof (float)) == 0; };
    const float values[] { std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::infinity(),
                          -std::numeric_limits<float>::infinity(), 2.0f, -1.0e-20f };
    constexpr double rate = 48000.0;
    for (const int block : { 1, 64, 512 })
        for (const auto factor : { anamorph::OversampleFactor::Off, anamorph::OversampleFactor::x2,
                                  anamorph::OversampleFactor::x8 })
            for (const auto routing : { anamorph::ChannelMode::Stereo, anamorph::ChannelMode::LeftOnly,
                                       anamorph::ChannelMode::RightOnly })
                for (const float value : values)
                {
                    const bool invalid = ! std::isfinite (value);
                    const bool discarded = routing != anamorph::ChannelMode::Stereo;
                    const int channel = routing == anamorph::ChannelMode::LeftOnly ? 1 : 0;
                    anamorph::EngineParameters p;
                    p.channelMode = routing;
                    p.algorithm = anamorph::Algorithm::Haas;
                    p.driveDb = 6.0f; // selected factors run their actual filters
                    p.oversample = factor;
                    auto actual = std::make_unique<anamorph::AnamorphEngine>();
                    auto control = std::make_unique<anamorph::AnamorphEngine>();
                    actual->primeParameters (p); actual->prepare (rate, block);
                    control->primeParameters (p); control->prepare (rate, block);
                    juce::AudioBuffer<float> work (2, block), clean (2, block);
                    juce::SmoothedValue<float> blend;
                    blend.reset (rate, 0.010);
                    blend.setCurrentAndTargetValue (0.0f);
                    int position = 0, nonFinite = 0, differences = 0, off = 0, on = 0, interior = 0;
                    bool recovered = true, measured = true;
                    const int blocks = (int) std::ceil (0.03 * rate / (double) block) + 1;
                    // Off (write-only history) -> On -> Off, then re-enter On
                    // with FINITE input while the delay can still contain bad history.
                    for (int phase = 0; phase < 5; ++phase)
                    {
                        p.bypass = phase % 2 != 0;
                        blend.setTargetValue (p.bypass ? 1.0f : 0.0f);
                        // The same scratch also feeds a forced duck before
                        // the processed-output guard. Exercise that consumer.
                        if (phase == 2) { actual->requestDuck(); control->requestDuck(); }
                        for (int b = 0; b < blocks; ++b)
                        {
                            for (int i = 0; i < block; ++i)
                                for (int ch = 0; ch < 2; ++ch)
                                {
                                    const float finite = 0.2f * std::sin ((float) (position + i) * 0.013f + (float) ch);
                                    const float input = phase < 3 && ch == channel ? value : finite;
                                    work.setSample (ch, i, input);
                                    clean.setSample (ch, i, std::isfinite (input) ? input : 0.0f);
                                }
                            actual->setParameters (p); control->setParameters (p);
                            actual->process (work); control->process (clean);
                            for (int i = 0; i < block; ++i)
                            {
                                const float coefficient = blend.getNextValue();
                                if (juce::exactlyEqual (coefficient, 1.0f)) ++on;
                                else if (! (coefficient > 0.0f)) ++off;
                                else ++interior;
                                for (int ch = 0; ch < 2; ++ch)
                                {
                                    const float output = work.getSample (ch, i);
                                    nonFinite += ! std::isfinite (output);
                                    // A discarded invalid channel never poisons the
                                    // processed chain: no extra reset or finite-audio
                                    // change is justified there. Stereo poison still
                                    // takes ADR-0009's existing stateful reset.
                                    if (discarded || ! invalid)
                                        differences += ! sameBits (output, clean.getSample (ch, i));
                                    // Both twins would share an accidental finite
                                    // clamp. With this channel routed away, wet is
                                    // zero and the settled raw endpoint is exact
                                    // even before D-01: check the input bits too.
                                    if (! invalid && discarded && phase == 1 && ch == channel
                                        && juce::exactlyEqual (coefficient, 1.0f))
                                        differences += ! sameBits (output, value);
                                    if (phase >= 3) recovered = recovered && std::isfinite (output);
                                }
                            }
                            measured = measured && std::isfinite (actual->getMatchGainDb());
                            position += block;
                        }
                    }
                    std::printf ("  block %d OS %d routing %d input %g: nonfinite %d control differences %d\n",
                                 block, (int) factor, (int) routing, (double) value, nonFinite, differences);
                    check (nonFinite == 0, "T-01: final host-facing output is finite at both endpoints and inside both ramps");
                    check (differences == 0, "T-01: discarded poison matches its finite twin; finite raw input bits survive readback");
                    check (off > 0 && on > 0 && interior > 0, "T-01 premise: off, interior and on coefficients were all exercised");
                    check (recovered, "T-01: finite input stays finite when re-entering bypass over previously stored bad history");
                    check (measured, "T-01: the Level-Match result stays finite after the existing processed-path self-heal");
                }
}
