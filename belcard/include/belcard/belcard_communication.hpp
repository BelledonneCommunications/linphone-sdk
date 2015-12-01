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
	
	class BelCardEmail : public BelCardProperty {
	public:
		static shared_ptr<BelCardEmail> create() {
			return make_shared<BelCardEmail>();
		}
		
		static shared_ptr<BelCardEmail> parse(const string& input) {
			ABNFGrammarBuilder grammar_builder;
			shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
			Parser<shared_ptr<BelCardGeneric>> parser(grammar);
			setHandlerAndCollectors(&parser);
			BelCardParam::setHandlerAndCollectors(&parser);
			shared_ptr<BelCardGeneric> ret = parser.parseInput("EMAIL", input, NULL);
			return dynamic_pointer_cast<BelCardEmail>(ret);
		}
		
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
			parser->setHandler("EMAIL", make_fn(&BelCardEmail::create))
					->setCollector("group", make_sfn(&BelCardEmail::setGroup))
					->setCollector("any-param", make_sfn(&BelCardEmail::addParam))
					->setCollector("EMAIL-value", make_sfn(&BelCardEmail::setValue));
		}
		
		BelCardEmail() : BelCardProperty() {
			setName("EMAIL");
		}
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardImpp : public BelCardProperty {
	public:
		static shared_ptr<BelCardImpp> create() {
			return make_shared<BelCardImpp>();
		}
		
		static shared_ptr<BelCardImpp> parse(const string& input) {
			ABNFGrammarBuilder grammar_builder;
			shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
			Parser<shared_ptr<BelCardGeneric>> parser(grammar);
			setHandlerAndCollectors(&parser);
			BelCardParam::setHandlerAndCollectors(&parser);
			shared_ptr<BelCardGeneric> ret = parser.parseInput("IMPP", input, NULL);
			return dynamic_pointer_cast<BelCardImpp>(ret);
		}
		
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
			parser->setHandler("IMPP", make_fn(&BelCardImpp::create))
					->setCollector("group", make_sfn(&BelCardImpp::setGroup))
					->setCollector("any-param", make_sfn(&BelCardImpp::addParam))
					->setCollector("IMPP-value", make_sfn(&BelCardImpp::setValue));
		}
		
		BelCardImpp() : BelCardProperty() {
			setName("IMPP");
		}
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardLang : public BelCardProperty {
	public:
		static shared_ptr<BelCardLang> create() {
			return make_shared<BelCardLang>();
		}
		
		static shared_ptr<BelCardLang> parse(const string& input) {
			ABNFGrammarBuilder grammar_builder;
			shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
			Parser<shared_ptr<BelCardGeneric>> parser(grammar);
			setHandlerAndCollectors(&parser);
			BelCardParam::setHandlerAndCollectors(&parser);
			shared_ptr<BelCardGeneric> ret = parser.parseInput("LANG", input, NULL);
			return dynamic_pointer_cast<BelCardLang>(ret);
		}
		
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
			parser->setHandler("LANG", make_fn(&BelCardLang::create))
					->setCollector("group", make_sfn(&BelCardLang::setGroup))
					->setCollector("any-param", make_sfn(&BelCardLang::addParam))
					->setCollector("LANG-value", make_sfn(&BelCardLang::setValue));
		}
		
		BelCardLang() : BelCardProperty() {
			setName("LANG");
		}
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
}

#endif