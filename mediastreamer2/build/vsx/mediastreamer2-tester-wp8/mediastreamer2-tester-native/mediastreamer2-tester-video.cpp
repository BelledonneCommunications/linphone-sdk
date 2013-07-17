#include "mediastreamer2-tester-video.h"

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msrtp.h"
#include "mediastreamer2/msticker.h"
#include "private.h"
#include "mediastreamer2_tester_private.h"

using namespace mediastreamer2_tester_native;


template <class T> class RefToPtrProxy
{
public:
	RefToPtrProxy(T obj) : mObj(obj) {}
	~RefToPtrProxy() { mObj = nullptr; }
	T Ref() { return mObj; }
private:
	T mObj;
};


namespace mediastreamer2_tester_native
{
	class Mediastreamer2TesterVideoPrivate
	{
	public:
		Mediastreamer2TesterVideoPrivate(Mediastreamer2::WP8Video::IVideoRenderer^ videoRenderer);
		virtual ~Mediastreamer2TesterVideoPrivate();

		void start();
		void stop();
		int getNativeWindowId();
		MSWebCam *getCamera() { return m_camera; }

	private:
		VideoStream *m_video_stream;
		Mediastreamer2::WP8Video::IVideoRenderer^ m_video_renderer;
		MSWebCam *m_camera;
	};
}

Mediastreamer2TesterVideoPrivate::Mediastreamer2TesterVideoPrivate(Mediastreamer2::WP8Video::IVideoRenderer^ videoRenderer)
	: m_video_stream(nullptr), m_video_renderer(videoRenderer), m_camera(nullptr)
{
}

Mediastreamer2TesterVideoPrivate::~Mediastreamer2TesterVideoPrivate()
{
}

void Mediastreamer2TesterVideoPrivate::start()
{
	MSWebCamManager *cam_manager;

	cam_manager = ms_web_cam_manager_get();
	m_camera = ms_web_cam_manager_get_cam(cam_manager, "MSWP8Cap: Front");
	if (m_camera == NULL) {
		m_camera = ms_web_cam_manager_get_cam(cam_manager, "MSWP8Cap: Back");
	}
	if (m_camera == NULL) {
		m_camera = ms_web_cam_manager_get_default_cam(cam_manager);
	}

	ms_filter_reset_statistics();
	m_video_stream = video_stream_new(20000, 0, FALSE);
	RefToPtrProxy<Mediastreamer2::WP8Video::IVideoRenderer^> *renderer = new RefToPtrProxy<Mediastreamer2::WP8Video::IVideoRenderer^>(m_video_renderer);
	video_stream_set_native_window_id(m_video_stream, (unsigned long)renderer);
	video_stream_set_display_filter_name(m_video_stream, "MSWP8Dis");
	video_stream_start(m_video_stream, &av_profile, "127.0.0.1", 20000, NULL, 0, 102, 0, m_camera);
}

void Mediastreamer2TesterVideoPrivate::stop()
{
	ms_filter_log_statistics();
	video_stream_stop(m_video_stream);
	m_video_stream = NULL;
}

int Mediastreamer2TesterVideoPrivate::getNativeWindowId()
{
	return video_stream_get_native_window_id(m_video_stream);
}

Mediastreamer2TesterVideo::Mediastreamer2TesterVideo(Mediastreamer2::WP8Video::IVideoRenderer^ videoRenderer)
	: d(new Mediastreamer2TesterVideoPrivate(videoRenderer))
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

int Mediastreamer2TesterVideo::GetNativeWindowId()
{
	return d->getNativeWindowId();
}

Platform::String^ Mediastreamer2TesterVideo::GetVideoDevice()
{
	MSWebCam *cam = d->getCamera();
	const char *cid = ms_web_cam_get_string_id(cam);
	wchar_t wstr[512];
	mbstowcs(wstr, cid, sizeof(wstr));
	Platform::String^ id = ref new Platform::String(wstr);
	return id;
}
