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

#include <algorithm>
#include <utility>

#include "mediastreamer2/ice-transport-address.h"

namespace mediastreamer::nat {

constexpr size_t kIpStringSize = 64;

IceTransportAddress::IceTransportAddress(const int family, std::string ip, const int port)
    : mIp(std::move(ip)), mPort(port), mFamily(family) {
}

IceTransportAddress::IceTransportAddress(const struct sockaddr *addr, const socklen_t addrlen)
    : mIp(kIpStringSize, '\0') {
	init(SockAddr(addr, addrlen));
}

IceTransportAddress::IceTransportAddress(const SockAddr &sockAddr) {
	init(sockAddr);
}

IceTransportAddress::IceTransportAddress(const StunAddress &stunAddress) : mIp(kIpStringSize, '\0') {
	init(stunAddress.toSockAddr());
}

bool IceTransportAddress::operator==(const IceTransportAddress &other) const {
	return (mFamily == other.mFamily) && (mPort == other.mPort) && (mIp == other.mIp);
}

std::string IceTransportAddress::asString() const {
	struct addrinfo *ai = bctbx_ip_address_to_addrinfo(mFamily, SOCK_DGRAM, mIp.c_str(), mPort);
	if (ai != nullptr) {
		std::string strRepr(kIpStringSize, '\0');
		bctbx_addrinfo_to_printable_ip_address(ai, strRepr.data(), strRepr.capacity());
		bctbx_freeaddrinfo(ai);
		strRepr.erase(std::find(strRepr.begin(), strRepr.end(), '\0'), strRepr.end());
		return strRepr;
	}
	return {};
}

//------------------------------------------------------------------------------

void IceTransportAddress::init(const SockAddr &sockAddr) {
	const auto [ip, port] = sockAddr.getIpPort();
	mIp = ip;
	mPort = port;
	mFamily = sockAddr.getFamily();
}

StunAddress IceTransportAddress::toStunAddress() const {
	return {mFamily, SOCK_DGRAM, mIp, mPort};
}

} // namespace mediastreamer::nat
