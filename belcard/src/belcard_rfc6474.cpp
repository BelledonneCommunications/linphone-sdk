#include "belcard/belcard_rfc6474.hpp"

using namespace::std;
using namespace::belr;
using namespace::belcard;

shared_ptr<BelCardBirthPlace> BelCardBirthPlace::parse(const string& input) {
	return BelCardProperty::parseProperty<BelCardBirthPlace>("BIRTHPLACE", input);
}

void BelCardBirthPlace::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("BIRTHPLACE", make_fn(BelCardGeneric::create<BelCardBirthPlace>))
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
			->setCollector("LANGUAGE-param", make_sfn(&BelCardProperty::setLanguageParam))
			->setCollector("BIRTHPLACE-value", make_sfn(&BelCardProperty::setValue));
}

BelCardBirthPlace::BelCardBirthPlace() : BelCardProperty() {
	setName("BIRTHPLACE");
}

shared_ptr<BelCardDeathPlace> BelCardDeathPlace::parse(const string& input) {
	return BelCardProperty::parseProperty<BelCardDeathPlace>("DEATHPLACE", input);
}

void BelCardDeathPlace::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("DEATHPLACE", make_fn(BelCardGeneric::create<BelCardDeathPlace>))
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
			->setCollector("LANGUAGE-param", make_sfn(&BelCardProperty::setLanguageParam))
			->setCollector("DEATHPLACE-value", make_sfn(&BelCardProperty::setValue));
}

BelCardDeathPlace::BelCardDeathPlace() : BelCardProperty() {
	setName("DEATHPLACE");
}

shared_ptr<BelCardDeathDate> BelCardDeathDate::parse(const string& input) {
	return BelCardProperty::parseProperty<BelCardDeathDate>("DEATHDATE", input);
}

void BelCardDeathDate::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("DEATHDATE", make_fn(BelCardGeneric::create<BelCardDeathDate>))
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
			->setCollector("LANGUAGE-param", make_sfn(&BelCardProperty::setLanguageParam))
			->setCollector("CALSCALE-param", make_sfn(&BelCardProperty::setCALSCALEParam))
			->setCollector("DEATHDATE-value", make_sfn(&BelCardProperty::setValue));
}

BelCardDeathDate::BelCardDeathDate() : BelCardProperty() {
	setName("DEATHDATE");
}