/*
 * Copyright (c) 2010-2026 Belledonne Communications SARL.
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

#pragma once

#include "mediastreamer2/mscommon.h"

namespace mediastreamer::nat {

class MS2_PUBLIC StunTransactionId {
public:
	explicit StunTransactionId(const UInt96 &id) : mId(id) {
	}
	~StunTransactionId() = default;

	bool operator==(const StunTransactionId &other) const {
		return memcmp(&mId, &other.mId, sizeof(UInt96)) == 0;
	}
	bool operator!=(const StunTransactionId &other) const {
		return !(*this == other);
	}

	[[nodiscard]] std::string asString() const;
	[[nodiscard]] const UInt96 &asUInt96() const {
		return mId;
	}

	static StunTransactionId random();

private:
	StunTransactionId() = default;

	UInt96 mId{};
};

}; // namespace mediastreamer::nat