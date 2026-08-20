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

#include <ortp/rtpsession.h>

#include "mediastreamer2/stun.h"

namespace ms2::IceUtils {

class SockAddr {
public:
	SockAddr() = default;
	SockAddr(const struct sockaddr *addr, socklen_t addrLen);
	explicit SockAddr(const ortp_recv_addr *ortpRecvAddr);
	explicit SockAddr(const MSStunAddress &stunAddr);
	~SockAddr() = default;

	[[nodiscard]] std::string asString() const;
	[[nodiscard]] const struct sockaddr *asStructSockAddr() const {
		return reinterpret_cast<const struct sockaddr *>(&mAddr);
	};
	[[nodiscard]] socklen_t getLen() const {
		return mLen;
	}
	[[nodiscard]] SockAddr ipv6toIpv4() const;
	[[nodiscard]] MSStunAddress toStunAddress() const;

private:
	struct sockaddr_storage mAddr{};
	socklen_t mLen = sizeof(mAddr);
};

uint16_t getComponentIdFromEventData(const OrtpEventData *eventData);

OrtpStream *getOrtpStreamFromRtpSessionAndComponentId(RtpSession *rtpSession, uint16_t componentId);

std::string getTransactionIdStr(UInt96 transactionId);

RtpTransport *getTransportFromRtpSession(const RtpSession *rtpSession, const OrtpEventData *eventData);

void sendErrorResponse(const RtpSession *rtpSession,
                       const OrtpEventData *eventData,
                       const MSStunMessage *msg,
                       const MSStunAddress &destStunAddress,
                       uint16_t errorNum,
                       const std::string &errorMsg);

int sendMessageToSocket(RtpTransport *rtpTransport,
                        const char *buf,
                        size_t len,
                        const struct sockaddr *from,
                        const struct sockaddr *to,
                        socklen_t toLen);

int sendMessageToStunAddress(
    RtpTransport *rtpTransport, const char *buf, size_t len, const MSStunAddress &source, const MSStunAddress &dest);

} // namespace ms2::IceUtils
