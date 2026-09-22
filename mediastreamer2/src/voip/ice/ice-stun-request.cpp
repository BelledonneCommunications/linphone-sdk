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
#include "mediastreamer2/stun-message.h"
#include "mediastreamer2/stun-raw-message.h"

namespace mediastreamer::nat {

std::string IceStunRequest::Transaction::getIdStr() const {
	return mId.asString();
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

std::shared_ptr<IceStunRequest> IceStunRequest::create(const std::shared_ptr<TurnContext> &turnContext,
                                                       RtpTransport *rtpTransport,
                                                       const IceTransportAddress &transportAddress,
                                                       const StunMessage::Method stunMethod) {
	auto request =
	    std::shared_ptr<IceStunRequest>(new IceStunRequest(turnContext, rtpTransport, transportAddress, stunMethod));
	if (request->getSourceAddrInfo() == nullptr) {
		BCTBX_SLOGE << "IceStunServerRequest::create(): source address not defined";
		return nullptr;
	}
	return request;
}

//------------------------------------------------------------------------------

IceStunRequest::IceStunRequest(const std::shared_ptr<TurnContext> &turnContext,
                               RtpTransport *rtpTransport,
                               const IceTransportAddress &transportAddress,
                               const StunMessage::Method stunMethod)
    : mRtpTransport(rtpTransport), mTurnContext(turnContext), mStunMethod(stunMethod) {
	mSourceAddrInfo = bctbx_ip_address_to_addrinfo(transportAddress.getFamily(), SOCK_DGRAM,
	                                               transportAddress.getIp().c_str(), transportAddress.getPort());
}

void IceStunRequest::addTransaction(const std::shared_ptr<Transaction> &transaction) {
	mTransactions.push_back(transaction);
}

void IceStunRequest::fillStunMessageAuthenticationFromTurnContext(const std::shared_ptr<StunMessage> &msg) const {
	if (mTurnContext->getRealm().has_value()) {
		msg->setRealm(mTurnContext->getRealm().value());
	}
	if (mTurnContext->getNonce().has_value()) {
		msg->setNonce(mTurnContext->getNonce().value());
	}
	if (mTurnContext->getUsername().has_value()) {
		msg->setUsername(mTurnContext->getUsername().value());
	}
	if (mTurnContext->getPassword().has_value()) {
		msg->setPassword(mTurnContext->getPassword().value());
	}
	if (mTurnContext->getHa1().has_value()) {
		msg->setHa1(mTurnContext->getHa1().value());
	}
	if (mTurnContext->getPassword().has_value() || mTurnContext->getHa1().has_value()) {
		msg->enableMessageIntegrity(true);
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

std::shared_ptr<IceStunRequest::Transaction>
IceStunRequest::getTransaction(const StunTransactionId &transactionId) const {
	const auto it = std::find_if(mTransactions.begin(), mTransactions.end(), [transactionId](const auto &transaction) {
		return transaction->getId() == transactionId;
	});
	if (it == mTransactions.end()) {
		return nullptr;
	}
	return *it;
}

bool IceStunRequest::needRetransmission() const {
	return std::chrono::steady_clock::now() >= mNextTransmissionTime;
}

std::shared_ptr<IceStunRequest::Transaction> IceStunRequest::send(const SockAddr &server) {
	std::shared_ptr<IceStunRequest::Transaction> transaction = nullptr;
	switch (mStunMethod) {
		case StunMessage::Method::Binding:
			transaction = sendStunBindingRequest(server);
			break;
		case StunMessage::Method::TurnAllocate:
			transaction = sendTurnAllocateRequest(server);
			break;
		case StunMessage::Method::TurnCreatePermission:
			transaction = sendTurnCreatePermissionRequest(server);
			break;
		case StunMessage::Method::TurnRefresh:
			transaction = sendTurnRefreshRequest(server);
			break;
		case StunMessage::Method::TurnChannelBind:
			transaction = sendTurnChannelBindRequest(server);
			break;
		default:
			break;
	}
	return transaction;
}

std::shared_ptr<IceStunRequest::Transaction> IceStunRequest::send(const SockAddr &serverAddr,
                                                                  const std::shared_ptr<StunMessage> &msg,
                                                                  const std::string &requestType) const {
	std::shared_ptr<IceStunRequest::Transaction> transaction = nullptr;
	const auto stunRawMessage = msg->encode();
	const auto data = stunRawMessage->getData();
	if (!data.empty()) {
		transaction = std::make_shared<IceStunRequest::Transaction>(msg->getTransactionId());
		const auto destAddress = serverAddr.ipv6toIpv4();
		const auto sourceAddress =
		    SockAddr(mSourceAddrInfo->ai_addr, static_cast<socklen_t>(mSourceAddrInfo->ai_addrlen));
		BCTBX_SLOGM << "ice: Send " << requestType << ": " << sourceAddress.asString() << " --> "
		            << destAddress.asString() << " [" << transaction->getIdStr() << "]";
		std::ignore =
		    sendMessageToSocket(mRtpTransport, reinterpret_cast<const char *>(data.data()), data.size(),
		                        sourceAddress.asStructSockAddr(), destAddress.asStructSockAddr(), destAddress.getLen());
	} else {
		BCTBX_SLOGE << "ice: encoding " << requestType << " failed";
	}
	return transaction;
}

std::shared_ptr<IceStunRequest::Transaction> IceStunRequest::sendStunBindingRequest(const SockAddr &server) const {
	return send(server, StunMessage::createStunBindingRequest(), "STUN binding request");
}

std::shared_ptr<IceStunRequest::Transaction> IceStunRequest::sendTurnAllocateRequest(const SockAddr &server) {
	const auto msg = StunMessage::createTurnAllocateRequest();
	mStunMethod = msg->getMethod();
	fillStunMessageAuthenticationFromTurnContext(msg);
	return send(server, msg,
	            ((msg->getUsername() == std::nullopt) ? "TURN allocate request" : "TURN allocate request with auth"));
}

std::shared_ptr<IceStunRequest::Transaction> IceStunRequest::sendTurnChannelBindRequest(const SockAddr &server) {
	const auto msg = StunMessage::createTurnChannelBindRequest(mPeerAddress, mChannelNumber);
	mStunMethod = msg->getMethod();
	fillStunMessageAuthenticationFromTurnContext(msg);
	return send(server, msg, "TURN channel bind request");
}

std::shared_ptr<IceStunRequest::Transaction> IceStunRequest::sendTurnCreatePermissionRequest(const SockAddr &server) {
	const auto msg = StunMessage::createTurnCreatePermissionRequest(mPeerAddress);
	mStunMethod = msg->getMethod();
	fillStunMessageAuthenticationFromTurnContext(msg);
	return send(server, msg, "TURN create permission request");
}

std::shared_ptr<IceStunRequest::Transaction> IceStunRequest::sendTurnRefreshRequest(const SockAddr &server) {
	if (mTurnContext->getState() != TurnContext::State::Idle) {
		const auto msg = StunMessage::createTurnRefreshRequest(
		    mTurnContext->getLifetime().has_value() ? mTurnContext->getLifetime().value() : 0);
		mStunMethod = msg->getMethod();
		fillStunMessageAuthenticationFromTurnContext(msg);
		return send(server, msg, "TURN refresh request");
	}
	return nullptr;
}

} // namespace mediastreamer::nat
