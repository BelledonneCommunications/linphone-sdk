#include "belcard/belcard_general.hpp"

using namespace::std;
using namespace::belr;
using namespace::belcard;

shared_ptr<BelCardSource> BelCardSource::parse(const string& input) {
	return BelCardProperty::parseProperty<BelCardSource>("SOURCE", input);
}

void BelCardSource::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("SOURCE", make_fn(BelCardGeneric::create<BelCardSource>))
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
			->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
			->setCollector("MEDIATYPE-param", make_sfn(&BelCardProperty::setMediaTypeParam))
			->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
			->setCollector("SOURCE-value", make_sfn(&BelCardProperty::setValue));
}

BelCardSource::BelCardSource() : BelCardProperty() {
	setName("SOURCE");
}

shared_ptr<BelCardKind> BelCardKind::parse(const string& input) {
	return BelCardProperty::parseProperty<BelCardKind>("KIND", input);
}

void BelCardKind::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("KIND", make_fn(BelCardGeneric::create<BelCardKind>))
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("KIND-value", make_sfn(&BelCardProperty::setValue));
}

BelCardKind::BelCardKind() : BelCardProperty() {
	setName("KIND");
}

shared_ptr<BelCardXML> BelCardXML::parse(const string& input) {
	return BelCardProperty::parseProperty<BelCardXML>("XML", input);
}

void BelCardXML::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("XML", make_fn(BelCardGeneric::create<BelCardXML>))
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("XML-value", make_sfn(&BelCardProperty::setValue));
}

BelCardXML::BelCardXML() : BelCardProperty() {
	setName("XML");
}