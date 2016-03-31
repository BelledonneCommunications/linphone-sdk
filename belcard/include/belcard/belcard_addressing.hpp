/*
	belcard_addressing.hpp
	Copyright (C) 2015  Belledonne Communications SARL

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef belcard_addressing_hpp
#define belcard_addressing_hpp

#include "belcard_property.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>
#include <sstream>

#ifndef BELCARD_PUBLIC
#if defined(_MSC_VER)
#define BELCARD_PUBLIC	__declspec(dllexport)
#else
#define BELCARD_PUBLIC
#endif
#endif

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
		shared_ptr<BelCardLabelParam> _label_param;
		
	public:
		BELCARD_PUBLIC static shared_ptr<BelCardAddress> parse(const string& input);
		BELCARD_PUBLIC static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BELCARD_PUBLIC BelCardAddress();
		
		BELCARD_PUBLIC void setPostOfficeBox(const string &value);
		BELCARD_PUBLIC const string &getPostOfficeBox() const;
		
		BELCARD_PUBLIC void setExtendedAddress(const string &value);
		BELCARD_PUBLIC const string &getExtendedAddress() const;
		
		BELCARD_PUBLIC void setStreet(const string &value);
		BELCARD_PUBLIC const string &getStreet() const;
		
		BELCARD_PUBLIC void setLocality(const string &value);
		BELCARD_PUBLIC const string &getLocality() const;
		
		BELCARD_PUBLIC void setRegion(const string &value);
		BELCARD_PUBLIC const string &getRegion() const;
		
		BELCARD_PUBLIC void setPostalCode(const string &value);
		BELCARD_PUBLIC const string &getPostalCode() const;
		
		BELCARD_PUBLIC void setCountry(const string &value);
		BELCARD_PUBLIC const string &getCountry() const;
		
		BELCARD_PUBLIC void setLabelParam(const shared_ptr<BelCardLabelParam> &param);
		BELCARD_PUBLIC const shared_ptr<BelCardLabelParam> &getLabelParam() const;
		
		BELCARD_PUBLIC virtual void serialize(ostream &output) const;
	};
}

#endif
