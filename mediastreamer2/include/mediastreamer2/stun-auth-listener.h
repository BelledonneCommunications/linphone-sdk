/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
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

#pragma once

#include "bctoolbox/defs.h"

#include "mediastreamer2/mscommon.h"

namespace mediastreamer::nat {

class MS2_PUBLIC StunAuthResponse {
public:
	StunAuthResponse() = default;
	~StunAuthResponse() = default;

	std::string username;
	std::string password;
	std::string ha1;
};

class MS2_PUBLIC StunAuthListener {
public:
	virtual ~StunAuthListener() = default;

	virtual StunAuthResponse onStunAuthRequested(BCTBX_UNUSED(const std::string &realm),
	                                             BCTBX_UNUSED(const std::string &nonce)) {
		return {};
	}
};

}; // namespace mediastreamer::nat