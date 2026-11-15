#pragma once

// N2-04: this test observes actual recovery and the distinct conditioned ring.
// The explicit-instantiation access is confined to tests; no production hook is added.
namespace n204_test_access
{
using Engine = anamorph::AnamorphEngine;
struct Conditioned { using type = juce::AudioBuffer<float> Engine::*; friend type member (Conditioned); };
struct Write { using type = int Engine::*; friend type member (Write); };
template <class Tag, typename Tag::type Ptr> struct Access { friend typename Tag::type member (Tag) { return Ptr; } };
template struct Access<Conditioned, &Engine::dryDelayBuffer>;
template struct Access<Write, &Engine::dryDelayWrite>;
inline bool bits (float a, float b) { return std::memcmp (&a, &b, sizeof (float)) == 0; }
}

static void testN204ConditionedHistoryRecovery()
{
    using namespace n204_test_access;
    std::printf ("Test 59 extension: self-heal preserves finite conditioned dry history (N2-04)\n");
    juce::ScopedNoDenormals noDenormals;
    constexpr int block = 64;
    constexpr double rate = 48000.0;
    const auto allocationLive = anamorph::testing::selfCheck();
    const bool allocationObserved = allocationLive.newLive || allocationLive.mallocLive;
    if (allocationObserved)
    {
        check (allocationLive.newLive, "N2-04 allocation guard: operator new detector is live");
        if (allocationLive.mallocCompiledIn)
            check (allocationLive.mallocLive, "N2-04 allocation guard: compiled malloc detector is live");
    }
    else
        std::printf ("::warning::N2-04 allocation guard unavailable in this build; rely on the applicable sanitizer lane\n");
    for (auto factor : { anamorph::OversampleFactor::x2, anamorph::OversampleFactor::x4,
                         anamorph::OversampleFactor::x8 })
        for (int badChannel : { 0, 1 })
        {
            anamorph::EngineParameters p;
            p.algorithm = anamorph::Algorithm::Haas;
            p.driveDb = 0.0f;
            p.mix = 0.0f;
            p.outputGainDb = 24.0f;
            p.oversample = factor;
            auto faulty = std::make_unique<Engine>();
            auto reference = std::make_unique<Engine>();
            for (auto* e : { faulty.get(), reference.get() }) { e->primeParameters (p); e->prepare (rate, block); }
            const int latency = faulty->getLatencySamples();
            juce::AudioBuffer<float> a (2, block), b (2, block);
            for (int k = 0; k < 8; ++k)
            {
                for (int i = 0; i < block; ++i)
                    for (int ch = 0; ch < 2; ++ch) { a.setSample (ch, i, ch == 0 ? .125f : .25f); b.setSample (ch, i, ch == 0 ? .125f : .25f); }
                faulty->process (a); reference->process (b);
            }
            for (int i = 0; i < block; ++i)
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float value = ch == 0 ? .125f : .25f;
                    a.setSample (ch, i, ch == badChannel && i == 7 ? 1.0e38f : value);
                    b.setSample (ch, i, value);
                }
            anamorph::testing::resetCounts();
            { anamorph::testing::Armed armed; faulty->process (a); }
            const long allocationsNew = anamorph::testing::newCount.load();
            const long allocationsMalloc = anamorph::testing::mallocCount.load();
            reference->process (b);
            const bool guardRan = latency > 0 && latency + 7 < block
                               && juce::exactlyEqual (a.getSample (badChannel, latency + 7), 0.0f);
            if (allocationLive.newLive)
                check (allocationsNew == 0, "N2-04: actual exceptional recovery does not allocate via new");
            if (allocationLive.mallocLive)
                check (allocationsMalloc == 0, "N2-04: actual exceptional recovery does not allocate via malloc");
            bool unaffectedCurrent = true;
            for (int i = 0; i < block; ++i)
                unaffectedCurrent &= bits (a.getSample (1-badChannel, i), b.getSample (1-badChannel, i));
            for (int i = 0; i < block; ++i)
                for (int ch = 0; ch < 2; ++ch) { a.setSample (ch, i, ch == 0 ? .125f : .25f); b.setSample (ch, i, ch == 0 ? .125f : .25f); }
            faulty->process (a); reference->process (b);
            bool unaffectedHistory = true, recovered = true;
            for (int i = 0; i < block; ++i)
            {
                unaffectedHistory &= bits (a.getSample (1-badChannel, i), b.getSample (1-badChannel, i));
                recovered &= std::isfinite (a.getSample (0, i)) && std::isfinite (a.getSample (1, i));
            }
            std::printf ("  selected factor %d, bad channel %d, latency %d: observed heal %d, current/history %d/%d\n",
                         1 << (int) factor, badChannel, latency, (int) guardRan,
                         (int) unaffectedCurrent, (int) unaffectedHistory);
            check (guardRan, "N2-04 premise: finite output-gain overflow reached the self-heal guard");
            check (unaffectedCurrent, "N2-04: valid opposite-channel output survives the recovery block");
            check (unaffectedHistory, "N2-04: valid opposite-channel conditioned delay history survives recovery");
            check (recovered && faulty->getLatencySamples() == latency,
                   "N2-04: subsequent output is finite and selected latency is unchanged");
        }

    // A retained ring can contain invalid input. Inject poison into a slot that
    // this block does not overwrite, cause a separate observed output overflow,
    // and prove both that poison is scrubbed and a finite signed-zero sentinel
    // in the other channel survives. An unconditional-retention mutation fails.
    for (float poison : { std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity(),
                         -std::numeric_limits<float>::infinity() })
    {
        anamorph::EngineParameters p;
        p.algorithm = anamorph::Algorithm::Haas; p.mix = 0.0f; p.outputGainDb = 24.0f;
        p.oversample = anamorph::OversampleFactor::x8;
        auto e = std::make_unique<Engine>(); e->primeParameters (p); e->prepare (rate, block);
        juce::AudioBuffer<float> a (2, block);
        for (int k = 0; k < 8; ++k)
        {
            for (int i = 0; i < block; ++i) { a.setSample (0, i, .125f); a.setSample (1, i, .25f); }
            e->process (a);
        }
        auto& ring = e.get()->*member (Conditioned{});
        const int oldWrite = e.get()->*member (Write{}), size = ring.getNumSamples();
        const int slot = (oldWrite + block) % size;
        ring.setSample (0, slot, poison); ring.setSample (1, slot, -0.0f);
        for (int i = 0; i < block; ++i) { a.setSample (0, i, i == 7 ? 1.0e38f : .125f); a.setSample (1, i, .25f); }
        e->process (a);
        check (juce::exactlyEqual (a.getSample (0, 7 + e->getLatencySamples()), 0.0f),
               "N2-04 poison premise: the output guard actually runs");
        check (bits (ring.getSample (0, slot), 0.0f),
               "N2-04: invalid retained conditioned history is sanitized before later reads");
        check (bits (ring.getSample (1, slot), -0.0f),
               "N2-04: finite conditioned history retains exact bits including signed zero");
    }

    // Unlike the injected ring-slot control, this poison enters through the
    // actual conditioned-input capture and is written by production processing.
    // A non-finite wet calculation still invokes the existing output self-heal;
    // this test does not claim to correct N2-05's Mix=0 masking behavior.
    for (float poison : { std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity(),
                         -std::numeric_limits<float>::infinity() })
    {
        anamorph::EngineParameters p;
        p.algorithm = anamorph::Algorithm::Haas; p.mix = 0.0f;
        p.oversample = anamorph::OversampleFactor::x8;
        auto e = std::make_unique<Engine>(); e->primeParameters (p); e->prepare (rate, block);
        juce::AudioBuffer<float> a (2, block);
        for (int k = 0; k < 8; ++k)
        {
            for (int i = 0; i < block; ++i) { a.setSample (0, i, .125f); a.setSample (1, i, .25f); }
            e->process (a);
        }
        auto& ring = e.get()->*member (Conditioned{});
        const int start = e.get()->*member (Write{}), slot = (start + block - 1) % ring.getNumSamples();
        for (int i = 0; i < block; ++i) { a.setSample (0, i, i == block - 1 ? poison : .125f); a.setSample (1, i, .25f); }
        e->process (a);
        // The selected factor's latency postpones this final input sample until
        // the next block. Verify that the actual write happened before requiring
        // the subsequent output self-heal to scrub the retained ring.
        check (! std::isfinite (ring.getSample (0, slot)),
               "N2-04 input-poison premise: actual conditioned history captured the non-finite input");
        for (int i = 0; i < block; ++i) { a.setSample (0, i, .125f); a.setSample (1, i, .25f); }
        e->process (a);
        const int delayedSample = e->getLatencySamples() - 1;
        check (delayedSample >= 0 && delayedSample < block
               && juce::exactlyEqual (a.getSample (0, delayedSample), 0.0f),
               "N2-04 input-poison premise: delayed output self-heal scrubbed a non-finite result");
        check (bits (ring.getSample (0, slot), 0.0f),
               "N2-04: non-finite actual conditioned input cannot remain in history after self-heal");
        check (bits (ring.getSample (1, slot), .25f),
               "N2-04: valid opposite-channel conditioned input survives the same heal");
    }
}
