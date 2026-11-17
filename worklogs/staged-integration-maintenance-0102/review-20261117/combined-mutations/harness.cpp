// Nonshipping selector compiled against the committed combined test headers.
#include "dsp/AnamorphEngine.h"
#include "AllocationGuard.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
static int checks=0,failures=0;
static void check(bool ok,const char* msg){++checks;if(!ok){++failures;std::printf("FAIL %s\n",msg);}}
#include "n2_conditioned_history_tests.h"
#include "n2_mix_endpoint_tests.h"
#include "n2_drive_endpoint_tests.h"
#include "n2_endpoint_tests.h"
#include "n2_control_tests.h"
int main(int argc,char** argv) {
 if(argc!=2)return 2;
 const std::string mode=argv[1];
 if(mode=="n204")testN204ConditionedHistoryRecovery();
 else if(mode=="n206")n2_mix_endpoint_tests::test();
 else if(mode=="n207")n2_drive_endpoint_tests::test();
 else if(mode=="n203")n2_endpoint_tests::test();
 else if(mode=="n202")testCrossfadedControlsDuringOrdinaryDuck();
 else return 2;
 std::printf("selector=%s %d checks, %d failures\n",mode.c_str(),checks,failures);
 return failures?1:0;
}
