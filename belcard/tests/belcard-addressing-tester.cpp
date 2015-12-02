#include "belcard/belcard_addressing.hpp"
#include "belcard-tester.hpp"

using namespace::std;
using namespace::belcard;

static void adr_property(void) {
	test_property<BelCardAddress>("ADR;TYPE=work:;;34 avenue de l'Europe,le Trident bat D;GRENOBLE;;38100;FRANCE\r\n");
}

static test_t tests[] = {
	{ "Adr", adr_property },
};

test_suite_t vcard_addressing_properties_test_suite = {
	"Addressing",
	NULL,
	NULL,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests
};