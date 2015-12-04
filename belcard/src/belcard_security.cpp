#include "belcard/belcard_security.hpp"

using namespace::std;
using namespace::belr;
using namespace::belcard;

shared_ptr<BelCardKey> BelCardKey::parse(const string& input) {
	return BelCardProperty::parseProperty<BelCardKey>("KEY", input);
}

void BelCardKey::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser) {
	parser->setHandler("KEY", make_fn(BelCardGeneric::create<BelCardKey>))
			->setCollector("group", make_sfn(&BelCardProperty::setGroup))
			->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
			->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
			->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
			->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
			->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
			->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
			->setCollector("KEY-value", make_sfn(&BelCardProperty::setValue));
}

BelCardKey::BelCardKey() : BelCardProperty() {
	setName("KEY");
}