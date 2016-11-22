#include <string>
#include <collection.h>

#include "mediastreamer2_tester_windows.h"
#include "mswinrtvid.h"

using namespace BelledonneCommunications::Mediastreamer2::Tester;
using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::System::Threading;
using namespace Windows::UI::ViewManagement;

#define MAX_TRACE_SIZE		2048
#define MAX_SUITE_NAME_SIZE	128
#define MAX_WRITABLE_DIR_SIZE 1024
#define MAX_FILEPATH_SIZE	2048
#define MAX_DEVICE_NAME_SIZE 256

static OutputTraceListener^ sTraceListener;

NativeTester^ NativeTester::_instance = ref new NativeTester();

static void nativeOutputTraceHandler(int lev, const char *fmt, va_list args)
{
	wchar_t wstr[MAX_TRACE_SIZE];
	std::string str;
	str.resize(MAX_TRACE_SIZE);
	vsnprintf((char *)str.c_str(), MAX_TRACE_SIZE, fmt, args);
	mbstowcs(wstr, str.c_str(), MAX_TRACE_SIZE - 1);
	if (sTraceListener) {
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
	OutputDebugStringW(wstr);
	OutputDebugStringW(L"\n");
}

static void ms2NativeOutputTraceHandler(const char *domain, OrtpLogLevel lev, const char *fmt, va_list args)
{
	nativeOutputTraceHandler((int)lev, fmt, args);
}


NativeTester::NativeTester()
	: _deviceRotation(0)
{
}

NativeTester::~NativeTester()
{
	uninitMS2();
	mediastreamer2_tester_uninit();
}

void NativeTester::setOutputTraceListener(OutputTraceListener^ traceListener)
{
	sTraceListener = traceListener;
}

void NativeTester::initialize(StorageFolder^ writableDirectory, Platform::Boolean ui)
{
	if (ui) {
		mediastreamer2_tester_init(nativeOutputTraceHandler);
	}
	else {
		mediastreamer2_tester_init(NULL);
		ortp_set_log_level_mask(NULL, (OrtpLogLevel)(ORTP_MESSAGE | ORTP_WARNING | ORTP_ERROR | ORTP_FATAL));
	}

	char writable_dir[MAX_WRITABLE_DIR_SIZE] = { 0 };
	const wchar_t *wwritable_dir = writableDirectory->Path->Data();
	wcstombs(writable_dir, wwritable_dir, sizeof(writable_dir));
	bc_tester_set_writable_dir_prefix(writable_dir);
	bc_tester_set_resource_dir_prefix("Assets");

	if (!ui) {
		char *xmlFile = bc_tester_file("MS2Windows10.xml");
		char *args[] = { "--xml-file", xmlFile };
		bc_tester_parse_args(2, args, 0);

		char *logFile = bc_tester_file("MS2Windows10.log");
		mediastreamer2_tester_set_log_file(logFile);
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
		ortp_set_log_level_mask(NULL, ORTP_MESSAGE | ORTP_WARNING | ORTP_ERROR | ORTP_FATAL);
	}
	else {
		ortp_set_log_level_mask(NULL, ORTP_ERROR | ORTP_FATAL);
	}
	ortp_set_log_handler(ms2NativeOutputTraceHandler);
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

Windows::Foundation::Collections::IVector<Platform::String^>^ NativeTester::VideoDevices::get()
{
	if (_factory == nullptr) {
		initMS2();
	}
	wchar_t wcname[MAX_DEVICE_NAME_SIZE];
	Vector<Platform::String^>^ devices = ref new Vector<Platform::String^>();
	const MSList *elem = ms_web_cam_manager_get_list(ms_factory_get_web_cam_manager(_factory));
	for (int i = 0; elem != NULL; elem = elem->next, i++) {
		const char *id = ms_web_cam_get_string_id((MSWebCam *)elem->data);
		memset(wcname, 0, sizeof(wcname));
		mbstowcs(wcname, id, sizeof(wcname));
		devices->Append(ref new String(wcname));
	}
	return devices;
}

void NativeTester::initVideo()
{
	if (_factory == nullptr) {
		initMS2();
	}
	rtp_profile_set_payload(&av_profile, 100, &payload_type_h263);
	rtp_profile_set_payload(&av_profile, 101, &payload_type_mp4v);
	rtp_profile_set_payload(&av_profile, 102, &payload_type_h264);
	rtp_profile_set_payload(&av_profile, 103, &payload_type_vp8);
	Platform::String^ appFolder = Windows::ApplicationModel::Package::Current->InstalledLocation->Path;
	Platform::String^ psPath = Platform::String::Concat(appFolder, ref new Platform::String(L"\\Assets\\Images\\nowebcamCIF.jpg"));
	std::wstring wsPath = psPath->Data();
	char cPath[MAX_FILEPATH_SIZE] = { 0 };
	wcstombs(cPath, wsPath.c_str(), sizeof(cPath));
	ms_static_image_set_default_image(cPath);
}

void NativeTester::uninitVideo()
{
	uninitMS2();
}

#define PLATFORM_STRING_TO_C_STRING(x) \
	memset(cst, 0, sizeof(cst)); \
	wst = x->Data(); \
	wcstombs(cst, wst.c_str(), sizeof(cst))


void NativeTester::startVideoStream(Platform::String^ videoSwapChainPanelName, Platform::String^ previewSwapChainPanelName, Platform::String^ camera, Platform::String^ codec, Platform::String^ videoSize, unsigned int frameRate, unsigned int bitRate, Platform::Boolean usePreviewStream)
{
	ms_factory_enable_statistics(_factory, TRUE);
	ms_factory_reset_statistics(_factory);

	MSVideoSize vsize = { MS_VIDEO_SIZE_CIF_W, MS_VIDEO_SIZE_CIF_H };
	int payload = 103;
	char cst[1024];
	std::wstring wst;
	MSWebCamManager *manager = ms_factory_get_web_cam_manager(_factory);
	PLATFORM_STRING_TO_C_STRING(camera);
	MSWebCam *cam = ms_web_cam_manager_get_cam(manager, cst);
	PLATFORM_STRING_TO_C_STRING(codec);
	if (strcmp(cst, "H263") == 0) payload = 100;
	else if (strcmp(cst, "H264") == 0) payload = 102;
	else if (strcmp(cst, "MPEG4") == 0) payload = 101;
	else if (strcmp(cst, "VP8") == 0) payload = 103;
	PLATFORM_STRING_TO_C_STRING(videoSize);
	if (strcmp(cst, "720P") == 0) {
		vsize.width = MS_VIDEO_SIZE_720P_W;
		vsize.height = MS_VIDEO_SIZE_720P_H;
	} else if (strcmp(cst, "VGA") == 0) {
		vsize.width = MS_VIDEO_SIZE_VGA_W;
		vsize.height = MS_VIDEO_SIZE_VGA_H;
	} else if (strcmp(cst, "CIF") == 0) {
		vsize.width = MS_VIDEO_SIZE_CIF_W;
		vsize.height = MS_VIDEO_SIZE_CIF_H;
	} else if (strcmp(cst, "QVGA") == 0) {
		vsize.width = MS_VIDEO_SIZE_QVGA_W;
		vsize.height = MS_VIDEO_SIZE_QVGA_H;
	} else if (strcmp(cst, "QCIF") == 0) {
		vsize.width = MS_VIDEO_SIZE_QCIF_W;
		vsize.height = MS_VIDEO_SIZE_QCIF_H;
	}
	PayloadType *pt = rtp_profile_get_payload(&av_profile, payload);
	pt->normal_bitrate = bitRate * 1000;
	if (usePreviewStream) {
		_usePreviewStream = usePreviewStream;
		_videoStream = video_preview_new(_factory);
		RefToPtrProxy<Platform::String^> *nativeWindowId = new RefToPtrProxy<Platform::String^>(videoSwapChainPanelName);
		video_preview_set_native_window_id(_videoStream, nativeWindowId);
		video_preview_set_display_filter_name(_videoStream, "MSWinRTBackgroundDis");
		video_preview_set_size(_videoStream, vsize);
		video_preview_set_fps(_videoStream, (float)frameRate);
		video_preview_set_device_rotation(_videoStream, _deviceRotation);
		video_preview_start(_videoStream, cam);
	} else {
		_videoStream = video_stream_new(_factory, 20000, 0, FALSE);
		RefToPtrProxy<Platform::String^> *nativeWindowId = new RefToPtrProxy<Platform::String^>(videoSwapChainPanelName);
		video_stream_set_native_window_id(_videoStream, nativeWindowId);
		RefToPtrProxy<Platform::String^> *nativePreviewWindowId = new RefToPtrProxy<Platform::String^>(previewSwapChainPanelName);
		video_stream_set_native_preview_window_id(_videoStream, nativePreviewWindowId);
		video_stream_use_preview_video_window(_videoStream, TRUE);
		video_stream_set_display_filter_name(_videoStream, "MSWinRTBackgroundDis");
		video_stream_use_video_preset(_videoStream, "custom");
		video_stream_set_sent_video_size(_videoStream, vsize);
		video_stream_set_fps(_videoStream, (float)frameRate);
		video_stream_set_device_rotation(_videoStream, _deviceRotation);
		video_stream_start(_videoStream, &av_profile, "127.0.0.1", 20000, NULL, 0, payload, 0, cam);
	}
}

void NativeTester::stopVideoStream()
{
	ms_factory_log_statistics(_factory);
	if (_usePreviewStream) video_preview_stop(_videoStream);
	else video_stream_stop(_videoStream);
	_videoStream = NULL;
}

void NativeTester::changeCamera(Platform::String^ camera)
{
	char cst[1024];
	std::wstring wst;
	MSWebCamManager *manager = ms_factory_get_web_cam_manager(_factory);
	PLATFORM_STRING_TO_C_STRING(camera);
	MSWebCam *cam = ms_web_cam_manager_get_cam(manager, cst);
	if (_usePreviewStream) video_preview_change_camera(_videoStream, cam);
	else video_stream_change_camera(_videoStream, cam);
}

void NativeTester::setOrientation(int degrees)
{
	_deviceRotation = degrees;
	if (_videoStream != NULL) {
		if (_usePreviewStream) {
			video_preview_set_device_rotation(_videoStream, _deviceRotation);
			video_preview_update_video_params(_videoStream);
		} else {
			video_stream_set_device_rotation(_videoStream, _deviceRotation);
			video_stream_update_video_params(_videoStream);
		}
	}
}

void NativeTester::initMS2()
{
	if (_factory == nullptr) {
		ortp_init();
		ortp_set_log_level_mask(NULL, ORTP_MESSAGE | ORTP_WARNING | ORTP_ERROR | ORTP_FATAL);
		ortp_set_log_handler(ms2NativeOutputTraceHandler);
		_factory = ms_factory_new_with_voip();
	}
}

void NativeTester::uninitMS2()
{
	if (_factory != nullptr) {
		ms_factory_destroy(_factory);
		_factory = nullptr;
	}
}
