#include "belcard/belcard_calendar.hpp"

using namespace::std;
using namespace::belr;
using namespace::belcard;

shared_ptr<BelCardFBURL> BelCardFBURL::create() {
	return BelCardGeneric::create<BelCardFBURL>();
}

shared_ptr<BelCardFBURL> BelCardFBURL::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("FBURL", input, NULL);
	return dynamic_pointer_cast<BelCardFBURL>(ret);
}

void BelCardFBURL::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("FBURL", make_fn(&BelCardFBURL::create))
			->setCollector("group", make_sfn(&BelCardFBURL::setGroup))
			->setCollector("any-param", make_sfn(&BelCardFBURL::addParam))
			->setCollector("FBURL-value", make_sfn(&BelCardFBURL::setValue));
}

BelCardFBURL::BelCardFBURL() : BelCardProperty() {
	setName("FBURL");
}

shared_ptr<BelCardCALADRURI> BelCardCALADRURI::create() {
	return BelCardGeneric::create<BelCardCALADRURI>();
}

shared_ptr<BelCardCALADRURI> BelCardCALADRURI::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("CALADRURI", input, NULL);
	return dynamic_pointer_cast<BelCardCALADRURI>(ret);
}

void BelCardCALADRURI::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("CALADRURI", make_fn(&BelCardCALADRURI::create))
			->setCollector("group", make_sfn(&BelCardCALADRURI::setGroup))
			->setCollector("any-param", make_sfn(&BelCardCALADRURI::addParam))
			->setCollector("CALADRURI-value", make_sfn(&BelCardCALADRURI::setValue));
}

BelCardCALADRURI::BelCardCALADRURI() : BelCardProperty() {
	setName("CALADRURI");
}

shared_ptr<BelCardCALURI> BelCardCALURI::create() {
	return BelCardGeneric::create<BelCardCALURI>();
}

shared_ptr<BelCardCALURI> BelCardCALURI::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("CALURI", input, NULL);
	return dynamic_pointer_cast<BelCardCALURI>(ret);
}

void BelCardCALURI::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("CALURI", make_fn(&BelCardCALURI::create))
			->setCollector("group", make_sfn(&BelCardCALURI::setGroup))
			->setCollector("any-param", make_sfn(&BelCardCALURI::addParam))
			->setCollector("CALURI-value", make_sfn(&BelCardCALURI::setValue));
}

BelCardCALURI::BelCardCALURI() : BelCardProperty() {
	setName("CALURI");
}