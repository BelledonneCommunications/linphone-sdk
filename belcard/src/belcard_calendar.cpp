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
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
			->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
			->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
			->setCollector("MEDIATYPE-param", make_sfn(&BelCardProperty::setMediaTypeParam))
			->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
			->setCollector("FBURL-value", make_sfn(&BelCardProperty::setValue));
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
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
			->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
			->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
			->setCollector("MEDIATYPE-param", make_sfn(&BelCardProperty::setMediaTypeParam))
			->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
			->setCollector("CALADRURI-value", make_sfn(&BelCardProperty::setValue));
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
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
			->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
			->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
			->setCollector("MEDIATYPE-param", make_sfn(&BelCardProperty::setMediaTypeParam))
			->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
			->setCollector("CALURI-value", make_sfn(&BelCardProperty::setValue));
}

BelCardCALURI::BelCardCALURI() : BelCardProperty() {
	setName("CALURI");
}