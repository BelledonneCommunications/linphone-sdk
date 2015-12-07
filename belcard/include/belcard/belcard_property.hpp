#ifndef belcard_property_hpp
#define belcard_property_hpp

#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include "belcard/belcard_generic.hpp"
#include "belcard_params.hpp"

#include <string>
#include <list>
#include <sstream>

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
			ABNFGrammarBuilder grammar_builder;
			shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
			Parser<shared_ptr<BelCardGeneric>> parser(grammar);
			BelCardParam::setAllParamsHandlersAndCollectors(&parser);
			T::setHandlerAndCollectors(&parser);
			shared_ptr<BelCardGeneric> ret = parser.parseInput(rule, input, NULL);
			return dynamic_pointer_cast<T>(ret);
		}

		static shared_ptr<BelCardProperty> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardProperty();
		
		virtual void setGroup(const string &group);
		virtual const string &getGroup() const;
		
		virtual void setName(const string &name);
		virtual const string &getName() const;
		
		virtual void setValue(const string &value);
		virtual const string &getValue() const;
		
		virtual void setLanguageParam(const shared_ptr<BelCardLanguageParam> &param);
		virtual const shared_ptr<BelCardLanguageParam> &getLanguageParam() const;
		
		virtual void setValueParam(const shared_ptr<BelCardValueParam> &param);
		virtual const shared_ptr<BelCardValueParam> &getValueParam() const;
		
		virtual void setPrefParam(const shared_ptr<BelCardPrefParam> &param);
		virtual const shared_ptr<BelCardPrefParam> &getPrefParam() const;
		
		virtual void setAlternativeIdParam(const shared_ptr<BelCardAlternativeIdParam> &param);
		virtual const shared_ptr<BelCardAlternativeIdParam> &getAlternativeIdParam() const;
		
		virtual void setParamIdParam(const shared_ptr<BelCardParamIdParam> &param);
		virtual const shared_ptr<BelCardParamIdParam> &getParamIdParam() const;
		
		virtual void setTypeParam(const shared_ptr<BelCardTypeParam> &param);
		virtual const shared_ptr<BelCardTypeParam> &getTypeParam() const;
		
		virtual void setMediaTypeParam(const shared_ptr<BelCardMediaTypeParam> &param);
		virtual const shared_ptr<BelCardMediaTypeParam> &getMediaTypeParam() const;
		
		virtual void setCALSCALEParam(const shared_ptr<BelCardCALSCALEParam> &param);
		virtual const shared_ptr<BelCardCALSCALEParam> &getCALSCALEParam() const;
		
		virtual void setSortAsParam(const shared_ptr<BelCardSortAsParam> &param);
		virtual const shared_ptr<BelCardSortAsParam> &getSortAsParam() const;
		
		virtual void setGeoParam(const shared_ptr<BelCardGeoParam> &param);
		virtual const shared_ptr<BelCardGeoParam> &getGeoParam() const;
		
		virtual void setTimezoneParam(const shared_ptr<BelCardTimezoneParam> &param);
		virtual const shared_ptr<BelCardTimezoneParam> &getTimezoneParam() const;
		
		virtual void addParam(const shared_ptr<BelCardParam> &param);
		virtual const list<shared_ptr<BelCardParam>> &getParams() const;
		virtual void removeParam(const shared_ptr<BelCardParam> &param);

		virtual void serialize(ostream &output) const;
	};
}
#endif