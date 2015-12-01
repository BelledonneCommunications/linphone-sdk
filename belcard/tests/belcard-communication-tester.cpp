#include "belcard/belcard_communication.hpp"
#include "common/bc_tester_utils.h"

#include <sstream>

using namespace::std;
using namespace::belcard;

static void tel_property(void) {
	string input = "TEL;VALUE=uri;TYPE=work:tel:+33-9-52-63-65-05\r\n";
	shared_ptr<BelCardTel> tel = BelCardTel::parse(input);
	BC_ASSERT_TRUE_FATAL(tel != NULL);
	stringstream sstream;
	sstream << *tel;
	BC_ASSERT_EQUAL(input.compare(sstream.str()), 0, int, "%d");
}

static test_t tests[] = {
	{ "Tel", tel_property },
};

test_suite_t vcard_communication_properties_test_suite = {
	"Communication",
	NULL,
	NULL,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests
};