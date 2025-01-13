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

#ifndef _MS_BAUDOT_ENCODING_CONTEXT_H
#define _MS_BAUDOT_ENCODING_CONTEXT_H

#include <optional>
#include <queue>
#include <string>

#include <bctoolbox/port.h>

#include "baudot_context.h"

namespace mediastreamer {

class BaudotEncodingContext : public BaudotContext {
public:
	MS2_PUBLIC BaudotEncodingContext() = default;
	virtual ~BaudotEncodingContext() = default;
	BaudotEncodingContext(const BaudotEncodingContext &) = delete;

	MS2_PUBLIC void encodeChar(const char c);
	MS2_PUBLIC void encodeString(const std::string &text);
	MS2_PUBLIC bool hasEncodedCharacters() const;
	MS2_PUBLIC std::optional<uint8_t> nextCode();
	MS2_PUBLIC void reinitialize();
	MS2_PUBLIC void setPauseTimeout(uint8_t timeout);

private:
	virtual bool hasSignificantPauseTimedOut() const;
	virtual void setMode(BaudotMode mode) override;

	std::queue<uint8_t> mEncodedCharacters;
	bctoolboxTimeSpec mLastEncodedCharacterTime;
	uint8_t mNbCharactersWithoutMode = 0;
	uint8_t mNbCharactersWithoutCrLf = 0;
	uint8_t mSignificantPauseTimeoutS = 5;
	bool mInitialLettersToneSent = false;
};

}; // namespace mediastreamer

#endif /* _MS_BAUDOT_ENCODING_CONTEXT_H */
