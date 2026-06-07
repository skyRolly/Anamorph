#include "PluginParameters.h"

using juce::AudioParameterFloat;
using juce::AudioParameterChoice;
using juce::AudioParameterBool;
using juce::NormalisableRange;
using juce::StringArray;
using juce::ParameterID;

namespace
{
    // Bump this when the parameter set changes so hosts re-scan automation.
    constexpr int kVersion = 1;

    auto pct = [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
    auto hz  = [] (float v, int) { return (v >= 1000.0f)
                                          ? juce::String (v / 1000.0f, 2) + " kHz"
                                          : juce::String (juce::roundToInt (v)) + " Hz"; };
    auto db  = [] (float v, int) { return juce::String (v, 1) + " dB"; };
    auto ms  = [] (float v, int) { return juce::String (v, 1) + " ms"; };
}

// A true logarithmic (octave-even) range so frequency knobs feel right end to
// end -- no big dead chunk at the bottom (feedback #3).
static juce::NormalisableRange<float> logFreqRange (float lo, float hi)
{
    return { lo, hi,
        [] (float s, float e, float v) { return s * std::pow (e / s, v); },                 // 0..1 -> Hz
        [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },      // Hz -> 0..1
        [] (float s, float e, float v) { return juce::jlimit (s, e, v); } };
}

juce::AudioProcessorValueTreeState::ParameterLayout createAnamorphLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto floatParam = [&] (const char* id, const juce::String& name,
                           NormalisableRange<float> range, float def,
                           std::function<juce::String(float,int)> toText = {},
                           std::function<float(const juce::String&)> fromText = {})
    {
        auto attr = juce::AudioParameterFloatAttributes();
        if (toText)   attr = attr.withStringFromValueFunction (std::move (toText));
        if (fromText) attr = attr.withValueFromStringFunction (std::move (fromText));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { id, kVersion }, name, range, def, attr));
    };

    // --- Raw-number-friendly parsers (edit boxes show bare numbers, #36) ---
    // Percent fields: "150" or "150%" -> 1.5 (value is the fraction).
    auto pctFrom = [] (const juce::String& t) { return t.removeCharacters ("% ").getFloatValue() / 100.0f; };
    // Frequency fields: "2k" / "2kHz" / "2000" all -> 2000 Hz (#37).
    auto hzFrom = [] (const juce::String& t)
    {
        auto s = t.toLowerCase().trim();
        const bool k = s.containsChar ('k');
        const float v = s.removeCharacters ("khz ").getFloatValue();
        return k ? v * 1000.0f : v;
    };
    // Balance: signed (negative = Left), "C"/"0" -> centre, +/-100 -> hard L/R (#29).
    auto balFrom = [] (const juce::String& t)
    {
        auto s = t.toLowerCase().trim();
        if (s.startsWithChar ('c')) return 0.0f;
        const bool left = s.containsChar ('l'), right = s.containsChar ('r');
        float v = s.removeCharacters ("lr%+ ").getFloatValue() / 100.0f;
        if (left)  v = -std::abs (v);
        if (right) v =  std::abs (v);
        return juce::jlimit (-1.0f, 1.0f, v);
    };

    // Balance reads as a signed percentage: Left is shown NEGATIVE, Right
    // positive (feedback #5), 0.1% resolution.
    auto balPct = [] (float v, int)
    {
        if (std::abs (v) < 0.0005f) return juce::String ("C");
        if (v < 0.0f) return "L -" + juce::String (-v * 100.0f, 1) + "%";
        return "R " + juce::String (v * 100.0f, 1) + "%";
    };
    // Mid/Hi crossover edits in kHz: bare 1-10 (or "5k") -> 1-10 kHz (#4).
    auto khzFrom = [] (const juce::String& t)
    {
        auto s = t.toLowerCase().trim();
        const bool k = s.containsChar ('k');
        const float v = s.removeCharacters ("khz ").getFloatValue();
        if (k) return v * 1000.0f;
        return (v <= 20.0f) ? v * 1000.0f : v;
    };

    // --- Input conditioning ---
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::channelMode, kVersion },
        "Input Channel", StringArray { "Stereo", "Left Only", "Right Only" }, 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::monoSum, kVersion }, "Mono", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::swap, kVersion }, "Swap L/R", false));
    floatParam (pid::inputBalance, "Input Balance", { -1.0f, 1.0f, 0.001f }, 0.0f, balPct, balFrom);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::polarityL, kVersion }, "Phase Invert L", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::polarityR, kVersion }, "Phase Invert R", false));

    // --- MS ---
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::msMode, kVersion }, "M/S Mode", false));

    // --- Effect engine ---
    floatParam (pid::drive, "Drive", { 0.0f, 24.0f, 0.01f }, 0.0f, db);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::algorithm, kVersion },
        "Algorithm", StringArray { "Haas", "Velvet Noise", "Chorus", "Dim-D" }, 1));
    // Unified widening intensity. Default 0 == transparent on load (#3).
    floatParam (pid::amount, "Amount", { 0.0f, 1.0f, 0.001f }, 0.0f, pct, pctFrom);
    floatParam (pid::haasDelay, "Haas Delay", { 1.0f, 35.0f, 0.01f }, 12.0f, ms);
    // Default perceived side = Left (#14); list order unchanged.
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::haasSide, kVersion },
        "Haas Side", StringArray { "Left", "Right" }, 0));
    floatParam (pid::velvetDensity, "Velvet Density", { 0.0f, 1.0f, 0.001f }, 0.5f, pct, pctFrom);
    floatParam (pid::chorusRate, "Chorus Rate", NormalisableRange<float> { 0.05f, 5.0f, 0.001f, 0.4f }, 0.5f,
                [] (float v, int) { return juce::String (v, 2) + " Hz"; }, hzFrom);
    floatParam (pid::chorusDepth, "Chorus Depth", { 0.0f, 1.0f, 0.001f }, 0.5f, pct, pctFrom);
    // Friendly Dimension-D voicing names (#14); long descriptions live in tooltips.
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::dimMode, kVersion },
        "Dimension Mode", StringArray { "Subtle", "Classic", "Wide", "Lush" }, 1));
    floatParam (pid::width, "Width", { 0.0f, 2.0f, 0.001f }, 1.0f, pct, pctFrom);

    // --- Multiband ---
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::mbEnable, kVersion }, "Multiband Enable", false));
    floatParam (pid::mbFreqLow,  "MB Low/Mid",  logFreqRange (50.0f, 1000.0f), 250.0f, hz, hzFrom);
    floatParam (pid::mbFreqHigh, "MB Mid/High", logFreqRange (1000.0f, 10000.0f), 2500.0f, hz, khzFrom);
    floatParam (pid::mbWidthLow,  "MB Width Low",  { 0.0f, 2.0f, 0.001f }, 1.0f, pct, pctFrom);
    floatParam (pid::mbWidthMid,  "MB Width Mid",  { 0.0f, 2.0f, 0.001f }, 1.0f, pct, pctFrom);
    floatParam (pid::mbWidthHigh, "MB Width High", { 0.0f, 2.0f, 0.001f }, 1.0f, pct, pctFrom);

    // --- Mono maker ---
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::monoMakerOn, kVersion }, "Mono Maker", false));
    floatParam (pid::monoMakerFreq, "Mono Maker Freq", logFreqRange (20.0f, 500.0f), 120.0f, hz, hzFrom);

    // --- Mix / gain ---
    floatParam (pid::mix, "Mix", { 0.0f, 1.0f, 0.001f }, 1.0f, pct, pctFrom);
    floatParam (pid::outputGain, "Output Gain", { -24.0f, 24.0f, 0.01f }, 0.0f, db);
    floatParam (pid::outputBalance, "Output Balance", { -1.0f, 1.0f, 0.001f }, 0.0f, balPct, balFrom);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::autoGainMatch, kVersion }, "Auto Gain Match", false));

    // --- Monitoring ---
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::solo, kVersion },
        "M/S Solo", StringArray { "Off", "Mid", "Side" }, 0));

    // --- Oversampling --- (default Off == 1x, which is itself zero-latency)
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::oversample, kVersion },
        "Oversampling", StringArray { "Off (1x)", "2x", "4x", "8x" }, 0));

    // --- Bypass (registered as the host bypass parameter) ---
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::bypass, kVersion }, "Bypass", false));

    // --- UI (saved with state, but UI-only) ---
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::advancedMode, kVersion }, "Advanced Mode", false));
    // Persist default 0.5; the scope remaps it so 50% reproduces the old 60% feel (#21).
    floatParam (pid::scopePersist, "Scope Persistence", { 0.0f, 1.0f, 0.001f }, 0.5f, pct, pctFrom);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::metersOn, kVersion }, "Show Meters", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid::tooltipsOn, kVersion }, "Show Tooltips", false));

    return layout;
}

void ParamPointers::bind (juce::AudioProcessorValueTreeState& s)
{
    channelMode   = s.getRawParameterValue (pid::channelMode);
    monoSum       = s.getRawParameterValue (pid::monoSum);
    swap          = s.getRawParameterValue (pid::swap);
    inputBalance  = s.getRawParameterValue (pid::inputBalance);
    polarityL     = s.getRawParameterValue (pid::polarityL);
    polarityR     = s.getRawParameterValue (pid::polarityR);
    msMode        = s.getRawParameterValue (pid::msMode);
    drive         = s.getRawParameterValue (pid::drive);
    algorithm     = s.getRawParameterValue (pid::algorithm);
    amount        = s.getRawParameterValue (pid::amount);
    haasDelay     = s.getRawParameterValue (pid::haasDelay);
    haasSide      = s.getRawParameterValue (pid::haasSide);
    velvetDensity = s.getRawParameterValue (pid::velvetDensity);
    chorusRate    = s.getRawParameterValue (pid::chorusRate);
    chorusDepth   = s.getRawParameterValue (pid::chorusDepth);
    dimMode       = s.getRawParameterValue (pid::dimMode);
    width         = s.getRawParameterValue (pid::width);
    mbEnable      = s.getRawParameterValue (pid::mbEnable);
    mbFreqLow     = s.getRawParameterValue (pid::mbFreqLow);
    mbFreqHigh    = s.getRawParameterValue (pid::mbFreqHigh);
    mbWidthLow    = s.getRawParameterValue (pid::mbWidthLow);
    mbWidthMid    = s.getRawParameterValue (pid::mbWidthMid);
    mbWidthHigh   = s.getRawParameterValue (pid::mbWidthHigh);
    monoMakerOn   = s.getRawParameterValue (pid::monoMakerOn);
    monoMakerFreq = s.getRawParameterValue (pid::monoMakerFreq);
    mix           = s.getRawParameterValue (pid::mix);
    outputGain    = s.getRawParameterValue (pid::outputGain);
    outputBalance = s.getRawParameterValue (pid::outputBalance);
    autoGainMatch = s.getRawParameterValue (pid::autoGainMatch);
    solo          = s.getRawParameterValue (pid::solo);
    oversample    = s.getRawParameterValue (pid::oversample);
    bypass        = s.getRawParameterValue (pid::bypass);
    advancedMode  = s.getRawParameterValue (pid::advancedMode);
}

anamorph::EngineParameters ParamPointers::toEngine() const
{
    using namespace anamorph;
    EngineParameters e;

    const bool advanced = advancedMode->load() > 0.5f;

    // --- Core widening (always active, both modes) ---
    e.driveDb      = drive->load();
    e.algorithm    = (Algorithm) (int) algorithm->load();
    e.algoAmount   = amount->load();
    e.haasDelayMs  = haasDelay->load();
    e.haasSide     = (HaasSide) (int) haasSide->load();
    e.velvetDensity = velvetDensity->load();
    e.chorusRate   = chorusRate->load();
    e.chorusDepth  = chorusDepth->load();
    e.dimMode      = (int) dimMode->load() + 1; // choice 0..3 -> mode 1..4
    e.width        = width->load();

    e.oversample   = (OversampleFactor) (int) oversample->load();
    e.bypass       = bypass->load() > 0.5f;

    if (advanced)
    {
        // --- Input module ---
        e.channelMode  = (ChannelMode) (int) channelMode->load();
        e.monoSum      = monoSum->load() > 0.5f;
        e.swapLR       = swap->load() > 0.5f;
        e.inputBalance = inputBalance->load();
        e.polarityL    = polarityL->load() > 0.5f;
        e.polarityR    = polarityR->load() > 0.5f;
        e.msMode       = msMode->load() > 0.5f;
        e.solo         = (SoloMode) (int) solo->load();

        // --- Multiband ---
        e.mbEnable     = mbEnable->load() > 0.5f;
        e.mbFreqLow    = mbFreqLow->load();
        e.mbFreqHigh   = mbFreqHigh->load();
        e.mbWidthLow   = mbWidthLow->load();
        e.mbWidthMid   = mbWidthMid->load();
        e.mbWidthHigh  = mbWidthHigh->load();

        // --- Output module ---
        e.monoMakerEnable = monoMakerOn->load() > 0.5f;
        e.monoMakerFreq   = monoMakerFreq->load();
        e.mix             = mix->load();
        e.outputGainDb    = outputGain->load();
        e.outputBalance   = outputBalance->load();
        e.autoGainMatch   = autoGainMatch->load() > 0.5f;
    }
    // else: Advanced-only modules behave as defaults (bypassed) while their knob
    // values stay put in the tree, so re-enabling Advanced restores them (#1).
    // EngineParameters' member initialisers already hold those neutral defaults.

    return e;
}
