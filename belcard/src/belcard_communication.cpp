#include "belcard/belcard_communication.hpp"

using namespace::std;
using namespace::belr;
using namespace::belcard;

shared_ptr<BelCardPhoneNumber> BelCardPhoneNumber::create() {
	return BelCardGeneric::create<BelCardPhoneNumber>();
}

shared_ptr<BelCardPhoneNumber> BelCardPhoneNumber::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("TEL", input, NULL);
	return dynamic_pointer_cast<BelCardPhoneNumber>(ret);
}

void BelCardPhoneNumber::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("TEL", make_fn(&BelCardPhoneNumber::create))
			->setCollector("group", make_sfn(&BelCardPhoneNumber::setGroup))
			->setCollector("any-param", make_sfn(&BelCardPhoneNumber::addParam))
			->setCollector("TEL-value", make_sfn(&BelCardPhoneNumber::setValue));
}

BelCardPhoneNumber::BelCardPhoneNumber() : BelCardProperty() {
	setName("TEL");
}

shared_ptr<BelCardEmail> BelCardEmail::create() {
	return BelCardGeneric::create<BelCardEmail>();
}

shared_ptr<BelCardEmail> BelCardEmail::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("EMAIL", input, NULL);
	return dynamic_pointer_cast<BelCardEmail>(ret);
}

void BelCardEmail::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("EMAIL", make_fn(&BelCardEmail::create))
			->setCollector("group", make_sfn(&BelCardEmail::setGroup))
			->setCollector("any-param", make_sfn(&BelCardEmail::addParam))
			->setCollector("EMAIL-value", make_sfn(&BelCardEmail::setValue));
}

BelCardEmail::BelCardEmail() : BelCardProperty() {
	setName("EMAIL");
}

shared_ptr<BelCardImpp> BelCardImpp::create() {
	return BelCardGeneric::create<BelCardImpp>();
}

shared_ptr<BelCardImpp> BelCardImpp::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("IMPP", input, NULL);
	return dynamic_pointer_cast<BelCardImpp>(ret);
}

void BelCardImpp::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("IMPP", make_fn(&BelCardImpp::create))
			->setCollector("group", make_sfn(&BelCardImpp::setGroup))
			->setCollector("any-param", make_sfn(&BelCardImpp::addParam))
			->setCollector("IMPP-value", make_sfn(&BelCardImpp::setValue));
}

BelCardImpp::BelCardImpp() : BelCardProperty() {
	setName("IMPP");
}

shared_ptr<BelCardLang> BelCardLang::create() {
	return BelCardGeneric::create<BelCardLang>();
}

shared_ptr<BelCardLang> BelCardLang::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("LANG", input, NULL);
	return dynamic_pointer_cast<BelCardLang>(ret);
}

void BelCardLang::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("LANG", make_fn(&BelCardLang::create))
			->setCollector("group", make_sfn(&BelCardLang::setGroup))
			->setCollector("any-param", make_sfn(&BelCardLang::addParam))
			->setCollector("LANG-value", make_sfn(&BelCardLang::setValue));
}

BelCardLang::BelCardLang() : BelCardProperty() {
	setName("LANG");
}