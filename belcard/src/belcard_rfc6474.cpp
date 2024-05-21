/*
    belcard_rfc6474.cpp
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

shared_ptr<BelCardBirthPlace> BelCardBirthPlace::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardBirthPlace>("BIRTHPLACE", input, v3);
}

void BelCardBirthPlace::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (!v3) {
		parser->setHandler("BIRTHPLACE", make_fn(BelCardGeneric::create<BelCardBirthPlace>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
		    ->setCollector("LANGUAGE-param", make_sfn(&BelCardProperty::setLanguageParam))
		    ->setCollector("BIRTHPLACE-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardBirthPlace::BelCardBirthPlace(bool v3) : BelCardProperty(v3) {
	setName("BIRTHPLACE");
}

shared_ptr<BelCardDeathPlace> BelCardDeathPlace::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardDeathPlace>("DEATHPLACE", input, v3);
}

void BelCardDeathPlace::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (!v3) {
		parser->setHandler("DEATHPLACE", make_fn(BelCardGeneric::create<BelCardDeathPlace>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
		    ->setCollector("LANGUAGE-param", make_sfn(&BelCardProperty::setLanguageParam))
		    ->setCollector("DEATHPLACE-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardDeathPlace::BelCardDeathPlace(bool v3) : BelCardProperty(v3) {
	setName("DEATHPLACE");
}

shared_ptr<BelCardDeathDate> BelCardDeathDate::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardDeathDate>("DEATHDATE", input, v3);
}

void BelCardDeathDate::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (!v3) {
		parser->setHandler("DEATHDATE", make_fn(BelCardGeneric::create<BelCardDeathDate>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
		    ->setCollector("LANGUAGE-param", make_sfn(&BelCardProperty::setLanguageParam))
		    ->setCollector("CALSCALE-param", make_sfn(&BelCardProperty::setCALSCALEParam))
		    ->setCollector("DEATHDATE-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardDeathDate::BelCardDeathDate(bool v3) : BelCardProperty(v3) {
	setName("DEATHDATE");
}