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

#include <variant>

#include "bctoolbox/port.h"
#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/stun-address.h"

namespace mediastreamer::nat {

StunAddress::StunAddress(const struct sockaddr *addr) {
	init(addr);
}

StunAddress::StunAddress(const SockAddr &sockAddr) {
	init(sockAddr.asStructSockAddr());
}

StunAddress::StunAddress(const int aiFamily, const int sockType, const std::string &hostname, const int port) {
	struct addrinfo *res = bctbx_ip_address_to_addrinfo(aiFamily, sockType, hostname.c_str(), port);
	if (res != nullptr) {
		init(res->ai_addr);
		bctbx_freeaddrinfo(res);
	}
}

bool StunAddress::operator==(const StunAddress &other) const {
	if (getFamily() != other.getFamily()) {
		return false;
	}
	if (mPort != other.mPort) {
		return false;
	}
	switch (getFamily()) {
		default:
		case Family::IpV4:
			return std::get<StunAddressV4>(mAddress) == std::get<StunAddressV4>(other.mAddress);
		case Family::IpV6:
			return memcmp(&std::get<StunAddressV6>(mAddress), &std::get<StunAddressV6>(other.mAddress),
			              sizeof(StunAddressV6)) == 0;
	}
}

StunAddress::Family StunAddress::getFamily() const {
	if (std::holds_alternative<StunAddressV6>(mAddress)) {
		return Family::IpV6;
	}
	return Family::IpV4;
}

std::optional<StunAddressV4> StunAddress::getIpV4Address() const {
	if (std::holds_alternative<StunAddressV4>(mAddress)) {
		return std::get<StunAddressV4>(mAddress);
	}
	return std::nullopt;
}

std::optional<StunAddressV6> StunAddress::getIpV6Address() const {
	if (std::holds_alternative<StunAddressV6>(mAddress)) {
		return std::get<StunAddressV6>(mAddress);
	}
	return std::nullopt;
}

SockAddr StunAddress::toSockAddr() const {
	struct sockaddr_storage addr{};
	socklen_t addrLen = sizeof(addr);
	if (getFamily() == Family::IpV4) {
		auto *addr_in = reinterpret_cast<struct sockaddr_in *>(&addr);
		addr_in->sin_family = AF_INET;
		addr_in->sin_port = htons(mPort);
		addr_in->sin_addr.s_addr = htonl(std::get<StunAddressV4>(mAddress));
		addrLen = sizeof(struct sockaddr_in);
	} else if (getFamily() == Family::IpV6) {
		auto *addr_in6 = reinterpret_cast<struct sockaddr_in6 *>(&addr);
		addr_in6->sin6_family = AF_INET6;
		addr_in6->sin6_port = htons(mPort);
		memcpy(addr_in6->sin6_addr.s6_addr, &std::get<StunAddressV6>(mAddress), sizeof(StunAddressV6));
		addrLen = sizeof(struct sockaddr_in6);
	}
	return {reinterpret_cast<struct sockaddr *>(&addr), addrLen};
}

StunAddress StunAddress::toXor(const StunTransactionId &transactionId) const {
	StunAddress xorAddress = *this;

	switch (getFamily()) {
		default:
		case Family::IpV4:
			xorAddress.mAddress = std::get<StunAddressV4>(xorAddress.mAddress) ^ STUN_MAGIC_COOKIE;
			break;
		case Family::IpV6:
			uint32_t magicCookie = htonl(STUN_MAGIC_COOKIE);
			auto *xorAddr = std::get_if<StunAddressV6>(&xorAddress.mAddress);
			for (size_t i = 0; i < 4; i++) {
				xorAddr->octet[i] =
				    std::get<StunAddressV6>(mAddress).octet[i] ^ reinterpret_cast<uint8_t *>(&magicCookie)[i];
			}
			for (size_t i = 0; i < 12; i++) {
				xorAddr->octet[i + 4] =
				    std::get<StunAddressV6>(mAddress).octet[i + 4] ^ transactionId.asUInt96().octet[i];
			}
			break;
	}
	xorAddress.mPort ^= (STUN_MAGIC_COOKIE >> 16);

	return xorAddress;
}

void StunAddress::init(const struct sockaddr *addr) {
	if (addr->sa_family == AF_INET) {
		mPort = ntohs(reinterpret_cast<const struct sockaddr_in *>(addr)->sin_port);
		mAddress = ntohl(reinterpret_cast<const struct sockaddr_in *>(addr)->sin_addr.s_addr);
	} else if (addr->sa_family == AF_INET6) {
		mPort = ntohs(reinterpret_cast<const struct sockaddr_in6 *>(addr)->sin6_port);
		UInt128 v6Addr{};
		memcpy(&v6Addr, reinterpret_cast<const struct sockaddr_in6 *>(addr)->sin6_addr.s6_addr, sizeof(UInt128));
		mAddress = v6Addr;
	}
}

} // namespace mediastreamer::nat
