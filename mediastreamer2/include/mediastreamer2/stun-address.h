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

#include <optional>
#include <variant>

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/sockaddr.h"
#include "mediastreamer2/stun-transaction-id.h"

namespace mediastreamer::nat {

static constexpr uint32_t STUN_MAGIC_COOKIE = 0x2112A442;

using StunAddressV4 = uint32_t;
using StunAddressV6 = UInt128;

class MS2_PUBLIC StunAddress {
public:
	friend class StunRawMessage;
	friend class TurnContext;

	enum class Family {
		IpV4 = 0x01,
		IpV6 = 0x02,
	};

	StunAddress() = default;
	StunAddress(const StunAddress &other) = default;
	explicit StunAddress(const struct sockaddr *addr);
	explicit StunAddress(const SockAddr &sockAddr);
	StunAddress(int aiFamily, int sockType, const std::string &hostname, int port);
	~StunAddress() = default;

	bool operator==(const StunAddress &other) const;
	bool operator!=(const StunAddress &other) const {
		return !(*this == other);
	}

	[[nodiscard]] Family getFamily() const;
	[[nodiscard]] std::optional<StunAddressV4> getIpV4Address() const;
	[[nodiscard]] std::optional<StunAddressV6> getIpV6Address() const;
	[[nodiscard]] uint16_t getPort() const {
		return mPort;
	}
	void setAddress(const StunAddressV4 addr) {
		mAddress = addr;
	}
	void setAddress(const StunAddressV6 addr) {
		mAddress = addr;
	}
	void setPort(const uint16_t port) {
		mPort = port;
	}
	[[nodiscard]] SockAddr toSockAddr() const;
	[[nodiscard]] StunAddress toXor(const StunTransactionId &transactionId) const;

private:
	void init(const struct sockaddr *addr);

	uint16_t mPort = 0;
	std::variant<StunAddressV4, StunAddressV6> mAddress = StunAddressV4{};
};

}; // namespace mediastreamer::nat