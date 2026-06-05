#pragma once

#include <cmath>

namespace anamorph
{

// ============================================================================
//  Mid-Side matrix
//
//  Uses the power-preserving 1/sqrt(2) convention in BOTH directions so an
//  encode -> decode round-trip is exactly gain- and phase-correct:
//
//     M = (L + R) * k,    S = (L - R) * k       (k = 1/sqrt(2))
//     L = (M + S) * k,    R = (M - S) * k
//
//  Round-trip: L' = ((L+R)k + (L-R)k) k = (2L k) k = 2L * (1/2) = L.  Exact.
// ============================================================================
struct MidSide
{
    static constexpr float k = 0.70710678118654752440f; // 1/sqrt(2)

    static inline void encode (float L, float R, float& M, float& S) noexcept
    {
        M = (L + R) * k;
        S = (L - R) * k;
    }

    static inline void decode (float M, float S, float& L, float& R) noexcept
    {
        L = (M + S) * k;
        R = (M - S) * k;
    }
};

// ----------------------------------------------------------------------------
//  Width as a pure MS-domain side-gain. width = 1 is identity; width = 0
//  collapses to mono; width > 1 widens. Mono compatibility is guaranteed by
//  construction: L + R = 2 * Mid, independent of the side gain, so summing to
//  mono never produces phase cancellation from the width control.
// ----------------------------------------------------------------------------
inline void applyWidth (float& L, float& R, float width) noexcept
{
    const float mid  = (L + R) * 0.5f;
    const float side = (L - R) * 0.5f * width;
    L = mid + side;
    R = mid - side;
}

} // namespace anamorph
