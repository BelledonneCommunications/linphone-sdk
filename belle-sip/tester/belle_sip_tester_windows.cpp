#include <string>

#include "belle_sip_tester_windows.h"

using namespace belle_sip_tester_runtime_component;
using namespace Platform;

#define MAX_TRACE_SIZE		512
#define MAX_SUITE_NAME_SIZE	128

static OutputTraceListener^ sTraceListener;
static belle_sip_object_pool_t *pool;

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


BelleSipTester::BelleSipTester()
{
	belle_sip_tester_init(nativeOutputTraceHandler);
	belle_sip_set_log_handler(belleSipNativeOutputTraceHandler);
}

BelleSipTester::~BelleSipTester()
{
	belle_sip_tester_uninit();
}

void BelleSipTester::setOutputTraceListener(OutputTraceListener^ traceListener)
{
	sTraceListener = traceListener;
}

void BelleSipTester::run(Platform::String^ suiteName, Platform::String^ caseName, Platform::Boolean verbose)
{
	std::wstring all(L"ALL");
	std::wstring wssuitename = suiteName->Data();
	std::wstring wscasename = caseName->Data();
	char csuitename[MAX_SUITE_NAME_SIZE] = { 0 };
	char ccasename[MAX_SUITE_NAME_SIZE] = { 0 };
	wcstombs(csuitename, wssuitename.c_str(), sizeof(csuitename));
	wcstombs(ccasename, wscasename.c_str(), sizeof(ccasename));

	if (verbose) {
		belle_sip_set_log_level(BELLE_SIP_LOG_DEBUG);
	} else {
		belle_sip_set_log_level(BELLE_SIP_LOG_ERROR);
	}

	belle_sip_tester_set_root_ca_path("Assets/rootca.pem");
	pool = belle_sip_object_pool_push();
	bc_tester_run_tests(wssuitename == all ? 0 : csuitename, wscasename == all ? 0 : ccasename);
}

unsigned int BelleSipTester::nbTestSuites()
{
	return bc_tester_nb_suites();
}

unsigned int BelleSipTester::nbTests(Platform::String^ suiteName)
{
	std::wstring suitename = suiteName->Data();
	char cname[MAX_SUITE_NAME_SIZE] = { 0 };
	wcstombs(cname, suitename.c_str(), sizeof(cname));
	return bc_tester_nb_tests(cname);
}

Platform::String^ BelleSipTester::testSuiteName(int index)
{
	const char *cname = bc_tester_suite_name(index);
	wchar_t wcname[MAX_SUITE_NAME_SIZE];
	mbstowcs(wcname, cname, sizeof(wcname));
	return ref new String(wcname);
}

Platform::String^ BelleSipTester::testName(Platform::String^ suiteName, int testIndex)
{
	std::wstring suitename = suiteName->Data();
	char csuitename[MAX_SUITE_NAME_SIZE] = { 0 };
	wcstombs(csuitename, suitename.c_str(), sizeof(csuitename));
	const char *cname = bc_tester_test_name(csuitename, testIndex);
	wchar_t wcname[MAX_SUITE_NAME_SIZE];
	mbstowcs(wcname, cname, sizeof(wcname));
	return ref new String(wcname);
}
