// Isolated nonshipping witness: compile against the checkout's real LR4Xover.h.
// Test-only access to inspect its state; not a full-engine recovery experiment.
#include <cmath>
#include <cstdio>
#include <limits>
#define private public
#include "src/dsp/LR4Xover.h"
#undef private
int main()
{
    anamorph::LR4Xover x;
    x.prepare(48000.0);
    x.setCutoffFrequency(float(48000.0 * 0.45 / 1.21));
    float lo, hi;
    x.processSample(0, std::numeric_limits<float>::max() * 0.95f, lo, hi);
    const float out = lo + hi;
    const bool hidden = std::isfinite(out) && !std::isfinite(x.s2[0]);
    x.processSample(0, 0.0f, lo, hi);
    const bool laterBad = !std::isfinite(lo + hi);
    const bool otherClean = std::isfinite(x.s1[1]) && std::isfinite(x.s2[1])
                         && std::isfinite(x.s3[1]) && std::isfinite(x.s4[1]);
    std::printf("first_output=%g hidden_s2=%d next_output_nonfinite=%d other_channel_finite=%d\n",
                out, hidden, laterBad, otherClean);
    return hidden && laterBad && otherClean ? 0 : 1;
}
