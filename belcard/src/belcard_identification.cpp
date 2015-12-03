#include "belcard/belcard_identification.hpp"

using namespace::std;
using namespace::belr;
using namespace::belcard;

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
	parser->setHandler("FN", make_fn(BelCardGeneric::create<BelCardFullName>))
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
			->setCollector("LANGUAGE-param", make_sfn(&BelCardProperty::setLanguageParam))
			->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
			->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
			->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
			->setCollector("FN-value", make_sfn(&BelCardProperty::setValue));
}

BelCardFullName::BelCardFullName() : BelCardProperty() {
	setName("FN");
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
	parser->setHandler("N", make_fn(BelCardGeneric::create<BelCardName>))
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("SORT-AS-param", make_sfn(&BelCardProperty::setSortAsParam))
			->setCollector("LANGUAGE-param", make_sfn(&BelCardProperty::setLanguageParam))
			->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
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
	parser->setHandler("NICKNAME", make_fn(BelCardGeneric::create<BelCardNickname>))
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
			->setCollector("LANGUAGE-param", make_sfn(&BelCardProperty::setLanguageParam))
			->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
			->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
			->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
			->setCollector("NICKNAME-value", make_sfn(&BelCardProperty::setValue));
}

BelCardNickname::BelCardNickname() : BelCardProperty() {
	setName("NICKNAME");
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
	parser->setHandler("BDAY", make_fn(BelCardGeneric::create<BelCardBirthday>))
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("LANGUAGE-param", make_sfn(&BelCardProperty::setLanguageParam))
			->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
			->setCollector("CALSCALE-param", make_sfn(&BelCardProperty::setCALSCALEParam))
			->setCollector("BDAY-value", make_sfn(&BelCardProperty::setValue));
}

BelCardBirthday::BelCardBirthday() : BelCardProperty() {
	setName("BDAY");
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
	parser->setHandler("ANNIVERSARY", make_fn(BelCardGeneric::create<BelCardAnniversary>))
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
			->setCollector("CALSCALE-param", make_sfn(&BelCardProperty::setCALSCALEParam))
			->setCollector("ANNIVERSARY-value", make_sfn(&BelCardProperty::setValue));
}

BelCardAnniversary::BelCardAnniversary() : BelCardProperty() {
	setName("ANNIVERSARY");
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
	parser->setHandler("GENDER", make_fn(BelCardGeneric::create<BelCardGender>))
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("GENDER-value", make_sfn(&BelCardProperty::setValue));
}

BelCardGender::BelCardGender() : BelCardProperty() {
	setName("GENDER");
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
	parser->setHandler("PHOTO", make_fn(BelCardGeneric::create<BelCardPhoto>))
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
			->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
			->setCollector("MEDIATYPE-param", make_sfn(&BelCardProperty::setMediaTypeParam))
			->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
			->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
			->setCollector("PHOTO-value", make_sfn(&BelCardProperty::setValue));
}

BelCardPhoto::BelCardPhoto() : BelCardProperty() {
	setName("PHOTO");
}