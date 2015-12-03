#ifndef belcard_security_hpp
#define belcard_security_hpp

#include "belcard_property.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardKey : public BelCardProperty {
	public:
		static shared_ptr<BelCardKey> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardKey();
	};
}

#endif