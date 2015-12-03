#ifndef belcard_calendar_hpp
#define belcard_calendar_hpp

#include "belcard_property.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardFBURL : public BelCardProperty {
	public:
		static shared_ptr<BelCardFBURL> create();
		static shared_ptr<BelCardFBURL> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardFBURL();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardCALADRURI : public BelCardProperty {
	public:
		static shared_ptr<BelCardCALADRURI> create();
		static shared_ptr<BelCardCALADRURI> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardCALADRURI();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardCALURI : public BelCardProperty {
	public:
		static shared_ptr<BelCardCALURI> create();
		static shared_ptr<BelCardCALURI> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardCALURI();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
}

#endif