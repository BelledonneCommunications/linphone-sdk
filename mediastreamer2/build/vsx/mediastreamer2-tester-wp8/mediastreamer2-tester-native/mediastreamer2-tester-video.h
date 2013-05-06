#pragma once

namespace mediastreamer2_tester_native
{
	class Mediastreamer2TesterVideoPrivate;

    public ref class Mediastreamer2TesterVideo sealed
    {
    public:
        Mediastreamer2TesterVideo();
		virtual ~Mediastreamer2TesterVideo();

	private:
		Mediastreamer2TesterVideoPrivate *d;
    };
}