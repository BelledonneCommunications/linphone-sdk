#pragma once

#include "mediastreamer2_tester.h"

namespace mediastreamer2_tester_native
{
	enum OutputTraceLevel {
		Debug,
		Message,
		Warning,
		Error,
		Raw
	};

	public interface class OutputTraceListener
	{
	public:
		void outputTrace(int level, Platform::String^ msg);
	};

    public ref class Mediastreamer2TesterNative sealed
    {
    public:
        Mediastreamer2TesterNative();
		virtual ~Mediastreamer2TesterNative();
		void setOutputTraceListener(OutputTraceListener^ traceListener);
		unsigned int nbTestSuites();
		unsigned int nbTests(Platform::String^ suiteName);
		Platform::String^ testSuiteName(int index);
		Platform::String^ testName(Platform::String^ suiteName, int testIndex);
		void run(Platform::String^ suiteName, Platform::String^ caseName, Platform::Boolean verbose);
    };
}
