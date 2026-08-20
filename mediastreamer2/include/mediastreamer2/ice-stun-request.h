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
#include "mediastreamer2/stun.h"

namespace ms2 {

class IceStunRequest {
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
		explicit Transaction(const UInt96 transactionId)
		    : mId(transactionId), mRequestTime(std::chrono::steady_clock::now()) {
		}
		~Transaction() = default;

		[[nodiscard]] const UInt96 &getId() const {
			return mId;
		}
		[[nodiscard]] std::string getIdStr() const;
		void setResponseTime(const MSTimeSpec &responseTime);
		[[nodiscard]] std::optional<std::chrono::milliseconds> getRoundTripTime() const;

	private:
		UInt96 mId;
		std::chrono::steady_clock::time_point mRequestTime;
		std::optional<std::chrono::steady_clock::time_point> mResponseTime = std::nullopt;
	};

	MS2_PUBLIC ~IceStunRequest();

	[[nodiscard]] MS2_PUBLIC uint16_t getChannelNumber() const {
		return mChannelNumber;
	}
	[[nodiscard]] MS2_PUBLIC std::vector<RoundTripTime> getGatheringRoundTripTimes() const;
	[[nodiscard]] MS2_PUBLIC const MSStunAddress &getPeerAddress() const {
		return mPeerAddress;
	}
	[[nodiscard]] MS2_PUBLIC RtpTransport *getRtpTransport() const {
		return mRtpTransport;
	}
	[[nodiscard]] MS2_PUBLIC struct addrinfo *getSourceAddrInfo() const {
		return mSourceAddrInfo;
	}
	[[nodiscard]] MS2_PUBLIC MSTurnContext *getTurnContext() const {
		return mTurnContext;
	}
	[[nodiscard]] MS2_PUBLIC bool isGathering() const {
		return mGathering;
	}
	[[nodiscard]] MS2_PUBLIC bool isResponded() const {
		return mResponded;
	}
	static MS2_PUBLIC std::shared_ptr<IceStunRequest> create(MSTurnContext *turnContext,
	                                                         RtpTransport *rtpTransport,
	                                                         const IceTransportAddress &transportAddress,
	                                                         uint16_t stunMethod);

private:
	IceStunRequest(MSTurnContext *turnContext,
	               RtpTransport *rtpTransport,
	               const IceTransportAddress &transportAddress,
	               uint16_t stunMethod);

	void addTransaction(const std::shared_ptr<Transaction> &transaction);
	void fillStunMessageAuthenticationFromTurnContext(MSStunMessage *msg) const;
	[[nodiscard]] size_t getNbTransactions() const {
		return mTransactions.size();
	}
	[[nodiscard]] std::chrono::steady_clock::time_point getNextTransmissionTime() const {
		return mNextTransmissionTime;
	}
	[[nodiscard]] std::shared_ptr<Transaction> getTransaction(const UInt96 &transactionId) const;
	[[nodiscard]] bool needRetransmission() const;
	void programNextTransmission(const std::chrono::steady_clock::time_point nextTransmissionTime) {
		mNextTransmissionTime = nextTransmissionTime;
	}
	[[nodiscard]] std::shared_ptr<Transaction> send(const IceUtils::SockAddr &server);
	[[nodiscard]] std::shared_ptr<Transaction>
	send(const IceUtils::SockAddr &server, const MSStunMessage *msg, const std::string &requestType) const;
	[[nodiscard]] std::shared_ptr<Transaction> sendStunBindingRequest(const IceUtils::SockAddr &server) const;
	[[nodiscard]] std::shared_ptr<Transaction> sendTurnAllocateRequest(const IceUtils::SockAddr &server);
	[[nodiscard]] std::shared_ptr<Transaction> sendTurnChannelBindRequest(const IceUtils::SockAddr &server);
	[[nodiscard]] std::shared_ptr<Transaction> sendTurnCreatePermissionRequest(const IceUtils::SockAddr &server);
	[[nodiscard]] std::shared_ptr<Transaction> sendTurnRefreshRequest(const IceUtils::SockAddr &server);
	void setChannelNumber(const uint16_t channelNumber) {
		mChannelNumber = channelNumber;
	}
	void setGathering(const bool gathering) {
		mGathering = gathering;
	}
	void setPeerAddress(const MSStunAddress &peerAddress) {
		mPeerAddress = peerAddress;
	}
	void setResponded(const bool responded) {
		mResponded = responded;
	}

	RtpTransport *mRtpTransport = nullptr;
	MSTurnContext *mTurnContext = nullptr;
	struct addrinfo *mSourceAddrInfo = nullptr;
	std::vector<std::shared_ptr<Transaction>> mTransactions;
	std::chrono::steady_clock::time_point mNextTransmissionTime;
	MSStunAddress mPeerAddress{};
	uint16_t mChannelNumber = 0;
	uint16_t mStunMethod = MS_STUN_METHOD_BINDING;
	bool mGathering = false;
	bool mResponded = false;
};

} // namespace ms2
