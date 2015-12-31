/*
	belcard_communication.hpp
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

#ifndef belcard_communication_hpp
#define belcard_communication_hpp

#include "belcard_property.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>
#include <sstream>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardPhoneNumber : public BelCardProperty {
	public:
		static shared_ptr<BelCardPhoneNumber> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardPhoneNumber();
	};
	
	class BelCardEmail : public BelCardProperty {
	public:
		static shared_ptr<BelCardEmail> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardEmail();
	};
	
	class BelCardImpp : public BelCardProperty {
	public:
		static shared_ptr<BelCardImpp> parse(const string& input) ;
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BELCARD_PUBLIC BelCardImpp();
	};
	
	class BelCardLang : public BelCardProperty {
	public:
		static shared_ptr<BelCardLang> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardLang();
	};
}

#endif