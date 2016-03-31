/*
	belcard_identification.hpp
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

#ifndef belcard_identification_hpp
#define belcard_identification_hpp

#include "belcard_property.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>
#include <list>
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
	class BelCardFullName : public BelCardProperty {
	public:
		BELCARD_PUBLIC static shared_ptr<BelCardFullName> parse(const string& input);
		BELCARD_PUBLIC static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BELCARD_PUBLIC BelCardFullName();
	};
	
	class BelCardName : public BelCardProperty {
	private:
		string _family_name;
		string _given_name;
		string _additional_name;
		string _prefixes;
		string _suffixes;
		
	public:
		BELCARD_PUBLIC static shared_ptr<BelCardName> parse(const string& input);
		BELCARD_PUBLIC static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) ;
		
		BELCARD_PUBLIC BelCardName();
		
		BELCARD_PUBLIC void setFamilyName(const string &value);
		BELCARD_PUBLIC const string &getFamilyName() const;
		
		BELCARD_PUBLIC void setGivenName(const string &value);
		BELCARD_PUBLIC const string &getGivenName() const;
		
		BELCARD_PUBLIC void setAdditionalName(const string &value);
		BELCARD_PUBLIC const string &getAdditionalName() const;
		
		BELCARD_PUBLIC void setPrefixes(const string &value);
		BELCARD_PUBLIC const string &getPrefixes() const;
		
		BELCARD_PUBLIC void setSuffixes(const string &value);
		BELCARD_PUBLIC const string &getSuffixes() const;
		
		BELCARD_PUBLIC virtual void serialize(ostream &output) const;
	};
	
	class BelCardNickname : public BelCardProperty {
	public:
		BELCARD_PUBLIC static shared_ptr<BelCardNickname> parse(const string& input);
		BELCARD_PUBLIC static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BELCARD_PUBLIC BelCardNickname();
	};
	
	class BelCardBirthday : public BelCardProperty {
	public:
		BELCARD_PUBLIC static shared_ptr<BelCardBirthday> parse(const string& input);
		BELCARD_PUBLIC static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BELCARD_PUBLIC BelCardBirthday();
	};
	
	class BelCardAnniversary : public BelCardProperty {
	public:
		BELCARD_PUBLIC static shared_ptr<BelCardAnniversary> parse(const string& input);
		BELCARD_PUBLIC static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BELCARD_PUBLIC BelCardAnniversary();
	};
	
	class BelCardGender : public BelCardProperty {
	public:
		BELCARD_PUBLIC static shared_ptr<BelCardGender> parse(const string& input);
		BELCARD_PUBLIC static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BELCARD_PUBLIC BelCardGender();
	};
	
	class BelCardPhoto : public BelCardProperty {
	public:
		BELCARD_PUBLIC static shared_ptr<BelCardPhoto> parse(const string& input);
		BELCARD_PUBLIC static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BELCARD_PUBLIC BelCardPhoto();
	};
}

#endif
