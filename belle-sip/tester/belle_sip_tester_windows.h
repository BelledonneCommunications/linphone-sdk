#pragma once

#include "belle-sip/belle-sip.h"
#include "belle_sip_tester.h"

namespace belle_sip_tester_runtime_component
{
	public interface class OutputTraceListener
	{
	public:
		void outputTrace(Platform::String^ msg);
	};

    public ref class BelleSipTester sealed
    {
    public:
        BelleSipTester(bool autoLaunch);
		virtual ~BelleSipTester();
		void setOutputTraceListener(OutputTraceListener^ traceListener);
		unsigned int nbTestSuites();
		unsigned int nbTests(Platform::String^ suiteName);
		Platform::String^ testSuiteName(int index);
		Platform::String^ testName(Platform::String^ suiteName, int testIndex);
		void run(Platform::String^ suiteName, Platform::String^ caseName, Platform::Boolean verbose);

		property Windows::Foundation::IAsyncAction^ AsyncAction
		{
			Windows::Foundation::IAsyncAction^ get();
		}
	private:
		void init(bool verbose);
	
		Windows::Foundation::IAsyncAction ^asyncAction;
	};
}