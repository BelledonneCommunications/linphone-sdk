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
#include <utility>

#include "ortp/rtpsession.h"

#include "mediastreamer2/mscommon.h"

namespace mediastreamer::nat {

class MS2_PUBLIC SockAddr {
public:
	SockAddr() = default;
	SockAddr(const struct sockaddr *addr, socklen_t addrLen);
	explicit SockAddr(const ortp_recv_addr *ortpRecvAddr);
	~SockAddr() = default;

	[[nodiscard]] std::string asString() const;
	[[nodiscard]] const struct sockaddr *asStructSockAddr() const {
		return reinterpret_cast<const struct sockaddr *>(&mAddr);
	};
	[[nodiscard]] int getFamily() const;
	[[nodiscard]] std::pair<std::string, int> getIpPort() const;
	[[nodiscard]] socklen_t getLen() const {
		return mLen;
	}
	[[nodiscard]] SockAddr ipv4ToIpv6() const;
	[[nodiscard]] SockAddr ipv6toIpv4() const;
	[[nodiscard]] SockAddr removeV4Mapping() const;

private:
	struct sockaddr_storage mAddr{};
	socklen_t mLen = sizeof(mAddr);
};

}; // namespace mediastreamer::nat