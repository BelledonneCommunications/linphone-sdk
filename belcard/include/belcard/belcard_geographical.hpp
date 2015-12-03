#ifndef belcard_geographical_hpp
#define belcard_geographical_hpp

#include "belcard_property.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardTimezone : public BelCardProperty {
	public:
		static shared_ptr<BelCardTimezone> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardTimezone();
	};
	
	class BelCardGeo : public BelCardProperty {
	public:
		static shared_ptr<BelCardGeo> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardGeo();
	};
}

#endif