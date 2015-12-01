#ifndef belcard_generic_hpp
#define belcard_generic_hpp

#include <belr/parser-impl.cc>
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>
#include "belcard/vcard_grammar.hpp"

#include <string>
#include <list>
#include <sstream>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardGeneric {
	public:
		BelCardGeneric() { }
		
		virtual ~BelCardGeneric() { } //put a virtual destructor to enable polymorphism and dynamic casting.
	};
	
	class BelCardParam : public BelCardGeneric {
	private:
		string _name;
		string _value;
	public:
		static shared_ptr<BelCardParam> create() {
			return make_shared<BelCardParam>();
		}
		
		static shared_ptr<BelCardParam> parse(const string& input) {
			ABNFGrammarBuilder grammar_builder;
			shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
			Parser<shared_ptr<BelCardGeneric>> parser(grammar);
			setHandlerAndCollectors(&parser);
			shared_ptr<BelCardGeneric> ret = parser.parseInput("any-param", input, NULL);
			return dynamic_pointer_cast<BelCardParam>(ret);
		}
		
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
			parser->setHandler("any-param", make_fn(&BelCardParam::create))
					->setCollector("param-name", make_sfn(&BelCardParam::setName))
					->setCollector("param-value", make_sfn(&BelCardParam::setValue));
		}
		
		BelCardParam() : BelCardGeneric() {
			
		}
		
		virtual void setName(const string &name) {
			_name = name;
		}
		virtual const string &getName() const {
			return _name;
		}
		
		virtual void setValue(const string &value) {
			_value = value;
		}
		virtual const string &getValue() const {
			return _value;
		}
		
		virtual string serialize() const {
			stringstream output;
			output << getName() << "=" << getValue();
			return output.str();
		}
		
		friend ostream &operator<<(ostream &output, const BelCardParam &param) {
			output << param.serialize();
			return output;
		}
	};
	
	class BelCardProperty : public BelCardGeneric {
	protected:
		string _group;
		string _name;
		string _value;
		list<shared_ptr<BelCardParam>> _params;
		
	public:
		static shared_ptr<BelCardProperty> create() {
			return make_shared<BelCardProperty>();
		}
		
		BelCardProperty() : BelCardGeneric() {
			
		}
		
		virtual void setGroup(const string &group) {
			_group = group;
		}
		virtual const string &getGroup() const {
			return _group;
		}
		
		virtual void setName(const string &name) {
			_name = name;
		}
		virtual const string &getName() const {
			return _name;
		}
		
		virtual void setValue(const string &value) {
			_value = value;
		}
		virtual const string &getValue() const {
			return _value;
		}
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			_params.push_back(param);
		}
		virtual const list<shared_ptr<BelCardParam>> &getParams() const {
			return _params;
		}
		
		virtual string serialize() const {
			stringstream output;
			
			if (getGroup().length() > 0) {
				output << getGroup() << ".";
			}
			
			output << getName();
			for (auto it = getParams().begin(); it != getParams().end(); ++it) {
				output << ";" << (**it); 
			}
			output << ":" << getValue() << "\r\n";
			
			return output.str();
		}
		
		friend ostream &operator<<(ostream &output, const BelCardProperty &prop) {
			output << prop.serialize();
			return output;            
		}
	};
}

#endif