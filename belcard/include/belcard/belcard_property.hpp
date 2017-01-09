/*
	belcard_property.hpp
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

#ifndef belcard_property_hpp
#define belcard_property_hpp

#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include "belcard/belcard_generic.hpp"
#include "belcard_params.hpp"
#include "belcard/belcard_parser.hpp"

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
	class BelCardProperty : public BelCardGeneric {
	protected:
		string _group;
		string _name;
		string _value;
		shared_ptr<BelCardLanguageParam> _lang_param;
		shared_ptr<BelCardValueParam> _value_param;
		shared_ptr<BelCardPrefParam> _pref_param;
		shared_ptr<BelCardAlternativeIdParam> _altid_param;
		shared_ptr<BelCardParamIdParam> _pid_param;
		shared_ptr<BelCardTypeParam> _type_param;
		shared_ptr<BelCardMediaTypeParam> _mediatype_param;
		shared_ptr<BelCardCALSCALEParam> _calscale_param;
		shared_ptr<BelCardSortAsParam> _sort_as_param;
		shared_ptr<BelCardGeoParam> _geo_param;
		shared_ptr<BelCardTimezoneParam> _tz_param;
		list<shared_ptr<BelCardParam>> _params;
		
	public:
		template <typename T>
		static shared_ptr<T> parseProperty(const string& rule, const string& input) {
			shared_ptr<BelCardParser> parser = BelCardParser::getInstance();
			shared_ptr<BelCardGeneric> ret = parser->_parser->parseInput(rule, input, NULL);
			if (ret) {
				return dynamic_pointer_cast<T>(ret);
			}
			return nullptr;
		}
		
		BELCARD_PUBLIC static shared_ptr<BelCardProperty> parse(const string& input);
		BELCARD_PUBLIC static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BELCARD_PUBLIC BelCardProperty();
		
		BELCARD_PUBLIC virtual void setGroup(const string &group);
		BELCARD_PUBLIC virtual const string &getGroup() const;
		
		BELCARD_PUBLIC virtual void setName(const string &name);
		BELCARD_PUBLIC virtual const string &getName() const;
		
		BELCARD_PUBLIC virtual void setValue(const string &value);
		BELCARD_PUBLIC virtual const string &getValue() const;
		
		BELCARD_PUBLIC virtual void setLanguageParam(const shared_ptr<BelCardLanguageParam> &param);
		BELCARD_PUBLIC virtual const shared_ptr<BelCardLanguageParam> &getLanguageParam() const;
		
		BELCARD_PUBLIC virtual void setValueParam(const shared_ptr<BelCardValueParam> &param);
		BELCARD_PUBLIC virtual const shared_ptr<BelCardValueParam> &getValueParam() const;
		
		BELCARD_PUBLIC virtual void setPrefParam(const shared_ptr<BelCardPrefParam> &param);
		BELCARD_PUBLIC virtual const shared_ptr<BelCardPrefParam> &getPrefParam() const;
		
		BELCARD_PUBLIC virtual void setAlternativeIdParam(const shared_ptr<BelCardAlternativeIdParam> &param);
		BELCARD_PUBLIC virtual const shared_ptr<BelCardAlternativeIdParam> &getAlternativeIdParam() const;
		
		BELCARD_PUBLIC virtual void setParamIdParam(const shared_ptr<BelCardParamIdParam> &param);
		BELCARD_PUBLIC virtual const shared_ptr<BelCardParamIdParam> &getParamIdParam() const;
		
		BELCARD_PUBLIC virtual void setTypeParam(const shared_ptr<BelCardTypeParam> &param);
		BELCARD_PUBLIC virtual const shared_ptr<BelCardTypeParam> &getTypeParam() const;
		
		BELCARD_PUBLIC virtual void setMediaTypeParam(const shared_ptr<BelCardMediaTypeParam> &param);
		BELCARD_PUBLIC virtual const shared_ptr<BelCardMediaTypeParam> &getMediaTypeParam() const;
		
		BELCARD_PUBLIC virtual void setCALSCALEParam(const shared_ptr<BelCardCALSCALEParam> &param);
		BELCARD_PUBLIC virtual const shared_ptr<BelCardCALSCALEParam> &getCALSCALEParam() const;
		
		BELCARD_PUBLIC virtual void setSortAsParam(const shared_ptr<BelCardSortAsParam> &param);
		BELCARD_PUBLIC virtual const shared_ptr<BelCardSortAsParam> &getSortAsParam() const;
		
		BELCARD_PUBLIC virtual void setGeoParam(const shared_ptr<BelCardGeoParam> &param);
		BELCARD_PUBLIC virtual const shared_ptr<BelCardGeoParam> &getGeoParam() const;
		
		BELCARD_PUBLIC virtual void setTimezoneParam(const shared_ptr<BelCardTimezoneParam> &param);
		BELCARD_PUBLIC virtual const shared_ptr<BelCardTimezoneParam> &getTimezoneParam() const;
		
		BELCARD_PUBLIC virtual void addParam(const shared_ptr<BelCardParam> &param);
		BELCARD_PUBLIC virtual const list<shared_ptr<BelCardParam>> &getParams() const;
		BELCARD_PUBLIC virtual void removeParam(const shared_ptr<BelCardParam> &param);

		BELCARD_PUBLIC virtual void serialize(ostream &output) const;
	};
}
#endif
