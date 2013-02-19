#include <string>

#include "belle-sip-tester-native.h"
#include "belle-sip/belle-sip.h"
#include "cunit/Util.h"

using namespace belle_sip_tester_native;
using namespace Platform;

#define MAX_TRACE_SIZE	512

static OutputTraceListener^ sTraceListener;

static void nativeOutputTraceHandler(int lev, const char *fmt, va_list args)
{
	if (sTraceListener) {
		wchar_t wstr[MAX_TRACE_SIZE];
		std::string str;
		str.resize(MAX_TRACE_SIZE);
		vsnprintf((char *)str.c_str(), MAX_TRACE_SIZE, fmt, args);
		mbstowcs(wstr, str.c_str(), sizeof(wstr));
		String^ msg = ref new String(wstr);
		sTraceListener->outputTrace(msg);
	}
}

static void belleSipNativeOutputTraceHandler(belle_sip_log_level lev, const char *fmt, va_list args)
{
	nativeOutputTraceHandler((int)lev, fmt, args);
}


CainSipTesterNative::CainSipTesterNative(OutputTraceListener^ traceListener)
{
	sTraceListener = traceListener;
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
	belle_sip_set_log_handler(belleSipNativeOutputTraceHandler);
	CU_set_trace_handler(nativeOutputTraceHandler);

	belle_sip_tester_run_tests(suitename == all ? 0 : cname, 0);
}
