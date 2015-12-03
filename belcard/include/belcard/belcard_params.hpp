#ifndef belcard_params_hpp
#define belcard_params_hpp

#include "belcard_generic.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardValueParam : public BelCardParam {
	public:
		static shared_ptr<BelCardValueParam> create();
		static shared_ptr<BelCardValueParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardValueParam();
	};
}

#endif