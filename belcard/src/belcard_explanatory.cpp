/*
    belcard_explanatory.cpp
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

shared_ptr<BelCardCategories> BelCardCategories::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardCategories>("CATEGORIES", input, v3);
}

void BelCardCategories::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (v3) {
		parser->setHandler("CATEGORIES", make_fn(BelCardGeneric::createV3<BelCardCategories>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("CATEGORIES-value", make_sfn(&BelCardProperty::setValue));
	} else {
		parser->setHandler("CATEGORIES", make_fn(BelCardGeneric::create<BelCardCategories>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
		    ->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
		    ->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
		    ->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
		    ->setCollector("CATEGORIES-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardCategories::BelCardCategories(bool v3) : BelCardProperty(v3) {
	setName("CATEGORIES");
}

shared_ptr<BelCardNote> BelCardNote::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardNote>("NOTE", input, v3);
}

void BelCardNote::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (v3) {
		parser->setHandler("NOTE", make_fn(BelCardGeneric::createV3<BelCardNote>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("NOTE-value", make_sfn(&BelCardProperty::setValue));
	} else {
		parser->setHandler("NOTE", make_fn(BelCardGeneric::create<BelCardNote>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("LANGUAGE-param", make_sfn(&BelCardProperty::setLanguageParam))
		    ->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
		    ->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
		    ->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
		    ->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
		    ->setCollector("NOTE-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardNote::BelCardNote(bool v3) : BelCardProperty(v3) {
	setName("NOTE");
}

shared_ptr<BelCardProductId> BelCardProductId::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardProductId>("PRODID", input, v3);
}

void BelCardProductId::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (v3) {
		parser->setHandler("PRODID", make_fn(BelCardGeneric::createV3<BelCardProductId>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("PRODID-value", make_sfn(&BelCardProperty::setValue));
	} else {
		parser->setHandler("PRODID", make_fn(BelCardGeneric::create<BelCardProductId>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("PRODID-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardProductId::BelCardProductId(bool v3) : BelCardProperty(v3) {
	setName("PRODID");
}

shared_ptr<BelCardRevision> BelCardRevision::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardRevision>("REV", input, v3);
}

void BelCardRevision::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (v3) {
		parser->setHandler("REV", make_fn(BelCardGeneric::createV3<BelCardRevision>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("REV-value", make_sfn(&BelCardProperty::setValue));
	} else {
		parser->setHandler("REV", make_fn(BelCardGeneric::create<BelCardRevision>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("REV-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardRevision::BelCardRevision(bool v3) : BelCardProperty(v3) {
	setName("REV");
}

shared_ptr<BelCardSound> BelCardSound::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardSound>("SOUND", input, v3);
}

void BelCardSound::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (v3) {
		parser->setHandler("SOUND", make_fn(BelCardGeneric::createV3<BelCardSound>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
		    ->setCollector("SOUND-value", make_sfn(&BelCardProperty::setValue));
	} else {
		parser->setHandler("SOUND", make_fn(BelCardGeneric::create<BelCardSound>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("LANGUAGE-param", make_sfn(&BelCardProperty::setLanguageParam))
		    ->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
		    ->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
		    ->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
		    ->setCollector("MEDIATYPE-param", make_sfn(&BelCardProperty::setMediaTypeParam))
		    ->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
		    ->setCollector("SOUND-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardSound::BelCardSound(bool v3) : BelCardProperty(v3) {
	setName("SOUND");
}

shared_ptr<BelCardUniqueId> BelCardUniqueId::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardUniqueId>("UID", input, v3);
}

void BelCardUniqueId::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (v3) {
		parser->setHandler("UID", make_fn(BelCardGeneric::createV3<BelCardUniqueId>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("TYPE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("UID-value", make_sfn(&BelCardProperty::setValue));
	} else {
		parser->setHandler("UID", make_fn(BelCardGeneric::create<BelCardUniqueId>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("UID-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardUniqueId::BelCardUniqueId(bool v3) : BelCardProperty(v3) {
	setName("UID");
}

shared_ptr<BelCardClientProductIdMap> BelCardClientProductIdMap::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardClientProductIdMap>("CLIENTPIDMAP", input, v3);
}

void BelCardClientProductIdMap::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (!v3) {
		parser->setHandler("CLIENTPIDMAP", make_fn(BelCardGeneric::create<BelCardClientProductIdMap>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("CLIENTPIDMAP-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardClientProductIdMap::BelCardClientProductIdMap(bool v3) : BelCardProperty(v3) {
	setName("CLIENTPIDMAP");
}

shared_ptr<BelCardURL> BelCardURL::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardURL>("URL", input, v3);
}

void BelCardURL::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (v3) {
		parser->setHandler("URL", make_fn(BelCardGeneric::createV3<BelCardURL>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("URL-value", make_sfn(&BelCardProperty::setValue));
	} else {
		parser->setHandler("URL", make_fn(BelCardGeneric::create<BelCardURL>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
		    ->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
		    ->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
		    ->setCollector("MEDIATYPE-param", make_sfn(&BelCardProperty::setMediaTypeParam))
		    ->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
		    ->setCollector("URL-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardURL::BelCardURL(bool v3) : BelCardProperty(v3) {
	setName("URL");
}