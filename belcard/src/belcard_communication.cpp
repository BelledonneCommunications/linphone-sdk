/*
    belcard_communication.cpp
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
#include <bctoolbox/parser.h>

using namespace ::std;
using namespace ::belr;
using namespace ::belcard;

shared_ptr<BelCardPhoneNumber> BelCardPhoneNumber::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardPhoneNumber>("TEL", input, v3);
}

void BelCardPhoneNumber::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (v3) {
		parser->setHandler("TEL", make_fn(BelCardGeneric::createV3<BelCardPhoneNumber>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
		    ->setCollector("TEL-value", make_sfn(&BelCardProperty::setValue));
	} else {
		parser->setHandler("TEL", make_fn(BelCardGeneric::create<BelCardPhoneNumber>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
		    ->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
		    ->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
		    ->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
		    ->setCollector("TEL-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardPhoneNumber::BelCardPhoneNumber(bool v3) : BelCardProperty(v3) {
	setName("TEL");
}

shared_ptr<BelCardEmail> BelCardEmail::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardEmail>("EMAIL", input, v3);
}

void BelCardEmail::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (v3) {
		parser->setHandler("EMAIL", make_fn(BelCardGeneric::createV3<BelCardEmail>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
		    ->setCollector("EMAIL-value", make_sfn(&BelCardProperty::setValue));
	} else {
		parser->setHandler("EMAIL", make_fn(BelCardGeneric::create<BelCardEmail>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
		    ->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
		    ->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
		    ->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
		    ->setCollector("EMAIL-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardEmail::BelCardEmail(bool v3) : BelCardProperty(v3) {
	setName("EMAIL");
}

shared_ptr<BelCardImpp> BelCardImpp::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardImpp>("IMPP", input, v3);
}

void BelCardImpp::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (v3) {
		parser->setHandler("IMPP", make_fn(BelCardGeneric::createV3<BelCardImpp>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
		    ->setCollector("IMPP-value", make_sfn(&BelCardProperty::setValue));
	} else {
		parser->setHandler("IMPP", make_fn(BelCardGeneric::create<BelCardImpp>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
		    ->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
		    ->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
		    ->setCollector("MEDIATYPE-param", make_sfn(&BelCardProperty::setMediaTypeParam))
		    ->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
		    ->setCollector("IMPP-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardImpp::BelCardImpp(bool v3) : BelCardProperty(v3) {
	setName("IMPP");
}

shared_ptr<BelCardLang> BelCardLang::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardLang>("LANG", input, v3);
}

void BelCardLang::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (!v3) {
		parser->setHandler("LANG", make_fn(BelCardGeneric::create<BelCardLang>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
		    ->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
		    ->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
		    ->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
		    ->setCollector("LANG-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardLang::BelCardLang(bool v3) : BelCardProperty(v3) {
	setName("LANG");
}

shared_ptr<BelCardMailer> BelCardMailer::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardMailer>("MAILER", input, v3);
}

void BelCardMailer::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (v3) {
		parser->setHandler("MAILER", make_fn(BelCardGeneric::createV3<BelCardMailer>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("MAILER-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardMailer::BelCardMailer(bool v3) : BelCardProperty(v3) {
	setName("MAILER");
}