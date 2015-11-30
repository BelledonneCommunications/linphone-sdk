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
	delete parser;
}

static void unfolding(void) {
	string vcard = openFile("vcards/unfoldtest.vcf");
	
	BelCardParser *parser = new BelCardParser();
	string unfolded_vcard = parser->unfold(vcard);
	delete parser;
}

static void vcard_parsing(void) {
	string vcard = openFile("vcards/vcard.vcf");
	
	BelCardParser *parser = new BelCardParser();
	shared_ptr<BelCard> belCard = parser->parse(vcard);
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