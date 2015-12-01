#ifndef belcard_addressing_hpp
#define belcard_addressing_hpp

#include "belcard_generic.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>
#include <list>
#include <map>
#include <memory>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardAddress : public BelCardProperty {
	private:
		string _po_box;
		string _extended_address;
		string _street;
		string _locality;
		string _region;
		string _postal_code;
		string _country;
		
	public:
		static shared_ptr<BelCardAddress> create() {
			return make_shared<BelCardAddress>();
		}
		
		static shared_ptr<BelCardAddress> parse(const string& input) {
			ABNFGrammarBuilder grammar_builder;
			shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
			Parser<shared_ptr<BelCardGeneric>> parser(grammar);
			setHandlerAndCollectors(&parser);
			BelCardParam::setHandlerAndCollectors(&parser);
			shared_ptr<BelCardGeneric> ret = parser.parseInput("ADR", input, NULL);
			return dynamic_pointer_cast<BelCardAddress>(ret);
		}
		
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
			parser->setHandler("ADR", make_fn(&BelCardAddress::create))
					->setCollector("group", make_sfn(&BelCardAddress::setGroup))
					->setCollector("any-param", make_sfn(&BelCardAddress::addParam))
					->setCollector("ADR-pobox", make_sfn(&BelCardAddress::setPostOfficeBox))
					->setCollector("ADR-ext", make_sfn(&BelCardAddress::setExtendedAddress))
					->setCollector("ADR-street", make_sfn(&BelCardAddress::setStreet))
					->setCollector("ADR-locality", make_sfn(&BelCardAddress::setLocality))
					->setCollector("ADR-region", make_sfn(&BelCardAddress::setRegion))
					->setCollector("ADR-code", make_sfn(&BelCardAddress::setPostalCode))
					->setCollector("ADR-country", make_sfn(&BelCardAddress::setCountry));
		}
		
		BelCardAddress() : BelCardProperty() {
			setName("ADR");
		}
		
		void setPostOfficeBox(const string &value) {
			_po_box = value;
		}
		const string &getPostOfficeBox() const {
			return _po_box;
		}
		
		void setExtendedAddress(const string &value) {
			_extended_address = value;
		}
		const string &getExtendedAddress() const {
			return _extended_address;
		}
		
		void setStreet(const string &value) {
			_street = value;
		}
		const string &getStreet() const {
			return _street;
		}
		
		void setLocality(const string &value) {
			_locality = value;
		}
		const string &getLocality() const {
			return _locality;
		}
		
		void setRegion(const string &value) {
			_region = value;
		}
		const string &getRegion() const {
			return _region;
		}
		
		void setPostalCode(const string &value) {
			_postal_code = value;
		}
		const string &getPostalCode() const {
			return _postal_code;
		}
		
		void setCountry(const string &value) {
			_country = value;
		}
		const string &getCountry() const {
			return _country;
		}
		
		virtual void addParam(const shared_ptr<BelCardParam> &param) {
			BelCardProperty::addParam(param);
		}
		
		string serialize() const {
			stringstream output;
			
			if (getGroup().length() > 0) {
				output << getGroup() << ".";
			}
			
			output << getName();
			for (auto it = getParams().begin(); it != getParams().end(); ++it) {
				output << ";" << (**it); 
			}
			output << ":" << getPostOfficeBox() << ";" << getExtendedAddress()
				<< ";" << getStreet() << ";" << getLocality() << ";" << getRegion() 
				<< ";" << getPostalCode() << ";" << getCountry() << "\r\n";
				
			return output.str();            
		}
	};
}

#endif