#pragma once

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2_tester.h"

namespace ms2_tester_runtime_component
{
	public interface class OutputTraceListener
	{
	public:
		void outputTrace(Platform::String^ lev, Platform::String^ msg);
	};

    public ref class MS2Tester sealed
    {
    public:
		void setWritableDirectory(Windows::Storage::StorageFolder^ folder);
		void setOutputTraceListener(OutputTraceListener^ traceListener);
		unsigned int nbTestSuites();
		unsigned int nbTests(Platform::String^ suiteName);
		Platform::String^ testSuiteName(int index);
		Platform::String^ testName(Platform::String^ suiteName, int testIndex);
		bool run(Platform::String^ suiteName, Platform::String^ caseName, Platform::Boolean verbose);
		void runAllToXml();
		void initVideo();
		void uninitVideo();
		void startVideoStream(Platform::Object^ CaptureElement, Platform::Object^ MediaElement, Platform::String^ camera, Platform::String^ codec, Platform::String^ videoSize, unsigned int frameRate, unsigned int bitRate);
		void stopVideoStream();
		int getOrientation() { return _deviceRotation; }
		void setOrientation(int degrees);

		static property MS2Tester^ Instance
		{
			MS2Tester^ get() { return _instance; }
		}
		property Windows::Foundation::IAsyncAction^ AsyncAction
		{
			Windows::Foundation::IAsyncAction^ get() { return _asyncAction; }
		}
		property Windows::Foundation::Collections::IVector<Platform::String^>^ VideoDevices
		{
			Windows::Foundation::Collections::IVector<Platform::String^>^ get();
		}
	private:
		MS2Tester();
		~MS2Tester();
		void init(bool verbose);

		static MS2Tester^ _instance;
		Windows::Foundation::IAsyncAction^ _asyncAction;
		VideoStream *_videoStream;
		int _deviceRotation;
	};
}