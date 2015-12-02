#include "belcard/belcard_parser.hpp"
#include "common/bc_tester_utils.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

using namespace::std;
using namespace::belcard;

static string openFile(const char *name) {
	ifstream istr(bc_tester_file(name));
	if (!istr.is_open()) {
		BC_FAIL(name);
	}
	
	stringstream vcardStream;
	vcardStream << istr.rdbuf();
	string vcard = vcardStream.str();
	return vcard;
}

static void folding(void) {
	string vcard = openFile("vcards/foldtest.vcf");
	
	BelCardParser *parser = new BelCardParser();
	string folded_vcard = parser->fold(vcard);
	
	string unfolded_vcard = openFile("vcards/unfoldtest.vcf");
	BC_ASSERT_EQUAL(unfolded_vcard.compare(folded_vcard), 0, int, "%d");
	delete parser;
}

static void unfolding(void) {
	string vcard = openFile("vcards/unfoldtest.vcf");
	
	BelCardParser *parser = new BelCardParser();
	string unfolded_vcard = parser->unfold(vcard);
	
	string folded_vcard = openFile("vcards/foldtest.vcf");
	BC_ASSERT_EQUAL(folded_vcard.compare(unfolded_vcard), 0, int, "%d");
	delete parser;
}

static void vcard_parsing(void) {
	string vcard = openFile("vcards/vcard.vcf");
	
	BelCardParser *parser = new BelCardParser();
	shared_ptr<BelCard> belCard = parser->parse(vcard);
	BC_ASSERT_TRUE(belCard != NULL);
	//TODO: find a way to check the belCard object
	
	delete parser;
}

static test_t tests[] = {
	{ "Folding", folding },
	{ "Unfolding", unfolding },
	{ "VCard parsing", vcard_parsing },
};

test_suite_t vcard_test_suite = {
	"VCard",
	NULL,
	NULL,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests
};