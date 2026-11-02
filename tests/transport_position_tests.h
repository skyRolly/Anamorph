#pragma once

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>

// The real wrapper consumes host position metadata. These are deterministic
// callback inputs, not claims about a particular DAW or native transport.
static void testTransportPositionArithmetic()
{
    std::printf ("State test 153: transport positions cannot overflow or invent a seek\n");
    struct Head final : juce::AudioPlayHead
    {
        bool samples = true, beats = true, tempo = true, playing = true;
        juce::int64 time = 0;
        double ppq = 1.0, bpm = 120.0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo result;
            result.setIsPlaying (playing);
            if (samples) result.setTimeInSamples (time);
            if (beats) result.setPpqPosition (ppq);
            if (tempo) result.setBpm (bpm);
            return result;
        }
    };
    struct Rig
    {
        std::unique_ptr<AnamorphAudioProcessor> proc = std::make_unique<AnamorphAudioProcessor>();
        Head head;
        juce::AudioBuffer<float> buffer { 2, 512 };
        juce::MidiBuffer midi;
        Rig()
        {
            proc->setPlayHead (&head);
            proc->setRateAndBufferSizeDetails (48000.0, 512);
            proc->prepareToPlay (48000.0, 512);
        }
        ~Rig() { proc->setPlayHead (nullptr); }
        void run (int frames, bool loud = false)
        {
            buffer.setSize (2, frames, false, false, true);
            const float input = loud ? 0.9f : 0.02f;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < frames; ++i) buffer.setSample (ch, i, input);
            proc->processBlock (buffer, midi);
            bool identical = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < frames; ++i)
                    identical &= std::bit_cast<std::uint32_t> (buffer.getSample (ch, i))
                                  == std::bit_cast<std::uint32_t> (input);
            check (identical, "153: transport metadata never changes transparent audio samples");
        }
        float held() const { return proc->getEngine().getLevels().output.getPeakHoldL(); }
        void high()
        {
            run (128, true);
            check (held() > -2.0f, "153 control: a real high peak is latched");
        }
    };

    using I = juce::int64;
    constexpr I lo = std::numeric_limits<I>::min(), hi = std::numeric_limits<I>::max();
    struct Jump { I before, after; int firstBlock, nextBlock; };
    const std::array<Jump, 5> jumps {{
        { -128, lo, 128, 128 },              // abs(INT64_MIN)
        { lo, hi, 32, 128 },                 // subtraction wraps to a small negative value
        { hi - 31, lo + 32, 64, 64 },        // the expected-position addition overflows
        { 0, hi, 128, 128 },
        { hi - 256, -1024, 128, 512 }
    }};
    for (const auto& jump : jumps)
    {
        Rig r;
        r.head.time = jump.before;
        r.run (jump.firstBlock, true);
        check (r.held() > -2.0f, "153 control: integer jump starts with a latched peak");
        r.head.time = jump.after;
        r.run (jump.nextBlock);
        check (r.held() < -20.0f, "153: full-range sample-clock jumps clear the held peak");
    }

    // Invalid or unrepresentable derived positions behave like absent position
    // metadata. The first valid position after the gap establishes a new origin.
    const auto invalidThenRecover = [] (double ppq, double bpm)
    {
        Rig r;
        r.head.samples = false;
        r.high();
        const float held = r.held();
        r.head.ppq = ppq; r.head.bpm = bpm;
        r.run (128);
        check (juce::exactlyEqual (r.held(), held), "153: invalid PPQ/tempo cannot fabricate a seek");
        r.head.ppq = 2.0; r.head.bpm = 120.0;
        r.run (128);
        check (juce::exactlyEqual (r.held(), held), "153: rejected metadata never becomes a clock origin");
        r.head.ppq += 128.0 / 24000.0;
        r.run (128);
        check (juce::exactlyEqual (r.held(), held), "153: valid clock resumes continuously after a gap");
        r.head.ppq += 4.0;
        r.run (128);
        check (r.held() < -20.0f, "153 control: a later valid seek still clears the latch");
    };
    for (const double ppq : { std::numeric_limits<double>::quiet_NaN(),
                              std::numeric_limits<double>::infinity(),
                             -std::numeric_limits<double>::infinity(),
                              std::numeric_limits<double>::max(),
                             -std::numeric_limits<double>::max(),
                              0x1p63 / 24000.0, -0x1p64 / 24000.0 })
        invalidThenRecover (ppq, 120.0);
    for (const double bpm : { std::numeric_limits<double>::quiet_NaN(),
                              std::numeric_limits<double>::infinity(),
                             -std::numeric_limits<double>::infinity() })
        invalidThenRecover (1.0 + 128.0 / 24000.0, bpm);

    // Ordinary negative timelines, variable callback sizes, missing tempo and
    // the existing finite-tempo floor must retain their old semantics.
    for (const double bpm : { -4.0, 0.0, 0.5, 123.4 })
        for (const bool tempo : { false, true })
        {
            Rig r;
            r.head.samples = false; r.head.bpm = bpm; r.head.tempo = tempo;
            r.head.ppq = -2.0;
            r.high();
            const float held = r.held();
            int previousFrames = 128;
            const double effectiveBpm = ! tempo ? 120.0 : bpm < 1.0 ? 1.0 : bpm;
            for (const int frames : { 32, 512, 64 })
            {
                r.head.ppq += previousFrames * effectiveBpm / (60.0 * 48000.0);
                r.run (frames);
                check (juce::exactlyEqual (r.held(), held), "153 control: valid PPQ playback retains the latch");
                previousFrames = frames;
            }
            r.head.ppq -= 8.0;
            r.run (128);
            check (r.held() < -20.0f, "153 control: a valid backward PPQ seek is detected");
        }

    {
        Rig r;
        r.head.time = -128;
        r.high();
        const float held = r.held();
        r.head.time = 0;
        r.head.ppq = r.head.bpm = std::numeric_limits<double>::quiet_NaN();
        r.run (128);
        check (juce::exactlyEqual (r.held(), held), "153 control: sample clock takes priority over invalid musical data");
        r.head.playing = false;
        r.run (128);
        check (juce::exactlyEqual (r.held(), held), "153 control: stopping preserves the held peak");
        r.head.playing = true;
        r.run (128);
        check (r.held() < -20.0f, "153 control: restart still resets independently of the position");
    }
}
