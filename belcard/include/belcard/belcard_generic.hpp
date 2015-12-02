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
		template<typename T>
		static shared_ptr<T> create() {
			return make_shared<T>();
		}
		
		BelCardGeneric() { }
		
		virtual ~BelCardGeneric() { } //put a virtual destructor to enable polymorphism and dynamic casting.
	};
	
	class BelCardParam : public BelCardGeneric {
	private:
		string _name;
		string _value;
	public:
		static shared_ptr<BelCardParam> create();
		static shared_ptr<BelCardParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardParam();
		
		friend ostream &operator<<(ostream &output, const BelCardParam &param) {
			output << param.serialize();
			return output;
		}
		
		virtual void setName(const string &name);
		virtual const string &getName() const;
		
		virtual void setValue(const string &value) ;
		virtual const string &getValue() const;
		
		virtual string serialize() const;
	};
	
	class BelCardProperty : public BelCardGeneric {
	protected:
		string _group;
		string _name;
		string _value;
		list<shared_ptr<BelCardParam>> _params;
		
	public:
		static shared_ptr<BelCardProperty> create();
		static shared_ptr<BelCardProperty> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardProperty();
		
		friend ostream &operator<<(ostream &output, const BelCardProperty &prop) {
			output << prop.serialize();
			return output;            
		}
		
		virtual void setGroup(const string &group);
		virtual const string &getGroup() const;
		
		virtual void setName(const string &name);
		virtual const string &getName() const;
		
		virtual void setValue(const string &value);
		virtual const string &getValue() const;
		
		virtual void addParam(const shared_ptr<BelCardParam> &param);
		virtual const list<shared_ptr<BelCardParam>> &getParams() const;
		
		virtual string serialize() const;
	};
}

#endif