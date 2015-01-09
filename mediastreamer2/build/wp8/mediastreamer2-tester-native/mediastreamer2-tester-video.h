#pragma once

namespace mediastreamer2_tester_native
{
	class Mediastreamer2TesterVideoPrivate;

    public ref class Mediastreamer2TesterVideo sealed
    {
    public:
        Mediastreamer2TesterVideo(Mediastreamer2::WP8Video::IVideoRenderer^ videoRenderer);
		virtual ~Mediastreamer2TesterVideo();
		int GetNativeWindowId();
		Platform::String^ GetVideoDevice();

	private:
		Mediastreamer2TesterVideoPrivate *d;
    };
}
