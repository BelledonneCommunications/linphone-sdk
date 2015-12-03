#ifndef belcard_property_hpp
#define belcard_property_hpp

#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include "belcard/belcard_generic.hpp"
#include "belcard_params.hpp"

#include <string>
#include <list>
#include <sstream>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardProperty : public BelCardGeneric {
	protected:
		string _group;
		string _name;
		string _value;
		shared_ptr<BelCardValueParam> _value_param;
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
		
		virtual void addValueParam(const shared_ptr<BelCardValueParam> &param);
		
		virtual void addParam(const shared_ptr<BelCardParam> &param);
		virtual const list<shared_ptr<BelCardParam>> &getParams() const;
		
		virtual string serialize() const;
	};
}
#endif