#ifndef belcard_hpp
#define belcard_hpp

#include <string>
#include <list>
#include <map>
#include <memory>
#include <vector>

using namespace::std;

namespace belcard {
	class BelCardGeneric {
	public:
		BelCardGeneric() { }
		
		virtual ~BelCardGeneric() { } //put a virtual destructor to enable polymorphism and dynamic casting.
	};
	
	class BelCardParam : public BelCardGeneric {
	private:
		string _name;
		string _value;
	public:
		static shared_ptr<BelCardParam> create() {
			return make_shared<BelCardParam>();
		}
		
		BelCardParam() : BelCardGeneric() {
			
		}
		
		virtual void setName(const string &name) {
			_name = name;
		}
		virtual const string &getName() const {
			return _name;
		}
		
		virtual void setValue(const string &value) {
			_value = value;
		}
		virtual const string &getValue() const {
			return _value;
		}
		
		virtual string toString() {
			return _name + "=" + _value;
		}
	};
	
	class BelCardProperty : public BelCardGeneric {
	protected:
		string _group;
		string _name;
		string _value;
		list<shared_ptr<BelCardParam>> _params;
	public:
		static shared_ptr<BelCardProperty> create() {
			return make_shared<BelCardProperty>();
		}
		
		BelCardProperty() : BelCardGeneric() {
			
		}
		
		virtual void setGroup(const string &group) {
			_group = group;
		}
		virtual const string &getGroup() const {
			return _group;
		}
		
		virtual void setName(const string &name) {
			_name = name;
		}
		virtual const string &getName() const {
			return _name;
		}
		
		virtual void setValue(const string &value) {
			_value = value;
		}
		virtual const string &getValue() const {
			return _value;
		}
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			_params.push_back(param);
		}
		virtual const list<shared_ptr<BelCardParam>> &getParams() const {
			return _params;
		}
		
		virtual string toString() {
			string property;
			if (_group.length() > 0)
				property += _group + ".";
			property += _name;
			for (auto it = _params.begin(); it != _params.end(); ++it) {
				property += ";" + (*it)->toString(); 
			}
			property += ":" + _value + "\r\n";
			return property;
		}
	};
	
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
	
	class BelCard : public BelCardGeneric {
	private:
		shared_ptr<BelCardFN> _fn;
		shared_ptr<BelCardN> _n;
		list<shared_ptr<BelCardNickname>> _nicknames;
		list<shared_ptr<BelCardProperty>> _properties;
		
	public:
		static shared_ptr<BelCard> create() {
			return make_shared<BelCard>();
		}
		
		BelCard() {
			
		}
		
		void setFN(const shared_ptr<BelCardFN> &fn) {
			_fn = fn;
			addProperty(_fn);
		}
		const shared_ptr<BelCardFN> &getFN() const {
			return _fn;
		}
		
		void setN(const shared_ptr<BelCardN> &n) {
			_n = n;
			addProperty(_n);
		}
		const shared_ptr<BelCardN> &getN() const {
			return _n;
		}
		
		void addNickname(const shared_ptr<BelCardNickname> &nickname) {
			_nicknames.push_back(nickname);
			addProperty(nickname);
		}
		const list<shared_ptr<BelCardNickname>> &getNicknames() const {
			return _nicknames;
		}
		
		void addProperty(const shared_ptr<BelCardProperty> &property) {
			_properties.push_back(property);
		}
		const list<shared_ptr<BelCardProperty>> &getProperties() const {
			return _properties;
		}
		
		string toString() {
			string vcard = "BEGIN:VCARD\r\nVERSION:4.0\r\n";
			for (auto it = _properties.begin(); it != _properties.end(); ++it) {
				vcard += (*it)->toString(); 
			}
			vcard += "END:VCARD\r\n";
			return vcard;
		}
	};
}

#endif