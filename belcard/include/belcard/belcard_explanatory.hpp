/*
	belcard_explanatory.hpp
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

#ifndef belcard_explanatory_hpp
#define belcard_explanatory_hpp

#include "belcard_property.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardCategories : public BelCardProperty {
	public:
		BELCARD_PUBLIC static shared_ptr<BelCardCategories> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardCategories();
	};
	
	class BelCardNote : public BelCardProperty {
	public:
		BELCARD_PUBLIC static shared_ptr<BelCardNote> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardNote();
	};
	
	class BelCardProductId : public BelCardProperty {
	public:
		BELCARD_PUBLIC static shared_ptr<BelCardProductId> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardProductId();
	};
	
	class BelCardRevision : public BelCardProperty {
	public:
		BELCARD_PUBLIC static shared_ptr<BelCardRevision> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardRevision();
	};
	
	class BelCardSound : public BelCardProperty {
	public:
		BELCARD_PUBLIC static shared_ptr<BelCardSound> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardSound();
	};
	
	class BelCardUniqueId : public BelCardProperty {
	public:
		BELCARD_PUBLIC static shared_ptr<BelCardUniqueId> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BELCARD_PUBLIC BelCardUniqueId();
	};
	
	class BelCardClientProductIdMap : public BelCardProperty {
	public:
		BELCARD_PUBLIC static shared_ptr<BelCardClientProductIdMap> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardClientProductIdMap();
	};
	
	class BelCardURL : public BelCardProperty {
	public:
		BELCARD_PUBLIC static shared_ptr<BelCardURL> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardURL();
	};
}

#endif