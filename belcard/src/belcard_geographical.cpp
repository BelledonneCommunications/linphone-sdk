#include "belcard/belcard_geographical.hpp"

using namespace::std;
using namespace::belr;
using namespace::belcard;

shared_ptr<BelCardTimezone> BelCardTimezone::create() {
	return make_shared<BelCardTimezone>();
}

shared_ptr<BelCardTimezone> BelCardTimezone::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("TZ", input, NULL);
	return dynamic_pointer_cast<BelCardTimezone>(ret);
}

void BelCardTimezone::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("TZ", make_fn(&BelCardTimezone::create))
			->setCollector("group", make_sfn(&BelCardTimezone::setGroup))
			->setCollector("any-param", make_sfn(&BelCardTimezone::addParam))
			->setCollector("TZ-value", make_sfn(&BelCardTimezone::setValue));
}

BelCardTimezone::BelCardTimezone() : BelCardProperty() {
	setName("TZ");
}

shared_ptr<BelCardGeo> BelCardGeo::create() {
	return make_shared<BelCardGeo>();
}

shared_ptr<BelCardGeo> BelCardGeo::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("GEO", input, NULL);
	return dynamic_pointer_cast<BelCardGeo>(ret);
}

void BelCardGeo::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("GEO", make_fn(&BelCardGeo::create))
			->setCollector("group", make_sfn(&BelCardGeo::setGroup))
			->setCollector("any-param", make_sfn(&BelCardGeo::addParam))
			->setCollector("GEO-value", make_sfn(&BelCardGeo::setValue));
}

BelCardGeo::BelCardGeo() : BelCardProperty() {
	setName("GEO");
}