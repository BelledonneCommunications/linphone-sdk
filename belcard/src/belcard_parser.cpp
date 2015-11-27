#include "belcard/belcard_parser.hpp"
#include "belcard/belcard.hpp"

#include <belr/parser-impl.cc>

using namespace::std;
using namespace::belr;
using namespace::belcard;

BelCardParser::BelCardParser() {
	_grammar = _grammar_builder.createFromAbnf("vcardgrammar.txt", make_shared<CoreRules>());
}

BelCardParser::~BelCardParser() {
	
}

shared_ptr<BelCard> BelCardParser::parse(const string &input) {
	string vcard = unfold(input);
	
	Parser<shared_ptr<BelCardGeneric>> parser(_grammar);
	parser.setHandler("vcard", make_fn(&BelCard::create))
		->setCollector("FN", make_sfn(&BelCard::setFN))
		->setCollector("N", make_sfn(&BelCard::setN))
		->setCollector("BDAY", make_sfn(&BelCard::setBirthday))
		->setCollector("ANNIVERSARY", make_sfn(&BelCard::setAnniversary))
		->setCollector("GENDER", make_sfn(&BelCard::setGender))
		->setCollector("NICKNAME", make_sfn(&BelCard::addNickname))
		->setCollector("PHOTO", make_sfn(&BelCard::addPhoto));
		
	parser.setHandler("any-param", make_fn(&BelCardParam::create))
		->setCollector("param-name", make_sfn(&BelCardParam::setName))
		->setCollector("param-value", make_sfn(&BelCardParam::setValue));
		
	parser.setHandler("FN", make_fn(&BelCardFN::create))
		->setCollector("group", make_sfn(&BelCardFN::setGroup))
		->setCollector("any-param", make_sfn(&BelCardFN::addParam))
		->setCollector("FN-value", make_sfn(&BelCardFN::setValue));
		
	parser.setHandler("N", make_fn(&BelCardN::create))
		->setCollector("group", make_sfn(&BelCardN::setGroup))
		->setCollector("any-param", make_sfn(&BelCardN::addParam))
		->setCollector("N-fn", make_sfn(&BelCardN::setFamilyName))
		->setCollector("N-gn", make_sfn(&BelCardN::setGivenName))
		->setCollector("N-an", make_sfn(&BelCardN::setAdditionalName))
		->setCollector("N-prefixes", make_sfn(&BelCardN::setPrefixes))
		->setCollector("N-suffixes", make_sfn(&BelCardN::setSuffixes));
		
	parser.setHandler("BDAY", make_fn(&BelCardBirthday::create))
		->setCollector("group", make_sfn(&BelCardBirthday::setGroup))
		->setCollector("any-param", make_sfn(&BelCardBirthday::addParam))
		->setCollector("BDAY-value", make_sfn(&BelCardBirthday::setValue));
		
	parser.setHandler("ANNIVERSARY", make_fn(&BelCardAnniversary::create))
		->setCollector("group", make_sfn(&BelCardAnniversary::setGroup))
		->setCollector("any-param", make_sfn(&BelCardAnniversary::addParam))
		->setCollector("ANNIVERSARY-value", make_sfn(&BelCardAnniversary::setValue));
		
	parser.setHandler("GENDER", make_fn(&BelCardGender::create))
		->setCollector("group", make_sfn(&BelCardGender::setGroup))
		->setCollector("any-param", make_sfn(&BelCardGender::addParam))
		->setCollector("GENDER-value", make_sfn(&BelCardGender::setValue));
		
	parser.setHandler("NICKNAME", make_fn(&BelCardNickname::create))
		->setCollector("group", make_sfn(&BelCardNickname::setGroup))
		->setCollector("any-param", make_sfn(&BelCardNickname::addParam))
		->setCollector("NICKNAME-value", make_sfn(&BelCardNickname::setValue));
		
	parser.setHandler("PHOTO", make_fn(&BelCardPhoto::create))
		->setCollector("group", make_sfn(&BelCardPhoto::setGroup))
		->setCollector("any-param", make_sfn(&BelCardPhoto::addParam))
		->setCollector("PHOTO-value", make_sfn(&BelCardPhoto::setValue));
		
	size_t parsedSize = 0;
	shared_ptr<BelCardGeneric> ret = parser.parseInput("vcard", vcard, &parsedSize);
	shared_ptr<BelCard> belCard = dynamic_pointer_cast<BelCard>(ret);
	return belCard;
}

string BelCardParser::dumpVCard(const shared_ptr<BelCard> &card) {
	string output = card->toString();
	return fold(output);
}

string BelCardParser::fold(string input) {
	size_t crlf = 0;
	size_t next_crlf = 0;
	
	while (next_crlf != string::npos) {
		next_crlf = input.find("\r\n", crlf);
		if (next_crlf != string::npos) {
			if (next_crlf - crlf > 75) {
				input.insert(crlf + 74, "\r\n ");
				crlf += 76;
			} else {
				crlf = next_crlf + 2;
			}
		}
	}
	
	return input;
}

string BelCardParser::unfold(string input) {
	size_t crlf = input.find("\r\n");
	
	while (crlf != string::npos) {
		if (isspace(input[crlf + 2])) {
			input.erase(crlf, 3);
		} else {
			crlf += 2;
		}
		
		crlf = input.find("\r\n", crlf);
	}
	
	return input;
}