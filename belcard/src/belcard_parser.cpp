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
#include "belcard/vcard_grammar.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

using namespace ::std;
using namespace ::belr;
using namespace ::belcard;

shared_ptr<BelCardParser> BelCardParser::getInstance(bool useVCard3Grammar) {
	static shared_ptr<BelCardParser> parser(new BelCardParser(useVCard3Grammar));
	return parser;
}

BelCardParser::BelCardParser(bool useVCard3Grammar) {
	_v3 = useVCard3Grammar;

	if (_v3) {
		bctbx_warning("[BelCard] Loading vCard 3.0 grammar");
		shared_ptr<Grammar> grammar = loadVcard3Grammar();
		_parser = new Parser<shared_ptr<BelCardGeneric>>(grammar);
	} else {
		bctbx_message("[BelCard] Loading vCard 4.0 grammar");
		shared_ptr<Grammar> grammar = loadVcardGrammar();
		_parser = new Parser<shared_ptr<BelCardGeneric>>(grammar);
	}

	BelCardList::setHandlerAndCollectors(_parser);

	BelCard::setHandlerAndCollectors(_parser, _v3);

	BelCardParam::setHandlerAndCollectors(_parser, _v3);
	BelCardLabelParam::setHandlerAndCollectors(_parser, _v3);
	BelCardValueParam::setHandlerAndCollectors(_parser, _v3);
	BelCardPrefParam::setHandlerAndCollectors(_parser, _v3);
	BelCardAlternativeIdParam::setHandlerAndCollectors(_parser, _v3);
	BelCardParamIdParam::setHandlerAndCollectors(_parser, _v3);
	BelCardTypeParam::setHandlerAndCollectors(_parser, _v3);
	BelCardMediaTypeParam::setHandlerAndCollectors(_parser, _v3);
	BelCardCALSCALEParam::setHandlerAndCollectors(_parser, _v3);
	BelCardSortAsParam::setHandlerAndCollectors(_parser, _v3);
	BelCardGeoParam::setHandlerAndCollectors(_parser, _v3);
	BelCardTimezoneParam::setHandlerAndCollectors(_parser, _v3);
	BelCardLabelParam::setHandlerAndCollectors(_parser, _v3);

	BelCardProperty::setHandlerAndCollectors(_parser, _v3);
	BelCardSource::setHandlerAndCollectors(_parser, _v3);
	BelCardKind::setHandlerAndCollectors(_parser, _v3);
	BelCardXML::setHandlerAndCollectors(_parser, _v3);

	BelCardFullName::setHandlerAndCollectors(_parser, _v3);
	BelCardName::setHandlerAndCollectors(_parser, _v3);
	BelCardNickname::setHandlerAndCollectors(_parser, _v3);
	BelCardPhoto::setHandlerAndCollectors(_parser, _v3);
	BelCardDisplayName::setHandlerAndCollectors(_parser, _v3);
	BelCardSortString::setHandlerAndCollectors(_parser, _v3);
	BelCardBirthday::setHandlerAndCollectors(_parser, _v3);
	BelCardAnniversary::setHandlerAndCollectors(_parser, _v3);
	BelCardGender::setHandlerAndCollectors(_parser, _v3);

	BelCardAddress::setHandlerAndCollectors(_parser, _v3);
	BelCardAddressLabel::setHandlerAndCollectors(_parser, _v3);

	BelCardPhoneNumber::setHandlerAndCollectors(_parser, _v3);
	BelCardEmail::setHandlerAndCollectors(_parser, _v3);
	BelCardImpp::setHandlerAndCollectors(_parser, _v3);
	BelCardLang::setHandlerAndCollectors(_parser, _v3);
	BelCardMailer::setHandlerAndCollectors(_parser, _v3);

	BelCardTimezone::setHandlerAndCollectors(_parser, _v3);
	BelCardGeo::setHandlerAndCollectors(_parser, _v3);

	BelCardTitle::setHandlerAndCollectors(_parser, _v3);
	BelCardRole::setHandlerAndCollectors(_parser, _v3);
	BelCardLogo::setHandlerAndCollectors(_parser, _v3);
	BelCardOrganization::setHandlerAndCollectors(_parser, _v3);
	BelCardMember::setHandlerAndCollectors(_parser, _v3);
	BelCardRelated::setHandlerAndCollectors(_parser, _v3);
	BelCardAgent::setHandlerAndCollectors(_parser, _v3);

	BelCardCategories::setHandlerAndCollectors(_parser, _v3);
	BelCardNote::setHandlerAndCollectors(_parser, _v3);
	BelCardProductId::setHandlerAndCollectors(_parser, _v3);
	BelCardRevision::setHandlerAndCollectors(_parser, _v3);
	BelCardSound::setHandlerAndCollectors(_parser, _v3);
	BelCardUniqueId::setHandlerAndCollectors(_parser, _v3);
	BelCardClientProductIdMap::setHandlerAndCollectors(_parser, _v3);
	BelCardURL::setHandlerAndCollectors(_parser, _v3);

	BelCardKey::setHandlerAndCollectors(_parser, _v3);
	BelCardClass::setHandlerAndCollectors(_parser, _v3);

	BelCardFBURL::setHandlerAndCollectors(_parser, _v3);
	BelCardCALADRURI::setHandlerAndCollectors(_parser, _v3);
	BelCardCALURI::setHandlerAndCollectors(_parser, _v3);

	BelCardBirthPlace::setHandlerAndCollectors(_parser, _v3);
	BelCardDeathPlace::setHandlerAndCollectors(_parser, _v3);
	BelCardDeathDate::setHandlerAndCollectors(_parser, _v3);
}

BelCardParser::~BelCardParser() {
	delete _parser;
}

bool BelCardParser::isUsingV3Grammar() const {
	return _v3;
}

shared_ptr<BelCardGeneric> BelCardParser::_parse(const string &input, const string &rule) {
	size_t parsedSize = 0;
	shared_ptr<BelCardGeneric> ret = _parser->parseInput(rule, input, &parsedSize);
	if (parsedSize < input.size()) {
		bctbx_error("[BelCard] Parsing ended prematuraly at pos %llu", (unsigned long long)parsedSize);
	}
	return ret;
}

shared_ptr<BelCard> BelCardParser::parseOne(const string &input) {
	string vCard = belcard_unfold(input);
	shared_ptr<BelCardGeneric> ret = _parse(vCard, "vcard");
	shared_ptr<BelCard> belCard = dynamic_pointer_cast<BelCard>(ret);
	return belCard;
}

shared_ptr<BelCardList> BelCardParser::parse(const string &input) {
	string vCards = belcard_unfold(input);
	shared_ptr<BelCardGeneric> ret = _parse(vCards, "vcard-list");
	shared_ptr<BelCardList> belCards = dynamic_pointer_cast<BelCardList>(ret);
	return belCards;
}

shared_ptr<BelCardList> BelCardParser::parseFile(const string &filename) {
	string file = belcard_read_file(filename);
	shared_ptr<BelCardList> belCards = parse(file);
	return belCards;
}
