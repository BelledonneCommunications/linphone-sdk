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

#include "bctoolbox/port.h"

#include "mediastreamer2/sockaddr.h"

namespace mediastreamer::nat {

constexpr size_t kIpStringSize = 64;

SockAddr::SockAddr(const ortp_recv_addr *ortpRecvAddr) {
	ortp_recvaddr_to_sockaddr(const_cast<ortp_recv_addr *>(ortpRecvAddr), reinterpret_cast<struct sockaddr *>(&mAddr),
	                          &mLen);
}

SockAddr::SockAddr(const struct sockaddr *addr, const socklen_t addrLen) : mLen(addrLen) {
	memcpy(&mAddr, addr, addrLen);
}

std::string SockAddr::asString() const {
	std::string output;
	output.resize(kIpStringSize, '\0');
	bctbx_sockaddr_to_printable_ip_address(
	    const_cast<struct sockaddr *>(reinterpret_cast<const struct sockaddr *>(&mAddr)), mLen, output.data(),
	    output.capacity());
	output.erase(std::find(output.begin(), output.end(), '\0'), output.end());
	return output;
}

int SockAddr::getFamily() const {
	return reinterpret_cast<const struct sockaddr *>(&mAddr)->sa_family;
}

std::pair<std::string, int> SockAddr::getIpPort() const {
	std::string ip;
	ip.resize(kIpStringSize, '\0');
	int port = 0;
	bctbx_sockaddr_to_ip_address(asStructSockAddr(), getLen(), ip.data(), ip.capacity(), &port);
	ip.erase(std::find(ip.begin(), ip.end(), '\0'), ip.end());
	return {ip, port};
}

SockAddr SockAddr::ipv4ToIpv6() const {
	SockAddr result{};
	bctbx_sockaddr_ipv4_to_ipv6(reinterpret_cast<const struct sockaddr *>(&mAddr),
	                            reinterpret_cast<struct sockaddr *>(&result.mAddr), &result.mLen);
	return result;
}

SockAddr SockAddr::ipv6toIpv4() const {
	SockAddr result{};
	bctbx_sockaddr_ipv6_to_ipv4(reinterpret_cast<const struct sockaddr *>(&mAddr),
	                            reinterpret_cast<struct sockaddr *>(&result.mAddr), &result.mLen);
	return result;
}

SockAddr SockAddr::removeV4Mapping() const {
	SockAddr result{};
	bctbx_sockaddr_remove_v4_mapping(reinterpret_cast<const struct sockaddr *>(&mAddr),
	                                 reinterpret_cast<struct sockaddr *>(&result.mAddr), &result.mLen);
	return result;
}

} // namespace mediastreamer::nat
