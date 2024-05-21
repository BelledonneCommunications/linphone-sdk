/*
    belcard_organizational.cpp
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

shared_ptr<BelCardTitle> BelCardTitle::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardTitle>("TITLE", input, v3);
}

void BelCardTitle::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (v3) {
		parser->setHandler("TITLE", make_fn(BelCardGeneric::createV3<BelCardTitle>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("TITLE-value", make_sfn(&BelCardProperty::setValue));
	} else {
		parser->setHandler("TITLE", make_fn(BelCardGeneric::create<BelCardTitle>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("LANGUAGE-param", make_sfn(&BelCardProperty::setLanguageParam))
		    ->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
		    ->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
		    ->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
		    ->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
		    ->setCollector("TITLE-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardTitle::BelCardTitle(bool v3) : BelCardProperty(v3) {
	setName("TITLE");
}

shared_ptr<BelCardRole> BelCardRole::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardRole>("ROLE", input, v3);
}

void BelCardRole::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (v3) {
		parser->setHandler("ROLE", make_fn(BelCardGeneric::createV3<BelCardRole>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("ROLE-value", make_sfn(&BelCardProperty::setValue));
	} else {
		parser->setHandler("ROLE", make_fn(BelCardGeneric::create<BelCardRole>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("LANGUAGE-param", make_sfn(&BelCardProperty::setLanguageParam))
		    ->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
		    ->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
		    ->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
		    ->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
		    ->setCollector("ROLE-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardRole::BelCardRole(bool v3) : BelCardProperty(v3) {
	setName("ROLE");
}

shared_ptr<BelCardLogo> BelCardLogo::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardLogo>("LOGO", input, v3);
}

void BelCardLogo::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (v3) {
		parser->setHandler("LOGO", make_fn(BelCardGeneric::createV3<BelCardLogo>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
		    ->setCollector("LOGO-value", make_sfn(&BelCardProperty::setValue));
	} else {
		parser->setHandler("LOGO", make_fn(BelCardGeneric::create<BelCardLogo>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("LANGUAGE-param", make_sfn(&BelCardProperty::setLanguageParam))
		    ->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
		    ->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
		    ->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
		    ->setCollector("MEDIATYPE-param", make_sfn(&BelCardProperty::setMediaTypeParam))
		    ->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
		    ->setCollector("LOGO-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardLogo::BelCardLogo(bool v3) : BelCardProperty(v3) {
	setName("LOGO");
}

shared_ptr<BelCardOrganization> BelCardOrganization::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardOrganization>("ORG", input, v3);
}

void BelCardOrganization::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (v3) {
		parser->setHandler("ORG", make_fn(BelCardGeneric::createV3<BelCardOrganization>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("ORG-value", make_sfn(&BelCardProperty::setValue));
	} else {
		parser->setHandler("ORG", make_fn(BelCardGeneric::create<BelCardOrganization>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("SORT-AS-param", make_sfn(&BelCardProperty::setSortAsParam))
		    ->setCollector("LANGUAGE-param", make_sfn(&BelCardProperty::setLanguageParam))
		    ->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
		    ->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
		    ->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
		    ->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
		    ->setCollector("ORG-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardOrganization::BelCardOrganization(bool v3) : BelCardProperty(v3) {
	setName("ORG");
}

shared_ptr<BelCardMember> BelCardMember::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardMember>("MEMBER", input, v3);
}

void BelCardMember::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (!v3) {
		parser->setHandler("MEMBER", make_fn(BelCardGeneric::create<BelCardMember>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
		    ->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
		    ->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
		    ->setCollector("MEDIATYPE-param", make_sfn(&BelCardProperty::setMediaTypeParam))
		    ->setCollector("MEMBER-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardMember::BelCardMember(bool v3) : BelCardProperty(v3) {
	setName("MEMBER");
}

shared_ptr<BelCardRelated> BelCardRelated::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardRelated>("RELATED", input, v3);
}

void BelCardRelated::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (!v3) {
		parser->setHandler("RELATED", make_fn(BelCardGeneric::create<BelCardRelated>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("any-param", make_sfn(&BelCardProperty::addParam))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("PID-param", make_sfn(&BelCardProperty::setParamIdParam))
		    ->setCollector("PREF-param", make_sfn(&BelCardProperty::setPrefParam))
		    ->setCollector("ALTID-param", make_sfn(&BelCardProperty::setAlternativeIdParam))
		    ->setCollector("TYPE-param", make_sfn(&BelCardProperty::setTypeParam))
		    ->setCollector("RELATED-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardRelated::BelCardRelated(bool v3) : BelCardProperty(v3) {
	setName("RELATED");
}

shared_ptr<BelCardAgent> BelCardAgent::parse(const string &input, bool v3) {
	return BelCardProperty::parseProperty<BelCardAgent>("AGENT", input, v3);
}

void BelCardAgent::setHandlerAndCollectors(Parser<shared_ptr<BelCardGeneric>> *parser, bool v3) {
	if (v3) {
		parser->setHandler("AGENT", make_fn(BelCardGeneric::create<BelCardAgent>))
		    ->setCollector("group", make_sfn(&BelCardProperty::setGroup))
		    ->setCollector("VALUE-param", make_sfn(&BelCardProperty::setValueParam))
		    ->setCollector("AGENT-value", make_sfn(&BelCardProperty::setValue));
	}
}

BelCardAgent::BelCardAgent(bool v3) : BelCardProperty(v3) {
	setName("AGENT");
}