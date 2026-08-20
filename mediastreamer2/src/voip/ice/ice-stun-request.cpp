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

#include "mediastreamer2/ice-stun-request.h"

#include "mediastreamer2/ice-utils.h"

namespace ms2 {

std::string IceStunRequest::Transaction::getIdStr() const {
	return IceUtils::getTransactionIdStr(mId);
}

std::optional<std::chrono::milliseconds> IceStunRequest::Transaction::getRoundTripTime() const {
	if (!mResponseTime.has_value()) {
		return std::nullopt;
	}
	return std::chrono::duration_cast<std::chrono::milliseconds>(*mResponseTime - mRequestTime);
}

void IceStunRequest::Transaction::setResponseTime(const MSTimeSpec &responseTime) {
	const auto duration = std::chrono::seconds(responseTime.tv_sec) + std::chrono::nanoseconds(responseTime.tv_nsec);
	mResponseTime = std::chrono::steady_clock::time_point{
	    std::chrono::duration_cast<std::chrono::steady_clock::duration>(duration)};
}

IceStunRequest::~IceStunRequest() {
	if (mSourceAddrInfo != nullptr) {
		bctbx_freeaddrinfo(mSourceAddrInfo);
	}
}

std::shared_ptr<IceStunRequest> IceStunRequest::create(MSTurnContext *turnContext,
                                                       RtpTransport *rtpTransport,
                                                       const IceTransportAddress &transportAddress,
                                                       const uint16_t stunMethod) {
	auto request =
	    std::shared_ptr<IceStunRequest>(new IceStunRequest(turnContext, rtpTransport, transportAddress, stunMethod));
	if (request->getSourceAddrInfo() == nullptr) {
		ms_error("IceStunServerRequest::create(): source address not defined");
		return nullptr;
	}
	return request;
}

//------------------------------------------------------------------------------

IceStunRequest::IceStunRequest(MSTurnContext *turnContext,
                               RtpTransport *rtpTransport,
                               const IceTransportAddress &transportAddress,
                               const uint16_t stunMethod)
    : mRtpTransport(rtpTransport), mTurnContext(turnContext), mStunMethod(stunMethod) {
	mSourceAddrInfo = bctbx_ip_address_to_addrinfo(transportAddress.getFamily(), SOCK_DGRAM,
	                                               transportAddress.getIp().c_str(), transportAddress.getPort());
}

void IceStunRequest::addTransaction(const std::shared_ptr<Transaction> &transaction) {
	mTransactions.push_back(transaction);
}

void IceStunRequest::fillStunMessageAuthenticationFromTurnContext(MSStunMessage *msg) const {
	ms_stun_message_set_realm(msg, ms_turn_context_get_realm(mTurnContext));
	ms_stun_message_set_nonce(msg, ms_turn_context_get_nonce(mTurnContext));
	ms_stun_message_set_username(msg, ms_turn_context_get_username(mTurnContext));
	ms_stun_message_set_password(msg, ms_turn_context_get_password(mTurnContext));
	ms_stun_message_set_ha1(msg, ms_turn_context_get_ha1(mTurnContext));
	if ((ms_turn_context_get_password(mTurnContext) != nullptr) || (ms_turn_context_get_ha1(mTurnContext) != nullptr)) {
		ms_stun_message_enable_message_integrity(msg, TRUE);
	}
}

std::vector<IceStunRequest::RoundTripTime> IceStunRequest::getGatheringRoundTripTimes() const {
	std::vector<IceStunRequest::RoundTripTime> roundTripTimes;
	if (mGathering) {
		for (const auto &transaction : mTransactions) {
			const auto rtt = transaction->getRoundTripTime();
			if (rtt.has_value()) {
				roundTripTimes.emplace_back(*rtt);
			}
		}
	}
	return roundTripTimes;
}

std::shared_ptr<IceStunRequest::Transaction> IceStunRequest::getTransaction(const UInt96 &transactionId) const {
	const auto it = std::find_if(mTransactions.begin(), mTransactions.end(), [transactionId](const auto &transaction) {
		return memcmp(&transaction->getId(), &transactionId, sizeof(UInt96)) == 0;
	});
	if (it == mTransactions.end()) {
		return nullptr;
	}
	return *it;
}

bool IceStunRequest::needRetransmission() const {
	return std::chrono::steady_clock::now() >= mNextTransmissionTime;
}

std::shared_ptr<IceStunRequest::Transaction> IceStunRequest::send(const IceUtils::SockAddr &server) {
	std::shared_ptr<IceStunRequest::Transaction> transaction = nullptr;
	switch (mStunMethod) {
		case MS_STUN_METHOD_BINDING:
			transaction = sendStunBindingRequest(server);
			break;
		case MS_TURN_METHOD_ALLOCATE:
			transaction = sendTurnAllocateRequest(server);
			break;
		case MS_TURN_METHOD_CREATE_PERMISSION:
			transaction = sendTurnCreatePermissionRequest(server);
			break;
		case MS_TURN_METHOD_REFRESH:
			transaction = sendTurnRefreshRequest(server);
			break;
		case MS_TURN_METHOD_CHANNEL_BIND:
			transaction = sendTurnChannelBindRequest(server);
			break;
		default:
			break;
	}
	return transaction;
}

std::shared_ptr<IceStunRequest::Transaction> IceStunRequest::send(const IceUtils::SockAddr &serverAddr,
                                                                  const MSStunMessage *msg,
                                                                  const std::string &requestType) const {
	std::shared_ptr<IceStunRequest::Transaction> transaction = nullptr;
	char *buf = nullptr;
	const size_t len = ms_stun_message_encode(msg, &buf);
	if (len > 0) {
		transaction = std::make_shared<IceStunRequest::Transaction>(ms_stun_message_get_tr_id(msg));
		const auto transactionId = transaction->getIdStr();
		const auto destAddress = serverAddr.ipv6toIpv4();
		const auto sourceAddress =
		    IceUtils::SockAddr(mSourceAddrInfo->ai_addr, static_cast<socklen_t>(mSourceAddrInfo->ai_addrlen));
		ms_message("ice: Send %s: %s --> %s [%s]", requestType.c_str(), sourceAddress.asString().c_str(),
		           destAddress.asString().c_str(), transactionId.c_str());
		IceUtils::sendMessageToSocket(mRtpTransport, buf, len, sourceAddress.asStructSockAddr(),
		                              destAddress.asStructSockAddr(), destAddress.getLen());
	} else {
		ms_error("ice: encoding %s failed", requestType.c_str());
	}
	if (buf != nullptr) {
		ms_free(buf);
	}
	return transaction;
}

std::shared_ptr<IceStunRequest::Transaction>
IceStunRequest::sendStunBindingRequest(const IceUtils::SockAddr &server) const {
	MSStunMessage *msg = ms_stun_binding_request_create();
	auto transaction = send(server, msg, "STUN binding request");
	ms_stun_message_destroy(msg);
	return transaction;
}

std::shared_ptr<IceStunRequest::Transaction> IceStunRequest::sendTurnAllocateRequest(const IceUtils::SockAddr &server) {
	MSStunMessage *msg = ms_turn_allocate_request_create();
	mStunMethod = ms_stun_message_get_method(msg);
	fillStunMessageAuthenticationFromTurnContext(msg);
	auto transaction =
	    send(server, msg, ((msg->username == nullptr) ? "TURN allocate request" : "TURN allocate request with auth"));
	ms_stun_message_destroy(msg);
	return transaction;
}

std::shared_ptr<IceStunRequest::Transaction>
IceStunRequest::sendTurnChannelBindRequest(const IceUtils::SockAddr &server) {
	MSStunMessage *msg = ms_turn_channel_bind_request_create(mPeerAddress, mChannelNumber);
	mStunMethod = ms_stun_message_get_method(msg);
	fillStunMessageAuthenticationFromTurnContext(msg);
	auto transaction = send(server, msg, "TURN channel bind request");
	ms_stun_message_destroy(msg);
	return transaction;
}

std::shared_ptr<IceStunRequest::Transaction>
IceStunRequest::sendTurnCreatePermissionRequest(const IceUtils::SockAddr &server) {
	MSStunMessage *msg = ms_turn_create_permission_request_create(mPeerAddress);
	mStunMethod = ms_stun_message_get_method(msg);
	fillStunMessageAuthenticationFromTurnContext(msg);
	auto transaction = send(server, msg, "TURN create permission request");
	ms_stun_message_destroy(msg);
	return transaction;
}

std::shared_ptr<IceStunRequest::Transaction> IceStunRequest::sendTurnRefreshRequest(const IceUtils::SockAddr &server) {
	if (ms_turn_context_get_state(mTurnContext) != MS_TURN_CONTEXT_STATE_IDLE) {
		MSStunMessage *msg = ms_turn_refresh_request_create(ms_turn_context_get_lifetime(mTurnContext));
		mStunMethod = ms_stun_message_get_method(msg);
		fillStunMessageAuthenticationFromTurnContext(msg);
		auto transaction = send(server, msg, "TURN refresh request");
		ms_stun_message_destroy(msg);
		return transaction;
	}
	return nullptr;
}

} // namespace ms2
