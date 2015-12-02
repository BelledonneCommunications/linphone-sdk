#include "belcard/belcard_addressing.hpp"

using namespace::std;
using namespace::belr;
using namespace::belcard;

shared_ptr<BelCardAddress> BelCardAddress::create() {
	return make_shared<BelCardAddress>();
}

shared_ptr<BelCardAddress> BelCardAddress::parse(const string& input) {
	ABNFGrammarBuilder grammar_builder;
	shared_ptr<Grammar> grammar = grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	setHandlerAndCollectors(&parser);
	BelCardParam::setHandlerAndCollectors(&parser);
	shared_ptr<BelCardGeneric> ret = parser.parseInput("ADR", input, NULL);
	return dynamic_pointer_cast<BelCardAddress>(ret);
}

void BelCardAddress::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("ADR", make_fn(&BelCardAddress::create))
			->setCollector("group", make_sfn(&BelCardAddress::setGroup))
			->setCollector("any-param", make_sfn(&BelCardAddress::addParam))
			->setCollector("ADR-pobox", make_sfn(&BelCardAddress::setPostOfficeBox))
			->setCollector("ADR-ext", make_sfn(&BelCardAddress::setExtendedAddress))
			->setCollector("ADR-street", make_sfn(&BelCardAddress::setStreet))
			->setCollector("ADR-locality", make_sfn(&BelCardAddress::setLocality))
			->setCollector("ADR-region", make_sfn(&BelCardAddress::setRegion))
			->setCollector("ADR-code", make_sfn(&BelCardAddress::setPostalCode))
			->setCollector("ADR-country", make_sfn(&BelCardAddress::setCountry));
}

BelCardAddress::BelCardAddress() : BelCardProperty() {
	setName("ADR");
}

void BelCardAddress::setPostOfficeBox(const string &value) {
	_po_box = value;
}
const string &BelCardAddress::getPostOfficeBox() const {
	return _po_box;
}

void BelCardAddress::setExtendedAddress(const string &value) {
	_extended_address = value;
}
const string &BelCardAddress::getExtendedAddress() const {
	return _extended_address;
}

void BelCardAddress::setStreet(const string &value) {
	_street = value;
}
const string &BelCardAddress::getStreet() const {
	return _street;
}

void BelCardAddress::setLocality(const string &value) {
	_locality = value;
}
const string &BelCardAddress::getLocality() const {
	return _locality;
}

void BelCardAddress::setRegion(const string &value) {
	_region = value;
}
const string &BelCardAddress::getRegion() const {
	return _region;
}

void BelCardAddress::setPostalCode(const string &value) {
	_postal_code = value;
}
const string &BelCardAddress::getPostalCode() const {
	return _postal_code;
}

void BelCardAddress::setCountry(const string &value) {
	_country = value;
}
const string &BelCardAddress::getCountry() const {
	return _country;
}

string BelCardAddress::serialize() const {
	stringstream output;
	
	if (getGroup().length() > 0) {
		output << getGroup() << ".";
	}
	
	output << getName();
	for (auto it = getParams().begin(); it != getParams().end(); ++it) {
		output << ";" << (**it); 
	}
	output << ":" << getPostOfficeBox() << ";" << getExtendedAddress()
		<< ";" << getStreet() << ";" << getLocality() << ";" << getRegion() 
		<< ";" << getPostalCode() << ";" << getCountry() << "\r\n";
		
	return output.str();            
}