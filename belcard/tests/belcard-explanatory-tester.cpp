#include "belcard/belcard_explanatory.hpp"
#include "belcard-tester.hpp"

using namespace::std;
using namespace::belcard;

static void categories_property(void) {
	test_property<BelCardCategories>("CATEGORIES:INTERNET,IETF,INDUSTRY,INFORMATION TECHNOLOGY\r\n");
}

static void note_property(void) {
	test_property<BelCardNote>("NOTE:This fax number is operational 0800 to 1715\\n EST\\, Mon-Fri.\r\n");
}

static void prodid_property(void) {
	test_property<BelCardProductId>("PRODID:-//ONLINE DIRECTORY//NONSGML Version 1//EN\r\n");
}

static void rev_property(void) {
	test_property<BelCardRevision>("REV:19951031T222710Z\r\n");
}

static void sound_property(void) {
	test_property<BelCardSound>("SOUND:data:audio/basic;base64,MIICajCCAdOgAwIBAgICBEUwDQYJKoZIhAQEEBQAwdzELMAkGA1UEBhMCVVMxLDAqBgNVBAoTI05ldHNjYXBlIENvbW11bmljYXRpb25zIENvcnBvcmF0aW9uMRwwGgYDVQQLExNJbmZvcm1hdGlvbiBTeXN0\r\n");
}

static void uid_property(void) {
	test_property<BelCardUniqueId>("UID:urn:uuid:f81d4fae-7dec-11d0-a765-00a0c91e6bf6\r\n");
}

static void clientpidmap_property(void) {
	test_property<BelCardClientProductIdMap>("CLIENTPIDMAP:1;urn:uuid:3df403f4-5924-4bb7-b077-3c711d9eb34b\r\n");
}

static void url_property(void) {
	test_property<BelCardURL>("URL:http://example.org/restaurant.french/~chezchic.html\r\n");
}

static test_t tests[] = {
	{ "CATEGORIES", categories_property },
	{ "NOTE", note_property },
	{ "PRODID", prodid_property },
	{ "REV", rev_property },
	{ "SOUND", sound_property },
	{ "UID", uid_property },
	{ "CLIENTPIDMAP", clientpidmap_property },
	{ "URL", url_property },
};

test_suite_t vcard_explanatory_properties_test_suite = {
	"Explanatory",
	NULL,
	NULL,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests
};