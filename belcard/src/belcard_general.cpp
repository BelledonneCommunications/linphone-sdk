#include "belcard/belcard_general.hpp"

using namespace::std;
using namespace::belr;
using namespace::belcard;

shared_ptr<BelCardSource> BelCardSource::create() {
	return BelCardGeneric::create<BelCardSource>();
}

shared_ptr<BelCardSource> BelCardSource::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("SOURCE", input, NULL);
	return dynamic_pointer_cast<BelCardSource>(ret);
}

void BelCardSource::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("SOURCE", make_fn(&BelCardSource::create))
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
			->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
			->setCollector("MEDIATYPE-param", make_sfn(&BelCardProperty::setMediaTypeParam))
			->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
			->setCollector("SOURCE-value", make_sfn(&BelCardProperty::setValue));
}

BelCardSource::BelCardSource() : BelCardProperty() {
	setName("SOURCE");
}

shared_ptr<BelCardKind> BelCardKind::create() {
	return BelCardGeneric::create<BelCardKind>();
}

shared_ptr<BelCardKind> BelCardKind::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("KIND", input, NULL);
	return dynamic_pointer_cast<BelCardKind>(ret);
}

void BelCardKind::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("KIND", make_fn(&BelCardKind::create))
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("KIND-value", make_sfn(&BelCardProperty::setValue));
}

BelCardKind::BelCardKind() : BelCardProperty() {
	setName("KIND");
}

shared_ptr<BelCardXML> BelCardXML::create() {
	return BelCardGeneric::create<BelCardXML>();
}

shared_ptr<BelCardXML> BelCardXML::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("XML", input, NULL);
	return dynamic_pointer_cast<BelCardXML>(ret);
}

void BelCardXML::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("XML", make_fn(&BelCardXML::create))
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("XML-value", make_sfn(&BelCardProperty::setValue));
}

BelCardXML::BelCardXML() : BelCardProperty() {
	setName("XML");
}