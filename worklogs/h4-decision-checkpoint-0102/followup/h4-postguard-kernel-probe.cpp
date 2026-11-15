// Nonshipping arithmetic witness: injected finite operands, not engine reachability.
#include "dsp/AnamorphEngine.h"
#include <cmath>
#include <cstdio>
#include <limits>
namespace h4_postguard_access {
struct Kernel { friend auto member(Kernel); };
template<class Tag, auto P> struct Get { friend auto member(Tag) { return P; } };
template struct Get<Kernel, &anamorph::AnamorphEngine::blendMultibandEnableSample>;
}
int main()
{
    volatile float left = -std::numeric_limits<float>::max();
    volatile float right = std::numeric_limits<float>::max();
    volatile float blend = 0.5f;
    const float output = member(h4_postguard_access::Kernel{})(left, right, blend);
    const bool a = std::isfinite(left), b = std::isfinite(right);
    const bool interior = blend > 0.0f && blend < 1.0f;
    const bool finite = std::isfinite(output);
    std::printf("from_finite=%d to_finite=%d blend_interior=%d output_finite=%d\n", a, b, interior, finite);
    return a && b && interior && !finite ? 0 : 1;
}
