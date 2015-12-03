#include "belcard/belcard_params.hpp"

using namespace::std;
using namespace::belr;
using namespace::belcard;

shared_ptr<BelCardValueParam> BelCardValueParam::create() {
	return BelCardGeneric::create<BelCardValueParam>();
}

shared_ptr<BelCardValueParam> BelCardValueParam::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("VALUE-param", input, NULL);
	return dynamic_pointer_cast<BelCardValueParam>(ret);
}

void BelCardValueParam::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("VALUE-param", make_fn(&BelCardValueParam::create))
			->setCollector("VALUE-param", make_sfn(&BelCardValueParam::setValue));
}

BelCardValueParam::BelCardValueParam() : BelCardParam() {
	setName("VALUE");
}