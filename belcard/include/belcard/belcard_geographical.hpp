#ifndef belcard_geographical_hpp
#define belcard_geographical_hpp

#include "belcard_generic.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardTimezone : public BelCardProperty {
	public:
		static shared_ptr<BelCardTimezone> create();
		static shared_ptr<BelCardTimezone> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardTimezone();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardGeo : public BelCardProperty {
	public:
		static shared_ptr<BelCardGeo> create();
		static shared_ptr<BelCardGeo> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardGeo();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
}

#endif