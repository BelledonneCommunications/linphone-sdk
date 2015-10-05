#pragma once

#include "belle-sip/belle-sip.h"
#include "belle_sip_tester.h"

namespace belle_sip_tester_runtime_component
{
	public interface class OutputTraceListener
	{
	public:
		void outputTrace(Platform::String^ lev, Platform::String^ msg);
	};

    public ref class BelleSipTester sealed
    {
    public:
		void setOutputTraceListener(OutputTraceListener^ traceListener);
		unsigned int nbTestSuites();
		unsigned int nbTests(Platform::String^ suiteName);
		Platform::String^ testSuiteName(int index);
		Platform::String^ testName(Platform::String^ suiteName, int testIndex);
		void initialize(Windows::Storage::StorageFolder^ writableDirectory, Platform::Boolean ui);
		bool run(Platform::String^ suiteName, Platform::String^ caseName, Platform::Boolean verbose);
		void runAllToXml();

		static property BelleSipTester^ Instance
		{
			BelleSipTester^ get() { return _instance; }
		}
		property Windows::Foundation::IAsyncAction^ AsyncAction
		{
			Windows::Foundation::IAsyncAction^ get() { return _asyncAction; }
		}
	private:
		BelleSipTester();
		~BelleSipTester();

		static BelleSipTester^ _instance;
		Windows::Foundation::IAsyncAction^ _asyncAction;
	};
}