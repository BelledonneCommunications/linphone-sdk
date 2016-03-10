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
		
		void setLabelParam(const shared_ptr<BelCardLabelParam> &param);
		const shared_ptr<BelCardLabelParam> &getLabelParam() const;
		
		virtual void serialize(ostream &output) const;
	};
}

#endif