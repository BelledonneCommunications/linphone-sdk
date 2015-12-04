#include "belcard/belcard_calendar.hpp"

using namespace::std;
using namespace::belr;
using namespace::belcard;

shared_ptr<BelCardFBURL> BelCardFBURL::parse(const string& input) {
	return BelCardProperty::parseProperty<BelCardFBURL>("FBURL", input);
}

void BelCardFBURL::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("FBURL", make_fn(BelCardGeneric::create<BelCardFBURL>))
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
			->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
			->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
			->setCollector("MEDIATYPE-param", make_sfn(&BelCardProperty::setMediaTypeParam))
			->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
			->setCollector("FBURL-value", make_sfn(&BelCardProperty::setValue));
}

BelCardFBURL::BelCardFBURL() : BelCardProperty() {
	setName("FBURL");
}

shared_ptr<BelCardCALADRURI> BelCardCALADRURI::parse(const string& input) {
	return BelCardProperty::parseProperty<BelCardCALADRURI>("CALADRURI", input);
}

void BelCardCALADRURI::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("CALADRURI", make_fn(BelCardGeneric::create<BelCardCALADRURI>))
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
			->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
			->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
			->setCollector("MEDIATYPE-param", make_sfn(&BelCardProperty::setMediaTypeParam))
			->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
			->setCollector("CALADRURI-value", make_sfn(&BelCardProperty::setValue));
}

BelCardCALADRURI::BelCardCALADRURI() : BelCardProperty() {
	setName("CALADRURI");
}

shared_ptr<BelCardCALURI> BelCardCALURI::parse(const string& input) {
	return BelCardProperty::parseProperty<BelCardCALURI>("CALURI", input);
}

void BelCardCALURI::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("CALURI", make_fn(BelCardGeneric::create<BelCardCALURI>))
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
			->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
			->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
			->setCollector("MEDIATYPE-param", make_sfn(&BelCardProperty::setMediaTypeParam))
			->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
			->setCollector("CALURI-value", make_sfn(&BelCardProperty::setValue));
}

BelCardCALURI::BelCardCALURI() : BelCardProperty() {
	setName("CALURI");
}