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
		VideoStream *m_video_stream;
	};
}

Mediastreamer2TesterVideoPrivate::Mediastreamer2TesterVideoPrivate()
	: m_video_stream(nullptr)
{
}

Mediastreamer2TesterVideoPrivate::~Mediastreamer2TesterVideoPrivate()
{
}

void Mediastreamer2TesterVideoPrivate::start()
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

	ms_filter_reset_statistics();
	m_video_stream = video_stream_new(20000, 0, FALSE);
	video_stream_set_display_filter_name(m_video_stream, "MSWP8Dis");
	video_stream_start(m_video_stream, &av_profile, "127.0.0.1", 20000, NULL, 0, 102, 0, camera);
}

void Mediastreamer2TesterVideoPrivate::stop()
{
	ms_filter_log_statistics();
	video_stream_stop(m_video_stream);
	m_video_stream = NULL;
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
