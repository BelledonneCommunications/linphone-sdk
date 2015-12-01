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
	
	input = "TEL;VALUE=uri;TYPE=\"voice,work\":tel:+33952636505\r\n";
	tel = BelCardTel::parse(input);
	BC_ASSERT_TRUE_FATAL(tel != NULL);
	sstream.clear();
	sstream.str(std::string());
	sstream << *tel;
	BC_ASSERT_EQUAL(input.compare(sstream.str()), 0, int, "%d");
	
	input = "TEL;VALUE=text;TYPE=work:+33 9 52 63 65 05\r\n";
	tel = BelCardTel::parse(input);
	BC_ASSERT_TRUE_FATAL(tel != NULL);
	sstream.clear();
	sstream.str(std::string());
	sstream << *tel;
	BC_ASSERT_EQUAL(input.compare(sstream.str()), 0, int, "%d");
}

static void email_property(void) {
	string input = "EMAIL;TYPE=work:sylvain.berfini@belledonne-communications.com\r\n";
	shared_ptr<BelCardEmail> email = BelCardEmail::parse(input);
	BC_ASSERT_TRUE_FATAL(email != NULL);
	stringstream sstream;
	sstream << *email;
	BC_ASSERT_EQUAL(input.compare(sstream.str()), 0, int, "%d");
}

static void impp_property(void) {
	string input = "IMPP;TYPE=work:sip:sylvain@sip.linphone.org\r\n";
	shared_ptr<BelCardImpp> impp = BelCardImpp::parse(input);
	BC_ASSERT_TRUE_FATAL(impp != NULL);
	stringstream sstream;
	sstream << *impp;
	BC_ASSERT_EQUAL(input.compare(sstream.str()), 0, int, "%d");
}

static void lang_property(void) {
	string input = "LANG;TYPE=home:fr\r\n";
	shared_ptr<BelCardLang> lang = BelCardLang::parse(input);
	BC_ASSERT_TRUE_FATAL(lang != NULL);
	stringstream sstream;
	sstream << *lang;
	BC_ASSERT_EQUAL(input.compare(sstream.str()), 0, int, "%d");
	
	input = "LANG:fr-FR\r\n";
	lang = BelCardLang::parse(input);
	BC_ASSERT_TRUE_FATAL(lang != NULL);
	sstream.clear();
	sstream.str(std::string());
	sstream << *lang;
	BC_ASSERT_EQUAL(input.compare(sstream.str()), 0, int, "%d");
}

static test_t tests[] = {
	{ "Tel", tel_property },
	{ "Email", email_property },
	{ "IMPP", impp_property },
	{ "Language", lang_property },
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