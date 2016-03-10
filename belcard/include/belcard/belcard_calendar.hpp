/*
	belcard_calendar.hpp
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

#ifndef belcard_calendar_hpp
#define belcard_calendar_hpp

#include "belcard_property.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardFBURL : public BelCardProperty {
	public:
		BELCARD_PUBLIC static shared_ptr<BelCardFBURL> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardFBURL();
	};
	
	class BelCardCALADRURI : public BelCardProperty {
	public:
		BELCARD_PUBLIC static shared_ptr<BelCardCALADRURI> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardCALADRURI();
	};
	
	class BelCardCALURI : public BelCardProperty {
	public:
		BELCARD_PUBLIC static shared_ptr<BelCardCALURI> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardCALURI();
	};
}

#endif