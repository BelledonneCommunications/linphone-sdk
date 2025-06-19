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

#ifndef _MS_BAUDOT_CONTEXT_H
#define _MS_BAUDOT_CONTEXT_H

#include <cstdint>

#include "mediastreamer2/baudot.h"

namespace mediastreamer {

extern const uint8_t BAUDOT_LETTERS_CODE;
extern const uint8_t BAUDOT_FIGURES_CODE;
extern const uint8_t BAUDOT_CARRIER_CODE; // Special code to represent the 150 ms carrier before sending characters.

enum BaudotMode {
	Letters,
	Figures,
};

class BaudotContext {
public:
	BaudotContext() {
	}
	virtual ~BaudotContext() {
	}
	BaudotContext(const BaudotContext &) = delete;

	MS2_PUBLIC MSBaudotStandard getStandard() const {
		return mStandard;
	}
	MS2_PUBLIC void setStandard(MSBaudotStandard standard) {
		mStandard = standard;
	}

protected:
	virtual void setMode(BaudotMode mode) {
		mMode = mode;
	}

	BaudotMode mMode = Letters;
	MSBaudotStandard mStandard = MSBaudotStandardUs;
};

}; // namespace mediastreamer

#endif /* _MS_BAUDOT_CONTEXT_H */
