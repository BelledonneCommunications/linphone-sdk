#include "belcard/belcard_general.hpp"

using namespace::std;
using namespace::belr;
using namespace::belcard;

shared_ptr<BelCardKind> BelCardKind::create() {
	return make_shared<BelCardKind>();
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
			->setCollector("group", make_sfn(&BelCardKind::setGroup))
			->setCollector("any-param", make_sfn(&BelCardKind::addParam))
			->setCollector("KIND-value", make_sfn(&BelCardKind::setValue));
}

BelCardKind::BelCardKind() : BelCardProperty() {
	setName("KIND");
}

shared_ptr<BelCardSource> BelCardSource::create() {
	return make_shared<BelCardSource>();
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
			->setCollector("group", make_sfn(&BelCardSource::setGroup))
			->setCollector("any-param", make_sfn(&BelCardSource::addParam))
			->setCollector("SOURCE-value", make_sfn(&BelCardSource::setValue));
}

BelCardSource::BelCardSource() : BelCardProperty() {
	setName("SOURCE");
}

shared_ptr<BelCardXML> BelCardXML::create() {
	return make_shared<BelCardXML>();
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
			->setCollector("group", make_sfn(&BelCardXML::setGroup))
			->setCollector("any-param", make_sfn(&BelCardXML::addParam))
			->setCollector("XML-value", make_sfn(&BelCardXML::setValue));
}

BelCardXML::BelCardXML() : BelCardProperty() {
	setName("XML");
}