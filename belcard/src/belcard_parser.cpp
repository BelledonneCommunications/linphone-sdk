/*
	belcard_parser.cpp
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

#include "belcard/belcard_parser.hpp"
#include "belcard/belcard.hpp"
#include "belcard/belcard_utils.hpp"

#include <iostream>
#include <fstream>
#include <sstream>

using namespace::std;
using namespace::belr;
using namespace::belcard;

BelCardParser::BelCardParser() {
	_grammar = _grammar_builder.createFromAbnf((const char*)vcard_grammar, make_shared<CoreRules>());
}

BelCardParser::~BelCardParser() {
	
}

shared_ptr<BelCardGeneric> BelCardParser::_parse(const string &input, const string &rule) {
	Parser<shared_ptr<BelCardGeneric>> parser(_grammar);
	
	BelCardList::setHandlerAndCollectors(&parser);
	BelCard::setHandlerAndCollectors(&parser);
	BelCardParam::setAllParamsHandlersAndCollectors(&parser);
	BelCardProperty::setHandlerAndCollectors(&parser);
	
	BelCardSource::setHandlerAndCollectors(&parser);
	BelCardKind::setHandlerAndCollectors(&parser);
	BelCardXML::setHandlerAndCollectors(&parser);
	
	BelCardFullName::setHandlerAndCollectors(&parser);
	BelCardName::setHandlerAndCollectors(&parser);
	BelCardNickname::setHandlerAndCollectors(&parser);
	BelCardPhoto::setHandlerAndCollectors(&parser);
	BelCardBirthday::setHandlerAndCollectors(&parser);
	BelCardAnniversary::setHandlerAndCollectors(&parser);
	BelCardGender::setHandlerAndCollectors(&parser);
	
	BelCardAddress::setHandlerAndCollectors(&parser);
	
	BelCardPhoneNumber::setHandlerAndCollectors(&parser);
	BelCardEmail::setHandlerAndCollectors(&parser);
	BelCardImpp::setHandlerAndCollectors(&parser);
	BelCardLang::setHandlerAndCollectors(&parser);
	
	BelCardTimezone::setHandlerAndCollectors(&parser);
	BelCardGeo::setHandlerAndCollectors(&parser);
	
	BelCardTitle::setHandlerAndCollectors(&parser);
	BelCardRole::setHandlerAndCollectors(&parser);
	BelCardLogo::setHandlerAndCollectors(&parser);
	BelCardOrganization::setHandlerAndCollectors(&parser);
	BelCardMember::setHandlerAndCollectors(&parser);
	BelCardRelated::setHandlerAndCollectors(&parser);
	
	BelCardCategories::setHandlerAndCollectors(&parser);
	BelCardNote::setHandlerAndCollectors(&parser);
	BelCardProductId::setHandlerAndCollectors(&parser);
	BelCardRevision::setHandlerAndCollectors(&parser);
	BelCardSound::setHandlerAndCollectors(&parser);
	BelCardUniqueId::setHandlerAndCollectors(&parser);
	BelCardClientProductIdMap::setHandlerAndCollectors(&parser);
	BelCardURL::setHandlerAndCollectors(&parser);
	
	BelCardKey::setHandlerAndCollectors(&parser);
	
	BelCardFBURL::setHandlerAndCollectors(&parser);
	BelCardCALADRURI::setHandlerAndCollectors(&parser);
	BelCardCALURI::setHandlerAndCollectors(&parser);
	
	BelCardBirthPlace::setHandlerAndCollectors(&parser);
	BelCardDeathPlace::setHandlerAndCollectors(&parser);
	BelCardDeathDate::setHandlerAndCollectors(&parser);
		
	size_t parsedSize = 0;
	shared_ptr<BelCardGeneric> ret = parser.parseInput(rule, input, &parsedSize);
	return ret;
}

shared_ptr<BelCard> BelCardParser::parseOne(const string &input) {
	string vcard = belcard_unfold(input);
	shared_ptr<BelCardGeneric> ret = _parse(vcard, "vcard");
	shared_ptr<BelCard> belCard = dynamic_pointer_cast<BelCard>(ret);
	return belCard;
}

shared_ptr<BelCardList> BelCardParser::parse(const string &input) {
	string vcards = belcard_unfold(input);
	shared_ptr<BelCardGeneric> ret = _parse(vcards, "vcard-list");
	shared_ptr<BelCardList> belCards = dynamic_pointer_cast<BelCardList>(ret);
	return belCards;
}

shared_ptr<BelCardList> BelCardParser::parseFile(const string &filename) {
	ifstream istr(filename);
	if (!istr.is_open()) {
		return NULL;
	}
	
	stringstream vcardStream;
	vcardStream << istr.rdbuf();
	string vcard = vcardStream.str();
	
	string vcards = belcard_unfold(vcard);
	shared_ptr<BelCardGeneric> ret = _parse(vcards, "vcard-list");
	shared_ptr<BelCardList> belCards = dynamic_pointer_cast<BelCardList>(ret);
	return belCards;
}

BelCardParser& BelCardParser::getInstance() {
	static BelCardParser *instance = new BelCardParser();
	return *instance;
}
