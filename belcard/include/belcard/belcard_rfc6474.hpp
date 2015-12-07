#ifndef belcard_rfc6474_hpp
#define belcard_rfc6474_hpp

#include "belcard_property.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>
#include <sstream>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardBirthPlace : public BelCardProperty {
	private:
		
	public:
		static shared_ptr<BelCardBirthPlace> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardBirthPlace();
	};
	
	class BelCardDeathPlace : public BelCardProperty {
	private:
		
	public:
		static shared_ptr<BelCardDeathPlace> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardDeathPlace();
	};
	
	class BelCardDeathDate : public BelCardProperty {
	private:
		
	public:
		static shared_ptr<BelCardDeathDate> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardDeathDate();
	};
}

#endif