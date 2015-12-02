#include "belcard/belcard_calendar.hpp"
#include "belcard-tester.hpp"

using namespace::std;
using namespace::belcard;

static void fburl_property(void) {
	test_property<BelCardFBURL>("FBURL;MEDIATYPE=text/calendar:ftp://example.com/busy/project-a.ifb\r\n");
}

static void caladruri_property(void) {
	test_property<BelCardCALADRURI>("CALADRURI:http://example.com/calendar/jdoe\r\n");
}

static void caluri_property(void) {
	test_property<BelCardCALURI>("CALURI;MEDIATYPE=text/calendar:ftp://ftp.example.com/calA.ics\r\n");
}

static test_t tests[] = {
	{ "FBURL", fburl_property },
	{ "CALADRURI", caladruri_property },
	{ "CALURI", caluri_property },
};

test_suite_t vcard_calendar_properties_test_suite = {
	"Calendar",
	NULL,
	NULL,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests
};