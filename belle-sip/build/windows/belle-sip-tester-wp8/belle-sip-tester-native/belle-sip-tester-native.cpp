#include <string>

#include "belle-sip-tester-native.h"
#include "belle-sip/belle-sip.h"

using namespace belle_sip_tester_native;
using namespace Platform;

CainSipTesterNative::CainSipTesterNative()
{
}

void CainSipTesterNative::run(Platform::String^ name, Platform::Boolean verbose)
{
	std::wstring all(L"ALL");
	std::wstring suitename = name->Data();
	char cname[128] = { 0 };
	wcstombs(cname, suitename.c_str(), sizeof(cname));

	if (verbose) {
		belle_sip_set_log_level(BELLE_SIP_LOG_DEBUG);
	} else {
		belle_sip_set_log_level(BELLE_SIP_LOG_ERROR);
	}

	belle_sip_tester_run_tests(suitename == all ? 0 : cname, 0);
}
