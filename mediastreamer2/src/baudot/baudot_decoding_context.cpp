/*
 * Copyright (c) 2010-2024 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
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

#include "baudot_decoding_context.h"

using namespace mediastreamer;

// Change to Figures is encoded with the [Shift Out] character: 0x0E.
static const char FIGURES_CHAR = 0x0E;

// Change to Letters is encoded with the [Shift In] character: 0x0F.
static const char LETTERS_CHAR = 0x0F;

// WRU: Who are you? is encoded with the [Enquiry] character: 0x05.
static const char WRU_CHAR = 0x05;

constexpr static const char LETTERS_CHARACTERS_TABLE[] = {
    '\b', 'E', '\n', 'A', ' ', 'S', 'I', 'U', '\r', 'D', 'R', 'J',          'N', 'F', 'C', 'K',
    'T',  'Z', 'L',  'W', 'H', 'Y', 'P', 'Q', 'O',  'B', 'G', FIGURES_CHAR, 'M', 'X', 'V', LETTERS_CHAR};
constexpr static const char US_FIGURES_CHARACTERS_TABLE[] = {
    '\b', '3', '\n', '-', ' ', '\a', '8', '7', '\r', '$', '4', '\'',         ',', '!', ':', '(',
    '5',  '"', ')',  '2', '=', '6',  '0', '1', '9',  '?', '+', FIGURES_CHAR, '.', '/', ';', LETTERS_CHAR};
constexpr static const char EUROPE_FIGURES_CHARACTERS_TABLE[] = {
    '\b', '3', '\n', '-', ' ',        '\'', '8', '7', '\r', WRU_CHAR, '4', '\a',         ',', '!', ':', '(',
    '5',  '+', ')',  '2', (char)0x9C, '6',  '0', '1', '9',  '?',      '&', FIGURES_CHAR, '.', '/', '=', LETTERS_CHAR};

void BaudotDecodingContext::clear() {
	std::queue<char> empty;
	std::swap(mDecodedCharacters, empty);
}

void BaudotDecodingContext::feed(uint8_t encodedCharacter) {
	encodedCharacter = (encodedCharacter >> 1) & 0b11111;

	char decodedChar = decode(encodedCharacter);
	if (decodedChar == LETTERS_CHAR) {
		mMode = BaudotMode::Letters;
	} else if (decodedChar == FIGURES_CHAR) {
		mMode = BaudotMode::Figures;
	} else {
		mDecodedCharacters.push(decodedChar);
	}
}

std::optional<char> BaudotDecodingContext::nextDecodedCharacter() {
	if (mDecodedCharacters.empty()) {
		return std::nullopt;
	}

	char decodedCharacter = mDecodedCharacters.front();
	mDecodedCharacters.pop();
	return decodedCharacter;
}

char BaudotDecodingContext::decode(uint8_t encodedCharacter) {
	switch (mMode) {
		default:
		case BaudotMode::Letters:
			return LETTERS_CHARACTERS_TABLE[encodedCharacter];
		case BaudotMode::Figures:
			switch (mStandard) {
				default:
				case MSBaudotStandardUs:
					return US_FIGURES_CHARACTERS_TABLE[encodedCharacter];
				case MSBaudotStandardEurope:
					return EUROPE_FIGURES_CHARACTERS_TABLE[encodedCharacter];
			}
	}
}