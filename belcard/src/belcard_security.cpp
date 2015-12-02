#include "belcard/belcard_security.hpp"

using namespace::std;
using namespace::belr;
using namespace::belcard;

shared_ptr<BelCardKey> BelCardKey::create() {
	return make_shared<BelCardKey>();
}

shared_ptr<BelCardKey> BelCardKey::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("KEY", input, NULL);
	return dynamic_pointer_cast<BelCardKey>(ret);
}

void BelCardKey::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("KEY", make_fn(&BelCardKey::create))
			->setCollector("group", make_sfn(&BelCardKey::setGroup))
			->setCollector("any-param", make_sfn(&BelCardKey::addParam))
			->setCollector("KEY-value", make_sfn(&BelCardKey::setValue));
}

BelCardKey::BelCardKey() : BelCardProperty() {
	setName("KEY");
}