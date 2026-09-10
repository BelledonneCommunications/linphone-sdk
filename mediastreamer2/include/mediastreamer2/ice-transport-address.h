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

#include <bctoolbox/port.h>

#include "mediastreamer2/sockaddr.h"
#include "mediastreamer2/stun-address.h"

namespace ms2::nat {

/**
 * Represents an ICE transport address.
 */
class MS2_PUBLIC IceTransportAddress {
public:
	friend class IceCheckList;
	friend class IceCandidatePair;

	/**
	 * Create an ICE transport address.
	 * @param family The address family (AF_INET or AF_INET6)
	 * @param ip The IP address as a string (eg. 192.168.0.10)
	 * @param port The port number
	 */
	IceTransportAddress(int family, std::string ip, int port);
	IceTransportAddress(const struct sockaddr *addr, socklen_t addrlen);
	explicit IceTransportAddress(const SockAddr &sockAddr);
	explicit IceTransportAddress(const StunAddress &stunAddress);
	~IceTransportAddress() = default;

	bool operator==(const IceTransportAddress &other) const;

	[[nodiscard]] std::string asString() const;
	[[nodiscard]] int getFamily() const {
		return mFamily;
	}
	[[nodiscard]] const std::string &getIp() const {
		return mIp;
	}
	[[nodiscard]] int getPort() const {
		return mPort;
	}

private:
	void init(const SockAddr &sockAddr);
	[[nodiscard]] StunAddress toStunAddress() const;

	std::string mIp;
	int mPort = 0;
	int mFamily = AF_INET;
	// TODO: Handling of transport type: TCP, UDP...
};

} // namespace ms2::nat
