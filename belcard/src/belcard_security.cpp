/*
    belcard_security.cpp
    Copyright (C) 2015  Belledonne Communications SARL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "belcard/belcard.hpp"

using namespace ::std;
using namespace ::belr;
using namespace ::belcard;

shared_ptr<BelCardKey> BelCardKey::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardKey>("KEY", input, v3);
}

void BelCardKey::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (v3) {
		parser->setHandler("KEY", make_fn(BelCardGeneric::createV3<BelCardKey>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("KEY-value", make_sfn(&BelCardProperty::setValue));
	} else {
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
}

BelCardKey::BelCardKey(bool v3) : BelCardProperty(v3) {
	setName("KEY");
}

shared_ptr<BelCardClass> BelCardClass::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardClass>("CLASS", input, v3);
}

void BelCardClass::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (v3) {
		parser->setHandler("CLASS", make_fn(BelCardGeneric::createV3<BelCardClass>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("CLASS-VALUE", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardClass::BelCardClass(bool v3) : BelCardProperty(v3) {
	setName("CLASS");
}