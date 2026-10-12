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
#include <optional>
#include <stdexcept>

#include "mediastreamer2/turn-context.h"

#include "mediastreamer2/stun-message.h"
#include "mediastreamer2/stun-raw-message.h"
#include "mediastreamer2/turn-client.h"
#include "mediastreamer2/turn-transport.h"

namespace mediastreamer::nat {

void TurnTransportContext::allowPeerAddress(const StunAddress &address) {
	if (isPeerAddressAllowed(address)) {
		return;
	}
	mAllowedPeerAddresses.push_back(address);
	mStatistics.nbSuccessfulCreatePermission++;
}

void TurnTransportContext::setState(const State state) {
	mState = state;
	BCTBX_SLOGM << "TurnTransportContext::setState: context=" << mContext << ", type=" << mContext->getTypeStr()
	            << ", transport=" << turnTransportAsString(mTransport) << ", state=" << getStateStr();
	if (state == State::AllocationCreated) {
		mStatistics.nbSuccessfulAllocate++;
	} else if (state == State::ChannelBound) {
		mStatistics.nbSuccessfulChannelBind++;
	}
}

std::string TurnTransportContext::getStateStr() const {
	static const std::array<std::string, 8> stateStrs = {
	    "Idle",         "CreatingAllocation", "AllocationCreated", "CreatingPermissions", "PermissionsCreated",
	    "Deallocating", "BindingChannel",     "ChannelBound",
	};
	return stateStrs[static_cast<size_t>(mState)];
}

bool TurnTransportContext::isPeerAddressAllowed(const StunAddress &address) const {
	return std::any_of(mAllowedPeerAddresses.begin(), mAllowedPeerAddresses.end(),
	                   [address](const auto &stunAddress) { return address == stunAddress; });
}

TurnContext::TurnContext(const Type type, RtpSession *rtpSession) : mRtpSession(rtpSession), mType(type) {
}

TurnContext::~TurnContext() {
	if (mEndpoint != nullptr) {
		delete mEndpoint;
		mEndpoint = nullptr;
	}
}

bool TurnContext::allStatesAreIdle() const {
	return std::none_of(mTransportContextMap.begin(), mTransportContextMap.end(),
	                    [](const auto &transportContextEntry) {
		                    return transportContextEntry.second.getState() != TurnTransportContext::State::Idle;
	                    });
}

void TurnContext::connect() {
	for (auto &[transport, transportContext] : mTransportContextMap) {
		switch (transport) {
			case TurnTransport::Udp:
			default:
				transportContext.mTurnClient = std::make_shared<TurnUdpClient>(this);
				break;
			case TurnTransport::Tcp:
				transportContext.mTurnClient = std::make_shared<TurnTcpClient>(this);
				break;
			case TurnTransport::Tls:
				transportContext.mTurnClient = std::make_shared<TurnTlsClient>(this);
				break;
		}
		transportContext.mTurnClient->connect();
	}
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

TurnTransportContext *TurnContext::getTransportContext(const TurnTransport transport) const {
	try {
		return const_cast<TurnTransportContext *>(&mTransportContextMap.at(transport));
	} catch (std::out_of_range &) {
		return nullptr;
	}
}

void TurnContext::setServerAddresses(const std::map<TurnTransport, SockAddr> &serverSockAddrMap) {
	for (const auto transport : {TurnTransport::Udp, TurnTransport::Tcp, TurnTransport::Tls}) {
		const auto it = serverSockAddrMap.find(transport);
		if (it == serverSockAddrMap.end()) {
			mTransportContextMap.erase(transport);
			continue;
		}

		auto *transportContext = getTransportContext(transport);
		if (transportContext == nullptr) {
			mTransportContextMap.emplace(transport, TurnTransportContext(transport, this));
			transportContext = getTransportContext(transport);
		}
		// The media sockets are bound in IPv6 so convert the TURN server destination address to IPv6 so that we do not
		// get errors on MacOS
		const auto sockAddr = serverSockAddrMap.at(transport);
		if ((sockAddr.getFamily() == AF_INET) && (mRtpSession->rtp.gs.sockfamily == AF_INET6)) {
			transportContext->mServerSockAddr = sockAddr.ipv4ToIpv6();
		} else {
			transportContext->mServerSockAddr = sockAddr;
		}
	}
}

std::shared_ptr<TurnClient> TurnContext::getTurnClient(TurnTransport transport) const {
	const auto *turnTransportContext = getTransportContext(transport);
	if (turnTransportContext == nullptr) {
		return nullptr;
	}
	return turnTransportContext->mTurnClient;
}

std::string TurnContext::getTypeStr() const {
	static const std::array<std::string, 2> typeStrs = {
	    "RTP",
	    "RTCP",
	};
	return typeStrs[static_cast<size_t>(mType)];
}

bool TurnContext::rtpEndpointShouldBeSentToTurnServer(const SockAddr &toAddr, const TurnTransport transport) const {
	const auto *transportContext = getTransportContext(transport);
	if (transportContext == nullptr) {
		return false;
	}
	const auto serverSockAddr = transportContext->getServerSockAddr();
	if (serverSockAddr.getFamily() != toAddr.getFamily()) {
		return false;
	}
	if (serverSockAddr.getFamily() == AF_INET) {
		const auto *turnServerSaIn = reinterpret_cast<const struct sockaddr_in *>(serverSockAddr.asStructSockAddr());
		const auto *toSaIn = reinterpret_cast<const struct sockaddr_in *>(toAddr.asStructSockAddr());
		return (turnServerSaIn->sin_port == toSaIn->sin_port) &&
		       (turnServerSaIn->sin_addr.s_addr == toSaIn->sin_addr.s_addr);
	}
	if (serverSockAddr.getFamily() == AF_INET6) {
		const auto *turnServerSaIn6 = reinterpret_cast<const struct sockaddr_in6 *>(serverSockAddr.asStructSockAddr());
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
bool TurnContext::rtpEndpointShouldSendViaTurnRelay(const TurnTransport transport, const SockAddr &fromAddr) const {
	const auto *transportContext = getTransportContext(transport);
	if (transportContext == nullptr) {
		return false;
	}

	if ((fromAddr.getFamily() == AF_UNSPEC) || (fromAddr.getLen() == 0)) {
		return mForcedRtpSendingViaRelay;
	}

	const auto relayAddr = transportContext->getRelayAddress().toSockAddr();
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

int TurnContext::recvFrom(mblk_t *msg, int flags, struct sockaddr *from, socklen_t *fromLen) const {
	int msgsize = 0;

	// Check first if we received a message from the TURN clients (with the priority: TLS - TCP - UDP)
	if (getTurnClient(TurnTransport::Tls) != nullptr) {
		msgsize = getTurnClient(TurnTransport::Tls)->recvfrom(msg, flags, from, fromLen);
	}
	if ((msgsize == 0) && (getTurnClient(TurnTransport::Tcp) != nullptr)) {
		msgsize = getTurnClient(TurnTransport::Tcp)->recvfrom(msg, flags, from, fromLen);
	}
	if ((msgsize == 0) && (getTurnClient(TurnTransport::Udp) != nullptr)) {
		msgsize = getTurnClient(TurnTransport::Udp)->recvfrom(msg, flags, from, fromLen);
	}

	// If not use the common way
	if (msgsize == 0) {
		msgsize = rtp_session_recvfrom(mRtpSession, (mType == Type::Rtp) ? TRUE : FALSE, msg, flags, from, fromLen);
	}

	if ((msgsize < RTP_FIXED_HEADER_SIZE) || (rtp_get_version(msg) == 2)) {
		return msgsize;
	}

	const auto transport = getTurnTransportFromMsg(msg);
	auto *transportContext = getTransportContext(transport);
	if (transportContext == nullptr) {
		return msgsize;
	}

	// This is not a RTP packet, try to see if it is a TURN ChannelData message
	if ((transportContext->getState() >= TurnTransportContext::State::BindingChannel) &&
	    static_cast<bool>(*msg->b_rptr & 0x40)) {
		const uint16_t channel = ntohs(*(reinterpret_cast<uint16_t *>(msg->b_rptr)));
		const uint16_t dataSize = ntohs(*(reinterpret_cast<uint16_t *>(msg->b_rptr) + 1));

		if ((channel == transportContext->getChannelNumber()) && (msgsize >= (dataSize + 4))) {
			msg->b_rptr += 4; // Unpack the TURN ChannelData message
			transportContext->mStatistics.nbReceivedChannelMsg++;
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
		if (!stunMessage->isIndication() || !data || (data->empty())) {
			return msgsize;
		}

		// This is TURN data indication
		const auto peerAddress = stunMessage->getXorPeerAddress();
		if (!peerAddress) {
			return msgsize;
		}

		auto permissionAddress = peerAddress.value();
		permissionAddress.setPort(0);
		if (!transportContext->isPeerAddressAllowed(permissionAddress)) {
			return msgsize;
		}

		// Copy the data of the TURN data indication in the mblk_t so that it contains the unpacked data
		msgsize = static_cast<int>(data->size());
		memcpy(msg->b_rptr, data->data(), data->size());
		// Overwrite the ortp_recv_addr of the mblk_t so that ICE source address is correct
		const auto relaySockAddr = transportContext->getRelayAddress().toSockAddr();
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
			transportContext->mStatistics.nbDataIndication++;
		}
	}
	return msgsize;
}

int TurnContext::sendTo(mblk_t *msg, int flags, const struct sockaddr *to, socklen_t toLen) const {
	auto toAddr = SockAddr(to, toLen);
	const auto sourceAddr = SockAddr(&msg->recv_addr);
	// msg->recv_addr is internally set to the TURN server for RTP/RTCP packets that need to go through the TURN server.
	// However, this does not mean that the socket needs to use this address (which makes no sense). Hence, reset the
	// recv_addr so that it is not taken into account by the transport layer.
	memset(&msg->recv_addr, 0, sizeof(msg->recv_addr));
	auto transport = getTurnTransportFromMsg(msg);
	auto *transportContext = getTransportContext(transport);

	mblk_t *newMsg = nullptr; // If a new mblk_t is forged, it will have to be destroyed at the end of this function.
	                          // 'msg' itself is freed by the caller.
	const int ret = static_cast<int>(msgdsize(msg));
	if (rtpEndpointShouldBeSentToTurnServer(toAddr, transport)) {
	} else if (rtpEndpointShouldSendViaTurnRelay(transport, sourceAddr)) {
		if (transportContext->getState() >= TurnTransportContext::State::ChannelBound) {
			// Use a TURN ChannelData message
			mblk_t *header = allocb(4, 0);
			*(reinterpret_cast<uint16_t *>(header->b_wptr)) = htons(transportContext->getChannelNumber().value());
			header->b_wptr += 2;
			*(reinterpret_cast<uint16_t *>(header->b_wptr)) = htons(static_cast<uint16_t>(ret));
			header->b_wptr += 2;
			concatb(header, dupmsg(msg));
			newMsg = msg = header;
			transportContext->mStatistics.nbSentChannelMsg++;
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
			transportContext->mStatistics.nbSendIndication++;
		}

		toAddr = transportContext->getServerSockAddr();
	} else {
		transport = TurnTransport::Udp;
	}

	int subRet = 0;
	if (getTurnClient(transport) != nullptr) {
		subRet = getTurnClient(transport)->sendto(msg, flags, toAddr.asStructSockAddr(), toAddr.getLen());
	} else {
		subRet = rtp_session_sendto(mRtpSession, (mType == Type::Rtp) ? TRUE : FALSE, msg, flags,
		                            toAddr.asStructSockAddr(), toAddr.getLen());
	}

	if (newMsg != nullptr) {
		freemsg(newMsg);
	}
	return subRet > 0 ? ret : subRet; // The sendto() function shall not return more or less than requested. The same
	                                  // amount, otherwise an error.
}

TurnTransport TurnContext::getTurnTransportFromMsg(const mblk_t *msg) {
	return fromOrtpTurnTransport(msg->turn_transport);
}

void TurnContext::setTurnTransportInMsg(mblk_t *msg, const TurnTransport turnTransport) {
	msg->turn_transport = toOrtpTurnTransport(turnTransport);
}

int TurnContext::rtpEndpointRecvfrom(
    RtpTransport *rtpTransport, mblk_t *msg, int flags, struct sockaddr *from, socklen_t *fromLen) {
	auto *context = static_cast<TurnContext *>(rtpTransport->data);
	if ((context == nullptr) || (context->mRtpSession == nullptr)) {
		return 0;
	}
	return context->recvFrom(msg, flags, from, fromLen);
}

int TurnContext::rtpEndpointSendto(
    RtpTransport *rtpTransport, mblk_t *msg, int flags, const struct sockaddr *to, socklen_t toLen) {
	auto *context = static_cast<TurnContext *>(rtpTransport->data);
	if ((context == nullptr) || (context->mRtpSession == nullptr)) {
		return 0;
	}
	return context->sendTo(msg, flags, to, toLen);
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
