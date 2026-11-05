/*
 * Copyright (c) 2010-2026 Belledonne Communications SARL.
 *
 * This file is part of Liblinphone
 * (see https://gitlab.linphone.org/BC/public/liblinphone).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "vcard-context.h"
#include "vcard.h"

// =============================================================================

using namespace std;

LINPHONE_BEGIN_NAMESPACE

#ifdef VCARD_ENABLED

VcardContext::VcardContext() {
	lInfo() << "[vCard] Creating parser using 3.0 grammar (RFC 2425/2426)";
	mParserV3 = shared_ptr<belcard::BelCardParser>(belcard::BelCardParser::getInstance(true));
	lInfo() << "[vCard] Creating parser using 4.0 grammar (RFC 6350/6358)";
	mParserV4 = shared_ptr<belcard::BelCardParser>(belcard::BelCardParser::getInstance(false));
}

VcardContext *VcardContext::clone() const {
	return nullptr;
}

// -----------------------------------------------------------------------------

shared_ptr<Vcard> VcardContext::getVcardFromBuffer(const string &buffer) const {
	if (buffer.empty()) return nullptr;
	shared_ptr<belcard::BelCard> belCard = mParserV4->parseOne(buffer);
	if (belCard || (belCard = mParserV3->parseOne(buffer))) {
		return Vcard::create(belCard);
	}
	lError() << "[vCard] Couldn't parse buffer " << buffer;
	return nullptr;
}

vector<shared_ptr<Vcard>> VcardContext::getVcardListFromBuffer(const string &buffer) const {
	vector<shared_ptr<Vcard>> result;
	if (!buffer.empty()) {
		// We assume that each file or buffer contains data in a single version.
		// This way, the parser either successfully parses the entire content or fails to return any results.
		// An empty result indicates either that the input contains no data or that it uses an unsupported version.
		// In that case, the parser retries using an older version as a fallback.
		// This limitation comes from grammar rules that stop parsing if VERSION doesn't match exactly.
		shared_ptr<belcard::BelCardList> belCards = mParserV4->parse(buffer);
		if (!belCards) belCards = mParserV3->parse(buffer);
		if (belCards) {
			auto cards = belCards->getCards();
			result.reserve(cards.size());
			for (const auto &belCard : cards)
				result.push_back(Vcard::create(belCard));
		}
	}
	return result;
}

vector<shared_ptr<Vcard>> VcardContext::getVcardListFromFile(const string &filename) const {
	vector<shared_ptr<Vcard>> result;
	if (!filename.empty()) {
		shared_ptr<belcard::BelCardList> belCards = mParserV4->parseFile(filename);
		if (!belCards) belCards = mParserV3->parseFile(filename);
		if (belCards) {
			auto cards = belCards->getCards();
			result.reserve(cards.size());
			for (const auto &belCard : cards)
				result.push_back(Vcard::create(belCard));
		}
	}
	return result;
}

#else

VcardContext::VcardContext() {
}

VcardContext *VcardContext::clone() const {
	return nullptr;
}

// -----------------------------------------------------------------------------

shared_ptr<Vcard> VcardContext::getVcardFromBuffer(BCTBX_UNUSED(const string &buffer)) const {
	return nullptr;
}

vector<shared_ptr<Vcard>> VcardContext::getVcardListFromBuffer(BCTBX_UNUSED(const string &buffer)) const {
	return {};
}

vector<shared_ptr<Vcard>> VcardContext::getVcardListFromFile(BCTBX_UNUSED(const string &filename)) const {
	return {};
}

#endif /* VCARD_ENABLED */

LINPHONE_END_NAMESPACE
