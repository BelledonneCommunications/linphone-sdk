#ifndef belcard_identification_hpp
#define belcard_identification_hpp

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
	class BelCardFN : public BelCardProperty {
	public:
		static shared_ptr<BelCardFN> create() {
			return make_shared<BelCardFN>();
		}
		
		static shared_ptr<BelCardFN> parse(const string& input) {
			ABNFGrammarBuilder grammar_builder;
			shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
			Parser<shared_ptr<BelCardGeneric>> parser(grammar);
			setHandlerAndCollectors(&parser);
			BelCardParam::setHandlerAndCollectors(&parser);
			shared_ptr<BelCardGeneric> ret = parser.parseInput("FN", input, NULL);
			return dynamic_pointer_cast<BelCardFN>(ret);
		}
		
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
			parser->setHandler("FN", make_fn(&BelCardFN::create))
					->setCollector("group", make_sfn(&BelCardFN::setGroup))
					->setCollector("any-param", make_sfn(&BelCardFN::addParam))
					->setCollector("FN-value", make_sfn(&BelCardFN::setValue));
		}
		
		BelCardFN() : BelCardProperty() {
			setName("FN");
		}
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardN : public BelCardProperty {
	private:
		string _family_name;
		string _given_name;
		string _additional_name;
		string _prefixes;
		string _suffixes;
	public:
		static shared_ptr<BelCardN> create() {
			return make_shared<BelCardN>();
		}
		
		static shared_ptr<BelCardN> parse(const string& input) {
			ABNFGrammarBuilder grammar_builder;
			shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
			Parser<shared_ptr<BelCardGeneric>> parser(grammar);
			setHandlerAndCollectors(&parser);
			BelCardParam::setHandlerAndCollectors(&parser);
			shared_ptr<BelCardGeneric> ret = parser.parseInput("N", input, NULL);
			return dynamic_pointer_cast<BelCardN>(ret);
		}
		
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
			parser->setHandler("N", make_fn(&BelCardN::create))
					->setCollector("group", make_sfn(&BelCardN::setGroup))
					->setCollector("any-param", make_sfn(&BelCardN::addParam))
					->setCollector("N-fn", make_sfn(&BelCardN::setFamilyName))
					->setCollector("N-gn", make_sfn(&BelCardN::setGivenName))
					->setCollector("N-an", make_sfn(&BelCardN::setAdditionalName))
					->setCollector("N-prefixes", make_sfn(&BelCardN::setPrefixes))
					->setCollector("N-suffixes", make_sfn(&BelCardN::setSuffixes));
		}
		
		BelCardN() : BelCardProperty() {
			setName("N");
		}
		
		virtual void setFamilyName(const string &value) {
			_family_name = value;
		}
		virtual const string &getFamilyName() const {
			return _family_name;
		}
		
		virtual void setGivenName(const string &value) {
			_given_name = value;
		}
		virtual const string &getGivenName() const {
			return _given_name;
		}
		
		virtual void setAdditionalName(const string &value) {
			_additional_name = value;
		}
		virtual const string &getAdditionalName() const {
			return _additional_name;
		}
		
		virtual void setPrefixes(const string &value) {
			_prefixes = value;
		}
		virtual const string &getPrefixes() const {
			return _prefixes;
		}
		
		virtual void setSuffixes(const string &value) {
			_suffixes = value;
		}
		virtual const string &getSuffixes() const {
			return _suffixes;
		}
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
		
		friend ostream &operator<<(ostream &output, const BelCardN &n) {
			if (n.getGroup().length() > 0) {
				output << n.getGroup() << ".";
			}
			
			output << n.getName();
			for (auto it = n.getParams().begin(); it != n.getParams().end(); ++it) {
				output << ";" << (*it); 
			}
			output << ":" << n.getFamilyName() + ";" + n.getGivenName() + ";" + n.getAdditionalName() + ";" + n.getPrefixes() + ";" + n.getSuffixes() << "\r\n";
			return output;            
		}
	};
	
	class BelCardNickname : public BelCardProperty {
	public:
		static shared_ptr<BelCardNickname> create() {
			return make_shared<BelCardNickname>();
		}
		
		static shared_ptr<BelCardNickname> parse(const string& input) {
			ABNFGrammarBuilder grammar_builder;
			shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
			Parser<shared_ptr<BelCardGeneric>> parser(grammar);
			setHandlerAndCollectors(&parser);
			BelCardParam::setHandlerAndCollectors(&parser);
			shared_ptr<BelCardGeneric> ret = parser.parseInput("NICKNAME", input, NULL);
			return dynamic_pointer_cast<BelCardNickname>(ret);
		}
		
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
			parser->setHandler("NICKNAME", make_fn(&BelCardNickname::create))
					->setCollector("group", make_sfn(&BelCardNickname::setGroup))
					->setCollector("any-param", make_sfn(&BelCardNickname::addParam))
					->setCollector("NICKNAME-value", make_sfn(&BelCardNickname::setValue));
		}
		
		BelCardNickname() : BelCardProperty() {
			setName("NICKNAME");
		}
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardBirthday : public BelCardProperty {
	public:
		static shared_ptr<BelCardBirthday> create() {
			return make_shared<BelCardBirthday>();
		}
		
		static shared_ptr<BelCardBirthday> parse(const string& input) {
			ABNFGrammarBuilder grammar_builder;
			shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
			Parser<shared_ptr<BelCardGeneric>> parser(grammar);
			setHandlerAndCollectors(&parser);
			BelCardParam::setHandlerAndCollectors(&parser);
			shared_ptr<BelCardGeneric> ret = parser.parseInput("BDAY", input, NULL);
			return dynamic_pointer_cast<BelCardBirthday>(ret);
		}
		
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
			parser->setHandler("BDAY", make_fn(&BelCardBirthday::create))
					->setCollector("group", make_sfn(&BelCardBirthday::setGroup))
					->setCollector("any-param", make_sfn(&BelCardBirthday::addParam))
					->setCollector("BDAY-value", make_sfn(&BelCardBirthday::setValue));
		}
		
		BelCardBirthday() : BelCardProperty() {
			setName("BDAY");
		}
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardAnniversary : public BelCardProperty {
	public:
		static shared_ptr<BelCardAnniversary> create() {
			return make_shared<BelCardAnniversary>();
		}
		
		static shared_ptr<BelCardAnniversary> parse(const string& input) {
			ABNFGrammarBuilder grammar_builder;
			shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
			Parser<shared_ptr<BelCardGeneric>> parser(grammar);
			setHandlerAndCollectors(&parser);
			BelCardParam::setHandlerAndCollectors(&parser);
			shared_ptr<BelCardGeneric> ret = parser.parseInput("ANNIVERSARY", input, NULL);
			return dynamic_pointer_cast<BelCardAnniversary>(ret);
		}
		
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
			parser->setHandler("ANNIVERSARY", make_fn(&BelCardAnniversary::create))
					->setCollector("group", make_sfn(&BelCardAnniversary::setGroup))
					->setCollector("any-param", make_sfn(&BelCardAnniversary::addParam))
					->setCollector("ANNIVERSARY-value", make_sfn(&BelCardAnniversary::setValue));
		}
		
		BelCardAnniversary() : BelCardProperty() {
			setName("ANNIVERSARY");
		}
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardGender : public BelCardProperty {
	public:
		static shared_ptr<BelCardGender> create() {
			return make_shared<BelCardGender>();
		}
		
		static shared_ptr<BelCardGender> parse(const string& input) {
			ABNFGrammarBuilder grammar_builder;
			shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
			Parser<shared_ptr<BelCardGeneric>> parser(grammar);
			setHandlerAndCollectors(&parser);
			BelCardParam::setHandlerAndCollectors(&parser);
			shared_ptr<BelCardGeneric> ret = parser.parseInput("GENDER", input, NULL);
			return dynamic_pointer_cast<BelCardGender>(ret);
		}
		
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
			parser->setHandler("GENDER", make_fn(&BelCardGender::create))
					->setCollector("group", make_sfn(&BelCardGender::setGroup))
					->setCollector("any-param", make_sfn(&BelCardGender::addParam))
					->setCollector("GENDER-value", make_sfn(&BelCardGender::setValue));
		}
		
		BelCardGender() : BelCardProperty() {
			setName("GENDER");
		}
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardPhoto : public BelCardProperty {
	public:
		static shared_ptr<BelCardPhoto> create() {
			return make_shared<BelCardPhoto>();
		}
		
		static shared_ptr<BelCardPhoto> parse(const string& input) {
			ABNFGrammarBuilder grammar_builder;
			shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
			Parser<shared_ptr<BelCardGeneric>> parser(grammar);
			setHandlerAndCollectors(&parser);
			BelCardParam::setHandlerAndCollectors(&parser);
			shared_ptr<BelCardGeneric> ret = parser.parseInput("PHOTO", input, NULL);
			return dynamic_pointer_cast<BelCardPhoto>(ret);
		}
		
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
			parser->setHandler("PHOTO", make_fn(&BelCardPhoto::create))
					->setCollector("group", make_sfn(&BelCardPhoto::setGroup))
					->setCollector("any-param", make_sfn(&BelCardPhoto::addParam))
					->setCollector("PHOTO-value", make_sfn(&BelCardPhoto::setValue));
		}
		
		BelCardPhoto() : BelCardProperty() {
			setName("PHOTO");
		}
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
}

#endif