#pragma once

#include "mediastreamer2_tester.h"

namespace mediastreamer2_tester_native
{
	public interface class OutputTraceListener
	{
	public:
		void outputTrace(Platform::String^ msg);
	};

    public ref class Mediastreamer2TesterNative sealed
    {
    public:
        Mediastreamer2TesterNative();
		virtual ~Mediastreamer2TesterNative();
		void setOutputTraceListener(OutputTraceListener^ traceListener);
		unsigned int nbTestSuites();
		Platform::String^ testSuiteName(int index);
		void run(Platform::String^ name, Platform::Boolean verbose);
    };
}