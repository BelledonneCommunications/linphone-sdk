#ifndef belcard_organizational_hpp
#define belcard_organizational_hpp

#include "belcard_property.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardTitle : public BelCardProperty {
	public:
		static shared_ptr<BelCardTitle> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardTitle();
	};
	
	class BelCardRole : public BelCardProperty {
	public:
		static shared_ptr<BelCardRole> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardRole();
	};
	
	class BelCardLogo : public BelCardProperty {
	public:
		static shared_ptr<BelCardLogo> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardLogo();
	};
	
	class BelCardOrganization : public BelCardProperty {
	public:
		static shared_ptr<BelCardOrganization> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardOrganization();
	};
	
	class BelCardMember : public BelCardProperty {
	public:
		static shared_ptr<BelCardMember> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardMember();
	};
	
	class BelCardRelated : public BelCardProperty {
	public:
		static shared_ptr<BelCardRelated> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardRelated();
	};
}

#endif