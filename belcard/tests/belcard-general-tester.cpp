#include "belcard/belcard_general.hpp"
#include "belcard-tester.hpp"

using namespace::std;
using namespace::belcard;

static void kind_property(void) {
	test_property<BelCardKind>("KIND:individual\r\n");
}

static void source_property(void) {
	test_property<BelCardSource>("SOURCE:ldap://ldap.example.com/cn=Babs%20Jensen,%20o=Babsco,%20c=US\r\n");
	test_property<BelCardSource>("SOURCE:http://directory.example.com/addressbooks/jdoe/Jean%20Dupont.vcf\r\n");
}

static void xml_property(void) {
	test_property<BelCardXML>("XML:<config xmlns=\"http://www.linphone.org/xsds/lpconfig.xsd\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:schemaLocation=\"http://www.linphone.org/xsds/lpconfig.xsd lpconfig.xsd\"></config>\r\n");
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