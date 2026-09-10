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

#include <bctoolbox/port.h>

#include "mediastreamer2/ice-utils.h"

#include "mediastreamer2/ice-constants.h"
#include "mediastreamer2/sockaddr.h"
#include "mediastreamer2/stun-error.h"
#include "mediastreamer2/stun-message.h"
#include "mediastreamer2/stun-raw-message.h"

namespace ms2::nat {

uint16_t getComponentIdFromEventData(const OrtpEventData *eventData) {
	if (eventData->info.socket_type == OrtpRTPSocket) {
		return ICE_RTP_COMPONENT_ID;
	}
	if (eventData->info.socket_type == OrtpRTCPSocket) {
		return ICE_RTCP_COMPONENT_ID;
	}
	BCTBX_SLOGE << "ice: Invalid OrtpEventData (socket_type=" << static_cast<int>(eventData->info.socket_type) << ")";
	return ICE_INVALID_COMPONENT_ID;
}

OrtpStream *getOrtpStreamFromRtpSessionAndComponentId(RtpSession *rtpSession, uint16_t componentId) {
	if (componentId == ICE_RTP_COMPONENT_ID) {
		return &rtpSession->rtp.gs;
	}
	if (componentId == ICE_RTCP_COMPONENT_ID) {
		return &rtpSession->rtcp.gs;
	}
	return nullptr;
}

RtpTransport *getTransportFromRtpSession(const RtpSession *rtpSession, const OrtpEventData *eventData) {
	RtpTransport *rtpTransport = nullptr;
	if (eventData->info.socket_type == OrtpRTPSocket) {
		rtp_session_get_transports(rtpSession, &rtpTransport, nullptr);
	} else if (eventData->info.socket_type == OrtpRTCPSocket) {
		rtp_session_get_transports(rtpSession, nullptr, &rtpTransport);
	}
	return rtpTransport;
}

void sendErrorResponse(const RtpSession *rtpSession,
                       const OrtpEventData *eventData,
                       const std::shared_ptr<StunMessage> &msg,
                       const StunAddress &destStunAddress,
                       const StunError &error) {
	RtpTransport *rtpTransport = getTransportFromRtpSession(rtpSession, eventData);
	if (rtpTransport == nullptr) {
		return;
	}

	// Create the error response, copying the transaction ID from the request.
	const auto transactionId = msg->getTransactionId();
	const auto response = StunMessage::createStunBindingErrorResponse();
	response->enableFingerprint(true);
	response->setError(error);

	const auto rawStunMessage = response->encode();
	if (rawStunMessage != nullptr) {
		const auto destAddr = destStunAddress.toSockAddr();
		const auto sourceAddr = SockAddr(&eventData->packet->recv_addr).ipv6toIpv4();
		const auto data = rawStunMessage->getData();
		BCTBX_SLOGM << "IceCheckList::sendErrorResponse: Send error response: " << sourceAddr.asString() << " --> "
		            << destAddr.asString() << " [" << transactionId.asString() << "]";
		sendMessageToSocket(rtpTransport, reinterpret_cast<const char *>(data.data()), data.size(),
		                    sourceAddr.asStructSockAddr(), destAddr.asStructSockAddr(), destAddr.getLen());
	}
}

int sendMessageToSocket(RtpTransport *rtpTransport,
                        const char *buf,
                        const size_t len,
                        const struct sockaddr *from,
                        const struct sockaddr *to,
                        socklen_t toLen) {
	mblk_t *m = rtp_session_create_packet_raw(reinterpret_cast<const uint8_t *>(buf), len);
	struct addrinfo *v6ai = nullptr;

	if (from != nullptr) {
		ortp_sockaddr_to_recvaddr(from, &m->recv_addr);
	}
	if ((rtpTransport->session->rtp.gs.sockfamily == AF_INET6) && (to->sa_family == AF_INET)) {
		char toAddrStr[64];
		int toPort = 0;
		memset(toAddrStr, 0, sizeof(toAddrStr));
		bctbx_sockaddr_to_ip_address(to, toLen, toAddrStr, sizeof(toAddrStr), &toPort);
		v6ai = bctbx_ip_address_to_addrinfo(AF_INET6, SOCK_DGRAM, toAddrStr, toPort);
		to = v6ai->ai_addr;
		toLen = static_cast<socklen_t>(v6ai->ai_addrlen);
	}
	const auto err = meta_rtp_transport_modifier_inject_packet_to_send_to(rtpTransport, nullptr, m, 0, to, toLen);
	freemsg(m);
	if (v6ai != nullptr) {
		bctbx_freeaddrinfo(v6ai);
	}
	return err;
}

int sendMessageToStunAddress(
    RtpTransport *rtpTransport, const char *buf, const size_t len, const StunAddress &source, const StunAddress &dest) {
	const auto sourceAddr = source.toSockAddr();
	const auto destAddr = dest.toSockAddr();
	return sendMessageToSocket(rtpTransport, buf, len, sourceAddr.asStructSockAddr(), destAddr.asStructSockAddr(),
	                           destAddr.getLen());
}

} // namespace ms2::nat
