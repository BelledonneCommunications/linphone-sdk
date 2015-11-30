#ifndef belcard_general_hpp
#define belcard_general_hpp

#include "belcard_generic.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>
#include <list>
#include <map>
#include <memory>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardKind : public BelCardProperty {
	public:
		static shared_ptr<BelCardKind> create() {
			return make_shared<BelCardKind>();
		}
		
		static shared_ptr<BelCardKind> parse(const string& input) {
			ABNFGrammarBuilder grammar_builder;
			shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
			Parser<shared_ptr<BelCardGeneric>> parser(grammar);
			setHandlerAndCollectors(&parser);
			BelCardParam::setHandlerAndCollectors(&parser);
			shared_ptr<BelCardGeneric> ret = parser.parseInput("KIND", input, NULL);
			return dynamic_pointer_cast<BelCardKind>(ret);
		}
		
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
			parser->setHandler("KIND", make_fn(&BelCardKind::create))
					->setCollector("group", make_sfn(&BelCardKind::setGroup))
					->setCollector("any-param", make_sfn(&BelCardKind::addParam))
					->setCollector("KIND-value", make_sfn(&BelCardKind::setValue));
		}
		
		BelCardKind() : BelCardProperty() {
			setName("KIND");
		}
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
}

#endif