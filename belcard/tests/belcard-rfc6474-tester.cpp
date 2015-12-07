#include "belcard/belcard_rfc6474.hpp"
#include "belcard-tester.hpp"

using namespace::std;
using namespace::belcard;

static void birth_place_property(void) {
	test_property<BelCardBirthPlace>("BIRTHPLACE:Babies'R'Us Hospital\r\n");
	test_property<BelCardBirthPlace>("BIRTHPLACE;VALUE=uri:http://example.com/hospitals/babiesrus.vcf\r\n");
	test_property<BelCardBirthPlace>("BIRTHPLACE;VALUE=uri:geo:46.769307\\,-71.283079\r\n");
}

static void death_place_property(void) {
	test_property<BelCardDeathPlace>("DEATHPLACE:Aboard the Titanic\\, near Newfoundland\r\n");
	test_property<BelCardDeathPlace>("DEATHPLACE;VALUE=uri:http://example.com/ships/titanic.vcf\r\n");
	test_property<BelCardDeathPlace>("DEATHPLACE;VALUE=uri:geo:41.731944\\,-49.945833\r\n");
}

static void death_date_property(void) {
	test_property<BelCardDeathDate>("DEATHDATE:19960415\r\n");
	test_property<BelCardDeathDate>("DEATHDATE:--0415\r\n");
	test_property<BelCardDeathDate>("DEATHDATE:19531015T231000Z\r\n");
	test_property<BelCardDeathDate>("DEATHDATE;VALUE=text:circa 1800\r\n");
}

static test_t tests[] = {
	{ "Birth place", birth_place_property },
	{ "Death place", death_place_property },
	{ "Death date", death_date_property },
};

test_suite_t vcard_rfc6474_properties_test_suite = {
	"RFC 6474",
	NULL,
	NULL,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests
};