/*
	belcard_organizational.hpp
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

#ifndef belcard_organizational_hpp
#define belcard_organizational_hpp

#include "belcard_property.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardTitle : public BelCardProperty {
	public:
		static shared_ptr<BelCardTitle> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardTitle();
	};
	
	class BelCardRole : public BelCardProperty {
	public:
		static shared_ptr<BelCardRole> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardRole();
	};
	
	class BelCardLogo : public BelCardProperty {
	public:
		static shared_ptr<BelCardLogo> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardLogo();
	};
	
	class BelCardOrganization : public BelCardProperty {
	public:
		static shared_ptr<BelCardOrganization> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardOrganization();
	};
	
	class BelCardMember : public BelCardProperty {
	public:
		static shared_ptr<BelCardMember> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardMember();
	};
	
	class BelCardRelated : public BelCardProperty {
	public:
		static shared_ptr<BelCardRelated> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardRelated();
	};
}

#endif