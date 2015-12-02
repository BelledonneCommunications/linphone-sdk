#include "belcard/belcard_geographical.hpp"
#include "belcard-tester.hpp"

using namespace::std;
using namespace::belcard;

static void tz_property(void) {
	test_property<BelCardTimezone>("TZ:Paris/Europe\r\n");
}

static void geo_property(void) {
	test_property<BelCardGeo>("GEO:geo:45.159612,5.732511\r\n");
}

static test_t tests[] = {
	{ "TZ", tz_property },
	{ "Geo", geo_property },
};

test_suite_t vcard_geographical_properties_test_suite = {
	"Geographical",
	NULL,
	NULL,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests
};