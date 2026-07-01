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

VcardContext::VcardContext(bool useVCard3Grammar) {
	if (useVCard3Grammar) {
		lInfo() << "[vCard] Creating parser using 3.0 grammar (RFC 2425/2426)";
	} else {
		lInfo() << "[vCard] Creating parser using 4.0 grammar (RFC 6350/6358)";
	}
	mParser = shared_ptr<belcard::BelCardParser>(belcard::BelCardParser::getInstance(useVCard3Grammar));
}

VcardContext *VcardContext::clone() const {
	return nullptr;
}

// -----------------------------------------------------------------------------

shared_ptr<Vcard> VcardContext::getVcardFromBuffer(const string &buffer) const {
	if (buffer.empty()) {
		return nullptr;
	}

	if (shared_ptr<belcard::BelCard> belCard = mParser->parseOne(buffer)) {
		return Vcard::create(belCard);
	}

	lError() << "[vCard] Couldn't parse buffer " << buffer;
	return nullptr;
}

vector<shared_ptr<Vcard>> VcardContext::getVcardListFromBuffer(const string &buffer) const {
	if (buffer.empty()) {
		return {};
	}

	vector<shared_ptr<Vcard>> result;
	if (shared_ptr<belcard::BelCardList> belCards = mParser->parse(buffer)) {
		auto cards = belCards->getCards();
		result.reserve(cards.size());
		for (const auto &belCard : cards) {
			result.push_back(Vcard::create(belCard));
		}
	}
	return result;
}

vector<shared_ptr<Vcard>> VcardContext::getVcardListFromFile(const string &filename) const {
	if (filename.empty()) {
		return {};
	}

	vector<shared_ptr<Vcard>> result;
	if (shared_ptr<belcard::BelCardList> belCards = mParser->parseFile(filename)) {
		auto cards = belCards->getCards();
		result.reserve(cards.size());
		for (const auto &belCard : belCards->getCards()) {
			result.push_back(Vcard::create(belCard));
		}
	}
	return result;
}

#else

VcardContext::VcardContext(bool) {
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
