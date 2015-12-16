/*
	belcard_params.hpp
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

#ifndef belcard_params_hpp
#define belcard_params_hpp

#include <belr/parser-impl.cc>
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>
#include "belcard_generic.hpp"

#include <string>
#include <sstream>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardParam : public BelCardGeneric {
	private:
		string _name;
		string _value;
	public:
		template <typename T>
		static shared_ptr<T> parseParam(const string& rule, const string& input);
		static shared_ptr<BelCardParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		static void setAllParamsHandlersAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardParam();
		
		virtual void setName(const string &name);
		virtual const string &getName() const;
		
		virtual void setValue(const string &value) ;
		virtual const string &getValue() const;
		
		virtual void serialize(ostream &output) const;
	};
	
	class BelCardLanguageParam : public BelCardParam {
	public:
		static shared_ptr<BelCardLanguageParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardLanguageParam();
	};
	
	class BelCardValueParam : public BelCardParam {
	public:
		static shared_ptr<BelCardValueParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardValueParam();
	};
	
	class BelCardPrefParam : public BelCardParam {
	public:
		static shared_ptr<BelCardPrefParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardPrefParam();
	};
	
	class BelCardAlternativeIdParam : public BelCardParam {
	public:
		static shared_ptr<BelCardAlternativeIdParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardAlternativeIdParam();
	};
	
	class BelCardParamIdParam : public BelCardParam {
	public:
		static shared_ptr<BelCardParamIdParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardParamIdParam();
	};
	
	class BelCardTypeParam : public BelCardParam {
	public:
		static shared_ptr<BelCardTypeParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardTypeParam();
	};
	
	class BelCardMediaTypeParam : public BelCardParam {
	public:
		static shared_ptr<BelCardMediaTypeParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardMediaTypeParam();
	};
	
	class BelCardCALSCALEParam : public BelCardParam {
	public:
		static shared_ptr<BelCardCALSCALEParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardCALSCALEParam();
	};
	
	class BelCardSortAsParam : public BelCardParam {
	public:
		static shared_ptr<BelCardSortAsParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardSortAsParam();
	};
	
	class BelCardGeoParam : public BelCardParam {
	public:
		static shared_ptr<BelCardGeoParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardGeoParam();
	};
	
	class BelCardTimezoneParam : public BelCardParam {
	public:
		static shared_ptr<BelCardTimezoneParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardTimezoneParam();
	};
	
	class BelCardLabelParam : public BelCardParam {
	public:
		static shared_ptr<BelCardLabelParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardLabelParam();
	};
}

#endif