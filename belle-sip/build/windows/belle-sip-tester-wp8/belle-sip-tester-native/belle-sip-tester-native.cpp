// belle-sip-tester-native.cpp
#include "pch.h"
#include "belle-sip-tester-native.h"
#include "belle-sip/belle-sip.h"

using namespace belle_sip_tester_native;
using namespace Platform;

CainSipTesterNative::CainSipTesterNative()
{
}

void CainSipTesterNative::run()
{
	belle_sip_set_log_level(BELLE_SIP_LOG_DEBUG);
	belle_sip_tester_run_tests(0, 0);
}
