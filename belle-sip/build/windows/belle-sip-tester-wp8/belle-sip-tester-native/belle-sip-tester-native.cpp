// belle-sip-tester-native.cpp
#include "pch.h"
#include "belle-sip-tester-native.h"

using namespace belle_sip_tester_native;
using namespace Platform;

CainSipTesterNative::CainSipTesterNative()
{
}

void CainSipTesterNative::run()
{
	belle_sip_tester_run_tests("Resolver", 0);
}
