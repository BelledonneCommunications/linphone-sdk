#include "mediastreamer2-tester-video.h"

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "private.h"
#include "mediastreamer2_tester_private.h"

using namespace mediastreamer2_tester_native;


namespace mediastreamer2_tester_native
{
	class Mediastreamer2TesterVideoPrivate
	{
	public:
		Mediastreamer2TesterVideoPrivate();
		virtual ~Mediastreamer2TesterVideoPrivate();

		void start();
		void stop();

	private:
		void createTicker();
		void createFilters();
		void createGraph();
		void destroyGraph();
		void destroyFilters();
		void destroyTicker();

		MSTicker *m_ticker;
		MSFilter *m_video_capture;
		MSFilter *m_void_sink;
	};
}

Mediastreamer2TesterVideoPrivate::Mediastreamer2TesterVideoPrivate()
	: m_ticker(nullptr), m_video_capture(nullptr), m_void_sink(nullptr)
{
}

Mediastreamer2TesterVideoPrivate::~Mediastreamer2TesterVideoPrivate()
{
}

void Mediastreamer2TesterVideoPrivate::start()
{
	ms_filter_reset_statistics();
	createTicker();
	createFilters();
	createGraph();
}

void Mediastreamer2TesterVideoPrivate::stop()
{
	destroyGraph();
	ms_filter_log_statistics();
	destroyFilters();
	destroyTicker();
}

void Mediastreamer2TesterVideoPrivate::createTicker()
{
	MSTickerParams params;
	params.name = "Video Tester MSTicker";
	params.prio = MS_TICKER_PRIO_NORMAL;
	m_ticker = ms_ticker_new_with_params(&params);
}

void Mediastreamer2TesterVideoPrivate::createFilters()
{
	MSWebCamManager *cam_manager;
	MSWebCam *camera;
	cam_manager = ms_web_cam_manager_get();
	camera = ms_web_cam_manager_get_default_cam(cam_manager);
	m_video_capture = ms_web_cam_create_reader(camera);
	m_void_sink = ms_filter_new(MS_VOID_SINK_ID);
}

void Mediastreamer2TesterVideoPrivate::createGraph()
{
	MSConnectionHelper h;
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, m_video_capture, -1, 0);
	ms_connection_helper_link(&h, m_void_sink, 0, -1);
	ms_ticker_attach(m_ticker, m_video_capture);
}

void Mediastreamer2TesterVideoPrivate::destroyGraph()
{
	MSConnectionHelper h;
	ms_ticker_detach(m_ticker, m_video_capture);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, m_video_capture, -1, 0);
	ms_connection_helper_unlink(&h, m_void_sink, 0, -1);
}

void Mediastreamer2TesterVideoPrivate::destroyFilters()
{
	if (m_void_sink != nullptr) {
		ms_filter_destroy(m_void_sink);
		m_void_sink = nullptr;
	}
	if (m_video_capture != nullptr) {
		ms_filter_destroy(m_video_capture);
		m_video_capture = nullptr;
	}
}

void Mediastreamer2TesterVideoPrivate::destroyTicker()
{
	if (m_ticker != nullptr) {
		ms_ticker_destroy(m_ticker);
		m_ticker = nullptr;
	}
}

Mediastreamer2TesterVideo::Mediastreamer2TesterVideo()
	: d(new Mediastreamer2TesterVideoPrivate())
{
	ms_init();
	ms_filter_enable_statistics(TRUE);
	ortp_init();
	d->start();
}

Mediastreamer2TesterVideo::~Mediastreamer2TesterVideo()
{
	d->stop();
	ms_exit();
	delete d;
}
