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

#include <utility>

#include "mediastreamer2/mscommon.h"

namespace ms2::nat {

class MS2_PUBLIC StunError {
public:
	enum class Code {
		StunTryAlternate = 300,
		StunBadRequest = 400,
		StunUnauthorized = 401,
		StunUnknownAttribute = 420,
		StunStaleNonce = 438,
		StunServerError = 500,

		TurnForbidden = 403,
		TurnAllocationMismatch = 437,
		TurnWrongCredentials = 441,
		TurnUnsupportedTransportProtocol = 442,
		TurnAllocationQuotaReached = 486,
		TurnInsufficientCapacity = 508,

		IceRoleConflict = 487,
	};

	explicit StunError(const Code errorCode, std::string reason = "")
	    : mErrorCode(errorCode), mReason(std::move(reason)) {
		if (mReason.size() > MAX_REASON_LENGTH) {
			mReason.resize(MAX_REASON_LENGTH);
		}
	}
	~StunError() = default;

	[[nodiscard]] Code getErrorCode() const {
		return mErrorCode;
	}
	[[nodiscard]] std::string getReason() const {
		return mReason;
	}

	static constexpr size_t MAX_REASON_LENGTH = 127;

private:
	Code mErrorCode{};
	std::string mReason{};
};

}; // namespace ms2::nat