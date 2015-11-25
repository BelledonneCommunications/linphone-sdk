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
		string _name;
		string _value;
	public:
		static shared_ptr<BelCardProperty> create() {
			return make_shared<BelCardProperty>();
		}
		
		BelCardProperty() {
			
		}
		
		virtual void setName(const string & name) {
			_name = name;
		}
		virtual const string & getName() const {
			return _name;
		}
		
		virtual void setValue(const string & value) {
			_value = value;
		}
		virtual const string & getValue() const {
			return _value;
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
	
	class BelCard : public BelCardGeneric {
	private:
		shared_ptr<BelCardFN> _fn;
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
		
		void addProperty(const shared_ptr<BelCardProperty> &property) {
			_properties.push_back(property);
		}
		const list<shared_ptr<BelCardProperty>> &getProperties() const {
			return _properties;
		}
	};
}

#endif