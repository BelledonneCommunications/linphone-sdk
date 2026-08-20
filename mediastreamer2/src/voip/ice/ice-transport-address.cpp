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

namespace ms2 {

constexpr size_t IP_STRING_SIZE = 64;

IceTransportAddress::IceTransportAddress(const int family, std::string ip, const int port)
    : mIp(std::move(ip)), mPort(port), mFamily(family) {
}

IceTransportAddress::IceTransportAddress(const struct sockaddr *addr, const socklen_t addrlen)
    : mIp(IP_STRING_SIZE, '\0') {
	init(addr, addrlen);
}

IceTransportAddress::IceTransportAddress(const MSStunAddress *stunAddress) : mIp(IP_STRING_SIZE, '\0') {
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	memset(&addr, 0, addrlen);
	ms_stun_address_to_sockaddr(stunAddress, reinterpret_cast<struct sockaddr *>(&addr), &addrlen);
	init(reinterpret_cast<struct sockaddr *>(&addr), addrlen);
}

bool IceTransportAddress::operator==(const IceTransportAddress &other) const {
	return (mFamily == other.mFamily) && (mPort == other.mPort) && (mIp == other.mIp);
}

std::string IceTransportAddress::asString() const {
	struct addrinfo *ai = bctbx_ip_address_to_addrinfo(mFamily, SOCK_DGRAM, mIp.c_str(), mPort);
	if (ai != nullptr) {
		std::string strRepr(IP_STRING_SIZE, '\0');
		bctbx_addrinfo_to_printable_ip_address(ai, strRepr.data(), strRepr.size());
		bctbx_freeaddrinfo(ai);
		return strRepr;
	}
	return {};
}

//------------------------------------------------------------------------------

void IceTransportAddress::init(const struct sockaddr *addr, const socklen_t addrlen) {
	bctbx_sockaddr_to_ip_address(addr, addrlen, mIp.data(), mIp.capacity(), &mPort);
	mIp.erase(std::find(mIp.begin(), mIp.end(), '\0'), mIp.end());
	mFamily = addr->sa_family;
}

MSStunAddress IceTransportAddress::toStunAddress() const {
	return ms_ip_address_to_stun_address(mFamily, SOCK_DGRAM, mIp.c_str(), mPort);
}

} // namespace ms2
