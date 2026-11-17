#include "dsp/AnamorphEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdio>
#include <memory>

static int checks=0,failures=0;
static void check(bool ok,const char* message)
{
    ++checks;
    if(!ok){++failures;std::printf("FAIL %s\n",message);}
}
#include "n2_mix_endpoint_tests.h"
int main()
{
    n2_mix_endpoint_tests::test();
    std::printf("%d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
