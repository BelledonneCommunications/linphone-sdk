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

#include <sstream>

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

// Sanitize vCard properties that are syntactically invalid per RFC 6350 / belcard grammar.
// A single invalid property line causes the entire vCard to fail parsing in belcard/belr
// because the strict PEG grammar's 1*property loop stops on the first non-matching line.
//
// Known problematic patterns from Nextcloud/Sabre/Google contacts:
//   - URL;VALUE=URI:Google+   => value is not a valid URI (no scheme), remove the line
//   - PHOTO;VALUE=URI:data:application/octet-stream;base64\,  => empty data, remove the line
static string sanitizeVcardProperties(const string &input) {
	istringstream stream(input);
	string line;
	string result;
	result.reserve(input.size());
	int removedCount = 0;

	while (getline(stream, line)) {
		// Remove trailing \r if present (getline strips \n but not \r)
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}

		bool removeLine = false;

		// Check URL properties: value must be a valid URI (must contain "://")
		// Handles grouped properties like ITEM1.URL;VALUE=URI:...
		{
			string upper = line;
			for (auto &c : upper) c = static_cast<char>(toupper((unsigned char)c));
			// Find "URL" preceded by start-of-line or "."
			size_t urlPos = upper.find("URL");
			if (urlPos != string::npos && (urlPos == 0 || upper[urlPos - 1] == '.')) {
				// Find the colon that separates property name/params from value
				size_t colonPos = line.find(':', urlPos + 3);
				if (colonPos != string::npos) {
					string value = line.substr(colonPos + 1);
					// A valid URI must have a scheme followed by ":" (e.g. "http:", "https:", "tel:")
					if (value.find(':') == string::npos) {
						lWarning() << "[vCard] Removing invalid URL property (value '" << value
						           << "' is not a valid URI): " << line;
						removeLine = true;
					}
				}
			}
		}

		// Check PHOTO properties with empty data
		// e.g. PHOTO;VALUE=URI:data:application/octet-stream;base64\,
		if (!removeLine) {
			string upper = line;
			for (auto &c : upper) c = static_cast<char>(toupper((unsigned char)c));
			size_t photoPos = upper.find("PHOTO");
			if (photoPos != string::npos && (photoPos == 0 || upper[photoPos - 1] == '.')) {
				// Check if it's a base64 data URI with empty data
				size_t base64Pos = line.find("base64");
				if (base64Pos == string::npos) base64Pos = line.find("BASE64");
				if (base64Pos != string::npos) {
					// Find the comma after base64 — data should follow
					size_t commaPos = line.find(',', base64Pos);
					if (commaPos == string::npos) commaPos = line.find("\\,", base64Pos);
					if (commaPos != string::npos) {
						string afterComma = line.substr(
						    line[commaPos] == '\\' ? commaPos + 2 : commaPos + 1);
						// Trim whitespace
						size_t start = afterComma.find_first_not_of(" \t");
						if (start == string::npos || afterComma.substr(start).empty()) {
							lWarning() << "[vCard] Removing PHOTO property with empty base64 data: "
							           << line.substr(0, 80) << "...";
							removeLine = true;
						}
					}
				}
			}
		}

		if (removeLine) {
			removedCount++;
		} else {
			result += line + "\r\n";
		}
	}

	if (removedCount > 0) {
		lInfo() << "[vCard] Sanitized vCard: removed " << removedCount << " invalid property line(s)";
	}

	return result;
}

shared_ptr<Vcard> VcardContext::getVcardFromBuffer(const string &buffer) const {
	if (buffer.empty()) return nullptr;
	string fixedBuffer = fixVcardTimestamps(buffer);
	if (fixedBuffer != buffer) {
		lInfo() << "[vCard] Applied timestamp fix to vCard buffer";
	}
	fixedBuffer = sanitizeVcardProperties(fixedBuffer);
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
		fixedBuffer = sanitizeVcardProperties(fixedBuffer);
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
