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

namespace ms2 {

class IceSession;

/**
 * Represents an ICE check list.
 *
 * Each media stream must be assigned a check list.
 * Check lists are added to an ICE session using the ice_session_add_check_list() function.
 */
class IceCheckList {
public:
	friend class IceSession;

	/**
	 * ICE check list state.
	 * See paragraph 5.7.4 ("Computing states") of RFC 5245 for more details.
	 */
	enum class State { Running, Completed, Failed };

	/**
	 * Allocate a new ICE check list.
	 * A check list must be allocated for each media stream of a media session and be added to an ICE session using the
	 * IceSession::addCheckList() method.
	 */
	MS2_PUBLIC IceCheckList() = default;

	/**
	 * Destroy a previously allocated ICE check list.
	 */
	MS2_PUBLIC ~IceCheckList();

	/**
	 * Add a local candidate to an ICE check list.
	 * @param type The type of the local candidate to add
	 * @param transportAddress The transport address of the local candidate to add
	 * @param componentId The component ID of the local candidate (usually 1 for RTP and 2 for RTCP)
	 * @param base A pointer to the base candidate of the candidate to add.
	 * This function is to be called when gathering local candidates.
	 */
	MS2_PUBLIC std::shared_ptr<IceCandidate> addLocalCandidate(IceCandidate::Type type,
	                                                           const IceTransportAddress &transportAddress,
	                                                           uint16_t componentId,
	                                                           const std::shared_ptr<IceCandidate> &base);

	/**
	 * Add a losing pair to an ICE check list.
	 * @param componentId The component ID of the candidates of the pair to add
	 * @param localTransportAddress The transport address of the local candidate of the pair to add
	 * @param remoteTransportAddress The transport address of the remote candidate of the pair to add
	 * This function is to be called when a RE-INVITE with an SDP containing a remote-candidates attribute is received.
	 */
	MS2_PUBLIC void addLosingPair(uint16_t componentId,
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
	MS2_PUBLIC std::shared_ptr<IceCandidate> addRemoteCandidate(IceCandidate::Type type,
	                                                            const IceTransportAddress &transportAddress,
	                                                            uint16_t componentId,
	                                                            uint32_t priority,
	                                                            const std::string &foundation,
	                                                            bool isDefault);

	/**
	 * Tell whether ICE local candidates have been gathered for an ICE check list or not.
	 * @return true if local candidates have been gathered for the check list, false otherwise.
	 */
	[[nodiscard]] MS2_PUBLIC bool areCandidatesGathered() const {
		return mGatheringFinished;
	}

	/**
	 * Check if an ICE check list can be set in the Completed state after handling losing pairs.
	 */
	MS2_PUBLIC void checkCompleted();

	/**
	 * Dump the candidate pairs of an ICE check list in the traces (debug function).
	 */
	MS2_PUBLIC void dumpCandidatePairs() const;

	/**
	 * Dump the list of candidate pair foundations of an ICE check list in the traces (debug function).
	 */
	MS2_PUBLIC void dumpCandidatePairsFoundations() const;

	/**
	 * Dump the candidates of an ICE check list in the traces (debug function).
	 */
	MS2_PUBLIC void dumpCandidates() const;

	/**
	 * Dump an ICE check list in the traces (debug function).
	 */
	MS2_PUBLIC void dumpCheckList() const;

	/**
	 * Dump the list of component IDs of an ICE check list in the traces (debug function).
	 */
	MS2_PUBLIC void dumpComponentIds() const;

	/**
	 * Dump the triggered checks queue of an ICE check list in the traces (debug function).
	 */
	MS2_PUBLIC void dumpTriggeredChecksQueue() const;

	/**
	 * Dump the valid list of an ICE check list in the traces (debug function).
	 */
	MS2_PUBLIC void dumpValidList() const;

	/**
	 * Get the default local candidate for an ICE check list.
	 * @return A pointer to the default local candidate for RTCP if any, nullptr otherwise.
	 */
	[[nodiscard]] MS2_PUBLIC std::optional<std::shared_ptr<IceCandidate>> getDefaultLocalCandidateForRtcp() const;

	/**
	 * Get the default local candidate for an ICE check list.
	 * @return A pointer to the default local candidate for RTP if any, nullptr otherwise.
	 */
	[[nodiscard]] MS2_PUBLIC std::shared_ptr<IceCandidate> getDefaultLocalCandidateForRtp() const;

	/**
	 * Get the list of local candidates of an ICE check list.
	 * @return The list of the local candidates of the ICE check list.
	 */
	[[nodiscard]] MS2_PUBLIC const std::list<std::shared_ptr<IceCandidate>> &getLocalCandidates() const {
		return mLocalCandidates;
	}

	/**
	 * Get the local credentials of an ICE check list.
	 * @return A reference to the local credentials of the ICE check list.
	 */
	[[nodiscard]] MS2_PUBLIC const IceCredentials &getLocalCredentials() const;

	/**
	 * Get the remote credentials of an ICE check list.
	 * @return A reference to the remote credentials of the ICE check list.
	 */
	[[nodiscard]] MS2_PUBLIC const std::optional<IceCredentials> &getRemoteCredentials() const;

	/**
	 * Get the TURN context used for RTCP in an ICE check list.
	 * @return A pointer to the TURN context used for RTCP in the ICE check list
	 */
	[[nodiscard]] MS2_PUBLIC MSTurnContext *getRtcpTurnContext() const {
		return mRtcpTurnContext;
	}

	/**
	 * Get the RTP session used by an ICE check list.
	 * @return A pointer to the RTP session used by the ICE check list.
	 */
	[[nodiscard]] MS2_PUBLIC RtpSession *getRtpSession() const {
		return mRtpSession;
	}

	/**
	 * Get the TURN context used for RTP in an ICE check list.
	 * @return A pointer to the TURN context used for RTP in the ICE check list
	 */
	[[nodiscard]] MS2_PUBLIC MSTurnContext *getRtpTurnContext() const {
		return mRtpTurnContext;
	}

	/**
	 * Get the type of the selected valid candidate for an ICE check list.
	 * @return The type of the selected valid candidate
	 */
	[[nodiscard]] MS2_PUBLIC IceCandidate::Type getSelectedValidCandidateType() const;

	/**
	 * Get the selected valid base candidate for an ICE check list.
	 * @return A pointer to the valid local base candidate for RTCP if any, nullptr otherwise.
	 */
	[[nodiscard]] MS2_PUBLIC std::shared_ptr<IceCandidate> getSelectedValidLocalBaseCandidateForRtcp() const;

	/**
	 * Get the selected valid base candidate for an ICE check list.
	 * @return A pointer to the valid local base candidate for RTP if any, nullptr otherwise.
	 */
	[[nodiscard]] MS2_PUBLIC std::shared_ptr<IceCandidate> getSelectedValidLocalBaseCandidateForRtp() const;

	/**
	 * Get the selected valid local candidate for an ICE check list.
	 * @return A pointer to the valid local candidate for RTCP if any, nullptr otherwise.
	 */
	[[nodiscard]] MS2_PUBLIC std::optional<std::shared_ptr<IceCandidate>> getSelectedValidLocalCandidateForRtcp() const;

	/**
	 * Get the selected valid local candidate for an ICE check list.
	 * @return A pointer to the valid local candidate for RTP if any, nullptr otherwise.
	 */
	[[nodiscard]] MS2_PUBLIC std::shared_ptr<IceCandidate> getSelectedValidLocalCandidateForRtp() const;

	/**
	 * Get the selected valid remote candidate for an ICE check list.
	 * @return A pointer to the valid remote candidate for RTCP if any, nullptr otherwise.
	 */
	[[nodiscard]] MS2_PUBLIC std::optional<std::shared_ptr<IceCandidate>>
	getSelectedValidRemoteCandidateForRtcp() const;

	/**
	 * Get the selected valid remote candidate for an ICE check list.
	 * @return A pointer to the valid remote candidate for RTP if any, nullptr otherwise.
	 */
	[[nodiscard]] MS2_PUBLIC std::shared_ptr<IceCandidate> getSelectedValidRemoteCandidateForRtp() const;

	[[nodiscard]] MS2_PUBLIC IceSession *getSession() const {
		return mSession;
	}

	/**
	 * Get the state of an ICE check list.
	 * @return The check list state
	 */
	[[nodiscard]] MS2_PUBLIC State getState() const {
		return mState;
	}

	/**
	 * Get the humanly readable state of an ICE check list.
	 * @return The humanly readable check list state.
	 */
	[[nodiscard]] MS2_PUBLIC const std::string &getStateStr() const;

	/**
	 * Handle a STUN packet that has been received.
	 * This function is called from the audiostream or the videostream and is NOT to be called by the user.
	 */
	MS2_PUBLIC void handleStunPacket(RtpSession *rtpSession, const OrtpEventData *eventData);

	/**
	 * Tell if remote credentials of an ICE check list have changed or not.
	 * @param newCredentials The new remote credentials
	 * @return true if the remote credentials of the check list have changed, false otherwise.
	 */
	[[nodiscard]] MS2_PUBLIC bool haveRemoteCredentialsChanged(const IceCredentials &newCredentials) const;

	/**
	 * Get the mismatch property of an ICE check list.
	 * @return true if there was a mismatch for the check list, false otherwise
	 */
	[[nodiscard]] MS2_PUBLIC bool isMismatch() const {
		return mMismatch;
	}

	/**
	 * Print the route used to send the stream if the ICE process has finished successfully.
	 * @param message A message to print before the route
	 */
	MS2_PUBLIC void printRoute(const std::string &message) const;

	/**
	 * Core ICE check list processing.
	 * This function is called from the audiostream or the videostream and is NOT to be called by the user.
	 */
	MS2_PUBLIC void process(RtpSession *rtpSession);

	/**
	 * Remove local and remote RTCP candidates from an ICE check list.
	 * This function MUST be called before calling IceSession::startConnectivityChecks(). It is useful when using
	 * rtcp-mux.
	 */
	MS2_PUBLIC void removeRtcpCandidates();

	/**
	 * Set the remote credentials of an ICE check list.
	 * @param credentials The remote credentials
	 * This function is to be called once the remote credentials have been received via SDP.
	 */
	MS2_PUBLIC void setRemoteCredentials(const IceCredentials &credentials) {
		mRemoteCredentials = credentials;
	}

	/**
	 * Assign an RTP session to an ICE check list.
	 * @param rtpSession A pointer to the RTP session to assign to the check list
	 */
	MS2_PUBLIC void setRtpSession(RtpSession *rtpSession) {
		mRtpSession = rtpSession;
	}

	/**
	 * Set the state of an ICE check list.
	 * @param state The new state of the check list
	 */
	MS2_PUBLIC void setState(State state);

	static MS2_PUBLIC constexpr uint16_t MIN_COMPONENT_ID = 1;
	static MS2_PUBLIC constexpr uint16_t MAX_COMPONENT_ID = 256;

private:
	void addStunRequest(const std::shared_ptr<IceStunRequest> &request);
	bool checkGatheringTimeout(RtpSession *rtpSession, std::chrono::steady_clock::time_point currentTime) const;
	void checkMismatch();
	bool checkReceivedBindingRequestAttributes(const RtpSession *rtpSession,
	                                           const OrtpEventData *eventData,
	                                           const MSStunMessage *msg,
	                                           const MSStunAddress &remoteStunAddress);
	bool checkReceivedBindingRequestIntegrity(const RtpSession *rtpSession,
	                                          const OrtpEventData *eventData,
	                                          const MSStunMessage *msg,
	                                          const MSStunAddress &remoteStunAddress);
	bool checkReceivedBindingRequestRoleConflict(const RtpSession *rtpSession,
	                                             const OrtpEventData *eventData,
	                                             const MSStunMessage *msg,
	                                             const MSStunAddress &remoteStunAddress) const;
	bool checkReceivedBindingRequestUsername(const RtpSession *rtpSession,
	                                         const OrtpEventData *eventData,
	                                         const MSStunMessage *msg,
	                                         const MSStunAddress &remoteStunAddress) const;
	bool checkReceivedBindingResponseAddresses(const OrtpEventData *eventData,
	                                           const std::shared_ptr<IceCandidatePair> &candidatePair,
	                                           const MSStunAddress &remoteStunAddress);
	bool checkReceivedBindingResponseAttributes(const MSStunMessage *msg) const;
	void chooseDefaultLocalCandidates() const;
	void chooseDefaultRemoteCandidates() const;
	void chooseLocalOrRemoteDefaultCandidates(const std::list<std::shared_ptr<IceCandidate>> &candidates) const;
	void clearLocalCandidates() {
		mLocalCandidates.clear();
	}
	void clearLocalComponentsIds() {
		mLocalComponentsIds.clear();
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
	                                                  UInt96 transactionId);
	void createTurnChannel(RtpTransport *rtpTransport,
	                       const struct sockaddr *localAddress,
	                       socklen_t localAddressLen,
	                       const IceTransportAddress &remoteTransportAddress,
	                       uint16_t componentId);
	void createTurnContexts();
	void createTurnPermissions();
	void deallocateRtcpTurnCandidate() const;
	void deallocateRtpTurnCandidate() const;
	void deallocateTurnCandidate(MSTurnContext *turnContext, RtpTransport *rtpTransport, OrtpStream *stream) const;
	void deallocateTurnCandidates();
	void destroyTurnContexts();
	std::shared_ptr<IceCandidate> discoverPeerReflexiveCandidate(const std::shared_ptr<IceCandidatePair> &candidatePair,
	                                                             const MSStunMessage *msg);
	void eliminateRedundantCandidates();
	std::shared_ptr<IceTransaction> findTransaction(const std::shared_ptr<IceCandidatePair> &candidatePair);
	void formCandidatePairs();
	bool gatherCandidates(size_t &checkListIndex);
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
	[[nodiscard]] RtpTransport *getRtpTransport(uint16_t componentId) const;
	[[nodiscard]] std::shared_ptr<IceValidCandidatePair> getSelectedValidCandidatePair(uint16_t componentId) const;
	[[nodiscard]] std::shared_ptr<IceStunRequest> getStunRequest(const UInt96 &transactionId) const;
	[[nodiscard]] MSTurnContext *getTurnContextFromComponentId(uint16_t componentId) const;
	[[nodiscard]] std::vector<std::shared_ptr<IceCandidatePair>> getValidPairs() const;
	[[nodiscard]] std::vector<std::shared_ptr<IceCandidatePair>> getValidPairs(uint16_t componentId) const;
	void handleReceivedBindingRequest(RtpSession *rtpSession,
	                                  const OrtpEventData *eventData,
	                                  const MSStunMessage *msg,
	                                  const MSStunAddress &remoteAddress);
	void handleReceivedBindingResponse(RtpSession *rtpSession,
	                                   const OrtpEventData *eventData,
	                                   const MSStunMessage *msg,
	                                   const MSStunAddress &remoteAddress);
	void handleReceivedErrorResponse(RtpSession *rtpSession, const OrtpEventData *eventData, const MSStunMessage *msg);
	bool handleReceivedTurnAllocateSuccessResponse(RtpSession *rtpSession,
	                                               const OrtpEventData *eventData,
	                                               const MSStunMessage *msg,
	                                               const MSStunAddress &remoteStunAddress);
	void handleReceivedTurnChannelBindSuccessResponse(const OrtpEventData *eventData, const MSStunMessage *msg);
	void handleReceivedTurnCreatePermissionSuccessResponse(const OrtpEventData *eventData, const MSStunMessage *msg);
	void handleReceivedTurnRefreshSuccessResponse(const OrtpEventData *eventData, const MSStunMessage *msg);
	void
	handleStunErrorResponse(const RtpSession *rtpSession, const OrtpEventData *eventData, const MSStunMessage *msg);
	[[nodiscard]] bool hasLocalComponentId(uint16_t componentId) const;
	[[nodiscard]] bool isFrozen() const;
	[[nodiscard]] bool isGatheringCandidates() const {
		return mGatheringCandidates;
	}
	[[nodiscard]] bool isGatheringNeeded() const {
		return !mGatheringFinished;
	}
	std::shared_ptr<IceCandidate> learnPeerReflexiveCandidate(const OrtpEventData *eventData,
	                                                          const MSStunMessage *msg,
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
	void removeStunRequest(const UInt96 &transactionId);
	void removeTransactionUsingPair(const std::shared_ptr<IceCandidatePair> &pair);
	void restart();
	void retransmitConnectivityChecks(std::chrono::steady_clock::time_point currentTime, const RtpSession *rtpSession);
	void scheduleTurnAllocationRefresh(uint16_t componentId, uint32_t lifetime);
	void scheduleTurnChannelBindRefresh(uint16_t componentId, uint16_t channelNumber, const MSStunAddress &peerAddress);
	void scheduleTurnPermissionRefresh(uint16_t componentId, const MSStunAddress &peerAddress);
	void selectCandidates();
	void sendBindingRequest(const std::shared_ptr<IceCandidatePair> &candidatePair, const RtpSession *rtpSession);
	void sendBindingResponse(const RtpSession *rtpSession,
	                         const OrtpEventData *eventData,
	                         const MSStunMessage *msg,
	                         const MSStunAddress &remoteAddress) const;
	void sendKeepAlivePackets(RtpSession *rtpSession) const;
	void sendStunRequests();
	std::shared_ptr<IceCandidatePair> sendTriggeredCheck(const RtpSession *rtpSession);
	void setBaseForSrflxCandidates();
	void setBaseForSrflxCandidates(uint16_t componentId);
	void setSelectedValidCandidatePair(const std::shared_ptr<IceValidCandidatePair> &validCandidatePair) const;
	void setSession(IceSession *session) {
		mSession = session;
	}
	void setTransactionResponseTime(const UInt96 &transactionId, MSTimeSpec responseTime);
	void stopGathering();
	void stopRetransmissions();
	std::shared_ptr<IceCandidatePair>
	triggerConnectivityCheckOnBindingRequest(const OrtpEventData *eventData,
	                                         const std::shared_ptr<IceCandidate> &peerReflexiveCandidate,
	                                         const IceTransportAddress &remoteTransportAddress);
	void updateNominatedFlagOnBindingRequest(const MSStunMessage *msg,
	                                         const std::shared_ptr<IceCandidatePair> &candidatePair);
	void updateNominatedFlagOnBindingResponse(const std::shared_ptr<IceCandidatePair> &validPair,
	                                          const std::shared_ptr<IceCandidatePair> &succeededPair) const;
	void updatePairStatesOnBindingResponse(const std::shared_ptr<IceCandidatePair> &candidatePair) const;
	static std::shared_ptr<IceCandidate> findCandidate(const std::list<std::shared_ptr<IceCandidate>> &candidates,
	                                                   IceCandidate::Type type,
	                                                   uint16_t componentId,
	                                                   int family);
	static std::pair<const MSStunAddress *, const MSStunAddress *> parseStunResponse(const MSStunMessage *msg);

	IceSession *mSession = nullptr;            /**< Pointer to the ICE session */
	MSTurnContext *mRtpTurnContext = nullptr;  /**< TURN context for RTP socket */
	MSTurnContext *mRtcpTurnContext = nullptr; /**< TURN context for RTCP socket */
	RtpSession *mRtpSession = nullptr;         /**< Pointer to the RTP session associated with this ICE check list */
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
	std::set<uint16_t> mLocalComponentsIds;
	std::set<uint16_t> mRemoteComponentIds;
	State mState = State::Running; /**< Global state of the ICE check list */
	std::chrono::steady_clock::time_point mTaTime =
	    std::chrono::steady_clock::now(); /**< Time when the Ta timer has been processed for the last time */
	std::chrono::steady_clock::time_point
	    mKeepAliveTime;                /**< Time when the last keepalive packet has been sent for this stream */
	uint32_t mFoundationGenerator = 1; /**< Auto-incremented integer to generate unique foundation values */
	std::chrono::steady_clock::time_point mGatheringStartTime; /**< Time when the gathering process was started */
	std::chrono::steady_clock::time_point
	    mNominationDelayStartTime; /**< Time when the nomination process has been delayed */
	IceStunRequest::RoundTripTime mRtt;
	bool mMismatch = false;                  /**< Tells whether there was a mismatch during the answer/offer */
	bool mGatheringCandidates = false;       /**< Tells whether a candidate gathering
	      process is running or not */
	bool mGatheringFinished = false;         /**< Tells whether the candidate gathering process has finished or not */
	bool mNominationDelayRunning = false;    /**< Tells whether the nomination process has been delayed or not */
	bool mConnectivityChecksRunning = false; /**< Indicates that check list processing is in progress */
	bool mNominationInProgress = false;      /**< Substate between State::Running and State::Completed, when the
	 USE-CANDIDATE requests	are waiting for their responses */
};

} // namespace ms2
