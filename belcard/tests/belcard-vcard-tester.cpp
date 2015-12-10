#include "belcard/belcard_parser.hpp"
#include "belcard/belcard_utils.hpp"
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
	string folded_vcard = belcard_fold(vcard);
	
	string unfolded_vcard = openFile("vcards/unfoldtest.vcf");
	BC_ASSERT_EQUAL(unfolded_vcard.compare(folded_vcard), 0, int, "%d");
	
	delete parser;
}

static void unfolding(void) {
	string vcard = openFile("vcards/unfoldtest.vcf");
	
	BelCardParser *parser = new BelCardParser();
	string unfolded_vcard = belcard_unfold(vcard);
	
	string folded_vcard = openFile("vcards/foldtest.vcf");
	BC_ASSERT_EQUAL(folded_vcard.compare(unfolded_vcard), 0, int, "%d");
	
	delete parser;
}

static void vcard_parsing(void) {
	string vcard = openFile("vcards/vcard.vcf");
	
	BelCardParser *parser = new BelCardParser();
	shared_ptr<BelCard> belCard = parser->parseOne(vcard);
	BC_ASSERT_TRUE_FATAL(belCard != NULL);
	BC_ASSERT_TRUE(belCard->assertRFCCompliance());
	
	string vcard2 = belCard->toFoldedString();
	BC_ASSERT_EQUAL(vcard2.compare(vcard), 0, int, "%d");
	
	delete parser;
}

static void vcards_parsing(void) {
	string vcards = openFile("vcards/vcards.vcf");
	
	BelCardParser *parser = new BelCardParser();
	shared_ptr<BelCardList> belCards = parser->parse(vcards);
	BC_ASSERT_TRUE_FATAL(belCards != NULL);
	BC_ASSERT_TRUE(belCards->getCards().size() == 2);
	
	string vcards2 = belCards->toString();
	BC_ASSERT_EQUAL(vcards2.compare(vcards), 0, int, "%d");
	
	delete parser;
}

static void create_vcard_from_api(void) {
	shared_ptr<BelCard> belCard = BelCard::create<BelCard>();
	BC_ASSERT_TRUE_FATAL(belCard != NULL);
	BC_ASSERT_FALSE(belCard->assertRFCCompliance());
	
	shared_ptr<BelCardFullName> fn = BelCard::create<BelCardFullName>();
	fn->setValue("Sylvain Berfini");
	belCard->setFullName(fn);
	BC_ASSERT_TRUE(belCard->assertRFCCompliance());
	
	string vcard = belCard->toString();
	BelCardParser *parser = new BelCardParser();
	shared_ptr<BelCard> belCard2 = parser->parseOne(vcard);
	BC_ASSERT_TRUE_FATAL(belCard2 != NULL);
	BC_ASSERT_TRUE(belCard2->assertRFCCompliance());
	string vcard2 = belCard2->toString();
	BC_ASSERT_TRUE(vcard.compare(vcard2) == 0);
}

static test_t tests[] = {
	{ "Folding", folding },
	{ "Unfolding", unfolding },
	{ "VCard parsing", vcard_parsing },
	{ "VCards parsing", vcards_parsing },
	{ "VCard created from scratch", create_vcard_from_api },
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