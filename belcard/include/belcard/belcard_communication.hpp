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
	class BelCardPhoneNumber : public BelCardProperty {
	public:
		static shared_ptr<BelCardPhoneNumber> create();
		static shared_ptr<BelCardPhoneNumber> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardPhoneNumber();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardEmail : public BelCardProperty {
	public:
		static shared_ptr<BelCardEmail> create();
		static shared_ptr<BelCardEmail> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardEmail();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardImpp : public BelCardProperty {
	public:
		static shared_ptr<BelCardImpp> create();
		static shared_ptr<BelCardImpp> parse(const string& input) ;
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardImpp();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardLang : public BelCardProperty {
	public:
		static shared_ptr<BelCardLang> create();
		static shared_ptr<BelCardLang> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardLang();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
}

#endif