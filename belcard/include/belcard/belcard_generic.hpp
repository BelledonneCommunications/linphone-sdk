#ifndef belcard_generic_hpp
#define belcard_generic_hpp

#include <belr/parser-impl.cc>

#include <string>
#include <list>
#include <map>
#include <memory>

using namespace::std;
using namespace::belr;

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
		
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
			parser->setHandler("any-param", make_fn(&BelCardParam::create))
					->setCollector("param-name", make_sfn(&BelCardParam::setName))
					->setCollector("param-value", make_sfn(&BelCardParam::setValue));
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
		
		friend ostream &operator<<(ostream &output, const BelCardParam &param) {
			output << param.getName() << "=" << param.getValue();
			return output;
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
		
		friend ostream &operator<<(ostream &output, const BelCardProperty &prop) {
			if (prop.getGroup().length() > 0) {
				output << prop.getGroup() << ".";
			}
			
			output << prop.getName();
			for (auto it = prop.getParams().begin(); it != prop.getParams().end(); ++it) {
				output << ";" << (**it); 
			}
			output << ":" << prop.getValue() << "\r\n";
			return output;            
		}
	};
}

#endif