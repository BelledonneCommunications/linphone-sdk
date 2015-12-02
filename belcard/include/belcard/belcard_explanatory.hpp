#ifndef belcard_explanatory_hpp
#define belcard_explanatory_hpp

#include "belcard_generic.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardCategories : public BelCardProperty {
	public:
		static shared_ptr<BelCardCategories> create();
		static shared_ptr<BelCardCategories> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardCategories();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardNote : public BelCardProperty {
	public:
		static shared_ptr<BelCardNote> create();
		static shared_ptr<BelCardNote> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardNote();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardProductId : public BelCardProperty {
	public:
		static shared_ptr<BelCardProductId> create();
		static shared_ptr<BelCardProductId> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardProductId();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardRevision : public BelCardProperty {
	public:
		static shared_ptr<BelCardRevision> create();
		static shared_ptr<BelCardRevision> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardRevision();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardSound : public BelCardProperty {
	public:
		static shared_ptr<BelCardSound> create();
		static shared_ptr<BelCardSound> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardSound();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardUniqueId : public BelCardProperty {
	public:
		static shared_ptr<BelCardUniqueId> create();
		static shared_ptr<BelCardUniqueId> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardUniqueId();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardClientProductIdMap : public BelCardProperty {
	public:
		static shared_ptr<BelCardClientProductIdMap> create();
		static shared_ptr<BelCardClientProductIdMap> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardClientProductIdMap();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardURL : public BelCardProperty {
	public:
		static shared_ptr<BelCardURL> create();
		static shared_ptr<BelCardURL> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardURL();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
}

#endif