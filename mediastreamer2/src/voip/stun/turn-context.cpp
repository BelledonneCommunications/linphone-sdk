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
#include <array>

#include "mediastreamer2/turn-context.h"

#include "mediastreamer2/stun-message.h"
#include "mediastreamer2/stun-raw-message.h"

namespace mediastreamer::nat {

TurnContext::~TurnContext() {
	if (mEndpoint != nullptr) {
		delete mEndpoint;
		mEndpoint = nullptr;
	}
}

void TurnContext::allowPeerAddress(const StunAddress &address) {
	if (isPeerAddressAllowed(address)) {
		return;
	}
	mAllowedPeerAddresses.push_back(address);
	mStatistics.nb_successful_create_permission++;
}

RtpTransport *TurnContext::createEndpoint() {
	if (mEndpoint == nullptr) {
		mEndpoint = new RtpTransport();
		mEndpoint->data = this;
		mEndpoint->t_recvfrom = rtpEndpointRecvfrom;
		mEndpoint->t_sendto = rtpEndpointSendto;
		mEndpoint->t_close = rtpEndpointClose;
		mEndpoint->t_destroy = rtpEndpointDestroy;
	}
	return mEndpoint;
}

const std::shared_ptr<TurnTcpClient> &TurnContext::getOrCreateTcpClient() {
	if (mTurnTcpClient == nullptr) {
		mTurnTcpClient = std::make_shared<TurnTcpClient>(this);
	}
	return mTurnTcpClient;
}

void TurnContext::setServerAddress(const SockAddr &sockAddr) {
	// The media sockets are bound in IPv6 so convert the TURN server destination address to IPv6 so that we do not get
	// errors on MacOS
	if ((sockAddr.getFamily() == AF_INET) && (mRtpSession->rtp.gs.sockfamily == AF_INET6)) {
		mServerSockAddr = sockAddr.ipv4ToIpv6();
	} else {
		mServerSockAddr = sockAddr;
	}
}

void TurnContext::setState(State state) {
	mState = state;
	BCTBX_SLOGM << "TurnContext::setState: context=" << this << ", type=" << getTypeStr()
	            << ", state=" << getStateStr();
	if (state == State::AllocationCreated) {
		mStatistics.nb_successful_allocate++;
	} else if (state == State::ChannelBound) {
		mStatistics.nb_successful_channel_bind++;
	}
}

std::string TurnContext::getStateStr() const {
	static const std::array<std::string, 7> stateStrs = {
	    "Idle",           "CreatingAllocation", "AllocationCreated", "CreatingPermissions", "PermissionsCreated",
	    "BindingChannel", "ChannelBound",
	};
	return stateStrs[static_cast<size_t>(mState)];
}

std::string TurnContext::getTypeStr() const {
	static const std::array<std::string, 2> typeStrs = {
	    "RTP",
	    "RTCP",
	};
	return typeStrs[static_cast<size_t>(mType)];
}

bool TurnContext::isPeerAddressAllowed(const StunAddress &address) const {
	return std::any_of(mAllowedPeerAddresses.begin(), mAllowedPeerAddresses.end(),
	                   [address](const auto &stunAddress) { return address == stunAddress; });
}

bool TurnContext::rtpEndpointShouldBeSentToTurnServer(const SockAddr &toAddr) const {
	if (mServerSockAddr.getFamily() != toAddr.getFamily()) {
		return false;
	}
	if (mServerSockAddr.getFamily() == AF_INET) {
		const auto *turnServerSaIn = reinterpret_cast<const struct sockaddr_in *>(mServerSockAddr.asStructSockAddr());
		const auto *toSaIn = reinterpret_cast<const struct sockaddr_in *>(toAddr.asStructSockAddr());
		return (turnServerSaIn->sin_port == toSaIn->sin_port) &&
		       (turnServerSaIn->sin_addr.s_addr == toSaIn->sin_addr.s_addr);
	}
	if (mServerSockAddr.getFamily() == AF_INET6) {
		const auto *turnServerSaIn6 = reinterpret_cast<const struct sockaddr_in6 *>(mServerSockAddr.asStructSockAddr());
		const auto *toSaIn6 = reinterpret_cast<const struct sockaddr_in6 *>(toAddr.asStructSockAddr());
		return (turnServerSaIn6->sin6_port == toSaIn6->sin6_port) &&
		       (memcmp(&turnServerSaIn6->sin6_addr, &toSaIn6->sin6_addr, sizeof(toSaIn6->sin6_addr)) == 0);
	}
	return false;
}

/**
 * This method checks whether the specified source address requires to go through the TURN relay.
 * The convention is that when source address is the relay address of the TURN server, then it has to go through TURN.
 * One exception: if the address is INADDR_ANY or has family AF_UNSPEC, which means that the source address wasn't
 * specified by the originator of the packet, then the default policy of the context in mForcedRtpSendingViaRelay is
 * used. The code is a bit complex because we have to take into account that the source address is a V4 mapped address.
 */
bool TurnContext::rtpEndpointShouldSendViaTurnRelay(const SockAddr &fromAddr) const {
	if ((fromAddr.getFamily() == AF_UNSPEC) || (fromAddr.getLen() == 0)) {
		return mForcedRtpSendingViaRelay;
	}

	const auto relayAddr = mRelayAddress.toSockAddr();
	if (relayAddr.getFamily() == AF_INET) {
		const auto *relaySaIn = reinterpret_cast<const struct sockaddr_in *>(relayAddr.asStructSockAddr());
		if (fromAddr.getFamily() == AF_INET6) {
			// Handle the case of a V4 mapped address.
			const auto fromAddrWithoutV4Mapping = fromAddr.removeV4Mapping();
			if (fromAddrWithoutV4Mapping.getFamily() == AF_INET) {
				const auto *fromSaIn =
				    reinterpret_cast<const struct sockaddr_in *>(fromAddrWithoutV4Mapping.asStructSockAddr());
				return (relaySaIn->sin_addr.s_addr == fromSaIn->sin_addr.s_addr) ||
				       (mForcedRtpSendingViaRelay && (fromSaIn->sin_addr.s_addr == 0));
			}
			// It is a pure IPv6, so this can't match our IPv4 relay address.
			return false;
		}
		const auto *fromSaIn = reinterpret_cast<const struct sockaddr_in *>(fromAddr.asStructSockAddr());
		return (relaySaIn->sin_addr.s_addr == fromSaIn->sin_addr.s_addr) ||
		       (mForcedRtpSendingViaRelay && (fromSaIn->sin_addr.s_addr == 0));
	}
	if (relayAddr.getFamily() == AF_INET6) {
		const auto *relaySaIn6 = reinterpret_cast<const struct sockaddr_in6 *>(relayAddr.asStructSockAddr());
		const auto *fromSaIn6 = reinterpret_cast<const struct sockaddr_in6 *>(fromAddr.asStructSockAddr());
		return (memcmp(&relaySaIn6->sin6_addr, &fromSaIn6->sin6_addr, sizeof(fromSaIn6->sin6_addr)) == 0) ||
		       (mForcedRtpSendingViaRelay &&
		        (memcmp(&fromSaIn6->sin6_addr, &in6addr_any, sizeof(fromSaIn6->sin6_addr)) == 0));
	}
	return false;
}

int TurnContext::rtpEndpointRecvfrom(
    RtpTransport *rtpTransport, mblk_t *msg, int flags, struct sockaddr *from, socklen_t *fromLen) {
	auto *context = static_cast<TurnContext *>(rtpTransport->data);
	if ((context == nullptr) || (context->mRtpSession == nullptr)) {
		return 0;
	}

	int msgsize = 0;

	// Check first if we received a message from turn tcp
	if ((context->mTransport != Transport::Udp) && (context->mTurnTcpClient != nullptr)) {
		msgsize = context->mTurnTcpClient->recvfrom(msg, flags, from, fromLen);
	}

	// If not use the common way
	if (msgsize == 0) {
		msgsize = rtp_session_recvfrom(context->mRtpSession, (context->mType == Type::Rtp) ? TRUE : FALSE, msg, flags,
		                               from, fromLen);
	}

	if ((msgsize < RTP_FIXED_HEADER_SIZE) || (rtp_get_version(msg) == 2)) {
		return msgsize;
	}

	// This is not a RTP packet, try to see if it is a TURN ChannelData message
	if ((context->getState() >= State::BindingChannel) && static_cast<bool>(*msg->b_rptr & 0x40)) {
		const uint16_t channel = ntohs(*(reinterpret_cast<uint16_t *>(msg->b_rptr)));
		const uint16_t dataSize = ntohs(*(reinterpret_cast<uint16_t *>(msg->b_rptr) + 1));

		if ((channel == context->getChannelNumber()) && (msgsize >= (dataSize + 4))) {
			msg->b_rptr += 4; // Unpack the TURN ChannelData message
			context->mStatistics.nb_received_channel_msg++;
		}
	} else {
		// This is not a RTP packet and not a TURN ChannelData message, try to see if it is a STUN one
		const uint16_t stunLen = ntohs(*(reinterpret_cast<uint16_t *>(msg->b_rptr + sizeof(uint16_t))));
		if (msgsize != (stunLen + 20)) {
			return msgsize;
		}

		// It seems to be a STUN packet
		const auto stunMessage = StunMessage::parse(reinterpret_cast<const char *>(msg->b_rptr), msgsize);
		if (stunMessage == nullptr) {
			return msgsize;
		}
		const auto data = stunMessage->getData();
		if (!stunMessage->isIndication() || !data.has_value() || (data->empty())) {
			return msgsize;
		}

		// This is TURN data indication
		const auto peerAddress = stunMessage->getXorPeerAddress();
		if (!peerAddress.has_value()) {
			return msgsize;
		}

		auto permissionAddress = peerAddress.value();
		permissionAddress.setPort(0);
		if (!context->isPeerAddressAllowed(permissionAddress)) {
			return msgsize;
		}

		// Copy the data of the TURN data indication in the mblk_t so that it contains the unpacked data
		msgsize = static_cast<int>(data->size());
		memcpy(msg->b_rptr, data->data(), data->size());
		// Overwrite the ortp_recv_addr of the mblk_t so that ICE source address is correct
		const auto relaySockAddr = context->mRelayAddress.toSockAddr();
		const auto *const relaySa = relaySockAddr.asStructSockAddr();
		msg->recv_addr.family = relaySa->sa_family;
		if (relaySa->sa_family == AF_INET) {
			msg->recv_addr.addr.ipi_addr = (reinterpret_cast<const struct sockaddr_in *>(relaySa))->sin_addr;
			msg->recv_addr.port = (reinterpret_cast<const struct sockaddr_in *>(relaySa))->sin_port;
		} else if (relaySa->sa_family == AF_INET6) {
			memcpy(&msg->recv_addr.addr.ipi6_addr, &(reinterpret_cast<const struct sockaddr_in6 *>(relaySa))->sin6_addr,
			       sizeof(struct in6_addr));
			msg->recv_addr.port = (reinterpret_cast<const struct sockaddr_in6 *>(relaySa))->sin6_port;
		} else {
			BCTBX_SLOGW << "turn: Unknown address family in mRelayAddress";
			msgsize = 0;
		}
		// Overwrite the source address of the packet so that it uses the peer address instead of the TURN server one
		const auto peerSockAddr = peerAddress->toSockAddr();
		*fromLen = peerSockAddr.getLen();
		memcpy(from, peerSockAddr.asStructSockAddr(), *fromLen);
		if (msgsize > 0) {
			context->mStatistics.nb_data_indication++;
		}
	}
	return msgsize;
}

int TurnContext::rtpEndpointSendto(
    RtpTransport *rtpTransport, mblk_t *msg, int flags, const struct sockaddr *to, socklen_t toLen) {
	auto *context = static_cast<TurnContext *>(rtpTransport->data);
	if ((context == nullptr) || (context->mRtpSession == nullptr)) {
		return 0;
	}

	auto toAddr = SockAddr(to, toLen);
	const auto sourceAddr = SockAddr(&msg->recv_addr);
	// msg->recv_addr is internally set to the TURN server for RTP/RTCP packets that need to go through the TURN server.
	// However, this does not mean that the socket needs to use this address (which makes no sense). Hence, reset the
	// recv_addr so that it is not taken into account by the transport layer.
	memset(&msg->recv_addr, 0, sizeof(msg->recv_addr));

	bool sendViaTurnTcp = false;
	mblk_t *newMsg = nullptr; // If a new mblk_t is forged, it will have to be destroyed at the end of this function.
	                          // 'msg' itself is freed by the caller.
	int ret = static_cast<int>(msgdsize(msg));
	if (context->rtpEndpointShouldBeSentToTurnServer(toAddr)) {
		if (context->mTransport != Transport::Udp) {
			sendViaTurnTcp = true;
		}
	} else if (context->rtpEndpointShouldSendViaTurnRelay(sourceAddr)) {
		if (context->getState() >= State::ChannelBound) {
			// Use a TURN ChannelData message
			mblk_t *header = allocb(4, 0);
			*(reinterpret_cast<uint16_t *>(header->b_wptr)) = htons(context->getChannelNumber().value());
			header->b_wptr += 2;
			*(reinterpret_cast<uint16_t *>(header->b_wptr)) = htons(static_cast<uint16_t>(ret));
			header->b_wptr += 2;
			concatb(header, dupmsg(msg));
			newMsg = msg = header;
			context->mStatistics.nb_sent_channel_msg++;
		} else {
			// Use a TURN send indication to encapsulate the data to be sent
			msgpullup(msg, -1);
			const auto stunMessage = StunMessage::createTurnSendIndication(StunAddress(toAddr.ipv6toIpv4()));
			stunMessage->setData(reinterpret_cast<const char *>(msg->b_rptr),
			                     static_cast<size_t>(msg->b_wptr - msg->b_rptr));
			const auto stunRawMessage = stunMessage->encode();
			const auto data = stunRawMessage->getData();
			auto *buf = static_cast<uint8_t *>(ms_malloc(data.size()));
			memcpy(buf, data.data(), data.size());

			// Allocate a new mblk_t englobing the STUN encoded message.
			newMsg = msg = esballoc(buf, data.size(), 0, ms_free);
			msg->b_wptr += data.size();
			context->mStatistics.nb_send_indication++;
		}

		toAddr = context->mServerSockAddr;
		if (context->getTransport() != Transport::Udp) {
			sendViaTurnTcp = true;
		}
	}

	int subRet = 0;
	if (sendViaTurnTcp && (context->mTurnTcpClient != nullptr)) {
		subRet = context->mTurnTcpClient->sendto(msg, flags, toAddr.asStructSockAddr(), toAddr.getLen());
	} else {
		subRet = rtp_session_sendto(context->mRtpSession, (context->mType == Type::Rtp) ? TRUE : FALSE, msg, flags,
		                            toAddr.asStructSockAddr(), toAddr.getLen());
	}

	if (newMsg != nullptr) {
		freemsg(newMsg);
	}
	return subRet > 0 ? ret : subRet; // The sendto() function shall not return more or less than requested. The same
	                                  // amount, otherwise an error.
}

void TurnContext::rtpEndpointClose(RtpTransport *rtpTransport) {
	auto *context = static_cast<TurnContext *>(rtpTransport->data);
	if (context != nullptr) {
		context->mRtpSession = nullptr;
	}
}

void TurnContext::rtpEndpointDestroy(RtpTransport *rtpTransport) {
	static_cast<TurnContext *>(rtpTransport->data)->mEndpoint = nullptr;
	delete rtpTransport;
}

} // namespace mediastreamer::nat
