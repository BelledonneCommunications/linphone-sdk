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
#include <deque>
#include <functional>
#include <list>
#include <optional>
#include <set>
#include <vector>

#include "mediastreamer2/ice-candidate.h"
#include "mediastreamer2/ice-credentials.h"
#include "mediastreamer2/ice-pair-foundation.h"
#include "mediastreamer2/ice-stun-request.h"
#include "mediastreamer2/ice-transaction.h"
#include "mediastreamer2/ice-valid-candidate-pair.h"
#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/stun-address.h"
#include "mediastreamer2/stun-message.h"
#include "mediastreamer2/stun-transaction-id.h"
#include "mediastreamer2/turn-context.h"

namespace mediastreamer::nat {

class IceSession;

/**
 * Represents an ICE check list.
 *
 * Each media stream must be assigned a check list.
 * Check lists are added to an ICE session using the IceSession::addCheckList() method.
 */
class MS2_PUBLIC IceCheckList {
public:
	friend class IceSession;

	/**
	 * ICE check list state.
	 * See paragraph 5.7.4 ("Computing states") of RFC 5245 for more details.
	 */
	enum class State { Running, Completed, Failed };

	enum class Phase {
		Initial,
		GatheringCandidates,
		CandidatesGathered,
		CheckingConnectivity,
		Nominating,
		Completed,
		Failed
	};

	/**
	 * Allocate a new ICE check list.
	 * A check list must be allocated for each media stream of a media session and be added to an ICE session using the
	 * IceSession::addCheckList() method.
	 */
	IceCheckList() = default;

	/**
	 * Destroy a previously allocated ICE check list.
	 */
	~IceCheckList();

	/**
	 * Add a local candidate to an ICE check list.
	 * @param type The type of the local candidate to add
	 * @param transportAddress The transport address of the local candidate to add
	 * @param componentId The component ID of the local candidate (usually 1 for RTP and 2 for RTCP)
	 * @param base A pointer to the base candidate of the candidate to add.
	 * This function is to be called when gathering local candidates.
	 */
	std::shared_ptr<IceCandidate> addLocalCandidate(IceCandidate::Type type,
	                                                const IceTransportAddress &transportAddress,
	                                                ComponentId componentId,
	                                                const std::shared_ptr<IceCandidate> &base);

	/**
	 * Add a losing pair to an ICE check list.
	 * @param componentId The component ID of the candidates of the pair to add
	 * @param localTransportAddress The transport address of the local candidate of the pair to add
	 * @param remoteTransportAddress The transport address of the remote candidate of the pair to add
	 * This function is to be called when a RE-INVITE with an SDP containing a remote-candidates attribute is received.
	 */
	void addLosingPair(ComponentId componentId,
	                   const IceTransportAddress &localTransportAddress,
	                   const IceTransportAddress &remoteTransportAddress);

	/**
	 * Add a remote candidate to an ICE check list.
	 * @param type The type of the remote candidate to add
	 * @param transportAddress The transport address of the remote candidate to add
	 * @param componentId The component ID of the remote candidate (usually 1 for RTP and 2 for RTCP)
	 * @param priority The priority of the remote candidate
	 * @param foundation The foundation of the remote candidate
	 * @param isDefault Boolean value telling whether the remote candidate is a default candidate or not
	 * This function is to be called once the remote candidate list has been received via SDP.
	 */
	std::shared_ptr<IceCandidate> addRemoteCandidate(IceCandidate::Type type,
	                                                 const IceTransportAddress &transportAddress,
	                                                 ComponentId componentId,
	                                                 uint32_t priority,
	                                                 const std::string &foundation,
	                                                 bool isDefault);

	/**
	 * Tell whether ICE local candidates have been gathered for an ICE check list or not.
	 * @return true if local candidates have been gathered for the check list, false otherwise.
	 */
	[[nodiscard]] bool areCandidatesGathered() const {
		return (mPhase > Phase::GatheringCandidates);
	}

	/**
	 * Check if an ICE check list can be set in the Completed state after handling losing pairs.
	 */
	void checkCompleted();

	/**
	 * Dump the candidate pairs of an ICE check list in the traces (debug function).
	 */
	void dumpCandidatePairs() const;

	/**
	 * Dump the list of candidate pair foundations of an ICE check list in the traces (debug function).
	 */
	void dumpCandidatePairsFoundations() const;

	/**
	 * Dump the candidates of an ICE check list in the traces (debug function).
	 */
	void dumpCandidates() const;

	/**
	 * Dump an ICE check list in the traces (debug function).
	 */
	void dumpCheckList() const;

	/**
	 * Dump the list of component IDs of an ICE check list in the traces (debug function).
	 */
	void dumpComponentIds() const;

	/**
	 * Dump the triggered checks queue of an ICE check list in the traces (debug function).
	 */
	void dumpTriggeredChecksQueue() const;

	/**
	 * Dump the valid list of an ICE check list in the traces (debug function).
	 */
	void dumpValidList() const;

	/**
	 * Get the default local candidate for an ICE check list.
	 * @return A pointer to the default local candidate for RTCP if any, nullptr otherwise.
	 */
	[[nodiscard]] std::optional<std::shared_ptr<IceCandidate>> getDefaultLocalCandidateForRtcp() const;

	/**
	 * Get the default local candidate for an ICE check list.
	 * @return A pointer to the default local candidate for RTP if any, nullptr otherwise.
	 */
	[[nodiscard]] std::shared_ptr<IceCandidate> getDefaultLocalCandidateForRtp() const;

	/**
	 * Get the list of local candidates of an ICE check list.
	 * @return The list of the local candidates of the ICE check list.
	 */
	[[nodiscard]] const std::list<std::shared_ptr<IceCandidate>> &getLocalCandidates() const {
		return mLocalCandidates;
	}

	/**
	 * Get the local credentials of an ICE check list.
	 * @return A reference to the local credentials of the ICE check list.
	 */
	[[nodiscard]] const IceCredentials &getLocalCredentials() const;

	/**
	 * Get the remote credentials of an ICE check list.
	 * @return A reference to the remote credentials of the ICE check list.
	 */
	[[nodiscard]] const std::optional<IceCredentials> &getRemoteCredentials() const;

	/**
	 * Get the TURN context used for RTCP in an ICE check list.
	 * @return A pointer to the TURN context used for RTCP in the ICE check list
	 */
	[[nodiscard]] const std::shared_ptr<TurnContext> &getRtcpTurnContext() const {
		return mRtcpTurnContext;
	}

	/**
	 * Get the RTP session used by an ICE check list.
	 * @return A pointer to the RTP session used by the ICE check list.
	 */
	[[nodiscard]] RtpSession *getRtpSession() const {
		return mRtpSession;
	}

	/**
	 * Get the TURN context used for RTP in an ICE check list.
	 * @return A pointer to the TURN context used for RTP in the ICE check list
	 */
	[[nodiscard]] const std::shared_ptr<TurnContext> &getRtpTurnContext() const {
		return mRtpTurnContext;
	}

	/**
	 * Get the type of the selected valid candidate for an ICE check list.
	 * @return The type of the selected valid candidate
	 */
	[[nodiscard]] IceCandidate::Type getSelectedValidCandidateType() const;

	/**
	 * Get the selected valid base candidate for an ICE check list.
	 * @return A pointer to the valid local base candidate for RTCP if any, nullptr otherwise.
	 */
	[[nodiscard]] std::shared_ptr<IceCandidate> getSelectedValidLocalBaseCandidateForRtcp() const;

	/**
	 * Get the selected valid base candidate for an ICE check list.
	 * @return A pointer to the valid local base candidate for RTP if any, nullptr otherwise.
	 */
	[[nodiscard]] std::shared_ptr<IceCandidate> getSelectedValidLocalBaseCandidateForRtp() const;

	/**
	 * Get the selected valid local candidate for an ICE check list.
	 * @return A pointer to the valid local candidate for RTCP if any, nullptr otherwise.
	 */
	[[nodiscard]] std::optional<std::shared_ptr<IceCandidate>> getSelectedValidLocalCandidateForRtcp() const;

	/**
	 * Get the selected valid local candidate for an ICE check list.
	 * @return A pointer to the valid local candidate for RTP if any, nullptr otherwise.
	 */
	[[nodiscard]] std::shared_ptr<IceCandidate> getSelectedValidLocalCandidateForRtp() const;

	/**
	 * Get the selected valid remote candidate for an ICE check list.
	 * @return A pointer to the valid remote candidate for RTCP if any, nullptr otherwise.
	 */
	[[nodiscard]] std::optional<std::shared_ptr<IceCandidate>> getSelectedValidRemoteCandidateForRtcp() const;

	/**
	 * Get the selected valid remote candidate for an ICE check list.
	 * @return A pointer to the valid remote candidate for RTP if any, nullptr otherwise.
	 */
	[[nodiscard]] std::shared_ptr<IceCandidate> getSelectedValidRemoteCandidateForRtp() const;

	[[nodiscard]] std::shared_ptr<IceSession> getSession() const {
		return mSession.lock();
	}

	/**
	 * Get the state of an ICE check list.
	 * @return The check list state
	 */
	[[nodiscard]] State getState() const {
		return mState;
	}

	/**
	 * Get the humanly readable state of an ICE check list.
	 * @return The humanly readable check list state.
	 */
	[[nodiscard]] const std::string &getStateStr() const;

	/**
	 * Handle a STUN packet that has been received.
	 * This function is called from the audiostream or the videostream and is NOT to be called by the user.
	 */
	void handleStunPacket(RtpSession *rtpSession, const OrtpEventData *eventData);

	/**
	 * Get the mismatch property of an ICE check list.
	 * @return true if there was a mismatch for the check list, false otherwise
	 */
	[[nodiscard]] bool isMismatch() const {
		return mMismatch;
	}

	/**
	 * Print the route used to send the stream if the ICE process has finished successfully.
	 * @param message A message to print before the route
	 */
	void printRoute(const std::string &message) const;

	/**
	 * Core ICE check list processing.
	 * This function is called from the audiostream or the videostream and is NOT to be called by the user.
	 */
	void process(RtpSession *rtpSession);

	/**
	 * Remove local and remote RTCP candidates from an ICE check list.
	 * This function MUST be called before calling IceSession::startConnectivityChecks(). It is useful when using
	 * rtcp-mux.
	 */
	void removeRtcpCandidates();

	/**
	 * Set the remote credentials of an ICE check list.
	 * @param credentials The remote credentials
	 * This function is to be called once the remote credentials have been received via SDP.
	 */
	void setRemoteCredentials(const IceCredentials &credentials) {
		mRemoteCredentials = credentials;
	}

	/**
	 * Assign an RTP session to an ICE check list.
	 * @param rtpSession A pointer to the RTP session to assign to the check list
	 */
	void setRtpSession(RtpSession *rtpSession) {
		mRtpSession = rtpSession;
	}

	/**
	 * Set the state of an ICE check list.
	 * @param state The new state of the check list
	 */
	void setState(State state);

private:
	void addStunRequest(const std::shared_ptr<IceStunRequest> &request);
	bool checkGatheringTimeout(RtpSession *rtpSession, std::chrono::steady_clock::time_point currentTime) const;
	void checkMismatch();
	bool checkReceivedBindingRequestAttributes(const RtpSession *rtpSession,
	                                           const OrtpEventData *eventData,
	                                           const std::shared_ptr<StunMessage> &msg,
	                                           const StunAddress &remoteStunAddress);
	bool checkReceivedBindingRequestIntegrity(const RtpSession *rtpSession,
	                                          const OrtpEventData *eventData,
	                                          const std::shared_ptr<StunMessage> &msg,
	                                          const StunAddress &remoteStunAddress);
	bool checkReceivedBindingRequestRoleConflict(const RtpSession *rtpSession,
	                                             const OrtpEventData *eventData,
	                                             const std::shared_ptr<StunMessage> &msg,
	                                             const StunAddress &remoteStunAddress) const;
	bool checkReceivedBindingRequestUsername(const RtpSession *rtpSession,
	                                         const OrtpEventData *eventData,
	                                         const std::shared_ptr<StunMessage> &msg,
	                                         const StunAddress &remoteStunAddress) const;
	bool checkReceivedBindingResponseAddresses(const OrtpEventData *eventData,
	                                           const std::shared_ptr<IceCandidatePair> &candidatePair,
	                                           const StunAddress &remoteStunAddress);
	[[nodiscard]] bool checkReceivedBindingResponseAttributes(const std::shared_ptr<StunMessage> &msg) const;
	void chooseDefaultLocalCandidates() const;
	void chooseDefaultRemoteCandidates() const;
	void chooseLocalOrRemoteDefaultCandidates(const std::list<std::shared_ptr<IceCandidate>> &candidates) const;
	void clearLocalCandidates() {
		mLocalCandidates.clear();
	}
	void clearLocalComponentsIds() {
		mLocalComponentIds.clear();
	}
	void collectGatheringRoundTripTimes();
	void computeCandidateFoundation(const std::shared_ptr<IceCandidate> &candidate);
	void computeCandidatesFoundations();
	void computePairPriorities();
	void computePairsStates() const;
	void concludeWaitingFrozenAndInProgressPairs();
	void concludeProcessing(RtpSession *rtpSession, bool nominationDelayExpired);
	std::shared_ptr<IceCandidatePair> constructValidPair(RtpSession *rtpSession,
	                                                     const std::shared_ptr<IceCandidate> &candidate,
	                                                     const std::shared_ptr<IceCandidatePair> &succeededPair);
	std::shared_ptr<IceTransaction> createTransaction(const std::shared_ptr<IceCandidatePair> &candidatePair,
	                                                  StunTransactionId transactionId);
	void createTurnChannel(RtpTransport *rtpTransport,
	                       const SockAddr &localAddress,
	                       const IceTransportAddress &remoteTransportAddress,
	                       ComponentId componentId);
	void createTurnContexts();
	void createTurnPermissions();
	void deallocateTurnCandidate(ComponentId componentId) const;
	void deallocateTurnCandidates() const;
	void destroyTurnContexts();
	std::shared_ptr<IceCandidate> discoverPeerReflexiveCandidate(const std::shared_ptr<IceCandidatePair> &candidatePair,
	                                                             const std::shared_ptr<StunMessage> &msg);
	void eliminateRedundantCandidates();
	std::shared_ptr<IceTransaction> findTransaction(const std::shared_ptr<IceCandidatePair> &candidatePair);
	void formCandidatePairs();
	[[nodiscard]] bool gatherCandidates(size_t &checkListIndex);
	[[nodiscard]] bool gatherCandidates(ComponentId componentId,
	                                    std::chrono::steady_clock::time_point nextTransmissionTime,
	                                    bool sendRequest = false);
	[[nodiscard]] std::string generateArbitraryFoundation() const;
	[[nodiscard]] std::chrono::steady_clock::time_point getGatheringStartTime() const {
		return mGatheringStartTime;
	}
	[[nodiscard]] unsigned int getNbLosingPairs() const {
		return static_cast<unsigned int>(mLosingPairs.size());
	}
	[[nodiscard]] IceStunRequest::RoundTripTime getRoundTripTime() const {
		return mRtt;
	}
	[[nodiscard]] std::shared_ptr<IceCandidate> getSelectedCandidate(
	    ComponentId componentId,
	    const std::function<std::shared_ptr<IceCandidate>(const std::shared_ptr<IceValidCandidatePair> &)>
	        &getCandidateFromValidPair,
	    const std::string &errorMessage = "") const;
	[[nodiscard]] std::shared_ptr<IceValidCandidatePair> getSelectedValidCandidatePair(ComponentId componentId) const;
	[[nodiscard]] std::shared_ptr<IceStunRequest> getStunRequest(const StunTransactionId &transactionId) const;
	[[nodiscard]] std::shared_ptr<TurnContext> getTurnContextFromComponentId(ComponentId componentId) const;
	[[nodiscard]] std::vector<std::shared_ptr<IceCandidatePair>> getValidPairs() const;
	[[nodiscard]] std::vector<std::shared_ptr<IceCandidatePair>> getValidPairs(ComponentId componentId) const;
	void handleReceivedBindingRequest(RtpSession *rtpSession,
	                                  const OrtpEventData *eventData,
	                                  const std::shared_ptr<StunMessage> &msg,
	                                  const StunAddress &remoteAddress);
	void handleReceivedBindingResponse(RtpSession *rtpSession,
	                                   const OrtpEventData *eventData,
	                                   const std::shared_ptr<StunMessage> &msg,
	                                   const StunAddress &remoteAddress);
	void handleReceivedErrorResponse(RtpSession *rtpSession,
	                                 const OrtpEventData *eventData,
	                                 const std::shared_ptr<StunMessage> &msg);
	bool handleReceivedTurnAllocateSuccessResponse(RtpSession *rtpSession,
	                                               const OrtpEventData *eventData,
	                                               const std::shared_ptr<StunMessage> &msg,
	                                               const StunAddress &remoteStunAddress);
	void handleReceivedTurnChannelBindSuccessResponse(const OrtpEventData *eventData,
	                                                  const std::shared_ptr<StunMessage> &msg);
	void handleReceivedTurnCreatePermissionSuccessResponse(const OrtpEventData *eventData,
	                                                       const std::shared_ptr<StunMessage> &msg);
	void handleReceivedTurnRefreshSuccessResponse(const OrtpEventData *eventData,
	                                              const std::shared_ptr<StunMessage> &msg);
	void handleStunErrorResponse(const RtpSession *rtpSession,
	                             const OrtpEventData *eventData,
	                             const std::shared_ptr<StunMessage> &msg);
	[[nodiscard]] bool hasLocalComponentId(ComponentId componentId) const;
	[[nodiscard]] bool isFrozen() const;
	[[nodiscard]] bool isGatheringCandidates() const {
		return (mPhase == Phase::GatheringCandidates);
	}
	[[nodiscard]] bool isGatheringNeeded() const {
		return (mPhase == Phase::Initial);
	}
	std::shared_ptr<IceCandidate> learnPeerReflexiveCandidate(const OrtpEventData *eventData,
	                                                          const std::shared_ptr<StunMessage> &msg,
	                                                          const IceTransportAddress &transportAddress);
	[[nodiscard]] std::shared_ptr<IceCandidatePair>
	lookupPossibleValidPair(const std::shared_ptr<IceCandidatePair> &candidatePair) const;
	void nominate(const std::vector<std::shared_ptr<IceValidCandidatePair>> &bestValidCandidatePairs);
	void pairCandidates();
	void performNominations(bool nominationDelayExpired);
	void pruneCandidatePairs();
	void queueTriggeredCheck(const std::shared_ptr<IceCandidatePair> &pair);
	void removeGatheringStunRequests();
	void removeRtcpCandidatePairs();
	void removeStunRequest(const StunTransactionId &transactionId);
	void removeTransactionUsingPair(const std::shared_ptr<IceCandidatePair> &pair);
	void restart();
	void retransmitConnectivityChecks(std::chrono::steady_clock::time_point currentTime, const RtpSession *rtpSession);
	void scheduleTurnAllocationRefresh(ComponentId componentId, uint32_t lifetime);
	void
	scheduleTurnChannelBindRefresh(ComponentId componentId, uint16_t channelNumber, const StunAddress &peerAddress);
	void scheduleTurnPermissionRefresh(ComponentId componentId, const StunAddress &peerAddress);
	[[nodiscard]] std::shared_ptr<IceStunRequest>
	scheduleTurnRequest(ComponentId componentId,
	                    StunMessage::Method method,
	                    std::chrono::milliseconds nextTransmission,
	                    std::chrono::milliseconds shortTurnRefreshNextTransmission);
	void selectCandidates();
	void sendBindingRequest(const std::shared_ptr<IceCandidatePair> &candidatePair, const RtpSession *rtpSession);
	void sendBindingResponse(const RtpSession *rtpSession,
	                         const OrtpEventData *eventData,
	                         const std::shared_ptr<StunMessage> &msg,
	                         const StunAddress &remoteAddress) const;
	void sendKeepAlivePackets(const RtpSession *rtpSession) const;
	void sendStunRequests();
	std::shared_ptr<IceCandidatePair> sendTriggeredCheck(const RtpSession *rtpSession);
	void setBaseForSrflxCandidates();
	void setBaseForSrflxCandidates(ComponentId componentId);
	void setPhase(Phase phase);
	void setSelectedValidCandidatePair(const std::shared_ptr<IceValidCandidatePair> &validCandidatePair) const;
	void setSession(const std::shared_ptr<IceSession> &session);
	void setTransactionResponseTime(const StunTransactionId &transactionId, MSTimeSpec responseTime);
	void stopGathering();
	void stopRetransmissions();
	std::shared_ptr<IceCandidatePair>
	triggerConnectivityCheckOnBindingRequest(const OrtpEventData *eventData,
	                                         const std::shared_ptr<IceCandidate> &peerReflexiveCandidate,
	                                         const IceTransportAddress &remoteTransportAddress);
	void updateNominatedFlagOnBindingRequest(const std::shared_ptr<StunMessage> &msg,
	                                         const std::shared_ptr<IceCandidatePair> &candidatePair) const;
	void updateNominatedFlagOnBindingResponse(const std::shared_ptr<IceCandidatePair> &validPair,
	                                          const std::shared_ptr<IceCandidatePair> &succeededPair) const;
	void updatePairStatesOnBindingResponse(const std::shared_ptr<IceCandidatePair> &candidatePair) const;

	static std::shared_ptr<IceCandidate>
	addCandidate(IceCandidate::Type type,
	             const IceTransportAddress &transportAddress,
	             ComponentId componentId,
	             std::list<std::shared_ptr<IceCandidate>> &candidatesList,
	             std::set<ComponentId> &componentIdsList,
	             const std::function<void(const std::shared_ptr<IceCandidate> &)> &setIceCandidateProperties);
	static void dispatchIceEvent(RtpSession *rtpSession,
	                             OrtpEventType eventType,
	                             std::optional<bool> iceProcessingSuccessful = std::nullopt);
	static std::shared_ptr<IceCandidate> findCandidate(const std::list<std::shared_ptr<IceCandidate>> &candidates,
	                                                   IceCandidate::Type type,
	                                                   ComponentId componentId,
	                                                   int family);
	static std::pair<std::optional<StunAddress>, std::optional<StunAddress>>
	parseStunResponse(const std::shared_ptr<StunMessage> &msg);

	std::weak_ptr<IceSession> mSession;            /**< Pointer to the ICE session */
	std::shared_ptr<TurnContext> mRtpTurnContext;  /**< TURN context for RTP socket */
	std::shared_ptr<TurnContext> mRtcpTurnContext; /**< TURN context for RTCP socket */
	RtpSession *mRtpSession = nullptr; /**< Pointer to the RTP session associated with this ICE check list */
	std::optional<IceCredentials>
	    mRemoteCredentials; /**< Remote credentials for this check list (provided via SDP by the peer) */
	std::vector<std::shared_ptr<IceStunRequest>> mStunRequests;
	std::list<std::shared_ptr<IceCandidate>> mLocalCandidates;
	std::list<std::shared_ptr<IceCandidate>> mRemoteCandidates;
	std::vector<std::shared_ptr<IceCandidatePair>> mPairs;
	std::vector<std::shared_ptr<IceCandidatePair>> mLosingPairs;
	std::deque<std::shared_ptr<IceCandidatePair>> mTriggeredChecksQueue;
	std::vector<std::shared_ptr<IceCandidatePair>> mCheckList;
	std::vector<std::shared_ptr<IceValidCandidatePair>> mValidList;
	std::deque<std::shared_ptr<IceTransaction>> mTransactionList;
	std::set<IcePairFoundation> mFoundations;
	std::set<ComponentId> mLocalComponentIds;
	std::set<ComponentId> mRemoteComponentIds;
	State mState = State::Running; /**< Global state of the ICE check list */
	Phase mPhase = Phase::Initial;
	std::chrono::steady_clock::time_point mTaTime =
	    std::chrono::steady_clock::now(); /**< Time when the Ta timer has been processed for the last time */
	std::chrono::steady_clock::time_point
	    mKeepAliveTime;                /**< Time when the last keepalive packet has been sent for this stream */
	uint32_t mFoundationGenerator = 1; /**< Auto-incremented integer to generate unique foundation values */
	std::chrono::steady_clock::time_point mGatheringStartTime; /**< Time when the gathering process was started */
	std::optional<std::chrono::steady_clock::time_point> mNominationDelayStartTime =
	    std::nullopt; /**< Time when the nomination process has been delayed */
	IceStunRequest::RoundTripTime mRtt;
	bool mMismatch = false; /**< Tells whether there was a mismatch during the answer/offer */
};

} // namespace mediastreamer::nat
