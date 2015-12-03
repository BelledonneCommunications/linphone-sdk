#ifndef belcard_addressing_hpp
#define belcard_addressing_hpp

#include "belcard_property.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>
#include <sstream>

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
		static shared_ptr<BelCardAddress> create();
		static shared_ptr<BelCardAddress> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardAddress();
		
		void setPostOfficeBox(const string &value);
		const string &getPostOfficeBox() const;
		
		void setExtendedAddress(const string &value);
		const string &getExtendedAddress() const;
		
		void setStreet(const string &value);
		const string &getStreet() const;
		
		void setLocality(const string &value);
		const string &getLocality() const;
		
		void setRegion(const string &value);
		const string &getRegion() const;
		
		void setPostalCode(const string &value);
		const string &getPostalCode() const;
		
		void setCountry(const string &value);
		const string &getCountry() const;
		
		string serialize() const;
	};
}

#endif