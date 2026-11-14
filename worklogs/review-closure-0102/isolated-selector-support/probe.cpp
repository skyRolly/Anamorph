// Nonshipping entry point for the exact committed DSP selector headers.
#include <juce_dsp/juce_dsp.h>
#include "dsp/AnamorphEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

static int checks=0, failures=0;
static void check(bool condition, const char* name)
{
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL %s\n",name); }
}
#include "n2_control_tests.h"
#include "n2_endpoint_tests.h"
int main(int argc, char** argv)
{
    if (argc!=2) return 2;
    if (std::strcmp(argv[1],"--n2-controls")==0) testCrossfadedControlsDuringOrdinaryDuck();
    else if (std::strcmp(argv[1],"--multiband-endpoints")==0) n2_endpoint_tests::test();
    else return 2;
    std::printf("%d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
