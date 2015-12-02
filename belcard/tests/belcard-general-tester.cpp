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

static void source_property(void) {
	string input = "SOURCE:ldap://ldap.example.com/cn=Babs%20Jensen,%20o=Babsco,%20c=US\r\n";
	shared_ptr<BelCardSource> source = BelCardSource::parse(input);
	BC_ASSERT_TRUE_FATAL(source != NULL);
	stringstream sstream;
	sstream << *source;
	BC_ASSERT_EQUAL(input.compare(sstream.str()), 0, int, "%d");
	
	input = "SOURCE:http://directory.example.com/addressbooks/jdoe/Jean%20Dupont.vcf\r\n";
	source = BelCardSource::parse(input);
	BC_ASSERT_TRUE_FATAL(source != NULL);
	sstream.clear();
	sstream.str(std::string());
	sstream << *source;
	BC_ASSERT_EQUAL(input.compare(sstream.str()), 0, int, "%d");
}

static void xml_property(void) {
	string input = "XML:<config xmlns=\"http://www.linphone.org/xsds/lpconfig.xsd\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:schemaLocation=\"http://www.linphone.org/xsds/lpconfig.xsd lpconfig.xsd\"></config>\r\n";
	shared_ptr<BelCardXML> xml = BelCardXML::parse(input);
	BC_ASSERT_TRUE_FATAL(xml != NULL);
	stringstream sstream;
	sstream << *xml;
	BC_ASSERT_EQUAL(input.compare(sstream.str()), 0, int, "%d");
}

static test_t tests[] = {
	{ "Kind", kind_property },
	{ "Source", source_property },
	{ "XML", xml_property },
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