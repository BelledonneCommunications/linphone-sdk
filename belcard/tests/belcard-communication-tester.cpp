#include "belcard/belcard_communication.hpp"
#include "belcard-tester.hpp"

using namespace::std;
using namespace::belcard;

static void tel_property(void) {
	test_property<BelCardPhoneNumber>("TEL;VALUE=uri;TYPE=work:tel:+33-9-52-63-65-05\r\n");
	test_property<BelCardPhoneNumber>("TEL;VALUE=uri;TYPE=\"voice,work\":tel:+33952636505\r\n");
	test_property<BelCardPhoneNumber>("TEL;VALUE=text;TYPE=work:+33 9 52 63 65 05\r\n");
}

static void email_property(void) {
	test_property<BelCardEmail>("EMAIL;TYPE=work:sylvain.berfini@belledonne-communications.com\r\n");
}

static void impp_property(void) {
	test_property<BelCardImpp>("IMPP;TYPE=work:sip:sylvain@sip.linphone.org\r\n");
}

static void lang_property(void) {
	test_property<BelCardLang>("LANG;TYPE=home:fr\r\n");
	test_property<BelCardLang>("LANG:fr-FR\r\n");
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