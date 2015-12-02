#ifndef belcard_organizational_hpp
#define belcard_organizational_hpp

#include "belcard_generic.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardTitle : public BelCardProperty {
	public:
		static shared_ptr<BelCardTitle> create();
		static shared_ptr<BelCardTitle> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardTitle();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardRole : public BelCardProperty {
	public:
		static shared_ptr<BelCardRole> create();
		static shared_ptr<BelCardRole> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardRole();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardLogo : public BelCardProperty {
	public:
		static shared_ptr<BelCardLogo> create();
		static shared_ptr<BelCardLogo> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardLogo();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardOrganization : public BelCardProperty {
	public:
		static shared_ptr<BelCardOrganization> create();
		static shared_ptr<BelCardOrganization> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardOrganization();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardMember : public BelCardProperty {
	public:
		static shared_ptr<BelCardMember> create();
		static shared_ptr<BelCardMember> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardMember();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardRelated : public BelCardProperty {
	public:
		static shared_ptr<BelCardRelated> create();
		static shared_ptr<BelCardRelated> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardRelated();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
}

#endif