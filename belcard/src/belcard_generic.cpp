#include "belcard/belcard_generic.hpp"

using namespace::std;
using namespace::belr;
using namespace::belcard;

shared_ptr<BelCardParam> BelCardParam::create() {
	return BelCardGeneric::create<BelCardParam>();
}

shared_ptr<BelCardParam> BelCardParam::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("any-param", input, NULL);
	return dynamic_pointer_cast<BelCardParam>(ret);
}

void BelCardParam::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("any-param", make_fn(&BelCardParam::create))
			->setCollector("param-name", make_sfn(&BelCardParam::setName))
			->setCollector("param-value", make_sfn(&BelCardParam::setValue));
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
	output << getName() << "=" << getValue();
	return output.str();
}

shared_ptr<BelCardProperty> BelCardProperty::create() {
	return BelCardGeneric::create<BelCardProperty>();
}

shared_ptr<BelCardProperty> BelCardProperty::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
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