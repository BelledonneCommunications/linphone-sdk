#ifndef belcard_identification_hpp
#define belcard_identification_hpp

#include "belcard_generic.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>
#include <list>
#include <sstream>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardFN : public BelCardProperty {
	public:
		static shared_ptr<BelCardFN> create();
		static shared_ptr<BelCardFN> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardFN();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardN : public BelCardProperty {
	private:
		string _family_name;
		string _given_name;
		string _additional_name;
		string _prefixes;
		string _suffixes;
		
	public:
		static shared_ptr<BelCardN> create();
		static shared_ptr<BelCardN> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) ;
		
		BelCardN();
		
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
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardNickname : public BelCardProperty {
	public:
		static shared_ptr<BelCardNickname> create();
		static shared_ptr<BelCardNickname> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardNickname();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardBirthday : public BelCardProperty {
	public:
		static shared_ptr<BelCardBirthday> create();
		static shared_ptr<BelCardBirthday> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardBirthday();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardAnniversary : public BelCardProperty {
	public:
		static shared_ptr<BelCardAnniversary> create();
		static shared_ptr<BelCardAnniversary> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardAnniversary();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardGender : public BelCardProperty {
	public:
		static shared_ptr<BelCardGender> create();
		static shared_ptr<BelCardGender> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardGender();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardPhoto : public BelCardProperty {
	public:
		static shared_ptr<BelCardPhoto> create();
		static shared_ptr<BelCardPhoto> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardPhoto();
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
}

#endif