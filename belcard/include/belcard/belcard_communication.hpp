#ifndef belcard_communication_hpp
#define belcard_communication_hpp

#include "belcard_generic.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>
#include <sstream>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardTel : public BelCardProperty {
	public:
		static shared_ptr<BelCardTel> create() {
			return make_shared<BelCardTel>();
		}
		
		static shared_ptr<BelCardTel> parse(const string& input) {
			ABNFGrammarBuilder grammar_builder;
			shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
			Parser<shared_ptr<BelCardGeneric>> parser(grammar);
			setHandlerAndCollectors(&parser);
			BelCardParam::setHandlerAndCollectors(&parser);
			shared_ptr<BelCardGeneric> ret = parser.parseInput("TEL", input, NULL);
			return dynamic_pointer_cast<BelCardTel>(ret);
		}
		
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
			parser->setHandler("TEL", make_fn(&BelCardTel::create))
					->setCollector("group", make_sfn(&BelCardTel::setGroup))
					->setCollector("any-param", make_sfn(&BelCardTel::addParam))
					->setCollector("TEL-value", make_sfn(&BelCardTel::setValue));
		}
		
		BelCardTel() : BelCardProperty() {
			setName("TEL");
		}
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
}

#endif