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

#include <ortp/rtpsession.h>

#include "mediastreamer2/ice-candidate.h"
#include "mediastreamer2/ice-constants.h"
#include "mediastreamer2/ice-role.h"
#include "mediastreamer2/mscommon.h"

namespace ms2::nat {

/**
 * Represents an ICE candidate pair.
 */
class MS2_PUBLIC IceCandidatePair {
public:
	friend class IceCheckList;
	friend class IceValidCandidatePair;

	/**
	 * ICE candidate pair state.
	 *
	 * See paragraph 5.7.4 ("Computing states") of RFC 5245 for more details.
	 */
	enum class State { Waiting, InProgress, Succeeded, Failed, Frozen };

	~IceCandidatePair() = default;

	bool operator==(const IceCandidatePair &other) const;

	[[nodiscard]] uint64_t getPriority() const {
		return mPriority;
	}
	[[nodiscard]] bool isDefault() const {
		return mIsDefault;
	}
	[[nodiscard]] bool isNominated() const {
		return mIsNominated;
	}

private:
	IceCandidatePair(const std::shared_ptr<IceCandidate> &localCandidate,
	                 const std::shared_ptr<IceCandidate> &remoteCandidate,
	                 IceRole role,
	                 bool retryWithDummyMessageIntegrity = false);

	void computePriority(IceRole role);
	void dump(unsigned int index) const;
	[[nodiscard]] const std::shared_ptr<IceCandidate> &getLocalCandidate() const {
		return mLocalCandidate;
	}
	[[nodiscard]] uint8_t getNbRetransmissions() const {
		return mRetransmissions;
	}
	[[nodiscard]] const std::shared_ptr<IceCandidate> &getRemoteCandidate() const {
		return mRemoteCandidate;
	}
	[[nodiscard]] IceRole getRole() const {
		return mRole;
	}
	[[nodiscard]] std::chrono::milliseconds getRto() const {
		return mRto;
	}
	[[nodiscard]] State getState() const {
		return mState;
	}
	[[nodiscard]] const std::string &getStateStr() const;
	[[nodiscard]] std::chrono::steady_clock::time_point getTransmissionTime() const {
		return mTransmissionTime;
	}
	[[nodiscard]] bool hasCanceledTransaction() const {
		return mHasCanceledTransaction;
	}
	[[nodiscard]] bool hasUseCandidate() const {
		return mUseCandidate;
	}
	void increaseRetransmissionTimer();
	void incrementRetransmissions() {
		mRetransmissions++;
	}
	void initializeRetransmissionTimer();
	void initializeTransmissionTime();
	[[nodiscard]] bool isNominationFailing() const {
		return mNominationFailing;
	}
	[[nodiscard]] bool isNominationPending() const {
		return mNominationPending;
	}
	[[nodiscard]] bool isRetransmissionPending() const;
	void replaceSrflxCandidateByBase();
	void sendIndication(const RtpSession *rtpSession);
	void setHasCanceledTransaction(const bool hasCanceledTransaction) {
		mHasCanceledTransaction = hasCanceledTransaction;
	}
	void setIsNominated(const bool isNominated) {
		mIsNominated = isNominated;
	}
	void setNominationFailing(const bool nominationFailing) {
		mNominationFailing = nominationFailing;
	}
	void setNominationPending(const bool pending) {
		mNominationPending = pending;
	}
	void setRetryWithDummyMessageIntegrity(const bool retryWithDummyMessageIntegrity) {
		mRetryWithDummyMessageIntegrity = retryWithDummyMessageIntegrity;
	}
	void setRole(const IceRole role) {
		mRole = role;
	}
	void setState(State state);
	void setTransmissionTime(const std::chrono::steady_clock::time_point transmissionTime) {
		mTransmissionTime = transmissionTime;
	}
	void setUseCandidate(const bool useCandidate) {
		mUseCandidate = useCandidate;
	}
	void setUseDummyHmac(const bool useDummyHmac) {
		mUseDummyHmac = useDummyHmac;
	}
	[[nodiscard]] bool shouldRetryWithDummyMessageIntegrity() const {
		return mRetryWithDummyMessageIntegrity;
	}
	[[nodiscard]] bool shouldUseDummyHmac() const {
		return mUseDummyHmac;
	}

	IceRole mRole; /**< Role of the agent when the connectivity check has been sent for the candidate pair */
	std::shared_ptr<IceCandidate> mLocalCandidate = nullptr;  /**< Pointer to the local candidate of the pair */
	std::shared_ptr<IceCandidate> mRemoteCandidate = nullptr; /**< Pointer to the remote candidate of the pair */
	State mState = State::Frozen;                             /**< State of the candidate pair */
	uint64_t mPriority = 0;                                   /**< Priority of the candidate pair */
	std::chrono::steady_clock::time_point
	    mTransmissionTime; /**< Time when the connectivity check for the candidate pair has been sent */
	std::chrono::milliseconds mRto = ICE_DEFAULT_RTO_DURATION; /**< Duration of the retransmit timer for the
	                                        connectivity check sent for the candidate pair in ms */
	uint8_t mRetransmissions =
	    0;                      /**< Number of retransmissions for the connectivity check sent for the candidate pair */
	bool mIsDefault = false;    /**< Tells whether this candidate pair is a default candidate pair or not. */
	bool mUseCandidate = false; /**< Tells if the USE-CANDIDATE attribute must be set for the connectivity
	                         checks send for the candidate pair. */
	bool mIsNominated = false;  /**< Tells whether this candidate pair is nominated or not. */
	bool mNominationPending = false;      /** Tells whether this candidate pair was nominated by the remote (in
	                                   controlled mode), but we could not yet complete the check. */
	bool mHasCanceledTransaction = false; /**< Tells whether this candidate pair has a cancelled transaction, see
	                                    RFC5245 7.2.1.4. Triggered Checks. */
	bool mNominationFailing = false;      /**< Indicates that this pair was nominated but it is apparently failing
	                                   because no response is received. */
	bool mRetryWithDummyMessageIntegrity = false; /** Use to tell to retry with dummy message integrity. Useful to keep
	                                              backward compatibility with older versions. */
	bool mUseDummyHmac = false;                   /* Don't compute real hmac. Used for backward compatibility. */
};

} // namespace ms2::nat
