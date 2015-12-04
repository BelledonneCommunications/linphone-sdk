#include "belcard/belcard_params.hpp"

using namespace::std;
using namespace::belr;
using namespace::belcard;

template <typename T>
shared_ptr<T> BelCardParam::parseParam(const string& rule, const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	T::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput(rule, input, NULL);
	return dynamic_pointer_cast<T>(ret);
}

shared_ptr<BelCardParam> BelCardParam::parse(const string& input) {
	return BelCardParam::parseParam<BelCardParam>("any-param", input);
}

void BelCardParam::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("any-param", make_fn(BelCardGeneric::create<BelCardParam>))
			->setCollector("param-name", make_sfn(&BelCardParam::setName))
			->setCollector("param-value", make_sfn(&BelCardParam::setValue));
}

void BelCardParam::setAllParamsHandlersAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	BelCardParam::setHandlerAndCollectors(parser);
	BelCardLabelParam::setHandlerAndCollectors(parser);
	BelCardValueParam::setHandlerAndCollectors(parser);
	BelCardPrefParam::setHandlerAndCollectors(parser);
	BelCardAlternativeIdParam::setHandlerAndCollectors(parser);
	BelCardParamIdParam::setHandlerAndCollectors(parser);
	BelCardTypeParam::setHandlerAndCollectors(parser);
	BelCardMediaTypeParam::setHandlerAndCollectors(parser);
	BelCardCALSCALEParam::setHandlerAndCollectors(parser);
	BelCardSortAsParam::setHandlerAndCollectors(parser);
	BelCardGeoParam::setHandlerAndCollectors(parser);
	BelCardTimezoneParam::setHandlerAndCollectors(parser);
	BelCardLabelParam::setHandlerAndCollectors(parser);
}

BelCardParam::BelCardParam() : BelCardGeneric() {
	
}

void BelCardParam::setName(const string &name) {
	_name = name;
}
const string &BelCardParam::getName() const {
	return _name;
}

void BelCardParam::setValue(const string &value) {
	_value = value;
}
const string &BelCardParam::getValue() const {
	return _value;
}

string BelCardParam::serialize() const {
	stringstream output;
	output << *this;
	return output.str();
}

shared_ptr<BelCardLanguageParam> BelCardLanguageParam::parse(const string& input) {
	return BelCardParam::parseParam<BelCardLanguageParam>("LANGUAGE-param", input);
}

void BelCardLanguageParam::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("LANGUAGE-param", make_fn(BelCardGeneric::create<BelCardLanguageParam>))
			->setCollector("LANGUAGE-param-value", make_sfn(&BelCardLanguageParam::setValue));
}

BelCardLanguageParam::BelCardLanguageParam() : BelCardParam() {
	setName("LANGUAGE");
}

shared_ptr<BelCardValueParam> BelCardValueParam::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("VALUE-param", input, NULL);
	return dynamic_pointer_cast<BelCardValueParam>(ret);
}

void BelCardValueParam::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("VALUE-param", make_fn(BelCardGeneric::create<BelCardValueParam>))
			->setCollector("VALUE-param-value", make_sfn(&BelCardValueParam::setValue));
}

BelCardValueParam::BelCardValueParam() : BelCardParam() {
	setName("VALUE");
}

shared_ptr<BelCardPrefParam> BelCardPrefParam::parse(const string& input) {
	return BelCardParam::parseParam<BelCardPrefParam>("PREF-param", input);
}

void BelCardPrefParam::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("PREF-param", make_fn(BelCardGeneric::create<BelCardPrefParam>))
			->setCollector("PREF-param-value", make_sfn(&BelCardPrefParam::setValue));
}

BelCardPrefParam::BelCardPrefParam() : BelCardParam() {
	setName("PREF");
}

shared_ptr<BelCardAlternativeIdParam> BelCardAlternativeIdParam::parse(const string& input) {
	return BelCardParam::parseParam<BelCardAlternativeIdParam>("ALTID-param", input);
}

void BelCardAlternativeIdParam::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("ALTID-param", make_fn(BelCardGeneric::create<BelCardAlternativeIdParam>))
			->setCollector("ALTID-param-value", make_sfn(&BelCardAlternativeIdParam::setValue));
}

BelCardAlternativeIdParam::BelCardAlternativeIdParam() : BelCardParam() {
	setName("ALTID");
}

shared_ptr<BelCardParamIdParam> BelCardParamIdParam::parse(const string& input) {
	return BelCardParam::parseParam<BelCardParamIdParam>("PID-param", input);
}

void BelCardParamIdParam::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("PID-param", make_fn(BelCardGeneric::create<BelCardParamIdParam>))
			->setCollector("PID-param-value", make_sfn(&BelCardParamIdParam::setValue));
}

BelCardParamIdParam::BelCardParamIdParam() : BelCardParam() {
	setName("PID");
}

shared_ptr<BelCardTypeParam> BelCardTypeParam::parse(const string& input) {
	return BelCardParam::parseParam<BelCardTypeParam>("TYPE-param", input);
}

void BelCardTypeParam::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("TYPE-param", make_fn(BelCardGeneric::create<BelCardTypeParam>))
			->setCollector("TYPE-param-value", make_sfn(&BelCardTypeParam::setValue));
}

BelCardTypeParam::BelCardTypeParam() : BelCardParam() {
	setName("TYPE");
}

shared_ptr<BelCardMediaTypeParam> BelCardMediaTypeParam::parse(const string& input) {
	return BelCardParam::parseParam<BelCardMediaTypeParam>("MEDIATYPE-param", input);
}

void BelCardMediaTypeParam::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("MEDIATYPE-param", make_fn(BelCardGeneric::create<BelCardMediaTypeParam>))
			->setCollector("MEDIATYPE-param-value", make_sfn(&BelCardMediaTypeParam::setValue));
}

BelCardMediaTypeParam::BelCardMediaTypeParam() : BelCardParam() {
	setName("MEDIATYPE");
}

shared_ptr<BelCardCALSCALEParam> BelCardCALSCALEParam::parse(const string& input) {
	return BelCardParam::parseParam<BelCardCALSCALEParam>("CALSCALE-param", input);
}

void BelCardCALSCALEParam::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("CALSCALE-param", make_fn(BelCardGeneric::create<BelCardCALSCALEParam>))
			->setCollector("CALSCALE-param-value", make_sfn(&BelCardCALSCALEParam::setValue));
}

BelCardCALSCALEParam::BelCardCALSCALEParam() : BelCardParam() {
	setName("CALSCALE");
}

shared_ptr<BelCardSortAsParam> BelCardSortAsParam::parse(const string& input) {
	return BelCardParam::parseParam<BelCardSortAsParam>("SORT-AS-param", input);
}

void BelCardSortAsParam::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("SORT-AS-param", make_fn(BelCardGeneric::create<BelCardSortAsParam>))
			->setCollector("SORT-AS-param-value", make_sfn(&BelCardSortAsParam::setValue));
}

BelCardSortAsParam::BelCardSortAsParam() : BelCardParam() {
	setName("SORT-AS");
}

shared_ptr<BelCardGeoParam> BelCardGeoParam::parse(const string& input) {
	return BelCardParam::parseParam<BelCardGeoParam>("GEO-PARAM-param", input);
}

void BelCardGeoParam::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("GEO-PARAM-param", make_fn(BelCardGeneric::create<BelCardGeoParam>))
			->setCollector("GEO-PARAM-param-value", make_sfn(&BelCardGeoParam::setValue));
}

BelCardGeoParam::BelCardGeoParam() : BelCardParam() {
	setName("GEO");
}

shared_ptr<BelCardTimezoneParam> BelCardTimezoneParam::parse(const string& input) {
	return BelCardParam::parseParam<BelCardTimezoneParam>("TZ-PARAM-param", input);
}

void BelCardTimezoneParam::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("TZ-PARAM-param", make_fn(BelCardGeneric::create<BelCardTimezoneParam>))
			->setCollector("TZ-PARAM-param-value", make_sfn(&BelCardTimezoneParam::setValue));
}

BelCardTimezoneParam::BelCardTimezoneParam() : BelCardParam() {
	setName("TZ");
}

shared_ptr<BelCardLabelParam> BelCardLabelParam::parse(const string& input) {
	return BelCardParam::parseParam<BelCardLabelParam>("LABEL-param", input);
}

void BelCardLabelParam::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("LABEL-param", make_fn(BelCardGeneric::create<BelCardLabelParam>))
			->setCollector("LABEL-param-value", make_sfn(&BelCardLabelParam::setValue));
}

BelCardLabelParam::BelCardLabelParam() : BelCardParam() {
	setName("LABEL");
}