#ifndef belcard_general_hpp
#define belcard_general_hpp

#include "belcard_property.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardKind : public BelCardProperty {
	public:
		static shared_ptr<BelCardKind> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardKind();
	};
	
	class BelCardSource : public BelCardProperty {
	public:
		static shared_ptr<BelCardSource> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardSource();
	};
	
	class BelCardXML : public BelCardProperty {
	public:
		static shared_ptr<BelCardXML> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardXML();
	};
}

#endif