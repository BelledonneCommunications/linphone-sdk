#include "mediastreamer2-tester-video.h"

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msrtp.h"
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
		void createGraphs();
		void destroyGraphs();
		void destroyFilters();
		void destroyTicker();

		MSTicker *m_ticker;
		MSFilter *m_video_capture;
		MSFilter *m_video_display;
		MSFilter *m_rtp_send;
		MSFilter *m_rtp_recv;
		RtpSession *m_rtps;
	};
}

Mediastreamer2TesterVideoPrivate::Mediastreamer2TesterVideoPrivate()
	: m_ticker(nullptr), m_video_capture(nullptr), m_video_display(nullptr),
	m_rtp_send(nullptr), m_rtp_recv(nullptr), m_rtps(nullptr)
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
	createGraphs();
}

void Mediastreamer2TesterVideoPrivate::stop()
{
	destroyGraphs();
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
	camera = ms_web_cam_manager_get_cam(cam_manager, "MSWP8Cap: Front");
	if (camera == NULL) {
		camera = ms_web_cam_manager_get_cam(cam_manager, "MSWP8Cap: Back");
	}
	if (camera == NULL) {
		camera = ms_web_cam_manager_get_default_cam(cam_manager);
	}
	m_video_capture = ms_web_cam_create_reader(camera);
	m_video_display = ms_filter_new_from_name("MSWP8Dis");
	m_rtp_send = ms_filter_new(MS_RTP_SEND_ID);
	m_rtp_recv = ms_filter_new(MS_RTP_RECV_ID);
	m_rtps = create_duplex_rtpsession(20000, 0, FALSE);
	rtp_session_set_remote_addr_full(m_rtps, "127.0.0.1", 20000, NULL, 0);
	rtp_session_set_payload_type(m_rtps, 102);
	rtp_session_enable_rtcp(m_rtps,FALSE);
	ms_filter_call_method(m_rtp_send, MS_RTP_SEND_SET_SESSION, m_rtps);
	ms_filter_call_method(m_rtp_recv, MS_RTP_RECV_SET_SESSION, m_rtps);
}

void Mediastreamer2TesterVideoPrivate::createGraphs()
{
	MSConnectionHelper h;

	// Sending graph
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, m_video_capture, -1, 0);
	ms_connection_helper_link(&h, m_rtp_send, 0, -1);

	// Receiving graph
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, m_rtp_recv, -1, 0);
	ms_connection_helper_link(&h, m_video_display, 0, -1);

	ms_ticker_attach_multiple(m_ticker, m_video_capture, m_rtp_recv, NULL);
}

void Mediastreamer2TesterVideoPrivate::destroyGraphs()
{
	MSConnectionHelper h;

	ms_ticker_detach(m_ticker, m_video_capture);
	ms_ticker_detach(m_ticker, m_rtp_recv);

	// Sending graph
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, m_video_capture, -1, 0);
	ms_connection_helper_unlink(&h, m_rtp_send, 0, -1);

	// Receiving graph
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, m_rtp_recv, -1, -0);
	ms_connection_helper_unlink(&h, m_video_display, 0, -1);
}

void Mediastreamer2TesterVideoPrivate::destroyFilters()
{
	if (m_rtps != nullptr) {
		rtp_session_destroy(m_rtps);
		m_rtps = nullptr;
	}
	if (m_rtp_recv != nullptr) {
		ms_filter_destroy(m_rtp_recv);
		m_rtp_recv = nullptr;
	}
	if (m_rtp_send != nullptr) {
		ms_filter_destroy(m_rtp_send);
		m_rtp_send = nullptr;
	}
	if (m_video_display != nullptr) {
		ms_filter_destroy(m_video_display);
		m_video_display = nullptr;
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
	rtp_profile_set_payload(&av_profile, 102, &payload_type_h264);
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
