#ifndef belcard_hpp
#define belcard_hpp

#include <string>
#include <list>
#include <map>
#include <memory>

using namespace::std;

namespace belcard {
	class BelCardGeneric {
	public:
		BelCardGeneric() { }
		
		virtual ~BelCardGeneric() { } //put a virtual destructor to enable polymorphism and dynamic casting.
	};
	
	class BelCardProperty : public BelCardGeneric {
	protected:
		string _group;
		string _name;
		string _value;
	public:
		static shared_ptr<BelCardProperty> create() {
			return make_shared<BelCardProperty>();
		}
		
		BelCardProperty() {
			
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
		
		virtual string toString() {
			return (_group.length() > 0 ? _group + "." : "") + _name + ":" + _value + "\r\n";
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
	};
	
	class BelCardN : public BelCardProperty {
	private:
		string _family_name;
		string _given_name;
		string _additional_name;
		string _honorific_prefixes;
		string _honorific_suffixes;
	public:
		static shared_ptr<BelCardN> create() {
			return make_shared<BelCardN>();
		}
		
		BelCardN() : BelCardProperty() {
			setName("N");
		}
	};
	
	class BelCard : public BelCardGeneric {
	private:
		shared_ptr<BelCardFN> _fn;
		shared_ptr<BelCardN> _n;
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