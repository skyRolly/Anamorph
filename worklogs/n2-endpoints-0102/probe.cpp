#include <juce_dsp/juce_dsp.h>
#include "dsp/AnamorphEngine.h"
#include <cstdio>
int checks=0,failures=0;
void check(bool value,const char* message){++checks;if(!value){++failures;std::printf("FAIL %s\n",message);}}
#include "../../tests/n2_endpoint_tests.h"
int main(){n2_endpoint_tests::test();std::printf("%d checks, %d failures\n",checks,failures);return failures?1:0;}
