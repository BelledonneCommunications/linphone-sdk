#ifndef belcard_explanatory_hpp
#define belcard_explanatory_hpp

#include "belcard_property.hpp"
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
	};
	
	class BelCardNote : public BelCardProperty {
	public:
		static shared_ptr<BelCardNote> create();
		static shared_ptr<BelCardNote> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardNote();
	};
	
	class BelCardProductId : public BelCardProperty {
	public:
		static shared_ptr<BelCardProductId> create();
		static shared_ptr<BelCardProductId> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardProductId();
	};
	
	class BelCardRevision : public BelCardProperty {
	public:
		static shared_ptr<BelCardRevision> create();
		static shared_ptr<BelCardRevision> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardRevision();
	};
	
	class BelCardSound : public BelCardProperty {
	public:
		static shared_ptr<BelCardSound> create();
		static shared_ptr<BelCardSound> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardSound();
	};
	
	class BelCardUniqueId : public BelCardProperty {
	public:
		static shared_ptr<BelCardUniqueId> create();
		static shared_ptr<BelCardUniqueId> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardUniqueId();
	};
	
	class BelCardClientProductIdMap : public BelCardProperty {
	public:
		static shared_ptr<BelCardClientProductIdMap> create();
		static shared_ptr<BelCardClientProductIdMap> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardClientProductIdMap();
	};
	
	class BelCardURL : public BelCardProperty {
	public:
		static shared_ptr<BelCardURL> create();
		static shared_ptr<BelCardURL> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardURL();
	};
}

#endif