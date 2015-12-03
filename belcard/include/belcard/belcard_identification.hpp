#ifndef belcard_identification_hpp
#define belcard_identification_hpp

#include "belcard_property.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>
#include <list>
#include <sstream>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardFullName : public BelCardProperty {
	public:
		static shared_ptr<BelCardFullName> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardFullName();
	};
	
	class BelCardName : public BelCardProperty {
	private:
		string _family_name;
		string _given_name;
		string _additional_name;
		string _prefixes;
		string _suffixes;
		
	public:
		static shared_ptr<BelCardName> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) ;
		
		BelCardName();
		
		void setFamilyName(const string &value);
		const string &getFamilyName() const;
		
		void setGivenName(const string &value);
		const string &getGivenName() const;
		
		void setAdditionalName(const string &value);
		const string &getAdditionalName() const;
		
		void setPrefixes(const string &value);
		const string &getPrefixes() const;
		
		void setSuffixes(const string &value);
		const string &getSuffixes() const;
		
		string serialize() const;
	};
	
	class BelCardNickname : public BelCardProperty {
	public:
		static shared_ptr<BelCardNickname> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardNickname();
	};
	
	class BelCardBirthday : public BelCardProperty {
	public:
		static shared_ptr<BelCardBirthday> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardBirthday();
	};
	
	class BelCardAnniversary : public BelCardProperty {
	public:
		static shared_ptr<BelCardAnniversary> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardAnniversary();
	};
	
	class BelCardGender : public BelCardProperty {
	public:
		static shared_ptr<BelCardGender> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardGender();
	};
	
	class BelCardPhoto : public BelCardProperty {
	public:
		static shared_ptr<BelCardPhoto> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardPhoto();
	};
}

#endif