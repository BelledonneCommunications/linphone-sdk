#include <string>

#include "mediastreamer2_tester_windows.h"

using namespace ms2_tester_runtime_component;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::System::Threading;

#define MAX_TRACE_SIZE		2048
#define MAX_SUITE_NAME_SIZE	128
#define MAX_WRITABLE_DIR_SIZE 1024

static OutputTraceListener^ sTraceListener;

MS2Tester^ MS2Tester::_instance = ref new MS2Tester();

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
		case ORTP_FATAL:
		case ORTP_ERROR:
			l = ref new String(L"Error");
			break;
		case ORTP_WARNING:
			l = ref new String(L"Warning");
			break;
		case ORTP_MESSAGE:
			l = ref new String(L"Message");
			break;
		default:
			l = ref new String(L"Debug");
			break;
		}
		sTraceListener->outputTrace(l, msg);
	}
}

static void ms2NativeOutputTraceHandler(OrtpLogLevel lev, const char *fmt, va_list args)
{
	nativeOutputTraceHandler((int)lev, fmt, args);
}


MS2Tester::MS2Tester()
{
	char writable_dir[MAX_WRITABLE_DIR_SIZE];
	StorageFolder ^folder = ApplicationData::Current->LocalFolder;
	const wchar_t *wwritable_dir = folder->Path->Data();
	wcstombs(writable_dir, wwritable_dir, sizeof(writable_dir));
	mediastreamer2_tester_init(nativeOutputTraceHandler);
	bc_tester_set_resource_dir_prefix("Assets");
	bc_tester_set_writable_dir_prefix(writable_dir);
}

MS2Tester::~MS2Tester()
{
	mediastreamer2_tester_uninit();
}

void MS2Tester::setOutputTraceListener(OutputTraceListener^ traceListener)
{
	sTraceListener = traceListener;
}

void MS2Tester::init(bool verbose)
{
	if (verbose) {
		ortp_set_log_level_mask(ORTP_MESSAGE | ORTP_WARNING | ORTP_ERROR | ORTP_FATAL);
	}
	else {
		ortp_set_log_level_mask(ORTP_ERROR | ORTP_FATAL);
	}
}

bool MS2Tester::run(Platform::String^ suiteName, Platform::String^ caseName, Platform::Boolean verbose)
{
	std::wstring all(L"ALL");
	std::wstring wssuitename = suiteName->Data();
	std::wstring wscasename = caseName->Data();
	char csuitename[MAX_SUITE_NAME_SIZE] = { 0 };
	char ccasename[MAX_SUITE_NAME_SIZE] = { 0 };
	wcstombs(csuitename, wssuitename.c_str(), sizeof(csuitename));
	wcstombs(ccasename, wscasename.c_str(), sizeof(ccasename));

	init(verbose);
	ortp_set_log_handler(ms2NativeOutputTraceHandler);
	return bc_tester_run_tests(wssuitename == all ? 0 : csuitename, wscasename == all ? 0 : ccasename) != 0;
}

void MS2Tester::runAllToXml()
{
	auto workItem = ref new WorkItemHandler([this](IAsyncAction ^workItem) {
		char *xmlFile = bc_tester_file("MS2Windows10.xml");
		char *logFile = bc_tester_file("MS2Windows10.log");
		char *args[] = { "--xml-file", xmlFile };
		bc_tester_parse_args(2, args, 0);
		init(true);
		FILE *f = fopen(logFile, "w");
		ortp_set_log_file(f);
		bc_tester_start();
		bc_tester_uninit();
		fclose(f);
		free(xmlFile);
		free(logFile);
	});
	_asyncAction = ThreadPool::RunAsync(workItem);
}

unsigned int MS2Tester::nbTestSuites()
{
	return bc_tester_nb_suites();
}

unsigned int MS2Tester::nbTests(Platform::String^ suiteName)
{
	std::wstring suitename = suiteName->Data();
	char cname[MAX_SUITE_NAME_SIZE] = { 0 };
	wcstombs(cname, suitename.c_str(), sizeof(cname));
	return bc_tester_nb_tests(cname);
}

Platform::String^ MS2Tester::testSuiteName(int index)
{
	const char *cname = bc_tester_suite_name(index);
	wchar_t wcname[MAX_SUITE_NAME_SIZE];
	mbstowcs(wcname, cname, sizeof(wcname));
	return ref new String(wcname);
}

Platform::String^ MS2Tester::testName(Platform::String^ suiteName, int testIndex)
{
	std::wstring suitename = suiteName->Data();
	char csuitename[MAX_SUITE_NAME_SIZE] = { 0 };
	wcstombs(csuitename, suitename.c_str(), sizeof(csuitename));
	const char *cname = bc_tester_test_name(csuitename, testIndex);
	wchar_t wcname[MAX_SUITE_NAME_SIZE];
	mbstowcs(wcname, cname, sizeof(wcname));
	return ref new String(wcname);
}
