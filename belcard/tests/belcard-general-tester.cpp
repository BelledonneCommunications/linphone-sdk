#include "belcard/belcard_general.hpp"
#include "common/bc_tester_utils.h"

#include <sstream>

using namespace::std;
using namespace::belcard;

static void kind_property(void) {
	string input = "KIND:individual\r\n";
	shared_ptr<BelCardKind> kind = BelCardKind::parse(input);
	BC_ASSERT_TRUE_FATAL(kind != NULL);
	stringstream sstream;
	sstream << *kind;
	BC_ASSERT_EQUAL(input.compare(sstream.str()), 0, int, "%d");
}

static test_t tests[] = {
	{ "Kind", kind_property },
};

test_suite_t vcard_general_properties_test_suite = {
	"General",
	NULL,
	NULL,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests
};