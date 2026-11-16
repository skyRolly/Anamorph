#include "dsp/AnamorphEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdio>
#include <memory>
static int checks=0,failures=0;
static void check(bool ok,const char* msg){++checks;if(!ok){++failures;std::printf("FAIL %s\n",msg);}}
#include "n2_drive_endpoint_tests.h"
int main(){n2_drive_endpoint_tests::test();std::printf("%d checks, %d failures\n",checks,failures);return failures?1:0;}
