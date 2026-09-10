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

#include <chrono>
#include <memory>
#include <optional>
#include <vector>

#include <numeric>
#include <ortp/rtpsession.h>

#include "mediastreamer2/ice-transport-address.h"
#include "mediastreamer2/ice-utils.h"
#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/sockaddr.h"
#include "mediastreamer2/stun-message.h"
#include "mediastreamer2/stun-transaction-id.h"
#include "mediastreamer2/turn-context.h"

namespace ms2::nat {

class MS2_PUBLIC IceStunRequest {
public:
	friend class IceCheckList;

	class RoundTripTime {
	public:
		RoundTripTime() = default;
		explicit RoundTripTime(std::chrono::milliseconds duration) : mDurations({duration}) {
		}
		~RoundTripTime() = default;

		void operator<<(const RoundTripTime &other) {
			mDurations.insert(mDurations.end(), other.mDurations.begin(), other.mDurations.end());
		}

		[[nodiscard]] std::optional<std::chrono::milliseconds> getAverage() const {
			const auto total = getTotal();
			if (total.has_value()) {
				return *total / mDurations.size();
			}
			return std::nullopt;
		}
		[[nodiscard]] std::optional<std::chrono::milliseconds> getTotal() const {
			if (mDurations.empty()) {
				return std::nullopt;
			}
			return std::reduce(mDurations.begin(), mDurations.end());
		}

	private:
		std::vector<std::chrono::milliseconds> mDurations;
	};

	class Transaction {
	public:
		explicit Transaction(const StunTransactionId transactionId)
		    : mId(transactionId), mRequestTime(std::chrono::steady_clock::now()) {
		}
		~Transaction() = default;

		[[nodiscard]] const StunTransactionId &getId() const {
			return mId;
		}
		[[nodiscard]] std::string getIdStr() const;
		void setResponseTime(const MSTimeSpec &responseTime);
		[[nodiscard]] std::optional<std::chrono::milliseconds> getRoundTripTime() const;

	private:
		StunTransactionId mId;
		std::chrono::steady_clock::time_point mRequestTime;
		std::optional<std::chrono::steady_clock::time_point> mResponseTime = std::nullopt;
	};

	~IceStunRequest();

	[[nodiscard]] uint16_t getChannelNumber() const {
		return mChannelNumber;
	}
	[[nodiscard]] std::vector<RoundTripTime> getGatheringRoundTripTimes() const;
	[[nodiscard]] const StunAddress &getPeerAddress() const {
		return mPeerAddress;
	}
	[[nodiscard]] RtpTransport *getRtpTransport() const {
		return mRtpTransport;
	}
	[[nodiscard]] struct addrinfo *getSourceAddrInfo() const {
		return mSourceAddrInfo;
	}
	[[nodiscard]] const std::shared_ptr<TurnContext> &getTurnContext() const {
		return mTurnContext;
	}
	[[nodiscard]] bool isGathering() const {
		return mGathering;
	}
	[[nodiscard]] bool isResponded() const {
		return mResponded;
	}
	static std::shared_ptr<IceStunRequest> create(const std::shared_ptr<TurnContext> &turnContext,
	                                              RtpTransport *rtpTransport,
	                                              const IceTransportAddress &transportAddress,
	                                              StunMessage::Method stunMethod);

private:
	IceStunRequest(const std::shared_ptr<TurnContext> &turnContext,
	               RtpTransport *rtpTransport,
	               const IceTransportAddress &transportAddress,
	               StunMessage::Method stunMethod);

	void addTransaction(const std::shared_ptr<Transaction> &transaction);
	void fillStunMessageAuthenticationFromTurnContext(const std::shared_ptr<StunMessage> &msg) const;
	[[nodiscard]] size_t getNbTransactions() const {
		return mTransactions.size();
	}
	[[nodiscard]] std::chrono::steady_clock::time_point getNextTransmissionTime() const {
		return mNextTransmissionTime;
	}
	[[nodiscard]] std::shared_ptr<Transaction> getTransaction(const StunTransactionId &transactionId) const;
	[[nodiscard]] bool needRetransmission() const;
	void programNextTransmission(const std::chrono::steady_clock::time_point nextTransmissionTime) {
		mNextTransmissionTime = nextTransmissionTime;
	}
	[[nodiscard]] std::shared_ptr<Transaction> send(const SockAddr &server);
	[[nodiscard]] std::shared_ptr<Transaction>
	send(const SockAddr &server, const std::shared_ptr<StunMessage> &msg, const std::string &requestType) const;
	[[nodiscard]] std::shared_ptr<Transaction> sendStunBindingRequest(const SockAddr &server) const;
	[[nodiscard]] std::shared_ptr<Transaction> sendTurnAllocateRequest(const SockAddr &server);
	[[nodiscard]] std::shared_ptr<Transaction> sendTurnChannelBindRequest(const SockAddr &server);
	[[nodiscard]] std::shared_ptr<Transaction> sendTurnCreatePermissionRequest(const SockAddr &server);
	[[nodiscard]] std::shared_ptr<Transaction> sendTurnRefreshRequest(const SockAddr &server);
	void setChannelNumber(const uint16_t channelNumber) {
		mChannelNumber = channelNumber;
	}
	void setGathering(const bool gathering) {
		mGathering = gathering;
	}
	void setPeerAddress(const StunAddress &peerAddress) {
		mPeerAddress = peerAddress;
	}
	void setResponded(const bool responded) {
		mResponded = responded;
	}

	RtpTransport *mRtpTransport = nullptr;
	std::shared_ptr<TurnContext> mTurnContext = nullptr;
	struct addrinfo *mSourceAddrInfo = nullptr;
	std::vector<std::shared_ptr<Transaction>> mTransactions;
	std::chrono::steady_clock::time_point mNextTransmissionTime;
	StunAddress mPeerAddress;
	uint16_t mChannelNumber = 0;
	StunMessage::Method mStunMethod = StunMessage::Method::Binding;
	bool mGathering = false;
	bool mResponded = false;
};

} // namespace ms2::nat
