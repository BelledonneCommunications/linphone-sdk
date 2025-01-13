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

#include "baudot_encoding_context.h"
#include "baudot_context.h"

using namespace mediastreamer;

// Figures or Letters mode tones should be retransmitted every 72 continuous characters.
static const uint8_t MODE_RETRANSMISSION_FREQUENCY = 72;
// CR or LF characters must be sent at least once every 60 to 72 characters.
static const uint8_t CRLF_MIN_RETRANSMISSION_FREQUENCY = 60;
static const uint8_t CRLF_MAX_RETRANSMISSION_FREQUENCY = 72;

static const uint8_t NO_MAPPING = 0xFF;
static const uint8_t LETTER_MASK = 0b1000000;
static const uint8_t FIGURE_MASK = 0b0100000;
static const uint8_t CHARACTER_MASK = 0b00011111;
constexpr static const uint8_t US_CHAR_TO_BAUDOT_TABLE[] = {
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    FIGURE_MASK | 0b00101, /* '\a' */
    LETTER_MASK | FIGURE_MASK | 0b00000 /* '\b' */,
    NO_MAPPING,
    LETTER_MASK | FIGURE_MASK | 0b00010 /* '\n' */,
    NO_MAPPING,
    NO_MAPPING,
    LETTER_MASK | FIGURE_MASK | 0b01000 /* '\r' */,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    LETTER_MASK | FIGURE_MASK | 0b00100 /* ' ' */,
    FIGURE_MASK | 0b01101 /* '!' */,
    FIGURE_MASK | 0b10001 /* '"' */,
    NO_MAPPING /* '#' */,
    FIGURE_MASK | 0b01001 /* '$' */,
    NO_MAPPING /* '%' */,
    NO_MAPPING /* '&' */,
    FIGURE_MASK | 0b01011 /* '\'' */,
    FIGURE_MASK | 0b01111 /* '(' */,
    FIGURE_MASK | 0b10010 /* ')' */,
    NO_MAPPING /* '*' */,
    FIGURE_MASK | 0b11010 /* '+' */,
    FIGURE_MASK | 0b01100 /* ',' */,
    FIGURE_MASK | 0b00011 /* '-' */,
    FIGURE_MASK | 0b11100 /* '.' */,
    FIGURE_MASK | 0b11101 /* '/' */,
    FIGURE_MASK | 0b10110 /* '0' */,
    FIGURE_MASK | 0b10111 /* '1' */,
    FIGURE_MASK | 0b10011 /* '2' */,
    FIGURE_MASK | 0b00001 /* '3' */,
    FIGURE_MASK | 0b01010 /* '4' */,
    FIGURE_MASK | 0b10000 /* '5' */,
    FIGURE_MASK | 0b10101 /* '6' */,
    FIGURE_MASK | 0b00111 /* '7' */,
    FIGURE_MASK | 0b00110 /* '8' */,
    FIGURE_MASK | 0b11000 /* '9' */,
    FIGURE_MASK | 0b01110 /* ':' */,
    FIGURE_MASK | 0b11110 /* ';' */,
    NO_MAPPING /* '<' */,
    FIGURE_MASK | 0b10100 /* '=' */,
    NO_MAPPING /* '>' */,
    FIGURE_MASK | 0b11001 /* '?' */,
    NO_MAPPING /* '@' */,
    LETTER_MASK | 0b00011 /* 'A' */,
    LETTER_MASK | 0b11001 /* 'B' */,
    LETTER_MASK | 0b01110 /* 'C' */,
    LETTER_MASK | 0b01001 /* 'D' */,
    LETTER_MASK | 0b00001 /* 'E' */,
    LETTER_MASK | 0b01101 /* 'F' */,
    LETTER_MASK | 0b11010 /* 'G' */,
    LETTER_MASK | 0b10100 /* 'H' */,
    LETTER_MASK | 0b00110 /* 'I' */,
    LETTER_MASK | 0b01011 /* 'J' */,
    LETTER_MASK | 0b01111 /* 'K' */,
    LETTER_MASK | 0b10010 /* 'L' */,
    LETTER_MASK | 0b11100 /* 'M' */,
    LETTER_MASK | 0b01100 /* 'N' */,
    LETTER_MASK | 0b11000 /* 'O' */,
    LETTER_MASK | 0b10110 /* 'P' */,
    LETTER_MASK | 0b10111 /* 'Q' */,
    LETTER_MASK | 0b01010 /* 'R' */,
    LETTER_MASK | 0b00101 /* 'S' */,
    LETTER_MASK | 0b10000 /* 'T' */,
    LETTER_MASK | 0b00111 /* 'U' */,
    LETTER_MASK | 0b11110 /* 'V' */,
    LETTER_MASK | 0b10011 /* 'W' */,
    LETTER_MASK | 0b11101 /* 'X' */,
    LETTER_MASK | 0b10101 /* 'Y' */,
    LETTER_MASK | 0b10001 /* 'Z' */,
    NO_MAPPING /* '[' */,
    NO_MAPPING /* '\\' */,
    NO_MAPPING /* ']' */,
    NO_MAPPING /* '^' */,
    NO_MAPPING /* '_' */,
    NO_MAPPING /* '`' */,
    LETTER_MASK | 0b00011 /* 'a' */,
    LETTER_MASK | 0b11001 /* 'b' */,
    LETTER_MASK | 0b01110 /* 'c' */,
    LETTER_MASK | 0b01001 /* 'd' */,
    LETTER_MASK | 0b00001 /* 'e' */,
    LETTER_MASK | 0b01101 /* 'f' */,
    LETTER_MASK | 0b11010 /* 'g' */,
    LETTER_MASK | 0b10100 /* 'h' */,
    LETTER_MASK | 0b00110 /* 'i' */,
    LETTER_MASK | 0b01011 /* 'j' */,
    LETTER_MASK | 0b01111 /* 'k' */,
    LETTER_MASK | 0b10010 /* 'l' */,
    LETTER_MASK | 0b11100 /* 'm' */,
    LETTER_MASK | 0b01100 /* 'n' */,
    LETTER_MASK | 0b11000 /* 'o' */,
    LETTER_MASK | 0b10110 /* 'p' */,
    LETTER_MASK | 0b10111 /* 'q' */,
    LETTER_MASK | 0b01010 /* 'r' */,
    LETTER_MASK | 0b00101 /* 's' */,
    LETTER_MASK | 0b10000 /* 't' */,
    LETTER_MASK | 0b00111 /* 'u' */,
    LETTER_MASK | 0b11110 /* 'v' */,
    LETTER_MASK | 0b10011 /* 'w' */,
    LETTER_MASK | 0b11101 /* 'x' */,
    LETTER_MASK | 0b10101 /* 'y' */,
    LETTER_MASK | 0b10001 /* 'z' */,
    NO_MAPPING /* '{' */,
    NO_MAPPING /* '|' */,
    NO_MAPPING /* '}' */,
    NO_MAPPING /* '~' */,
    NO_MAPPING, /* DEL */
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
};

constexpr static const uint8_t EUROPE_CHAR_TO_BAUDOT_TABLE[] = {
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    FIGURE_MASK | 0b01001 /* WRU */,
    NO_MAPPING,
    FIGURE_MASK | 0b01011 /* '\a' */,
    LETTER_MASK | FIGURE_MASK | 0b00000 /* '\b' */,
    NO_MAPPING,
    LETTER_MASK | FIGURE_MASK | 0b00010 /* '\n' */,
    NO_MAPPING,
    NO_MAPPING,
    LETTER_MASK | FIGURE_MASK | 0b01000 /* '\r' */,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    LETTER_MASK | FIGURE_MASK | 0b00100 /* ' ' */,
    FIGURE_MASK | 0b01101 /* '!' */,
    NO_MAPPING /* '"' */,
    NO_MAPPING /* '#' */,
    NO_MAPPING /* '$' */,
    NO_MAPPING /* '%' */,
    FIGURE_MASK | 0b11010 /* '&' */,
    FIGURE_MASK | 0b00101 /* '\'' */,
    FIGURE_MASK | 0b01111 /* '(' */,
    FIGURE_MASK | 0b10010 /* ')' */,
    NO_MAPPING /* '*' */,
    FIGURE_MASK | 0b10001 /* '+' */,
    FIGURE_MASK | 0b01100 /* ',' */,
    FIGURE_MASK | 0b00011 /* '-' */,
    FIGURE_MASK | 0b11100 /* '.' */,
    FIGURE_MASK | 0b11101 /* '/' */,
    FIGURE_MASK | 0b10110 /* '0' */,
    FIGURE_MASK | 0b10111 /* '1' */,
    FIGURE_MASK | 0b10011 /* '2' */,
    FIGURE_MASK | 0b00001 /* '3' */,
    FIGURE_MASK | 0b01010 /* '4' */,
    FIGURE_MASK | 0b10000 /* '5' */,
    FIGURE_MASK | 0b10101 /* '6' */,
    FIGURE_MASK | 0b00111 /* '7' */,
    FIGURE_MASK | 0b00110 /* '8' */,
    FIGURE_MASK | 0b11000 /* '9' */,
    FIGURE_MASK | 0b01110 /* ':' */,
    NO_MAPPING /* ';' */,
    NO_MAPPING /* '<' */,
    FIGURE_MASK | 0b11110 /* '=' */,
    NO_MAPPING /* '>' */,
    FIGURE_MASK | 0b11001 /* '?' */,
    NO_MAPPING /* '@' */,
    LETTER_MASK | 0b00011 /* 'A' */,
    LETTER_MASK | 0b11001 /* 'B' */,
    LETTER_MASK | 0b01110 /* 'C' */,
    LETTER_MASK | 0b01001 /* 'D' */,
    LETTER_MASK | 0b00001 /* 'E' */,
    LETTER_MASK | 0b01101 /* 'F' */,
    LETTER_MASK | 0b11010 /* 'G' */,
    LETTER_MASK | 0b10100 /* 'H' */,
    LETTER_MASK | 0b00110 /* 'I' */,
    LETTER_MASK | 0b01011 /* 'J' */,
    LETTER_MASK | 0b01111 /* 'K' */,
    LETTER_MASK | 0b10010 /* 'L' */,
    LETTER_MASK | 0b11100 /* 'M' */,
    LETTER_MASK | 0b01100 /* 'N' */,
    LETTER_MASK | 0b11000 /* 'O' */,
    LETTER_MASK | 0b10110 /* 'P' */,
    LETTER_MASK | 0b10111 /* 'Q' */,
    LETTER_MASK | 0b01010 /* 'R' */,
    LETTER_MASK | 0b00101 /* 'S' */,
    LETTER_MASK | 0b10000 /* 'T' */,
    LETTER_MASK | 0b00111 /* 'U' */,
    LETTER_MASK | 0b11110 /* 'V' */,
    LETTER_MASK | 0b10011 /* 'W' */,
    LETTER_MASK | 0b11101 /* 'X' */,
    LETTER_MASK | 0b10101 /* 'Y' */,
    LETTER_MASK | 0b10001 /* 'Z' */,
    NO_MAPPING /* '[' */,
    NO_MAPPING /* '\\' */,
    NO_MAPPING /* ']' */,
    NO_MAPPING /* '^' */,
    NO_MAPPING /* '_' */,
    NO_MAPPING /* '`' */,
    LETTER_MASK | 0b00011 /* 'a' */,
    LETTER_MASK | 0b11001 /* 'b' */,
    LETTER_MASK | 0b01110 /* 'c' */,
    LETTER_MASK | 0b01001 /* 'd' */,
    LETTER_MASK | 0b00001 /* 'e' */,
    LETTER_MASK | 0b01101 /* 'f' */,
    LETTER_MASK | 0b11010 /* 'g' */,
    LETTER_MASK | 0b10100 /* 'h' */,
    LETTER_MASK | 0b00110 /* 'i' */,
    LETTER_MASK | 0b01011 /* 'j' */,
    LETTER_MASK | 0b01111 /* 'k' */,
    LETTER_MASK | 0b10010 /* 'l' */,
    LETTER_MASK | 0b11100 /* 'm' */,
    LETTER_MASK | 0b01100 /* 'n' */,
    LETTER_MASK | 0b11000 /* 'o' */,
    LETTER_MASK | 0b10110 /* 'p' */,
    LETTER_MASK | 0b10111 /* 'q' */,
    LETTER_MASK | 0b01010 /* 'r' */,
    LETTER_MASK | 0b00101 /* 's' */,
    LETTER_MASK | 0b10000 /* 't' */,
    LETTER_MASK | 0b00111 /* 'u' */,
    LETTER_MASK | 0b11110 /* 'v' */,
    LETTER_MASK | 0b10011 /* 'w' */,
    LETTER_MASK | 0b11101 /* 'x' */,
    LETTER_MASK | 0b10101 /* 'y' */,
    LETTER_MASK | 0b10001 /* 'z' */,
    NO_MAPPING /* '{' */,
    NO_MAPPING /* '|' */,
    NO_MAPPING /* '}' */,
    NO_MAPPING /* '~' */,
    NO_MAPPING /* DEL */,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    FIGURE_MASK | 0b10100 /* '£' */,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
    NO_MAPPING,
};

const uint8_t mediastreamer::BAUDOT_LETTERS_CODE =
    0b11111110; // Letters code with space and long mark (in reverse order, will be sent from right to left).
const uint8_t mediastreamer::BAUDOT_FIGURES_CODE =
    0b11110110; // Figures code with space and long mark (in reverse order, will be sent from right to left).
const uint8_t mediastreamer::BAUDOT_CARRIER_CODE = 0b11111111;
static const uint8_t BAUDOT_CR_CODE =
    0b11010000; // CR code with space and long mark (in reverse order, will be sent from right to left).

static bool isCrLf(char c) {
	return (c == '\r') || (c == '\n');
}

static bool isSpace(char c) {
	return c == ' ';
}

void BaudotEncodingContext::encodeChar(const char c) {
	uint8_t code;
	switch (mStandard) {
		default:
		case MSBaudotStandardUs:
			code = US_CHAR_TO_BAUDOT_TABLE[(uint8_t)c];
			break;
		case MSBaudotStandardEurope:
			code = EUROPE_CHAR_TO_BAUDOT_TABLE[(uint8_t)c];
			break;
	}

	// Skip character that has no Baudot code mapping.
	if (code == NO_MAPPING) return;

	// Handle transmission of first LETTERS mode tone.
	if (!mInitialLettersToneSent || hasSignificantPauseTimedOut()) {
		mEncodedCharacters.push(BAUDOT_CARRIER_CODE);
		mEncodedCharacters.push(BAUDOT_LETTERS_CODE);
		setMode(BaudotMode::Letters);
		mInitialLettersToneSent = true;
	}

	// Handle switching of modes.
	if ((mMode == Letters) && !(code & LETTER_MASK) && (code & FIGURE_MASK)) {
		mEncodedCharacters.push(BAUDOT_FIGURES_CODE);
		setMode(BaudotMode::Figures);
	} else if ((mMode == Figures) && !(code & FIGURE_MASK) && (code & LETTER_MASK)) {
		mEncodedCharacters.push(BAUDOT_LETTERS_CODE);
		setMode(BaudotMode::Letters);
	} else if (mNbCharactersWithoutMode == MODE_RETRANSMISSION_FREQUENCY) {
		switch (mMode) {
			case Letters:
				mEncodedCharacters.push(BAUDOT_LETTERS_CODE);
				break;
			case Figures:
				mEncodedCharacters.push(BAUDOT_FIGURES_CODE);
				break;
		}
		mNbCharactersWithoutMode = 0;
	}

	// Add a CR character every 60 to 72 characters.
	if (!isCrLf(c) && (((mNbCharactersWithoutCrLf >= CRLF_MIN_RETRANSMISSION_FREQUENCY) && isSpace(c)) ||
	                   (mNbCharactersWithoutCrLf == CRLF_MAX_RETRANSMISSION_FREQUENCY))) {
		mEncodedCharacters.push(BAUDOT_CR_CODE);
		mNbCharactersWithoutMode++;
		mNbCharactersWithoutCrLf = 0;
	}

	// Add the character (long mark at the beginning and space at the end, because it will be sent from right to
	// left).
	code = ((code & CHARACTER_MASK) << 1) | 0b11000000;
	mEncodedCharacters.push(code);
	mNbCharactersWithoutMode++;
	if (isCrLf(code)) {
		mNbCharactersWithoutCrLf = 0;
	} else {
		mNbCharactersWithoutCrLf++;
	}
	bctbx_get_cur_time(&mLastEncodedCharacterTime);
}

void BaudotEncodingContext::encodeString(const std::string &text) {
	for (const char &c : text) {
		encodeChar(c);
	}
}

bool BaudotEncodingContext::hasEncodedCharacters() const {
	return !mEncodedCharacters.empty();
}

std::optional<uint8_t> BaudotEncodingContext::nextCode() {
	if (mEncodedCharacters.empty()) return std::nullopt;
	uint8_t code = mEncodedCharacters.front();
	mEncodedCharacters.pop();
	return code;
}

void BaudotEncodingContext::reinitialize() {
	mInitialLettersToneSent = false;
	mNbCharactersWithoutMode = 0;
	mNbCharactersWithoutCrLf = 0;
}

void BaudotEncodingContext::setPauseTimeout(uint8_t timeout) {
	mSignificantPauseTimeoutS = timeout;
}

bool BaudotEncodingContext::hasSignificantPauseTimedOut() const {
	bctoolboxTimeSpec testedTime;

	// Get the current time and remove the significant pause timeout duration to it.
	bctbx_get_cur_time(&testedTime);
	bctbx_timespec_add_secs(&testedTime, 0 - (int64_t)mSignificantPauseTimeoutS);

	return (bctbx_timespec_compare(&testedTime, &mLastEncodedCharacterTime) >= 0);
}

void BaudotEncodingContext::setMode(BaudotMode mode) {
	BaudotContext::setMode(mode);
	mNbCharactersWithoutMode = 0;
}
