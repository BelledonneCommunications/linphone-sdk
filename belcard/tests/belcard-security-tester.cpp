#include "belcard/belcard_security.hpp"
#include "belcard-tester.hpp"

using namespace::std;
using namespace::belcard;

static void key_property(void) {
	test_property<BelCardKey>("KEY;MEDIATYPE=application/pgp-keys:ftp://example.com/keys/jdoe\r\n");
	test_property<BelCardKey>("KEY:data:application/pgp-keys;base64,MIICajCCAdOgAwIBAgICBEUwDQYJKoZIhvcNAQEEBQAwdzELMAkGA1UEBhMCVVMxLDAqBgNVBAoTI05l\r\n");
}

static test_t tests[] = {
	{ "Key", key_property },
};

test_suite_t vcard_security_properties_test_suite = {
	"Security",
	NULL,
	NULL,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests
};