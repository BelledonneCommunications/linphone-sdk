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

#include <string>

#include "mediastreamer2/ice-constants.h"
#include "mediastreamer2/mscommon.h"

namespace ms2::nat {

class MS2_PUBLIC IceCredentials {
public:
	IceCredentials(const std::string &ufrag, const std::string &pwd) {
		mUfrag = ufrag.substr(0, ICE_MAX_UFRAG_LEN);
		mPwd = pwd.substr(0, ICE_MAX_PWD_LEN);
	}
	IceCredentials(const IceCredentials &other) = default;
	IceCredentials() = default;
	~IceCredentials() = default;

	bool operator==(const IceCredentials &other) const {
		return (mUfrag == other.mUfrag) && (mPwd == other.mPwd);
	}
	bool operator!=(const IceCredentials &other) const {
		return !(*this == other);
	}

	/**
	 * Get the password of an ICE credentials.
	 *
	 * @return A reference to the password of the ICE credentials
	 */
	[[nodiscard]] const std::string &getPwd() const {
		return mPwd;
	}

	/**
	 * Get the username fragment of an ICE credentials.
	 *
	 * @return A reference to the username fragment of the ICE credentials
	 */
	[[nodiscard]] const std::string &getUfrag() const {
		return mUfrag;
	};

private:
	std::string mUfrag; /**< Username fragment */
	std::string mPwd;   /**< Password */
};

} // namespace ms2::nat
