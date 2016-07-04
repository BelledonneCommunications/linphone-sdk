#include <string>

#include "belle_sip_tester_windows.h"

using namespace BelledonneCommunications::BelleSip::Tester;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::System::Threading;

#define MAX_TRACE_SIZE		2048
#define MAX_SUITE_NAME_SIZE	128
#define MAX_WRITABLE_DIR_SIZE 1024

static OutputTraceListener^ sTraceListener;
static belle_sip_object_pool_t *pool;

NativeTester^ NativeTester::_instance = ref new NativeTester();

static void nativeOutputTraceHandler(int lev, const char *fmt, va_list args)
{
	if (sTraceListener) {
		wchar_t wstr[MAX_TRACE_SIZE];
		std::string str;
		str.resize(MAX_TRACE_SIZE);
		vsnprintf((char *)str.c_str(), MAX_TRACE_SIZE, fmt, args);
		mbstowcs(wstr, str.c_str(), MAX_TRACE_SIZE - 1);
		String^ msg = ref new String(wstr);
		String^ l;
		switch (lev) {
		case BELLE_SIP_LOG_FATAL:
		case BELLE_SIP_LOG_ERROR:
			l = ref new String(L"Error");
			break;
		case BELLE_SIP_LOG_WARNING:
			l = ref new String(L"Warning");
			break;
		case BELLE_SIP_LOG_MESSAGE:
			l = ref new String(L"Message");
			break;
		default:
			l = ref new String(L"Debug");
			break;
		}
		sTraceListener->outputTrace(l, msg);
	}
}

static void belleSipNativeOutputTraceHandler(const char *domain, BctbxLogLevel lev, const char *fmt, va_list args)
{
	nativeOutputTraceHandler((int)lev, fmt, args);
}


NativeTester::NativeTester()
{
}

NativeTester::~NativeTester()
{
	belle_sip_tester_uninit();
}

void NativeTester::setOutputTraceListener(OutputTraceListener^ traceListener)
{
	sTraceListener = traceListener;
}

void NativeTester::initialize(StorageFolder^ writableDirectory, Platform::Boolean ui)
{
	if (ui) {
		belle_sip_tester_init(nativeOutputTraceHandler);
	}
	else {
		belle_sip_tester_init(NULL);
		belle_sip_set_log_level(BELLE_SIP_LOG_DEBUG);
	}

	belle_sip_tester_set_root_ca_path("Assets/rootca.pem");
	pool = belle_sip_object_pool_push();

	char writable_dir[MAX_WRITABLE_DIR_SIZE] = { 0 };
	const wchar_t *wwritable_dir = writableDirectory->Path->Data();
	wcstombs(writable_dir, wwritable_dir, sizeof(writable_dir));
	bc_tester_set_writable_dir_prefix(writable_dir);
	bc_tester_set_resource_dir_prefix("Assets");

	if (!ui) {
		char *xmlFile = bc_tester_file("BelleSipWindows10.xml");
		char *args[] = { "--xml-file", xmlFile };
		bc_tester_parse_args(2, args, 0);

		char *logFile = bc_tester_file("BelleSipWindows10.log");
		belle_sip_tester_set_log_file(logFile);
		free(logFile);
	}
}

bool NativeTester::run(Platform::String^ suiteName, Platform::String^ caseName, Platform::Boolean verbose)
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
	}
	else {
		belle_sip_set_log_level(BELLE_SIP_LOG_ERROR);
	}
	belle_sip_set_log_handler(belleSipNativeOutputTraceHandler);
	return bc_tester_run_tests(wssuitename == all ? 0 : csuitename, wscasename == all ? 0 : ccasename, NULL) != 0;
}

void NativeTester::runAllToXml()
{
	auto workItem = ref new WorkItemHandler([this](IAsyncAction ^workItem) {
		bc_tester_start(NULL);
		bc_tester_uninit();
	});
	_asyncAction = ThreadPool::RunAsync(workItem);
}

unsigned int NativeTester::nbTestSuites()
{
	return bc_tester_nb_suites();
}

unsigned int NativeTester::nbTests(Platform::String^ suiteName)
{
	std::wstring suitename = suiteName->Data();
	char cname[MAX_SUITE_NAME_SIZE] = { 0 };
	wcstombs(cname, suitename.c_str(), sizeof(cname));
	return bc_tester_nb_tests(cname);
}

Platform::String^ NativeTester::testSuiteName(int index)
{
	const char *cname = bc_tester_suite_name(index);
	wchar_t wcname[MAX_SUITE_NAME_SIZE];
	mbstowcs(wcname, cname, sizeof(wcname));
	return ref new String(wcname);
}

Platform::String^ NativeTester::testName(Platform::String^ suiteName, int testIndex)
{
	std::wstring suitename = suiteName->Data();
	char csuitename[MAX_SUITE_NAME_SIZE] = { 0 };
	wcstombs(csuitename, suitename.c_str(), sizeof(csuitename));
	const char *cname = bc_tester_test_name(csuitename, testIndex);
	wchar_t wcname[MAX_SUITE_NAME_SIZE];
	mbstowcs(wcname, cname, sizeof(wcname));
	return ref new String(wcname);
}
