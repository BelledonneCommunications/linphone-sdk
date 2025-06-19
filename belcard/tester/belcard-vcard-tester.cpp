/*
    belcard-vcard-tester.cpp
    Copyright (C) 2015  Belledonne Communications SARL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "belcard-tester.hpp"
#include "belcard/belcard.hpp"
#include "belcard/belcard_parser.hpp"
#include "belcard/belcard_utils.hpp"
#include <belr/belr.h>

#include <bctoolbox/logging.h>
#include <bctoolbox/tester.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace ::std;
using namespace ::belr;
using namespace ::belcard;

static string openFile(const char *name) {
	char *res = bc_tester_res(name);

	ifstream istr(res, std::ios::binary);
	if (!istr.is_open()) {
		BC_FAIL("File couldn't be opened!");
	}

	bctbx_free(res);

	stringstream vcardStream;
	vcardStream << istr.rdbuf();
	string vcard = vcardStream.str();
	return vcard;
}

static void folding(void) {
	string vcard = openFile("vcards/foldtest.vcf");
	string folded_vcard = belcard_fold(vcard);
	string unfolded_vcard = openFile("vcards/unfoldtest.vcf");
	BC_ASSERT_EQUAL(unfolded_vcard.compare(folded_vcard), 0, int, "%d");
}

static void unfolding(void) {
	string vcard = openFile("vcards/unfoldtest.vcf");
	string unfolded_vcard = belcard_unfold(vcard);
	string folded_vcard = openFile("vcards/foldtest.vcf");
	BC_ASSERT_EQUAL(folded_vcard.compare(unfolded_vcard), 0, int, "%d");
}

static void vcard_parsing(void) {
	string vcard = openFile("vcards/vcard.vcf");

	BelCardParser *parser = new BelCardParser();
	BC_ASSERT_FALSE(parser->isUsingV3Grammar());
	shared_ptr<BelCard> belCard = parser->parseOne(vcard);
	if (!BC_ASSERT_TRUE(belCard != NULL)) {
		delete (parser);
		return;
	}
	BC_ASSERT_TRUE(belCard->assertRFCCompliance());

	string vcard2 = belCard->toFoldedString();
	BC_ASSERT_EQUAL(vcard2.compare(vcard), 0, int, "%d");
	delete (parser);
}

static void vcards_parsing(void) {
	string vcards = openFile("vcards/vcards.vcf");

	BelCardParser *parser = new BelCardParser();
	BC_ASSERT_FALSE(parser->isUsingV3Grammar());
	shared_ptr<BelCardList> belCards = parser->parse(vcards);
	if (!BC_ASSERT_TRUE(belCards != NULL)) {
		delete (parser);
		return;
	}
	BC_ASSERT_EQUAL((unsigned int)belCards->getCards().size(), 2, unsigned int, "%u");

	string vcards2 = belCards->toString();
	BC_ASSERT_EQUAL(vcards2.compare(vcards), 0, int, "%d");
	delete (parser);
}

static void create_vcard_from_api(void) {
	shared_ptr<BelCard> belCard = BelCard::create<BelCard>();
	if (!BC_ASSERT_TRUE(belCard != NULL)) return;
	BC_ASSERT_FALSE(belCard->assertRFCCompliance());

	shared_ptr<BelCardFullName> fn = BelCard::create<BelCardFullName>();
	fn->setValue("Sylvain Berfini");
	belCard->setFullName(fn);
	BC_ASSERT_TRUE(belCard->assertRFCCompliance());
	BC_ASSERT(belCard->getFullName()->toString() == fn->toString());

	fn = BelCard::create<BelCardFullName>();
	fn->setValue("Belcard Tester");
	belCard->setFullName(fn);
	BC_ASSERT(belCard->getFullName()->toString() == fn->toString());

	string vcard = belCard->toString();
	BelCardParser *parser = new BelCardParser();
	BC_ASSERT_FALSE(parser->isUsingV3Grammar());
	shared_ptr<BelCard> belCard2 = parser->parseOne(vcard);
	if (!BC_ASSERT_TRUE(belCard2 != NULL)) {
		delete (parser);
		return;
	}
	BC_ASSERT_TRUE(belCard2->assertRFCCompliance());
	string vcard2 = belCard2->toString();
	BC_ASSERT_EQUAL(vcard.compare(vcard2), 0, unsigned, "%d");
	delete (parser);
}

static void property_sort_using_pref_param(void) {
	shared_ptr<BelCard> belCard = BelCard::create<BelCard>();
	BC_ASSERT_TRUE(belCard != NULL);

	shared_ptr<BelCardImpp> impp1 = BelCardImpp::parse("IMPP;TYPE=home;PREF=2:sip:viish@sip.linphone.org\r\n", true);
	BC_ASSERT_TRUE(impp1 != NULL);

	shared_ptr<BelCardImpp> impp2 = BelCardImpp::parse("IMPP;PREF=1;TYPE=work:sip:sylvain@sip.linphone.org\r\n", true);
	BC_ASSERT_TRUE(impp2 != NULL);

	belCard->addImpp(impp1);
	belCard->addImpp(impp2);

	const list<shared_ptr<BelCardImpp>> imppList = belCard->getImpp();
	BC_ASSERT_EQUAL((unsigned int)imppList.size(), 2, unsigned int, "%u");
	BC_ASSERT_TRUE(imppList.front() == impp2);
	BC_ASSERT_TRUE(imppList.back() == impp1);

	const list<shared_ptr<BelCardProperty>> propertiesList = belCard->getProperties();
	BC_ASSERT_EQUAL((unsigned int)propertiesList.size(), 2, unsigned int, "%u");

	belCard->removeImpp(impp1);
	BC_ASSERT_EQUAL((unsigned int)belCard->getImpp().size(), 1, unsigned int, "%u");
	BC_ASSERT_EQUAL((unsigned int)belCard->getProperties().size(), 1, unsigned int, "%u");
}

static void vcard3_parsing(void) {
	string vcard = openFile("vcards3/franck.vcard");

	BelCardParser *parser = new BelCardParser(true);
	BC_ASSERT_TRUE(parser->isUsingV3Grammar());
	shared_ptr<BelCard> belCard = parser->parseOne(vcard);
	if (!BC_ASSERT_TRUE(belCard != NULL)) {
		delete (parser);
		return;
	}
	BC_ASSERT_TRUE(belCard->assertRFCCompliance());

	string vcard2 = belCard->toFoldedString();
	BC_ASSERT_EQUAL(vcard2.compare(vcard), 0, int, "%d");
	delete (parser);
}

static void vcards3_parsing(void) {
	string vcards = openFile("vcards3/list.vcard");

	BelCardParser *parser = new BelCardParser(true);
	BC_ASSERT_TRUE(parser->isUsingV3Grammar());
	shared_ptr<BelCardList> belCards = parser->parse(vcards);
	if (!BC_ASSERT_TRUE(belCards != NULL)) {
		delete (parser);
		return;
	}
	BC_ASSERT_EQUAL((unsigned int)belCards->getCards().size(), 2, unsigned int, "%u");

	string vcards2 = belCards->toString();
	BC_ASSERT_EQUAL(vcards2.compare(vcards), 0, int, "%d");
	delete (parser);
}

static void create_vcard3_from_api(void) {
	shared_ptr<BelCard> belCard = BelCard::createV3<BelCard>();
	if (!BC_ASSERT_TRUE(belCard != NULL)) return;
	BC_ASSERT_FALSE(belCard->assertRFCCompliance());

	shared_ptr<BelCardFullName> fn = BelCard::createV3<BelCardFullName>();
	fn->setValue("Sylvain Berfini");
	belCard->setFullName(fn);
	BC_ASSERT_FALSE(belCard->assertRFCCompliance()); // For vCard 3 N property is also mandatory according to RFC 2426

	shared_ptr<BelCardName> n = BelCard::createV3<BelCardName>();
	n->setValue("Sylvain Berfini");
	belCard->setName(n);
	BC_ASSERT_TRUE(belCard->assertRFCCompliance());
	BC_ASSERT(belCard->getFullName()->toString() == fn->toString());

	fn = BelCard::createV3<BelCardFullName>();
	fn->setValue("Belcard Tester");
	belCard->setFullName(fn);
	BC_ASSERT(belCard->getFullName()->toString() == fn->toString());
	string vcard = belCard->toString();

	BelCardParser *parser = new BelCardParser(true);
	BC_ASSERT_TRUE(parser->isUsingV3Grammar());
	shared_ptr<BelCard> belCard2 = parser->parseOne(vcard);
	if (!BC_ASSERT_TRUE(belCard2 != NULL)) {
		delete (parser);
		return;
	}

	BC_ASSERT_TRUE(belCard2->assertRFCCompliance());
	string vcard2 = belCard2->toString();
	BC_ASSERT_EQUAL(vcard.compare(vcard2), 0, unsigned, "%d");
	delete (parser);
}

static void vcard3_parsing_of_vcard4_file(void) {
	string vcard = openFile("vcards/vcard.vcf");

	BelCardParser *parser = new BelCardParser(true);
	BC_ASSERT_TRUE(parser->isUsingV3Grammar());
	shared_ptr<BelCard> belCard = parser->parseOne(vcard);
	BC_ASSERT_PTR_NULL(belCard);
	delete (parser);
}

static void vcard4_parsing_of_vcard3_file(void) {
	string vcard = openFile("vcards3/franck.vcard");

	BelCardParser *parser = new BelCardParser();
	BC_ASSERT_FALSE(parser->isUsingV3Grammar());
	shared_ptr<BelCard> belCard = parser->parseOne(vcard);
	BC_ASSERT_PTR_NULL(belCard);
	delete (parser);
}

static test_t tests[] = {
    TEST_NO_TAG("Folding", folding),
    TEST_NO_TAG("Unfolding", unfolding),
    TEST_NO_TAG("VCard parsing", vcard_parsing),
    TEST_NO_TAG("VCards parsing", vcards_parsing),
    TEST_NO_TAG("VCard created from scratch", create_vcard_from_api),
    TEST_NO_TAG("Property sort using pref param", property_sort_using_pref_param),
    TEST_NO_TAG("VCard 3.0 parsing", vcard3_parsing),
    TEST_NO_TAG("VCards 3.0 parsing", vcards3_parsing),
    TEST_NO_TAG("VCard 3.0 created from scratch", create_vcard3_from_api),
    TEST_NO_TAG("Parse vCard 4.0 file with 3.0 grammar", vcard3_parsing_of_vcard4_file),
    TEST_NO_TAG("Parse vCard 3.0 file with 4.0 grammar", vcard4_parsing_of_vcard3_file),
};

test_suite_t vcard_test_suite = {"VCard", NULL, NULL, NULL, NULL, sizeof(tests) / sizeof(tests[0]), tests, 0, 0};
