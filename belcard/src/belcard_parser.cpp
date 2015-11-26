#include "belcard/belcard_parser.hpp"
#include "belcard/belcard.hpp"

#include <belr/parser-impl.cc>

using namespace::std;
using namespace::belr;
using namespace::belcard;

BelCardParser::BelCardParser() {
	_grammar = _grammar_builder.createFromAbnf("vcardgrammar.txt", make_shared<CoreRules>());
}

shared_ptr<BelCard> BelCardParser::parse(const string &input) {
	Parser<shared_ptr<BelCardGeneric>> parser(_grammar);
	parser.setHandler("vcard", make_fn(&BelCard::create))
		->setCollector("FN", make_sfn(&BelCard::setFN))
		->setCollector("N", make_sfn(&BelCard::setN))
		->setCollector("NICKNAME", make_sfn(&BelCard::addNickname));
		
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
		
	parser.setHandler("NICKNAME", make_fn(&BelCardNickname::create))
		->setCollector("group", make_sfn(&BelCardNickname::setGroup))
		->setCollector("any-param", make_sfn(&BelCardNickname::addParam))
		->setCollector("NICKNAME-value", make_sfn(&BelCardNickname::setValue));
		
	size_t parsedSize = 0;
	shared_ptr<BelCardGeneric> ret = parser.parseInput("vcard", input, &parsedSize);
	shared_ptr<BelCard> belCard = dynamic_pointer_cast<BelCard>(ret);
	return belCard;
}