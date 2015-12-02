#ifndef belcard_general_hpp
#define belcard_general_hpp

#include "belcard_generic.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>

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
	
	class BelCardSource : public BelCardProperty {
	public:
		static shared_ptr<BelCardSource> create() {
			return make_shared<BelCardSource>();
		}
		
		static shared_ptr<BelCardSource> parse(const string& input) {
			ABNFGrammarBuilder grammar_builder;
			shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
			Parser<shared_ptr<BelCardGeneric>> parser(grammar);
			setHandlerAndCollectors(&parser);
			BelCardParam::setHandlerAndCollectors(&parser);
			shared_ptr<BelCardGeneric> ret = parser.parseInput("SOURCE", input, NULL);
			return dynamic_pointer_cast<BelCardSource>(ret);
		}
		
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
			parser->setHandler("SOURCE", make_fn(&BelCardSource::create))
					->setCollector("group", make_sfn(&BelCardSource::setGroup))
					->setCollector("any-param", make_sfn(&BelCardSource::addParam))
					->setCollector("SOURCE-value", make_sfn(&BelCardSource::setValue));
		}
		
		BelCardSource() : BelCardProperty() {
			setName("SOURCE");
		}
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardXML : public BelCardProperty {
	public:
		static shared_ptr<BelCardXML> create() {
			return make_shared<BelCardXML>();
		}
		
		static shared_ptr<BelCardXML> parse(const string& input) {
			ABNFGrammarBuilder grammar_builder;
			shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
			Parser<shared_ptr<BelCardGeneric>> parser(grammar);
			setHandlerAndCollectors(&parser);
			BelCardParam::setHandlerAndCollectors(&parser);
			shared_ptr<BelCardGeneric> ret = parser.parseInput("XML", input, NULL);
			return dynamic_pointer_cast<BelCardXML>(ret);
		}
		
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
			parser->setHandler("XML", make_fn(&BelCardXML::create))
					->setCollector("group", make_sfn(&BelCardXML::setGroup))
					->setCollector("any-param", make_sfn(&BelCardXML::addParam))
					->setCollector("XML-value", make_sfn(&BelCardXML::setValue));
		}
		
		BelCardXML() : BelCardProperty() {
			setName("XML");
		}
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
}

#endif