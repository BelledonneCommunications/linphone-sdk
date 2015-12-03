#include "belcard/belcard_property.hpp"

using namespace::std;
using namespace::belr;
using namespace::belcard;

shared_ptr<BelCardProperty> BelCardProperty::create() {
	return BelCardGeneric::create<BelCardProperty>();
}

shared_ptr<BelCardProperty> BelCardProperty::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("X-PROPERTY", input, NULL);
	return dynamic_pointer_cast<BelCardProperty>(ret);
}

void BelCardProperty::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("X-PROPERTY", make_fn(&BelCardProperty::create))
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("X-PROPERTY-name", make_sfn(&BelCardProperty::setName))
			->setCollector("X-PROPERTY-value", make_sfn(&BelCardProperty::setValue));
}

BelCardProperty::BelCardProperty() : BelCardGeneric() {
	
}

void BelCardProperty::setGroup(const string &group) {
	_group = group;
}
const string &BelCardProperty::getGroup() const {
	return _group;
}

void BelCardProperty::setName(const string &name) {
	_name = name;
}
const string &BelCardProperty::getName() const {
	return _name;
}

void BelCardProperty::setValue(const string &value) {
	_value = value;
}
const string &BelCardProperty::getValue() const {
	return _value;
}

void BelCardProperty::setLanguageParam(const shared_ptr<BelCardLanguageParam> &param) {
	_lang_param = param;
	_params.push_back(_lang_param);
}
const shared_ptr<BelCardLanguageParam> &BelCardProperty::getLanguageParam() const {
	return _lang_param;
}

void BelCardProperty::setValueParam(const shared_ptr<BelCardValueParam> &param) {
	_value_param = param;
	_params.push_back(_value_param);
}
const shared_ptr<BelCardValueParam> &BelCardProperty::getValueParam() const {
	return _value_param;
}

void BelCardProperty::setPrefParam(const shared_ptr<BelCardPrefParam> &param) {
	_pref_param = param;
	_params.push_back(_pref_param);
}
const shared_ptr<BelCardPrefParam> &BelCardProperty::getPrefParam() const {
	return _pref_param;
}

void BelCardProperty::setAlternativeIdParam(const shared_ptr<BelCardAlternativeIdParam> &param) {
	_altid_param = param;
	_params.push_back(_altid_param);
}
const shared_ptr<BelCardAlternativeIdParam> &BelCardProperty::getAlternativeIdParam() const {
	return _altid_param;
}

void BelCardProperty::setParamIdParam(const shared_ptr<BelCardParamIdParam> &param) {
	_pid_param = param;
	_params.push_back(_pid_param);
}
const shared_ptr<BelCardParamIdParam> &BelCardProperty::getParamIdParam() const {
	return _pid_param;
}

void BelCardProperty::setTypeParam(const shared_ptr<BelCardTypeParam> &param) {
	_type_param = param;
	_params.push_back(_type_param);
}
const shared_ptr<BelCardTypeParam> &BelCardProperty::getTypeParam() const {
	return _type_param;
}

void BelCardProperty::setMediaTypeParam(const shared_ptr<BelCardMediaTypeParam> &param) {
	_mediatype_param = param;
	_params.push_back(_mediatype_param);
}
const shared_ptr<BelCardMediaTypeParam> &BelCardProperty::getMediaTypeParam() const {
	return _mediatype_param;
}

void BelCardProperty::setCALSCALEParam(const shared_ptr<BelCardCALSCALEParam> &param) {
	_calscale_param = param;
	_params.push_back(_calscale_param);
}
const shared_ptr<BelCardCALSCALEParam> &BelCardProperty::getCALSCALEParam() const {
	return _calscale_param;
}

void BelCardProperty::setSortAsParam(const shared_ptr<BelCardSortAsParam> &param) {
	_sort_as_param = param;
	_params.push_back(_sort_as_param);
}
const shared_ptr<BelCardSortAsParam> &BelCardProperty::getSortAsParam() const {
	return _sort_as_param;
}

void BelCardProperty::setGeoParam(const shared_ptr<BelCardGeoParam> &param) {
	_geo_param = param;
	_params.push_back(_geo_param);
}
const shared_ptr<BelCardGeoParam> &BelCardProperty::getGeoParam() const {
	return _geo_param;
}

void BelCardProperty::setTimezoneParam(const shared_ptr<BelCardTimezoneParam> &param) {
	_tz_param = param;
	_params.push_back(_tz_param);
}
const shared_ptr<BelCardTimezoneParam> &BelCardProperty::getTimezoneParam() const {
	return _tz_param;
}

void BelCardProperty::addParam(const shared_ptr<BelCardParam> &param) {
	_params.push_back(param);
}
const list<shared_ptr<BelCardParam>> &BelCardProperty::getParams() const {
	return _params;
}

string BelCardProperty::serialize() const {
	stringstream output;
	
	if (getGroup().length() > 0) {
		output << getGroup() << ".";
	}
	
	output << getName();
	for (auto it = getParams().begin(); it != getParams().end(); ++it) {
		output << ";" << (**it); 
	}
	output << ":" << getValue() << "\r\n";
	
	return output.str();
}