#include "belcard/belcard_addressing.hpp"
#include "common/bc_tester_utils.h"

#include <sstream>

using namespace::std;
using namespace::belcard;

static void adr_property(void) {
	string input = "ADR;TYPE=work:;;34 avenue de l'Europe,le Trident bat D;GRENOBLE;;38100;FRANCE\r\n";
	shared_ptr<BelCardAddress> addr = BelCardAddress::parse(input);
	BC_ASSERT_TRUE_FATAL(addr != NULL);
	stringstream sstream;
	sstream << *addr;
	BC_ASSERT_EQUAL(input.compare(sstream.str()), 0, int, "%d");
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