#include "belcard/belcard_identification.hpp"

using namespace::std;
using namespace::belr;
using namespace::belcard;

shared_ptr<BelCardFullName> BelCardFullName::create() {
	return BelCardGeneric::create<BelCardFullName>();
}

shared_ptr<BelCardFullName> BelCardFullName::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("FN", input, NULL);
	return dynamic_pointer_cast<BelCardFullName>(ret);
}

void BelCardFullName::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("FN", make_fn(&BelCardFullName::create))
			->setCollector("group", make_sfn(&BelCardFullName::setGroup))
			->setCollector("any-param", make_sfn(&BelCardFullName::addParam))
			->setCollector("FN-value", make_sfn(&BelCardFullName::setValue));
}

BelCardFullName::BelCardFullName() : BelCardProperty() {
	setName("FN");
}

shared_ptr<BelCardName> BelCardName::create() {
	return BelCardGeneric::create<BelCardName>();
}

shared_ptr<BelCardName> BelCardName::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("N", input, NULL);
	return dynamic_pointer_cast<BelCardName>(ret);
}

void BelCardName::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("N", make_fn(&BelCardName::create))
			->setCollector("group", make_sfn(&BelCardName::setGroup))
			->setCollector("any-param", make_sfn(&BelCardName::addParam))
			->setCollector("N-fn", make_sfn(&BelCardName::setFamilyName))
			->setCollector("N-gn", make_sfn(&BelCardName::setGivenName))
			->setCollector("N-an", make_sfn(&BelCardName::setAdditionalName))
			->setCollector("N-prefixes", make_sfn(&BelCardName::setPrefixes))
			->setCollector("N-suffixes", make_sfn(&BelCardName::setSuffixes));
}

BelCardName::BelCardName() : BelCardProperty() {
	setName("N");
}

void BelCardName::setFamilyName(const string &value) {
	_family_name = value;
}
const string &BelCardName::getFamilyName() const {
	return _family_name;
}

void BelCardName::setGivenName(const string &value) {
	_given_name = value;
}
const string &BelCardName::getGivenName() const {
	return _given_name;
}

void BelCardName::setAdditionalName(const string &value) {
	_additional_name = value;
}
const string &BelCardName::getAdditionalName() const {
	return _additional_name;
}

void BelCardName::setPrefixes(const string &value) {
	_prefixes = value;
}
const string &BelCardName::getPrefixes() const {
	return _prefixes;
}

void BelCardName::setSuffixes(const string &value) {
	_suffixes = value;
}
const string &BelCardName::getSuffixes() const {
	return _suffixes;
}

string BelCardName::serialize() const {
	stringstream output;
	
	if (getGroup().length() > 0) {
		output << getGroup() << ".";
	}
	
	output << getName();
	for (auto it = getParams().begin(); it != getParams().end(); ++it) {
		output << ";" << (**it); 
	}
	output << ":" << getFamilyName() + ";" + getGivenName() + ";" + getAdditionalName() + ";" + getPrefixes() + ";" + getSuffixes() << "\r\n";
	
	return output.str();            
}

shared_ptr<BelCardNickname> BelCardNickname::create() {
	return BelCardGeneric::create<BelCardNickname>();
}

shared_ptr<BelCardNickname> BelCardNickname::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("NICKNAME", input, NULL);
	return dynamic_pointer_cast<BelCardNickname>(ret);
}

void BelCardNickname::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("NICKNAME", make_fn(&BelCardNickname::create))
			->setCollector("group", make_sfn(&BelCardNickname::setGroup))
			->setCollector("any-param", make_sfn(&BelCardNickname::addParam))
			->setCollector("NICKNAME-value", make_sfn(&BelCardNickname::setValue));
}

BelCardNickname::BelCardNickname() : BelCardProperty() {
	setName("NICKNAME");
}

shared_ptr<BelCardBirthday> BelCardBirthday::create() {
	return BelCardGeneric::create<BelCardBirthday>();
}

shared_ptr<BelCardBirthday> BelCardBirthday::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("BDAY", input, NULL);
	return dynamic_pointer_cast<BelCardBirthday>(ret);
}

void BelCardBirthday::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("BDAY", make_fn(&BelCardBirthday::create))
			->setCollector("group", make_sfn(&BelCardBirthday::setGroup))
			->setCollector("any-param", make_sfn(&BelCardBirthday::addParam))
			->setCollector("BDAY-value", make_sfn(&BelCardBirthday::setValue));
}

BelCardBirthday::BelCardBirthday() : BelCardProperty() {
	setName("BDAY");
}

shared_ptr<BelCardAnniversary> BelCardAnniversary::create() {
	return BelCardGeneric::create<BelCardAnniversary>();
}

shared_ptr<BelCardAnniversary> BelCardAnniversary::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("ANNIVERSARY", input, NULL);
	return dynamic_pointer_cast<BelCardAnniversary>(ret);
}

void BelCardAnniversary::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("ANNIVERSARY", make_fn(&BelCardAnniversary::create))
			->setCollector("group", make_sfn(&BelCardAnniversary::setGroup))
			->setCollector("any-param", make_sfn(&BelCardAnniversary::addParam))
			->setCollector("ANNIVERSARY-value", make_sfn(&BelCardAnniversary::setValue));
}

BelCardAnniversary::BelCardAnniversary() : BelCardProperty() {
	setName("ANNIVERSARY");
}

shared_ptr<BelCardGender> BelCardGender::create() {
	return BelCardGeneric::create<BelCardGender>();
}

shared_ptr<BelCardGender> BelCardGender::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("GENDER", input, NULL);
	return dynamic_pointer_cast<BelCardGender>(ret);
}

void BelCardGender::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("GENDER", make_fn(&BelCardGender::create))
			->setCollector("group", make_sfn(&BelCardGender::setGroup))
			->setCollector("any-param", make_sfn(&BelCardGender::addParam))
			->setCollector("GENDER-value", make_sfn(&BelCardGender::setValue));
}

BelCardGender::BelCardGender() : BelCardProperty() {
	setName("GENDER");
}

shared_ptr<BelCardPhoto> BelCardPhoto::create() {
	return BelCardGeneric::create<BelCardPhoto>();
}

shared_ptr<BelCardPhoto> BelCardPhoto::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("PHOTO", input, NULL);
	return dynamic_pointer_cast<BelCardPhoto>(ret);
}

void BelCardPhoto::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("PHOTO", make_fn(&BelCardPhoto::create))
			->setCollector("group", make_sfn(&BelCardPhoto::setGroup))
			->setCollector("any-param", make_sfn(&BelCardPhoto::addParam))
			->setCollector("PHOTO-value", make_sfn(&BelCardPhoto::setValue));
}

BelCardPhoto::BelCardPhoto() : BelCardProperty() {
	setName("PHOTO");
}