#include "PluginParameters.h"
#include <atomic>
#include <cmath>

using juce::AudioParameterFloat;
using juce::AudioParameterChoice;
using juce::AudioParameterBool;
using juce::NormalisableRange;
using juce::StringArray;
using juce::ParameterID;

// pluginval's "Plugin state restoration" sets a RAW normalised value on each parameter and checks
// getValue() back within 0.1. Stock AudioParameterBool/Choice/Int SNAP getValue() to the nearest
// legal step -- and make getValue()/setValue() PRIVATE, so they cannot be subclassed for this -- so
// a few-step discrete param given a mid-step raw value (e.g. a 3-choice at 0.807 -> 1.0, 0.193 away)
// is intermittently reported "not restored" under --randomise. These minimal RangedAudioParameter
// subclasses (getValue/setValue are PUBLIC pure-virtuals here) keep the EXACT raw normalised value in
// getValue() -- restored bit-exactly via the "raw" state attribute + reassertParameters -- while the
// host text AND the DSP (getRawParameterValue() = convertFrom0to1(getValue())) still see the snapped
// value. No parameter ID, range, default, or choice ordering changes.
namespace
{
// Only non-finite display inputs change; numeric readback stays exact (ADR-0067).
float finiteDisplayPosition (float value, float defaultValue) noexcept
{
    if (std::isfinite (value)) return value;
    if (std::isnan (value)) return defaultValue;
    return value > 0.0f ? 1.0f : 0.0f;
}

struct RawChoice : juce::RangedAudioParameter
{
    RawChoice (const juce::ParameterID& pid, const juce::String& nm, juce::StringArray c, int defIndex)
        : juce::RangedAudioParameter (pid, nm), choices (std::move (c)),
          range (0.0f, (float) juce::jmax (1, choices.size() - 1), 1.0f),
          normValue (toNorm (defIndex)), defaultNorm (toNorm (defIndex)) {}

    float getValue() const override        { return normValue.load(); }
    void  setValue (float v) override      { normValue.store (v); }
    float getDefaultValue() const override { return defaultNorm; }
    int   getNumSteps() const override     { return choices.size(); }
    bool  isDiscrete() const override      { return true; }
    juce::String getText (float v, int) const override { return choices[indexFor (v)]; }
    float getValueForText (const juce::String& t) const override { const int i = choices.indexOf (t); return toNorm (i < 0 ? 0 : i); }
    const juce::NormalisableRange<float>& getNormalisableRange() const override { return range; }

    int   indexFor (float v) const { return juce::jlimit (0, choices.size() - 1, juce::roundToInt (finiteDisplayPosition (v, defaultNorm) * (float) juce::jmax (1, choices.size() - 1))); }
    float toNorm (int i) const     { return choices.size() > 1 ? (float) juce::jlimit (0, choices.size() - 1, i) / (float) (choices.size() - 1) : 0.0f; }

    juce::StringArray choices;
    juce::NormalisableRange<float> range;
    std::atomic<float> normValue;
    float defaultNorm;
};

struct RawBool : juce::RangedAudioParameter
{
    RawBool (const juce::ParameterID& pid, const juce::String& nm, bool def, bool automatable = true)
        : juce::RangedAudioParameter (pid, nm), range (0.0f, 1.0f, 1.0f),
          normValue (def ? 1.0f : 0.0f), defaultNorm (def ? 1.0f : 0.0f), autom (automatable) {}

    float getValue() const override        { return normValue.load(); }
    void  setValue (float v) override      { normValue.store (v); }
    float getDefaultValue() const override { return defaultNorm; }
    int   getNumSteps() const override     { return 2; }
    bool  isDiscrete() const override      { return true; }
    bool  isBoolean() const override       { return true; }
    bool  isAutomatable() const override   { return autom; }
    juce::String getText (float v, int) const override { return finiteDisplayPosition (v, defaultNorm) >= 0.5f ? "On" : "Off"; }
    float getValueForText (const juce::String& t) const override
    { return (t.equalsIgnoreCase ("on") || t.equalsIgnoreCase ("true") || t.getIntValue() != 0) ? 1.0f : 0.0f; }
    const juce::NormalisableRange<float>& getNormalisableRange() const override { return range; }

    juce::NormalisableRange<float> range;
    std::atomic<float> normValue;
    float defaultNorm;
    bool  autom;
};

struct RawInt : juce::RangedAudioParameter
{
    RawInt (const juce::ParameterID& pid, const juce::String& nm, int mn, int mx, int def)
        : juce::RangedAudioParameter (pid, nm), range ((float) mn, (float) mx, 1.0f),
          normValue (range.convertTo0to1 ((float) def)), defaultNorm (range.convertTo0to1 ((float) def)) {}

    float getValue() const override        { return normValue.load(); }
    void  setValue (float v) override      { normValue.store (v); }
    float getDefaultValue() const override { return defaultNorm; }
    int   getNumSteps() const override     { return juce::roundToInt (range.end - range.start) + 1; }
    bool  isDiscrete() const override      { return true; }
    juce::String getText (float v, int) const override { return juce::String (juce::roundToInt (range.convertFrom0to1 (finiteDisplayPosition (v, defaultNorm)))); }
    float getValueForText (const juce::String& t) const override { return range.convertTo0to1 ((float) t.getIntValue()); }
    const juce::NormalisableRange<float>& getNormalisableRange() const override { return range; }

    juce::NormalisableRange<float> range;
    std::atomic<float> normValue;
    float defaultNorm;
};
} // namespace

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

// A LOG range (so the low end keeps a sensible density -- no huge dead zone near the bottom)
// but warped with a smooth quadratic so `centre` lands on the slider's middle. Keeps the
// endpoints and the even log feel, just shifts the midpoint (0.6.16 #E).
static juce::NormalisableRange<float> logFreqRangeCentred (float lo, float hi, float centre)
{
    const float lr = std::log (hi / lo);
    const float tc = std::log (centre / lo) / lr;     // log-proportion of the centre value
    const float a  = 4.0f * (tc - 0.5f);              // g(p)=p+a*p*(1-p) maps 0.5 -> tc
    return { lo, hi,
        [lr, a] (float s, float, float p) { const float g = p + a * p * (1.0f - p); return s * std::exp (lr * g); },
        [lr, a] (float s, float, float v)
        {
            const float g = std::log (v / s) / lr;
            if (std::abs (a) < 1.0e-6f) return juce::jlimit (0.0f, 1.0f, g);
            const float disc = std::sqrt (juce::jmax (0.0f, (1.0f + a) * (1.0f + a) - 4.0f * a * g));
            return juce::jlimit (0.0f, 1.0f, ((1.0f + a) - disc) / (2.0f * a)); // root in [0,1]
        },
        [] (float s, float e, float v) { return juce::jlimit (s, e, v); } };
}

juce::AudioProcessorValueTreeState::ParameterLayout createAnamorphLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto floatParam = [&] (const char* id, const juce::String& name,
                           NormalisableRange<float> range, float def,
                           std::function<juce::String(float,int)> toText = {},
                           std::function<float(const juce::String&)> fromText = {},
                           bool automatable = true)
    {
        auto attr = juce::AudioParameterFloatAttributes();
        if (toText)
            attr = attr.withStringFromValueFunction (
                [format = std::move (toText), def] (float value, int length)
                {
                    return format (std::isfinite (value) ? value : def, length);
                });
        if (fromText) attr = attr.withValueFromStringFunction (std::move (fromText));
        if (! automatable) attr = attr.withAutomatable (false);
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
        // Left (l) / Mid (m) = negative side; Right (r) / Side (s) = positive (#12).
        const bool left  = s.containsChar ('l') || s.containsChar ('m');
        const bool right = s.containsChar ('r') || s.containsChar ('s');
        float v = s.removeCharacters ("lrms%+ ").getFloatValue() / 100.0f;
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
    layout.add (std::make_unique<RawChoice> (ParameterID { pid::channelMode, kVersion },
        "Input Channel", StringArray { "Stereo", "Left Only", "Right Only" }, 0));
    layout.add (std::make_unique<RawBool> (ParameterID { pid::monoSum, kVersion }, "Mono", false));
    // Names reflect BOTH operating modes: in M/S mode L/R become Mid/Side (Issue 6).
    layout.add (std::make_unique<RawBool> (ParameterID { pid::swap, kVersion }, "Swap L/R (M/S)", false));
    floatParam (pid::inputBalance, "Input Balance", { -1.0f, 1.0f, 0.001f }, 0.0f, balPct, balFrom);
    layout.add (std::make_unique<RawBool> (ParameterID { pid::polarityL, kVersion }, "Phase Invert L/M", false));
    layout.add (std::make_unique<RawBool> (ParameterID { pid::polarityR, kVersion }, "Phase Invert R/S", false));

    // --- MS ---
    layout.add (std::make_unique<RawBool> (ParameterID { pid::msMode, kVersion }, "M/S Mode", false));

    // --- Effect engine ---
    floatParam (pid::drive, "Drive", { 0.0f, 24.0f, 0.01f }, 0.0f, db);
    layout.add (std::make_unique<RawChoice> (ParameterID { pid::algorithm, kVersion },
        "Widen Algorithm", StringArray { "Haas", "Velvet Noise", "Chorus", "Dimensional" }, 1));
    // Unified widening intensity. Default 0 == transparent on load (#3).
    floatParam (pid::amount, "Amount", { 0.0f, 1.0f, 0.001f }, 0.0f, pct, pctFrom);
    floatParam (pid::haasDelay, "Haas Delay", { 1.0f, 35.0f, 0.01f }, 12.0f, ms);
    // Default perceived side = Left (#14); list order unchanged.
    layout.add (std::make_unique<RawChoice> (ParameterID { pid::haasSide, kVersion },
        "Haas Focus", StringArray { "Left", "Right" }, 0));
    floatParam (pid::velvetDensity, "Velvet Density", { 0.0f, 1.0f, 0.001f }, 0.5f, pct, pctFrom);
    floatParam (pid::chorusRate, "Chorus Rate", NormalisableRange<float> { 0.05f, 5.0f, 0.001f, 0.4f }, 0.5f,
                [] (float v, int) { return juce::String (v, 2) + " Hz"; }, hzFrom);
    floatParam (pid::chorusDepth, "Chorus Depth", { 0.0f, 1.0f, 0.001f }, 0.5f, pct, pctFrom);
    // Friendly Dimensional voicing names (#14); long descriptions live in tooltips.
    layout.add (std::make_unique<RawChoice> (ParameterID { pid::dimMode, kVersion },
        "Dimensional Style", StringArray { "Subtle", "Classic", "Wide", "Lush" }, 1));
    floatParam (pid::width, "Width", { 0.0f, 2.0f, 0.001f }, 1.0f, pct, pctFrom);

    // --- Multiband (1..4 bands, up to 3 crossovers) ---
    layout.add (std::make_unique<RawBool> (ParameterID { pid::mbEnable, kVersion }, "Multiband Enable", true));
    // Multiband Bands / Solo are now EXPOSED to host automation (they appear in the DAW's
    // automation list alongside every other parameter). They are still primarily driven by the
    // drag-to-split display and remain in state save/recall, GUI and A/B; Solo is a 4-bit mask.
    layout.add (std::make_unique<RawInt> (ParameterID { pid::mbBands, kVersion }, "Multiband Bands", 1, 4, 4));
    // Solo is a 4-bit mask (any combination of bands), not a single index (0.6.9 #7).
    layout.add (std::make_unique<RawInt> (ParameterID { pid::mbSolo, kVersion }, "Multiband Solo", 0, 15, 0));
    // Full-range splits: the display enforces a minimum on-screen gap and the DSP
    // re-orders them, so a split may be dragged anywhere (0.6.10 #5/#26).
    floatParam (pid::mbFreqLow,  "Multiband Split 1", logFreqRange (20.0f, 20000.0f), 180.0f,  hz, hzFrom);
    floatParam (pid::mbFreqMid,  "Multiband Split 2", logFreqRange (20.0f, 20000.0f), 800.0f,  hz, hzFrom);
    floatParam (pid::mbFreqHigh, "Multiband Split 3", logFreqRange (20.0f, 20000.0f), 3000.0f, hz, khzFrom);
    floatParam (pid::mbWidthLow,   "Multiband Width 1", { 0.0f, 2.0f, 0.001f }, 1.0f, pct, pctFrom);
    floatParam (pid::mbWidthMid,   "Multiband Width 2", { 0.0f, 2.0f, 0.001f }, 1.0f, pct, pctFrom);
    floatParam (pid::mbWidthHiMid, "Multiband Width 3", { 0.0f, 2.0f, 0.001f }, 1.0f, pct, pctFrom);
    floatParam (pid::mbWidthHigh,  "Multiband Width 4", { 0.0f, 2.0f, 0.001f }, 1.0f, pct, pctFrom);

    // --- Mono maker ---
    layout.add (std::make_unique<RawBool> (ParameterID { pid::monoMakerOn, kVersion }, "Mono Maker", false));
    // 20..500 with 120 on the bar's middle, via a centred LOG warp -- the low end keeps a
    // healthy density (not the very sparse tail a linear centre-skew gave) (0.6.16 #E).
    floatParam (pid::monoMakerFreq, "Mono Maker Freq", logFreqRangeCentred (20.0f, 500.0f, 120.0f), 120.0f, hz, hzFrom);

    // --- Mix / gain ---
    floatParam (pid::mix, "Mix", { 0.0f, 1.0f, 0.001f }, 1.0f, pct, pctFrom);
    floatParam (pid::outputGain, "Output Gain", { -24.0f, 24.0f, 0.01f }, 0.0f, db);
    floatParam (pid::outputBalance, "Output Balance", { -1.0f, 1.0f, 0.001f }, 0.0f, balPct, balFrom);
    layout.add (std::make_unique<RawBool> (ParameterID { pid::autoGainMatch, kVersion }, "Level Match", false));

    // --- Monitoring ---
    layout.add (std::make_unique<RawChoice> (ParameterID { pid::solo, kVersion },
        "M/S Solo", StringArray { "Off", "Mid", "Side" }, 0));

    // --- Oversampling / Settings / Show Meters are NOT here anymore ---------------
    // These are Settings-panel + view controls. JUCE's `withAutomatable(false)` does NOT
    // hide a parameter in REAPER (it lists every VST3 parameter regardless), so the only
    // reliable way to keep them off the host's parameter list is to keep them OUT of the
    // APVTS / VST3 tree entirely. They now live in anamorph::InternalState (session state,
    // GUI-bound, Oversampling drives the DSP), and never touch A/B / Undo / presets.
    // Moved out: Oversampling, Scope Persistence, Show Meters, Show Tooltips, UI
    // Animations, UI Scale. (Advanced Mode stays an APVTS param -- it travels with A/B.)

    // --- Bypass (registered as the host bypass parameter) ---
    layout.add (std::make_unique<RawBool> (ParameterID { pid::bypass, kVersion }, "Bypass", false));

    // --- UI (saved with state, but UI-only) ---
    // advancedMode is a UI-layout toggle (show/hide the advanced panel), a VIEW concern that travels
    // with A/B -- it is deliberately NOT host-automatable. Host-automating it drives editor RESIZES
    // (applyUiScale), and on Linux/X11 the resize ConfigureNotify storm hits a use-after-free in the
    // HOST's JUCE XEmbedComponent during rapid open/close (reproduced locally; core dump lands in
    // XEmbedComponent -- KI-003 / KI-007). A layout toggle has no place in an automation lane anyway;
    // isAutomatable() is false. The on-screen toggle and A/B/undo travel are unchanged.
    layout.add (std::make_unique<RawBool> (ParameterID { pid::advancedMode, kVersion }, "Advanced Mode", false, /*automatable*/ false));

    return layout;
}

void ParamPointers::bind (juce::AudioProcessorValueTreeState& s)
{
    const auto parameter = [&s] (const char* id)
    {
        const auto* p = s.getParameter (id);
        jassert (p != nullptr);
        return Parameter { s.getRawParameterValue (id), p->convertFrom0to1 (p->getDefaultValue()) };
    };
    channelMode   = parameter (pid::channelMode);
    monoSum       = parameter (pid::monoSum);
    swap          = parameter (pid::swap);
    inputBalance  = parameter (pid::inputBalance);
    polarityL     = parameter (pid::polarityL);
    polarityR     = parameter (pid::polarityR);
    msMode        = parameter (pid::msMode);
    drive         = parameter (pid::drive);
    algorithm     = parameter (pid::algorithm);
    amount        = parameter (pid::amount);
    haasDelay     = parameter (pid::haasDelay);
    haasSide      = parameter (pid::haasSide);
    velvetDensity = parameter (pid::velvetDensity);
    chorusRate    = parameter (pid::chorusRate);
    chorusDepth   = parameter (pid::chorusDepth);
    dimMode       = parameter (pid::dimMode);
    width         = parameter (pid::width);
    mbEnable      = parameter (pid::mbEnable);
    mbBands       = parameter (pid::mbBands);
    mbSolo        = parameter (pid::mbSolo);
    mbFreqLow     = parameter (pid::mbFreqLow);
    mbFreqMid     = parameter (pid::mbFreqMid);
    mbFreqHigh    = parameter (pid::mbFreqHigh);
    mbWidthLow    = parameter (pid::mbWidthLow);
    mbWidthMid    = parameter (pid::mbWidthMid);
    mbWidthHiMid  = parameter (pid::mbWidthHiMid);
    mbWidthHigh   = parameter (pid::mbWidthHigh);
    monoMakerOn   = parameter (pid::monoMakerOn);
    monoMakerFreq = parameter (pid::monoMakerFreq);
    mix           = parameter (pid::mix);
    outputGain    = parameter (pid::outputGain);
    outputBalance = parameter (pid::outputBalance);
    autoGainMatch = parameter (pid::autoGainMatch);
    solo          = parameter (pid::solo);
    bypass        = parameter (pid::bypass);
    advancedMode  = parameter (pid::advancedMode);
}

// ADR-0057, PRECONDITION 2. Every load below is the default (seq_cst) `load()`: a bulk swap's forced
// bottom adopts the destination only as complete as these reads are ordered. A relaxed load here would
// let the engine trust a snapshot holding a NEWER swap's write without seeing its request (Claim 2) --
// on ARMv8, invisibly to every x86-64 test. check-realtime.py rejects a weak order in this body.
anamorph::EngineParameters ParamPointers::toEngine (int oversampleIndex) const
{
    using namespace anamorph;
    EngineParameters e;
    // The actual declared default is bound once, not a mutable last-value cache.
    // Keep this seq_cst load inside the body covered by ADR-0057's order lint.
    const auto read = [] (const Parameter& parameter) noexcept
    {
        const float value = parameter.raw->load();
        return std::isfinite (value) ? value : parameter.defaultValue;
    };

    const bool advanced = read (advancedMode) > 0.5f;

    // --- Core widening (always active, both modes) ---
    // Discrete choice indices are read with roundToInt (not a truncating cast): the raw parameter
    // value is range.convertFrom0to1(getValue()), and with the exact-raw getValue() a legal choice
    // can denormalise a hair below its integer (e.g. Chorus 2/3 -> 1.9999), which (int) would floor
    // to the wrong choice. roundToInt snaps to the intended index for every legal selection.
    e.driveDb      = read (drive);
    e.algorithm    = (Algorithm) juce::roundToInt (read (algorithm));
    e.algoAmount   = read (amount);
    e.haasDelayMs  = read (haasDelay);
    e.haasSide     = (HaasSide) juce::roundToInt (read (haasSide));
    e.velvetDensity = read (velvetDensity);
    e.chorusRate   = read (chorusRate);
    e.chorusDepth  = read (chorusDepth);
    e.dimMode      = juce::roundToInt (read (dimMode)) + 1; // choice 0..3 -> mode 1..4
    e.width        = read (width);

    e.oversample   = (OversampleFactor) juce::jlimit (0, 3, oversampleIndex); // from InternalState
    e.bypass       = read (bypass) > 0.5f;

    if (advanced)
    {
        // --- Input module ---
        e.channelMode  = (ChannelMode) juce::roundToInt (read (channelMode));
        e.monoSum      = read (monoSum) > 0.5f;
        e.swapLR       = read (swap) > 0.5f;
        e.inputBalance = read (inputBalance);
        e.polarityL    = read (polarityL) > 0.5f;
        e.polarityR    = read (polarityR) > 0.5f;
        e.msMode       = read (msMode) > 0.5f;
        e.solo         = (SoloMode) juce::roundToInt (read (solo));

        // --- Multiband ---
        e.mbEnable     = read (mbEnable) > 0.5f;
        e.mbBands      = juce::roundToInt (read (mbBands)); // roundToInt like the other discrete reads above
        e.mbSolo       = juce::roundToInt (read (mbSolo));  // (was (int)(x+0.5f) -- equivalent, now uniform)
        e.mbFreqLow    = read (mbFreqLow);
        e.mbFreqMid    = read (mbFreqMid);
        e.mbFreqHigh   = read (mbFreqHigh);
        e.mbWidthLow   = read (mbWidthLow);
        e.mbWidthMid   = read (mbWidthMid);
        e.mbWidthHiMid = read (mbWidthHiMid);
        e.mbWidthHigh  = read (mbWidthHigh);

        // --- Output module ---
        e.monoMakerEnable = read (monoMakerOn) > 0.5f;
        e.monoMakerFreq   = read (monoMakerFreq);
        e.mix             = read (mix);
        e.outputGainDb    = read (outputGain);
        e.outputBalance   = read (outputBalance);
        e.autoGainMatch   = read (autoGainMatch) > 0.5f;
    }
    // else: Advanced-only modules behave as defaults (bypassed) while their knob
    // values stay put in the tree, so re-enabling Advanced restores them (#1).
    // EngineParameters' member initialisers already hold those neutral defaults.
    //
    // That claim covers the ADVANCED-GATED fields above and nothing else, and two
    // fields differ from the struct's initialiser in ways worth naming here rather
    // than leaving to be rediscovered (ER-DOC-04):
    //   * `dimMode` is set OUTSIDE the gate, and this snapshot's default is 2 (choice
    //     index 1 + 1) against `EngineParameters::dimMode = 1`. It is in
    //     `discreteDiffers`, so that one-field disagreement is enough to read as a
    //     settings change -- which is what made ER-DSP-06's spurious duck fire on
    //     EVERY instance, restored session or not, until `prepareToPlay` was made to
    //     prime the engine from this snapshot before preparing it.
    //   * `mbEnable` differs too (APVTS default true vs struct false), but it IS
    //     gated, so in Simple mode the struct default is what the engine sees --
    //     which is the intended behaviour, not a mismatch.
    // Neither is a defect to fix by changing a default: the APVTS values are the
    // product's documented defaults, and the struct's are the engine's neutral
    // no-parameters-yet state. They are simply not the same thing, and the wrapper
    // must never let the engine settle on the second when the first is available.

    return e;
}
