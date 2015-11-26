#ifndef belcard_generic_hpp
#define belcard_generic_hpp

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
}

#endif