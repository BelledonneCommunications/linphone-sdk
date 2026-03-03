/*
 * Copyright (c) 2010-2023 Belledonne Communications SARL.
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

static string fixVcardTimestamps(const string &input) {
	// Nextcloud/Sabre generates REV timestamps without seconds (e.g. REV:20260221T2148Z)
	// but RFC 6350 and belcard grammar require seconds (REV:20260221T214800Z).
	// Fix by inserting "00" seconds before the trailing "Z" when only HHmm is present.
	//
	// NOTE: We avoid std::regex_replace here because the replacement pattern "$100$2"
	// is misinterpreted by some C++ stdlib implementations (notably libc++ on macOS):
	// "$10" is parsed as backreference to group 10 instead of group 1 + literal "0",
	// producing "0Z" instead of "REV:20260221T222600Z".
	string result = input;
	size_t pos = 0;
	while ((pos = result.find("REV:", pos)) != string::npos) {
		// REV:<8 digits>T<4 digits>Z → insert "00" before Z
		size_t valueStart = pos + 4;
		size_t expectedZ = valueStart + 13; // 8 digits + T + 4 digits = 13
		if (expectedZ < result.size() && result[valueStart + 8] == 'T' &&
		    (result[expectedZ] == 'Z' || result[expectedZ] == 'z')) {
			bool valid = true;
			for (size_t i = 0; i < 8 && valid; i++) valid = isdigit((unsigned char)result[valueStart + i]);
			for (size_t i = 9; i < 13 && valid; i++) valid = isdigit((unsigned char)result[valueStart + i]);
			if (valid) {
				result.insert(expectedZ, "00");
				pos = expectedZ + 3; // skip past inserted "00Z"
				continue;
			}
		}
		pos += 4;
	}
	return result;
}

shared_ptr<Vcard> VcardContext::getVcardFromBuffer(const string &buffer) const {
	if (buffer.empty()) return nullptr;
	string fixedBuffer = fixVcardTimestamps(buffer);
	if (fixedBuffer != buffer) {
		lInfo() << "[vCard] Applied timestamp fix to vCard buffer";
	}
	shared_ptr<belcard::BelCard> belCard = mParser->parseOne(fixedBuffer);
	if (belCard) {
		return Vcard::create(belCard);
	} else {
		// Log the fixed buffer so we can see exactly what belcard failed to parse
		lError() << "[vCard] Couldn't parse buffer (length=" << fixedBuffer.size() << "):\n" << fixedBuffer;
		return nullptr;
	}
}

list<shared_ptr<Vcard>> VcardContext::getVcardListFromBuffer(const string &buffer) const {
	list<shared_ptr<Vcard>> result;
	if (!buffer.empty()) {
		string fixedBuffer = fixVcardTimestamps(buffer);
		shared_ptr<belcard::BelCardList> belCards = mParser->parse(fixedBuffer);
		if (belCards) {
			for (const auto &belCard : belCards->getCards())
				result.push_back(Vcard::create(belCard));
		}
	}
	return result;
}

list<shared_ptr<Vcard>> VcardContext::getVcardListFromFile(const string &filename) const {
	list<shared_ptr<Vcard>> result;
	if (!filename.empty()) {
		shared_ptr<belcard::BelCardList> belCards = mParser->parseFile(filename);
		if (belCards) {
			for (const auto &belCard : belCards->getCards())
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

list<shared_ptr<Vcard>> VcardContext::getVcardListFromBuffer(BCTBX_UNUSED(const string &buffer)) const {
	return list<shared_ptr<Vcard>>();
}

list<shared_ptr<Vcard>> VcardContext::getVcardListFromFile(BCTBX_UNUSED(const string &filename)) const {
	return list<shared_ptr<Vcard>>();
}

#endif /* VCARD_ENABLED */

LINPHONE_END_NAMESPACE
