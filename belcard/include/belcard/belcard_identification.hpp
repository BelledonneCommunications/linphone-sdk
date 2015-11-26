#ifndef belcard_identification_hpp
#define belcard_identification_hpp

#include "belcard.hpp"

#include <string>
#include <list>
#include <map>
#include <memory>

using namespace::std;

namespace belcard {
	class BelCardFN : public BelCardProperty {
	public:
		static shared_ptr<BelCardFN> create() {
			return make_shared<BelCardFN>();
		}
		
		BelCardFN() : BelCardProperty() {
			setName("FN");
		}
		
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
		static shared_ptr<BelCardN> create() {
			return make_shared<BelCardN>();
		}
		
		BelCardN() : BelCardProperty() {
			setName("N");
		}
		
		virtual void setFamilyName(const string &value) {
			_family_name = value;
			setValue(_family_name + ";" + _given_name + ";" + _additional_name + ";" + _prefixes + ";" + _suffixes);
		}
		virtual const string &getFamilyName() const {
			return _family_name;
		}
		
		virtual void setGivenName(const string &value) {
			_given_name = value;
			setValue(_family_name + ";" + _given_name + ";" + _additional_name + ";" + _prefixes + ";" + _suffixes);
		}
		virtual const string &getGivenName() const {
			return _given_name;
		}
		
		virtual void setAdditionalName(const string &value) {
			_additional_name = value;
			setValue(_family_name + ";" + _given_name + ";" + _additional_name + ";" + _prefixes + ";" + _suffixes);
		}
		virtual const string &getAdditionalName() const {
			return _additional_name;
		}
		
		virtual void setPrefixes(const string &value) {
			_prefixes = value;
			setValue(_family_name + ";" + _given_name + ";" + _additional_name + ";" + _prefixes + ";" + _suffixes);
		}
		virtual const string &getPrefixes() const {
			return _prefixes;
		}
		
		virtual void setSuffixes(const string &value) {
			_suffixes = value;
			setValue(_family_name + ";" + _given_name + ";" + _additional_name + ";" + _prefixes + ";" + _suffixes);
		}
		virtual const string &getSuffixes() const {
			return _suffixes;
		}
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardNickname : public BelCardProperty {
	public:
		static shared_ptr<BelCardNickname> create() {
			return make_shared<BelCardNickname>();
		}
		
		BelCardNickname() : BelCardProperty() {
			setName("NICKNAME");
		}
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardBirthday : public BelCardProperty {
	public:
		static shared_ptr<BelCardBirthday> create() {
			return make_shared<BelCardBirthday>();
		}
		
		BelCardBirthday() : BelCardProperty() {
			setName("BDAY");
		}
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardAnniversary : public BelCardProperty {
	public:
		static shared_ptr<BelCardAnniversary> create() {
			return make_shared<BelCardAnniversary>();
		}
		
		BelCardAnniversary() : BelCardProperty() {
			setName("ANNIVERSARY");
		}
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
	
	class BelCardGender : public BelCardProperty {
	public:
		static shared_ptr<BelCardGender> create() {
			return make_shared<BelCardGender>();
		}
		
		BelCardGender() : BelCardProperty() {
			setName("GENDER");
		}
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
	};
}

#endif