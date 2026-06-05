#pragma once

#include <cmath>

namespace anamorph
{

// ============================================================================
//  Saturation (Drive)
//
//  Sits at the front of the effect engine. A smooth tanh waveshaper with a
//  pre-gain (the "Drive" amount) and a level compensation so that increasing
//  drive does not simply make everything louder (which would fool the ear and
//  the Auto-Gain matcher). Nonlinear -> must run inside oversampling.
// ============================================================================
class Saturation
{
public:
    void setDriveDb (float db) noexcept
    {
        drive = std::pow (10.0f, db * (1.0f / 20.0f));
        // Normalise by the slope at the origin (d/dx tanh(drive*x)|0 = drive)
        // so the small-signal gain stays unity: tone change, not a level boost.
        comp  = 1.0f / drive;
    }

    inline float processSample (float x) const noexcept
    {
        return std::tanh (drive * x) * comp;
    }

private:
    float drive = 1.0f;
    float comp  = 1.0f;
};

} // namespace anamorph
