#ifndef belcard_params_hpp
#define belcard_params_hpp

#include "belcard_generic.hpp"
#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#include <string>

using namespace::std;
using namespace::belr;

namespace belcard {
	class BelCardLanguageParam : public BelCardParam {
	public:
		static shared_ptr<BelCardLanguageParam> create();
		static shared_ptr<BelCardLanguageParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardLanguageParam();
	};
	
	class BelCardValueParam : public BelCardParam {
	public:
		static shared_ptr<BelCardValueParam> create();
		static shared_ptr<BelCardValueParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardValueParam();
	};
	
	class BelCardPrefParam : public BelCardParam {
	public:
		static shared_ptr<BelCardPrefParam> create();
		static shared_ptr<BelCardPrefParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardPrefParam();
	};
	
	class BelCardAlternativeIdParam : public BelCardParam {
	public:
		static shared_ptr<BelCardAlternativeIdParam> create();
		static shared_ptr<BelCardAlternativeIdParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardAlternativeIdParam();
	};
	
	class BelCardParamIdParam : public BelCardParam {
	public:
		static shared_ptr<BelCardParamIdParam> create();
		static shared_ptr<BelCardParamIdParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardParamIdParam();
	};
	
	class BelCardTypeParam : public BelCardParam {
	public:
		static shared_ptr<BelCardTypeParam> create();
		static shared_ptr<BelCardTypeParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardTypeParam();
	};
	
	class BelCardMediaTypeParam : public BelCardParam {
	public:
		static shared_ptr<BelCardMediaTypeParam> create();
		static shared_ptr<BelCardMediaTypeParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardMediaTypeParam();
	};
	
	class BelCardCALSCALEParam : public BelCardParam {
	public:
		static shared_ptr<BelCardCALSCALEParam> create();
		static shared_ptr<BelCardCALSCALEParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardCALSCALEParam();
	};
	
	class BelCardSortAsParam : public BelCardParam {
	public:
		static shared_ptr<BelCardSortAsParam> create();
		static shared_ptr<BelCardSortAsParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardSortAsParam();
	};
	
	class BelCardGeoParam : public BelCardParam {
	public:
		static shared_ptr<BelCardGeoParam> create();
		static shared_ptr<BelCardGeoParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardGeoParam();
	};
	
	class BelCardTimezoneParam : public BelCardParam {
	public:
		static shared_ptr<BelCardTimezoneParam> create();
		static shared_ptr<BelCardTimezoneParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardTimezoneParam();
	};
	
	class BelCardLabelParam : public BelCardParam {
	public:
		static shared_ptr<BelCardLabelParam> create();
		static shared_ptr<BelCardLabelParam> parse(const string& input);
		static void setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser);
		
		BelCardLabelParam();
	};
}

#endif