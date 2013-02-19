#pragma once

#ifdef __cplusplus
extern "C" {
#endif
	int belle_sip_tester_run_tests(char *suite_name, char *test_name);
#ifdef __cplusplus
}
#endif

namespace belle_sip_tester_native
{
    public ref class CainSipTesterNative sealed
    {
    public:
        CainSipTesterNative();
		void run(Platform::String^ name, Platform::Boolean verbose);
    };
}