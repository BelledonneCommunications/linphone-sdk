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

namespace ms2::IceUtils {

SockAddr::SockAddr(const ortp_recv_addr *ortpRecvAddr) {
	ortp_recvaddr_to_sockaddr(const_cast<ortp_recv_addr *>(ortpRecvAddr), reinterpret_cast<struct sockaddr *>(&mAddr),
	                          &mLen);
}

SockAddr::SockAddr(const struct sockaddr *addr, const socklen_t addrLen) : mLen(addrLen) {
	memcpy(&mAddr, addr, addrLen);
}

SockAddr::SockAddr(const MSStunAddress &stunAddr) {
	ms_stun_address_to_sockaddr(&stunAddr, reinterpret_cast<struct sockaddr *>(&mAddr), &mLen);
}

SockAddr SockAddr::ipv6toIpv4() const {
	SockAddr result;
	bctbx_sockaddr_ipv6_to_ipv4(reinterpret_cast<const struct sockaddr *>(&mAddr),
	                            reinterpret_cast<struct sockaddr *>(&result.mAddr), &result.mLen);
	return result;
}

std::string SockAddr::asString() const {
	std::string output;
	output.resize(64, '\0');
	bctbx_sockaddr_to_printable_ip_address(
	    const_cast<struct sockaddr *>(reinterpret_cast<const struct sockaddr *>(&mAddr)), mLen, output.data(),
	    output.size());
	return output;
}

MSStunAddress SockAddr::toStunAddress() const {
	MSStunAddress stunAddr;
	ms_sockaddr_to_stun_address(asStructSockAddr(), &stunAddr);
	return stunAddr;
}

uint16_t getComponentIdFromEventData(const OrtpEventData *eventData) {
	if (eventData->info.socket_type == OrtpRTPSocket) {
		return ICE_RTP_COMPONENT_ID;
	}
	if (eventData->info.socket_type == OrtpRTCPSocket) {
		return ICE_RTCP_COMPONENT_ID;
	}
	ms_error("ice: Invalid OrtpEventData (socket_type=%i)", static_cast<int>(eventData->info.socket_type));
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

std::string getTransactionIdStr(const UInt96 transactionId) {
	std::ostringstream oss;
	const auto *bytes = reinterpret_cast<const unsigned char *>(&transactionId);
	for (int i = 0; i < 12; i++) {
		oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(bytes[i]);
	}
	return oss.str();
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
                       const MSStunMessage *msg,
                       const MSStunAddress &destStunAddress,
                       const uint16_t errorNum,
                       const std::string &errorMsg) {
	RtpTransport *rtpTransport = getTransportFromRtpSession(rtpSession, eventData);
	if (rtpTransport == nullptr) {
		return;
	}

	// Create the error response, copying the transaction ID from the request.
	const UInt96 transactionId = ms_stun_message_get_tr_id(msg);
	MSStunMessage *response = ms_stun_binding_error_response_create();
	ms_stun_message_enable_fingerprint(response, TRUE);
	ms_stun_message_set_error_code(response, errorNum, errorMsg.c_str());

	char *buf = nullptr;
	const size_t len = ms_stun_message_encode(response, &buf);
	if (len > 0) {
		const auto destAddr = SockAddr(destStunAddress);
		const auto sourceAddr = SockAddr(&eventData->packet->recv_addr).ipv6toIpv4();
		ms_message("IceCheckList::sendErrorResponse: Send error response: %s --> %s [%s]",
		           sourceAddr.asString().c_str(), destAddr.asString().c_str(),
		           getTransactionIdStr(transactionId).c_str());
		sendMessageToSocket(rtpTransport, buf, len, sourceAddr.asStructSockAddr(), destAddr.asStructSockAddr(),
		                    destAddr.getLen());
	}
	if (buf != nullptr) {
		ms_free(buf);
	}
	ms_stun_message_destroy(response);
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

int sendMessageToStunAddress(RtpTransport *rtpTransport,
                             const char *buf,
                             const size_t len,
                             const MSStunAddress &source,
                             const MSStunAddress &dest) {
	const auto sourceAddr = SockAddr(source);
	const auto destAddr = SockAddr(dest);
	return sendMessageToSocket(rtpTransport, buf, len, sourceAddr.asStructSockAddr(), destAddr.asStructSockAddr(),
	                           destAddr.getLen());
}

} // namespace ms2::IceUtils
