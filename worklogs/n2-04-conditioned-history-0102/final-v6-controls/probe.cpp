#include <juce_dsp/juce_dsp.h>
#include <juce_data_structures/juce_data_structures.h>
#include "AllocationGuard.h"
#include "dsp/AnamorphEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
namespace { int failures=0, checks=0; void check(bool cond,const char* what) { ++checks; if(!cond) { ++failures; std::printf("  [FAIL] %s\n",what); } } }
#include "n2_conditioned_history_tests.h"
int main() { std::setvbuf(stdout,nullptr,_IONBF,0); testN204ConditionedHistoryRecovery(); std::printf("%d checks, %d failures\n",checks,failures); return failures==0?0:1; }
