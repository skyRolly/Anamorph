#pragma once

#include <cmath>

// ============================================================================
//  SerializedNumber
//
//  Single source of truth for "does this serialized property carry a number we
//  can actually use?", shared by the two restore paths that ask it:
//  `PresetManager::applySoundTree` (preset files) and
//  `AnamorphAudioProcessor::reassertParameters` (session state). They had
//  drifted apart once already; one predicate is what keeps them from drifting
//  again.
//
//  WHY THE TEST IS ON THE INPUT, NOT THE CONVERTED VALUE. Both paths used to
//  read the property, run it through `RangedAudioParameter::convertTo0to1`, and
//  then test the RESULT with std::isfinite. That order cannot work, because the
//  conversion CLAMPS: NormalisableRange maps anything at or above the range top
//  to exactly 1.0, so an infinity arrives at the finiteness test already
//  laundered into a perfectly finite range ENDPOINT. Measured on the shipped
//  build, per parameter, through the real loaders:
//
//      value="inf" / "1e39" / "1e400"   -> normalised 1.0  (range MAXIMUM)
//      value="-inf" / "-1e400"          -> normalised 0.0  (range MINIMUM)
//      value="abc" / "" / "0x10"        -> normalised 0.0  (range MINIMUM)
//      value="nan", skewed ranges       -> normalised 1.0  (range MAXIMUM)
//
//  Only the last of those was ever caught, and only for UNskewed ranges, which
//  is why the round-2 non-finite guard looked like it worked.
//
//  The text rule is deliberately STRICTER than any general number parser. It
//  accepts exactly the plain decimal shape JUCE's own writer emits -- optional
//  sign, digits with at most one point, optional decimal exponent -- and rejects
//  everything else. That is what makes "0x10" a rejection rather than 16: a
//  strtod-family parser would accept hex floats, "inf" and "nan" as legitimate
//  numbers, which is precisely the input this guard exists to refuse. It is also
//  locale-independent, which strtod is not.
// ============================================================================
namespace anamorph
{
    // True when `text` is the plain decimal shape a serialized number may take.
    // Dependency-free on purpose so the invariant can be guarded directly.
    // Leading/trailing whitespace is NOT accepted here -- callers trim first if
    // they wish to tolerate a hand edit that added some.
    inline bool looksLikePlainNumber (const char* text) noexcept
    {
        if (text == nullptr) return false;
        const char* p = text;
        if (*p == '+' || *p == '-') ++p;

        int mantissaDigits = 0;
        while (*p >= '0' && *p <= '9') { ++p; ++mantissaDigits; }
        if (*p == '.')
        {
            ++p;
            while (*p >= '0' && *p <= '9') { ++p; ++mantissaDigits; }
        }
        if (mantissaDigits == 0) return false;   // "", "+", ".", "abc", "inf", "nan"

        if (*p == 'e' || *p == 'E')
        {
            ++p;
            if (*p == '+' || *p == '-') ++p;
            int expDigits = 0;
            while (*p >= '0' && *p <= '9') { ++p; ++expDigits; }
            if (expDigits == 0) return false;    // "1e", "1e+"
        }
        return *p == '\0';                       // "12abc", "0x10", "1.2.3" fail here
    }

    // The finiteness half, applied to the value BEFORE any range conversion.
    // Rejects NaN and both infinities, including the ones that only appear after
    // the double -> float narrowing ("1e39").
    inline bool isUsableSerializedValue (float v) noexcept { return std::isfinite (v); }
}
